#include "AngelscriptTypeBindInfoApply.h"
#include "as_scriptengine.h"
#include "as_objecttype.h"
#include "Containers/ScriptArray.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "AngelscriptTypeBindInfoReflection.h"
#include "AngelscriptEngine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"

#ifdef AS_USE_NAMESPACE
using namespace AngelScript;
#endif

struct FAngelscriptTypeBindInfoInstallation::FDelegateSubscription
{
	FScriptDelegate* Single = nullptr;
	FMulticastScriptDelegate* Multicast = nullptr;
	TStrongObjectPtr<UObject> Object;
	FName FunctionName;
	UFunction* Signature = nullptr;
};
struct FAngelscriptTypeBindInfoInstallation::FDelegatePayload
{
	int32 Handle = INDEX_NONE;
	TStrongObjectPtr<UObject> Object;
	UFunction* Function = nullptr;
	FProperty* Parameter = nullptr;
	TUniquePtr<FStructOnScope> Payload;
};
FAngelscriptTypeBindInfoInstallation::~FAngelscriptTypeBindInfoInstallation()
{
	for (const TSharedPtr<FDelegateSubscription>& Subscription : DelegateSubscriptions)
	{
		if (!Subscription) continue;
		if (Subscription->Single) Subscription->Single->Clear();
		if (Subscription->Multicast) Subscription->Multicast->Remove(Subscription->Object.Get(), Subscription->FunctionName);
	}
	DelegateSubscriptions.Reset(); DelegatePayloads.Reset(); ReflectedEventRegistry.Reset();
}

namespace
{
	bool ConstructElement(const FAngelscriptTemplateElementOperations& Element, void* Address, FString& Diagnostic)
	{
		if (Element.Nested) return Element.Nested->ConstructValue(Address, Diagnostic);
		if (Element.bPrimitive || Element.bEnumScalar || Element.bObjectHandle) { FMemory::Memzero(Address, Element.Size); return true; }
		if (Element.Declaration && Element.Declaration->DefaultConstruct)
		{
			Element.Declaration->DefaultConstruct(Address); return true;
		}
		if (Element.Reflected && Element.Reflected->Lifetime.Initialize && Element.Reflected->Lifetime.Struct)
		{
			Element.Reflected->Lifetime.Initialize(Element.Reflected->Lifetime.Struct, Address); return true;
		}
		Diagnostic = FString::Printf(TEXT("element '%s' has no construction recipe"), *Element.DisplayName); return false;
	}
	bool CopyElement(const FAngelscriptTemplateElementOperations& Element, void* Address, const void* Source, FString& Diagnostic)
	{
		if (Element.Nested) return Element.Nested->CopyConstructValue(Address, Source, Diagnostic);
		if (Element.bPrimitive || Element.bEnumScalar || Element.bObjectHandle || (Element.Declaration && Element.Declaration->bTriviallyCopyable))
		{
			FMemory::Memcpy(Address, Source, Element.Size); return true;
		}
		if (Element.Declaration && Element.Declaration->CopyConstruct)
		{
			Element.Declaration->CopyConstruct(Address, Source); return true;
		}
		if (Element.Reflected && Element.Reflected->Lifetime.Copy && Element.Reflected->Lifetime.Struct)
		{
			Element.Reflected->Lifetime.Copy(Element.Reflected->Lifetime.Struct, Address, Source); return true;
		}
		Diagnostic = FString::Printf(TEXT("element '%s' has no copy recipe"), *Element.DisplayName); return false;
	}
	bool DestroyElement(const FAngelscriptTemplateElementOperations& Element, void* Address, FString& Diagnostic)
	{
		if (Element.Nested) return Element.Nested->DestroyValue(Address, Diagnostic);
		if (Element.bPrimitive || Element.bEnumScalar || Element.bObjectHandle || (Element.Declaration && Element.Declaration->bTriviallyDestructible)) return true;
		if (Element.Declaration && Element.Declaration->Destroy)
		{
			Element.Declaration->Destroy(Address); return true;
		}
		if (Element.Reflected && Element.Reflected->Lifetime.Destroy && Element.Reflected->Lifetime.Struct)
		{
			Element.Reflected->Lifetime.Destroy(Element.Reflected->Lifetime.Struct, Address); return true;
		}
		Diagnostic = FString::Printf(TEXT("element '%s' has no destruction recipe"), *Element.DisplayName); return false;
	}
	void VisitElement(const FAngelscriptTemplateElementOperations& Element, const void* Address, TFunctionRef<void(UObject*)> Visitor)
	{
		if (Element.Nested) { Element.Nested->EnumerateReferences(Address, Visitor); return; }
		if (Element.bObjectHandle)
		{
			if (UObject* Object = *static_cast<UObject* const*>(Address)) Visitor(Object);
		}
	}
	bool EqualElement(const FAngelscriptTemplateElementOperations& Element, const void* A, const void* B, FString& Diagnostic)
	{
		if (Element.Nested)
		{
			if (Element.Nested->ContainerKind == EAngelscriptTemplateContainerKind::Array)
				return Element.Nested->ArrayEquals(*static_cast<const FScriptArray*>(A), *static_cast<const FScriptArray*>(B), Diagnostic);
			Diagnostic = FString::Printf(TEXT("nested element '%s' has no equality adapter"), *Element.DisplayName); return false;
		}
		if (Element.bObjectHandle) return *static_cast<void* const*>(A) == *static_cast<void* const*>(B);
		if (Element.bEnumScalar) return FMemory::Memcmp(A, B, Element.Size) == 0;
		if (Element.bPrimitive)
		{
			switch (Element.Type.GetTokenType())
			{
			case ttBool: case ttInt8: case ttUInt8: return *static_cast<const uint8*>(A) == *static_cast<const uint8*>(B);
			case ttInt16: case ttUInt16: return *static_cast<const uint16*>(A) == *static_cast<const uint16*>(B);
			case ttInt: case ttUInt: return *static_cast<const uint32*>(A) == *static_cast<const uint32*>(B);
			case ttInt64: case ttUInt64: return *static_cast<const uint64*>(A) == *static_cast<const uint64*>(B);
			case ttFloat32: return *static_cast<const float*>(A) == *static_cast<const float*>(B);
			case ttFloat64: case ttDouble: return *static_cast<const double*>(A) == *static_cast<const double*>(B);
			case ttFloat:
				return Element.Size == sizeof(double) ? *static_cast<const double*>(A) == *static_cast<const double*>(B)
					: *static_cast<const float*>(A) == *static_cast<const float*>(B);
			default: break;
			}
		}
		if (Element.Declaration && Element.Declaration->Equal) return Element.Declaration->Equal(A, B);
		Diagnostic = FString::Printf(TEXT("element '%s' has no equality recipe"), *Element.DisplayName); return false;
	}
	uint32 HashElement(const FAngelscriptTemplateElementOperations& Element, const void* Address, FString& Diagnostic)
	{
		if (Element.bObjectHandle) return PointerHash(*static_cast<void* const*>(Address));
		if (Element.bEnumScalar) return FCrc::MemCrc32(Address, Element.Size);
		if (Element.bPrimitive)
		{
			switch (Element.Type.GetTokenType())
			{
			case ttBool: case ttInt8: case ttUInt8: return GetTypeHash(*static_cast<const uint8*>(Address));
			case ttInt16: case ttUInt16: return GetTypeHash(*static_cast<const uint16*>(Address));
			case ttInt: case ttUInt: return GetTypeHash(*static_cast<const uint32*>(Address));
			case ttInt64: case ttUInt64: return GetTypeHash(*static_cast<const uint64*>(Address));
			case ttFloat32: return GetTypeHash(*static_cast<const float*>(Address));
			case ttFloat64: case ttDouble: return GetTypeHash(*static_cast<const double*>(Address));
			case ttFloat: return Element.Size == sizeof(double) ? GetTypeHash(*static_cast<const double*>(Address))
				: GetTypeHash(*static_cast<const float*>(Address));
			default: break;
			}
		}
		if (Element.Declaration && Element.Declaration->Hash) return Element.Declaration->Hash(Address);
		Diagnostic = FString::Printf(TEXT("map key '%s' has no hash recipe"), *Element.DisplayName); return 0;
	}
	FScriptMapLayout MapLayout(const FAngelscriptTemplateInstanceOperations& Operations)
	{
		return FScriptMap::GetScriptLayout(Operations.Arguments[0].Size, Operations.Arguments[0].Alignment,
			Operations.Arguments[1].Size, Operations.Arguments[1].Alignment);
	}
	const void* MapPair(const FScriptMap& Map, int32 Index, const FScriptMapLayout& Layout)
	{
		return Map.GetData(Index, Layout);
	}
	void* MapPair(FScriptMap& Map, int32 Index, const FScriptMapLayout& Layout)
	{
		return Map.GetData(Index, Layout);
	}
	const void* MapPairValue(const void* Pair, const FScriptMapLayout& Layout)
	{
		return static_cast<const uint8*>(Pair) + Layout.ValueOffset;
	}
	void* MapPairValue(void* Pair, const FScriptMapLayout& Layout)
	{
		return static_cast<uint8*>(Pair) + Layout.ValueOffset;
	}
	int32 FindMapIndex(const FAngelscriptTemplateInstanceOperations& Operations, const FScriptMap& Map,
		const void* Key, FString& Diagnostic)
	{
		const auto Layout = MapLayout(Operations);
		auto Hash = [&](const void* Address) { return HashElement(Operations.Arguments[0], Address, Diagnostic); };
		auto Equal = [&](const void* A, const void* B) { return EqualElement(Operations.Arguments[0], A, B, Diagnostic); };
		return const_cast<FScriptMap&>(Map).FindPairIndex(Key, Layout, Hash, Equal);
	}
	FScriptSetLayout SetLayout(const FAngelscriptTemplateInstanceOperations& Operations)
	{
		return FScriptSet::GetScriptLayout(Operations.Arguments[0].Size, Operations.Arguments[0].Alignment);
	}
	const void* SetAddress(const FScriptSet& Set, int32 Index, const FScriptSetLayout& Layout)
	{
		return const_cast<FScriptSet&>(Set).GetData(Index, Layout);
	}
	void* SetAddress(FScriptSet& Set, int32 Index, const FScriptSetLayout& Layout)
	{
		return Set.GetData(Index, Layout);
	}
	int32 FindSetIndex(const FAngelscriptTemplateInstanceOperations& Operations, const FScriptSet& Set,
		const void* Value, FString& Diagnostic)
	{
		const auto Layout = SetLayout(Operations);
		auto Hash = [&](const void* Address) { return HashElement(Operations.Arguments[0], Address, Diagnostic); };
		auto Equal = [&](const void* A, const void* B) { return EqualElement(Operations.Arguments[0], A, B, Diagnostic); };
		return const_cast<FScriptSet&>(Set).FindIndex(Value, Layout, Hash, Equal);
	}
	uint32 OptionalSetOffset(const FAngelscriptTemplateInstanceOperations& Operations)
	{
		return Align(Operations.Arguments[0].Size, Operations.Arguments[0].Alignment);
	}
	bool* OptionalSetFlag(void* Optional, const FAngelscriptTemplateInstanceOperations& Operations)
	{
		return reinterpret_cast<bool*>(static_cast<uint8*>(Optional) + OptionalSetOffset(Operations));
	}
	const bool* OptionalSetFlag(const void* Optional, const FAngelscriptTemplateInstanceOperations& Operations)
	{
		return reinterpret_cast<const bool*>(static_cast<const uint8*>(Optional) + OptionalSetOffset(Operations));
	}
}

