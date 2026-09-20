#include "as_bytecode_emitter_internal.h"
#include "as_bytecode_opcode.h"
#include "as_config.h"
#include "frontend/AST/as_type.h"

BEGIN_AS_NAMESPACE

namespace
{
	asEPrimitiveType StmtPrimitive(asCType* Type)
	{
		if (const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Type))
		{
			return Builtin->GetPrimitive();
		}
		return asEPrimitiveType::Int32;
	}

	asUINT StmtDwordWidth(asEPrimitiveType Primitive)
	{
		switch (Primitive)
		{
		case asEPrimitiveType::Int64:
		case asEPrimitiveType::UInt64:
		case asEPrimitiveType::Float64:
			return 2;
		default:
			return 1;
		}
	}

	asEBCInstr StmtCopyVtoV(asEPrimitiveType Primitive)
	{
		return StmtDwordWidth(Primitive) == 2 ? asBC_CpyVtoV8 : asBC_CpyVtoV4;
	}

	asEBCInstr StmtCopyVtoR(asEPrimitiveType Primitive)
	{
		return StmtDwordWidth(Primitive) == 2 ? asBC_CpyVtoR8 : asBC_CpyVtoR4;
	}

	bool EmitStmtJump(asSByteCodeEmitFrame& Frame, asEBCInstr Opcode, asUINT Label)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::Label;
		Operand.Label = Label;
		return Frame.Builder->Emit(Opcode, MakeArrayView(&Operand, 1)) == asEByteCodeImageStatus::Succeeded;
	}

	bool EmitStmtCpyToR4(asSByteCodeEmitFrame& Frame, int32 Local)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::Local;
		Operand.Immediate = Local;
		return Frame.Builder->Emit(asBC_CpyVtoR4, MakeArrayView(&Operand, 1)) == asEByteCodeImageStatus::Succeeded;
	}

	bool EmitLoopCondition(asSByteCodeEmitFrame& Frame, asCExpr* Condition, asUINT ExitLabel)
	{
		if (!Condition) return true;
		int32 Value = 0;
		if (!asEmitExpr(Frame, Condition, Value) || !EmitStmtCpyToR4(Frame, Value)) return false;
		return EmitStmtJump(Frame, asBC_JZ, ExitLabel);
	}

	void PushBreak(asSByteCodeEmitFrame& Frame, asUINT Label, int32 LiveStart)
	{
		Frame.BreakLabels.Add(Label);
		Frame.BreakLiveStarts.Add(LiveStart);
	}

	void PopBreak(asSByteCodeEmitFrame& Frame)
	{
		if (Frame.BreakLabels.Num() > 0) Frame.BreakLabels.Pop();
		if (Frame.BreakLiveStarts.Num() > 0) Frame.BreakLiveStarts.Pop();
	}

	void PushContinue(asSByteCodeEmitFrame& Frame, asUINT Label, int32 LiveStart)
	{
		Frame.ContinueLabels.Add(Label);
		Frame.ContinueLiveStarts.Add(LiveStart);
	}

	void PopContinue(asSByteCodeEmitFrame& Frame)
	{
		if (Frame.ContinueLabels.Num() > 0) Frame.ContinueLabels.Pop();
		if (Frame.ContinueLiveStarts.Num() > 0) Frame.ContinueLiveStarts.Pop();
	}
}

