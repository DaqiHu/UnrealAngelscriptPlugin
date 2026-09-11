#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestUtilities.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_type.h"
#include "as_builder.h"
#include "as_datatype.h"
#include "frontend/as_decl.h"
#include "frontend/as_expr.h"
#include "as_module.h"
#include "as_objecttype.h"
#include "frontend/as_parser.h"
#include "source/as_scriptcode.h"
#include "as_scriptengine.h"
#include "frontend/as_sema.h"
#include "EndAngelscriptHeaders.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

static FString DescribeLiteralAssetHostSurface(asCScriptEngine* ScriptEngine)
{
	FString Text;
	if (ScriptEngine == nullptr)
	{
		return TEXT("<no engine>");
	}

	const asUINT GlobalPropertyCount = ScriptEngine->GetGlobalPropertyCount();
	for (asUINT PropertyIndex = 0; PropertyIndex < GlobalPropertyCount; ++PropertyIndex)
	{
		const char* PropertyName = nullptr;
		const char* NameSpaceName = nullptr;
		int TypeId = 0;
		bool bIsConst = false;
		if (ScriptEngine->GetGlobalPropertyByIndex(
				PropertyIndex, &PropertyName, &NameSpaceName, &TypeId, &bIsConst) < 0
			|| PropertyName == nullptr
			|| FCStringAnsi::Strcmp(PropertyName, "__StaticType_UObject") != 0)
		{
			continue;
		}
		asCDataType DataType = ScriptEngine->GetDataTypeFromTypeId(TypeId);
		asCString Format = DataType.Format(nullptr, true);
		asITypeInfo* const ById = ScriptEngine->GetTypeInfoById(TypeId);
		asITypeInfo* const ByDecl = Format.GetLength()
			? ScriptEngine->GetTypeInfoByDecl(Format.AddressOf())
			: nullptr;
		Text += FString::Printf(
			TEXT("\n  staticType name=%s ns=%s typeId=%d const=%d format=%s byId=%s byDecl=%s flags=0x%x"),
			UTF8_TO_TCHAR(PropertyName),
			NameSpaceName ? UTF8_TO_TCHAR(NameSpaceName) : TEXT(""),
			TypeId,
			bIsConst ? 1 : 0,
			Format.GetLength() ? UTF8_TO_TCHAR(Format.AddressOf()) : TEXT("<empty>"),
			ById && ById->GetName() ? UTF8_TO_TCHAR(ById->GetName()) : TEXT("<null>"),
			ByDecl && ByDecl->GetName() ? UTF8_TO_TCHAR(ByDecl->GetName()) : TEXT("<null>"),
			ById ? ById->GetFlags() : 0);
		asITypeInfo* const TypeInfo = ById ? ById : ByDecl;
		if (TypeInfo == nullptr)
		{
			continue;
		}
		const asUINT MethodCount = TypeInfo->GetMethodCount();
		int32 ImplConvCount = 0;
		for (asUINT MethodIndex = 0; MethodIndex < MethodCount; ++MethodIndex)
		{
			asIScriptFunction* const Method = TypeInfo->GetMethodByIndex(MethodIndex);
			if (Method == nullptr || Method->GetName() == nullptr
				|| FCStringAnsi::Strcmp(Method->GetName(), "opImplConv") != 0)
			{
				continue;
			}
			++ImplConvCount;
			const char* const Declaration = Method->GetDeclaration(true, true, true);
			Text += FString::Printf(
				TEXT("\n    opImplConv[%d] params=%u decl=%s"),
				Method->GetId(),
				Method->GetParamCount(),
				Declaration ? UTF8_TO_TCHAR(Declaration) : TEXT("<no-decl>"));
		}
		Text += FString::Printf(TEXT("\n    opImplConvCount=%d methodCount=%u"), ImplConvCount, MethodCount);
	}
	return Text;
}