bool FAngelscriptTemplateInstanceOperations::ConstructValue(void* Address, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (!Address) { Diagnostic = TEXT("template construction requires storage"); return false; }
	if (ContainerKind == EAngelscriptTemplateContainerKind::Array)
	{
		new(Address) FScriptArray(); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::Map && Arguments.Num() == 2)
	{
		new(Address) FScriptMap(); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::Set && Arguments.Num() == 1)
	{
		new(Address) FScriptSet(); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::Optional && Arguments.Num() == 1)
	{
		FMemory::Memzero(Address, ValueSize); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::ObjectPtr)
	{
		new(Address) TObjectPtr<UObject>(); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::SubclassOf)
	{
		new(Address) TSubclassOf<UObject>(); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::WeakObjectPtr)
	{
		new(Address) TWeakObjectPtr<UObject>(); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::SoftObjectPtr
		|| ContainerKind == EAngelscriptTemplateContainerKind::SoftClassPtr)
	{
		new(Address) FSoftObjectPtr(); return true;
	}
	FMemory::Memzero(Address, ValueSize); return true;
}
bool FAngelscriptTemplateInstanceOperations::CopyConstructValue(void* Address, const void* Source, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (!Address || !Source) { Diagnostic = TEXT("template copy requires source and destination storage"); return false; }
	if (ContainerKind == EAngelscriptTemplateContainerKind::Array && Arguments.Num() == 1)
	{
		const auto& Element = Arguments[0]; const auto& Input = *static_cast<const FScriptArray*>(Source);
		auto* Output = new(Address) FScriptArray();
		const int32 Count = Input.Num();
		if (Count) Output->Add(Count, Element.Size, Element.Alignment);
		int32 Copied = 0;
		for (; Copied < Count; ++Copied)
		{
			void* Destination = static_cast<uint8*>(Output->GetData()) + static_cast<SIZE_T>(Copied) * Element.Size;
			const void* Value = static_cast<const uint8*>(Input.GetData()) + static_cast<SIZE_T>(Copied) * Element.Size;
			if (!CopyElement(Element, Destination, Value, Diagnostic)) break;
		}
		if (Copied == Count) return true;
		for (int32 Index = 0; Index < Copied; ++Index)
			DestroyElement(Element, static_cast<uint8*>(Output->GetData()) + static_cast<SIZE_T>(Index) * Element.Size, Diagnostic);
		Output->~FScriptArray(); return false;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::Map && Arguments.Num() == 2)
	{
		const auto& Input = *static_cast<const FScriptMap*>(Source); auto* Output = new(Address) FScriptMap();
		const auto Layout = MapLayout(*this);
		for (int32 Index = 0; Index < Input.GetMaxIndex(); ++Index)
		{
			if (!Input.IsValidIndex(Index)) continue;
			const void* Pair = MapPair(Input, Index, Layout);
			if (!MapSetCopy(*Output, Pair, MapPairValue(Pair, Layout), Diagnostic))
			{
				MapEmpty(*Output, Diagnostic); Output->~FScriptMap(); return false;
			}
		}
		return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::Set && Arguments.Num() == 1)
	{
		const auto& Input = *static_cast<const FScriptSet*>(Source); auto* Output = new(Address) FScriptSet();
		const auto Layout = SetLayout(*this);
		for (int32 Index = 0; Index < Input.GetMaxIndex(); ++Index)
		{
			if (!Input.IsValidIndex(Index)) continue;
			if (!SetAddCopy(*Output, SetAddress(Input, Index, Layout), Diagnostic))
			{
				SetEmpty(*Output, Diagnostic); Output->~FScriptSet(); return false;
			}
		}
		return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::Optional && Arguments.Num() == 1)
	{
		const auto& Element = Arguments[0]; const uint32 SetOffset = Align(Element.Size, Element.Alignment);
		FMemory::Memzero(Address, ValueSize);
		if (!*reinterpret_cast<const bool*>(static_cast<const uint8*>(Source) + SetOffset)) return true;
		if (!CopyElement(Element, Address, Source, Diagnostic)) return false;
		*reinterpret_cast<bool*>(static_cast<uint8*>(Address) + SetOffset) = true; return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::ObjectPtr)
	{
		new(Address) TObjectPtr<UObject>(*static_cast<const TObjectPtr<UObject>*>(Source)); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::SubclassOf)
	{
		new(Address) TSubclassOf<UObject>(*static_cast<const TSubclassOf<UObject>*>(Source)); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::WeakObjectPtr)
	{
		new(Address) TWeakObjectPtr<UObject>(*static_cast<const TWeakObjectPtr<UObject>*>(Source)); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::SoftObjectPtr
		|| ContainerKind == EAngelscriptTemplateContainerKind::SoftClassPtr)
	{
		new(Address) FSoftObjectPtr(*static_cast<const FSoftObjectPtr*>(Source)); return true;
	}
	Diagnostic = TEXT("container copy is owned by its complete family migration"); return false;
}
bool FAngelscriptTemplateInstanceOperations::DestroyValue(void* Address, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (!Address) { Diagnostic = TEXT("template destruction requires storage"); return false; }
	if (ContainerKind == EAngelscriptTemplateContainerKind::Array && Arguments.Num() == 1)
	{
		const auto& Element = Arguments[0]; auto* Array = static_cast<FScriptArray*>(Address);
		for (int32 Index = 0; Index < Array->Num(); ++Index)
			if (!DestroyElement(Element, static_cast<uint8*>(Array->GetData()) + static_cast<SIZE_T>(Index) * Element.Size, Diagnostic)) return false;
		Array->~FScriptArray(); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::Map && Arguments.Num() == 2)
	{
		auto* Map = static_cast<FScriptMap*>(Address);
		if (!MapEmpty(*Map, Diagnostic)) return false;
		Map->~FScriptMap(); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::Set && Arguments.Num() == 1)
	{
		auto* Set = static_cast<FScriptSet*>(Address);
		if (!SetEmpty(*Set, Diagnostic)) return false;
		Set->~FScriptSet(); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::Optional && Arguments.Num() == 1)
	{
		const auto& Element = Arguments[0]; const uint32 SetOffset = Align(Element.Size, Element.Alignment);
		if (*reinterpret_cast<const bool*>(static_cast<const uint8*>(Address) + SetOffset)
			&& !DestroyElement(Element, Address, Diagnostic)) return false;
		return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::ObjectPtr)
	{
		static_cast<TObjectPtr<UObject>*>(Address)->~TObjectPtr(); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::SubclassOf)
	{
		static_cast<TSubclassOf<UObject>*>(Address)->~TSubclassOf(); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::WeakObjectPtr)
	{
		static_cast<TWeakObjectPtr<UObject>*>(Address)->~TWeakObjectPtr(); return true;
	}
	if (ContainerKind == EAngelscriptTemplateContainerKind::SoftObjectPtr
		|| ContainerKind == EAngelscriptTemplateContainerKind::SoftClassPtr)
	{
		static_cast<FSoftObjectPtr*>(Address)->~FSoftObjectPtr(); return true;
	}
	return true;
}
void FAngelscriptTemplateInstanceOperations::EnumerateReferences(const void* Address, TFunctionRef<void(UObject*)> Visitor) const
{
	if (!Address || Arguments.IsEmpty()) return;
	if (ContainerKind == EAngelscriptTemplateContainerKind::Array)
	{
		const auto& Element = Arguments[0]; const auto& Array = *static_cast<const FScriptArray*>(Address);
		for (int32 Index = 0; Index < Array.Num(); ++Index)
			VisitElement(Element, static_cast<const uint8*>(Array.GetData()) + static_cast<SIZE_T>(Index) * Element.Size, Visitor);
	}
	else if (ContainerKind == EAngelscriptTemplateContainerKind::Map && Arguments.Num() == 2)
	{
		const auto& Map = *static_cast<const FScriptMap*>(Address); const auto Layout = MapLayout(*this);
		for (int32 Index = 0; Index < Map.GetMaxIndex(); ++Index)
		{
			if (!Map.IsValidIndex(Index)) continue;
			const void* Pair = MapPair(Map, Index, Layout);
			VisitElement(Arguments[0], Pair, Visitor); VisitElement(Arguments[1], MapPairValue(Pair, Layout), Visitor);
		}
	}
	else if (ContainerKind == EAngelscriptTemplateContainerKind::Set)
	{
		const auto& Set = *static_cast<const FScriptSet*>(Address); const auto Layout = SetLayout(*this);
		for (int32 Index = 0; Index < Set.GetMaxIndex(); ++Index)
			if (Set.IsValidIndex(Index)) VisitElement(Arguments[0], SetAddress(Set, Index, Layout), Visitor);
	}
	else if (ContainerKind == EAngelscriptTemplateContainerKind::Optional)
	{
		const auto& Element = Arguments[0]; const uint32 SetOffset = Align(Element.Size, Element.Alignment);
		if (*reinterpret_cast<const bool*>(static_cast<const uint8*>(Address) + SetOffset)) VisitElement(Element, Address, Visitor);
	}
	else if (ContainerKind == EAngelscriptTemplateContainerKind::ObjectPtr)
	{
		if (UObject* Object = static_cast<const TObjectPtr<UObject>*>(Address)->Get()) Visitor(Object);
	}
	else if (ContainerKind == EAngelscriptTemplateContainerKind::SubclassOf)
	{
		if (UClass* Class = static_cast<const TSubclassOf<UObject>*>(Address)->Get()) Visitor(Class);
	}
}
bool FAngelscriptTemplateInstanceOperations::ArrayAddCopy(FScriptArray& Array, const void* Value, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Array || Arguments.Num() != 1 || !Value)
	{
		Diagnostic = TEXT("array add requires an array specialization and a value"); return false;
	}
	const auto& Element = Arguments[0]; const int32 Index = Array.Num();
	Array.Add(1, Element.Size, Element.Alignment);
	void* Destination = static_cast<uint8*>(Array.GetData()) + static_cast<SIZE_T>(Index) * Element.Size;
	if (CopyElement(Element, Destination, Value, Diagnostic)) return true;
	Array.Remove(Index, 1, Element.Size, Element.Alignment, EAllowShrinking::No); return false;
}
bool FAngelscriptTemplateInstanceOperations::ArrayAppendCopy(FScriptArray& Array, const FScriptArray& Other, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (&Array == &Other) { Diagnostic = TEXT("array append source must not alias its destination"); return false; }
	const int32 OriginalNum = Array.Num();
	for (int32 Index = 0; Index < Other.Num(); ++Index)
	{
		const void* Value = ArrayElement(Other, Index, Diagnostic);
		if (!Value || !ArrayAddCopy(Array, Value, Diagnostic))
		{
			while (Array.Num() > OriginalNum) if (!ArrayRemoveAt(Array, Array.Num() - 1, Diagnostic)) break;
			return false;
		}
	}
	return true;
}
bool FAngelscriptTemplateInstanceOperations::ArrayRemoveAt(FScriptArray& Array, int32 Index, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Array || Arguments.Num() != 1
		|| Index < 0 || Index >= Array.Num())
	{
		Diagnostic = FString::Printf(TEXT("array index %d is outside [0, %d)"), Index, Array.Num()); return false;
	}
	const auto& Element = Arguments[0];
	void* Address = static_cast<uint8*>(Array.GetData()) + static_cast<SIZE_T>(Index) * Element.Size;
	if (!DestroyElement(Element, Address, Diagnostic)) return false;
	Array.Remove(Index, 1, Element.Size, Element.Alignment, EAllowShrinking::No); return true;
}
bool FAngelscriptTemplateInstanceOperations::ArrayEmpty(FScriptArray& Array, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Array || Arguments.Num() != 1)
	{
		Diagnostic = TEXT("array empty requires an array specialization"); return false;
	}
	const auto& Element = Arguments[0];
	for (int32 Index = 0; Index < Array.Num(); ++Index)
		if (!DestroyElement(Element, static_cast<uint8*>(Array.GetData()) + static_cast<SIZE_T>(Index) * Element.Size, Diagnostic)) return false;
	Array.Empty(0, Element.Size, Element.Alignment); return true;
}
void* FAngelscriptTemplateInstanceOperations::ArrayElement(FScriptArray& Array, int32 Index, FString& Diagnostic) const
{
	return const_cast<void*>(static_cast<const FAngelscriptTemplateInstanceOperations*>(this)->ArrayElement(
		static_cast<const FScriptArray&>(Array), Index, Diagnostic));
}
const void* FAngelscriptTemplateInstanceOperations::ArrayElement(const FScriptArray& Array, int32 Index, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Array || Arguments.Num() != 1
		|| Index < 0 || Index >= Array.Num())
	{
		Diagnostic = FString::Printf(TEXT("array index %d is outside [0, %d)"), Index, Array.Num()); return nullptr;
	}
	return static_cast<const uint8*>(Array.GetData()) + static_cast<SIZE_T>(Index) * Arguments[0].Size;
}
int32 FAngelscriptTemplateInstanceOperations::ArrayFindIndex(const FScriptArray& Array, const void* Value, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (!Value || ContainerKind != EAngelscriptTemplateContainerKind::Array || Arguments.Num() != 1)
	{
		Diagnostic = TEXT("array find requires an array specialization and a value"); return INDEX_NONE;
	}
	for (int32 Index = 0; Index < Array.Num(); ++Index)
	{
		const void* Candidate = static_cast<const uint8*>(Array.GetData()) + static_cast<SIZE_T>(Index) * Arguments[0].Size;
		if (EqualElement(Arguments[0], Candidate, Value, Diagnostic)) return Index;
		if (!Diagnostic.IsEmpty()) return INDEX_NONE;
	}
	return INDEX_NONE;
}
bool FAngelscriptTemplateInstanceOperations::ArrayEquals(const FScriptArray& A, const FScriptArray& B, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Array || Arguments.Num() != 1)
	{
		Diagnostic = TEXT("array equality requires an array specialization"); return false;
	}
	if (A.Num() != B.Num()) return false;
	for (int32 Index = 0; Index < A.Num(); ++Index)
	{
		const SIZE_T Offset = static_cast<SIZE_T>(Index) * Arguments[0].Size;
		if (!EqualElement(Arguments[0], static_cast<const uint8*>(A.GetData()) + Offset,
			static_cast<const uint8*>(B.GetData()) + Offset, Diagnostic)) return false;
	}
	return true;
}
bool FAngelscriptTemplateInstanceOperations::MapSetCopy(FScriptMap& Map, const void* Key, const void* Value, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Map || Arguments.Num() != 2 || !Key || !Value)
	{
		Diagnostic = TEXT("map set requires a map specialization, key, and value"); return false;
	}
	const auto Layout = MapLayout(*this); int32 Index = FindMapIndex(*this, Map, Key, Diagnostic);
	if (!Diagnostic.IsEmpty()) return false;
	if (Map.IsValidIndex(Index))
	{
		void* Destination = MapPairValue(MapPair(Map, Index, Layout), Layout);
		if (!DestroyElement(Arguments[1], Destination, Diagnostic)) return false;
		return CopyElement(Arguments[1], Destination, Value, Diagnostic);
	}
	Index = Map.AddUninitialized(Layout); void* Pair = MapPair(Map, Index, Layout);
	if (!CopyElement(Arguments[0], Pair, Key, Diagnostic)) { Map.RemoveAt(Index, Layout); return false; }
	if (!CopyElement(Arguments[1], MapPairValue(Pair, Layout), Value, Diagnostic))
	{
		DestroyElement(Arguments[0], Pair, Diagnostic); Map.RemoveAt(Index, Layout); return false;
	}
	auto Hash = [&](const void* Address) { return HashElement(Arguments[0], Address, Diagnostic); };
	Map.Rehash(Layout, Hash); return Diagnostic.IsEmpty();
}
bool FAngelscriptTemplateInstanceOperations::MapFindCopy(const FScriptMap& Map, const void* Key, void* OutValue, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Map || Arguments.Num() != 2 || !Key || !OutValue)
	{
		Diagnostic = TEXT("map find requires a map specialization, key, and output storage"); return false;
	}
	const int32 Index = FindMapIndex(*this, Map, Key, Diagnostic);
	if (!Diagnostic.IsEmpty() || !Map.IsValidIndex(Index)) return false;
	const auto Layout = MapLayout(*this); return CopyElement(Arguments[1], OutValue,
		MapPairValue(MapPair(Map, Index, Layout), Layout), Diagnostic);
}
bool FAngelscriptTemplateInstanceOperations::MapRemove(FScriptMap& Map, const void* Key, FString& Diagnostic) const
{
	Diagnostic.Reset(); const int32 Index = FindMapIndex(*this, Map, Key, Diagnostic);
	if (!Diagnostic.IsEmpty() || !Map.IsValidIndex(Index)) return false;
	const auto Layout = MapLayout(*this); void* Pair = MapPair(Map, Index, Layout);
	if (!DestroyElement(Arguments[0], Pair, Diagnostic)
		|| !DestroyElement(Arguments[1], MapPairValue(Pair, Layout), Diagnostic)) return false;
	Map.RemoveAt(Index, Layout); return true;
}
bool FAngelscriptTemplateInstanceOperations::MapEmpty(FScriptMap& Map, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Map || Arguments.Num() != 2)
	{
		Diagnostic = TEXT("map empty requires a map specialization"); return false;
	}
	const auto Layout = MapLayout(*this);
	for (int32 Index = 0; Index < Map.GetMaxIndex(); ++Index)
	{
		if (!Map.IsValidIndex(Index)) continue; void* Pair = MapPair(Map, Index, Layout);
		if (!DestroyElement(Arguments[0], Pair, Diagnostic)
			|| !DestroyElement(Arguments[1], MapPairValue(Pair, Layout), Diagnostic)) return false;
	}
	Map.Empty(0, Layout); return true;
}
int32 FAngelscriptTemplateInstanceOperations::MapFirstIndex(const FScriptMap& Map, FString& Diagnostic) const
{
	return MapNextIndex(Map, INDEX_NONE, Diagnostic);
}
int32 FAngelscriptTemplateInstanceOperations::MapNextIndex(const FScriptMap& Map, int32 Index, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Map || Arguments.Num() != 2)
	{
		Diagnostic = TEXT("map iteration requires a map specialization"); return INDEX_NONE;
	}
	for (int32 Candidate = Index + 1; Candidate < Map.GetMaxIndex(); ++Candidate) if (Map.IsValidIndex(Candidate)) return Candidate;
	return INDEX_NONE;
}
const void* FAngelscriptTemplateInstanceOperations::MapKey(const FScriptMap& Map, int32 Index, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (!Map.IsValidIndex(Index)) { Diagnostic = TEXT("map iterator is not positioned on a valid entry"); return nullptr; }
	return MapPair(Map, Index, MapLayout(*this));
}
void* FAngelscriptTemplateInstanceOperations::MapValue(FScriptMap& Map, int32 Index, FString& Diagnostic) const
{
	return const_cast<void*>(static_cast<const FAngelscriptTemplateInstanceOperations*>(this)->MapValue(
		static_cast<const FScriptMap&>(Map), Index, Diagnostic));
}
const void* FAngelscriptTemplateInstanceOperations::MapValue(const FScriptMap& Map, int32 Index, FString& Diagnostic) const
{
	const void* Pair = MapKey(Map, Index, Diagnostic); return Pair ? MapPairValue(Pair, MapLayout(*this)) : nullptr;
}
bool FAngelscriptTemplateInstanceOperations::SetAddCopy(FScriptSet& Set, const void* Value, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Set || Arguments.Num() != 1 || !Value)
	{
		Diagnostic = TEXT("set add requires a set specialization and a value"); return false;
	}
	const auto& Element = Arguments[0]; const auto Layout = SetLayout(*this); bool bCopySucceeded = true;
	auto Hash = [&](const void* Address) { return HashElement(Element, Address, Diagnostic); };
	auto Equal = [&](const void* A, const void* B) { return EqualElement(Element, A, B, Diagnostic); };
	auto Construct = [&](void* Address) { bCopySucceeded = CopyElement(Element, Address, Value, Diagnostic); };
	auto Destruct = [&](void* Address) { FString Ignored; DestroyElement(Element, Address, Ignored); };
	Set.Add(Value, Layout, Hash, Equal, Construct, Destruct);
	return bCopySucceeded && Diagnostic.IsEmpty();
}
bool FAngelscriptTemplateInstanceOperations::SetContains(const FScriptSet& Set, const void* Value, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Set || Arguments.Num() != 1 || !Value)
	{
		Diagnostic = TEXT("set lookup requires a set specialization and a value"); return false;
	}
	return FindSetIndex(*this, Set, Value, Diagnostic) != INDEX_NONE;
}
bool FAngelscriptTemplateInstanceOperations::SetRemove(FScriptSet& Set, const void* Value, FString& Diagnostic) const
{
	Diagnostic.Reset(); const int32 Index = FindSetIndex(*this, Set, Value, Diagnostic);
	if (Index == INDEX_NONE) return false;
	if (!DestroyElement(Arguments[0], SetAddress(Set, Index, SetLayout(*this)), Diagnostic)) return false;
	Set.RemoveAt(Index, SetLayout(*this)); return true;
}
bool FAngelscriptTemplateInstanceOperations::SetEmpty(FScriptSet& Set, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Set || Arguments.Num() != 1)
	{
		Diagnostic = TEXT("set empty requires a set specialization"); return false;
	}
	const auto Layout = SetLayout(*this);
	for (int32 Index = 0; Index < Set.GetMaxIndex(); ++Index)
		if (Set.IsValidIndex(Index) && !DestroyElement(Arguments[0], SetAddress(Set, Index, Layout), Diagnostic)) return false;
	Set.Empty(0, Layout); return true;
}
int32 FAngelscriptTemplateInstanceOperations::SetFirstIndex(const FScriptSet& Set, FString& Diagnostic) const
{
	return SetNextIndex(Set, INDEX_NONE, Diagnostic);
}
int32 FAngelscriptTemplateInstanceOperations::SetNextIndex(const FScriptSet& Set, int32 Index, FString& Diagnostic) const
{
	Diagnostic.Reset();
	for (int32 Next = Index + 1; Next < Set.GetMaxIndex(); ++Next) if (Set.IsValidIndex(Next)) return Next;
	return INDEX_NONE;
}
void* FAngelscriptTemplateInstanceOperations::SetElement(FScriptSet& Set, int32 Index, FString& Diagnostic) const
{
	return const_cast<void*>(static_cast<const FAngelscriptTemplateInstanceOperations*>(this)->SetElement(
		static_cast<const FScriptSet&>(Set), Index, Diagnostic));
}
const void* FAngelscriptTemplateInstanceOperations::SetElement(const FScriptSet& Set, int32 Index, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Set || Arguments.Num() != 1 || !Set.IsValidIndex(Index))
	{
		Diagnostic = FString::Printf(TEXT("set index %d is not a valid entry"), Index); return nullptr;
	}
	return SetAddress(Set, Index, SetLayout(*this));
}
bool FAngelscriptTemplateInstanceOperations::OptionalIsSet(const void* Optional, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Optional || Arguments.Num() != 1 || !Optional)
	{
		Diagnostic = TEXT("optional state requires an optional specialization and storage"); return false;
	}
	return *OptionalSetFlag(Optional, *this);
}
bool FAngelscriptTemplateInstanceOperations::OptionalSetCopy(void* Optional, const void* Value, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Optional || Arguments.Num() != 1 || !Optional || !Value)
	{
		Diagnostic = TEXT("optional set requires an optional specialization, storage, and a value"); return false;
	}
	if (*OptionalSetFlag(Optional, *this) && !DestroyElement(Arguments[0], Optional, Diagnostic)) return false;
	*OptionalSetFlag(Optional, *this) = false;
	if (!CopyElement(Arguments[0], Optional, Value, Diagnostic)) return false;
	*OptionalSetFlag(Optional, *this) = true; return true;
}
bool FAngelscriptTemplateInstanceOperations::OptionalReset(void* Optional, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Optional || Arguments.Num() != 1 || !Optional)
	{
		Diagnostic = TEXT("optional reset requires an optional specialization and storage"); return false;
	}
	if (!*OptionalSetFlag(Optional, *this)) return true;
	if (!DestroyElement(Arguments[0], Optional, Diagnostic)) return false;
	*OptionalSetFlag(Optional, *this) = false; return true;
}
void* FAngelscriptTemplateInstanceOperations::OptionalValue(void* Optional, FString& Diagnostic) const
{
	return const_cast<void*>(static_cast<const FAngelscriptTemplateInstanceOperations*>(this)->OptionalValue(
		static_cast<const void*>(Optional), Diagnostic));
}
const void* FAngelscriptTemplateInstanceOperations::OptionalValue(const void* Optional, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::Optional || Arguments.Num() != 1 || !Optional)
	{
		Diagnostic = TEXT("optional value access requires an optional specialization and storage"); return nullptr;
	}
	if (!*OptionalSetFlag(Optional, *this))
	{
		Diagnostic = TEXT("optional value access requires a set value; the optional is unset"); return nullptr;
	}
	return Optional;
}
UClass* FAngelscriptTemplateInstanceOperations::GetObjectConstraint(FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::ObjectPtr
		&& ContainerKind != EAngelscriptTemplateContainerKind::SubclassOf
		&& ContainerKind != EAngelscriptTemplateContainerKind::WeakObjectPtr
		&& ContainerKind != EAngelscriptTemplateContainerKind::SoftObjectPtr
		&& ContainerKind != EAngelscriptTemplateContainerKind::SoftClassPtr)
	{
		Diagnostic = TEXT("object constraint requires a UObject pointer template specialization"); return nullptr;
	}
	if (!ObjectConstraint)
	{
		Diagnostic = TEXT("pointer template has no installed UObject class constraint"); return nullptr;
	}
	return ObjectConstraint;
}
bool FAngelscriptTemplateInstanceOperations::AssignSubclass(TSubclassOf<UObject>& Value, UClass* Class, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::SubclassOf || !ObjectConstraint)
	{
		Diagnostic = TEXT("subclass assignment requires an installed TSubclassOf constraint"); return false;
	}
	if (Class && !Class->IsChildOf(ObjectConstraint))
	{
		Diagnostic = FString::Printf(TEXT("class '%s' is not a subclass of '%s'"), *Class->GetName(), *ObjectConstraint->GetName()); return false;
	}
	Value = Class; return true;
}
bool FAngelscriptTemplateInstanceOperations::AssignWeakObject(TWeakObjectPtr<UObject>& Value, UObject* Object, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (ContainerKind != EAngelscriptTemplateContainerKind::WeakObjectPtr || !ObjectConstraint)
	{
		Diagnostic = TEXT("weak object assignment requires an installed TWeakObjectPtr constraint"); return false;
	}
	if (Object && !Object->IsA(ObjectConstraint))
	{
		Diagnostic = FString::Printf(TEXT("object '%s' does not satisfy weak pointer class '%s'"), *Object->GetName(), *ObjectConstraint->GetName()); return false;
	}
	Value = Object; return true;
}
bool FAngelscriptTemplateInstanceOperations::AssignSoftObjectPath(FSoftObjectPtr& Value, const FSoftObjectPath& Path, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if ((ContainerKind != EAngelscriptTemplateContainerKind::SoftObjectPtr
		&& ContainerKind != EAngelscriptTemplateContainerKind::SoftClassPtr) || !ObjectConstraint)
	{
		Diagnostic = TEXT("soft object path assignment requires an installed soft pointer constraint"); return false;
	}
	Value = Path; return true;
}
int32 FAngelscriptTemplateInstanceOperations::GetArgumentCount() const { return Arguments.Num(); }

