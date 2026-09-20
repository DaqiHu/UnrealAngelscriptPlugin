#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_property.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace VMIntegerOpcodeMatrixPrivate
{
	asSByteCodeOperand Loc(int32 Offset)
	{
		return ByteLocal(Offset);
	}

	asSByteCodeOperand Imm32(int32 Value)
	{
		return ByteImm32(Value);
	}

	asSByteCodeOperand Imm64(int64 Value)
	{
		return ByteImm64(Value);
	}

	asSByteCodeOperand Imm16(int16 Value)
	{
		return ByteImm16(Value);
	}

	asSByteCodeOperand LabelAt(asUINT Index)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::Label;
		Operand.Label = Index;
		Operand.Immediate = static_cast<int64>(Index);
		return Operand;
	}

	asEByteCodeImageStatus Set4(asCByteCodeImageBuilder& Builder, int32 Offset, int32 Value)
	{
		asSByteCodeOperand Ops[2] = {Loc(Offset), Imm32(Value)};
		return Builder.Emit(asBC_SetV4, MakeArrayView(Ops, 2));
	}

	asEByteCodeImageStatus Set8(asCByteCodeImageBuilder& Builder, int32 Offset, int64 Value)
	{
		asSByteCodeOperand Ops[2] = {Loc(Offset), Imm64(Value)};
		return Builder.Emit(asBC_SetV8, MakeArrayView(Ops, 2));
	}

	asEByteCodeImageStatus Ternary(asCByteCodeImageBuilder& Builder, asEBCInstr Opcode, int32 Dest, int32 Left, int32 Right)
	{
		asSByteCodeOperand Ops[3] = {Loc(Dest), Loc(Left), Loc(Right)};
		return Builder.Emit(Opcode, MakeArrayView(Ops, 3));
	}

	asEByteCodeImageStatus Unary(asCByteCodeImageBuilder& Builder, asEBCInstr Opcode, int32 Offset)
	{
		asSByteCodeOperand Op = Loc(Offset);
		return Builder.Emit(Opcode, MakeArrayView(&Op, 1));
	}

	asEByteCodeImageStatus Pair(asCByteCodeImageBuilder& Builder, asEBCInstr Opcode, int32 Dest, int32 Source)
	{
		asSByteCodeOperand Ops[2] = {Loc(Dest), Loc(Source)};
		return Builder.Emit(Opcode, MakeArrayView(Ops, 2));
	}

	asEByteCodeImageStatus Ret4(asCByteCodeImageBuilder& Builder, int32 Offset, int16 Pop = 0)
	{
		asSByteCodeOperand Copy = Loc(Offset);
		const asEByteCodeImageStatus Status = Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Copy, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Ret = Imm16(Pop);
		return Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
	}

	asEByteCodeImageStatus Ret8(asCByteCodeImageBuilder& Builder, int32 Offset)
	{
		asSByteCodeOperand Copy = Loc(Offset);
		const asEByteCodeImageStatus Status = Builder.Emit(asBC_CpyVtoR8, MakeArrayView(&Copy, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Ret = Imm16(0);
		return Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
	}

	asCGlobalProperty* DefineIntGlobal(FDetachedDefinitionFixture& Fixture, const TCHAR* Name)
	{
		if (!Fixture.BindContext()) return nullptr;
		AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Namespace;
		Namespace.StableScope = Fixture.Scope;
		Namespace.Kind = AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::Namespace;
		Namespace.Name = TEXT("G");
		AS_NAMESPACE_QUALIFIER asSStableKey Owner;
		if (Fixture.Context->GetIdentityRegistry().InternDeclaration(Namespace, Owner)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Declaration;
		Declaration.StableScope = Fixture.Scope;
		Declaration.Kind = AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::GlobalVariable;
		Declaration.Name = Name;
		Declaration.Owner = Owner;
		asSStableKey Key;
		if (Fixture.Context->GetIdentityRegistry().InternDeclaration(Declaration, Key)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor TypeUse;
		TypeUse.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
		TypeUse.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32;
		TypeUse.Qualifiers = AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const;
		asSStableKey TypeKey;
		if (Fixture.Context->GetIdentityRegistry().InternTypeUse(TypeUse, TypeKey)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		asCGlobalProperty* Global = nullptr;
		if (Fixture.Set->CreateGlobalProperty(
			Key, TypeKey, TCHAR_TO_UTF8(Name), "G", Fixture.Set->Primitive(ttInt, true), Global)
			!= asEDefinitionResult::Succeeded)
		{
			return nullptr;
		}
		return Global;
	}

	asEByteCodeImageStatus AddGlobalRequirement(asCByteCodeImageBuilder& Builder, asCGlobalProperty& Global, asUINT& OutSlot)
	{
		asSByteCodeRequirement Requirement;
		Requirement.Role = asEByteCodeRequirementRole::Global;
		Requirement.Key = Global.GetStableKey();
		Requirement.Name = UTF8_TO_TCHAR(Global.name.AddressOf());
		return Builder.AddRequirement(Requirement, OutSlot);
	}

	struct FIntCase
	{
		FDetachedDefinitionFixture Fixture;
		FNativeEngineOwner Engine;
		asCScriptFunction* Function = nullptr;
		asCByteCodeImageBuilder Builder;
		TUniquePtr<asCByteCodeImage> Image;
		asCExecutableSnapshot* Snapshot = nullptr;
		asIScriptContext* Context = nullptr;
		asEByteCodeLinkStatus LinkStatus = asEByteCodeLinkStatus::InvalidArgument;

		bool Start(const TCHAR* Name, eTokenType Return, int32 ParamCount, asUINT FrameSize, asUINT ParameterSize)
		{
			asSDefinitionParameter Params[2];
			Params[0].Name = "a";
			Params[0].Type = Fixture.Set->Primitive(ttInt);
			Params[1].Name = "b";
			Params[1].Type = Fixture.Set->Primitive(ttInt);
			Function = Fixture.DefineFunction(
				Name,
				TEXT(""),
				asFUNC_SCRIPT,
				Fixture.Set->Primitive(Return),
				MakeArrayView(Params, ParamCount));
			if (!Function || !Fixture.FinalizeAndFreeze()) return false;
			if (RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set)) != asERegistrationResult::Succeeded) return false;
			asUINT Slot = 0;
			if (Builder.AddFunctionRequirement(*Function, Slot) != asEByteCodeImageStatus::Succeeded) return false;
			return Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), FrameSize, ParameterSize)
				== asEByteCodeImageStatus::Succeeded;
		}

		bool Close()
		{
			if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded) return false;
			if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded) return false;
			LinkStatus = asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot);
			if (LinkStatus != asEByteCodeLinkStatus::Succeeded) return false;
			Context = Engine.Ptr->CreateContext();
			return Context && Context->Prepare(Function) == 0;
		}

		int Execute()
		{
			return Context->Execute();
		}

		int32 RetI() const
		{
			return static_cast<int32>(Context->GetReturnDWord());
		}

		int64 RetQ() const
		{
			return static_cast<int64>(Context->GetReturnQWord());
		}

		~FIntCase()
		{
			if (Context) Context->Release();
			if (Snapshot) Snapshot->Release();
		}
	};
}

