#include "as_bytecode_emitter.h"
#include "as_bytecode_emitter_internal.h"
#include "as_bytecode_image.h"
#include "as_bytecode_image_builder.h"
#include "as_module_definition_set.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/Sema/as_body_fragment.h"
#include "frontend/Compile/as_compilation_session.h"
#include "frontend/AST/as_decl.h"
#include "frontend/Basic/as_source_manager.h"
#include "frontend/AST/as_stmt.h"

BEGIN_AS_NAMESPACE

namespace
{
	constexpr asUINT DefaultFrameDWords = 64;
	constexpr asUINT DiagnosticNotReady = 5101;
	constexpr asUINT DiagnosticInvalidInput = 5102;
	constexpr asUINT DiagnosticUnsupported = 5103;
	constexpr asUINT DiagnosticResource = 5104;
	const TCHAR* FragmentKey = TEXT("bytecode-emitter");

	bool SessionHasRecovery(const asCCompilationSession& Session)
	{
		if (Session.HasRecoveryNodes())
		{
			return true;
		}
		if (const asCTranslationUnitDecl* Unit = Session.GetTranslationUnit())
		{
			for (asCDecl* Decl : Unit->GetDecls())
			{
				if (isa<asCRecoveryDecl>(Decl))
				{
					return true;
				}
			}
		}
		return false;
	}

	asUINT ParameterDwords(const asCScriptFunction& Function)
	{
		asUINT Size = 0;
		if (Function.objectType)
		{
			Size += AS_PTR_SIZE;
		}
		if (Function.DoesReturnOnStack())
		{
			Size += AS_PTR_SIZE;
		}
		for (asUINT Index = 0; Index < Function.parameterTypes.GetLength(); ++Index)
		{
			Size += Function.parameterTypes[Index].GetSizeOnStackDWords();
		}
		return Size;
	}

	void FillFailureLocation(asSByteCodeEmissionFailure& Failure, const asCCompilationSession& Session, const asCASTNode* Node)
	{
		if (!Node)
		{
			return;
		}
		Failure.NodeKind = Node->GetRawKind();
		asCSourceManager Manager(Session.GetASTContext().GetSources());
		asSSourceLineColumn LineColumn;
		if (Manager.TryGetLineColumn(Node->GetSourceRange().Begin, LineColumn))
		{
			Failure.Line = static_cast<int32>(LineColumn.Line);
			Failure.Column = static_cast<int32>(LineColumn.Column);
		}
		asSStableSourceAnchor Anchor;
		if (Manager.TryCreateStableAnchor(Node->GetSourceRange(), Anchor))
		{
			Failure.StableSourceKey = Anchor.LogicalSourceKey;
		}
	}

	void FailEmission(
		asSByteCodeEmissionResult& Result,
		const asCCompilationSession* Session,
		asEByteCodeEmissionStatus Status,
		const asCScriptFunction* Function,
		const asCASTNode* Node,
		FStringView Message)
	{
		Result.EmissionStatus = Status;
		Result.Failure.StableFragmentKey = FragmentKey;
		if (Function)
		{
			Result.Failure.FunctionKey = Function->GetStableKey();
		}
		if (Session)
		{
			FillFailureLocation(Result.Failure, *Session, Node);
			asCDiagnosticFragment Fragment = Session->GetDiagnosticsEngine().CreateFragment(FragmentKey);
			asSDiagnosticRecord Record;
			switch (Status)
			{
			case asEByteCodeEmissionStatus::NotReady: Record.DiagnosticID = DiagnosticNotReady; break;
			case asEByteCodeEmissionStatus::InvalidInput: Record.DiagnosticID = DiagnosticInvalidInput; break;
			case asEByteCodeEmissionStatus::UnsupportedLowering: Record.DiagnosticID = DiagnosticUnsupported; break;
			case asEByteCodeEmissionStatus::ResourceLimit: Record.DiagnosticID = DiagnosticResource; break;
			default: Record.DiagnosticID = DiagnosticInvalidInput; break;
			}
			Record.Severity = asEDiagnosticSeverity::Error;
			if (Node && Node->GetSourceRange().IsOrdered())
			{
				Record.PrimaryRange = Node->GetSourceRange();
			}
			Record.Arguments.Add(asSDiagnosticArgument::FromText(Message));
			Record.StableFragmentKey = FragmentKey;
			Result.Failure.Diagnostics.Add(Record);
			Fragment.Report(Record);
			Session->GetDiagnosticsEngine().Submit(MoveTemp(Fragment));
		}
	}
}