namespace
{
	bool MemberFailure(const FAngelscriptTypeBindSource& Source, const FString& Declaration, const FString& Reason, FString& Diagnostic)
	{
		Diagnostic = FString::Printf(TEXT("Members: %s / %s (%s:%d), '%s': %s"), *Source.OwnerModule.ToString(),
			*Source.Provider.ToString(), *Source.File, Source.Line, *Declaration, *Reason);
		return false;
	}
	asEFunctionIdentityKind FunctionKind(const FAngelscriptTypeBindInfoMember& Member)
	{
		if (Member.Kind == EAngelscriptTypeBindMemberKind::GlobalFunction) return asEFunctionIdentityKind::Global;
		if (Member.Kind == EAngelscriptTypeBindMemberKind::Constructor) return asEFunctionIdentityKind::Constructor;
		if (Member.Kind == EAngelscriptTypeBindMemberKind::Destructor) return asEFunctionIdentityKind::Destructor;
		if (Member.Kind == EAngelscriptTypeBindMemberKind::Behaviour)
			return Member.Native.Behaviour == asBEHAVE_FACTORY || Member.Native.Behaviour == asBEHAVE_LIST_FACTORY
				? asEFunctionIdentityKind::Factory : asEFunctionIdentityKind::Behaviour;
		return asEFunctionIdentityKind::Method;
	}
	asSNativeFunctionFacts FunctionFacts(const FAngelscriptTypeBindInfoMember& Member, const FAngelscriptBindRecordingPolicy& Policy)
	{
		asSNativeFunctionFacts Facts;
		const TPair<EAngelscriptBindFunctionTrait, asEFuncTrait> Traits[] = {
			{EAngelscriptBindFunctionTrait::EditorOnly, asTRAIT_EDITOR_ONLY},
			{EAngelscriptBindFunctionTrait::Deprecated, asTRAIT_DEPRECATED},
			{EAngelscriptBindFunctionTrait::PropertyAccessor, asTRAIT_PROPERTY},
			{EAngelscriptBindFunctionTrait::GeneratedAccessor, asTRAIT_GENERATED_FUNCTION},
			{EAngelscriptBindFunctionTrait::NoDiscard, asTRAIT_NODISCARD},
			{EAngelscriptBindFunctionTrait::WorldContext, asTRAIT_USES_WORLDCONTEXT},
			{EAngelscriptBindFunctionTrait::NotCallable, asTRAIT_NOT_CALLABLE},
			{EAngelscriptBindFunctionTrait::ImplicitConstructor, asTRAIT_IMPLICITCONSTRUCTOR},
			{EAngelscriptBindFunctionTrait::ForceConstArguments, asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS},
		};
		for (const auto& Trait : Traits) if (EnumHasAnyFlags(Member.Traits, Trait.Key)) Facts.Traits |= Trait.Value;
		if (Member.bProtected) Facts.Traits |= asTRAIT_PROTECTED;
		if (FunctionKind(Member) == asEFunctionIdentityKind::Constructor) Facts.Traits |= asTRAIT_CONSTRUCTOR;
		if (FunctionKind(Member) == asEFunctionIdentityKind::Destructor) Facts.Traits |= asTRAIT_DESTRUCTOR;
		Facts.DeterminesOutputTypeArgument = Member.DeterminesOutputTypeArgument;
		Facts.DeprecationMessage = TCHAR_TO_UTF8(*Member.DeprecationMessage);
		const bool Cooked = Policy.bShipping || Policy.bSimulateCooked;
		const bool SuppressDevelopment = Policy.bTest || Cooked;
		if (Policy.bForceConstWithinDevelopmentOnlyFunctions &&
			(Member.CompileOut == EAngelscriptBindCompileOut::InTest || Member.CompileOut == EAngelscriptBindCompileOut::Check ||
			 Member.CompileOut == EAngelscriptBindCompileOut::IfNoLog)) Facts.Traits |= asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS;
		switch (Member.CompileOut)
		{
		case EAngelscriptBindCompileOut::Entirely: Facts.CompileOut = asECompileOutType::CompileOutEntirely; break;
		case EAngelscriptBindCompileOut::MethodChain: Facts.CompileOut = asECompileOutType::CompileOutAsMethodChain; break;
		case EAngelscriptBindCompileOut::InTest:
			if (SuppressDevelopment) Facts.CompileOut = asECompileOutType::CompileOutEntirely; break;
		case EAngelscriptBindCompileOut::Check:
			if (Cooked) Facts.CompileOut = asECompileOutType::CompileOutEntirely; break;
		case EAngelscriptBindCompileOut::IfNoLog:
			if (!Policy.bLogging || SuppressDevelopment) Facts.CompileOut = asECompileOutType::CompileOutEntirely; break;
		case EAngelscriptBindCompileOut::Ensure:
			if (Cooked) Facts.CompileOut = asECompileOutType::ReplaceWithFirstParam; break;
		case EAngelscriptBindCompileOut::ReplaceFirstArgumentInTest:
			if (SuppressDevelopment) Facts.CompileOut = asECompileOutType::ReplaceWithFirstParam; break;
		default: break;
		}
		return Facts;
	}
	asEFunctionDefinitionModifiers DefinitionModifiers(asDWORD Traits)
	{
		auto Result = asEFunctionDefinitionModifiers::None;
		if (Traits & asTRAIT_PROPERTY) Result = Result | asEFunctionDefinitionModifiers::Property;
		if (Traits & asTRAIT_GENERATED_FUNCTION) Result = Result | asEFunctionDefinitionModifiers::Generated;
		if (Traits & asTRAIT_NODISCARD) Result = Result | asEFunctionDefinitionModifiers::NoDiscard;
		if (Traits & asTRAIT_DEPRECATED) Result = Result | asEFunctionDefinitionModifiers::Deprecated;
		return Result;
	}
	bool GlobalIdentity(asCStableIdentityRegistry& Registry, const FString& Namespace, const FString& Name, asSStableKey& Out)
	{
		TArray<FString> Path; Namespace.ParseIntoArray(Path, TEXT("::"), true);
		asSDeclarationIdentityDescriptor D; D.StableScope = FAngelscriptTypeBindInfoDraft::StableScope;
		D.Kind = asEDeclarationIdentityKind::Namespace;
		for (const auto& Part : Path)
		{
			D.Name = Part; asSStableKey Key;
			if (Registry.InternDeclaration(D, Key) != asETypeIdentityStatus::Succeeded) return false;
			D.Owner = Key;
		}
		D.Kind = asEDeclarationIdentityKind::GlobalVariable; D.Name = Name;
		return Registry.InternDeclaration(D, Out) == asETypeIdentityStatus::Succeeded;
	}
}

