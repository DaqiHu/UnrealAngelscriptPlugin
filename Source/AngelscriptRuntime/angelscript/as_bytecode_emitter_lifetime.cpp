#include "as_bytecode_emitter_internal.h"
#include "as_property.h"
#include "frontend/as_compilation_session.h"
#include "frontend/as_diagnostics.h"
#include "frontend/as_source_snapshot.h"
#include "frontend/as_type.h"

BEGIN_AS_NAMESPACE

namespace
{
	bool EmitLocalOp(asSByteCodeEmitFrame& Frame, asEBCInstr Opcode, int32 Local)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::Local;
		Operand.Immediate = Local;
		return Frame.Builder->Emit(Opcode, MakeArrayView(&Operand, 1)) == asEByteCodeImageStatus::Succeeded;
	}

	asCScriptFunction* FindDefaultConstructor(asCObjectType* Type)
	{
		if (!Type) return nullptr;
		for (asUINT Index = 0; Index < Type->metadataBehaviours.GetLength(); ++Index)
		{
			const asCObjectType::asSMetadataBehaviour& Behaviour = Type->metadataBehaviours[Index];
			if (Behaviour.kind == asBEHAVE_CONSTRUCT && Behaviour.function && Behaviour.function->GetParamCount() == 0)
			{
				return Behaviour.function;
			}
		}
		return nullptr;
	}
}

asCObjectType* asEmitFindObjectType(asSByteCodeEmitFrame& Frame, asCType* Type)
{
	const auto* Nominal = dyn_cast<asCNominalType>(Type);
	if (!Nominal) return nullptr;
	return Frame.Types.FindRef(Nominal->GetDeclarationIdentity());
}

bool asEmitDestroyFrom(asSByteCodeEmitFrame& Frame, int32 FirstLive, bool bPopLive, bool bRecordCleanup)
{
	if (!Frame.Builder || FirstLive < 0) return false;
	if (FirstLive > Frame.LiveObjects.Num()) return true;
	for (int32 Index = Frame.LiveObjects.Num() - 1; Index >= FirstLive; --Index)
	{
		const int32 Local = Frame.LiveObjects[Index].Key;
		asCObjectType* Type = Frame.LiveObjects[Index].Value;
		if (!Type) return false;
		asUINT TypeSlot = 0;
		if (Frame.Builder->AddTypeRequirement(*Type, TypeSlot) != asEByteCodeImageStatus::Succeeded) return false;
		asSByteCodeOperand FreeOps[2];
		FreeOps[0].Kind = asEByteCodeOperandKind::Local;
		FreeOps[0].Immediate = Local;
		FreeOps[1].Kind = asEByteCodeOperandKind::TypeSlot;
		FreeOps[1].Slot = TypeSlot;
		if (Frame.Builder->Emit(asBC_FREE, MakeArrayView(FreeOps, 2)) != asEByteCodeImageStatus::Succeeded) return false;
		if (bRecordCleanup)
		{
			asSByteCodeCleanupAction Uninit;
			Uninit.Kind = asEByteCodeCleanupKind::DestroyObject;
			Uninit.Path = asEByteCodeCleanupPath::Normal;
			Uninit.FrameOffset = Local;
			Uninit.TypeSlot = TypeSlot;
			Uninit.InstructionIndex = Frame.Builder->GetInstructionCount();
			if (Uninit.InstructionIndex > 0) --Uninit.InstructionIndex;
			if (Frame.Builder->AddCleanupAction(Uninit) != asEByteCodeImageStatus::Succeeded) return false;
		}
	}
	if (bPopLive)
	{
		Frame.LiveObjects.SetNum(FirstLive);
	}
	return true;
}

bool asEmitNoteLiveObject(asSByteCodeEmitFrame& Frame, int32 Local, asCObjectType* Type)
{
	if (!Frame.Builder || !Type) return false;
	asUINT TypeSlot = 0;
	if (Frame.Builder->AddTypeRequirement(*Type, TypeSlot) != asEByteCodeImageStatus::Succeeded) return false;
	Frame.LiveObjects.Add(TPair<int32, asCObjectType*>(Local, Type));
	Frame.ConstructedSlots.AddUnique(Local);
	asSByteCodeCleanupAction Init;
	Init.Kind = asEByteCodeCleanupKind::DestroyObject;
	Init.Path = asEByteCodeCleanupPath::Exception;
	Init.FrameOffset = Local;
	Init.TypeSlot = TypeSlot;
	Init.InstructionIndex = Frame.Builder->GetInstructionCount();
	if (Init.InstructionIndex > 0) --Init.InstructionIndex;
	return Frame.Builder->AddCleanupAction(Init) == asEByteCodeImageStatus::Succeeded;
}

