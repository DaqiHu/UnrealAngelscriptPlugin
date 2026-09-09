#include "AngelscriptTypeBindInfoReflection.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/PropertyOptional.h"
#include "UObject/Package.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Binds/BlueprintCallableReflectiveFallback.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptType.h"

namespace
{
FAngelscriptReflectedTypeUse DescribePropertyUse(FProperty* Property, TSet<UObject*>& Retained)
{
	FAngelscriptReflectedTypeUse Result;
	if (!Property) return Result;
	Result.Kind = Property->GetClass()->GetName();
	Result.CppType = Property->GetCPPType();
	if (auto* Array = CastField<FArrayProperty>(Property)) Result.Arguments.Add(DescribePropertyUse(Array->Inner, Retained));
	else if (auto* Set = CastField<FSetProperty>(Property)) Result.Arguments.Add(DescribePropertyUse(Set->ElementProp, Retained));
	else if (auto* Map = CastField<FMapProperty>(Property))
	{
		Result.Arguments.Add(DescribePropertyUse(Map->KeyProp, Retained)); Result.Arguments.Add(DescribePropertyUse(Map->ValueProp, Retained));
	}
	else if (auto* Optional = CastField<FOptionalProperty>(Property)) Result.Arguments.Add(DescribePropertyUse(Optional->GetValueProperty(), Retained));
	else if (auto* Enum = CastField<FEnumProperty>(Property))
	{
		Result.ReferencedType = Enum->GetEnum(); Result.Arguments.Add(DescribePropertyUse(Enum->GetUnderlyingProperty(), Retained));
	}
	else if (auto* Byte = CastField<FByteProperty>(Property)) Result.ReferencedType = Byte->Enum;
	else if (auto* Class = CastField<FClassProperty>(Property)) Result.ReferencedType = Class->MetaClass;
	else if (auto* SoftClass = CastField<FSoftClassProperty>(Property)) Result.ReferencedType = SoftClass->MetaClass;
	else if (auto* Object = CastField<FObjectPropertyBase>(Property)) Result.ReferencedType = Object->PropertyClass;
	else if (auto* Interface = CastField<FInterfaceProperty>(Property)) Result.ReferencedType = Interface->InterfaceClass;
	else if (auto* Struct = CastField<FStructProperty>(Property)) Result.ReferencedType = Struct->Struct;
	else if (auto* Delegate = CastField<FDelegateProperty>(Property)) Result.ReferencedType = Delegate->SignatureFunction;
	else if (auto* Multicast = CastField<FMulticastDelegateProperty>(Property)) Result.ReferencedType = Multicast->SignatureFunction;
	if (Result.ReferencedType) Retained.Add(Result.ReferencedType);
	return Result;
}

void DescribeProperties(UStruct* Struct, FAngelscriptReflectedTypeRecord& Record, TSet<UObject*>& Retained)
{
	for (TFieldIterator<FProperty> It(Struct, EFieldIterationFlags::IncludeDeprecated); It; ++It)
	{
		FProperty* Property = *It;
		if (Record.Kind == EAngelscriptReflectedKind::Delegate && !Property->HasAnyPropertyFlags(CPF_Parm)) continue;
		auto& Copy = Record.Properties.AddDefaulted_GetRef();
		Copy.Name = Property->GetName(); Copy.NativeProperty = Property;
		Copy.Offset = Property->GetOffset_ForInternal(); Copy.ElementSize = Property->GetElementSize(); Copy.ArrayDim = Property->ArrayDim;
		Copy.Flags = Property->GetPropertyFlags(); Copy.Type = DescribePropertyUse(Property, Retained);
	}
}

FString ReflectedTypeDeclaration(const FAngelscriptReflectedTypeUse& Type,
	const FAngelscriptReflectedTypeSnapshot& Snapshot)
{
	auto BuildTemplate = [&Type, &Snapshot](const TCHAR* TemplateName, int32 ExpectedArguments) -> FString
	{
		if (Type.Arguments.Num() != ExpectedArguments)
			return FString();
		TArray<FString> Arguments;
		Arguments.Reserve(Type.Arguments.Num());
		for (const FAngelscriptReflectedTypeUse& Argument : Type.Arguments)
		{
			FString Declaration = ReflectedTypeDeclaration(Argument, Snapshot);
			if (Declaration.IsEmpty())
				return FString();
			Arguments.Add(MoveTemp(Declaration));
		}
		return FString::Printf(TEXT("%s<%s>"), TemplateName, *FString::Join(Arguments, TEXT(", ")));
	};
	if (Type.Kind == TEXT("ArrayProperty")) return BuildTemplate(TEXT("TArray"), 1);
	if (Type.Kind == TEXT("SetProperty")) return BuildTemplate(TEXT("TSet"), 1);
	if (Type.Kind == TEXT("MapProperty")) return BuildTemplate(TEXT("TMap"), 2);
	if (Type.Kind == TEXT("OptionalProperty")) return BuildTemplate(TEXT("TOptional"), 1);
	if (Type.ReferencedType)
	{
		if (const auto* Record = Snapshot.Find(Type.ReferencedType))
		{
			if (!Record->bEligible || Record->Kind == EAngelscriptReflectedKind::Delegate)
				return FString();
			// Reflected UE classes are registered as asOBJ_IMPLICIT_HANDLE. Their
			// canonical source spelling deliberately omits an explicit '@'.
			return Record->Declaration.Name;
		}
	}
	if (Type.Kind == TEXT("Int8Property")) return TEXT("int8");
	if (Type.Kind == TEXT("Int16Property")) return TEXT("int16");
	if (Type.Kind == TEXT("IntProperty")) return TEXT("int");
	if (Type.Kind == TEXT("Int64Property")) return TEXT("int64");
	if (Type.Kind == TEXT("ByteProperty")) return TEXT("uint8");
	if (Type.Kind == TEXT("UInt16Property")) return TEXT("uint16");
	if (Type.Kind == TEXT("UInt32Property")) return TEXT("uint");
	if (Type.Kind == TEXT("UInt64Property")) return TEXT("uint64");
	if (Type.Kind == TEXT("FloatProperty")) return TEXT("float32");
	if (Type.Kind == TEXT("DoubleProperty")) return TEXT("double");
	if (Type.Kind == TEXT("BoolProperty")) return TEXT("bool");
	if (Type.Kind == TEXT("StrProperty")) return TEXT("FString");
	if (Type.Kind == TEXT("NameProperty")) return TEXT("FName");
	return FString();
}

bool UsesDelegateLanguageType(const FAngelscriptReflectedTypeUse& Type,
	const FAngelscriptReflectedTypeSnapshot& Snapshot)
{
	if (Type.ReferencedType)
	{
		if (const auto* Record = Snapshot.Find(Type.ReferencedType))
			if (Record->Kind == EAngelscriptReflectedKind::Delegate)
				return true;
	}
	for (const FAngelscriptReflectedTypeUse& Argument : Type.Arguments)
		if (UsesDelegateLanguageType(Argument, Snapshot))
			return true;
	return false;
}

FString CallableDeclarationShape(const FString& Declaration)
{
	const int32 Open = Declaration.Find(TEXT("("));
	if (Open == INDEX_NONE) return Declaration.TrimStartAndEnd();
	int32 Close = INDEX_NONE;
	int32 ParenthesisDepth = 0;
	for (int32 Index = Open; Index < Declaration.Len(); ++Index)
	{
		if (Declaration[Index] == TEXT('(')) ++ParenthesisDepth;
		else if (Declaration[Index] == TEXT(')') && --ParenthesisDepth == 0)
		{
			Close = Index;
			break;
		}
	}
	if (Close == INDEX_NONE) return Declaration.TrimStartAndEnd();

	TArray<FString> Parameters;
	int32 Start = Open + 1;
	int32 NestedParentheses = 0;
	int32 TemplateDepth = 0;
	for (int32 Index = Start; Index <= Close; ++Index)
	{
		const TCHAR Character = Index < Close ? Declaration[Index] : TEXT(',');
		if (Character == TEXT('(')) ++NestedParentheses;
		else if (Character == TEXT(')')) --NestedParentheses;
		else if (Character == TEXT('<')) ++TemplateDepth;
		else if (Character == TEXT('>')) --TemplateDepth;
		if (Character != TEXT(',') || NestedParentheses != 0 || TemplateDepth != 0) continue;

		FString Parameter = Declaration.Mid(Start, Index - Start).TrimStartAndEnd();
		int32 DefaultAt = INDEX_NONE;
		int32 ValueParentheses = 0;
		int32 ValueTemplates = 0;
		for (int32 Offset = 0; Offset < Parameter.Len(); ++Offset)
		{
			const TCHAR ValueCharacter = Parameter[Offset];
			if (ValueCharacter == TEXT('(')) ++ValueParentheses;
			else if (ValueCharacter == TEXT(')')) --ValueParentheses;
			else if (ValueCharacter == TEXT('<')) ++ValueTemplates;
			else if (ValueCharacter == TEXT('>')) --ValueTemplates;
			else if (ValueCharacter == TEXT('=') && ValueParentheses == 0 && ValueTemplates == 0)
			{
				DefaultAt = Offset;
				break;
			}
		}
		if (DefaultAt != INDEX_NONE) Parameter = Parameter.Left(DefaultAt).TrimEnd();
		Parameters.Add(MoveTemp(Parameter));
		Start = Index + 1;
	}
	const FString Shape = Declaration.Left(Open).TrimEnd() + TEXT("(") + FString::Join(Parameters, TEXT(",")) + TEXT(")")
		+ Declaration.Mid(Close + 1).TrimStartAndEnd();
	FString Normalized;
	auto IsIdentifierCharacter = [](TCHAR Character) { return FChar::IsAlnum(Character) || Character == TEXT('_'); };
	for (int32 Index = 0; Index < Shape.Len();)
	{
		if (!IsIdentifierCharacter(Shape[Index]))
		{
			Normalized.AppendChar(Shape[Index++]);
			continue;
		}
		const int32 TokenStart = Index++;
		while (Index < Shape.Len() && IsIdentifierCharacter(Shape[Index])) ++Index;
		FString Token = Shape.Mid(TokenStart, Index - TokenStart);
		if (Token == TEXT("int32")) Token = TEXT("int");
		else if (Token == TEXT("uint32")) Token = TEXT("uint");
		Normalized += Token;
	}
	return Normalized;
}

bool HasTemplateKeyRecipes(const FAngelscriptReflectedTypeUse& Type,
	const FAngelscriptReflectedTypeSnapshot& Snapshot, const FAngelscriptTypeBindInfoStore& Store)
{
	if (!Type.ReferencedType) return Type.Arguments.IsEmpty();
	const FAngelscriptReflectedTypeRecord* Record = Snapshot.Find(Type.ReferencedType);
	if (!Record || !Record->bEligible) return false;
	const FAngelscriptTypeBindInfo* Bound = Store.GetType(Store.FindType(
		Record->Declaration.Name, Record->Declaration.Namespace));
	if (!Bound) return false;
	const FAngelscriptTypeBindDeclaration& Declaration = Bound->Declaration;
	const bool bObjectHandle = Declaration.Kind == EAngelscriptTypeBindKind::ObjectHandle
		|| Declaration.Kind == EAngelscriptTypeBindKind::Interface;
	return bObjectHandle || (Declaration.Hash != nullptr && Declaration.Equal != nullptr);
}

bool HasSupportedTemplateOperations(const FAngelscriptReflectedTypeUse& Type,
	const FAngelscriptReflectedTypeSnapshot& Snapshot, const FAngelscriptTypeBindInfoStore& Store)
{
	for (const FAngelscriptReflectedTypeUse& Argument : Type.Arguments)
		if (!HasSupportedTemplateOperations(Argument, Snapshot, Store)) return false;
	if ((Type.Kind == TEXT("SetProperty") || Type.Kind == TEXT("MapProperty"))
		&& (!Type.Arguments.IsValidIndex(0) || !HasTemplateKeyRecipes(Type.Arguments[0], Snapshot, Store)))
		return false;
	return true;
}

void DescribeFunctions(UClass* Class, FAngelscriptReflectedTypeRecord& Record, TSet<UObject*>& Retained)
{
	const bool bFunctionLibrary = Class->IsChildOf(UBlueprintFunctionLibrary::StaticClass());
	const FString MixinTargets = Class->GetMetaData(TEXT("ScriptMixin"));
	for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		UFunction* Function = *It;
		auto& Copy = Record.Functions.AddDefaulted_GetRef();
		Copy.Name = Function->GetName();
		Copy.NativeFunction = Function;
		Copy.bEligible = Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_BlueprintEvent)
			|| Function->HasMetaData(TEXT("ScriptCallable"));
		if (!Copy.bEligible) Copy.ExclusionReason = TEXT("not BlueprintCallable, BlueprintPure, BlueprintEvent or ScriptCallable");
		Copy.Dispatch = Function->HasAnyFunctionFlags(FUNC_Native)
			? EAngelscriptReflectedFunctionDispatch::Generated
			: EAngelscriptReflectedFunctionDispatch::ReflectiveFallback;
		Copy.bStatic = Function->HasAnyFunctionFlags(FUNC_Static);
		Copy.bFunctionLibrary = bFunctionLibrary;
		Retained.Add(Function);
		for (TFieldIterator<FProperty> PropertyIt(Function); PropertyIt && PropertyIt->HasAnyPropertyFlags(CPF_Parm); ++PropertyIt)
		{
			FProperty* Property = *PropertyIt;
			auto& Parameter = Copy.Parameters.AddDefaulted_GetRef();
			Parameter.Name = Property->GetName();
			Parameter.NativeProperty = Property;
			Parameter.Type = DescribePropertyUse(Property, Retained);
			if (Property->HasAnyPropertyFlags(CPF_ReturnParm)) Parameter.Direction = EAngelscriptReflectedParameterDirection::Return;
			else if (Property->HasAnyPropertyFlags(CPF_OutParm))
				Parameter.Direction = Property->HasAllPropertyFlags(CPF_OutParm | CPF_ReferenceParm)
					? EAngelscriptReflectedParameterDirection::InOut : EAngelscriptReflectedParameterDirection::Out;
			else Parameter.Direction = EAngelscriptReflectedParameterDirection::In;
			if (const FString* Default = Function->FindMetaData(*FString::Printf(TEXT("CPP_Default_%s"), *Parameter.Name)))
				Parameter.DefaultValue = *Default;
		}
		const FString WorldContextName = Function->GetMetaData(TEXT("WorldContext"));
		const FString DeterminesOutputName = Function->GetMetaData(TEXT("DeterminesOutputType"));
		for (int32 Index = 0; Index < Copy.Parameters.Num(); ++Index)
		{
			if (Copy.Parameters[Index].Name == WorldContextName && !WorldContextName.IsEmpty()) Copy.WorldContextParameter = Index;
			if (Copy.Parameters[Index].Name == DeterminesOutputName && !DeterminesOutputName.IsEmpty()) Copy.DeterminesOutputTypeParameter = Index;
		}
		if (bFunctionLibrary && Copy.bStatic && !MixinTargets.IsEmpty())
		{
			const int32 First = Copy.Parameters.IndexOfByPredicate([](const auto& Parameter)
			{
				return Parameter.Direction != EAngelscriptReflectedParameterDirection::Return;
			});
			if (Copy.Parameters.IsValidIndex(First) && Copy.Parameters[First].Type.ReferencedType)
			{
				const FString NormalizedFirstType = Cast<UClass>(Copy.Parameters[First].Type.ReferencedType)
					? FAngelscriptType::GetBoundClassName(CastChecked<UClass>(Copy.Parameters[First].Type.ReferencedType))
					: Copy.Parameters[First].Type.ReferencedType->GetName();
				TArray<FString> Targets; MixinTargets.ParseIntoArrayWS(Targets);
				if (Targets.Contains(NormalizedFirstType)) Copy.MixinReceiverParameter = First;
			}
		}
	}
}

