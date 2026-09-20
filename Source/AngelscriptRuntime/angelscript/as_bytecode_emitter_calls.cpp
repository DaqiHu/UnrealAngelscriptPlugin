#include "as_bytecode_emitter_internal.h"
#include "as_bytecode_opcode.h"
#include "as_datatype.h"
#include "as_definitions.h"
#include "as_typeinfo.h"
#include "frontend/AST/as_type.h"
#include "frontend/Compile/as_compilation_session.h"

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

	bool EmitWriteCallableField(asSByteCodeEmitFrame& Frame, asCExpr* Base, int32 HandlerLocal)
	{
		asCExpr* Unwrapped = Base;
		while (auto* Paren = dyn_cast<asCParenExpr>(Unwrapped))
		{
			Unwrapped = Paren->GetSubExpr();
		}
		auto* Field = dyn_cast<asCMemberExpr>(Unwrapped);
		if (!Field) return true;
		if (!asEmitMemberAddress(Frame, Field)) return false;
		return EmitLocal(Frame, asBC_WRTV8, HandlerLocal);
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

	bool ParameterPassesAsAddress(const asCScriptFunction& Target, int32 Index)
	{
		if (Index < 0 || Index >= static_cast<int32>(Target.parameterTypes.GetLength()))
		{
			return false;
		}
		const asCDataType& Type = Target.parameterTypes[Index];
		return Type.GetTypeInfo() != nullptr && !Type.IsEnumType() && !Type.IsObjectHandle();
	}

	bool ParameterNeedsAddress(const asCScriptFunction& Target, int32 Index)
	{
		return ParameterIsReference(Target, Index) || ParameterPassesAsAddress(Target, Index);
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

	asCExpr* UnwrapTyped(asCExpr* Expr)
	{
		for (;;)
		{
			Expr = Unwrap(Expr);
			if (auto* Cast = dyn_cast<asCImplicitCastExpr>(Expr))
			{
				Expr = Cast->GetOperand();
				continue;
			}
			return Expr;
		}
	}

	asCScriptFunction* FindCallPtrSignature(asSByteCodeEmitFrame& Frame, asCExpr* Callee)
	{
		if (!Frame.Session || !Callee)
		{
			return nullptr;
		}
		asCType* Type = Callee->GetExpressionType();
		if (!Type)
		{
			if (asCExpr* Inner = UnwrapTyped(Callee))
			{
				Type = Inner->GetExpressionType();
			}
		}
		if (!Type)
		{
			return nullptr;
		}
		const asCDefinitions* Set = Frame.Session->GetFrozenDefinitions();
		if (const asCNominalType* Nominal = dyn_cast<asCNominalType>(Type))
		{
			if (const asCCallableTypeDecl* Decl = dyn_cast<asCCallableTypeDecl>(
				Frame.Session->FindNominalDeclaration(Nominal->GetDeclarationIdentity())))
			{
				if (Set)
				{
					if (asCTypeInfo* Info = Set->FindType(Decl->GetStableIdentity()))
					{
						if (asCCallableType* Callable = CastToCallableType(Info))
						{
							return static_cast<asCScriptFunction*>(Callable->GetCallableSignature());
						}
					}
				}
				if (asCFunctionType* Signature = Decl->GetResolvedSignature())
				{
					if (Set)
					{
						if (asCScriptFunction* Function = Set->FindFunction(Signature->GetStableIdentity()))
						{
							return Function;
						}
					}
					return Frame.Functions.FindRef(Signature->GetStableIdentity());
				}
			}
			if (Set)
			{
				if (asCTypeInfo* Info = Set->FindType(Nominal->GetDeclarationIdentity()))
				{
					if (asCCallableType* Callable = CastToCallableType(Info))
					{
						return static_cast<asCScriptFunction*>(Callable->GetCallableSignature());
					}
				}
			}
		}
		if (const asCFunctionType* FunctionType = dyn_cast<asCFunctionType>(Type))
		{
			if (Set)
			{
				if (asCScriptFunction* Function = Set->FindFunction(FunctionType->GetStableIdentity()))
				{
					return Function;
				}
			}
			return Frame.Functions.FindRef(FunctionType->GetStableIdentity());
		}
		return nullptr;
	}
}

bool asEmitCall(asSByteCodeEmitFrame& Frame, asCCallExpr* Call, int32& OutLocal)
{
	if (!Frame.Builder || !Call) return false;
	asCExpr* CalleeExpr = Call->GetCallee();
	asCScriptFunction* Target = nullptr;
	int32 ThisLocal = MAX_int32;
	int32 IndirectLocal = MAX_int32;
	if (auto* Member = dyn_cast<asCMemberExpr>(CalleeExpr))
	{
		if (asEmitCallableDecl(Frame, Member->GetBase() ? Member->GetBase()->GetExpressionType() : nullptr))
		{
			const FString& Operation = Call->GetCallableOperation();
			if (Operation == TEXT("IsBound"))
			{
				int32 HandlerLocal = 0;
				if (!asEmitExpr(Frame, Member->GetBase(), HandlerLocal)) return false;
				if (!EmitLocal(Frame, asBC_CmpPtrNull, HandlerLocal)) return false;
				if (Frame.Builder->Emit(asBC_TNZ, TConstArrayView<asSByteCodeOperand>()) != asEByteCodeImageStatus::Succeeded)
					return false;
				OutLocal = static_cast<int32>(Frame.AllocLocal());
				return EmitLocal(Frame, asBC_CpyRtoV4, OutLocal);
			}
			if (Operation == TEXT("BindUFunction") || Operation == TEXT("AddDynamic"))
			{
				if (Call->GetArguments().Num() != 2) return false;
				int32 HandlerLocal = 0;
				if (!asEmitExpr(Frame, Member->GetBase(), HandlerLocal)) return false;
				asUINT PayloadDwords = 0;
				for (int32 Index = 1; Index >= 0; --Index)
				{
					int32 ArgLocal = 0;
					if (!asEmitExpr(Frame, Call->GetArguments()[Index], ArgLocal)) return false;
					const int32 Width = (Index == 0) ? AS_PTR_SIZE : 2;
					const asEBCInstr Push = Width == 2 ? asBC_PshV8 : asBC_PshV4;
					if (!EmitLocal(Frame, Push, ArgLocal)) return false;
					PayloadDwords += static_cast<asUINT>(Width);
				}
				if (Frame.Builder->Emit(asBC_PshNull, TConstArrayView<asSByteCodeOperand>()) != asEByteCodeImageStatus::Succeeded)
					return false;
				if (Frame.Builder->Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>()) != asEByteCodeImageStatus::Succeeded)
					return false;
				asSByteCodeOperand Count;
				Count.Kind = asEByteCodeOperandKind::ImmediateS16;
				Count.Immediate = static_cast<int32>(PayloadDwords);
				if (Frame.Builder->Emit(asBC_BindPtr, MakeArrayView(&Count, 1)) != asEByteCodeImageStatus::Succeeded)
					return false;
				asSByteCodeOperand Dest;
				Dest.Kind = asEByteCodeOperandKind::Local;
				Dest.Immediate = HandlerLocal;
				const bool bAdd = Operation == TEXT("AddDynamic");
				const asEBCInstr Store = bAdd ? asBC_AddPtr : asBC_CpyRtoV8;
				if (Frame.Builder->Emit(Store, MakeArrayView(&Dest, 1)) != asEByteCodeImageStatus::Succeeded)
					return false;
				if (bAdd)
				{
					OutLocal = static_cast<int32>(Frame.AllocLocal(2));
					if (!EmitLocal(Frame, asBC_CpyRtoV8, OutLocal)) return false;
					return EmitWriteCallableField(Frame, Member->GetBase(), HandlerLocal);
				}
				OutLocal = 0;
				return EmitWriteCallableField(Frame, Member->GetBase(), HandlerLocal);
			}
			if (Operation == TEXT("Remove") || Operation == TEXT("Clear") || Operation == TEXT("RemoveAll"))
			{
				int32 HandlerLocal = 0;
				if (!asEmitExpr(Frame, Member->GetBase(), HandlerLocal)) return false;
				if (Operation == TEXT("Remove"))
				{
					if (Call->GetArguments().Num() != 1) return false;
					int32 HandleLocal = 0;
					if (!asEmitExpr(Frame, Call->GetArguments()[0], HandleLocal)) return false;
					asSByteCodeOperand HandleOp;
					HandleOp.Kind = asEByteCodeOperandKind::Local;
					HandleOp.Immediate = HandleLocal;
					if (Frame.Builder->Emit(asBC_CpyVtoR8, MakeArrayView(&HandleOp, 1)) != asEByteCodeImageStatus::Succeeded)
						return false;
					asSByteCodeOperand Dest;
					Dest.Kind = asEByteCodeOperandKind::Local;
					Dest.Immediate = HandlerLocal;
					if (Frame.Builder->Emit(asBC_RemovePtr, MakeArrayView(&Dest, 1)) != asEByteCodeImageStatus::Succeeded)
						return false;
				}
				else
				{
					asSByteCodeOperand Dest;
					Dest.Kind = asEByteCodeOperandKind::Local;
					Dest.Immediate = HandlerLocal;
					if (Frame.Builder->Emit(asBC_ClearPtr, MakeArrayView(&Dest, 1)) != asEByteCodeImageStatus::Succeeded)
						return false;
				}
				OutLocal = 0;
				return EmitWriteCallableField(Frame, Member->GetBase(), HandlerLocal);
			}
		}
		if (auto* FunctionDecl = dyn_cast<asCFunctionDecl>(Member->GetMember()))
		{
			if (asEmitCallableDecl(Frame, Member->GetBase() ? Member->GetBase()->GetExpressionType() : nullptr))
			{
				int32 HandlerLocal = 0;
				if (!asEmitExpr(Frame, Member->GetBase(), HandlerLocal)) return false;
				asCScriptFunction* Selected = Frame.Functions.FindRef(FunctionDecl->GetStableIdentity());
				if (!Selected) return false;
				asUINT Slot = 0;
				if (Frame.Builder->AddFunctionRequirement(*Selected, Slot) != asEByteCodeImageStatus::Succeeded) return false;
				asSByteCodeOperand FuncOp;
				FuncOp.Kind = asEByteCodeOperandKind::FunctionSlot;
				FuncOp.Slot = Slot;
				if (Frame.Builder->Emit(asBC_FuncPtr, MakeArrayView(&FuncOp, 1)) != asEByteCodeImageStatus::Succeeded) return false;
				if (Frame.Builder->Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>()) != asEByteCodeImageStatus::Succeeded) return false;
				const int32 FunctionLocal = static_cast<int32>(Frame.AllocLocal(AS_PTR_SIZE));
				if (!EmitLocal(Frame, asBC_CpyRtoV8, FunctionLocal)) return false;
				const TConstArrayView<asCExpr*> Payloads = Call->GetArguments();
				asUINT PayloadDwords = 0;
				for (int32 Index = Payloads.Num() - 1; Index >= 0; --Index)
				{
					int32 PayloadLocal = 0;
					if (!asEmitExpr(Frame, Payloads[Index], PayloadLocal)) return false;
					int32 Width = 1;
					if (const asCQualType Type = Payloads[Index]->GetExpressionQualType(); Type.IsValid())
					{
						if (Type.IsReference() || Type.IsHandle() || asEmitCallableDecl(Frame, Type.GetType()))
						{
							Width = AS_PTR_SIZE;
						}
						else if (const auto* Builtin = dyn_cast<asCBuiltinType>(Type.GetType()))
						{
							const asEPrimitiveType Primitive = Builtin->GetPrimitive();
							Width = (Primitive == asEPrimitiveType::Float64
								|| Primitive == asEPrimitiveType::Int64
								|| Primitive == asEPrimitiveType::UInt64) ? 2 : 1;
						}
					}
					const asEBCInstr Push = Width == 2 ? asBC_PshV8 : asBC_PshV4;
					if (!EmitLocal(Frame, Push, PayloadLocal)) return false;
					PayloadDwords += static_cast<asUINT>(Width);
				}
				if (!EmitLocal(Frame, asBC_CpyVtoR8, FunctionLocal)) return false;
				if (PayloadDwords > 0)
				{
					asSByteCodeOperand Count;
					Count.Kind = asEByteCodeOperandKind::ImmediateS16;
					Count.Immediate = static_cast<int32>(PayloadDwords);
					if (Frame.Builder->Emit(asBC_BindPtr, MakeArrayView(&Count, 1)) != asEByteCodeImageStatus::Succeeded) return false;
				}
				asSByteCodeOperand Dest;
				Dest.Kind = asEByteCodeOperandKind::Local;
				Dest.Immediate = HandlerLocal;
				const bool bAdd = Call->GetCallableOperation() == TEXT("Add");
				const asEBCInstr Store = bAdd ? asBC_AddPtr : asBC_CpyRtoV8;
				if (Frame.Builder->Emit(Store, MakeArrayView(&Dest, 1)) != asEByteCodeImageStatus::Succeeded) return false;
				if (bAdd)
				{
					OutLocal = static_cast<int32>(Frame.AllocLocal(2));
					if (!EmitLocal(Frame, asBC_CpyRtoV8, OutLocal)) return false;
					return EmitWriteCallableField(Frame, Member->GetBase(), HandlerLocal);
				}
				OutLocal = 0;
				return EmitWriteCallableField(Frame, Member->GetBase(), HandlerLocal);
			}
			Target = Frame.Functions.FindRef(FunctionDecl->GetStableIdentity());
		}
		if (asEmitCallableDecl(Frame, Member->GetExpressionType())
			|| asEmitCallableDecl(Frame, CalleeExpr->GetExpressionType()))
		{
			// Callable field / value invoke uses CallPtr. Do not emit a this pointer.
		}
		else if (!asEmitExpr(Frame, Member->GetBase(), ThisLocal))
		{
			return false;
		}
	}
	else if (auto* Ref = dyn_cast<asCDeclRefExpr>(CalleeExpr))
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
	if (!Target)
	{
		Target = FindCallPtrSignature(Frame, CalleeExpr);
		if (!Target || !asEmitExpr(Frame, CalleeExpr, IndirectLocal)) return false;
	}
	asUINT Slot = 0;
	if (Frame.Builder->AddFunctionRequirement(*Target, Slot) != asEByteCodeImageStatus::Succeeded) return false;
	asUINT SkipUnbound = 0;
	if (Call->GetCallableOperation() == TEXT("ExecuteIfBound") && IndirectLocal != MAX_int32)
	{
		if (!EmitLocal(Frame, asBC_CmpPtrNull, IndirectLocal)) return false;
		SkipUnbound = Frame.Builder->NewLabel();
		asSByteCodeOperand JumpSkip;
		JumpSkip.Kind = asEByteCodeOperandKind::Label;
		JumpSkip.Label = SkipUnbound;
		if (Frame.Builder->Emit(asBC_JZ, MakeArrayView(&JumpSkip, 1)) != asEByteCodeImageStatus::Succeeded)
			return false;
	}
	const TConstArrayView<asCExpr*> Arguments = Call->GetArguments();
	TArray<int32> ArgLocals;
	TArray<uint8> ArgPointerValue;
	ArgLocals.SetNum(Arguments.Num());
	ArgPointerValue.SetNumZeroed(Arguments.Num());
	for (int32 Index = Arguments.Num() - 1; Index >= 0; --Index)
	{
		if (ParameterNeedsAddress(*Target, Index))
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
		if (ParameterNeedsAddress(*Target, Index))
		{
			if (ParameterIsReference(*Target, Index)
				&& ParameterFlags(*Target, Index) == asTM_INREF
				&& ArgPointerValue[Index] == 0)
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
		else
		{
			const int32 Width = (Index >= 0 && Index < static_cast<int32>(Target->parameterTypes.GetLength()))
				? Target->parameterTypes[Index].GetSizeOnStackDWords()
				: 1;
			if (!EmitLocal(Frame, Width >= 2 ? asBC_PshV8 : asBC_PshV4, Local))
			{
				return false;
			}
		}
	}
	if (ThisLocal != MAX_int32 && !EmitLocal(Frame, asBC_PshVPtr, ThisLocal)) return false;
	if (IndirectLocal != MAX_int32)
	{
		asSByteCodeOperand Indirect[2];
		Indirect[0].Kind = asEByteCodeOperandKind::Local;
		Indirect[0].Immediate = IndirectLocal;
		Indirect[1].Kind = asEByteCodeOperandKind::FunctionSlot;
		Indirect[1].Slot = Slot;
		if (Frame.Builder->Emit(asBC_CallPtr, MakeArrayView(Indirect, 2)) != asEByteCodeImageStatus::Succeeded) return false;
	}
	else
	{
		asSByteCodeOperand CallOp;
		CallOp.Kind = asEByteCodeOperandKind::FunctionSlot;
		CallOp.Slot = Slot;
		const asEBCInstr CallOpcode = Target->funcType == asFUNC_SYSTEM ? asBC_CALLSYS : asBC_CALL;
		if (Frame.Builder->Emit(CallOpcode, MakeArrayView(&CallOp, 1)) != asEByteCodeImageStatus::Succeeded) return false;
	}
	if (Target->returnType.GetTokenType() == ttVoid)
	{
		OutLocal = 0;
		if (SkipUnbound != 0
			&& Frame.Builder->BindLabel(SkipUnbound) != asEByteCodeImageStatus::Succeeded)
		{
			return false;
		}
		return true;
	}
	OutLocal = static_cast<int32>(Frame.AllocLocal());
	if (!EmitLocal(Frame, asBC_CpyRtoV4, OutLocal)) return false;
	if (SkipUnbound != 0
		&& Frame.Builder->BindLabel(SkipUnbound) != asEByteCodeImageStatus::Succeeded)
	{
		return false;
	}
	return true;
}

END_AS_NAMESPACE
