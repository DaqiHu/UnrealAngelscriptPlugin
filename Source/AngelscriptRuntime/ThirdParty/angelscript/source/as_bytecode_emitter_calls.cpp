#include "as_bytecode_emitter_internal.h"
#include "as_datatype.h"

BEGIN_AS_NAMESPACE

namespace
{
	bool EmitLocal(asSByteCodeEmitFrame& Frame, asEBCInstr Opcode, int32 Local)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::Local;
		Operand.Immediate = Local;
		return Frame.Builder->Emit(Opcode, MakeArrayView(&Operand, 1)) == asEByteCodeImageStatus::Succeeded;
	}

	asETypeModifiers ParameterFlags(const asCScriptFunction& Target, int32 Index)
	{
		if (Index < 0 || Index >= static_cast<int32>(Target.inOutFlags.GetLength()))
		{
			return asTM_NONE;
		}
		return Target.inOutFlags[Index];
	}

	bool ParameterIsReference(const asCScriptFunction& Target, int32 Index)
	{
		if (Index < 0 || Index >= static_cast<int32>(Target.parameterTypes.GetLength()))
		{
			return false;
		}
		return Target.parameterTypes[Index].IsReference() || ParameterFlags(Target, Index) != asTM_NONE;
	}

	asCExpr* Unwrap(asCExpr* Expr)
	{
		while (auto* Paren = dyn_cast<asCParenExpr>(Expr))
		{
			Expr = Paren->GetSubExpr();
		}
		return Expr;
	}

	int32 ParameterSlot(asCScriptFunction& Function, const FString& Name)
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

	bool StoreRegisterPointer(asSByteCodeEmitFrame& Frame, int32& OutLocal)
	{
		OutLocal = static_cast<int32>(Frame.AllocLocal(2));
		asSByteCodeOperand Dest;
		Dest.Kind = asEByteCodeOperandKind::Local;
		Dest.Immediate = OutLocal;
		return Frame.Builder->Emit(asBC_CpyRtoV8, MakeArrayView(&Dest, 1)) == asEByteCodeImageStatus::Succeeded;
	}

	bool EmitRefArgument(asSByteCodeEmitFrame& Frame, asCExpr* Expr, int32& OutLocal, bool& bPointerValue)
	{
		Expr = Unwrap(Expr);
		bPointerValue = false;
		if (auto* Member = dyn_cast<asCMemberExpr>(Expr))
		{
			if (!asEmitMemberAddress(Frame, Member) || !StoreRegisterPointer(Frame, OutLocal))
			{
				return false;
			}
			bPointerValue = true;
			return true;
		}
		if (auto* DeclRef = dyn_cast<asCDeclRefExpr>(Expr))
		{
			if (auto* Var = dyn_cast<asCVarDecl>(DeclRef->GetDecl()))
			{
				if (Var->GetKind() == asEDeclKind::ParmVar && Var->GetResolvedQualType().IsReference())
				{
					int32 Slot = MAX_int32;
					if (const int32* Existing = Frame.Locals.Find(Var))
					{
						Slot = *Existing;
					}
					else if (Frame.Function)
					{
						Slot = ParameterSlot(*Frame.Function, Var->GetName());
						if (Slot != MAX_int32)
						{
							Frame.Locals.Add(Var, Slot);
						}
					}
					if (Slot == MAX_int32)
					{
						return false;
					}
					OutLocal = Slot;
					bPointerValue = true;
					return true;
				}
			}
		}
		if (!asEmitExpr(Frame, Expr, OutLocal))
		{
			return false;
		}
		bPointerValue = false;
		return true;
	}
}