bool asEmitReturn(asSByteCodeEmitFrame& Frame, asCReturnStmt* Stmt)
{
	if (!Stmt || !Stmt->GetValue()) return false;
	int32 Local = 0;
	if (!asEmitExpr(Frame, Stmt->GetValue(), Local)) return false;
	asSByteCodeOperand Copy;
	Copy.Kind = asEByteCodeOperandKind::Local;
	Copy.Immediate = static_cast<int64>(Local);
	const asEPrimitiveType Primitive = StmtPrimitive(Stmt->GetValue()->GetExpressionType());
	// A return ends this path; sibling branches still own the lexical locals.
	if (!asEmitDestroyFrom(Frame, 0, false, true)) return false;
	if (Frame.Builder->Emit(StmtCopyVtoR(Primitive), MakeArrayView(&Copy, 1)) != asEByteCodeImageStatus::Succeeded)
	{
		return false;
	}
	asSByteCodeOperand Ret;
	Ret.Kind = asEByteCodeOperandKind::ImmediateS16;
	Ret.Immediate = static_cast<int64>(Frame.ParameterDwords);
	return Frame.Builder->Emit(asBC_RET, MakeArrayView(&Ret, 1)) == asEByteCodeImageStatus::Succeeded;
}

bool asEmitStmt(asSByteCodeEmitFrame& Frame, asCStmt* Stmt)
{
	if (!Stmt) return false;
	if (auto* ClassDefault = dyn_cast<asCClassDefaultStmt>(Stmt))
	{
		return asEmitStmt(Frame, ClassDefault->GetStatement());
	}
	if (!asEmitNoteSource(Frame, Stmt)) return false;
	if (auto* Return = dyn_cast<asCReturnStmt>(Stmt))
	{
		return asEmitReturn(Frame, Return);
	}
	if (auto* DeclStmt = dyn_cast<asCDeclStmt>(Stmt))
	{
		asCVarDecl* Decl = DeclStmt->GetDeclaration();
		if (!Decl) return false;
		asCExpr* Initializer = DeclStmt->GetInitializer() ? DeclStmt->GetInitializer() : Decl->GetInitializer();
		if (auto* Construct = dyn_cast<asCConstructExpr>(Initializer))
		{
			if (asEmitCallableDecl(Frame, Decl->GetResolvedType(), Construct))
			{
				if (Construct->GetArguments().Num() == 1)
				{
					Initializer = Construct->GetArguments()[0];
				}
				else if (Construct->GetArguments().Num() == 0)
				{
					Initializer = nullptr;
				}
				else
				{
					return false;
				}
			}
			else
			{
				const int32 Local = static_cast<int32>(Frame.AllocLocal(2));
				Frame.Locals.Add(Decl, Local);
				if (!asEmitConstructInto(Frame, Construct, Local)) return false;
				if (asCObjectType* Type = asEmitFindObjectType(Frame, Construct->GetExpressionType()))
				{
					if (!asEmitNoteLiveObject(Frame, Local, Type)) return false;
				}
				return true;
			}
		}
		if (asEmitCallableDecl(Frame, Decl->GetResolvedType(), Initializer))
		{
			const int32 Local = static_cast<int32>(Frame.AllocLocal(AS_PTR_SIZE));
			Frame.Locals.Add(Decl, Local);
			asSByteCodeOperand Clear;
			Clear.Kind = asEByteCodeOperandKind::Local;
			Clear.Immediate = Local;
			if (Frame.Builder->Emit(asBC_ClrVPtr, MakeArrayView(&Clear, 1)) != asEByteCodeImageStatus::Succeeded) return false;
			if (!Initializer) return true;
			int32 Value = 0;
			if (!asEmitExpr(Frame, Initializer, Value)) return false;
			asSByteCodeOperand Ops[2];
			Ops[0].Kind = asEByteCodeOperandKind::Local;
			Ops[0].Immediate = static_cast<int64>(Local);
			Ops[1].Kind = asEByteCodeOperandKind::Local;
			Ops[1].Immediate = static_cast<int64>(Value);
			return Frame.Builder->Emit(asBC_ClonePtr, MakeArrayView(Ops, 2)) == asEByteCodeImageStatus::Succeeded;
		}
		if (asCObjectType* Type = asEmitFindObjectType(Frame, Decl->GetResolvedType()))
		{
			const int32 Local = static_cast<int32>(Frame.AllocLocal(2));
			Frame.Locals.Add(Decl, Local);
			asUINT TypeSlot = 0;
			if (Frame.Builder->AddTypeRequirement(*Type, TypeSlot) != asEByteCodeImageStatus::Succeeded) return false;
			asSByteCodeOperand Pointer;
			Pointer.Kind = asEByteCodeOperandKind::Local;
			Pointer.Immediate = Local;
			if (Frame.Builder->Emit(asBC_PSF, MakeArrayView(&Pointer, 1)) != asEByteCodeImageStatus::Succeeded) return false;
			asSByteCodeOperand AllocOps[2];
			AllocOps[0].Kind = asEByteCodeOperandKind::TypeSlot;
			AllocOps[0].Slot = TypeSlot;
			AllocOps[1].Kind = asEByteCodeOperandKind::ImmediateS32;
			AllocOps[1].Immediate = 0;
			if (Frame.Builder->Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2)) != asEByteCodeImageStatus::Succeeded) return false;
			if (!asEmitCallDefaultConstructor(Frame, Type, Local)) return false;
			if (!asEmitNoteLiveObject(Frame, Local, Type)) return false;
			return true;
		}
		const asEPrimitiveType Primitive = StmtPrimitive(Decl->GetResolvedType());
		const asUINT Local = Frame.AllocLocal(StmtDwordWidth(Primitive));
		Frame.Locals.Add(Decl, static_cast<int32>(Local));
		if (!Initializer) return true;
		int32 Value = 0;
		if (!asEmitExpr(Frame, Initializer, Value)) return false;
		asSByteCodeOperand Ops[2];
		Ops[0].Kind = asEByteCodeOperandKind::Local;
		Ops[0].Immediate = static_cast<int64>(Local);
		Ops[1].Kind = asEByteCodeOperandKind::Local;
		Ops[1].Immediate = static_cast<int64>(Value);
		return Frame.Builder->Emit(StmtCopyVtoV(Primitive), MakeArrayView(Ops, 2)) == asEByteCodeImageStatus::Succeeded;
	}
	if (auto* Compound = dyn_cast<asCCompoundStmt>(Stmt))
	{
		const int32 ScopeLive = Frame.LiveObjects.Num();
		for (asCStmt* Child : Compound->GetStatements())
		{
			if (!asEmitStmt(Frame, Child)) return false;
		}
		return asEmitDestroyFrom(Frame, ScopeLive);
	}
	if (auto* IfStmt = dyn_cast<asCIfStmt>(Stmt))
	{
		int32 Condition = 0;
		if (!asEmitExpr(Frame, IfStmt->GetCondition(), Condition)) return false;
		asSByteCodeOperand CondLocal;
		CondLocal.Kind = asEByteCodeOperandKind::Local;
		CondLocal.Immediate = Condition;
		if (Frame.Builder->Emit(asBC_CpyVtoR4, MakeArrayView(&CondLocal, 1)) != asEByteCodeImageStatus::Succeeded) return false;
		const asUINT ElseLabel = Frame.Builder->NewLabel();
		const asUINT EndLabel = Frame.Builder->NewLabel();
		asSByteCodeOperand JumpElse;
		JumpElse.Kind = asEByteCodeOperandKind::Label;
		JumpElse.Label = ElseLabel;
		if (Frame.Builder->Emit(asBC_JZ, MakeArrayView(&JumpElse, 1)) != asEByteCodeImageStatus::Succeeded) return false;
		if (IfStmt->GetThen() && !asEmitStmt(Frame, IfStmt->GetThen())) return false;
		asSByteCodeOperand JumpEnd;
		JumpEnd.Kind = asEByteCodeOperandKind::Label;
		JumpEnd.Label = EndLabel;
		if (Frame.Builder->Emit(asBC_JMP, MakeArrayView(&JumpEnd, 1)) != asEByteCodeImageStatus::Succeeded) return false;
		if (Frame.Builder->BindLabel(ElseLabel) != asEByteCodeImageStatus::Succeeded) return false;
		if (IfStmt->GetElse() && !asEmitStmt(Frame, IfStmt->GetElse())) return false;
		if (Frame.Builder->BindLabel(EndLabel) != asEByteCodeImageStatus::Succeeded) return false;
		return Frame.Builder->Emit(asBC_SUSPEND, TConstArrayView<asSByteCodeOperand>()) == asEByteCodeImageStatus::Succeeded;
	}
	if (auto* WhileStmt = dyn_cast<asCWhileStmt>(Stmt))
	{
		const asUINT CondLabel = Frame.Builder->NewLabel();
		const asUINT EndLabel = Frame.Builder->NewLabel();
		if (Frame.Builder->BindLabel(CondLabel) != asEByteCodeImageStatus::Succeeded) return false;
		int32 Condition = 0;
		if (!asEmitExpr(Frame, WhileStmt->GetCondition(), Condition)) return false;
		asSByteCodeOperand CondLocal;
		CondLocal.Kind = asEByteCodeOperandKind::Local;
		CondLocal.Immediate = Condition;
		if (Frame.Builder->Emit(asBC_CpyVtoR4, MakeArrayView(&CondLocal, 1)) != asEByteCodeImageStatus::Succeeded) return false;
		asSByteCodeOperand JumpEnd;
		JumpEnd.Kind = asEByteCodeOperandKind::Label;
		JumpEnd.Label = EndLabel;
		if (Frame.Builder->Emit(asBC_JZ, MakeArrayView(&JumpEnd, 1)) != asEByteCodeImageStatus::Succeeded) return false;
		const int32 BodyLive = Frame.LiveObjects.Num();
		PushBreak(Frame, EndLabel, BodyLive);
		PushContinue(Frame, CondLabel, BodyLive);
		if (WhileStmt->GetBody() && !asEmitStmt(Frame, WhileStmt->GetBody())) return false;
		if (!asEmitDestroyFrom(Frame, BodyLive)) return false;
		PopContinue(Frame);
		PopBreak(Frame);
		asSByteCodeOperand JumpCond;
		JumpCond.Kind = asEByteCodeOperandKind::Label;
		JumpCond.Label = CondLabel;
		if (Frame.Builder->Emit(asBC_JMP, MakeArrayView(&JumpCond, 1)) != asEByteCodeImageStatus::Succeeded) return false;
		if (Frame.Builder->BindLabel(EndLabel) != asEByteCodeImageStatus::Succeeded) return false;
		return Frame.Builder->Emit(asBC_SUSPEND, TConstArrayView<asSByteCodeOperand>()) == asEByteCodeImageStatus::Succeeded;
	}
	if (auto* DoStmt = dyn_cast<asCDoStmt>(Stmt))
	{
		const asUINT BodyLabel = Frame.Builder->NewLabel();
		const asUINT ContinueLabel = Frame.Builder->NewLabel();
		const asUINT BreakLabel = Frame.Builder->NewLabel();
		if (Frame.Builder->BindLabel(BodyLabel) != asEByteCodeImageStatus::Succeeded) return false;
		const int32 BodyLive = Frame.LiveObjects.Num();
		PushBreak(Frame, BreakLabel, BodyLive);
		PushContinue(Frame, ContinueLabel, BodyLive);
		if (DoStmt->GetBody() && !asEmitStmt(Frame, DoStmt->GetBody())) return false;
		if (!asEmitDestroyFrom(Frame, BodyLive)) return false;
		PopContinue(Frame);
		PopBreak(Frame);
		if (Frame.Builder->BindLabel(ContinueLabel) != asEByteCodeImageStatus::Succeeded) return false;
		int32 Condition = 0;
		if (!asEmitExpr(Frame, DoStmt->GetCondition(), Condition) || !EmitStmtCpyToR4(Frame, Condition)) return false;
		if (!EmitStmtJump(Frame, asBC_JNZ, BodyLabel)) return false;
		if (Frame.Builder->BindLabel(BreakLabel) != asEByteCodeImageStatus::Succeeded) return false;
		return Frame.Builder->Emit(asBC_SUSPEND, TConstArrayView<asSByteCodeOperand>()) == asEByteCodeImageStatus::Succeeded;
	}
	if (auto* ForStmt = dyn_cast<asCForStmt>(Stmt))
	{
		const int32 InitLive = Frame.LiveObjects.Num();
		for (asCStmt* Initializer : ForStmt->GetInitializers())
		{
			if (!asEmitStmt(Frame, Initializer)) return false;
		}
		const asUINT CondLabel = Frame.Builder->NewLabel();
		const asUINT ContinueLabel = Frame.Builder->NewLabel();
		const asUINT BreakLabel = Frame.Builder->NewLabel();
		if (Frame.Builder->BindLabel(CondLabel) != asEByteCodeImageStatus::Succeeded) return false;
		if (!EmitLoopCondition(Frame, ForStmt->GetCondition(), BreakLabel)) return false;
		const int32 BodyLive = Frame.LiveObjects.Num();
		PushBreak(Frame, BreakLabel, BodyLive);
		PushContinue(Frame, ContinueLabel, BodyLive);
		if (ForStmt->GetBody() && !asEmitStmt(Frame, ForStmt->GetBody())) return false;
		if (!asEmitDestroyFrom(Frame, BodyLive)) return false;
		PopContinue(Frame);
		PopBreak(Frame);
		if (Frame.Builder->BindLabel(ContinueLabel) != asEByteCodeImageStatus::Succeeded) return false;
		for (asCExpr* Increment : ForStmt->GetIncrements())
		{
			int32 Unused = 0;
			if (!asEmitExpr(Frame, Increment, Unused)) return false;
		}
		if (!EmitStmtJump(Frame, asBC_JMP, CondLabel)) return false;
		if (Frame.Builder->BindLabel(BreakLabel) != asEByteCodeImageStatus::Succeeded) return false;
		if (!asEmitDestroyFrom(Frame, InitLive)) return false;
		return Frame.Builder->Emit(asBC_SUSPEND, TConstArrayView<asSByteCodeOperand>()) == asEByteCodeImageStatus::Succeeded;
	}
	if (auto* SwitchStmt = dyn_cast<asCSwitchStmt>(Stmt))
	{
		int32 Selector = 0;
		if (!asEmitExpr(Frame, SwitchStmt->GetSelector(), Selector)) return false;
		const asUINT EndLabel = Frame.Builder->NewLabel();
		TArray<asUINT> ClauseLabels;
		asUINT DefaultLabel = EndLabel;
		for (asCSwitchClause* Clause : SwitchStmt->GetClauses())
		{
			const asUINT Label = Frame.Builder->NewLabel();
			ClauseLabels.Add(Label);
			if (dyn_cast<asCDefaultStmt>(Clause)) DefaultLabel = Label;
		}
		int32 CaseValue = static_cast<int32>(Frame.AllocLocal());
		for (int32 Index = 0; Index < SwitchStmt->GetClauses().Num(); ++Index)
		{
			if (auto* Case = dyn_cast<asCCaseStmt>(SwitchStmt->GetClauses()[Index]))
			{
				int32 Immediate = 0;
				if (Case->HasResolvedValue())
				{
					Immediate = static_cast<int32>(Case->GetResolvedValueBits());
				}
				else if (auto* Literal = dyn_cast<asCIntegerLiteral>(Case->GetLabel()))
				{
					Immediate = static_cast<int32>(Literal->GetValue());
				}
				asSByteCodeOperand SetOps[2];
				SetOps[0].Kind = asEByteCodeOperandKind::Local;
				SetOps[0].Immediate = CaseValue;
				SetOps[1].Kind = asEByteCodeOperandKind::ImmediateS32;
				SetOps[1].Immediate = Immediate;
				if (Frame.Builder->Emit(asBC_SetV4, MakeArrayView(SetOps, 2)) != asEByteCodeImageStatus::Succeeded) return false;
				asSByteCodeOperand Cmp[2];
				Cmp[0].Kind = asEByteCodeOperandKind::Local;
				Cmp[0].Immediate = Selector;
				Cmp[1].Kind = asEByteCodeOperandKind::Local;
				Cmp[1].Immediate = CaseValue;
				if (Frame.Builder->Emit(asBC_CMPi, MakeArrayView(Cmp, 2)) != asEByteCodeImageStatus::Succeeded) return false;
				if (Frame.Builder->Emit(asBC_TZ, TConstArrayView<asSByteCodeOperand>()) != asEByteCodeImageStatus::Succeeded) return false;
				if (!EmitStmtJump(Frame, asBC_JNZ, ClauseLabels[Index])) return false;
			}
		}
		if (!EmitStmtJump(Frame, asBC_JMP, DefaultLabel)) return false;
		const int32 SwitchLive = Frame.LiveObjects.Num();
		for (int32 Index = 0; Index < SwitchStmt->GetClauses().Num(); ++Index)
		{
			if (Frame.Builder->BindLabel(ClauseLabels[Index]) != asEByteCodeImageStatus::Succeeded) return false;
			PushBreak(Frame, EndLabel, SwitchLive);
			asCSwitchClause* Clause = SwitchStmt->GetClauses()[Index];
			bool bFallthrough = false;
			for (asCStmt* Child : Clause->GetStatements())
			{
				if (dyn_cast<asCFallthroughStmt>(Child))
				{
					bFallthrough = true;
					continue;
				}
				if (!asEmitStmt(Frame, Child)) return false;
			}
			if (!asEmitDestroyFrom(Frame, SwitchLive)) return false;
			PopBreak(Frame);
			if (!bFallthrough && !EmitStmtJump(Frame, asBC_JMP, EndLabel)) return false;
		}
		if (Frame.Builder->BindLabel(EndLabel) != asEByteCodeImageStatus::Succeeded) return false;
		return Frame.Builder->Emit(asBC_SUSPEND, TConstArrayView<asSByteCodeOperand>()) == asEByteCodeImageStatus::Succeeded;
	}
	if (auto* BreakStmt = dyn_cast<asCBreakStmt>(Stmt))
	{
		(void)BreakStmt;
		if (Frame.BreakLabels.Num() == 0 || Frame.BreakLiveStarts.Num() == 0) return false;
		if (!asEmitDestroyFrom(Frame, Frame.BreakLiveStarts.Last(), false, true)) return false;
		return EmitStmtJump(Frame, asBC_JMP, Frame.BreakLabels.Last());
	}
	if (auto* ContinueStmt = dyn_cast<asCContinueStmt>(Stmt))
	{
		(void)ContinueStmt;
		if (Frame.ContinueLabels.Num() == 0 || Frame.ContinueLiveStarts.Num() == 0) return false;
		if (!asEmitDestroyFrom(Frame, Frame.ContinueLiveStarts.Last(), false, true)) return false;
		return EmitStmtJump(Frame, asBC_JMP, Frame.ContinueLabels.Last());
	}
	if (dyn_cast<asCNullStmt>(Stmt) || dyn_cast<asCFallthroughStmt>(Stmt))
	{
		return true;
	}
	if (auto* Group = dyn_cast<asCDeclGroupStmt>(Stmt))
	{
		for (asCStmt* Child : Group->GetDeclarations())
		{
			if (!asEmitStmt(Frame, Child)) return false;
		}
		return true;
	}
	if (asCExpr* Expr = dyn_cast<asCExpr>(Stmt))
	{
		int32 Unused = 0;
		return asEmitExpr(Frame, Expr, Unused);
	}
	return false;
}

END_AS_NAMESPACE