void CallRecordedReflectedFunction(asIScriptGeneric* Generic)
{
	auto* Function = static_cast<UFunction*>(Generic ? Generic->GetAuxiliary() : nullptr);
	UClass* OwnerClass = Function ? Function->GetOuterUClass() : nullptr;
	const bool bStatic = Function && Function->HasAnyFunctionFlags(FUNC_Static);
	const bool bMixin = bStatic && OwnerClass && !OwnerClass->GetMetaData(TEXT("ScriptMixin")).IsEmpty();
	auto* Target = bStatic && OwnerClass ? OwnerClass->GetDefaultObject() : static_cast<UObject*>(Generic ? Generic->GetObject() : nullptr);
	if (!InvokeReflectionFallbackFromGenericCall(Generic, Target, Function, bMixin))
		FAngelscriptEngine::Throw("Reflected UFunction invocation failed.");
}

bool IsNominalObject(UObject* Object)
{
	return Object && (Object->IsA<UClass>() || Object->IsA<UScriptStruct>() || Object->IsA<UEnum>()
		|| (Object->IsA<UFunction>() && CastChecked<UFunction>(Object)->HasAnyFunctionFlags(FUNC_Delegate)));
}
}

const FAngelscriptReflectedTypeRecord* FAngelscriptReflectedTypeSnapshot::Find(const UObject* NativeType) const
{
	const int32* Index = TypeIndices.Find(NativeType); return Index ? &Types[*Index] : nullptr;
}

