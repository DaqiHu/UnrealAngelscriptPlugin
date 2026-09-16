#include "as_bytecode_emitter_internal.h"
#include "as_bytecode_opcode.h"
#include "as_property.h"
#include "frontend/AST/as_type.h"
#include "HAL/UnrealMemory.h"

BEGIN_AS_NAMESPACE

namespace
{
	bool EmitThreeLocal(asSByteCodeEmitFrame& Frame, asEBCInstr Opcode, int32 Dest, int32 Left, int32 Right)
	{
		asSByteCodeOperand Ops[3];
		Ops[0].Kind = asEByteCodeOperandKind::Local;
		Ops[0].Immediate = Dest;
		Ops[1].Kind = asEByteCodeOperandKind::Local;
		Ops[1].Immediate = Left;
		Ops[2].Kind = asEByteCodeOperandKind::Local;
		Ops[2].Immediate = Right;
		return Frame.Builder->Emit(Opcode, MakeArrayView(Ops, 3)) == asEByteCodeImageStatus::Succeeded;
	}

	bool EmitTwoLocal(asSByteCodeEmitFrame& Frame, asEBCInstr Opcode, int32 Dest, int32 Src)
	{
		asSByteCodeOperand Ops[2];
		Ops[0].Kind = asEByteCodeOperandKind::Local;
		Ops[0].Immediate = Dest;
		Ops[1].Kind = asEByteCodeOperandKind::Local;
		Ops[1].Immediate = Src;
		return Frame.Builder->Emit(Opcode, MakeArrayView(Ops, 2)) == asEByteCodeImageStatus::Succeeded;
	}