asSByteCodeEmissionResult asEmitByteCodeImage(
	const asCCompilationSession* Session,
	const asCModuleDefinitionSet* FrozenDefinitions,
	const asSByteCodeEmissionOptions& Options,
	TUniquePtr<asCByteCodeImage>& OutImage)
{
	asSByteCodeEmissionResult Result;
	if (!Session)
	{
		FailEmission(Result, nullptr, asEByteCodeEmissionStatus::InvalidInput, nullptr, nullptr, TEXT("null-session"));
		return Result;
	}
	if (SessionHasRecovery(*Session))
	{
		FailEmission(Result, Session, asEByteCodeEmissionStatus::InvalidInput, nullptr, Session->GetTranslationUnit(), TEXT("recovery-ast"));
		return Result;
	}
	if (!Session->IsASTVerified())
	{
		FailEmission(Result, Session, asEByteCodeEmissionStatus::NotReady, nullptr, Session->GetTranslationUnit(), TEXT("unverified-session"));
		return Result;
	}
	if (!FrozenDefinitions)
	{
		FailEmission(Result, Session, asEByteCodeEmissionStatus::InvalidInput, nullptr, Session->GetTranslationUnit(), TEXT("null-definitions"));
		return Result;
	}
	if (!FrozenDefinitions->IsFrozen())
	{
		FailEmission(Result, Session, asEByteCodeEmissionStatus::NotReady, nullptr, Session->GetTranslationUnit(), TEXT("unfrozen-definitions"));
		return Result;
	}
	if (FrozenDefinitions != Session->GetFrozenDefinitions())
	{
		FailEmission(Result, Session, asEByteCodeEmissionStatus::InvalidInput, nullptr, Session->GetTranslationUnit(), TEXT("foreign-definitions"));
		return Result;
	}
	if (Options.PointerByteSize != sizeof(void*)
		|| Options.PointerByteSize != FrozenDefinitions->GetOptions().PointerByteSize
		|| Options.bFloatIsFloat64 != FrozenDefinitions->GetOptions().FloatIsFloat64)
	{
		FailEmission(Result, Session, asEByteCodeEmissionStatus::UnsupportedLowering, nullptr, Session->GetTranslationUnit(), TEXT("unsupported-target"));
		return Result;
	}
	if (Options.MaxFrameDWords == 0 || Options.MaxFunctions == 0)
	{
		asCScriptFunction* First = nullptr;
		for (const asCBodyFragment& Fragment : Session->GetBodyFragments())
		{
			First = FrozenDefinitions->FindFunction(Fragment.GetWorkItem().StableFunctionKey);
			if (First)
			{
				break;
			}
		}
		FailEmission(Result, Session, asEByteCodeEmissionStatus::ResourceLimit, First, Session->GetTranslationUnit(), TEXT("resource-budget"));
		return Result;
	}

	asUINT BodyCount = 0;
	for (const asCBodyFragment& Fragment : Session->GetBodyFragments())
	{
		if (Fragment.GetBody() && FrozenDefinitions->FindFunction(Fragment.GetWorkItem().StableFunctionKey))
		{
			++BodyCount;
		}
	}
	if (BodyCount > Options.MaxFunctions)
	{
		FailEmission(Result, Session, asEByteCodeEmissionStatus::ResourceLimit, nullptr, Session->GetTranslationUnit(), TEXT("function-budget"));
		return Result;
	}

	asCByteCodeImageBuilder ImageBuilder;
	TMap<asSStableKey, asCScriptFunction*> FunctionsByKey;
	TMap<asSStableKey, asCObjectType*> TypesByKey;
	for (asCScriptFunction* Candidate : FrozenDefinitions->GetFunctions())
	{
		if (Candidate && Candidate->funcType == asFUNC_SCRIPT)
		{
			FunctionsByKey.Add(Candidate->GetStableKey(), Candidate);
		}
	}
	TArray<const asCModuleDefinitionSet*> TypeSets;
	TypeSets.Add(FrozenDefinitions);
	for (int32 SetIndex = 0; SetIndex < TypeSets.Num(); ++SetIndex)
	{
		const asCModuleDefinitionSet* TypeSet = TypeSets[SetIndex];
		if (!TypeSet)
		{
			continue;
		}
		for (asCTypeInfo* Type : TypeSet->GetTypes())
		{
			if (asCObjectType* Object = CastToObjectType(Type))
			{
				TypesByKey.Add(Object->GetStableKey(), Object);
			}
		}
		for (asCModuleDefinitionSet* Dependency : TypeSet->GetDependencies())
		{
			if (Dependency && !TypeSets.Contains(Dependency))
			{
				TypeSets.Add(Dependency);
			}
		}
	}
	TMap<asSStableKey, asCGlobalProperty*> GlobalsByKey;
	for (asCGlobalProperty* Global : FrozenDefinitions->GetGlobalProperties())
	{
		if (Global)
		{
			GlobalsByKey.Add(Global->GetStableKey(), Global);
		}
	}

	bool bEmittedAny = false;
	for (const asCBodyFragment& Fragment : Session->GetBodyFragments())
	{
		asCScriptFunction* Current = FunctionsByKey.FindRef(Fragment.GetWorkItem().StableFunctionKey);
		if (!Current)
		{
			continue;
		}
		const asCCompoundStmt* CurrentBody = Fragment.GetBody();
		if (!CurrentBody)
		{
			continue;
		}
		asUINT Slot = 0;
		if (ImageBuilder.AddFunctionRequirement(*Current, Slot) != asEByteCodeImageStatus::Succeeded)
		{
			FailEmission(Result, Session, asEByteCodeEmissionStatus::InvalidInput, Current, CurrentBody, TEXT("function-requirement"));
			return Result;
		}
		const asUINT ParamSize = ParameterDwords(*Current);
		const asUINT FrameBudget = FMath::Max(Options.MaxFrameDWords, DefaultFrameDWords);
		if (ImageBuilder.BeginFunction(Current->GetStableKey(), Current->GetStableKey(), FrameBudget, ParamSize) != asEByteCodeImageStatus::Succeeded)
		{
			FailEmission(Result, Session, asEByteCodeEmissionStatus::ResourceLimit, Current, CurrentBody, TEXT("begin-function"));
			return Result;
		}
		asSByteCodeEmitFrame Frame;
		Frame.Builder = &ImageBuilder;
		Frame.Session = Session;
		Frame.Function = Current;
		Frame.MaxFrameDWords = Options.MaxFrameDWords;
		Frame.ParameterDwords = ParamSize;
		Frame.Functions = FunctionsByKey;
		Frame.Types = TypesByKey;
		Frame.Globals = GlobalsByKey;
		for (asCStmt* Stmt : CurrentBody->GetStatements())
		{
			if (!asEmitStmt(Frame, Stmt))
			{
				FailEmission(Result, Session, asEByteCodeEmissionStatus::UnsupportedLowering, Current, Stmt, TEXT("unsupported-lowering"));
				return Result;
			}
		}
		if (!asEmitDestroyLive(Frame))
		{
			FailEmission(Result, Session, asEByteCodeEmissionStatus::UnsupportedLowering, Current, CurrentBody, TEXT("live-destroy"));
			return Result;
		}
		if (Frame.NextLocal > Options.MaxFrameDWords)
		{
			FailEmission(Result, Session, asEByteCodeEmissionStatus::ResourceLimit, Current, CurrentBody, TEXT("frame-budget"));
			return Result;
		}
		asSByteCodeFrameDescriptor Measured;
		Measured.LocalDwords = Frame.NextLocal;
		Measured.ParameterDwords = ParamSize;
		Measured.ResultDwords = static_cast<asUINT>(FMath::Max(0, Current->returnType.GetSizeOnStackDWords()));
		Measured.ResultOffset = static_cast<int32>(Measured.ResultDwords);
		Measured.InitializedObjectSlots = Frame.ConstructedSlots;
		if (ImageBuilder.SetFrameDescriptor(Measured) != asEByteCodeImageStatus::Succeeded)
		{
			FailEmission(Result, Session, asEByteCodeEmissionStatus::InvalidInput, Current, CurrentBody, TEXT("frame-descriptor"));
			return Result;
		}
		asSByteCodeOperand Ret;
		Ret.Kind = asEByteCodeOperandKind::ImmediateS16;
		Ret.Immediate = static_cast<int64>(ParamSize);
		if (ImageBuilder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded
			|| ImageBuilder.EndFunction() != asEByteCodeImageStatus::Succeeded)
		{
			FailEmission(Result, Session, asEByteCodeEmissionStatus::InvalidInput, Current, CurrentBody, TEXT("end-function"));
			return Result;
		}
		bEmittedAny = true;
	}
	if (!bEmittedAny)
	{
		Result.EmissionStatus = asEByteCodeEmissionStatus::Succeeded;
		return Result;
	}
	TUniquePtr<asCByteCodeImage> Image;
	if (ImageBuilder.Finish(Image) != asEByteCodeImageStatus::Succeeded)
	{
		FailEmission(Result, Session, asEByteCodeEmissionStatus::InvalidInput, nullptr, Session->GetTranslationUnit(), TEXT("finish-image"));
		return Result;
	}
	OutImage = MoveTemp(Image);
	Result.EmissionStatus = asEByteCodeEmissionStatus::Succeeded;
	return Result;
}