bool FAngelscriptTypeBindInfoApply::ConnectNative(const FAngelscriptTypeBindInfoInstallation& Installation,
	AS_NAMESPACE_QUALIFIER asCScriptEngine& Engine, FString& Diagnostic)
{
	Diagnostic.Reset();
	if (Installation.GetImage()->GetBoundEngine() != &Engine)
	{
		Diagnostic = TEXT("Native calls: installation belongs to another engine."); return false;
	}
	TArray<asSNativeFunctionBinding> Functions;
	TArray<asSNativeGlobalBinding> Globals;
	TArray<const FAngelscriptTypeBindInfoMember*> FunctionRecords, GlobalRecords;
	for (const auto& Installed : Installation.GetMembers())
	{
		const auto* Record = Installation.GetStore().GetMember(Installed.Record);
		if (Record && EnumHasAnyFlags(Record->Traits, EAngelscriptBindFunctionTrait::NotCallable)) continue;
		if (Installed.Function && Installed.Function->funcType == asFUNC_SYSTEM)
		{
			asSNativeFunctionBinding Request;
			Request.Declaration = Installed.Function; Request.Function = Record->Native.Function;
			Request.CallConv = Record->Native.CallConv; Request.Auxiliary = Record->Native.Auxiliary;
			static_assert(sizeof(Request.Caller.FunctionCaller) == sizeof(Record->Native.Caller.FuncPtr));
			FMemory::Memcpy(&Request.Caller.FunctionCaller, &Record->Native.Caller.FuncPtr, sizeof(Request.Caller.FunctionCaller));
			Request.Caller.type = Record->Native.Caller.type;
			Request.NativeParameterCount = Record->Native.Caller.ParameterCount;
			switch (Record->FirstParameter)
			{
			case EAngelscriptBindFirstParameter::ScriptFunction: Request.FirstParameter = asEFirstParamMetaData::ScriptFunction; break;
			case EAngelscriptBindFirstParameter::ScriptObjectType: Request.FirstParameter = asEFirstParamMetaData::ScriptObjectType; break;
			default: break;
			}
			Functions.Add(Request); FunctionRecords.Add(Record);
		}
		else if (Installed.Global && !Record->bPureConstant)
		{
			Globals.Add({Installed.Global, const_cast<void*>(Record->GlobalAddress)}); GlobalRecords.Add(Record);
		}
	}
	int FailedIndex = -1;
	const auto Result = Engine.BindNativeFunction(Functions, Globals, FailedIndex);
	if (Result != asEByteCodeLinkStatus::Succeeded)
	{
		const auto* Record = FailedIndex >= 0 && FailedIndex < FunctionRecords.Num() ? FunctionRecords[FailedIndex] :
			(GlobalRecords.IsValidIndex(FailedIndex - FunctionRecords.Num()) ? GlobalRecords[FailedIndex - FunctionRecords.Num()] : nullptr);
		const FString Reason = FString::Printf(TEXT("native connection rejected signature, owner, target or engine state (status %d); batch was not published"), int(Result));
		if (Record) return MemberFailure(Record->Source, Record->Declaration, Reason, Diagnostic);
		Diagnostic = Reason; return false;
	}
	return true;
}

AS_NAMESPACE_QUALIFIER asCScriptFunction* FAngelscriptTypeBindInfoInstallation::FindFunction(FAngelscriptTypeBindMemberHandle Record) const
{
	for (const auto& Member : Members)
		if (Member.Record.Type == Record.Type && Member.Record.Index == Record.Index) return Member.Function;
	return nullptr;
}

bool FAngelscriptTypeBindInfoInstallation::BuildTemplateOperations(asCObjectType* Type, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (!Type || !Type->GetMetadataTemplateDefinition())
	{
		Diagnostic = TEXT("template operations require a concrete specialization"); return false;
	}
	if (FindTemplateOperations(Type)) return true;
	const auto* TemplateDeclaration = Draft->FindDeclaration(Type->GetMetadataTemplateDefinition());
	if (!TemplateDeclaration)
	{
		Diagnostic = TEXT("template specialization has no detached declaration recipe"); return false;
	}
	auto Operations = MakeUnique<FAngelscriptTemplateInstanceOperations>();
	Operations->Type = Type; Operations->ContainerKind = TemplateDeclaration->TemplateContainer;
	Operations->ValueSize = Type->GetSize(); Operations->ValueAlignment = Type->alignment;
	for (asUINT ArgumentIndex = 0; ArgumentIndex < Type->templateSubTypes.GetLength(); ++ArgumentIndex)
	{
		const asCDataType& ArgumentType = Type->templateSubTypes[ArgumentIndex];
		FAngelscriptTemplateElementOperations Element; Element.Type = ArgumentType;
		Element.Size = static_cast<uint32>(ArgumentType.GetSizeInMemoryBytes());
		Element.Alignment = static_cast<uint32>(ArgumentType.GetAlignment());
		if (!ArgumentType.GetTypeInfo())
		{
			Element.bPrimitive = true; Element.DisplayName = TEXT("primitive");
			Element.bCanConstruct = Element.bCanCopy = Element.bCanDestroy = Element.bCanEqual = Element.bCanHash = true;
		}
		else if (auto* NestedType = CastToObjectType(ArgumentType.GetTypeInfo()); NestedType && NestedType->GetMetadataTemplateDefinition())
		{
			if (!BuildTemplateOperations(NestedType, Diagnostic)) return false;
			Element.Nested = FindTemplateOperations(NestedType); Element.DisplayName = UTF8_TO_TCHAR(NestedType->GetName());
			Element.bCanConstruct = Element.bCanCopy = Element.bCanDestroy = Element.Nested != nullptr;
		}
		else
		{
			Element.Declaration = Draft->FindDeclaration(ArgumentType.GetTypeInfo());
			Element.DisplayName = Element.Declaration ? Element.Declaration->Name : UTF8_TO_TCHAR(ArgumentType.GetTypeInfo()->GetName());
			Element.bObjectHandle = ArgumentType.IsObjectHandle()
				|| (ArgumentType.GetTypeInfo()->flags & (asOBJ_REF | asOBJ_IMPLICIT_HANDLE)) == (asOBJ_REF | asOBJ_IMPLICIT_HANDLE);
			if (Element.bObjectHandle)
			{
				Element.Size = sizeof(void*); Element.Alignment = alignof(void*);
				Element.bCanConstruct = Element.bCanCopy = Element.bCanDestroy = Element.bCanEqual = Element.bCanHash = true;
			}
			else if (Element.Declaration)
			{
				Element.bEnumScalar = Element.Declaration->Kind == EAngelscriptTypeBindKind::Enum;
				if (Element.bEnumScalar)
				{
					Element.Size = Element.Declaration->NativeSize;
					Element.Alignment = Element.Declaration->NativeAlignment;
				}
				if (Element.Declaration->UnrealType && Draft->GetStore().GetReflectedSnapshot().IsValid())
					Element.Reflected = Draft->GetStore().GetReflectedSnapshot()->Find(Element.Declaration->UnrealType);
				Element.bCanConstruct = Element.bEnumScalar || Element.Declaration->DefaultConstruct != nullptr
					|| (Element.Reflected && Element.Reflected->Lifetime.Initialize != nullptr);
				Element.bCanCopy = Element.bEnumScalar || Element.Declaration->bTriviallyCopyable || Element.Declaration->CopyConstruct != nullptr
					|| (Element.Reflected && Element.Reflected->Lifetime.Copy != nullptr);
				Element.bCanDestroy = Element.bEnumScalar || Element.Declaration->bTriviallyDestructible || Element.Declaration->Destroy != nullptr
					|| (Element.Reflected && Element.Reflected->Lifetime.Destroy != nullptr);
				Element.bCanEqual = Element.bEnumScalar || Element.Declaration->Equal != nullptr;
				Element.bCanHash = Element.bEnumScalar || Element.Declaration->Hash != nullptr;
			}
		}
		if (Element.Size == 0 || Element.Alignment == 0)
		{
			Diagnostic = FString::Printf(TEXT("template element '%s' has no complete native layout"),
				*Element.DisplayName);
			return false;
		}
		const auto FailElement = [&](const TCHAR* Capability)
		{
			const auto* Source = Element.Declaration ? &Element.Declaration->Source : &TemplateDeclaration->Source;
			Diagnostic = FString::Printf(TEXT("Template operations: %s / %s (%s:%d), element '%s' has no %s recipe."),
				*Source->OwnerModule.ToString(), *Source->Provider.ToString(), *Source->File, Source->Line,
				*Element.DisplayName, Capability);
			return false;
		};
		if (!Element.bCanConstruct) return FailElement(TEXT("construction"));
		if (!Element.bCanCopy) return FailElement(TEXT("copy"));
		if (!Element.bCanDestroy) return FailElement(TEXT("destruction"));
		Operations->Arguments.Add(MoveTemp(Element));
	}
	const int32 ExpectedArity = TemplateDeclaration->TemplateContainer == EAngelscriptTemplateContainerKind::Map ? 2 : 1;
	if (Operations->Arguments.Num() != ExpectedArity)
	{
		Diagnostic = TEXT("template specialization argument count does not match its container recipe"); return false;
	}
	const bool bObjectPointer = TemplateDeclaration->TemplateContainer == EAngelscriptTemplateContainerKind::ObjectPtr
		|| TemplateDeclaration->TemplateContainer == EAngelscriptTemplateContainerKind::SubclassOf
		|| TemplateDeclaration->TemplateContainer == EAngelscriptTemplateContainerKind::WeakObjectPtr
		|| TemplateDeclaration->TemplateContainer == EAngelscriptTemplateContainerKind::SoftObjectPtr
		|| TemplateDeclaration->TemplateContainer == EAngelscriptTemplateContainerKind::SoftClassPtr;
	if (bObjectPointer)
	{
		const auto& Argument = Operations->Arguments[0];
		const auto& Snapshot = GetStore().GetReflectedSnapshot();
		if (Snapshot.IsValid() && Argument.Declaration)
		{
			for (const FAngelscriptReflectedTypeRecord& Record : Snapshot->GetTypes())
			{
				if (Record.bEligible && Record.Declaration.Name == Argument.Declaration->Name
					&& Record.Declaration.Namespace == Argument.Declaration->Namespace)
				{
					Operations->ObjectConstraint = Cast<UClass>(Record.NativeType); break;
				}
			}
		}
		if (!Operations->ObjectConstraint)
		{
			Diagnostic = FString::Printf(TEXT("pointer template argument '%s' is not an installed reflected UObject class"), *Argument.DisplayName); return false;
		}
	}
	if (TemplateDeclaration->TemplateContainer == EAngelscriptTemplateContainerKind::Set
		|| TemplateDeclaration->TemplateContainer == EAngelscriptTemplateContainerKind::Map)
	{
		const auto& Key = Operations->Arguments[0];
		const auto* Source = Key.Declaration ? &Key.Declaration->Source : &TemplateDeclaration->Source;
		if (!Key.bCanHash || !Key.bCanEqual)
		{
			Diagnostic = FString::Printf(TEXT("Template operations: %s / %s (%s:%d), key element '%s' has no %s recipe."),
				*Source->OwnerModule.ToString(), *Source->Provider.ToString(), *Source->File, Source->Line, *Key.DisplayName,
				!Key.bCanHash ? TEXT("hash") : TEXT("equality"));
			return false;
		}
	}
	Type->plainUserData = reinterpret_cast<asPWORD>(Operations.Get());
	TemplateOperations.Add(MoveTemp(Operations)); return true;
}

