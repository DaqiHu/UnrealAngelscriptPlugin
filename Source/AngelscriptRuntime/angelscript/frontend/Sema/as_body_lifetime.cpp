#include "frontend/Sema/as_body_lifetime.h"

#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_type.h"

#include "Templates/Function.h"

BEGIN_AS_NAMESPACE

bool asCBodyLifetimePlan::Analyze(asCCompoundStmt* Body)
	{
		ConstructedLocals.Reset();
		ExitCleanups.Reset();
		if (!Body) return false;

		struct asSControlScope
		{
			asCStmt* Target = nullptr;
			int32 FirstExitedScope = 0;
			int32 FirstContinueExitedScope = 0;
		};
		TArray<TArray<asSBodyCleanupObligation>> Scopes;
		TArray<asSControlScope> Controls;

		auto AppendCleanups = [&Scopes](asSBodyExitCleanup& Exit, int32 FirstScope)
		{
			for (int32 ScopeIndex = Scopes.Num() - 1; ScopeIndex >= FirstScope; --ScopeIndex)
			{
				const TArray<asSBodyCleanupObligation>& Locals = Scopes[ScopeIndex];
				for (int32 LocalIndex = Locals.Num() - 1; LocalIndex >= 0; --LocalIndex)
				{
					Exit.Obligations.Add(Locals[LocalIndex]);
				}
			}
		};

		TFunction<void(asCStmt*)> AnalyzeStatement = [&](asCStmt* Statement)
		{
			if (!Statement) return;
			if (asCCompoundStmt* Compound = dyn_cast<asCCompoundStmt>(Statement))
			{
				Scopes.AddDefaulted();
				for (asCStmt* Child : Compound->GetStatements()) AnalyzeStatement(Child);
				Scopes.Pop();
				return;
			}
			if (asCDeclStmt* DeclarationStatement = dyn_cast<asCDeclStmt>(Statement))
			{
				asCVarDecl* Declaration = DeclarationStatement->GetDeclaration();
				if (Declaration && !Declaration->IsInvalid() && Declaration->GetResolvedType() &&
					Declaration->GetResolvedType()->GetKind() == asETypeKind::Nominal && !Scopes.IsEmpty()
					&& !Declaration->GetTypeSyntax().bHandle && Declaration->GetTypeSyntax().Reference == asETypeReferenceDirection::None)
				{
					asSBodyConstructedLocal Constructed;
					Constructed.Declaration = Declaration;
					Constructed.StableLocalKey = Declaration->GetSemanticKey();
					Constructed.ScopeDepth = Scopes.Num() - 1;
					ConstructedLocals.Add(Constructed);
					asSBodyCleanupObligation Cleanup;
					Cleanup.StableLocalKey = Constructed.StableLocalKey;
					Cleanup.ScopeDepth = Constructed.ScopeDepth;
					Scopes.Last().Add(MoveTemp(Cleanup));
				}
				return;
			}
			if (asCDeclGroupStmt* Group = dyn_cast<asCDeclGroupStmt>(Statement))
			{
				for (asCStmt* Declaration : Group->GetDeclarations()) AnalyzeStatement(Declaration);
				return;
			}
			if (asCWhileStmt* While = dyn_cast<asCWhileStmt>(Statement))
			{
				Controls.Add({While, Scopes.Num(), Scopes.Num()});
				AnalyzeStatement(While->GetBody());
				Controls.Pop();
				return;
			}
			if (asCForStmt* For = dyn_cast<asCForStmt>(Statement))
			{
				const int32 ForScope = Scopes.Num();
				Scopes.AddDefaulted();
				for (asCStmt* Initializer : For->GetInitializers()) AnalyzeStatement(Initializer);
				Controls.Add({For, ForScope, ForScope + 1});
				AnalyzeStatement(For->GetBody());
				Controls.Pop();
				Scopes.Pop();
				return;
			}
			if (asCDoStmt* Do = dyn_cast<asCDoStmt>(Statement))
			{
				Controls.Add({Do, Scopes.Num(), Scopes.Num()});
				AnalyzeStatement(Do->GetBody());
				Controls.Pop();
				return;
			}
			if (asCSwitchStmt* Switch = dyn_cast<asCSwitchStmt>(Statement))
			{
				Controls.Add({Switch, Scopes.Num(), Scopes.Num()});
				for (asCSwitchClause* Clause : Switch->GetClauses())
					for (asCStmt* Child : Clause->GetStatements()) AnalyzeStatement(Child);
				Controls.Pop();
				return;
			}
			if (asCForeachStmt* Foreach = dyn_cast<asCForeachStmt>(Statement))
			{
				const int32 IterationScope = Scopes.Num();
				Scopes.AddDefaulted();
				AnalyzeStatement(Foreach->GetValueStatement());
				AnalyzeStatement(Foreach->GetKeyStatement());
				Controls.Add({Foreach, IterationScope, IterationScope});
				AnalyzeStatement(Foreach->GetBody());
				Controls.Pop();
				Scopes.Pop();
				return;
			}
			if (asCClassDefaultStmt* Default = dyn_cast<asCClassDefaultStmt>(Statement))
			{
				AnalyzeStatement(Default->GetStatement());
				return;
			}
			if (asCBreakStmt* Break = dyn_cast<asCBreakStmt>(Statement))
			{
				asSBodyExitCleanup Exit;
				Exit.Exit = Break;
				Exit.Target = Break->GetTarget();
				for (int32 Index = Controls.Num() - 1; Index >= 0; --Index)
				{
					if (Controls[Index].Target == Exit.Target)
					{
						AppendCleanups(Exit, Controls[Index].FirstExitedScope);
						break;
					}
				}
				ExitCleanups.Add(MoveTemp(Exit));
				return;
			}
			if (asCContinueStmt* Continue = dyn_cast<asCContinueStmt>(Statement))
			{
				asSBodyExitCleanup Exit;
				Exit.Exit = Continue;
				Exit.Target = Continue->GetTarget();
				for (int32 Index = Controls.Num() - 1; Index >= 0; --Index)
				{
					if (Controls[Index].Target == Exit.Target)
					{
						AppendCleanups(Exit, Controls[Index].FirstContinueExitedScope);
						break;
					}
				}
				ExitCleanups.Add(MoveTemp(Exit));
				return;
			}
			if (asCReturnStmt* Return = dyn_cast<asCReturnStmt>(Statement))
			{
				asSBodyExitCleanup Exit;
				Exit.Exit = Return;
				Exit.Target = Return->GetTarget();
				AppendCleanups(Exit, 0);
				ExitCleanups.Add(MoveTemp(Exit));
				return;
			}
			if (asCIfStmt* If = dyn_cast<asCIfStmt>(Statement))
			{
				AnalyzeStatement(If->GetThen());
				AnalyzeStatement(If->GetElse());
			}
		};
		AnalyzeStatement(Body);
		return true;
	}

	FString asCBodyLifetimePlan::SerializeStable() const
	{
		TArray<FString> Records;
		for (const asSBodyConstructedLocal& Local : ConstructedLocals)
		{
			Records.Add(FString::Printf(TEXT("local|%s|%d"), *Local.StableLocalKey, Local.ScopeDepth));
		}
		for (const asSBodyExitCleanup& Exit : ExitCleanups)
		{
			TArray<FString> Keys;
			for (const asSBodyCleanupObligation& Cleanup : Exit.Obligations) Keys.Add(Cleanup.StableLocalKey);
			Records.Add(FString::Printf(
				TEXT("exit|%u|%u|%s"),
				Exit.Exit ? static_cast<uint16>(Exit.Exit->GetKind()) : 0,
				Exit.Exit ? Exit.Exit->GetSourceRange().Begin.ByteOffset : 0,
				*FString::Join(Keys, TEXT(","))));
		}
		return FString::Join(Records, TEXT("\n"));
	}

END_AS_NAMESPACE