	asEPrimitiveType PrimitiveOf(asCType* Type)
	{
		if (const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Type))
		{
			return Builtin->GetPrimitive();
		}
		return asEPrimitiveType::Invalid;
	}

	asEPrimitiveType PrimitiveOf(asCExpr* Expr)
	{
		const asEPrimitiveType Primitive = Expr ? PrimitiveOf(Expr->GetExpressionType()) : asEPrimitiveType::Invalid;
		return Primitive == asEPrimitiveType::Invalid ? asEPrimitiveType::Int32 : Primitive;
	}

	asUINT DwordWidth(asEPrimitiveType Primitive)
	{
		switch (Primitive)
		{
		case asEPrimitiveType::Int64:
		case asEPrimitiveType::UInt64:
		case asEPrimitiveType::Float64:
			return 2;
		case asEPrimitiveType::Void:
			return 0;
		default:
			return 1;
		}
	}

	asEBCInstr CopyVtoV(asEPrimitiveType Primitive)
	{
		return DwordWidth(Primitive) == 2 ? asBC_CpyVtoV8 : asBC_CpyVtoV4;
	}

	asEBCInstr CopyVtoR(asEPrimitiveType Primitive)
	{
		return DwordWidth(Primitive) == 2 ? asBC_CpyVtoR8 : asBC_CpyVtoR4;
	}

	asEBCInstr ConversionOpcode(asEPrimitiveType From, asEPrimitiveType To)
	{
		if (From == To)
		{
			return asBC_MAXBYTECODE;
		}
		if (From == asEPrimitiveType::Int8 && To == asEPrimitiveType::Int32) return asBC_sbTOi;
		if (From == asEPrimitiveType::Int16 && To == asEPrimitiveType::Int32) return asBC_swTOi;
		if (From == asEPrimitiveType::UInt8 && To == asEPrimitiveType::Int32) return asBC_ubTOi;
		if (From == asEPrimitiveType::UInt16 && To == asEPrimitiveType::Int32) return asBC_uwTOi;
		if (From == asEPrimitiveType::Int32 && To == asEPrimitiveType::Int8) return asBC_iTOb;
		if (From == asEPrimitiveType::Int32 && To == asEPrimitiveType::Int16) return asBC_iTOw;
		if (From == asEPrimitiveType::Int32 && To == asEPrimitiveType::Int64) return asBC_iTOi64;
		if (From == asEPrimitiveType::UInt32 && To == asEPrimitiveType::Int64) return asBC_uTOi64;
		if (From == asEPrimitiveType::UInt32 && To == asEPrimitiveType::UInt64) return asBC_uTOi64;
		if (From == asEPrimitiveType::Int32 && To == asEPrimitiveType::UInt64) return asBC_uTOi64;
		if (From == asEPrimitiveType::Int64 && To == asEPrimitiveType::Int32) return asBC_i64TOi;
		if (From == asEPrimitiveType::Int32 && To == asEPrimitiveType::Float64) return asBC_iTOd;
		if (From == asEPrimitiveType::Int32 && To == asEPrimitiveType::Float32) return asBC_iTOf;
		if (From == asEPrimitiveType::Int64 && To == asEPrimitiveType::Float64) return asBC_i64TOd;
		if (From == asEPrimitiveType::UInt64 && To == asEPrimitiveType::Float64) return asBC_u64TOd;
		if (From == asEPrimitiveType::Float64 && To == asEPrimitiveType::Int32) return asBC_dTOi;
		if (From == asEPrimitiveType::Float64 && To == asEPrimitiveType::Int64) return asBC_dTOi64;
		if (From == asEPrimitiveType::Float64 && To == asEPrimitiveType::Float32) return asBC_dTOf;
		if (From == asEPrimitiveType::Float32 && To == asEPrimitiveType::Float64) return asBC_fTOd;
		if (From == asEPrimitiveType::Float32 && To == asEPrimitiveType::Int32) return asBC_fTOi;
		if (From == asEPrimitiveType::UInt32 && To == asEPrimitiveType::Float64) return asBC_uTOd;
		if ((From == asEPrimitiveType::Int8 || From == asEPrimitiveType::Int16) && To == asEPrimitiveType::Float64)
		{
			return asBC_iTOd;
		}
		if ((From == asEPrimitiveType::Int8 || From == asEPrimitiveType::Int16) && To == asEPrimitiveType::Int64)
		{
			return asBC_iTOi64;
		}
		if (From == asEPrimitiveType::Bool && To == asEPrimitiveType::Int32)
		{
			return asBC_MAXBYTECODE;
		}
		return asBC_MAXBYTECODE;
	}

	bool EmitNumericConversion(asSByteCodeEmitFrame& Frame, int32 Source, asEPrimitiveType From, asEPrimitiveType To, int32& OutLocal)
	{
		if (From == asEPrimitiveType::Invalid) From = asEPrimitiveType::Int32;
		if (To == asEPrimitiveType::Invalid) To = asEPrimitiveType::Int32;
		if (From == To || (From == asEPrimitiveType::Bool && To == asEPrimitiveType::Int32))
		{
			OutLocal = Source;
			return true;
		}
		asEPrimitiveType StepFrom = From;
		if ((From == asEPrimitiveType::Int8 || From == asEPrimitiveType::Int16)
			&& (To == asEPrimitiveType::Float64 || To == asEPrimitiveType::Int64))
		{
			int32 Widened = 0;
			if (!EmitNumericConversion(Frame, Source, From, asEPrimitiveType::Int32, Widened)) return false;
			Source = Widened;
			StepFrom = asEPrimitiveType::Int32;
		}
		const asEBCInstr Opcode = ConversionOpcode(StepFrom, To);
		if (Opcode == asBC_MAXBYTECODE)
		{
			OutLocal = Source;
			return StepFrom == To;
		}
		OutLocal = static_cast<int32>(Frame.AllocLocal(FMath::Max(DwordWidth(To), DwordWidth(StepFrom))));
		return EmitTwoLocal(Frame, Opcode, OutLocal, Source);
	}

	asEBCInstr ArithOpcode(asEBinaryOperator Operator, asEPrimitiveType Primitive)
	{
		const bool bDouble = Primitive == asEPrimitiveType::Float64;
		const bool bFloat = Primitive == asEPrimitiveType::Float32;
		const bool b64 = Primitive == asEPrimitiveType::Int64 || Primitive == asEPrimitiveType::UInt64;
		const bool bUnsigned64 = Primitive == asEPrimitiveType::UInt64;
		const bool bUnsigned32 = Primitive == asEPrimitiveType::UInt32 || Primitive == asEPrimitiveType::UInt8
			|| Primitive == asEPrimitiveType::UInt16;
		switch (Operator)
		{
		case asEBinaryOperator::Add:
			return bDouble ? asBC_ADDd : (bFloat ? asBC_ADDf : (b64 ? asBC_ADDi64 : asBC_ADDi));
		case asEBinaryOperator::Subtract:
			return bDouble ? asBC_SUBd : (bFloat ? asBC_SUBf : (b64 ? asBC_SUBi64 : asBC_SUBi));
		case asEBinaryOperator::Multiply:
			return bDouble ? asBC_MULd : (bFloat ? asBC_MULf : (b64 ? asBC_MULi64 : asBC_MULi));
		case asEBinaryOperator::Divide:
			if (bDouble) return asBC_DIVd;
			if (bFloat) return asBC_DIVf;
			if (bUnsigned64) return asBC_DIVu64;
			if (bUnsigned32) return asBC_DIVu;
			return b64 ? asBC_DIVi64 : asBC_DIVi;
		case asEBinaryOperator::Modulo:
			if (bDouble) return asBC_MODd;
			if (bFloat) return asBC_MODf;
			if (bUnsigned64) return asBC_MODu64;
			if (bUnsigned32) return asBC_MODu;
			return b64 ? asBC_MODi64 : asBC_MODi;
		case asEBinaryOperator::Power:
			return bDouble ? asBC_POWd : (bFloat ? asBC_POWf : (b64 ? asBC_POWi64 : asBC_POWi));
		case asEBinaryOperator::BitwiseOr:
			return b64 ? asBC_BOR64 : asBC_BOR;
		case asEBinaryOperator::BitwiseAnd:
			return b64 ? asBC_BAND64 : asBC_BAND;
		default:
			return asBC_ADDi;
		}
	}

	asEBCInstr CompareOpcode(asEPrimitiveType Primitive)
	{
		if (Primitive == asEPrimitiveType::Float64) return asBC_CMPd;
		if (Primitive == asEPrimitiveType::Float32) return asBC_CMPf;
		if (Primitive == asEPrimitiveType::UInt64) return asBC_CMPu64;
		if (Primitive == asEPrimitiveType::Int64) return asBC_CMPi64;
		if (Primitive == asEPrimitiveType::UInt32 || Primitive == asEPrimitiveType::UInt8
			|| Primitive == asEPrimitiveType::UInt16)
		{
			return asBC_CMPu;
		}
		return asBC_CMPi;
	}

	asEBCInstr NegOpcode(asEPrimitiveType Primitive)
	{
		if (Primitive == asEPrimitiveType::Float64) return asBC_NEGd;
		if (Primitive == asEPrimitiveType::Float32) return asBC_NEGf;
		if (Primitive == asEPrimitiveType::Int64) return asBC_NEGi64;
		return asBC_NEGi;
	}

	int32 ParameterLocal(asCScriptFunction& Function, const FString& Name)
	{
		for (asUINT Index = 0; Index < Function.parameterNames.GetLength(); ++Index)
		{
			if (FString(UTF8_TO_TCHAR(Function.parameterNames[Index].AddressOf())) == Name)
			{
				int32 Offset = 0;
				if (Function.parameterOffsets.GetLength() > Index)
				{
					Offset = Function.parameterOffsets[Index];
				}
				else
				{
					if (Function.objectType)
					{
						Offset += AS_PTR_SIZE;
					}
					if (Function.DoesReturnOnStack())
					{
						Offset += AS_PTR_SIZE;
					}
					for (asUINT Prior = 0; Prior < Index; ++Prior)
					{
						Offset += Function.parameterTypes[Prior].GetSizeOnStackDWords();
					}
				}
				return -Offset;
			}
		}
		return MAX_int32;
	}

	int16 PropertyOffset(asCObjectType* Type, const FString& Name)
	{
		if (!Type) return -1;
		for (asUINT Index = 0; Index < Type->localProperties.GetLength(); ++Index)
		{
			asCObjectProperty* Property = Type->localProperties[Index];
			if (Property && FString(UTF8_TO_TCHAR(Property->name.AddressOf())) == Name)
			{
				return static_cast<int16>(Property->byteOffset);
			}
		}
		return -1;
	}

	asCObjectType* ObjectTypeOf(asSByteCodeEmitFrame& Frame, asCExpr* Expr)
	{
		if (!Expr) return nullptr;
		if (asCObjectType* Type = asEmitFindObjectType(Frame, Expr->GetExpressionType())) return Type;
		if (auto* DeclRef = dyn_cast<asCDeclRefExpr>(Expr))
		{
			if (auto* Var = dyn_cast<asCVarDecl>(DeclRef->GetDecl()))
			{
				return asEmitFindObjectType(Frame, Var->GetResolvedType());
			}
		}
		if (auto* This = dyn_cast<asCThisExpr>(Expr))
		{
			(void)This;
			return Frame.Function ? Frame.Function->objectType : nullptr;
		}
		return nullptr;
	}

}