bool FAngelscriptTypeBindInfoReflection::Capture(TConstArrayView<UObject*> Roots, const FAngelscriptBindRecordingPolicy& Policy,
	TSharedPtr<const FAngelscriptReflectedTypeSnapshot>& Out, FString& Diagnostic)
{
	Out.Reset(); Diagnostic.Reset();
	if (!IsInGameThread()) { Diagnostic = TEXT("Reflection declarations must be captured on the GameThread."); return false; }
	auto Snapshot = MakeShared<FAngelscriptReflectedTypeSnapshot>(); Snapshot->Policy = Policy;
	TArray<UObject*> Pending; Pending.Append(Roots.GetData(), Roots.Num()); TSet<UObject*> Seen, Retained;
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		UObject* Object = Pending[Index];
		if (!IsValid(Object) || !IsNominalObject(Object) || Seen.Contains(Object)) continue;
		Seen.Add(Object); Retained.Add(Object);
		FAngelscriptReflectedTypeRecord Record; Record.NativeType = Object; Record.NativePath = Object->GetPathName();
		if (auto* Class = Cast<UClass>(Object))
		{
			UE::Angelscript::Private::DescribeReflectedClass(Class, Policy, Record);
			Record.Parent = Class->GetSuperClass();
			if (Record.Parent) Pending.Add(Record.Parent);
			for (const auto& Interface : Class->Interfaces)
			{
				if (Interface.Class) { Record.Interfaces.Add(Interface.Class); Pending.Add(Interface.Class); }
			}
			DescribeProperties(Class, Record, Retained);
			DescribeFunctions(Class, Record, Retained);
		}
		else if (auto* Struct = Cast<UScriptStruct>(Object))
		{
			UE::Angelscript::Private::DescribeReflectedStruct(Struct, Policy, Record);
			Record.Parent = Struct->GetSuperStruct(); if (Record.Parent) Pending.Add(Record.Parent);
			DescribeProperties(Struct, Record, Retained);
		}
		else if (auto* Enum = Cast<UEnum>(Object)) UE::Angelscript::Private::DescribeReflectedEnum(Enum, Policy, Record);
		else if (auto* Signature = Cast<UFunction>(Object))
		{
			UE::Angelscript::Private::DescribeReflectedDelegate(Signature, Policy, Record);
			DescribeProperties(Signature, Record, Retained);
		}
		Snapshot->Types.Add(MoveTemp(Record));
	}
	Snapshot->Types.Sort([](const auto& A, const auto& B) { return A.NativePath.Compare(B.NativePath, ESearchCase::CaseSensitive) < 0; });
	for (int32 Index = 0; Index < Snapshot->Types.Num(); ++Index) Snapshot->TypeIndices.Add(Snapshot->Types[Index].NativeType, Index);
	for (auto& Record : Snapshot->Types)
	{
		for (FAngelscriptReflectedFunction& Function : Record.Functions)
		{
			if (!Function.bEligible) continue;
			if (Function.Parameters.ContainsByPredicate(
				[&Snapshot](const FAngelscriptReflectedFunctionParameter& Parameter)
				{
					return UsesDelegateLanguageType(Parameter.Type, *Snapshot);
				}))
			{
				Function.bEligible = false;
				Function.ExclusionReason = TEXT("Delegate-typed parameters require the separate delegate-language feature.");
			}
			else if (Function.Parameters.ContainsByPredicate(
				[&Snapshot](const FAngelscriptReflectedFunctionParameter& Parameter)
				{
					return ReflectedTypeDeclaration(Parameter.Type, *Snapshot).IsEmpty();
				}))
			{
				Function.bEligible = false;
				Function.ExclusionReason = TEXT("A parameter or return property has no reconstructed AngelScript declaration.");
			}
		}
		// Keep the exact native parent separately; script declarations use the nearest eligible base.
		for (UObject* Parent = Record.Kind == EAngelscriptReflectedKind::Interface ? nullptr : Record.Parent; Parent;)
		{
			const auto* ParentRecord = Snapshot->Find(Parent); if (!ParentRecord) break;
			if (ParentRecord->bEligible) { Record.Declaration.BaseType = ParentRecord->Declaration.Name; break; }
			Parent = ParentRecord->Parent;
		}
		for (UClass* NativeInterface : Record.Interfaces)
		{
			const auto* InterfaceRecord = Snapshot->Find(NativeInterface);
			if (InterfaceRecord && InterfaceRecord->bEligible)
				Record.Declaration.Interfaces.AddUnique(InterfaceRecord->Declaration.Name);
		}
	}
	for (UObject* Object : Retained) Snapshot->RetainedObjects.Emplace(Object);
	Out = Snapshot; return true;
}