const FAngelscriptTemplateInstanceOperations* FAngelscriptTypeBindInfoInstallation::FindTemplateOperations(
	const AS_NAMESPACE_QUALIFIER asCObjectType* Type) const
{
	for (const auto& Operations : TemplateOperations) if (Operations->Type == Type) return Operations.Get();
	return nullptr;
}

namespace
{
	const FAngelscriptReflectedTypeRecord* ResolveInstalledReflectedType(
		const FAngelscriptTypeBindInfoInstallation& Installation, const asCTypeInfo* OwnerType,
		UObject* NativeType, FString& Diagnostic)
	{
		Diagnostic.Reset(); const auto& Snapshot = Installation.GetStore().GetReflectedSnapshot();
		if (!Snapshot.IsValid() || !OwnerType || !NativeType)
		{
			Diagnostic = TEXT("reflected access requires an installed owner type and native identity"); return nullptr;
		}
		const auto* Record = Snapshot->Find(NativeType);
		if (!Record || !Record->bEligible)
		{
			Diagnostic = FString::Printf(TEXT("native reflected type '%s' is not eligible in this owner"), *NativeType->GetPathName()); return nullptr;
		}
		if (Installation.FindType(Record->Declaration.Name, Record->Declaration.Namespace) != OwnerType)
		{
			Diagnostic = FString::Printf(TEXT("reflected type '%s' belongs to a different binding owner"), *Record->Declaration.Name); return nullptr;
		}
		return Record;
	}
	const FAngelscriptReflectedProperty* FindReflectedProperty(
		const FAngelscriptReflectedTypeRecord& Record, FName Name, FString& Diagnostic)
	{
		const auto* Property = Record.Properties.FindByPredicate([&](const auto& Candidate) { return Candidate.Name == Name.ToString(); });
		if (!Property || !Property->NativeProperty)
			Diagnostic = FString::Printf(TEXT("reflected property '%s.%s' was not captured"), *Record.Declaration.Name, *Name.ToString());
		return Property;
	}
}

bool FAngelscriptTypeBindInfoInstallation::ReadReflectedProperty(const asCTypeInfo* OwnerType, UObject* NativeType,
	const void* Container, FName PropertyName, void* OutValue, FString& Diagnostic) const
{
	const auto* Record = ResolveInstalledReflectedType(*this, OwnerType, NativeType, Diagnostic);
	if (!Record || !Container || !OutValue) { if (Diagnostic.IsEmpty()) Diagnostic = TEXT("reflected read requires container and output storage"); return false; }
	const auto* Property = FindReflectedProperty(*Record, PropertyName, Diagnostic); if (!Property) return false;
	if (const UClass* Class = Cast<UClass>(NativeType); Class && !static_cast<const UObject*>(Container)->IsA(Class))
	{
		Diagnostic = TEXT("reflected property container does not match its native class"); return false;
	}
	Property->NativeProperty->CopySingleValue(OutValue, Property->NativeProperty->ContainerPtrToValuePtr<void>(Container)); return true;
}

bool FAngelscriptTypeBindInfoInstallation::WriteReflectedProperty(const asCTypeInfo* OwnerType, UObject* NativeType,
	void* Container, FName PropertyName, const void* Value, FString& Diagnostic) const
{
	const auto* Record = ResolveInstalledReflectedType(*this, OwnerType, NativeType, Diagnostic);
	if (!Record || !Container || !Value) { if (Diagnostic.IsEmpty()) Diagnostic = TEXT("reflected write requires container and input storage"); return false; }
	const auto* Property = FindReflectedProperty(*Record, PropertyName, Diagnostic); if (!Property) return false;
	if ((Property->Flags & (CPF_BlueprintReadOnly | CPF_EditConst | CPF_ConstParm)) != 0)
	{
		Diagnostic = FString::Printf(TEXT("reflected property '%s.%s' is read-only"), *Record->Declaration.Name, *Property->Name); return false;
	}
	if (const UClass* Class = Cast<UClass>(NativeType); Class && !static_cast<UObject*>(Container)->IsA(Class))
	{
		Diagnostic = TEXT("reflected property container does not match its native class"); return false;
	}
	Property->NativeProperty->CopySingleValue(Property->NativeProperty->ContainerPtrToValuePtr<void>(Container), Value); return true;
}

bool FAngelscriptTypeBindInfoInstallation::ReflectedTypeImplements(const asCTypeInfo* OwnerType, UClass* NativeClass,
	UClass* InterfaceClass, FString& Diagnostic) const
{
	const auto* Record = ResolveInstalledReflectedType(*this, OwnerType, NativeClass, Diagnostic); if (!Record || !InterfaceClass) return false;
	if (!InterfaceClass->HasAnyClassFlags(CLASS_Interface)) { Diagnostic = TEXT("target reflected type is not an interface"); return false; }
	return Record->Interfaces.Contains(InterfaceClass) || NativeClass->ImplementsInterface(InterfaceClass);
}

bool FAngelscriptTypeBindInfoInstallation::InitializeReflectedStruct(const asCTypeInfo* OwnerType, UScriptStruct* Struct,
	void* Address, FString& Diagnostic) const
{
	const auto* Record = ResolveInstalledReflectedType(*this, OwnerType, Struct, Diagnostic);
	if (!Record || !Address || !Record->Lifetime.Initialize) { if (Diagnostic.IsEmpty()) Diagnostic = TEXT("reflected struct has no initialize recipe"); return false; }
	Record->Lifetime.Initialize(Struct, Address); return true;
}

bool FAngelscriptTypeBindInfoInstallation::CopyReflectedStruct(const asCTypeInfo* OwnerType, UScriptStruct* Struct,
	void* Destination, const void* Source, FString& Diagnostic) const
{
	const auto* Record = ResolveInstalledReflectedType(*this, OwnerType, Struct, Diagnostic);
	if (!Record || !Destination || !Source || !Record->Lifetime.Copy) { if (Diagnostic.IsEmpty()) Diagnostic = TEXT("reflected struct has no copy recipe"); return false; }
	Record->Lifetime.Copy(Struct, Destination, Source); return true;
}

bool FAngelscriptTypeBindInfoInstallation::DestroyReflectedStruct(const asCTypeInfo* OwnerType, UScriptStruct* Struct,
	void* Address, FString& Diagnostic) const
{
	const auto* Record = ResolveInstalledReflectedType(*this, OwnerType, Struct, Diagnostic);
	if (!Record || !Address || !Record->Lifetime.Destroy) { if (Diagnostic.IsEmpty()) Diagnostic = TEXT("reflected struct has no destruction recipe"); return false; }
	Record->Lifetime.Destroy(Struct, Address); return true;
}

bool FAngelscriptTypeBindInfoInstallation::RetainReflectedObject(UObject* Object, FString& Diagnostic) const
{
	Diagnostic.Reset(); if (!IsValid(Object)) { Diagnostic = TEXT("cannot retain an invalid reflected object"); return false; }
	if (!ReflectedObjectReferences.ContainsByPredicate([Object](const auto& Existing) { return Existing.Get() == Object; }))
		ReflectedObjectReferences.Emplace(Object);
	return true;
}

bool FAngelscriptTypeBindInfoInstallation::AssignStrongObject(const FAngelscriptTemplateInstanceOperations& Operations,
	TObjectPtr<UObject>& Value, UObject* Object, FString& Diagnostic) const
{
	Diagnostic.Reset();
	if (Operations.ContainerKind != EAngelscriptTemplateContainerKind::ObjectPtr
		|| FindTemplateOperations(Operations.Type) != &Operations || !Operations.ObjectConstraint)
	{
		Diagnostic = TEXT("strong object assignment requires an engine-local TObjectPtr specialization"); return false;
	}
	if (Object && !Object->IsA(Operations.ObjectConstraint))
	{
		Diagnostic = FString::Printf(TEXT("object '%s' does not satisfy strong pointer class '%s'"), *Object->GetName(), *Operations.ObjectConstraint->GetName()); return false;
	}
	if (Object && !RetainReflectedObject(Object, Diagnostic)) return false;
	Value = Object; return true;
}

void FAngelscriptTypeBindInfoInstallation::ReleaseReflectedObjects() const
{
	ReflectedObjectReferences.Reset();
}

namespace
{
	UFunction* ValidateDelegateTarget(UObject* Object, FName FunctionName, UFunction* Signature, FString& Diagnostic)
	{
		Diagnostic.Reset();
		if (!IsValid(Object) || !Signature)
		{
			Diagnostic = TEXT("delegate binding requires a valid object and signature"); return nullptr;
		}
		UFunction* Function = Object->FindFunction(FunctionName);
		if (!Function)
		{
			Diagnostic = FString::Printf(TEXT("delegate target function '%s' was not found"), *FunctionName.ToString()); return nullptr;
		}
		if (!Function->IsSignatureCompatibleWith(Signature))
		{
			Diagnostic = FString::Printf(TEXT("delegate target '%s' has the wrong signature"), *FunctionName.ToString()); return nullptr;
		}
		return Function;
	}
}