bool asEmitMemberAddress(asSByteCodeEmitFrame& Frame, asCMemberExpr* Member)
{
	int32 Base = 0;
	if (!asEmitExpr(Frame, Member->GetBase(), Base)) return false;
	asCObjectType* Type = ObjectTypeOf(Frame, Member->GetBase());
	asCNamedDecl* Field = Member->GetMember();
	if (!Type || !Field) return false;
	const int16 Offset = PropertyOffset(Type, Field->GetName());
	if (Offset < 0) return false;
	asSByteCodeOperand Ops[2];
	Ops[0].Kind = asEByteCodeOperandKind::Local;
	Ops[0].Immediate = Base;
	Ops[1].Kind = asEByteCodeOperandKind::ImmediateS16;
	Ops[1].Immediate = Offset;
	return Frame.Builder->Emit(asBC_LoadRObjR, MakeArrayView(Ops, 2)) == asEByteCodeImageStatus::Succeeded;
}

namespace
{
	bool EmitJump(asSByteCodeEmitFrame& Frame, asEBCInstr Opcode, asUINT Label)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::Label;
		Operand.Label = Label;
		return Frame.Builder->Emit(Opcode, MakeArrayView(&Operand, 1)) == asEByteCodeImageStatus::Succeeded;
	}

	bool EmitCpyToR4(asSByteCodeEmitFrame& Frame, int32 Local)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::Local;
		Operand.Immediate = Local;
		return Frame.Builder->Emit(asBC_CpyVtoR4, MakeArrayView(&Operand, 1)) == asEByteCodeImageStatus::Succeeded;
	}
}