bool asEmitNoteSource(asSByteCodeEmitFrame& Frame, const asCStmt* Stmt)
{
	if (!Frame.Builder || !Stmt) return false;
	asSByteCodeSourceRecord Record;
	Record.InstructionIndex = Frame.Builder->GetInstructionCount();
	if (Record.InstructionIndex > 0) --Record.InstructionIndex;
	Record.Line = 1;
	Record.Column = 1;
	if (Frame.Session)
	{
		const asCSourceSnapshot& Snapshot = Frame.Session->GetDiagnosticsEngine().GetSnapshot();
		asSSourceLineColumn LineColumn;
		if (Snapshot.TryGetLineColumn(Stmt->GetSourceRange().Begin, LineColumn))
		{
			Record.Line = static_cast<int32>(LineColumn.Line);
			Record.Column = static_cast<int32>(LineColumn.Column);
		}
		FStringView FileKey;
		if (Snapshot.TryGetLogicalSourceKey(Stmt->GetSourceRange().Begin.FileID, FileKey))
		{
			asUINT StringSlot = 0;
			if (Frame.Builder->AddString(FString(FileKey), StringSlot) != asEByteCodeImageStatus::Succeeded) return false;
			Record.FileStringSlot = StringSlot;
		}
	}
	return Frame.Builder->AddSourceRecord(Record) == asEByteCodeImageStatus::Succeeded;
}

bool asEmitCallDefaultConstructor(asSByteCodeEmitFrame& Frame, asCObjectType* Type, int32 DestLocal)
{
	if (!Frame.Builder || !Type) return false;
	asCScriptFunction* Constructor = FindDefaultConstructor(Type);
	if (!Constructor) return true;
	asUINT CtorSlot = 0;
	if (Frame.Builder->AddFunctionRequirement(*Constructor, CtorSlot) != asEByteCodeImageStatus::Succeeded) return false;
	asSByteCodeOperand ThisOp;
	ThisOp.Kind = asEByteCodeOperandKind::Local;
	ThisOp.Immediate = DestLocal;
	if (Frame.Builder->Emit(asBC_PshVPtr, MakeArrayView(&ThisOp, 1)) != asEByteCodeImageStatus::Succeeded) return false;
	asSByteCodeOperand CallOp;
	CallOp.Kind = asEByteCodeOperandKind::FunctionSlot;
	CallOp.Slot = CtorSlot;
	return Frame.Builder->Emit(asBC_CALL, MakeArrayView(&CallOp, 1)) == asEByteCodeImageStatus::Succeeded;
}

bool asEmitDestroyLive(asSByteCodeEmitFrame& Frame)
{
	return asEmitDestroyFrom(Frame, 0);
}

bool asEmitConstructInto(asSByteCodeEmitFrame& Frame, asCConstructExpr* Construct, int32 DestLocal)
{
	if (!Frame.Builder || !Construct) return false;
	asCObjectType* Type = asEmitFindObjectType(Frame, Construct->GetExpressionType());
	if (!Type) return false;
	asUINT TypeSlot = 0;
	if (Frame.Builder->AddTypeRequirement(*Type, TypeSlot) != asEByteCodeImageStatus::Succeeded) return false;
	if (!EmitLocalOp(Frame, asBC_PSF, DestLocal)) return false;
	asSByteCodeOperand AllocOps[2];
	AllocOps[0].Kind = asEByteCodeOperandKind::TypeSlot;
	AllocOps[0].Slot = TypeSlot;
	AllocOps[1].Kind = asEByteCodeOperandKind::ImmediateS32;
	AllocOps[1].Immediate = 0;
	if (Frame.Builder->Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2)) != asEByteCodeImageStatus::Succeeded) return false;
	if (asCFunctionDecl* ConstructorDecl = Construct->GetConstructor())
	{
		asCScriptFunction* Constructor = Frame.Functions.FindRef(ConstructorDecl->GetStableIdentity());
		if (!Constructor) return false;
		asUINT CtorSlot = 0;
		if (Frame.Builder->AddFunctionRequirement(*Constructor, CtorSlot) != asEByteCodeImageStatus::Succeeded) return false;
		const TConstArrayView<asCExpr*> Arguments = Construct->GetArguments();
		TArray<int32> ArgLocals;
		ArgLocals.SetNum(Arguments.Num());
		for (int32 Index = Arguments.Num() - 1; Index >= 0; --Index)
		{
			if (!asEmitExpr(Frame, Arguments[Index], ArgLocals[Index])) return false;
		}
		for (int32 Index = Arguments.Num() - 1; Index >= 0; --Index)
		{
			if (!EmitLocalOp(Frame, asBC_PshV4, ArgLocals[Index])) return false;
		}
		if (!EmitLocalOp(Frame, asBC_PshVPtr, DestLocal)) return false;
		asSByteCodeOperand CallOp;
		CallOp.Kind = asEByteCodeOperandKind::FunctionSlot;
		CallOp.Slot = CtorSlot;
		if (Frame.Builder->Emit(asBC_CALL, MakeArrayView(&CallOp, 1)) != asEByteCodeImageStatus::Succeeded) return false;
	}
	else if (Construct->GetArguments().Num() == 1)
	{
		int32 Source = 0;
		if (!asEmitExpr(Frame, Construct->GetArguments()[0], Source)) return false;
		if (!EmitLocalOp(Frame, asBC_PshVPtr, Source)) return false;
		if (!EmitLocalOp(Frame, asBC_PshVPtr, DestLocal)) return false;
		asSByteCodeOperand Copy;
		Copy.Kind = asEByteCodeOperandKind::ImmediateS16;
		Copy.Immediate = static_cast<int16>(Type->size);
		if (Frame.Builder->Emit(asBC_COPY, MakeArrayView(&Copy, 1)) != asEByteCodeImageStatus::Succeeded) return false;
	}
	return true;
}

END_AS_NAMESPACE