TEST_CLASS_WITH_FLAGS(VMIntegerOpcodeMatrix, "Angelscript.UnitTest.NativeEngine.VM",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ConstantMovesAndWidths)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("SetV4"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 42)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 0x5A5A5A5A)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("SetV1"), ttInt, 0, 16, 0)));
			asSByteCodeOperand Ops[2] = {Loc(1), Imm32(0x7F)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_SetV1, MakeArrayView(Ops, 2))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(0x7F, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("SetV2"), ttInt, 0, 16, 0)));
			asSByteCodeOperand Ops[2] = {Loc(1), Imm32(0x1234)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_SetV2, MakeArrayView(Ops, 2))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(0x1234, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("SetV8"), ttInt64, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 0x10000002A)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 0x10000002A));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CpyVtoV4"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 42)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 0x11111111)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_CpyVtoV4, 2, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CpyVtoV8"), ttInt64, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 99)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_CpyVtoV8, 4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 99));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CpyVtoR4"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 42)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CpyRtoV4"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 42)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyVtoR4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CpyVtoR8"), ttInt64, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 1234567890123LL)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 1234567890123LL));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CpyRtoV8"), ttInt64, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 77)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyVtoR8, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 4, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV8, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 77));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CpyVtoR1"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyVtoR4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 0xAABBCCDD)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyVtoR1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 3)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(0xDD, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("ClrHi"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0x01020304)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyVtoR4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_ClrHi, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(0x04, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("PshC4"), ttInt, 0, 16, 0)));
			asSByteCodeOperand Zero = Imm32(0);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PshC4, MakeArrayView(&Zero, 1))));
			asSByteCodeOperand Push = Imm32(42);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("PshC8"), ttInt64, 0, 16, 0)));
			asSByteCodeOperand Push = Imm64(0x200000015LL);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PshC8, MakeArrayView(&Push, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV8, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 0x200000015LL));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("PshV4"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 42)));
			asSByteCodeOperand Zero = Imm32(0);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PshC4, MakeArrayView(&Zero, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PshV4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("PshV8"), ttInt64, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 0x30000002ALL)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PshV8, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV8, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 0x30000002ALL));
		}
	}

	TEST_METHOD(Integer32Arithmetic)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		auto RunTernary = [&](const TCHAR* Name, asEBCInstr Opcode, int32 Left, int32 Right, int32 Expected)
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(Name, ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, Left)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, Right)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, Opcode, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(Expected, Case.RetI()));
		};
		RunTernary(TEXT("ADDi"), asBC_ADDi, 7, 35, 42);
		RunTernary(TEXT("SUBi"), asBC_SUBi, 50, 8, 42);
		RunTernary(TEXT("MULi"), asBC_MULi, 6, 7, 42);
		RunTernary(TEXT("DIVi"), asBC_DIVi, 84, 2, 42);
		RunTernary(TEXT("MODi"), asBC_MODi, 85, 43, 42);
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("ADDIi"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 40)));
			asSByteCodeOperand Ops[3] = {Loc(2), Loc(1), Imm32(2)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_ADDIi, MakeArrayView(Ops, 3))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("SUBIi"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 50)));
			asSByteCodeOperand Ops[3] = {Loc(2), Loc(1), Imm32(8)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_SUBIi, MakeArrayView(Ops, 3))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("MULIi"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 6)));
			asSByteCodeOperand Ops[3] = {Loc(2), Loc(1), Imm32(7)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_MULIi, MakeArrayView(Ops, 3))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
	}

	TEST_METHOD(Integer64Arithmetic)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		auto Run = [&](const TCHAR* Name, asEBCInstr Opcode, int64 Left, int64 Right, int64 Expected)
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(Name, ttInt64, 0, 24, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, Left)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 4, Right)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, Opcode, 6, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 6)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == Expected));
		};
		Run(TEXT("ADDi64"), asBC_ADDi64, 40, 2, 42);
		Run(TEXT("SUBi64"), asBC_SUBi64, 50, 8, 42);
		Run(TEXT("MULi64"), asBC_MULi64, 6, 7, 42);
		Run(TEXT("DIVi64"), asBC_DIVi64, 84, 2, 42);
		Run(TEXT("MODi64"), asBC_MODi64, 85, 43, 42);
	}

	TEST_METHOD(UnsignedDivMod)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("DIVu"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, static_cast<int32>(0xFFFFFFFFu))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_DIVu, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(static_cast<int32>(0x7FFFFFFFu), Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("MODu"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, static_cast<int32>(0xFFFFFFFFu))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 0x10000)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_MODu, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(0xFFFF, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("DIVu64"), ttInt64, 0, 24, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, static_cast<int64>(0x8000000000000000ULL))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_DIVu64, 6, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 6)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == static_cast<int64>(0x4000000000000000ULL)));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("MODu64"), ttInt64, 0, 24, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, static_cast<int64>(0xFFFFFFFFFFFFFFFFULL))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 4, 0x10000)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_MODu64, 6, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 6)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 0xFFFF));
		}
	}

	TEST_METHOD(PowerIntegerAndOverflow)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("POWi"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 3)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_POWi, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(8, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("POWu"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_POWu, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(16, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("POWi64"), ttInt64, 0, 24, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 4, 10)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_POWi64, 6, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 6)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 1024));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("POWu64"), ttInt64, 0, 24, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 3)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 4, 5)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_POWu64, 6, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 6)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 243));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("POWiOverflow"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 31)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_POWi, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Case.Execute()));
			ASSERT_THAT(IsTrue(FCStringAnsi::Strstr(Case.Context->GetExceptionString(), "Overflow in exponent") != nullptr));
		}
	}

	TEST_METHOD(Bitwise32)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("NOT"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_NOT, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(1, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("BNOT"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, -43)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_BNOT, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		auto Run = [&](const TCHAR* Name, asEBCInstr Opcode, int32 Left, int32 Right, int32 Expected)
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(Name, ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, Left)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, Right)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, Opcode, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(Expected, Case.RetI()));
		};
		Run(TEXT("BAND"), asBC_BAND, 0xFF, 0x2A, 42);
		Run(TEXT("BOR"), asBC_BOR, 0x20, 0x0A, 42);
		Run(TEXT("BXOR"), asBC_BXOR, 0x7A, 0x50, 42);
		Run(TEXT("BSLL"), asBC_BSLL, 21, 1, 42);
		Run(TEXT("BSRL"), asBC_BSRL, 168, 2, 42);
		Run(TEXT("BSRA"), asBC_BSRA, -84, 1, -42);
	}

	TEST_METHOD(Bitwise64)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("BNOT64"), ttInt64, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, -43)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_BNOT64, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 42));
		}
		auto Run = [&](const TCHAR* Name, asEBCInstr Opcode, int64 Left, int64 Right, int64 Expected)
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(Name, ttInt64, 0, 24, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, Left)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 4, Right)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, Opcode, 6, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 6)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == Expected));
		};
		Run(TEXT("BAND64"), asBC_BAND64, 0xFF, 0x2A, 42);
		Run(TEXT("BOR64"), asBC_BOR64, 0x20, 0x0A, 42);
		Run(TEXT("BXOR64"), asBC_BXOR64, 0x7A, 0x50, 42);
		Run(TEXT("BSLL64"), asBC_BSLL64, 21, 1, 42);
		Run(TEXT("BSRL64"), asBC_BSRL64, 168, 2, 42);
		Run(TEXT("BSRA64"), asBC_BSRA64, -84, 1, -42);
	}

	TEST_METHOD(NegIncDec)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("NEGi"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, -42)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_NEGi, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("NEGi64"), ttInt64, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, -42)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_NEGi64, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 42));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("IncVi"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 41)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_IncVi, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("DecVi"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 43)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_DecVi, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		auto RunPointed = [&](const TCHAR* Name, asEBCInstr Opcode, int32 Start, int32 Expected)
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(Name, ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, Start)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_LDV, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(Opcode, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(Expected, Case.RetI()));
		};
		RunPointed(TEXT("INCi"), asBC_INCi, 41, 42);
		RunPointed(TEXT("DECi"), asBC_DECi, 43, 42);
		RunPointed(TEXT("INCi8"), asBC_INCi8, 41, 42);
		RunPointed(TEXT("DECi8"), asBC_DECi8, 43, 42);
		RunPointed(TEXT("INCi16"), asBC_INCi16, 41, 42);
		RunPointed(TEXT("DECi16"), asBC_DECi16, 43, 42);
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("INCi64"), ttInt64, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 41)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_LDV, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_INCi64, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 42));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("DECi64"), ttInt64, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 43)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_LDV, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_DECi64, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 42));
		}
	}

	TEST_METHOD(IntegerConversions)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("sbTOi"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0xFFFFFF80)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_sbTOi, 2, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(-128, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("swTOi"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0xFFFF8000)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_swTOi, 2, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(-32768, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("ubTOi"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0xFF)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_ubTOi, 2, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(255, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("uwTOi"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0xFFFF)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_uwTOi, 2, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(65535, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("iTOb"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0x12345678)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 0x11111111)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_iTOb, 2, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(0x78, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("iTOw"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0x12345678)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 0x11111111)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_iTOw, 2, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(0x5678, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("i64TOi"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 0x10000002ALL)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_i64TOi, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("uTOi64"), ttInt64, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, static_cast<int32>(0xFFFFFFFFu))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_uTOi64, 4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 4294967295LL));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("iTOi64"), ttInt64, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, -5)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_iTOi64, 4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == -5));
		}
	}

	TEST_METHOD(ComparisonsAndTestFlags)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		auto CmpThenTest = [&](const TCHAR* Name, asEBCInstr Cmp, int32 Left, int32 Right, asEBCInstr Test, int32 Expected)
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(Name, ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, Left)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, Right)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, Cmp, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(Test, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 3)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(Expected, Case.RetI()));
		};
		CmpThenTest(TEXT("CMPiTZ"), asBC_CMPi, 5, 5, asBC_TZ, 1);
		CmpThenTest(TEXT("CMPiTNZ"), asBC_CMPi, 5, 7, asBC_TNZ, 1);
		CmpThenTest(TEXT("CMPiTS"), asBC_CMPi, 5, 7, asBC_TS, 1);
		CmpThenTest(TEXT("CMPiTNS"), asBC_CMPi, 7, 5, asBC_TNS, 1);
		CmpThenTest(TEXT("CMPiTP"), asBC_CMPi, 7, 5, asBC_TP, 1);
		CmpThenTest(TEXT("CMPiTNP"), asBC_CMPi, 5, 7, asBC_TNP, 1);
		CmpThenTest(TEXT("CMPu"), asBC_CMPu, static_cast<int32>(0xFFFFFFFFu), 1, asBC_TP, 1);
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CMPIi"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 5)));
			asSByteCodeOperand Ops[2] = {Loc(1), Imm32(5)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_CMPIi, MakeArrayView(Ops, 2))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_TZ, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(1, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CMPIu"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, static_cast<int32>(0xFFFFFFFFu))));
			asSByteCodeOperand Ops[2] = {Loc(1), Imm32(0)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_CMPIu, MakeArrayView(Ops, 2))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_TP, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(1, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CMPi64"), ttInt, 0, 24, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, -3)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_CMPi64, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_TS, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(1, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CMPu64"), ttInt, 0, 24, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, static_cast<int64>(0x8000000000000000ULL))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_CMPu64, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_TP, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(1, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CmpPtr"), ttInt, 0, 24, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 0x1000)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 4, 0x1000)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_CmpPtr, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_TZ, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(1, Case.RetI()));
		}
	}

	TEST_METHOD(ConditionalJumpsTakenAndNotTaken)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		auto JumpIf = [&](const TCHAR* Name, int32 RegisterValue, asEBCInstr Jump, int32 TakenValue, int32 FallValue, int32 Expected)
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(Name, ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, RegisterValue)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyVtoR4, 1)));
			const asUINT Taken = Case.Builder.NewLabel();
			const asUINT Done = Case.Builder.NewLabel();
			asSByteCodeOperand ToTaken = LabelAt(Taken);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(Jump, MakeArrayView(&ToTaken, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, FallValue)));
			asSByteCodeOperand ToDone = LabelAt(Done);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_JMP, MakeArrayView(&ToDone, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Taken)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, TakenValue)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Done)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(Expected, Case.RetI()));
		};
		JumpIf(TEXT("JZTaken"), 0, asBC_JZ, 11, 22, 11);
		JumpIf(TEXT("JZNotTaken"), 3, asBC_JZ, 11, 22, 22);
		JumpIf(TEXT("JNZTaken"), 3, asBC_JNZ, 11, 22, 11);
		JumpIf(TEXT("JNZNotTaken"), 0, asBC_JNZ, 11, 22, 22);
		JumpIf(TEXT("JSTaken"), -1, asBC_JS, 11, 22, 11);
		JumpIf(TEXT("JSNotTaken"), 1, asBC_JS, 11, 22, 22);
		JumpIf(TEXT("JNSTaken"), 0, asBC_JNS, 11, 22, 11);
		JumpIf(TEXT("JNSNotTaken"), -2, asBC_JNS, 11, 22, 22);
		JumpIf(TEXT("JPTaken"), 4, asBC_JP, 11, 22, 11);
		JumpIf(TEXT("JPNotTaken"), 0, asBC_JP, 11, 22, 22);
		JumpIf(TEXT("JNPTaken"), -1, asBC_JNP, 11, 22, 11);
		JumpIf(TEXT("JNPNotTaken"), 5, asBC_JNP, 11, 22, 22);
		JumpIf(TEXT("JLowZTaken"), 0x100, asBC_JLowZ, 11, 22, 11);
		JumpIf(TEXT("JLowZNotTaken"), 1, asBC_JLowZ, 11, 22, 22);
		JumpIf(TEXT("JLowNZTaken"), 1, asBC_JLowNZ, 11, 22, 11);
		JumpIf(TEXT("JLowNZNotTaken"), 0x100, asBC_JLowNZ, 11, 22, 22);
	}

	TEST_METHOD(TerminatingLoopSumOneToTen)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		FIntCase Case;
		ASSERT_THAT(IsTrue(Case.Start(TEXT("LoopSum"), ttInt, 0, 16, 0)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 1)));
		const asUINT Loop = Case.Builder.NewLabel();
		const asUINT Done = Case.Builder.NewLabel();
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Loop)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_ADDi, 1, 1, 2)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_IncVi, 2)));
		asSByteCodeOperand Cmp[2] = {Loc(2), Imm32(11)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_CMPIi, MakeArrayView(Cmp, 2))));
		asSByteCodeOperand Back = LabelAt(Loop);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_JS, MakeArrayView(&Back, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Done)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
		ASSERT_THAT(IsTrue(Case.Close()));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
		ASSERT_THAT(AreEqual(55, Case.RetI()));
	}

	TEST_METHOD(JumpTableEachArmDefaultAndMalformed)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		auto RunIndex = [&](int32 Index, int32 Expected)
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("JMPP"), ttInt, 1, 16, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_CpyVtoV4, 1, 0)));
			asSByteCodeOperand Cmp[2] = {Loc(1), Imm32(3)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_CMPIu, MakeArrayView(Cmp, 2))));
			const asUINT Table = Case.Builder.NewLabel();
			const asUINT Arm0 = Case.Builder.NewLabel();
			const asUINT Arm1 = Case.Builder.NewLabel();
			const asUINT Arm2 = Case.Builder.NewLabel();
			const asUINT Done = Case.Builder.NewLabel();
			asSByteCodeOperand ToTable = LabelAt(Table);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_JS, MakeArrayView(&ToTable, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 40)));
			asSByteCodeOperand ToDone = LabelAt(Done);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_JMP, MakeArrayView(&ToDone, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Table)));
			asSByteCodeOperand Jmpp[2] = {Loc(1), Imm32(2)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_JMPP, MakeArrayView(Jmpp, 2))));
			asSByteCodeOperand J0 = LabelAt(Arm0);
			asSByteCodeOperand J1 = LabelAt(Arm1);
			asSByteCodeOperand J2 = LabelAt(Arm2);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_JMP, MakeArrayView(&J0, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_JMP, MakeArrayView(&J1, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_JMP, MakeArrayView(&J2, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Arm0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 10)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_JMP, MakeArrayView(&ToDone, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Arm1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 20)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_JMP, MakeArrayView(&ToDone, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Arm2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 30)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Done)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(0, Case.Context->SetArgDWord(0, static_cast<asDWORD>(Index))));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(Expected, Case.RetI()));
		};
		RunIndex(0, 10);
		RunIndex(1, 20);
		RunIndex(2, 30);
		RunIndex(99, 40);
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("JMPPMalformed"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0)));
			asSByteCodeOperand Jmpp[2] = {Loc(1), Imm32(5)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_JMPP, MakeArrayView(Jmpp, 2))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsFalse(Case.Close()));
			ASSERT_THAT(AreNotEqual(asEByteCodeLinkStatus::Succeeded, Case.LinkStatus));
		}
	}

	TEST_METHOD(StackPointerAndVar)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("PopPtr"), ttInt64, 0, 16, 0)));
			asSByteCodeOperand First = Imm64(99);
			asSByteCodeOperand Second = Imm64(42);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PshC8, MakeArrayView(&First, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PshC8, MakeArrayView(&Second, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV8, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 99));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("SwapPtr"), ttInt64, 0, 16, 0)));
			asSByteCodeOperand First = Imm64(11);
			asSByteCodeOperand Second = Imm64(22);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PshC8, MakeArrayView(&First, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PshC8, MakeArrayView(&Second, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_SwapPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV8, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 11));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("PSF"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 42)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PSF, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_RDR4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("PshVPtr"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 42)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PSF, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV8, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PshVPtr, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_RDR4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("PshRPtr"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 42)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PSF, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PshRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_RDR4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("RDSPtr"), ttInt64, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 0x12340000)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PSF, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_RDSPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV8, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 0x12340000));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("RDSPtrNull"), ttInt, 0, 16, 0)));
			asSByteCodeOperand Zero = Imm64(0);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PshC8, MakeArrayView(&Zero, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_RDSPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Case.Execute()));
			ASSERT_THAT(IsTrue(FCStringAnsi::Strstr(Case.Context->GetExceptionString(), "Null pointer access") != nullptr));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("VAR"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_VAR, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(4, Case.RetI()));
		}
		{
			FIntCase Case;
			asCGlobalProperty* Global = DefineIntGlobal(Case.Fixture, TEXT("PtrSlot"));
			ASSERT_THAT(IsNotNull(Global));
			asCScriptFunction* Function = Case.Fixture.DefineFunction(TEXT("PshGPtr"), TEXT(""), asFUNC_SCRIPT, Case.Fixture.Set->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Case.Fixture.FinalizeAndFreeze()));
			ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, Case.Engine.Ptr->RegisterExternalDefinitions(Case.Fixture.TakeSet())));
			int32 Value = 42;
			asPWORD Storage = reinterpret_cast<asPWORD>(&Value);
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Case.Engine.Ptr->BindGlobalStorage(Global->GetStableKey(), &Storage)));
			asUINT RunSlot = 0;
			asUINT GlobalSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.AddFunctionRequirement(*Function, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, AddGlobalRequirement(Case.Builder, *Global, GlobalSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 16, 0)));
			asSByteCodeOperand GlobalOp = ByteSlot(asEByteCodeOperandKind::GlobalSlot, GlobalSlot);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PshGPtr, MakeArrayView(&GlobalOp, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_RDR4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			Case.Function = Function;
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
	}

	TEST_METHOD(MemoryWidthsWithSentinels)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("WRTV1"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0x11111111)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 0x22222222)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 3, 0x33333333)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 4, 0xAB)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PSF, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_WRTV1, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(0x222222AB, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("WRTV2"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0x11111111)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 0x22222222)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 3, 0x33333333)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 4, 0xABCD)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PSF, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_WRTV2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(0x2222ABCD, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("WRTV4"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 4, 42)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PSF, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_WRTV4, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("WRTV8"), ttInt64, 0, 24, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 4, 0x20000002ALL)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PSF, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_WRTV8, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 0x20000002ALL));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("RDR1"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0xAABBCCDD)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 0xFFFFFFFF)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PSF, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_RDR1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(0xDD, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("RDR2"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0xAABBCCDD)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 0xFFFFFFFF)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PSF, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_RDR2, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(0xCCDD, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("RDR4"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 42)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PSF, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_RDR4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("RDR8"), ttInt64, 0, 24, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 0x20000002ALL)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 4, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_PSF, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_RDR8, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 0x20000002ALL));
		}
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("LDV"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 42)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_LDV, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_WRTV4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
	}

	TEST_METHOD(GlobalMoves)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		{
			FIntCase Case;
			asCGlobalProperty* Global = DefineIntGlobal(Case.Fixture, TEXT("G"));
			ASSERT_THAT(IsNotNull(Global));
			asCScriptFunction* Function = Case.Fixture.DefineFunction(TEXT("PshG4"), TEXT(""), asFUNC_SCRIPT, Case.Fixture.Set->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Case.Fixture.FinalizeAndFreeze()));
			ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, Case.Engine.Ptr->RegisterExternalDefinitions(Case.Fixture.TakeSet())));
			int32 Storage = 42;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Case.Engine.Ptr->BindGlobalStorage(Global->GetStableKey(), &Storage)));
			asUINT RunSlot = 0;
			asUINT GlobalSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.AddFunctionRequirement(*Function, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, AddGlobalRequirement(Case.Builder, *Global, GlobalSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 16, 0)));
			asSByteCodeOperand GlobalOp = ByteSlot(asEByteCodeOperandKind::GlobalSlot, GlobalSlot);
			asSByteCodeOperand Zero = Imm32(0);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PshC4, MakeArrayView(&Zero, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PshG4, MakeArrayView(&GlobalOp, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			Case.Function = Function;
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			asCGlobalProperty* Global = DefineIntGlobal(Case.Fixture, TEXT("G"));
			ASSERT_THAT(IsNotNull(Global));
			asCScriptFunction* Function = Case.Fixture.DefineFunction(TEXT("SetG4"), TEXT(""), asFUNC_SCRIPT, Case.Fixture.Set->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Case.Fixture.FinalizeAndFreeze()));
			ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, Case.Engine.Ptr->RegisterExternalDefinitions(Case.Fixture.TakeSet())));
			int32 Storage = 7;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Case.Engine.Ptr->BindGlobalStorage(Global->GetStableKey(), &Storage)));
			asUINT RunSlot = 0;
			asUINT GlobalSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.AddFunctionRequirement(*Function, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, AddGlobalRequirement(Case.Builder, *Global, GlobalSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 16, 0)));
			asSByteCodeOperand GlobalOp = ByteSlot(asEByteCodeOperandKind::GlobalSlot, GlobalSlot);
			asSByteCodeOperand SetG[2] = {GlobalOp, Imm32(42)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_SetG4, MakeArrayView(SetG, 2))));
			asSByteCodeOperand Copy[2] = {Loc(1), GlobalOp};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_CpyGtoV4, MakeArrayView(Copy, 2))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			Case.Function = Function;
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
			ASSERT_THAT(AreEqual(42, Storage));
		}
		{
			FIntCase Case;
			asCGlobalProperty* Global = DefineIntGlobal(Case.Fixture, TEXT("G"));
			ASSERT_THAT(IsNotNull(Global));
			asCScriptFunction* Function = Case.Fixture.DefineFunction(TEXT("CpyVtoG4"), TEXT(""), asFUNC_SCRIPT, Case.Fixture.Set->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Case.Fixture.FinalizeAndFreeze()));
			ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, Case.Engine.Ptr->RegisterExternalDefinitions(Case.Fixture.TakeSet())));
			int32 Storage = 0;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Case.Engine.Ptr->BindGlobalStorage(Global->GetStableKey(), &Storage)));
			asUINT RunSlot = 0;
			asUINT GlobalSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.AddFunctionRequirement(*Function, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, AddGlobalRequirement(Case.Builder, *Global, GlobalSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 16, 0)));
			asSByteCodeOperand GlobalOp = ByteSlot(asEByteCodeOperandKind::GlobalSlot, GlobalSlot);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 42)));
			asSByteCodeOperand Copy[2] = {Loc(1), GlobalOp};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_CpyVtoG4, MakeArrayView(Copy, 2))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			Case.Function = Function;
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
			ASSERT_THAT(AreEqual(42, Storage));
		}
		{
			FIntCase Case;
			asCGlobalProperty* Global = DefineIntGlobal(Case.Fixture, TEXT("G"));
			ASSERT_THAT(IsNotNull(Global));
			asCScriptFunction* Function = Case.Fixture.DefineFunction(TEXT("LdGRdR4"), TEXT(""), asFUNC_SCRIPT, Case.Fixture.Set->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Case.Fixture.FinalizeAndFreeze()));
			ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, Case.Engine.Ptr->RegisterExternalDefinitions(Case.Fixture.TakeSet())));
			int32 Storage = 42;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Case.Engine.Ptr->BindGlobalStorage(Global->GetStableKey(), &Storage)));
			asUINT RunSlot = 0;
			asUINT GlobalSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.AddFunctionRequirement(*Function, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, AddGlobalRequirement(Case.Builder, *Global, GlobalSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 16, 0)));
			asSByteCodeOperand GlobalOp = ByteSlot(asEByteCodeOperandKind::GlobalSlot, GlobalSlot);
			asSByteCodeOperand Load[2] = {Loc(1), GlobalOp};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_LdGRdR4, MakeArrayView(Load, 2))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			Case.Function = Function;
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			asCGlobalProperty* Global = DefineIntGlobal(Case.Fixture, TEXT("G"));
			ASSERT_THAT(IsNotNull(Global));
			asCScriptFunction* Function = Case.Fixture.DefineFunction(TEXT("LDG"), TEXT(""), asFUNC_SCRIPT, Case.Fixture.Set->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Case.Fixture.FinalizeAndFreeze()));
			ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, Case.Engine.Ptr->RegisterExternalDefinitions(Case.Fixture.TakeSet())));
			int32 Storage = 42;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Case.Engine.Ptr->BindGlobalStorage(Global->GetStableKey(), &Storage)));
			asUINT RunSlot = 0;
			asUINT GlobalSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.AddFunctionRequirement(*Function, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, AddGlobalRequirement(Case.Builder, *Global, GlobalSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 16, 0)));
			asSByteCodeOperand GlobalOp = ByteSlot(asEByteCodeOperandKind::GlobalSlot, GlobalSlot);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_LDG, MakeArrayView(&GlobalOp, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_RDR4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			Case.Function = Function;
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
		{
			FIntCase Case;
			asCGlobalProperty* Global = DefineIntGlobal(Case.Fixture, TEXT("G"));
			ASSERT_THAT(IsNotNull(Global));
			asCScriptFunction* Function = Case.Fixture.DefineFunction(TEXT("PGA"), TEXT(""), asFUNC_SCRIPT, Case.Fixture.Set->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Case.Fixture.FinalizeAndFreeze()));
			ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, Case.Engine.Ptr->RegisterExternalDefinitions(Case.Fixture.TakeSet())));
			int32 Storage = 42;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Case.Engine.Ptr->BindGlobalStorage(Global->GetStableKey(), &Storage)));
			asUINT RunSlot = 0;
			asUINT GlobalSlot = 0;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.AddFunctionRequirement(*Function, RunSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, AddGlobalRequirement(Case.Builder, *Global, GlobalSlot)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 16, 0)));
			asSByteCodeOperand GlobalOp = ByteSlot(asEByteCodeOperandKind::GlobalSlot, GlobalSlot);
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PGA, MakeArrayView(&GlobalOp, 1))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_RDR4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			Case.Function = Function;
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(42, Case.RetI()));
		}
	}

	TEST_METHOD(NegativeArgOffsetsVersusPositiveLocals)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		FIntCase Case;
		ASSERT_THAT(IsTrue(Case.Start(TEXT("ArgOffsets"), ttInt, 2, 16, 2)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 0x5A5A5A5A)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_ADDi, 3, 0, -1)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3, 2)));
		ASSERT_THAT(IsTrue(Case.Close()));
		ASSERT_THAT(AreEqual(0, Case.Context->SetArgDWord(0, 7)));
		ASSERT_THAT(AreEqual(0, Case.Context->SetArgDWord(1, 35)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
		ASSERT_THAT(AreEqual(42, Case.RetI()));
	}

	TEST_METHOD(DivModByZeroTypedException)
	{
		using namespace VMIntegerOpcodeMatrixPrivate;
		auto ExpectDivideByZero = [&](const TCHAR* Name, eTokenType Return, asEBCInstr Opcode, bool Wide)
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(Name, Return, 0, 24, 0)));
			if (Wide)
			{
				ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 84)));
				ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 4, 0)));
				ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, Opcode, 6, 2, 4)));
				ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 6)));
			}
			else
			{
				ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 84)));
				ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, 0)));
				ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, Opcode, 3, 1, 2)));
				ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			}
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Case.Execute()));
			ASSERT_THAT(IsTrue(FCStringAnsi::Strstr(Case.Context->GetExceptionString(), "Divide by zero") != nullptr));
		};
		ExpectDivideByZero(TEXT("DIViZero"), ttInt, asBC_DIVi, false);
		ExpectDivideByZero(TEXT("MODiZero"), ttInt, asBC_MODi, false);
		ExpectDivideByZero(TEXT("DIVuZero"), ttInt, asBC_DIVu, false);
		ExpectDivideByZero(TEXT("MODuZero"), ttInt, asBC_MODu, false);
		ExpectDivideByZero(TEXT("DIVi64Zero"), ttInt64, asBC_DIVi64, true);
		ExpectDivideByZero(TEXT("MODi64Zero"), ttInt64, asBC_MODi64, true);
		ExpectDivideByZero(TEXT("DIVu64Zero"), ttInt64, asBC_DIVu64, true);
		ExpectDivideByZero(TEXT("MODu64Zero"), ttInt64, asBC_MODu64, true);
		{
			FIntCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("DIViOverflow"), ttInt, 0, 16, 0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, static_cast<int32>(0x80000000))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 2, -1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_DIVi, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Case.Execute()));
			ASSERT_THAT(IsTrue(FCStringAnsi::Strstr(Case.Context->GetExceptionString(), "Overflow in integer division") != nullptr));
		}
	}
};

#endif
