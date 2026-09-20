#pragma once

#include "NativeEngine/SourceExecution/NativeSourceExecutionTestSupport.h"
#include "../Definitions/NativeDetachedDefinitionTestSupport.h"
#include "../VM/NativeVMTestSupport.h"
#include "as_generic.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "HAL/PlatformTime.h"
#include "Misc/SecureHash.h"

#include <atomic>
#include <thread>

struct FTypeOwnershipPair
{
	int32 X = 20;
	int32 Y = 22;
};

namespace TypeOwnershipTest
{
	constexpr int32 PairX = 20;
	constexpr int32 PairY = 22;
	constexpr int32 PairSum = 42;
	constexpr int32 PairOne = 1;
	constexpr int32 PairTypeCount = 128;
	constexpr int32 MethodsPerType = 4;
	constexpr int32 VmWarmupCount = 8;
	constexpr int32 VmTimedCount = 64;

	inline const TCHAR* SemanticFixtureText()
	{
		return TEXT("Pair{int32 X=20,int32 Y=22; Sum()->int32=42; GetX()->int32=20; GetY()->int32=22; One()->int32=1} types=128 methods=4");
	}

	inline FString HashUtf8(FStringView Text)
	{
		const FTCHARToUTF8 Converted(Text.GetData(), Text.Len());
		FMD5 Md5;
		Md5.Update(reinterpret_cast<const uint8*>(Converted.Get()), static_cast<uint32>(Converted.Length()));
		uint8 Digest[16];
		Md5.Final(Digest);
		return BytesToHex(Digest, 16);
	}

	inline FString SemanticFixtureHash()
	{
		return HashUtf8(SemanticFixtureText());
	}

	inline void GenericPairSum(asIScriptGeneric* Generic)
	{
		const FTypeOwnershipPair* Self = static_cast<const FTypeOwnershipPair*>(Generic->GetObject());
		Generic->SetReturnDWord(static_cast<asDWORD>(Self->X + Self->Y));
	}

	inline void GenericPairGetX(asIScriptGeneric* Generic)
	{
		const FTypeOwnershipPair* Self = static_cast<const FTypeOwnershipPair*>(Generic->GetObject());
		Generic->SetReturnDWord(static_cast<asDWORD>(Self->X));
	}

	inline void GenericPairGetY(asIScriptGeneric* Generic)
	{
		const FTypeOwnershipPair* Self = static_cast<const FTypeOwnershipPair*>(Generic->GetObject());
		Generic->SetReturnDWord(static_cast<asDWORD>(Self->Y));
	}

	inline void GenericPairOne(asIScriptGeneric* Generic)
	{
		(void)Generic;
		Generic->SetReturnDWord(static_cast<asDWORD>(PairOne));
	}

	inline void GenericNativeAdd(asIScriptGeneric* Generic)
	{
		const int32 A = static_cast<int32>(Generic->GetArgDWord(0));
		const int32 B = static_cast<int32>(Generic->GetArgDWord(1));
		Generic->SetReturnDWord(static_cast<asDWORD>(A + B));
	}

	struct FNsTimer
	{
		uint64 Start = FPlatformTime::Cycles64();