bool asEmitExpr(asSByteCodeEmitFrame& Frame, asCExpr* Expr, int32& OutLocal)
{
	if (!Frame.Builder || !Expr) return false;
	if (auto* Conditional = dyn_cast<asCConditionalOperator>(Expr))
	{
		int32 Condition = 0;
		if (!asEmitExpr(Frame, Conditional->GetCondition(), Condition)) return false;
		OutLocal = static_cast<int32>(Frame.AllocLocal(DwordWidth(PrimitiveOf(Conditional))));
		const asUINT FalseLabel = Frame.Builder->NewLabel();
		const asUINT EndLabel = Frame.Builder->NewLabel();
		if (!EmitCpyToR4(Frame, Condition) || !EmitJump(Frame, asBC_JZ, FalseLabel)) return false;
		int32 TrueLocal = 0;
		if (!asEmitExpr(Frame, Conditional->GetTrueExpr(), TrueLocal)) return false;
		const asEPrimitiveType ResultPrimitive = PrimitiveOf(Conditional);
		if (!EmitTwoLocal(Frame, CopyVtoV(ResultPrimitive), OutLocal, TrueLocal) || !EmitJump(Frame, asBC_JMP, EndLabel)) return false;
		if (Frame.Builder->BindLabel(FalseLabel) != asEByteCodeImageStatus::Succeeded) return false;
		int32 FalseLocal = 0;
		if (!asEmitExpr(Frame, Conditional->GetFalseExpr(), FalseLocal)) return false;
		if (!EmitTwoLocal(Frame, CopyVtoV(ResultPrimitive), OutLocal, FalseLocal)) return false;
		return Frame.Builder->BindLabel(EndLabel) == asEByteCodeImageStatus::Succeeded;
	}
	if (auto* Call = dyn_cast<asCCallExpr>(Expr))
	{
		return asEmitCall(Frame, Call, OutLocal);
	}
	if (auto* Construct = dyn_cast<asCConstructExpr>(Expr))
	{
		const asEPrimitiveType DestPrimitive = PrimitiveOf(Construct->GetExpressionType());
		if (DestPrimitive != asEPrimitiveType::Invalid && Construct->GetArguments().Num() == 1)
		{
			int32 Source = 0;
			if (!asEmitExpr(Frame, Construct->GetArguments()[0], Source)) return false;
			return EmitNumericConversion(Frame, Source, PrimitiveOf(Construct->GetArguments()[0]), DestPrimitive, OutLocal);
		}
		OutLocal = static_cast<int32>(Frame.AllocLocal(2));
		if (!asEmitConstructInto(Frame, Construct, OutLocal)) return false;
		if (asCObjectType* Type = asEmitFindObjectType(Frame, Construct->GetExpressionType()))
		{
			if (!asEmitNoteLiveObject(Frame, OutLocal, Type)) return false;
		}
		return true;
	}
	if (auto* ExplicitCast = dyn_cast<asCExplicitCastExpr>(Expr))
	{
		int32 Source = 0;
		if (!asEmitExpr(Frame, ExplicitCast->GetOperand(), Source)) return false;
		return EmitNumericConversion(Frame, Source, PrimitiveOf(ExplicitCast->GetOperand()), PrimitiveOf(ExplicitCast), OutLocal);
	}
	if (auto* This = dyn_cast<asCThisExpr>(Expr))
	{
		(void)This;
		OutLocal = 0;
		return Frame.Function && Frame.Function->objectType;
	}
	if (auto* Member = dyn_cast<asCMemberExpr>(Expr))
	{
		if (asCNamedDecl* Field = Member->GetMember())
		{
			if (auto* EnumConstant = dyn_cast<asCEnumConstantDecl>(Field))
			{
				OutLocal = static_cast<int32>(Frame.AllocLocal());
				asSByteCodeOperand Ops[2];
				Ops[0].Kind = asEByteCodeOperandKind::Local;
				Ops[0].Immediate = static_cast<int64>(OutLocal);
				Ops[1].Kind = asEByteCodeOperandKind::ImmediateS32;
				Ops[1].Immediate = static_cast<int32>(EnumConstant->GetValue());
				return Frame.Builder->Emit(asBC_SetV4, MakeArrayView(Ops, 2)) == asEByteCodeImageStatus::Succeeded;
			}
		}
		if (!asEmitMemberAddress(Frame, Member)) return false;
		OutLocal = static_cast<int32>(Frame.AllocLocal());
		asSByteCodeOperand Dest;
		Dest.Kind = asEByteCodeOperandKind::Local;
		Dest.Immediate = OutLocal;
		return Frame.Builder->Emit(asBC_RDR4, MakeArrayView(&Dest, 1)) == asEByteCodeImageStatus::Succeeded;
	}
	if (auto* Default = dyn_cast<asCDefaultArgumentExpr>(Expr))
	{
		asCVarDecl* Parameter = Default->GetParameter();
		return Parameter && Parameter->GetInitializer() && asEmitExpr(Frame, Parameter->GetInitializer(), OutLocal);
	}
	if (auto* Paren = dyn_cast<asCParenExpr>(Expr))
	{
		return asEmitExpr(Frame, Paren->GetSubExpr(), OutLocal);
	}
	if (auto* Literal = dyn_cast<asCIntegerLiteral>(Expr))
	{
		const asEPrimitiveType Primitive = PrimitiveOf(Literal);
		OutLocal = static_cast<int32>(Frame.AllocLocal(DwordWidth(Primitive)));
		asSByteCodeOperand Ops[2];
		Ops[0].Kind = asEByteCodeOperandKind::Local;
		Ops[0].Immediate = static_cast<int64>(OutLocal);
		if (DwordWidth(Primitive) == 2)
		{
			Ops[1].Kind = asEByteCodeOperandKind::ImmediateS64;
			Ops[1].Immediate = Primitive == asEPrimitiveType::UInt64
				? static_cast<int64>(Literal->GetUnsignedValue())
				: Literal->GetValue();
			return Frame.Builder->Emit(asBC_SetV8, MakeArrayView(Ops, 2)) == asEByteCodeImageStatus::Succeeded;
		}
		Ops[1].Kind = asEByteCodeOperandKind::ImmediateS32;
		Ops[1].Immediate = static_cast<int32>(Literal->GetValue());
		return Frame.Builder->Emit(asBC_SetV4, MakeArrayView(Ops, 2)) == asEByteCodeImageStatus::Succeeded;
	}
	if (auto* Boolean = dyn_cast<asCBooleanLiteral>(Expr))
	{
		OutLocal = Frame.AllocLocal();
		asSByteCodeOperand Ops[2];
		Ops[0].Kind = asEByteCodeOperandKind::Local;
		Ops[0].Immediate = static_cast<int64>(OutLocal);
		Ops[1].Kind = asEByteCodeOperandKind::ImmediateS32;
		Ops[1].Immediate = Boolean->GetValue() ? 1 : 0;
		return Frame.Builder->Emit(asBC_SetV4, MakeArrayView(Ops, 2)) == asEByteCodeImageStatus::Succeeded;
	}
	if (auto* Floating = dyn_cast<asCFloatingLiteral>(Expr))
	{
		const asEPrimitiveType Primitive = PrimitiveOf(Floating);
		OutLocal = static_cast<int32>(Frame.AllocLocal(DwordWidth(Primitive)));
		asSByteCodeOperand Ops[2];
		Ops[0].Kind = asEByteCodeOperandKind::Local;
		Ops[0].Immediate = static_cast<int64>(OutLocal);
		if (Primitive == asEPrimitiveType::Float32)
		{
			const float Value = static_cast<float>(Floating->GetValue());
			int32 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			Ops[1].Kind = asEByteCodeOperandKind::ImmediateS32;
			Ops[1].Immediate = Bits;
			return Frame.Builder->Emit(asBC_SetV4, MakeArrayView(Ops, 2)) == asEByteCodeImageStatus::Succeeded;
		}
		Ops[1].Kind = asEByteCodeOperandKind::ImmediateS64;
		double Value = Floating->GetValue();
		int64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		Ops[1].Immediate = Bits;
		return Frame.Builder->Emit(asBC_SetV8, MakeArrayView(Ops, 2)) == asEByteCodeImageStatus::Succeeded;
	}
	if (auto* DeclRef = dyn_cast<asCDeclRefExpr>(Expr))
	{
		asCDecl* Decl = DeclRef->GetDecl();
		int32 Slot = MAX_int32;
		if (const int32* Existing = Frame.Locals.Find(Decl))
		{
			Slot = *Existing;
		}
		else if (auto* Var = dyn_cast<asCVarDecl>(Decl))
		{
			if (Frame.Function && Var->GetKind() == asEDeclKind::ParmVar)
			{
				Slot = ParameterLocal(*Frame.Function, Var->GetName());
				if (Slot == MAX_int32) return false;
				Frame.Locals.Add(Decl, Slot);
			}
			else if (asCGlobalProperty* Global = Frame.Globals.FindRef(Var->GetStableIdentity()))
			{
				asUINT GlobalSlot = 0;
				if (Frame.Builder->AddGlobalRequirement(*Global, GlobalSlot) != asEByteCodeImageStatus::Succeeded)
				{
					return false;
				}
				OutLocal = static_cast<int32>(Frame.AllocLocal(DwordWidth(PrimitiveOf(DeclRef))));
				asSByteCodeOperand Ops[2];
				Ops[0].Kind = asEByteCodeOperandKind::Local;
				Ops[0].Immediate = static_cast<int64>(OutLocal);
				Ops[1].Kind = asEByteCodeOperandKind::GlobalSlot;
				Ops[1].Slot = GlobalSlot;
				return Frame.Builder->Emit(asBC_CpyGtoV4, MakeArrayView(Ops, 2)) == asEByteCodeImageStatus::Succeeded;
			}
		}
		else if (auto* EnumConstant = dyn_cast<asCEnumConstantDecl>(Decl))
		{
			OutLocal = static_cast<int32>(Frame.AllocLocal());
			asSByteCodeOperand Ops[2];
			Ops[0].Kind = asEByteCodeOperandKind::Local;
			Ops[0].Immediate = static_cast<int64>(OutLocal);
			Ops[1].Kind = asEByteCodeOperandKind::ImmediateS32;
			Ops[1].Immediate = static_cast<int32>(EnumConstant->GetValue());
			return Frame.Builder->Emit(asBC_SetV4, MakeArrayView(Ops, 2)) == asEByteCodeImageStatus::Succeeded;
		}
		if (Slot == MAX_int32) return false;
		auto* Var = dyn_cast<asCVarDecl>(Decl);
		if (Var && Var->GetKind() == asEDeclKind::ParmVar && Var->GetResolvedQualType().IsReference())
		{
			asSByteCodeOperand Pointer;
			Pointer.Kind = asEByteCodeOperandKind::Local;
			Pointer.Immediate = Slot;
			if (Frame.Builder->Emit(asBC_PshVPtr, MakeArrayView(&Pointer, 1)) != asEByteCodeImageStatus::Succeeded
				|| Frame.Builder->Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>()) != asEByteCodeImageStatus::Succeeded)
			{
				return false;
			}
			OutLocal = static_cast<int32>(Frame.AllocLocal());
			asSByteCodeOperand Dest;
			Dest.Kind = asEByteCodeOperandKind::Local;
			Dest.Immediate = OutLocal;
			return Frame.Builder->Emit(asBC_RDR4, MakeArrayView(&Dest, 1)) == asEByteCodeImageStatus::Succeeded;
		}
		OutLocal = Slot;
		return true;
	}
	if (auto* Cast = dyn_cast<asCImplicitCastExpr>(Expr))
	{
		int32 Source = 0;
		if (!asEmitExpr(Frame, Cast->GetOperand(), Source)) return false;
		if (Cast->GetConversionKind() == asEImplicitConversionKind::Numeric)
		{
			return EmitNumericConversion(Frame, Source, PrimitiveOf(Cast->GetOperand()), PrimitiveOf(Cast), OutLocal);
		}
		OutLocal = Source;
		return true;
	}
	if (auto* Unary = dyn_cast<asCUnaryOperator>(Expr))
	{
		int32 Operand = 0;
		if (!asEmitExpr(Frame, Unary->GetOperand(), Operand)) return false;
		const asEPrimitiveType Primitive = PrimitiveOf(Unary);
		OutLocal = static_cast<int32>(Frame.AllocLocal(DwordWidth(Primitive)));
		if (!EmitTwoLocal(Frame, CopyVtoV(Primitive), OutLocal, Operand)) return false;
		asSByteCodeOperand Neg;
		Neg.Kind = asEByteCodeOperandKind::Local;
		Neg.Immediate = static_cast<int64>(OutLocal);
		if (Unary->GetOperator() == asEUnaryOperator::Minus)
		{
			return Frame.Builder->Emit(NegOpcode(Primitive), MakeArrayView(&Neg, 1)) == asEByteCodeImageStatus::Succeeded;
		}
		if (Unary->GetOperator() == asEUnaryOperator::BitwiseNot)
		{
			const asEBCInstr Not = DwordWidth(Primitive) == 2 ? asBC_BNOT64 : asBC_BNOT;
			return Frame.Builder->Emit(Not, MakeArrayView(&Neg, 1)) == asEByteCodeImageStatus::Succeeded;
		}
		return false;
	}
	if (auto* Binary = dyn_cast<asCBinaryOperator>(Expr))
	{
		if (Binary->GetOperator() == asEBinaryOperator::Assign)
		{
			int32 Right = 0;
			if (!asEmitExpr(Frame, Binary->GetRight(), Right)) return false;
			if (auto* Member = dyn_cast<asCMemberExpr>(Binary->GetLeft()))
			{
				if (!asEmitMemberAddress(Frame, Member)) return false;
				asSByteCodeOperand Source;
				Source.Kind = asEByteCodeOperandKind::Local;
				Source.Immediate = Right;
				if (Frame.Builder->Emit(asBC_WRTV4, MakeArrayView(&Source, 1)) != asEByteCodeImageStatus::Succeeded)
				{
					return false;
				}
				OutLocal = Right;
				return true;
			}
			auto* DestRef = dyn_cast<asCDeclRefExpr>(Binary->GetLeft());
			if (!DestRef) return false;
			auto* DestVar = dyn_cast<asCVarDecl>(DestRef->GetDecl());
			if (DestVar && DestVar->GetKind() == asEDeclKind::ParmVar && DestVar->GetResolvedQualType().IsReference())
			{
				int32 Slot = MAX_int32;
				if (const int32* Existing = Frame.Locals.Find(DestVar))
				{
					Slot = *Existing;
				}
				else if (Frame.Function)
				{
					Slot = ParameterLocal(*Frame.Function, DestVar->GetName());
					if (Slot != MAX_int32) Frame.Locals.Add(DestVar, Slot);
				}
				if (Slot == MAX_int32) return false;
				asSByteCodeOperand Pointer;
				Pointer.Kind = asEByteCodeOperandKind::Local;
				Pointer.Immediate = Slot;
				if (Frame.Builder->Emit(asBC_PshVPtr, MakeArrayView(&Pointer, 1)) != asEByteCodeImageStatus::Succeeded
					|| Frame.Builder->Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>()) != asEByteCodeImageStatus::Succeeded)
				{
					return false;
				}
				asSByteCodeOperand Source;
				Source.Kind = asEByteCodeOperandKind::Local;
				Source.Immediate = Right;
				if (Frame.Builder->Emit(asBC_WRTV4, MakeArrayView(&Source, 1)) != asEByteCodeImageStatus::Succeeded)
				{
					return false;
				}
				OutLocal = Right;
				return true;
			}
			int32 Dest = 0;
			if (!asEmitExpr(Frame, DestRef, Dest)) return false;
			OutLocal = Dest;
			return EmitTwoLocal(Frame, CopyVtoV(PrimitiveOf(Binary)), Dest, Right);
		}
		if (Binary->GetOperator() == asEBinaryOperator::LogicalAnd || Binary->GetOperator() == asEBinaryOperator::LogicalOr)
		{
			int32 Left = 0;
			if (!asEmitExpr(Frame, Binary->GetLeft(), Left)) return false;
			OutLocal = static_cast<int32>(Frame.AllocLocal());
			if (!EmitTwoLocal(Frame, asBC_CpyVtoV4, OutLocal, Left) || !EmitCpyToR4(Frame, Left)) return false;
			const asUINT SkipLabel = Frame.Builder->NewLabel();
			const asEBCInstr Skip = Binary->GetOperator() == asEBinaryOperator::LogicalAnd ? asBC_JZ : asBC_JNZ;
			if (!EmitJump(Frame, Skip, SkipLabel)) return false;
			int32 Right = 0;
			if (!asEmitExpr(Frame, Binary->GetRight(), Right)) return false;
			if (!EmitTwoLocal(Frame, asBC_CpyVtoV4, OutLocal, Right)) return false;
			return Frame.Builder->BindLabel(SkipLabel) == asEByteCodeImageStatus::Succeeded;
		}
		int32 Left = 0;
		int32 Right = 0;
		if (!asEmitExpr(Frame, Binary->GetLeft(), Left) || !asEmitExpr(Frame, Binary->GetRight(), Right)) return false;
		bool bCompare = false;
		switch (Binary->GetOperator())
		{
		case asEBinaryOperator::Equal:
		case asEBinaryOperator::NotEqual:
		case asEBinaryOperator::Less:
		case asEBinaryOperator::LessEqual:
		case asEBinaryOperator::Greater:
		case asEBinaryOperator::GreaterEqual:
			bCompare = true; break;
		case asEBinaryOperator::Add:
		case asEBinaryOperator::Subtract:
		case asEBinaryOperator::Multiply:
		case asEBinaryOperator::Divide:
		case asEBinaryOperator::Modulo:
		case asEBinaryOperator::Power:
		case asEBinaryOperator::BitwiseOr:
		case asEBinaryOperator::BitwiseAnd:
			break;
		default: return false;
		}
		if (bCompare)
		{
			asSByteCodeOperand Cmp[2];
			Cmp[0].Kind = asEByteCodeOperandKind::Local;
			Cmp[0].Immediate = static_cast<int64>(Left);
			Cmp[1].Kind = asEByteCodeOperandKind::Local;
			Cmp[1].Immediate = static_cast<int64>(Right);
			if (Frame.Builder->Emit(CompareOpcode(PrimitiveOf(Binary->GetLeft())), MakeArrayView(Cmp, 2)) != asEByteCodeImageStatus::Succeeded) return false;
			asEBCInstr Test = asBC_TZ;
			switch (Binary->GetOperator())
			{
			case asEBinaryOperator::Equal: Test = asBC_TZ; break;
			case asEBinaryOperator::NotEqual: Test = asBC_TNZ; break;
			case asEBinaryOperator::Less: Test = asBC_TS; break;
			case asEBinaryOperator::LessEqual: Test = asBC_TNP; break;
			case asEBinaryOperator::Greater: Test = asBC_TP; break;
			case asEBinaryOperator::GreaterEqual: Test = asBC_TNS; break;
			default: break;
			}
			if (Frame.Builder->Emit(Test, TConstArrayView<asSByteCodeOperand>()) != asEByteCodeImageStatus::Succeeded) return false;
			OutLocal = Frame.AllocLocal();
			asSByteCodeOperand Copy;
			Copy.Kind = asEByteCodeOperandKind::Local;
			Copy.Immediate = static_cast<int64>(OutLocal);
			return Frame.Builder->Emit(asBC_CpyRtoV4, MakeArrayView(&Copy, 1)) == asEByteCodeImageStatus::Succeeded;
		}
		const asEPrimitiveType ResultPrimitive = PrimitiveOf(Binary);
		OutLocal = static_cast<int32>(Frame.AllocLocal(DwordWidth(ResultPrimitive)));
		return EmitThreeLocal(Frame, ArithOpcode(Binary->GetOperator(), ResultPrimitive), OutLocal, Left, Right);
	}
	return false;
}

END_AS_NAMESPACE