bool FAngelscriptTypeBindInfoInstallation::BindDelegate(FScriptDelegate& Delegate, UObject* Object,
	FName FunctionName, UFunction* Signature, FString& Diagnostic) const
{
	if (!ValidateDelegateTarget(Object, FunctionName, Signature, Diagnostic)) return false;
	Delegate.BindUFunction(Object, FunctionName);
	auto Entry = MakeShared<FDelegateSubscription>(); Entry->Single = &Delegate; Entry->Object.Reset(Object);
	Entry->FunctionName = FunctionName; Entry->Signature = Signature; DelegateSubscriptions.Add(MoveTemp(Entry)); return true;
}
bool FAngelscriptTypeBindInfoInstallation::AddMulticastDelegate(FMulticastScriptDelegate& Delegate, UObject* Object,
	FName FunctionName, UFunction* Signature, FString& Diagnostic) const
{
	if (!ValidateDelegateTarget(Object, FunctionName, Signature, Diagnostic)) return false;
	FScriptDelegate Binding; Binding.BindUFunction(Object, FunctionName); Delegate.AddUnique(Binding);
	auto Entry = MakeShared<FDelegateSubscription>(); Entry->Multicast = &Delegate; Entry->Object.Reset(Object);
	Entry->FunctionName = FunctionName; Entry->Signature = Signature; DelegateSubscriptions.Add(MoveTemp(Entry)); return true;
}
bool FAngelscriptTypeBindInfoInstallation::RemoveMulticastDelegate(FMulticastScriptDelegate& Delegate,
	UObject* Object, FName FunctionName, FString& Diagnostic) const
{
	Diagnostic.Reset();
	const int32 Index = DelegateSubscriptions.IndexOfByPredicate([&](const TSharedPtr<FDelegateSubscription>& Entry)
	{
		return Entry && Entry->Multicast == &Delegate && Entry->Object.Get() == Object && Entry->FunctionName == FunctionName;
	});
	if (Index == INDEX_NONE) { Diagnostic = TEXT("multicast subscription does not belong to this binding owner"); return false; }
	Delegate.Remove(Object, FunctionName); DelegateSubscriptions.RemoveAt(Index); return true;
}
bool FAngelscriptTypeBindInfoInstallation::ExecuteDelegate(const FScriptDelegate& Delegate,
	UFunction* Signature, void* Parameters, FString& Diagnostic) const
{
	Diagnostic.Reset();
	const bool bOwned = DelegateSubscriptions.ContainsByPredicate([&](const TSharedPtr<FDelegateSubscription>& Entry)
	{
		return Entry && Entry->Single == &Delegate && Entry->Signature == Signature;
	});
	if (!bOwned) { Diagnostic = TEXT("delegate does not belong to this binding owner or signature"); return false; }
	if (!Delegate.IsBound()) { Diagnostic = TEXT("delegate is not bound"); return false; }
	const_cast<FScriptDelegate&>(Delegate).ProcessDelegate<UObject>(Parameters); return true;
}
bool FAngelscriptTypeBindInfoInstallation::ExecuteMulticastDelegate(const FMulticastScriptDelegate& Delegate,
	UFunction* Signature, void* Parameters, FString& Diagnostic) const
{
	Diagnostic.Reset();
	const bool bOwned = DelegateSubscriptions.ContainsByPredicate([&](const TSharedPtr<FDelegateSubscription>& Entry)
	{
		return Entry && Entry->Multicast == &Delegate && Entry->Signature == Signature;
	});
	if (!bOwned) { Diagnostic = TEXT("multicast delegate does not belong to this binding owner or signature"); return false; }
	const_cast<FMulticastScriptDelegate&>(Delegate).ProcessDelegate<UObject>(Parameters); return true;
}
bool FAngelscriptTypeBindInfoInstallation::BindDelegatePayload(UObject* Object, FName FunctionName,
	UScriptStruct* PayloadType, const void* Payload, int32& OutHandle, FString& Diagnostic) const
{
	OutHandle = INDEX_NONE; Diagnostic.Reset();
	if (!IsValid(Object) || !PayloadType || !Payload)
	{
		Diagnostic = TEXT("delegate payload binding requires an object, payload type and value"); return false;
	}
	UFunction* Function = Object->FindFunction(FunctionName);
	FProperty* Parameter = nullptr;
	if (Function && Function->NumParms == 1 && !Function->GetReturnProperty())
	{
		for (TFieldIterator<FProperty> It(Function); It; ++It)
			if (It->HasAnyPropertyFlags(CPF_Parm) && !It->HasAnyPropertyFlags(CPF_ReturnParm)) { Parameter = *It; break; }
	}
	const FStructProperty* StructParameter = CastField<FStructProperty>(Parameter);
	if (!StructParameter || StructParameter->Struct != PayloadType)
	{
		Diagnostic = FString::Printf(TEXT("delegate payload target '%s' has the wrong signature"), *FunctionName.ToString()); return false;
	}
	auto Entry = MakeShared<FDelegatePayload>(); Entry->Handle = NextDelegatePayloadHandle++;
	Entry->Object.Reset(Object); Entry->Function = Function; Entry->Parameter = Parameter;
	Entry->Payload = MakeUnique<FStructOnScope>(PayloadType);
	PayloadType->CopyScriptStruct(Entry->Payload->GetStructMemory(), Payload);
	OutHandle = Entry->Handle; DelegatePayloads.Add(MoveTemp(Entry)); return true;
}
bool FAngelscriptTypeBindInfoInstallation::ExecuteDelegatePayload(int32 Handle, FString& Diagnostic) const
{
	Diagnostic.Reset(); const TSharedPtr<FDelegatePayload>* Found = DelegatePayloads.FindByPredicate(
		[Handle](const TSharedPtr<FDelegatePayload>& Entry) { return Entry && Entry->Handle == Handle; });
	if (!Found || !IsValid((*Found)->Object.Get())) { Diagnostic = TEXT("delegate payload handle is not bound in this owner"); return false; }
	const TSharedPtr<FDelegatePayload>& Entry = *Found;
	TArray<uint8, TInlineAllocator<64>> Parameters;
	Parameters.SetNumZeroed(FMath::Max<int32>(Entry->Function->ParmsSize, 1));
	Entry->Parameter->InitializeValue_InContainer(Parameters.GetData());
	Entry->Parameter->CopyCompleteValue_InContainer(Parameters.GetData(), Entry->Payload->GetStructMemory());
	Entry->Object->ProcessEvent(Entry->Function, Parameters.GetData());
	Entry->Parameter->DestroyValue_InContainer(Parameters.GetData());
	return true;
}
bool FAngelscriptTypeBindInfoInstallation::UnbindDelegatePayload(int32 Handle, FString& Diagnostic) const
{
	Diagnostic.Reset();
	const int32 Index = DelegatePayloads.IndexOfByPredicate(
		[Handle](const TSharedPtr<FDelegatePayload>& Entry) { return Entry && Entry->Handle == Handle; });
	if (Index == INDEX_NONE) { Diagnostic = TEXT("delegate payload handle is not bound in this owner"); return false; }
	DelegatePayloads[Index]->Payload.Reset();
	DelegatePayloads.RemoveAt(Index);
	return true;
}
const FAngelscriptReflectedFunction* FAngelscriptTypeBindInfoInstallation::RegisterReflectedEvent(
	UClass* Class, FName FunctionName, FString& Diagnostic) const
{
	Diagnostic.Reset(); const auto& Snapshot = GetStore().GetReflectedSnapshot();
	const FAngelscriptReflectedTypeRecord* TypeRecord = Snapshot.IsValid() ? Snapshot->Find(Class) : nullptr;
	if (!TypeRecord || !TypeRecord->bEligible || FindType(TypeRecord->Declaration.Name, TypeRecord->Declaration.Namespace) == nullptr)
	{
		Diagnostic = TEXT("reflected event class does not belong to this binding owner"); return nullptr;
	}
	const FAngelscriptReflectedFunction* Event = TypeRecord->Functions.FindByPredicate([&](const FAngelscriptReflectedFunction& Function)
	{
		return Function.bEligible && Function.Name == FunctionName.ToString() && Function.NativeFunction
			&& Function.NativeFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent);
	});
	if (!Event) { Diagnostic = FString::Printf(TEXT("eligible reflected event '%s' was not captured"), *FunctionName.ToString()); return nullptr; }
	ReflectedEventRegistry.AddUnique(Event); return Event;
}
int32 FAngelscriptTypeBindInfoInstallation::GetDelegateRegistryEntryCount() const
{
	return DelegateSubscriptions.Num() + DelegatePayloads.Num() + ReflectedEventRegistry.Num();
}

bool FAngelscriptTypeBindInfoInstallation::InvokeCollisionLineTraceTestByChannel(UWorld* World,
	const FVector& Start, const FVector& End, ECollisionChannel Channel,
	const FCollisionQueryParams& QueryParams, const FCollisionResponseParams& ResponseParams,
	bool& OutHit, FString& Diagnostic) const
{
	OutHit = false;
	bool bInstalled = false;
	for (const FAngelscriptTypeBindInfo& Type : GetStore().GetTypes())
	{
		for (const FAngelscriptTypeBindInfoMember& Member : Type.Members)
		{
			if (Member.Source.Provider == TEXT("WorldCollision.ExplicitBindings")
				&& Member.Declaration.StartsWith(TEXT("bool LineTraceTestByChannel("), ESearchCase::CaseSensitive))
			{
				bInstalled = true;
				break;
			}
		}
		if (bInstalled) break;
	}
	if (!bInstalled)
	{
		Diagnostic = TEXT("collision query surface is not installed in this binding owner");
		return false;
	}
	if (!World)
	{
		Diagnostic = TEXT("collision query requires a valid world context");
		return false;
	}
	OutHit = World->LineTraceTestByChannel(Start, End, Channel, QueryParams, ResponseParams);
	Diagnostic.Reset();
	return true;
}

const FAngelscriptReflectedFunction* FAngelscriptTypeBindInfoInstallation::FindReflectedFunction(
	const asCTypeInfo* OwnerType, UClass* NativeClass, FName FunctionName, FString& Diagnostic) const
{
	const auto* ExactRecord = ResolveInstalledReflectedType(*this, OwnerType, NativeClass, Diagnostic);
	if (!ExactRecord) return nullptr;
	const auto& Snapshot = GetStore().GetReflectedSnapshot();
	for (UClass* Search = NativeClass; Search; Search = Search->GetSuperClass())
	{
		const auto* Record = Snapshot->Find(Search);
		if (!Record) continue;
		if (const auto* Function = Record->Functions.FindByPredicate([&](const auto& Candidate)
		{
			return Candidate.bEligible && Candidate.Name.Equals(FunctionName.ToString(), ESearchCase::CaseSensitive);
		}))
		{
			Diagnostic.Reset();
			return Function;
		}
	}
	Diagnostic = FString::Printf(TEXT("eligible reflected function '%s.%s' was not captured"),
		*ExactRecord->Declaration.Name, *FunctionName.ToString());
	return nullptr;
}

int32 FAngelscriptTypeBindInfoInstallation::GetInstalledReflectedFunctionCount() const
{
	int32 Count = 0;
	const auto& Snapshot = GetStore().GetReflectedSnapshot();
	if (!Snapshot) return 0;
	for (const auto& Type : Snapshot->GetTypes())
	{
		if (!Type.bEligible || !FindType(Type.Declaration.Name, Type.Declaration.Namespace)) continue;
		for (const auto& Function : Type.Functions) Count += Function.bEligible ? 1 : 0;
	}
	return Count;
}

bool FAngelscriptTypeBindInfoInstallation::InvokeReflectedFunction(const asCTypeInfo* OwnerType,
	UClass* NativeClass, UObject* Receiver, FName FunctionName, TConstArrayView<void*> Arguments,
	TConstArrayView<EAngelscriptReflectedParameterDirection> Directions, void* ReturnAddress,
	EAngelscriptReflectedFunctionDispatch* OutDispatch, FString& Diagnostic) const
{
	const auto* FunctionRecord = FindReflectedFunction(OwnerType, NativeClass, FunctionName, Diagnostic);
	if (!FunctionRecord || !FunctionRecord->NativeFunction) return false;
	if (!Receiver)
	{
		Diagnostic = FString::Printf(TEXT("reflected function '%s' requires a non-null receiver"), *FunctionName.ToString());
		return false;
	}
	if (!Receiver->IsA(NativeClass))
	{
		Diagnostic = FString::Printf(TEXT("reflected function '%s' receiver is incompatible with '%s'"),
			*FunctionName.ToString(), *NativeClass->GetName());
		return false;
	}
	int32 ExpectedArguments = 0;
	for (const auto& Parameter : FunctionRecord->Parameters)
		ExpectedArguments += Parameter.Direction == EAngelscriptReflectedParameterDirection::Return ? 0 : 1;
	if (Arguments.Num() != ExpectedArguments || Directions.Num() != ExpectedArguments)
	{
		Diagnostic = FString::Printf(TEXT("reflected function '%s' expected %d typed arguments, got %d"),
			*FunctionName.ToString(), ExpectedArguments, Arguments.Num());
		return false;
	}
	int32 ArgumentIndex = 0;
	for (const auto& Parameter : FunctionRecord->Parameters)
	{
		if (Parameter.Direction == EAngelscriptReflectedParameterDirection::Return) continue;
		if (Directions[ArgumentIndex] != Parameter.Direction)
		{
			Diagnostic = FString::Printf(TEXT("reflected parameter direction mismatch for '%s.%s'"),
				*FunctionName.ToString(), *Parameter.Name);
			return false;
		}
		if (!Arguments[ArgumentIndex])
		{
			Diagnostic = FString::Printf(TEXT("reflected parameter '%s.%s' has null storage"),
				*FunctionName.ToString(), *Parameter.Name);
			return false;
		}
		if (const auto* ObjectProperty = CastField<FObjectPropertyBase>(Parameter.NativeProperty))
		{
			UObject* Value = *static_cast<UObject**>(Arguments[ArgumentIndex]);
			if (Value && !Value->IsA(ObjectProperty->PropertyClass))
			{
				Diagnostic = FString::Printf(TEXT("reflected object argument '%s.%s' is incompatible with '%s'"),
					*FunctionName.ToString(), *Parameter.Name, *ObjectProperty->PropertyClass->GetName());
				return false;
			}
		}
		++ArgumentIndex;
	}

	UFunction* Function = FunctionRecord->NativeFunction;
	const int32 BufferSize = FMath::Max<int32>(Function->ParmsSize, 1);
	uint8* ParameterBuffer = static_cast<uint8*>(FMemory_Alloca(BufferSize));
	FMemory::Memzero(ParameterBuffer, BufferSize);
	for (const auto& Parameter : FunctionRecord->Parameters)
		Parameter.NativeProperty->InitializeValue_InContainer(ParameterBuffer);
	ArgumentIndex = 0;
	FProperty* ReturnProperty = nullptr;
	for (const auto& Parameter : FunctionRecord->Parameters)
	{
		if (Parameter.Direction == EAngelscriptReflectedParameterDirection::Return)
		{
			ReturnProperty = Parameter.NativeProperty;
			continue;
		}
		if (Parameter.Direction != EAngelscriptReflectedParameterDirection::Out)
		{
			void* Destination = Parameter.NativeProperty->ContainerPtrToValuePtr<void>(ParameterBuffer);
			Parameter.NativeProperty->CopyCompleteValue(Destination, Arguments[ArgumentIndex]);
		}
		++ArgumentIndex;
	}
	Receiver->ProcessEvent(Function, ParameterBuffer);
	ArgumentIndex = 0;
	for (const auto& Parameter : FunctionRecord->Parameters)
	{
		if (Parameter.Direction == EAngelscriptReflectedParameterDirection::Return) continue;
		if (Parameter.Direction == EAngelscriptReflectedParameterDirection::Out
			|| Parameter.Direction == EAngelscriptReflectedParameterDirection::InOut)
		{
			const void* Source = Parameter.NativeProperty->ContainerPtrToValuePtr<void>(ParameterBuffer);
			Parameter.NativeProperty->CopyCompleteValue(Arguments[ArgumentIndex], Source);
		}
		++ArgumentIndex;
	}
	if (ReturnProperty)
	{
		if (!ReturnAddress)
		{
			for (const auto& Parameter : FunctionRecord->Parameters)
				Parameter.NativeProperty->DestroyValue_InContainer(ParameterBuffer);
			Diagnostic = FString::Printf(TEXT("reflected function '%s' requires return storage"), *FunctionName.ToString());
			return false;
		}
		ReturnProperty->CopyCompleteValue(ReturnAddress, ReturnProperty->ContainerPtrToValuePtr<void>(ParameterBuffer));
	}
	for (const auto& Parameter : FunctionRecord->Parameters)
		Parameter.NativeProperty->DestroyValue_InContainer(ParameterBuffer);
	if (OutDispatch) *OutDispatch = FunctionRecord->Dispatch;
	Diagnostic.Reset();
	return true;
}