asSByteCodeEmissionResult asCByteCodeEmitter::Emit(
	const asCCompilationSession* Session,
	const asCModuleDefinitionSet* FrozenDefinitions,
	const asSByteCodeEmissionOptions& Options)
{
	TUniquePtr<asCByteCodeImage> Unused;
	return asEmitByteCodeImage(Session, FrozenDefinitions, Options, Unused);
}

asSByteCodeEmissionResult asCByteCodeEmitter::Emit(asCBuilder& Builder)
{
	TUniquePtr<asCByteCodeImage> Image;
	asSByteCodeEmissionResult Result = asEmitByteCodeImage(
		Builder.GetCompilationSession(),
		Builder.GetCompilationSession() ? Builder.GetCompilationSession()->GetFrozenDefinitions() : nullptr,
		asSByteCodeEmissionOptions(),
		Image);
	if (Result.EmissionStatus != asEByteCodeEmissionStatus::Succeeded) return Result;
	asCModuleDefinitionSet* Set = Builder.GetModuleDefinitionSet();
	if (!Set || !Image) return Result;
	for (const asSByteCodeFunctionBody& Body : Image->GetFunctions())
	{
		asCScriptFunction* Function = Set->FindFunction(Body.FunctionKey);
		if (!Function) continue;
		TArray<asDWORD> Words;
		Words.Reserve(Body.Instructions.Num());
		for (const asSByteCodeInstruction& Instruction : Body.Instructions)
		{
			Words.Add(static_cast<asDWORD>(Instruction.Opcode));
		}
		Function->SetStableByteCode(MoveTemp(Words));
	}
	Set->AdoptEmittedByteCode(MoveTemp(Image));
	return Result;
}

asSByteCodeEmissionResult asCByteCodeEmitter::Emit(asCBuilder& Builder, asCScriptFunction& Function)
{
	(void)Function;
	return Emit(Builder.GetCompilationSession(), Builder.GetModuleDefinitionSet(), asSByteCodeEmissionOptions());
}

END_AS_NAMESPACE