bool asEmitCall(asSByteCodeEmitFrame& Frame, asCCallExpr* Call, int32& OutLocal)
{
	if (!Frame.Builder || !Call) return false;
	asCExpr* CalleeExpr = Call->GetCallee();
	asCScriptFunction* Target = nullptr;
	int32 ThisLocal = MAX_int32;
	if (auto* Ref = dyn_cast<asCDeclRefExpr>(CalleeExpr))
	{
		Target = Ref->GetExternalFunction();
		if (!Target)
		{
			if (auto* FunctionDecl = dyn_cast<asCFunctionDecl>(Ref->GetDecl()))
			{
				Target = Frame.Functions.FindRef(FunctionDecl->GetStableIdentity());
			}
		}
	}
	else if (auto* Member = dyn_cast<asCMemberExpr>(CalleeExpr))
	{
		if (auto* FunctionDecl = dyn_cast<asCFunctionDecl>(Member->GetMember()))
		{
			Target = Frame.Functions.FindRef(FunctionDecl->GetStableIdentity());
		}
		if (!asEmitExpr(Frame, Member->GetBase(), ThisLocal)) return false;
	}
	if (!Target) return false;
	asUINT Slot = 0;
	if (Frame.Builder->AddFunctionRequirement(*Target, Slot) != asEByteCodeImageStatus::Succeeded) return false;
	const TConstArrayView<asCExpr*> Arguments = Call->GetArguments();
	TArray<int32> ArgLocals;
	TArray<uint8> ArgPointerValue;
	ArgLocals.SetNum(Arguments.Num());
	ArgPointerValue.SetNumZeroed(Arguments.Num());
	for (int32 Index = Arguments.Num() - 1; Index >= 0; --Index)
	{
		if (ParameterIsReference(*Target, Index))
		{
			bool bPointerValue = false;
			if (!EmitRefArgument(Frame, Arguments[Index], ArgLocals[Index], bPointerValue)) return false;
			ArgPointerValue[Index] = bPointerValue ? 1 : 0;
		}
		else if (!asEmitExpr(Frame, Arguments[Index], ArgLocals[Index]))
		{
			return false;
		}
	}
	for (int32 Index = Arguments.Num() - 1; Index >= 0; --Index)
	{
		int32 Local = ArgLocals[Index];
		if (ParameterIsReference(*Target, Index))
		{
			if (ParameterFlags(*Target, Index) == asTM_INREF && ArgPointerValue[Index] == 0)
			{
				const int32 Copy = static_cast<int32>(Frame.AllocLocal());
				asSByteCodeOperand Ops[2];
				Ops[0].Kind = asEByteCodeOperandKind::Local;
				Ops[0].Immediate = Copy;
				Ops[1].Kind = asEByteCodeOperandKind::Local;
				Ops[1].Immediate = Local;
				if (Frame.Builder->Emit(asBC_CpyVtoV4, MakeArrayView(Ops, 2)) != asEByteCodeImageStatus::Succeeded)
				{
					return false;
				}
				Local = Copy;
			}
			const asEBCInstr Push = ArgPointerValue[Index] ? asBC_PshVPtr : asBC_PSF;
			if (!EmitLocal(Frame, Push, Local)) return false;
		}
		else if (!EmitLocal(Frame, asBC_PshV4, Local))
		{
			return false;
		}
	}
	if (ThisLocal != MAX_int32 && !EmitLocal(Frame, asBC_PshVPtr, ThisLocal)) return false;
	asSByteCodeOperand CallOp;
	CallOp.Kind = asEByteCodeOperandKind::FunctionSlot;
	CallOp.Slot = Slot;
	const asEBCInstr CallOpcode = Target->funcType == asFUNC_SYSTEM ? asBC_CALLSYS : asBC_CALL;
	if (Frame.Builder->Emit(CallOpcode, MakeArrayView(&CallOp, 1)) != asEByteCodeImageStatus::Succeeded) return false;
	if (Target->returnType.GetTokenType() == ttVoid)
	{
		OutLocal = 0;
		return true;
	}
	OutLocal = static_cast<int32>(Frame.AllocLocal());
	return EmitLocal(Frame, asBC_CpyRtoV4, OutLocal);
}

END_AS_NAMESPACE