const FAngelscriptReflectedFunction* FAngelscriptTypeBindInfoInstallation::FindReflectedLibraryFunction(
	UClass* LibraryClass, FName FunctionName, FString& Diagnostic) const
{
	const auto& Snapshot = GetStore().GetReflectedSnapshot();
	const auto* Record = Snapshot && LibraryClass ? Snapshot->Find(LibraryClass) : nullptr;
	if (Record)
	{
		if (const auto* Function = Record->Functions.FindByPredicate([&](const auto& Candidate)
		{
			return Candidate.bEligible && Candidate.bFunctionLibrary && Candidate.bStatic
				&& Candidate.Name.Equals(FunctionName.ToString(), ESearchCase::CaseSensitive);
		}))
		{
			Diagnostic.Reset();
			return Function;
		}
	}
	Diagnostic = FString::Printf(TEXT("eligible reflected library function '%s.%s' was not captured"),
		LibraryClass ? *LibraryClass->GetName() : TEXT("<null>"), *FunctionName.ToString());
	return nullptr;
}

bool FAngelscriptTypeBindInfoInstallation::InvokeReflectedLibraryFunction(UClass* LibraryClass,
	UObject* MixinReceiver, UObject* WorldContext, FName FunctionName, TConstArrayView<void*> Arguments,
	TConstArrayView<EAngelscriptReflectedParameterDirection> Directions, void* ReturnAddress,
	UClass** OutDeterminedType, FString& Diagnostic) const
{
	const auto* FunctionRecord = FindReflectedLibraryFunction(LibraryClass, FunctionName, Diagnostic);
	if (!FunctionRecord || !FunctionRecord->NativeFunction) return false;
	if (FunctionRecord->WorldContextParameter != INDEX_NONE && !WorldContext)
	{
		Diagnostic = FString::Printf(TEXT("reflected library function '%s' requires an explicit world context"), *FunctionName.ToString());
		return false;
	}
	TOptional<FAngelscriptGameThreadScopeWorldContext> WorldScope;
	if (FunctionRecord->WorldContextParameter != INDEX_NONE) WorldScope.Emplace(WorldContext);

	if (FunctionRecord->MixinReceiverParameter != INDEX_NONE)
	{
		const auto& ReceiverParameter = FunctionRecord->Parameters[FunctionRecord->MixinReceiverParameter];
		const auto* ReceiverProperty = CastField<FObjectPropertyBase>(ReceiverParameter.NativeProperty);
		if (!MixinReceiver || !ReceiverProperty || !MixinReceiver->IsA(ReceiverProperty->PropertyClass))
		{
			Diagnostic = FString::Printf(TEXT("reflected mixin '%s' requires a compatible receiver"), *FunctionName.ToString());
			return false;
		}
	}

	int32 ExpectedArguments = 0;
	for (int32 ParameterIndex = 0; ParameterIndex < FunctionRecord->Parameters.Num(); ++ParameterIndex)
	{
		const auto Direction = FunctionRecord->Parameters[ParameterIndex].Direction;
		if (Direction != EAngelscriptReflectedParameterDirection::Return
			&& ParameterIndex != FunctionRecord->MixinReceiverParameter
			&& ParameterIndex != FunctionRecord->WorldContextParameter) ++ExpectedArguments;
	}
	if (Arguments.Num() != ExpectedArguments || Directions.Num() != ExpectedArguments)
	{
		Diagnostic = FString::Printf(TEXT("reflected library function '%s' expected %d typed arguments, got %d"),
			*FunctionName.ToString(), ExpectedArguments, Arguments.Num());
		return false;
	}

	int32 ArgumentIndex = 0;
	for (int32 ParameterIndex = 0; ParameterIndex < FunctionRecord->Parameters.Num(); ++ParameterIndex)
	{
		const auto& Parameter = FunctionRecord->Parameters[ParameterIndex];
		if (Parameter.Direction == EAngelscriptReflectedParameterDirection::Return
			|| ParameterIndex == FunctionRecord->MixinReceiverParameter
			|| ParameterIndex == FunctionRecord->WorldContextParameter) continue;
		if (!Arguments[ArgumentIndex] || Directions[ArgumentIndex] != Parameter.Direction)
		{
			Diagnostic = FString::Printf(TEXT("reflected library parameter direction or storage mismatch for '%s.%s'"),
				*FunctionName.ToString(), *Parameter.Name);
			return false;
		}
		if (const auto* ObjectProperty = CastField<FObjectPropertyBase>(Parameter.NativeProperty))
		{
			UObject* Value = *static_cast<UObject**>(Arguments[ArgumentIndex]);
			if (Value && !Value->IsA(ObjectProperty->PropertyClass))
			{
				Diagnostic = FString::Printf(TEXT("reflected library object argument '%s.%s' is incompatible with '%s'"),
					*FunctionName.ToString(), *Parameter.Name, *ObjectProperty->PropertyClass->GetName());
				return false;
			}
		}
		++ArgumentIndex;
	}

	UFunction* Function = FunctionRecord->NativeFunction;
	UObject* Target = LibraryClass->GetDefaultObject();
	if (!Target)
	{
		Diagnostic = FString::Printf(TEXT("reflected library '%s' has no default object"), *LibraryClass->GetName());
		return false;
	}
	const int32 BufferSize = FMath::Max<int32>(Function->ParmsSize, 1);
	uint8* ParameterBuffer = static_cast<uint8*>(FMemory_Alloca(BufferSize));
	FMemory::Memzero(ParameterBuffer, BufferSize);
	for (const auto& Parameter : FunctionRecord->Parameters) Parameter.NativeProperty->InitializeValue_InContainer(ParameterBuffer);

	ArgumentIndex = 0;
	FProperty* ReturnProperty = nullptr;
	UClass* DeterminedType = nullptr;
	for (int32 ParameterIndex = 0; ParameterIndex < FunctionRecord->Parameters.Num(); ++ParameterIndex)
	{
		const auto& Parameter = FunctionRecord->Parameters[ParameterIndex];
		if (Parameter.Direction == EAngelscriptReflectedParameterDirection::Return)
		{
			ReturnProperty = Parameter.NativeProperty;
			continue;
		}
		const void* Source = nullptr;
		if (ParameterIndex == FunctionRecord->MixinReceiverParameter) Source = &MixinReceiver;
		else if (ParameterIndex == FunctionRecord->WorldContextParameter) Source = &WorldContext;
		else Source = Arguments[ArgumentIndex++];
		if (Parameter.Direction != EAngelscriptReflectedParameterDirection::Out)
		{
			void* Destination = Parameter.NativeProperty->ContainerPtrToValuePtr<void>(ParameterBuffer);
			Parameter.NativeProperty->CopyCompleteValue(Destination, Source);
		}
		if (ParameterIndex == FunctionRecord->DeterminesOutputTypeParameter && Source)
		{
			if (CastField<FClassProperty>(Parameter.NativeProperty)) DeterminedType = *static_cast<UClass* const*>(Source);
			else if (CastField<FObjectPropertyBase>(Parameter.NativeProperty))
			{
				if (UObject* Value = *static_cast<UObject* const*>(Source)) DeterminedType = Value->GetClass();
			}
		}
	}
	Target->ProcessEvent(Function, ParameterBuffer);

	ArgumentIndex = 0;
	for (int32 ParameterIndex = 0; ParameterIndex < FunctionRecord->Parameters.Num(); ++ParameterIndex)
	{
		const auto& Parameter = FunctionRecord->Parameters[ParameterIndex];
		if (Parameter.Direction == EAngelscriptReflectedParameterDirection::Return
			|| ParameterIndex == FunctionRecord->MixinReceiverParameter
			|| ParameterIndex == FunctionRecord->WorldContextParameter) continue;
		if (Parameter.Direction == EAngelscriptReflectedParameterDirection::Out
			|| Parameter.Direction == EAngelscriptReflectedParameterDirection::InOut)
		{
			const void* Source = Parameter.NativeProperty->ContainerPtrToValuePtr<void>(ParameterBuffer);
			Parameter.NativeProperty->CopyCompleteValue(Arguments[ArgumentIndex], Source);
		}
		++ArgumentIndex;
	}
	if (ReturnProperty)
	{
		if (!ReturnAddress)
		{
			for (const auto& Parameter : FunctionRecord->Parameters) Parameter.NativeProperty->DestroyValue_InContainer(ParameterBuffer);
			Diagnostic = FString::Printf(TEXT("reflected library function '%s' requires return storage"), *FunctionName.ToString());
			return false;
		}
		ReturnProperty->CopyCompleteValue(ReturnAddress, ReturnProperty->ContainerPtrToValuePtr<void>(ParameterBuffer));
	}
	for (const auto& Parameter : FunctionRecord->Parameters) Parameter.NativeProperty->DestroyValue_InContainer(ParameterBuffer);
	if (OutDeterminedType) *OutDeterminedType = DeterminedType;
	Diagnostic.Reset();
	return true;
}

const FAngelscriptNativeFunctionMapRecord* FAngelscriptTypeBindInfoInstallation::FindNativeFunctionMap(
	const FString& OwnerType, const FString& FunctionName) const
{
	return NativeFunctionMaps.FindByPredicate([&](const FAngelscriptNativeFunctionMapRecord& Record)
	{
		return Record.OwnerType.Equals(OwnerType, ESearchCase::CaseSensitive)
			&& Record.FunctionName.Equals(FunctionName, ESearchCase::CaseSensitive);
	});
}

bool FAngelscriptTypeBindInfoInstallation::InstallAlternateNativeFunctionMap(
	TConstArrayView<FAngelscriptNativeFunctionMapRecord> Records, FString& Diagnostic) const
{
	TArray<FAngelscriptNativeFunctionMapRecord> Replacement = NativeFunctionMaps;
	for (const FAngelscriptNativeFunctionMapRecord& Record : Records)
	{
		FAngelscriptNativeFunctionMapRecord* Existing = Replacement.FindByPredicate([&](const FAngelscriptNativeFunctionMapRecord& Candidate)
		{
			return Candidate.OwnerType.Equals(Record.OwnerType, ESearchCase::CaseSensitive)
				&& Candidate.FunctionName.Equals(Record.FunctionName, ESearchCase::CaseSensitive);
		});
		if (Existing == nullptr)
		{
			Diagnostic = FString::Printf(TEXT("Native function-map alternate is missing configured symbol '%s::%s' (provider '%s')."),
				*Record.OwnerType, *Record.FunctionName, *Record.Source.Provider.ToString());
			return false;
		}
		auto FailCompatibility = [&](const TCHAR* Field)
		{
			Diagnostic = FString::Printf(TEXT("Native function-map %s mismatch for '%s::%s'; configured provider '%s' (%s:%d), alternate provider '%s' (%s:%d)."),
				Field, *Record.OwnerType, *Record.FunctionName,
				*Existing->Source.Provider.ToString(), *Existing->Source.File, Existing->Source.Line,
				*Record.Source.Provider.ToString(), *Record.Source.File, Record.Source.Line);
			return false;
		};
		if (Existing->ModuleName != Record.ModuleName) return FailCompatibility(TEXT("module"));
		if (Existing->LayoutVersion != Record.LayoutVersion) return FailCompatibility(TEXT("version"));
		if (!Existing->Signature.Equals(Record.Signature, ESearchCase::CaseSensitive)
			|| Existing->ArgCount != Record.ArgCount || Existing->ReturnSize != Record.ReturnSize
			|| Existing->Flags != Record.Flags) return FailCompatibility(TEXT("signature"));
		if (Record.Thunk == nullptr || Record.bPlaceholder) return FailCompatibility(TEXT("symbol"));
		*Existing = Record;
	}
	NativeFunctionMaps = MoveTemp(Replacement);
	Diagnostic.Reset();
	return true;
}

