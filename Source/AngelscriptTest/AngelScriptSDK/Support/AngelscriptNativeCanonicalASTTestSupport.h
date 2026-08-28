#pragma once

#include "Support/AngelscriptNativeCoreTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_context.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_traversal.h"
#include "source/as_ast_verifier.h"
#include "source/as_scriptfunction.h"
#include "source/as_source_manager.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptNativeTestSupport
{
	struct FCanonicalASTMatch
	{
		bool bMatched = false;
		asUINT MatchCount = 0;
		asSASTNodeRef Node;
		asSASTNodeRef Related;
		FString FailureMessage;

		bool IsMatch() const
		{
			return bMatched;
		}

		static FCanonicalASTMatch Success(
			const asSASTNodeRef& InNode,
			const asSASTNodeRef& InRelated = asSASTNodeRef(),
			asUINT InMatchCount = 1)
		{
			FCanonicalASTMatch Result;
			Result.bMatched = true;
			Result.MatchCount = InMatchCount;
			Result.Node = InNode;
			Result.Related = InRelated;
			return Result;
		}

		static FCanonicalASTMatch Failure(FString InMessage)
		{
			FCanonicalASTMatch Result;
			Result.FailureMessage = MoveTemp(InMessage);
			return Result;
		}
	};

	inline FString CanonicalASTNodeLabel(const asSASTNodeRef& Node)
	{
		const TCHAR Prefix = Node.nodeClass == asAST_NODE_DECL ? TEXT('D')
			: Node.nodeClass == asAST_NODE_TYPE ? TEXT('T')
			: Node.nodeClass == asAST_NODE_STMT ? TEXT('S')
			: Node.nodeClass == asAST_NODE_EXPR ? TEXT('E') : TEXT('I');
		return FString::Printf(TEXT("%c%u"), Prefix, Node.value);
	}

	inline bool CanonicalASTGetQualType(
		const asCASTContext& Context,
		const asSASTNodeRef& Node,
		asCQualType& OutType)
	{
		switch (Node.nodeClass)
		{
		case asAST_NODE_DECL:
			if (const asCDecl* const Decl = Context.GetDecl(asASTDeclId(Node.value, Node.snapshotOwner)))
			{
				OutType = Decl->type;
				return true;
			}
			break;
		case asAST_NODE_TYPE:
			if (Context.GetType(asASTTypeRef(Node.value, Node.snapshotOwner)) != nullptr)
			{
				OutType = asCQualType(asASTTypeRef(Node.value, Node.snapshotOwner), 0);
				return true;
			}
			break;
		case asAST_NODE_EXPR:
			if (const asCExpr* const Expr = Context.GetExpr(asASTExprId(Node.value, Node.snapshotOwner)))
			{
				OutType = Expr->type;
				return true;
			}
			break;
		default:
			break;
		}
		return false;
	}

	inline const char* CanonicalASTTypeKey(const asCASTContext& Context, const asCQualType& Type)
	{
		const asCType* const CanonicalType = Context.GetType(Type.type);
		return CanonicalType != nullptr && CanonicalType->stableKey.AddressOf() != nullptr
			? CanonicalType->stableKey.AddressOf()
			: "";
	}

	inline FCanonicalASTMatch CanonicalASTUniqueDecl(
		const asCASTContext& Context,
		const char* StableKey)
	{
		asASTDeclId Match;
		asUINT Count = 0;
		for (asUINT Index = 1; Index <= Context.GetDeclCount(); ++Index)
		{
			const asCDecl* const Decl = Context.GetDecl(asASTDeclId(Index));
			if (Decl != nullptr && Decl->stableKey.Equals(StableKey != nullptr ? StableKey : ""))
			{
				Match = Decl->id;
				++Count;
			}
		}
		if (Count != 1)
		{
			return FCanonicalASTMatch::Failure(FString::Printf(
				TEXT("unique-decl expected exactly one stableKey=\"%s\" actual=%u"),
				UTF8_TO_TCHAR(StableKey != nullptr ? StableKey : ""),
				Count));
		}
		return FCanonicalASTMatch::Success(asSASTNodeRef::FromDecl(Match));
	}

	inline FCanonicalASTMatch CanonicalASTExactType(
		const asCASTContext& Context,
		const asSASTNodeRef& Node,
		const char* StableKey,
		asDWORD Qualifiers)
	{
		asCQualType QualType;
		if (!CanonicalASTGetQualType(Context, Node, QualType))
		{
			return FCanonicalASTMatch::Failure(FString::Printf(
				TEXT("exact-type node=%s has no typed AST fact"), *CanonicalASTNodeLabel(Node)));
		}
		const char* const ActualKey = CanonicalASTTypeKey(Context, QualType);
		if (FCStringAnsi::Strcmp(ActualKey, StableKey != nullptr ? StableKey : "") != 0
			|| QualType.quals != Qualifiers)
		{
			return FCanonicalASTMatch::Failure(FString::Printf(
				TEXT("exact-type node=%s expected key=\"%s\" quals=%u actual key=\"%s\" quals=%u"),
				*CanonicalASTNodeLabel(Node),
				UTF8_TO_TCHAR(StableKey != nullptr ? StableKey : ""),
				Qualifiers,
				UTF8_TO_TCHAR(ActualKey),
				QualType.quals));
		}
		return FCanonicalASTMatch::Success(Node, asSASTNodeRef::FromType(QualType.type));
	}

	inline FCanonicalASTMatch CanonicalASTNamedEdge(
		const asCASTContext& Context,
		const asSASTNodeRef& Node,
		asEASTEdgeRole Role,
		asUINT Occurrence)
	{
		asUINT MatchingIndex = 0;
		const asUINT EdgeCount = asCASTGetEdgeCount(Context, Node);
		for (asUINT EdgeIndex = 0; EdgeIndex < EdgeCount; ++EdgeIndex)
		{
			asSASTTraversalEdge Edge;
			if (asCASTGetEdge(Context, Node, EdgeIndex, Edge) && Edge.role == Role)
			{
				if (MatchingIndex == Occurrence)
				{
					return FCanonicalASTMatch::Success(Node, Edge.target);
				}
				++MatchingIndex;
			}
		}
		return FCanonicalASTMatch::Failure(FString::Printf(
			TEXT("named-edge node=%s role=%d occurrence=%u missing; available=%u"),
			*CanonicalASTNodeLabel(Node), static_cast<int32>(Role), Occurrence, MatchingIndex));
	}

	inline FCanonicalASTMatch CanonicalASTResolvedCallee(
		const asCASTContext& Context,
		const char* StableKey)
	{
		asASTExprId Match;
		asASTDeclId Callee;
		asUINT Count = 0;
		for (asUINT Index = 1; Index <= Context.GetExprCount(); ++Index)
		{
			const asCExpr* const Expr = Context.GetExpr(asASTExprId(Index));
			if (Expr == nullptr || (Expr->kind != asAST_EXPR_CALL
				&& Expr->kind != asAST_EXPR_CONSTRUCT
				&& Expr->kind != asAST_EXPR_INDEX))
			{
				continue;
			}
			const asCDecl* const Decl = Context.GetDecl(Expr->resolvedDecl);
			if (Decl != nullptr && Decl->stableKey.Equals(StableKey != nullptr ? StableKey : ""))
			{
				Match = Expr->id;
				Callee = Decl->id;
				++Count;
			}
		}
		if (Count != 1)
		{
			return FCanonicalASTMatch::Failure(FString::Printf(
				TEXT("resolved-callee expected exactly one stableKey=\"%s\" actual=%u"),
				UTF8_TO_TCHAR(StableKey != nullptr ? StableKey : ""), Count));
		}
		return FCanonicalASTMatch::Success(
			asSASTNodeRef::FromExpr(Match), asSASTNodeRef::FromDecl(Callee));
	}

	inline FCanonicalASTMatch CanonicalASTNoResolvedCallee(
		const asCASTContext& Context,
		const char* StableKey)
	{
		asUINT Count = 0;
		for (asUINT Index = 1; Index <= Context.GetExprCount(); ++Index)
		{
			const asCExpr* const Expr = Context.GetExpr(asASTExprId(Index));
			if (Expr == nullptr || (Expr->kind != asAST_EXPR_CALL && Expr->kind != asAST_EXPR_CONSTRUCT))
			{
				continue;
			}
			const asCDecl* const Decl = Context.GetDecl(Expr->resolvedDecl);
			if (Decl != nullptr && Decl->stableKey.Equals(StableKey != nullptr ? StableKey : ""))
			{
				++Count;
			}
		}
		if (Count != 0)
		{
			return FCanonicalASTMatch::Failure(FString::Printf(
				TEXT("no-resolved-callee expected zero stableKey=\"%s\" actual=%u"),
				UTF8_TO_TCHAR(StableKey != nullptr ? StableKey : ""), Count));
		}
		return FCanonicalASTMatch::Success(asSASTNodeRef::FromDecl(Context.GetTranslationUnit()));
	}

	inline FCanonicalASTMatch CanonicalASTReceiver(
		const asCASTContext& Context,
		asASTExprId Call,
		asEASTExprKind ExpectedKind)
	{
		const asCExpr* const CallExpr = Context.GetExpr(Call);
		const asCExpr* const Receiver = CallExpr != nullptr ? Context.GetExpr(CallExpr->receiver) : nullptr;
		if (CallExpr == nullptr || CallExpr->kind != asAST_EXPR_CALL || Receiver == nullptr
			|| Receiver->kind != ExpectedKind)
		{
			return FCanonicalASTMatch::Failure(FString::Printf(
				TEXT("receiver call=E%u expected kind=%d actual=%d"), Call.value,
				static_cast<int32>(ExpectedKind), Receiver != nullptr ? static_cast<int32>(Receiver->kind) : -1));
		}
		return FCanonicalASTMatch::Success(
			asSASTNodeRef::FromExpr(CallExpr->id), asSASTNodeRef::FromExpr(Receiver->id));
	}

	inline FCanonicalASTMatch CanonicalASTConversion(
		const asCASTContext& Context,
		const char* DestinationTypeKey,
		const char* SourceTypeKey)
	{
		asASTExprId Match;
		asASTExprId Source;
		asUINT Count = 0;
		for (asUINT Index = 1; Index <= Context.GetExprCount(); ++Index)
		{
			const asCExpr* const Expr = Context.GetExpr(asASTExprId(Index));
			if (Expr == nullptr || Expr->kind != asAST_EXPR_CONVERSION || Expr->children.GetLength() != 1)
			{
				continue;
			}
			const asCExpr* const Child = Context.GetExpr(Expr->children[0]);
			if (Child != nullptr
				&& FCStringAnsi::Strcmp(CanonicalASTTypeKey(Context, Expr->type), DestinationTypeKey != nullptr ? DestinationTypeKey : "") == 0
				&& FCStringAnsi::Strcmp(CanonicalASTTypeKey(Context, Child->type), SourceTypeKey != nullptr ? SourceTypeKey : "") == 0)
			{
				Match = Expr->id;
				Source = Child->id;
				++Count;
			}
		}
		if (Count != 1)
		{
			return FCanonicalASTMatch::Failure(FString::Printf(
				TEXT("conversion expected exactly one dest=\"%s\" src=\"%s\" actual=%u"),
				UTF8_TO_TCHAR(DestinationTypeKey != nullptr ? DestinationTypeKey : ""),
				UTF8_TO_TCHAR(SourceTypeKey != nullptr ? SourceTypeKey : ""), Count));
		}
		return FCanonicalASTMatch::Success(
			asSASTNodeRef::FromExpr(Match), asSASTNodeRef::FromExpr(Source));
	}

	inline FCanonicalASTMatch CanonicalASTControlTarget(
		const asCASTContext& Context,
		asEASTStmtKind TransferKind,
		asEASTStmtKind TargetKind)
	{
		asASTStmtId Match;
		asASTStmtId Target;
		asUINT Count = 0;
		for (asUINT Index = 1; Index <= Context.GetStmtCount(); ++Index)
		{
			const asCStmt* const Stmt = Context.GetStmt(asASTStmtId(Index));
			const asCStmt* const TargetStmt = Stmt != nullptr ? Context.GetStmt(Stmt->target) : nullptr;
			if (Stmt != nullptr && Stmt->kind == TransferKind
				&& TargetStmt != nullptr && TargetStmt->kind == TargetKind)
			{
				Match = Stmt->id;
				Target = TargetStmt->id;
				++Count;
			}
		}
		if (Count != 1)
		{
			return FCanonicalASTMatch::Failure(FString::Printf(
				TEXT("control-target expected exactly one transferKind=%d targetKind=%d actual=%u"),
				static_cast<int32>(TransferKind), static_cast<int32>(TargetKind), Count));
		}
		return FCanonicalASTMatch::Success(
			asSASTNodeRef::FromStmt(Match), asSASTNodeRef::FromStmt(Target));
	}

	inline FCanonicalASTMatch CanonicalASTCleanup(
		const asCASTContext& Context,
		const char* DestructorStableKey)
	{
		asASTExprId FirstMatch;
		asASTDeclId FirstDestructor;
		asUINT Count = 0;
		for (asUINT Index = 1; Index <= Context.GetExprCount(); ++Index)
		{
			const asCExpr* const Expr = Context.GetExpr(asASTExprId(Index));
			const asCDecl* const Decl = Expr != nullptr ? Context.GetDecl(Expr->resolvedDecl) : nullptr;
			if (Expr != nullptr && Expr->kind == asAST_EXPR_CLEANUP && Expr->children.GetLength() == 1
				&& Decl != nullptr && Decl->stableKey.Equals(DestructorStableKey != nullptr ? DestructorStableKey : ""))
			{
				if (Count == 0)
				{
					FirstMatch = Expr->id;
					FirstDestructor = Decl->id;
				}
				++Count;
			}
		}
		if (Count == 0)
		{
			return FCanonicalASTMatch::Failure(FString::Printf(
				TEXT("cleanup expected at least one destructor=\"%s\" actual=%u"),
				UTF8_TO_TCHAR(DestructorStableKey != nullptr ? DestructorStableKey : ""), Count));
		}
		return FCanonicalASTMatch::Success(
			asSASTNodeRef::FromExpr(FirstMatch), asSASTNodeRef::FromDecl(FirstDestructor), Count);
	}

	inline FCanonicalASTMatch CanonicalASTSourceRange(
		const asCASTContext& Context,
		const asSASTNodeRef& Node,
		const char* LogicalSource,
		asUINT BeginOffset,
		asUINT EndOffset)
	{
		asCSourceRange Range;
		switch (Node.nodeClass)
		{
		case asAST_NODE_DECL:
			if (const asCDecl* const Decl = Context.GetDecl(asASTDeclId(Node.value, Node.snapshotOwner))) Range = Decl->range;
			break;
		case asAST_NODE_STMT:
			if (const asCStmt* const Stmt = Context.GetStmt(asASTStmtId(Node.value, Node.snapshotOwner))) Range = Stmt->range;
			break;
		case asAST_NODE_EXPR:
			if (const asCExpr* const Expr = Context.GetExpr(asASTExprId(Node.value, Node.snapshotOwner))) Range = Expr->range;
			break;
		default:
			break;
		}
		const asCSourceManager& SourceManager = Context.GetSourceManager();
		const char* const ActualLogical = Range.begin.IsValid()
			? SourceManager.GetLogicalKey(Range.begin.fileID) : "";
		if (!SourceManager.IsValidRange(Range)
			|| FCStringAnsi::Strcmp(ActualLogical != nullptr ? ActualLogical : "", LogicalSource != nullptr ? LogicalSource : "") != 0
			|| Range.begin.offset != BeginOffset || Range.end.offset != EndOffset)
		{
			return FCanonicalASTMatch::Failure(FString::Printf(
				TEXT("source-range node=%s expected %s:%u-%u actual %s:%u-%u"),
				*CanonicalASTNodeLabel(Node), UTF8_TO_TCHAR(LogicalSource != nullptr ? LogicalSource : ""),
				BeginOffset, EndOffset, UTF8_TO_TCHAR(ActualLogical != nullptr ? ActualLogical : ""),
				Range.begin.offset, Range.end.offset));
		}
		return FCanonicalASTMatch::Success(Node);
	}

	inline FString CanonicalAstDumpToFString(const asCString& Dump)
	{
		return UTF8_TO_TCHAR(Dump.AddressOf() != nullptr ? Dump.AddressOf() : "");
	}

	inline bool CanonicalAstDumpsEqual(const asCASTContext& Left, const asCASTContext& Right)
	{
		asCString LeftDump;
		asCString RightDump;
		asCASTDump(Left, LeftDump);
		asCASTDump(Right, RightDump);
		return LeftDump.Equals(RightDump);
	}

	inline FString CanonicalStripVolatileTokens(FString Text)
	{
		Text.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		Text.ReplaceInline(TEXT("\r"), TEXT("\n"));
		int32 SearchFrom = 0;
		while (true)
		{
			const int32 AddressIndex = Text.Find(
				TEXT("0x"),
				ESearchCase::IgnoreCase,
				ESearchDir::FromStart,
				SearchFrom);
			if (AddressIndex == INDEX_NONE)
			{
				break;
			}
			int32 End = AddressIndex + 2;
			while (End < Text.Len() && FChar::IsHexDigit(Text[End]))
			{
				++End;
			}
			Text.RemoveAt(AddressIndex, End - AddressIndex);
			SearchFrom = AddressIndex;
		}
		Text.TrimStartAndEndInline();
		return Text;
	}

	inline FString CanonicalNormalizeDiagnostics(const FNativeMessageCollector& Collector)
	{
		TArray<FString> Lines;
		for (const FNativeMessageEntry& Entry : Collector.Entries)
		{
			FString Section = Entry.Section;
			Section.ReplaceInline(TEXT("\\"), TEXT("/"));
			const int32 Slash = Section.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (Slash != INDEX_NONE)
			{
				Section = Section.Mid(Slash + 1);
			}
			if (Section.IsEmpty())
			{
				Section = TEXT("<memory>");
			}
			Lines.Add(FString::Printf(
				TEXT("%s|%s|%d|%d|%s"),
				ToMessageTypeString(Entry.Type),
				*Section,
				Entry.Row,
				Entry.Column,
				*Entry.Message));
		}
		return CanonicalStripVolatileTokens(FString::Join(Lines, TEXT("\n")));
	}

	inline FString CanonicalNormalizeAstDump(const asCASTContext& Context)
	{
		asCString Dump;
		asCASTDump(Context, Dump);
		return CanonicalStripVolatileTokens(CanonicalAstDumpToFString(Dump));
	}

	inline FString CanonicalNormalizeDebugPositions(asIScriptFunction* Function)
	{
		if (Function == nullptr)
		{
			return TEXT("<null>");
		}
		TArray<FString> Lines;
		const char* Section = Function->GetScriptSectionName();
		Lines.Add(FString::Printf(
			TEXT("section=%s decl=%s"),
			Section != nullptr ? UTF8_TO_TCHAR(Section) : TEXT("<none>"),
			UTF8_TO_TCHAR(Function->GetDeclaration(true, true, true))));
		int Line = Function->FindNextLineWithCode(0);
		int Guard = 0;
		while (Line > 0 && Guard++ < 256)
		{
			Lines.Add(FString::Printf(TEXT("line=%d"), Line));
			const int Next = Function->FindNextLineWithCode(Line + 1);
			if (Next <= Line)
			{
				break;
			}
			Line = Next;
		}
		return CanonicalStripVolatileTokens(FString::Join(Lines, TEXT("\n")));
	}

	struct FCanonicalExecutionTrace
	{
		TArray<int32> Events;

		void Reset()
		{
			Events.Reset();
		}

		FString ToNormalizedString() const
		{
			TArray<FString> Tokens;
			Tokens.Reserve(Events.Num());
			for (const int32 Event : Events)
			{
				Tokens.Add(FString::FromInt(Event));
			}
			return FString::Join(Tokens, TEXT(","));
		}
	};

	inline FCanonicalExecutionTrace*& CanonicalActiveExecutionTrace()
	{
		static thread_local FCanonicalExecutionTrace* Active = nullptr;
		return Active;
	}

	inline void CanonicalTraceGeneric(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FCanonicalExecutionTrace* const Trace = CanonicalActiveExecutionTrace();
		if (Trace == nullptr)
		{
			return;
		}
		Trace->Events.Add(static_cast<int32>(Generic->GetArgDWord(0)));
	}

	inline int RegisterCanonicalExecutionTrace(asIScriptEngine* ScriptEngine)
	{
		if (ScriptEngine == nullptr)
		{
			return asINVALID_ARG;
		}
		return ScriptEngine->RegisterGlobalFunction(
			"void Trace(int id)",
			asFUNCTION(CanonicalTraceGeneric),
			asCALL_GENERIC);
	}

	struct FCanonicalTraceScope
	{
		FCanonicalExecutionTrace& Trace;
		FCanonicalExecutionTrace* Previous;

		explicit FCanonicalTraceScope(FCanonicalExecutionTrace& InTrace)
			: Trace(InTrace)
			, Previous(CanonicalActiveExecutionTrace())
		{
			Trace.Reset();
			CanonicalActiveExecutionTrace() = &Trace;
		}

		~FCanonicalTraceScope()
		{
			CanonicalActiveExecutionTrace() = Previous;
		}
	};

	inline bool CanonicalExecuteInt(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		int32& OutValue)
	{
		return AngelscriptSDKTestSupport::ExecuteScriptFunction(
			Test,
			ScriptEngine,
			Module,
			Declaration,
			OutValue);
	}

	inline int CanonicalExecuteStatus(
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		FString& OutException)
	{
		OutException.Reset();
		asIScriptFunction* const Function = GetNativeFunctionByDecl(Module, Declaration);
		if (ScriptEngine == nullptr || Function == nullptr)
		{
			return asNO_FUNCTION;
		}
		asIScriptContext* const Context = ScriptEngine->CreateContext();
		if (Context == nullptr)
		{
			return asERROR;
		}
		const int Prepare = Context->Prepare(Function);
		if (Prepare < 0)
		{
			Context->Release();
			return Prepare;
		}
		const int Status = Context->Execute();
		if (Status == asEXECUTION_EXCEPTION)
		{
			const char* Exception = Context->GetExceptionString();
			OutException = UTF8_TO_TCHAR(Exception != nullptr ? Exception : "");
		}
		Context->Release();
		return Status;
	}
}