		double ElapsedNs() const
		{
			return FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - Start) * 1.0e9;
		}
	};

	struct FTestBarrier
	{
		const int32 Expected = 0;
		std::atomic<int32> Arrived{0};
		std::atomic<bool> Released{false};

		explicit FTestBarrier(int32 InExpected) : Expected(InExpected) {}

		void ArriveAndWait()
		{
			if (Arrived.fetch_add(1, std::memory_order_acq_rel) + 1 >= Expected)
			{
				Released.store(true, std::memory_order_release);
			}
			else
			{
				while (!Released.load(std::memory_order_acquire))
				{
					std::this_thread::yield();
				}
			}
		}
	};

	inline asCDataType MakeObjectRef(asCObjectType* Type)
	{
		asCDataType DataType = asCDataType::CreateType(Type, false);
		DataType.MakeReference(true);
		return DataType;
	}

	inline asSDefinitionParameter MakeObjectRefParam(const char* Name, asCObjectType* Type)
	{
		asSDefinitionParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = MakeObjectRef(Type);
		Parameter.Flags = asTM_INOUTREF;
		return Parameter;
	}

	inline asSDefinitionParameter MakeIntParam(const char* Name, asCDefinitions& Set)
	{
		asSDefinitionParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = Set.Primitive(ttInt);
		return Parameter;
	}

	struct FPairMembers
	{
		asCObjectType* Type = nullptr;
		asCObjectProperty* X = nullptr;
		asCObjectProperty* Y = nullptr;
		asCScriptFunction* Sum = nullptr;
		asCScriptFunction* GetX = nullptr;
		asCScriptFunction* GetY = nullptr;
		asCScriptFunction* One = nullptr;
		asCScriptFunction* RunSum = nullptr;
		asCScriptFunction* RunGetX = nullptr;
		asCScriptFunction* RunGetY = nullptr;
		asCScriptFunction* RunOne = nullptr;
	};

	inline bool DefineNativeIntProperty(
		FDetachedDefinitionFixture& Fixture,
		asCObjectType& Type,
		const char* Name,
		int Offset,
		asCObjectProperty*& Out)
	{
		asSNativePropertyAddress Address;
		Address.Mode = asENativeAddressMode::Direct;
		Address.Offset = Offset;
		return Fixture.Set->DefineNativeProperty(Type, Name, Fixture.Set->Primitive(ttInt), Address, Out)
			== asEDefinitionResult::Succeeded;
	}

	inline bool AddPairType(
		FDetachedDefinitionFixture& Fixture,
		const TCHAR* Name,
		FPairMembers& Out,
		bool bAddRunWrappers)
	{
		Out.Type = Fixture.DefineType(Name, TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
		if (!Out.Type)
		{
			return false;
		}
		if (Fixture.Set->SetNativeLayout(*Out.Type, sizeof(FTypeOwnershipPair), alignof(FTypeOwnershipPair))
			!= asEDefinitionResult::Succeeded)
		{
			return false;
		}
		if (!DefineNativeIntProperty(Fixture, *Out.Type, "X", static_cast<int>(offsetof(FTypeOwnershipPair, X)), Out.X)
			|| !DefineNativeIntProperty(Fixture, *Out.Type, "Y", static_cast<int>(offsetof(FTypeOwnershipPair, Y)), Out.Y))
		{
			return false;
		}
		Out.Sum = Fixture.DefineFunction(TEXT("Sum"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), {}, Out.Type);
		Out.GetX = Fixture.DefineFunction(TEXT("GetX"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), {}, Out.Type);
		Out.GetY = Fixture.DefineFunction(TEXT("GetY"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), {}, Out.Type);
		Out.One = Fixture.DefineFunction(TEXT("One"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), {}, Out.Type);
		if (!Out.Sum || !Out.GetX || !Out.GetY || !Out.One)
		{
			return false;
		}
		if (!bAddRunWrappers)
		{
			return true;
		}
		asSDefinitionParameter Self = MakeObjectRefParam("self", Out.Type);
		Out.RunSum = Fixture.DefineFunction(TEXT("RunSum"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&Self, 1));
		Out.RunGetX = Fixture.DefineFunction(TEXT("RunGetX"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&Self, 1));
		Out.RunGetY = Fixture.DefineFunction(TEXT("RunGetY"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&Self, 1));
		Out.RunOne = Fixture.DefineFunction(TEXT("RunOne"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), MakeArrayView(&Self, 1));
		return Out.RunSum && Out.RunGetX && Out.RunGetY && Out.RunOne;
	}

	inline asEByteCodeImageStatus EmitThiscallInt(
		asCByteCodeImageBuilder& Builder,
		asCScriptFunction* Run,
		asCScriptFunction* Method)
	{
		asUINT MethodSlot = 0;
		asUINT RunSlot = 0;
		asEByteCodeImageStatus Status = Builder.AddFunctionRequirement(*Method, MethodSlot);
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.AddFunctionRequirement(*Run, RunSlot);
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		Status = Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, ByteArgumentDwords(*Run));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Self = ByteLocal(0);
		Status = Builder.Emit(asBC_PshVPtr, MakeArrayView(&Self, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, MethodSlot);
		Status = Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		return Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) == asEByteCodeImageStatus::Succeeded
			? Builder.EndFunction()
			: asEByteCodeImageStatus::InvalidArgument;
	}

	inline asEByteCodeLinkStatus BindPairMethods(asCScriptEngine& Engine, const FPairMembers& Pair)
	{
		asEByteCodeLinkStatus Status = Engine.BindNativeFunction(Pair.Sum->GetStableKey(), asFUNCTION(GenericPairSum), asCALL_GENERIC);
		if (Status != asEByteCodeLinkStatus::Succeeded) return Status;
		Status = Engine.BindNativeFunction(Pair.GetX->GetStableKey(), asFUNCTION(GenericPairGetX), asCALL_GENERIC);
		if (Status != asEByteCodeLinkStatus::Succeeded) return Status;
		Status = Engine.BindNativeFunction(Pair.GetY->GetStableKey(), asFUNCTION(GenericPairGetY), asCALL_GENERIC);
		if (Status != asEByteCodeLinkStatus::Succeeded) return Status;
		return Engine.BindNativeFunction(Pair.One->GetStableKey(), asFUNCTION(GenericPairOne), asCALL_GENERIC);
	}

	inline int32 ExecuteThiscall(
		asCScriptEngine* Engine,
		asCScriptFunction* Run,
		asCScriptFunction* Method,
		FTypeOwnershipPair& Pair)
	{
		asCByteCodeImageBuilder Builder;
		if (EmitThiscallInt(Builder, Run, Method) != asEByteCodeImageStatus::Succeeded) return MIN_int32;
		TUniquePtr<asCByteCodeImage> Image;
		asCExecutableSnapshot* Snapshot = nullptr;
		asIScriptContext* Context = nullptr;
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded
			|| asLinkByteCodeImage(*Engine, *Image, Snapshot) != asEByteCodeLinkStatus::Succeeded)
		{
			if (Snapshot) Snapshot->Release();
			return MIN_int32;
		}
		Context = Engine->CreateContext();
		if (!Context || Context->Prepare(Run) != 0 || Context->SetArgAddress(0, &Pair) != 0)
		{
			if (Context) Context->Release();
			if (Snapshot) Snapshot->Release();
			return MIN_int32;
		}
		const int32 Result = Context->Execute() == asEXECUTION_FINISHED
			? static_cast<int32>(Context->GetReturnDWord())
			: MIN_int32;
		Context->Release();
		Snapshot->Release();
		return Result;
	}

	inline asCObjectType* FindSetType(asCDefinitions& Set, const char* Name)
	{
		for (asCTypeInfo* Type : Set.GetTypes())
		{
			if (Type && Type->name == Name)
			{
				return static_cast<asCObjectType*>(Type);
			}
		}
		return nullptr;
	}

	inline bool CompileScriptThing(
		const TCHAR* Scope,
		FSourceExecutionInput& Input,
		TUniquePtr<asCBuilder>& Builder,
		asCObjectType*& OutType)
	{
		Input.Options.StableScope = Scope;
		Builder = MakeUnique<asCBuilder>(Input.Sources, Input.Options);
		if (!Builder->RunThrough())
		{
			return false;
		}
		OutType = FindSetType(*Builder->GetDefinitions(), "ScriptThing");
		return OutType != nullptr;
	}
}