static FString DescribeLiteralAssetSemaDump(
	asCScriptEngine* ScriptEngine,
	const char* ModuleName,
	const char* SectionName,
	const std::string& ScriptSource)
{
	FString Text;
	if (ScriptEngine == nullptr || ModuleName == nullptr)
	{
		return TEXT("<no engine>");
	}

	asCModule* const Module = static_cast<asCModule*>(
		ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
	if (Module == nullptr)
	{
		return TEXT("<no dump module>");
	}
	ON_SCOPE_EXIT
	{
		ScriptEngine->DiscardModule(ModuleName);
	};

	asCBuilder Builder(ScriptEngine, Module);
	Builder.silent = true;
	asCScriptCode Code;
	Code.SetCode(SectionName, ScriptSource.c_str(), true);
	asCASTContext Context;
	asCSema Sema(ScriptEngine, Context);
	asCParser Parser(&Builder);
	Parser.SetSema(&Sema);
	const int ParseResult = Parser.ParseScript(&Code);
	const int SealResult = Context.Seal();
	Text += FString::Printf(TEXT("\n  parse=%d seal=%d diags=%u"),
		ParseResult, SealResult, Sema.GetDiagnostics().GetLength());
	for (asUINT DiagnosticIndex = 0; DiagnosticIndex < Sema.GetDiagnostics().GetLength(); ++DiagnosticIndex)
	{
		const asCString& Diagnostic = Sema.GetDiagnostics()[DiagnosticIndex];
		Text += FString::Printf(
			TEXT("\n  diag[%u]=%s"),
			DiagnosticIndex,
			Diagnostic.GetLength() ? UTF8_TO_TCHAR(Diagnostic.AddressOf()) : TEXT("<empty>"));
	}

	asCString Dump;
	asCASTDump(Context, Dump);
	Text += TEXT("\n  ast:\n");
	Text += Dump.GetLength() ? UTF8_TO_TCHAR(Dump.AddressOf()) : TEXT("<empty dump>");

	for (asUINT ExprIndex = 1; ExprIndex <= Context.GetExprCount(); ++ExprIndex)
	{
		const asCExpr* const Expr = Context.GetExpr(asASTExprId(ExprIndex));
		if (Expr == nullptr || Expr->kind != asAST_EXPR_CALL)
		{
			continue;
		}
		const asCDecl* const Callee = Context.GetDecl(Expr->resolvedDecl);
		const asCType* const CallType = Context.GetType(Expr->type.type);
		Text += FString::Printf(
			TEXT("\n  call E%u literal=%s resolved=%s callee=%s type=%s children=%u"),
			ExprIndex,
			Expr->literal.GetLength() ? UTF8_TO_TCHAR(Expr->literal.AddressOf()) : TEXT(""),
			Expr->resolvedDecl.IsValid() ? TEXT("1") : TEXT("0"),
			Callee && Callee->name.GetLength()
				? UTF8_TO_TCHAR(Callee->name.AddressOf())
				: TEXT("<none>"),
			CallType && CallType->stableKey.GetLength()
				? UTF8_TO_TCHAR(CallType->stableKey.AddressOf())
				: TEXT("<none>"),
			Expr->children.GetLength());
		for (asUINT ChildIndex = 0; ChildIndex < Expr->children.GetLength(); ++ChildIndex)
		{
			const asCExpr* Child = Context.GetExpr(Expr->children[ChildIndex]);
			FString Chain;
			int32 Depth = 0;
			while (Child != nullptr && Depth < 6)
			{
				const asCType* const ChildType = Context.GetType(Child->type.type);
				const asCDecl* const ChildDecl = Context.GetDecl(Child->resolvedDecl);
				Chain += FString::Printf(
					TEXT("%s%s ty=%s decl=%s lit=%s"),
					Depth == 0 ? TEXT("") : TEXT(" -> "),
					Child->kind == asAST_EXPR_CONVERSION ? TEXT("CONV")
						: Child->kind == asAST_EXPR_DECL_REF ? TEXT("REF")
						: Child->kind == asAST_EXPR_STRING_LITERAL ? TEXT("STR")
						: Child->kind == asAST_EXPR_CALL ? TEXT("CALL")
						: *FString::Printf(TEXT("K%d"), static_cast<int32>(Child->kind)),
					ChildType && ChildType->stableKey.GetLength()
						? UTF8_TO_TCHAR(ChildType->stableKey.AddressOf())
						: TEXT("?"),
					ChildDecl && ChildDecl->name.GetLength()
						? UTF8_TO_TCHAR(ChildDecl->name.AddressOf())
						: TEXT("-"),
					Child->literal.GetLength()
						? UTF8_TO_TCHAR(Child->literal.AddressOf())
						: TEXT(""));
				if (Child->children.GetLength() == 0)
				{
					break;
				}
				Child = Context.GetExpr(Child->children[0]);
				++Depth;
			}
			Text += FString::Printf(TEXT("\n    arg%u %s"), ChildIndex, *Chain);
		}
	}

	for (asUINT DeclIndex = 1; DeclIndex <= Context.GetDeclCount(); ++DeclIndex)
	{
		const asCDecl* const Decl = Context.GetDecl(asASTDeclId(DeclIndex));
		if (Decl == nullptr
			|| (Decl->kind != asAST_DECL_CLASS && Decl->kind != asAST_DECL_FUNCTION
				&& Decl->kind != asAST_DECL_VAR && Decl->kind != asAST_DECL_METHOD))
		{
			continue;
		}
		if (!Decl->name.Equals("__CreateLiteralAsset")
			&& !Decl->name.Equals("__StaticType_UObject")
			&& !Decl->name.Equals("opImplConv")
			&& !Decl->name.StartsWith("TSubclassOf"))
		{
			continue;
		}
		const asCType* const DeclType = Context.GetType(Decl->type.type);
		Text += FString::Printf(
			TEXT("\n  decl D%u kind=%d name=%s type=%s children=%u origin=%s"),
			DeclIndex,
			static_cast<int32>(Decl->kind),
			Decl->name.GetLength() ? UTF8_TO_TCHAR(Decl->name.AddressOf()) : TEXT(""),
			DeclType && DeclType->stableKey.GetLength()
				? UTF8_TO_TCHAR(DeclType->stableKey.AddressOf())
				: TEXT("<none>"),
			Decl->children.GetLength(),
			Decl->origin.GetLength() ? UTF8_TO_TCHAR(Decl->origin.AddressOf()) : TEXT(""));
	}
	return Text;
}

// Focused UE Sema slice for generation `asset Name of Type`.
// Native THost<T> + type-id + opImplConv already seals; this uses the real
// host binds `__CreateLiteralAsset(UClass, const FString&)` and
// `__StaticType_UObject`.

TEST_CLASS_WITH_FLAGS(FCanonicalASTLiteralAssetSemaTests,
	"Angelscript.TestModule.StaticJIT.CanonicalAST.LiteralAssetSema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(CreateLiteralAssetTypeIdentifierSealsResolvedCall)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("literal-asset Sema fixture needs the Engine-owned script engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		const int PreviousPipeline = ScriptEngine->GetCompilerPipeline();
		Engine.NextCompileASTRetentionPolicy =
			static_cast<int32>(asAST_RETAIN_SNAPSHOT);
		ON_SCOPE_EXIT
		{
			Engine.NextCompileASTRetentionPolicy = 0;
			Engine.DiscardModule(TEXT("CanonicalASTLiteralAssetSema"));
			ScriptEngine->SetCompilerPipeline(
				static_cast<asECompilerPipeline>(PreviousPipeline));
		};
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("literal-asset Sema must select CANONICAL")));

		FString HostDecls;
		int32 HostMatches = 0;
		const asUINT GlobalFunctionCount = ScriptEngine->GetGlobalFunctionCount();
		for (asUINT FunctionIndex = 0; FunctionIndex < GlobalFunctionCount; ++FunctionIndex)
		{
			asIScriptFunction* const Function =
				ScriptEngine->GetGlobalFunctionByIndex(FunctionIndex);
			if (Function == nullptr || Function->GetName() == nullptr
				|| FCStringAnsi::Strcmp(Function->GetName(), "__CreateLiteralAsset") != 0)
			{
				continue;
			}
			++HostMatches;
			const char* const Declaration = Function->GetDeclaration(true, true, true);
			HostDecls += FString::Printf(
				TEXT("\n  [%d] %s params=%u"),
				Function->GetId(),
				Declaration ? UTF8_TO_TCHAR(Declaration) : TEXT("<no-decl>"),
				Function->GetParamCount());
		}
		int32 StaticTypeMatches = 0;
		FString StaticTypeDecls;
		const asUINT GlobalPropertyCount = ScriptEngine->GetGlobalPropertyCount();
		for (asUINT PropertyIndex = 0; PropertyIndex < GlobalPropertyCount; ++PropertyIndex)
		{
			const char* PropertyName = nullptr;
			const char* NameSpaceName = nullptr;
			int TypeId = 0;
			bool bIsConst = false;
			if (ScriptEngine->GetGlobalPropertyByIndex(
					PropertyIndex, &PropertyName, &NameSpaceName, &TypeId, &bIsConst) < 0
				|| PropertyName == nullptr
				|| FCStringAnsi::Strcmp(PropertyName, "__StaticType_UObject") != 0)
			{
				continue;
			}
			++StaticTypeMatches;
			asITypeInfo* const TypeInfo = ScriptEngine->GetTypeInfoById(TypeId);
			StaticTypeDecls += FString::Printf(
				TEXT("\n  %s ns=%s typeId=%d type=%s const=%d"),
				UTF8_TO_TCHAR(PropertyName),
				NameSpaceName ? UTF8_TO_TCHAR(NameSpaceName) : TEXT(""),
				TypeId,
				TypeInfo && TypeInfo->GetName() ? UTF8_TO_TCHAR(TypeInfo->GetName()) : TEXT("?"),
				bIsConst ? 1 : 0);
		}
		ASSERT_THAT(IsTrue(HostMatches > 0,
			*FString::Printf(
				TEXT("host must expose __CreateLiteralAsset before compile. matches=%d decls:%s staticType=%d%s"),
				HostMatches,
				*HostDecls,
				StaticTypeMatches,
				*StaticTypeDecls)));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UObject GetLiteral()
			{
				return Cast<UObject>(__CreateLiteralAsset(UObject, "PrimaryTypedAsset"));
			}
			)AS");
		FString PreprocessedDump = TEXT("<preprocess failed>");
		{
			const FString DumpFilename = FPaths::Combine(
				FPaths::ProjectSavedDir(), TEXT("Automation"),
				TEXT("CanonicalASTLiteralAssetSemaPreprocess.as"));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(DumpFilename), true);
			FFileHelper::SaveStringToFile(ScriptSource, *DumpFilename);
			FAngelscriptPreprocessor Preprocessor;
			Preprocessor.AddFile(TEXT("CanonicalASTLiteralAssetSemaPreprocess.as"), DumpFilename);
			if (Preprocessor.Preprocess())
			{
				PreprocessedDump.Reset();
				const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules =
					Preprocessor.GetModulesToCompile();
				for (const TSharedRef<FAngelscriptModuleDesc>& ModuleDesc : Modules)
				{
					PreprocessedDump += FString::Printf(
						TEXT("\n  module=%s sections=%d"),
						*ModuleDesc->ModuleName,
						ModuleDesc->Code.Num());
					for (const FAngelscriptModuleDesc::FCodeSection& Section : ModuleDesc->Code)
					{
						PreprocessedDump += FString::Printf(
							TEXT("\n  section=%s code=\n%s"),
							*Section.RelativeFilename,
							*Section.Code);
					}
				}
				if (PreprocessedDump.IsEmpty())
				{
					PreprocessedDump = TEXT("<no modules>");
				}
			}
		}

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("CanonicalASTLiteralAssetSema"),
			ScriptSource);
		FString AstDump = TEXT("<no retained AST>");
		if (ModuleScope.IsValid())
		{
			asCModule* const InternalModule =
				static_cast<asCModule*>(&ModuleScope.GetModule());
			if (const asCASTContext* const Sealed =
				InternalModule != nullptr ? InternalModule->GetCanonicalASTContext() : nullptr)
			{
				asCString Dump;
				asCASTDump(*Sealed, Dump);
				AstDump = FString(UTF8_TO_TCHAR(Dump.AddressOf() ? Dump.AddressOf() : ""));
			}
		}
		const FString HostSurface = DescribeLiteralAssetHostSurface(ScriptEngine);
		const std::string SemaSource = ASTEST_AS_ANSI(R"AS(
			UObject GetLiteral()
			{
				return Cast<UObject>(__CreateLiteralAsset(UObject, "PrimaryTypedAsset"));
			}
			)AS");
		const FString SemaDump = DescribeLiteralAssetSemaDump(
			ScriptEngine,
			"CanonicalASTLiteralAssetSemaDump",
			"CanonicalASTLiteralAssetSemaDump.as",
			SemaSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(),
			*FString::Printf(
				TEXT("CANONICAL must compile __CreateLiteralAsset(UObject, \"Name\"). hostMatches=%d decls:%s staticType=%d%s hostSurface:%s preprocessed:%s semaDump:%s ast:\n%s"),
				HostMatches,
				*HostDecls,
				StaticTypeMatches,
				*StaticTypeDecls,
				*HostSurface,
				*PreprocessedDump,
				*SemaDump,
				*AstDump)));
		ASSERT_THAT(IsTrue(
			AstDump.Contains(TEXT("callee=__CreateLiteralAsset"))
				|| AstDump.Contains(TEXT("name=__CreateLiteralAsset")),
			*FString::Printf(
				TEXT("sealed AST must resolve __CreateLiteralAsset. ast:\n%s"),
				*AstDump)));
		ASSERT_THAT(IsTrue(
			!AstDump.Contains(TEXT("unresolved-callee:__CreateLiteralAsset")),
			*FString::Printf(
				TEXT("sealed AST must not keep unresolved __CreateLiteralAsset. ast:\n%s"),
				*AstDump)));
	}

	TEST_METHOD(DirectCanonicalModuleBuildSealsResolvedCall)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("direct module build needs the Engine-owned script engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		const int PreviousPipeline = ScriptEngine->GetCompilerPipeline();
		ON_SCOPE_EXIT
		{
			ScriptEngine->DiscardModule("CanonicalASTLiteralAssetDirectBuild");
			ScriptEngine->SetCompilerPipeline(
				static_cast<asECompilerPipeline>(PreviousPipeline));
		};
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("direct module build must select CANONICAL")));
		const bool PreviousDeferTemplates = ScriptEngine->deferValidationOfTemplateTypes;
		ScriptEngine->deferValidationOfTemplateTypes = true;
		ON_SCOPE_EXIT
		{
			ScriptEngine->deferValidationOfTemplateTypes = PreviousDeferTemplates;
		};

		asIScriptModule* const Module = ScriptEngine->GetModule(
			"CanonicalASTLiteralAssetDirectBuild", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("direct CANONICAL module must allocate")));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			UObject GetLiteral()
			{
				return Cast<UObject>(__CreateLiteralAsset(UObject, "PrimaryTypedAsset"));
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"CanonicalASTLiteralAssetDirectBuild.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Sealed =
			InternalModule != nullptr ? InternalModule->GetCanonicalASTContext() : nullptr)
		{
			asCString Dump;
			asCASTDump(*Sealed, Dump);
			AstDump = FString(UTF8_TO_TCHAR(Dump.AddressOf() ? Dump.AddressOf() : ""));
		}
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("asIScriptModule::Build CANONICAL must resolve __CreateLiteralAsset. ast:\n%s"),
				*AstDump)));
		ASSERT_THAT(IsTrue(
			AstDump.Contains(TEXT("callee=__CreateLiteralAsset"))
				|| AstDump.Contains(TEXT("__CreateLiteralAsset:reverse-formal")),
			*FString::Printf(
				TEXT("direct CANONICAL build must seal resolved __CreateLiteralAsset. ast:\n%s"),
				*AstDump)));
	}
};

#endif