bool FAngelscriptTypeBindInfoInstallation::InvokeNativeFunctionMap(const FString& OwnerType,
	const FString& FunctionName, UObject* Self, TConstArrayView<void*> Arguments,
	void* ReturnAddress, FString& Diagnostic) const
{
	const FAngelscriptNativeFunctionMapRecord* Record = FindNativeFunctionMap(OwnerType, FunctionName);
	if (Record == nullptr)
	{
		Diagnostic = FString::Printf(TEXT("Native function-map symbol '%s::%s' is missing from this binding owner."), *OwnerType, *FunctionName);
		return false;
	}
	if (Record->Thunk == nullptr || Record->bPlaceholder)
	{
		Diagnostic = FString::Printf(TEXT("Native function-map symbol '%s::%s' from provider '%s' has no connected native address."),
			*OwnerType, *FunctionName, *Record->Source.Provider.ToString());
		return false;
	}
	if (Arguments.Num() != Record->ArgCount || (Record->ReturnSize > 0 && ReturnAddress == nullptr))
	{
		Diagnostic = FString::Printf(TEXT("Native function-map invocation for '%s::%s' does not match provider '%s' ABI facts."),
			*OwnerType, *FunctionName, *Record->Source.Provider.ToString());
		return false;
	}
	FAngelscriptNativeModuleFunctionBindingCallFrame Frame = {
		const_cast<void**>(Arguments.GetData()), Record->ArgCount, 0,
		Record->ReturnSize > 0 ? ReturnAddress : nullptr, Self, nullptr, Record->Flags, 0
	};
	Record->Thunk(Self, &Frame);
	Diagnostic.Reset();
	return true;
}


bool FAngelscriptTypeBindInfoInstallation::MaterializeTemplateInstance(const FString& Declaration,
	asCScriptEngine& Engine, asCObjectType*& OutType, FString& Diagnostic) const
{
	OutType = nullptr; Diagnostic.Reset();
	if (GetImage()->GetBoundEngine() != &Engine)
	{
		Diagnostic = TEXT("template instance target engine does not own this installation"); return false;
	}
	std::shared_ptr<asCMetadataImage> NewImage;
	if (!Draft->MaterializeTemplateDeclaration(Declaration, NewImage, OutType, Diagnostic)) return false;
	if (!BuildTemplateOperations(OutType, Diagnostic)) { OutType = nullptr; return false; }
	if (NewImage && Engine.RegisterMetadataImage(NewImage) != asEMetadataRegistrationResult::Succeeded)
	{
		OutType = nullptr; Diagnostic = TEXT("target engine rejected the complete dependent template image"); return false;
	}
	return true;
}
bool FAngelscriptTypeBindInfoApply::Prepare(TSharedRef<const FAngelscriptTypeBindInfoStore> Store,
	TUniquePtr<FAngelscriptTypeBindInfoInstallation>& Out, FString& Diagnostic, EAngelscriptBindingStage* Stage)
{
	Out.Reset(); Diagnostic.Reset();
	if (Stage) *Stage = EAngelscriptBindingStage::Types;
	auto Installation = MakeUnique<FAngelscriptTypeBindInfoInstallation>();
	if (!FAngelscriptTypeBindInfoDraft::Create(Store, Installation->Draft, Diagnostic)) return false;
	Installation->NativeFunctionMaps.Append(Store->GetNativeFunctionMaps());
	if (Stage) *Stage = EAngelscriptBindingStage::Members;
	auto& Draft = *Installation->Draft; auto Image = Draft.GetImage();
	const auto Types = Store->GetTypes();
	for (int32 TypeIndex = 0; TypeIndex < Types.Num(); ++TypeIndex)
	{
		const auto& Record = Types[TypeIndex]; const auto& D = Record.Declaration;
		// Template member declarations describe the specialization surface. They are
		// retained in the sealed Store and materialized with the concrete template
		// instance; the frozen host image owns only template declaration facts.
		if (D.Kind == EAngelscriptTypeBindKind::Template) continue;
		auto* Owner = CastToObjectType(Draft.GetType(TypeIndex));
		for (int32 MemberIndex = 0; MemberIndex < Record.Members.Num(); ++MemberIndex)
		{
			const auto& M = Record.Members[MemberIndex];
			if (M.Kind == EAngelscriptTypeBindMemberKind::EnumValue) continue;
			const auto& Source = M.Source.File.IsEmpty() ? D.Source : M.Source;
			auto Fail = [&](const FString& Reason) { return MemberFailure(Source, M.Declaration, Reason, Diagnostic); };
			asSBindingDeclarationContext Environment;
			Environment.StableScope = FAngelscriptTypeBindInfoDraft::StableScope; Environment.Namespace = D.Namespace;
			Environment.FunctionKind = FunctionKind(M);
			if (Environment.FunctionKind != asEFunctionIdentityKind::Global && Owner) Environment.Owner = Owner->GetStableKey();
			auto Facts = FunctionFacts(M, Store->GetPolicy());
			Environment.DefinitionModifiers = DefinitionModifiers(Facts.Traits);
			asSBindingDeclaration Parsed; asSBindingDeclarationDiagnostic ParseError;
			if (!asCBindingDeclarationParser::Parse(M.Declaration, Environment, Draft.GetTypeContext().Get(),
				[&](const FString& Name, const FString& Namespace) { return Draft.FindNominal(Name, Namespace); }, Parsed, ParseError))
				return Fail(FString::Printf(TEXT("%s (bytes %u..%u)"), *ParseError.Message, ParseError.Range.Begin.ByteOffset, ParseError.Range.End.ByteOffset));
			FAngelscriptInstalledBindMember Installed; Installed.Record = {{&Store.Get(), TypeIndex}, MemberIndex};
			if (M.Kind == EAngelscriptTypeBindMemberKind::Property)
			{
				if (!Owner || !Parsed.Function.IsZero()) return Fail(TEXT("property requires an object and field declaration"));
				Installed.Property = Owner->GetFirstProperty(TCHAR_TO_UTF8(*Parsed.Name));
				if (!Installed.Property) return Fail(TEXT("property was not completed by layout installation"));
				asSNativePropertyFacts PropertyFacts;
				PropertyFacts.Readable = M.bCanRead; PropertyFacts.Writeable = M.bCanWrite; PropertyFacts.Editable = M.bCanEdit;
				PropertyFacts.Protected = M.bProtected;
				PropertyFacts.EditorOnly = EnumHasAnyFlags(M.Traits, EAngelscriptBindFunctionTrait::EditorOnly);
				PropertyFacts.Deprecated = EnumHasAnyFlags(M.Traits, EAngelscriptBindFunctionTrait::Deprecated);
				PropertyFacts.DeprecationMessage = TCHAR_TO_UTF8(*M.DeprecationMessage);
				if (Image->SetNativePropertyFacts(*Owner, *Installed.Property, PropertyFacts) != asEMetadataResult::Succeeded)
					return Fail(TEXT("invalid native property facts"));
			}
			else if (M.Kind == EAngelscriptTypeBindMemberKind::GlobalProperty)
			{
				if (!Parsed.Function.IsZero() || Parsed.Name.IsEmpty()) return Fail(TEXT("global requires a variable declaration"));
				asSStableKey Key; asCDataType Type; FString Reason;
				if (!GlobalIdentity(Draft.GetTypeContext()->GetIdentityRegistry(), D.Namespace, Parsed.Name, Key))
					return Fail(TEXT("invalid global declaration identity"));
				if (!Draft.ResolveTypeUse(Parsed.Type, Type, Reason)) return Fail(Reason);
				if (Image->CreateNativeGlobalProperty(Key, Parsed.Type, TCHAR_TO_UTF8(*Parsed.Name), TCHAR_TO_UTF8(*D.Namespace), Type, Installed.Global) != asEMetadataResult::Succeeded)
					return Fail(TEXT("invalid or duplicate native global property"));
				asSNativeGlobalFacts GlobalFacts; GlobalFacts.PureConstant = M.bPureConstant; GlobalFacts.ConstantValue = M.ConstantValue;
				GlobalFacts.EditorOnly = EnumHasAnyFlags(M.Traits, EAngelscriptBindFunctionTrait::EditorOnly);
				if (Image->SetNativeGlobalFacts(*Installed.Global, GlobalFacts) != asEMetadataResult::Succeeded)
					return Fail(TEXT("invalid native global facts or pure constant storage type"));
			}
			else
			{
				if (Parsed.Function.IsZero()) return Fail(TEXT("callable requires a complete function declaration"));
				if (Environment.FunctionKind != asEFunctionIdentityKind::Global && !Owner) return Fail(TEXT("callable owner is not an object"));
				asCDataType Return; FString Reason;
				if (!Draft.ResolveTypeUse(Parsed.Type, Return, Reason)) return Fail(Reason);
				TArray<asSMetadataParameter> Parameters;
				for (const auto& Parameter : Parsed.Parameters)
				{
					asSMetadataParameter P; P.Name = TCHAR_TO_UTF8(*Parameter.Name); P.DefaultArgument = TCHAR_TO_UTF8(*Parameter.DefaultText);
					if (!Draft.ResolveTypeUse(Parameter.Identity.Type, P.Type, Reason)) return Fail(Reason);
					switch (Parameter.Identity.PassingMode)
					{
					case asEParameterPassingMode::In: P.Flags = asTM_INREF; break;
					case asEParameterPassingMode::Out: P.Flags = asTM_OUTREF; break;
					case asEParameterPassingMode::InOut: P.Flags = asTM_INOUTREF; break;
					default: break;
					}
					Parameters.Add(MoveTemp(P));
				}
				const bool Global = Environment.FunctionKind == asEFunctionIdentityKind::Global;
				const bool Factory = Environment.FunctionKind == asEFunctionIdentityKind::Factory;
				const auto Kind = Owner && Owner->IsInterface() && !Global ? asFUNC_INTERFACE : asFUNC_SYSTEM;
				const FString Namespace = FString::Join(Parsed.Signature.NamespacePath, TEXT("::"));
				if (Image->CreateFunction(Parsed.Function, TCHAR_TO_UTF8(*Parsed.Name), TCHAR_TO_UTF8(*Namespace), Kind, Installed.Function) != asEMetadataResult::Succeeded)
					return Fail(TEXT("duplicate or invalid function identity"));
				if (Image->DefineFunction(*Installed.Function, Return, Parameters, Global || Factory ? nullptr : Owner) != asEMetadataResult::Succeeded ||
					Image->SetFunctionDeclarationFacts(*Installed.Function, Environment.FunctionKind, Global ? nullptr : Owner) != asEMetadataResult::Succeeded)
					return Fail(TEXT("invalid signature or callable owner"));
				if (Parsed.Signature.Modifiers == asEFunctionIdentityModifiers::ConstMethod) Facts.Traits |= asTRAIT_CONST;
				if (Image->SetNativeFunctionFacts(*Installed.Function, Facts) != asEMetadataResult::Succeeded)
					return Fail(TEXT("invalid native function facts"));
				if (M.Kind == EAngelscriptTypeBindMemberKind::Constructor || M.Kind == EAngelscriptTypeBindMemberKind::Destructor || M.Kind == EAngelscriptTypeBindMemberKind::Behaviour)
				{
					if (M.Native.Behaviour >= asBEHAVE_MAX || Image->AddBehaviour(*Owner, M.Native.Behaviour, *Installed.Function) != asEMetadataResult::Succeeded)
						return Fail(TEXT("invalid callable behaviour"));
				}
			}
			Installation->Members.Add(MoveTemp(Installed));
		}
	}
	if (Image->FinalizeLayouts() != asEMetadataResult::Succeeded || Image->Freeze() != asEMetadataResult::Succeeded || !Image->ValidateFrozenDefinitions())
	{
		Diagnostic = TEXT("Members: complete image failed layout, freeze or canonical definition validation."); return false;
	}
	for (asCTypeInfo* Type : Image->GetTypes())
		if (auto* Object = CastToObjectType(Type); Object && Object->GetMetadataTemplateDefinition()
			&& !Installation->BuildTemplateOperations(Object, Diagnostic)) return false;
	Out = MoveTemp(Installation);
	if (Stage) *Stage = EAngelscriptBindingStage::None;
	return true;
}

bool FAngelscriptTypeBindInfoApply::Install(TSharedRef<const FAngelscriptTypeBindInfoStore> Store,
	AS_NAMESPACE_QUALIFIER asCScriptEngine& Engine,
	TUniquePtr<FAngelscriptTypeBindInfoInstallation>& Out, FString& Diagnostic, EAngelscriptBindingStage* Stage)
{
	TUniquePtr<FAngelscriptTypeBindInfoInstallation> Prepared;
	if (!Prepare(Store, Prepared, Diagnostic, Stage)) return false;
	if (Stage) *Stage = EAngelscriptBindingStage::Members;
	if (Engine.RegisterMetadataImage(Prepared->GetImage()) != asEMetadataRegistrationResult::Succeeded)
	{
		Diagnostic = TEXT("Members: target engine rejected the complete frozen image."); return false;
	}
	Out = MoveTemp(Prepared);
	if (Stage) *Stage = EAngelscriptBindingStage::None;
	return true;
}