bool FAngelscriptTypeBindInfoReflection::CaptureLoaded(const FAngelscriptBindRecordingPolicy& Policy,
	TSharedPtr<const FAngelscriptReflectedTypeSnapshot>& Out, FString& Diagnostic)
{
	Out.Reset(); Diagnostic.Reset();
	if (!IsInGameThread()) { Diagnostic = TEXT("Loaded reflection declarations must be captured on the GameThread."); return false; }
	TArray<UObject*> Roots;
	for (UClass* Class : TObjectRange<UClass>()) Roots.Add(Class);
	for (UScriptStruct* Struct : TObjectRange<UScriptStruct>()) Roots.Add(Struct);
	for (UEnum* Enum : TObjectRange<UEnum>()) Roots.Add(Enum);
	for (UFunction* Function : TObjectRange<UFunction>()) if (Function->HasAnyFunctionFlags(FUNC_Delegate)) Roots.Add(Function);
	return Capture(Roots, Policy, Out, Diagnostic);
}

bool FAngelscriptTypeBindInfoReflection::RecordDeclarations(FAngelscriptTypeBindInfoStore& Store,
	TSharedRef<const FAngelscriptReflectedTypeSnapshot> Snapshot, FString& Diagnostic, EAngelscriptReflectedDeclarationFilter Filter)
{
	if (!Store.SetReflectedSnapshot(Snapshot, Diagnostic)) return false;
	for (const auto& Record : Snapshot->GetTypes())
	{
		if (!Record.bEligible || Record.Kind == EAngelscriptReflectedKind::Delegate) continue;
		const bool bClass = Record.Kind == EAngelscriptReflectedKind::Class || Record.Kind == EAngelscriptReflectedKind::Interface;
		if (Filter == EAngelscriptReflectedDeclarationFilter::Classes && !bClass) continue;
		if (Filter == EAngelscriptReflectedDeclarationFilter::Structs && Record.Kind != EAngelscriptReflectedKind::Struct) continue;
		if (Filter == EAngelscriptReflectedDeclarationFilter::Enums && Record.Kind != EAngelscriptReflectedKind::Enum) continue;
		FAngelscriptTypeBindHandle Handle;
		if (!Store.RecordType(Record.Declaration, Handle, Diagnostic)) return false;
	}
	Diagnostic.Reset(); return true;
}

bool FAngelscriptTypeBindInfoReflection::RecordLoadedDeclarations(FAngelscriptTypeBindInfoStore& Store, FString& Diagnostic,
	EAngelscriptReflectedDeclarationFilter Filter)
{
	TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot = Store.GetReflectedSnapshot();
	if (!Snapshot.IsValid() && !CaptureLoaded(Store.GetPolicy(), Snapshot, Diagnostic)) return false;
	return RecordDeclarations(Store, Snapshot.ToSharedRef(), Diagnostic, Filter);
}

bool FAngelscriptTypeBindInfoReflection::RecordFunctions(FAngelscriptTypeBindInfoStore& Store,
	TSharedRef<const FAngelscriptReflectedTypeSnapshot> Snapshot, FString& Diagnostic,
	TConstArrayView<UObject*> NativeTypes)
{
	if (Store.GetReflectedSnapshot() != Snapshot)
	{
		Diagnostic = TEXT("Reflected functions require the Store's exact captured snapshot.");
		return false;
	}
	Store.SetSource({TEXT("AngelscriptRuntime"), TEXT("BlueprintType.ReflectionBindings"),
		TEXT("Source/AngelscriptRuntime/Binds/Bind_BlueprintType.cpp"), 1477, EAngelscriptBindPhase::ReflectionBindings});
	for (const auto& Type : Snapshot->GetTypes())
	{
		if (!NativeTypes.IsEmpty() && !NativeTypes.Contains(Type.NativeType)) continue;
		if (!Type.bEligible || Type.Kind != EAngelscriptReflectedKind::Class) continue;
		const auto TypeHandle = Store.FindType(Type.Declaration.Name, Type.Declaration.Namespace);
		if (!Store.GetType(TypeHandle)) continue;
		for (const auto& Function : Type.Functions)
		{
			if (!Function.bEligible) continue;
			if (Function.Parameters.ContainsByPredicate(
				[&Snapshot, &Store](const FAngelscriptReflectedFunctionParameter& Parameter)
				{
					return !HasSupportedTemplateOperations(Parameter.Type, *Snapshot, Store);
				}))
				continue;
			FAngelscriptTypeBindHandle OwnerHandle = TypeHandle;
			FAngelscriptTypeBindInfoMember Member;
			if (Function.bFunctionLibrary && Function.bStatic)
			{
				if (Function.MixinReceiverParameter != INDEX_NONE)
				{
					const auto* ReceiverRecord = Snapshot->Find(Function.Parameters[Function.MixinReceiverParameter].Type.ReferencedType);
					if (!ReceiverRecord || !Store.GetType(OwnerHandle = Store.FindType(ReceiverRecord->Declaration.Name, ReceiverRecord->Declaration.Namespace)))
					{
						Diagnostic = FString::Printf(TEXT("Reflected mixin '%s.%s' has no captured receiver declaration"), *Type.Declaration.Name, *Function.Name);
						return false;
					}
					Member.Kind = EAngelscriptTypeBindMemberKind::Method;
				}
				else
				{
					FAngelscriptTypeBindDeclaration Scope; Scope.Kind = EAngelscriptTypeBindKind::Namespace;
					Scope.Namespace = Type.Declaration.Name; Scope.Source = Store.GetSource();
					if (!Store.RecordType(Scope, OwnerHandle, Diagnostic)) return false;
					Member.Kind = EAngelscriptTypeBindMemberKind::GlobalFunction;
				}
			}
			else Member.Kind = EAngelscriptTypeBindMemberKind::Method;
			FString ReturnType = TEXT("void");
			TArray<FString> Arguments;
			TArray<FString> ArgumentDefaults;
			int32 LastArgumentWithoutDefault = INDEX_NONE;
			for (int32 ParameterIndex = 0; ParameterIndex < Function.Parameters.Num(); ++ParameterIndex)
			{
				const auto& Parameter = Function.Parameters[ParameterIndex];
				if (ParameterIndex == Function.MixinReceiverParameter) continue;
				FString DeclaredType = ReflectedTypeDeclaration(Parameter.Type, *Snapshot);
				if (DeclaredType.IsEmpty())
				{
					Diagnostic = FString::Printf(TEXT("Unsupported reflected parameter type for %s::%s.%s"),
						*Type.Declaration.Name, *Function.Name, *Parameter.Name);
					return false;
				}
				if (Parameter.Direction == EAngelscriptReflectedParameterDirection::Return)
				{
					ReturnType = MoveTemp(DeclaredType);
					continue;
				}
				if (Parameter.Direction == EAngelscriptReflectedParameterDirection::Out) DeclaredType += TEXT("&out");
				else if (Parameter.Direction == EAngelscriptReflectedParameterDirection::InOut) DeclaredType += TEXT("&inout");
				FString Argument = FString::Printf(TEXT("%s %s"), *DeclaredType, *Parameter.Name);
				FString ScriptDefault;
				if (!Parameter.DefaultValue.IsEmpty())
				{
					ScriptDefault = Parameter.DefaultValue;
					if (Parameter.Type.Kind == TEXT("StrProperty"))
						ScriptDefault = FString::Printf(TEXT("\"%s\""), *ScriptDefault.ReplaceCharWithEscapedChar());
					// UE serializes common struct defaults as comma-separated field values.
					// Keep the commas inside one AS expression by spelling the value constructor.
					if (ScriptDefault.Contains(TEXT(",")) && Parameter.Type.ReferencedType)
						ScriptDefault = FString::Printf(TEXT("%s(%s)"),
							*ReflectedTypeDeclaration(Parameter.Type, *Snapshot), *ScriptDefault);
				}
				Arguments.Add(MoveTemp(Argument));
				ArgumentDefaults.Add(MoveTemp(ScriptDefault));
				if (ArgumentDefaults.Last().IsEmpty())
					LastArgumentWithoutDefault = Arguments.Num() - 1;
			}
			for (int32 ArgumentIndex = LastArgumentWithoutDefault + 1; ArgumentIndex < Arguments.Num(); ++ArgumentIndex)
				if (!ArgumentDefaults[ArgumentIndex].IsEmpty())
					Arguments[ArgumentIndex] += TEXT(" = ") + ArgumentDefaults[ArgumentIndex];
			Member.Declaration = FString::Printf(TEXT("%s %s(%s)"), *ReturnType, *Function.Name,
				*FString::Join(Arguments, TEXT(", ")));
			if (Function.WorldContextParameter != INDEX_NONE) Member.Traits |= EAngelscriptBindFunctionTrait::WorldContext;
			Member.DeterminesOutputTypeArgument = Function.DeterminesOutputTypeParameter;
			if (Function.MixinReceiverParameter != INDEX_NONE && Member.DeterminesOutputTypeArgument > Function.MixinReceiverParameter)
				--Member.DeterminesOutputTypeArgument;
			Member.Native.Function = asFUNCTION(CallRecordedReflectedFunction);
			Member.Native.CallConv = asCALL_GENERIC;
			Member.Native.Auxiliary = Function.NativeFunction;
			Member.Native.FunctionLifetime = EAngelscriptBindHostLifetime::BorrowedHost;
			Member.Native.AuxiliaryLifetime = EAngelscriptBindHostLifetime::RetainedHost;
			Member.Source = Store.GetSource();
			// Fixed native/custom providers are recorded before the reflection fallback.
			// Preserve that stronger implementation when reflection describes the exact
			// same callable surface.
			if (const FAngelscriptTypeBindInfo* Owner = Store.GetType(OwnerHandle))
			{
				const bool bAlreadyRecorded = Owner->Members.ContainsByPredicate(
					[&Member](const FAngelscriptTypeBindInfoMember& Existing)
					{
						return Existing.Kind == Member.Kind
							&& CallableDeclarationShape(Existing.Declaration) == CallableDeclarationShape(Member.Declaration);
					});
				if (bAlreadyRecorded) continue;
			}
			FAngelscriptTypeBindMemberHandle Handle;
			if (!Store.RecordMember(OwnerHandle, Member, Handle, Diagnostic)) return false;
		}
	}
	Diagnostic.Reset();
	return true;
}
