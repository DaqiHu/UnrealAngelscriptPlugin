#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_public_view.h"
#include "source/as_bytecode_codegen.h"
#include "source/as_module.h"
#include "source/as_objecttype.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_sema.h"
#include "source/as_runtime_type_binding.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	asCSourceRange AddTxnAuthoredFunctionSource(
		asCASTContext& Context,
		const char* LogicalKey)
	{
		static const char Source[] = "int F() { return 7; }";
		const asASTFileID File = Context.AddSourceSection(
			LogicalKey,
			asAST_SOURCE_AUTHORED,
			Source,
			sizeof(Source) - 1,
			0);
		return Context.GetSourceManager().MakeRange(File, 0, sizeof(Source) - 1);
	}

	void AttachTxnReturnAsBody(asCSema& Sema, asCASTContext& Ast, asASTDeclId Fn, asASTExprId Value)
	{
		const asCSourceRange Range;
		asCArray<asASTStmtId> Stmts;
		Stmts.PushLast(Sema.ActOnReturnStmt(Fn, Value, Range));
		Ast.SetBody(Fn, Sema.ActOnCompoundStmt(Fn, Stmts, Range));
	}

	struct FTxnListPatternBox
	{
		int32 Marker = 0;
	};

	struct FTxnRelocationPod
	{
		int32 Value = 0;
	};

	void TxnListPatternFactory(asIScriptGeneric* Generic)
	{
		FTxnListPatternBox* const Box = new FTxnListPatternBox();
		if (Generic != nullptr)
		{
			Generic->SetReturnAddress(Box);
		}
	}

	void TxnObserveWildcard(asIScriptGeneric*)
	{
	}

	struct FCodeGenTableSnapshot
	{
		asUINT ModuleFunctionCount = 0;
		asUINT ModuleScriptFunctions = 0;
		asUINT ModuleGlobalFunctionList = 0;
		asUINT ModuleGlobalVars = 0;
		asUINT ModuleScriptGlobalsList = 0;
		asUINT ModuleImportedFunctions = 0;
		asUINT ModuleFuncDefs = 0;
		asUINT ModuleClassTypes = 0;
		asUINT ModuleEnumTypes = 0;
		asUINT ModuleTypeDefs = 0;
		asUINT EngineScriptFunctions = 0;
		asUINT EngineScriptFunctionOccupied = 0;
		asUINT EngineFreeScriptFunctionIds = 0;
		asUINT EngineImportedFunctions = 0;
		asUINT EngineImportedFunctionsOccupied = 0;
		asUINT EngineFreeImportedFunctionIds = 0;
		asUINT EngineFuncDefs = 0;
		asUINT EngineRegisteredFuncDefs = 0;
		asUINT EngineGlobalProperties = 0;
		asUINT EngineGlobalPropertiesOccupied = 0;
		asUINT EngineFreeGlobalPropertyIds = 0;
		asUINT EngineVarAddressMap = 0;
		asUINT EngineAllScriptGlobalFunctions = 0;
		asUINT EngineAllScriptGlobalVariables = 0;
		asUINT EngineAllScriptDeclaredTypes = 0;
		asUINT EngineListPatternTypes = 0;
		int EngineTypeIdSequence = 0;
		asUINT EngineTypeIdMap = 0;
		asDWORD FunctionBehavioursInternalRefs = 0;
		asEBytecodePublisher Publisher = asBYTECODE_PUBLISHER_NONE;
		bool HasCanonicalDigest = false;
		asQWORD CanonicalDigest = 0;
		bool HasASTSnapshot = false;
		bool ASTSnapshotCurrent = false;
		FString ASTGenerationKey;
		asUINT RuntimeBindingCount = 0;
		FString RuntimeBindingFingerprint;
	};

	asUINT OccupiedPointers(const asCArray<asCScriptFunction*>& Values)
	{
		asUINT Count = 0;
		for (asUINT Index = 0; Index < Values.GetLength(); ++Index)
		{
			if (Values[Index] != nullptr)
			{
				++Count;
			}
		}
		return Count;
	}

	asUINT OccupiedGlobals(const asCArray<asCGlobalProperty*>& Values)
	{
		asUINT Count = 0;
		for (asUINT Index = 0; Index < Values.GetLength(); ++Index)
		{
			if (Values[Index] != nullptr)
			{
				++Count;
			}
		}
		return Count;
	}

	asUINT OccupiedImportBindings(const asCArray<sBindInfo*>& Values)
	{
		asUINT Count = 0;
		for (asUINT Index = 0; Index < Values.GetLength(); ++Index)
		{
			if (Values[Index] != nullptr)
			{
				++Count;
			}
		}
		return Count;
	}

	template<typename TSymbolMap>
	asUINT CountSymbolEntries(const TSymbolMap& Values)
	{
		asUINT Count = 0;
		Values.IterateAll([&Count](auto*)
		{
			++Count;
		});
		return Count;
	}

	FCodeGenTableSnapshot CaptureCodeGenTables(asCScriptEngine* Engine, asCModule* Module)
	{
		FCodeGenTableSnapshot Snapshot;
		if (Module != nullptr)
		{
			Snapshot.ModuleFunctionCount = Module->GetFunctionCount();
			Snapshot.ModuleScriptFunctions = Module->scriptFunctions.GetLength();
			Snapshot.ModuleGlobalFunctionList = Module->globalFunctionList.GetLength();
			Snapshot.ModuleGlobalVars = Module->GetGlobalVarCount();
			Snapshot.ModuleScriptGlobalsList = Module->scriptGlobalsList.GetLength();
			Snapshot.ModuleImportedFunctions = Module->GetImportedFunctionCount();
			Snapshot.ModuleFuncDefs = Module->funcDefs.GetLength();
			Snapshot.ModuleClassTypes = Module->classTypes.GetLength();
			Snapshot.ModuleEnumTypes = Module->enumTypes.GetLength();
			Snapshot.ModuleTypeDefs = Module->typeDefs.GetLength();
			Snapshot.Publisher = Module->GetLastBytecodePublisher();
			Snapshot.HasCanonicalDigest =
				Module->GetLastCanonicalCodeGenAstDigest(Snapshot.CanonicalDigest);
			asCASTSnapshot* const ASTSnapshot = static_cast<asCASTSnapshot*>(
				Module->AcquireASTSnapshot(asAST_API_VERSION_1));
			if (ASTSnapshot != nullptr)
			{
				Snapshot.HasASTSnapshot = true;
				Snapshot.ASTSnapshotCurrent = ASTSnapshot->IsCurrentGeneration();
				Snapshot.ASTGenerationKey = UTF8_TO_TCHAR(
					ASTSnapshot->GetGenerationKey());
				Snapshot.RuntimeBindingCount =
					ASTSnapshot->GetRuntimeTypeBindingCountForDiagnostics();
				for (asUINT Index = 0; Index < Snapshot.RuntimeBindingCount; ++Index)
				{
					const asSResolvedRuntimeTypeBinding* const Binding =
						ASTSnapshot->GetRuntimeTypeBindingForDiagnostics(Index);
					if (Binding == nullptr)
					{
						Snapshot.RuntimeBindingFingerprint += TEXT("<null>|");
						continue;
					}
					const FString StableKey = UTF8_TO_TCHAR(
						Binding->key.stableTypeKey.AddressOf()
							? Binding->key.stableTypeKey.AddressOf() : "");
					const FString Layout = UTF8_TO_TCHAR(
						Binding->key.layoutSignature.AddressOf()
							? Binding->key.layoutSignature.AddressOf() : "");
					const FString Behaviour = UTF8_TO_TCHAR(
						Binding->key.behaviourSignature.AddressOf()
							? Binding->key.behaviourSignature.AddressOf() : "");
					const FString DataType = UTF8_TO_TCHAR(
						Binding->dataType.Format(nullptr).AddressOf());
					Snapshot.RuntimeBindingFingerprint += FString::Printf(
						TEXT("%s;%d;%llu;%d;%d;%s;%s;%s;%d;%d;%d|"),
						*StableKey,
						static_cast<int32>(Binding->key.typeKind),
						static_cast<unsigned long long>(Binding->key.layoutHash),
						Binding->key.byteSize,
						Binding->key.byteAlignment,
						*Layout,
						*Behaviour,
						*DataType,
						Binding->publicTypeId,
						Binding->propertyOffset,
						Binding->function ? Binding->function->id : -1);
				}
				ASTSnapshot->Release();
			}
		}
		if (Engine != nullptr)
		{
			Snapshot.EngineScriptFunctions = Engine->scriptFunctions.GetLength();
			Snapshot.EngineScriptFunctionOccupied = OccupiedPointers(Engine->scriptFunctions);
			Snapshot.EngineFreeScriptFunctionIds = Engine->freeScriptFunctionIds.GetLength();
			Snapshot.EngineImportedFunctions = Engine->importedFunctions.GetLength();
			Snapshot.EngineImportedFunctionsOccupied = OccupiedImportBindings(Engine->importedFunctions);
			Snapshot.EngineFreeImportedFunctionIds = Engine->freeImportedFunctionIdxs.GetLength();
			Snapshot.EngineFuncDefs = Engine->funcDefs.GetLength();
			Snapshot.EngineRegisteredFuncDefs = Engine->GetFuncdefCount();
			Snapshot.EngineGlobalProperties = Engine->globalProperties.GetLength();
			Snapshot.EngineGlobalPropertiesOccupied = OccupiedGlobals(Engine->globalProperties);
			Snapshot.EngineFreeGlobalPropertyIds = Engine->freeGlobalPropertyIds.GetLength();
			Snapshot.EngineVarAddressMap = static_cast<asUINT>(Engine->varAddressMap.Num());
			Snapshot.EngineAllScriptGlobalFunctions = CountSymbolEntries(Engine->allScriptGlobalFunctions);
			Snapshot.EngineAllScriptGlobalVariables = CountSymbolEntries(Engine->allScriptGlobalVariables);
			Snapshot.EngineAllScriptDeclaredTypes = CountSymbolEntries(Engine->allScriptDeclaredTypes);
			Snapshot.EngineListPatternTypes = Engine->listPatternTypes.GetLength();
			Snapshot.EngineTypeIdSequence = Engine->typeIdSeqNbr;
			Snapshot.EngineTypeIdMap = static_cast<asUINT>(Engine->mapTypeIdToTypeInfo.Num());
			Snapshot.FunctionBehavioursInternalRefs =
				Engine->functionBehaviours.GetInternalReferenceCountForTesting();
		}
		return Snapshot;
	}

	bool AssertCodeGenTablesEqual(
		FAutomationTestBase& Test,
		const TCHAR* Label,
		const FCodeGenTableSnapshot& Before,
		const FCodeGenTableSnapshot& After)
	{
		bool bOk = true;
		auto CheckUInt = [&](const TCHAR* Name, asUINT Left, asUINT Right)
		{
			if (Left != Right)
			{
				Test.AddError(FString::Printf(
					TEXT("%s: %s before=%u after=%u (GetFunctionCount==0 is not a transaction proof)"),
					Label, Name, Left, Right));
				bOk = false;
			}
		};
		CheckUInt(TEXT("ModuleFunctionCount"), Before.ModuleFunctionCount, After.ModuleFunctionCount);
		CheckUInt(TEXT("ModuleScriptFunctions"), Before.ModuleScriptFunctions, After.ModuleScriptFunctions);
		CheckUInt(TEXT("ModuleGlobalFunctionList"), Before.ModuleGlobalFunctionList, After.ModuleGlobalFunctionList);
		CheckUInt(TEXT("ModuleGlobalVars"), Before.ModuleGlobalVars, After.ModuleGlobalVars);
		CheckUInt(TEXT("ModuleScriptGlobalsList"), Before.ModuleScriptGlobalsList, After.ModuleScriptGlobalsList);
		CheckUInt(TEXT("ModuleImportedFunctions"), Before.ModuleImportedFunctions, After.ModuleImportedFunctions);
		CheckUInt(TEXT("ModuleFuncDefs"), Before.ModuleFuncDefs, After.ModuleFuncDefs);
		CheckUInt(TEXT("ModuleClassTypes"), Before.ModuleClassTypes, After.ModuleClassTypes);
		CheckUInt(TEXT("ModuleEnumTypes"), Before.ModuleEnumTypes, After.ModuleEnumTypes);
		CheckUInt(TEXT("ModuleTypeDefs"), Before.ModuleTypeDefs, After.ModuleTypeDefs);
		CheckUInt(TEXT("EngineScriptFunctions"), Before.EngineScriptFunctions, After.EngineScriptFunctions);
		CheckUInt(TEXT("EngineScriptFunctionOccupied"), Before.EngineScriptFunctionOccupied, After.EngineScriptFunctionOccupied);
		CheckUInt(TEXT("EngineFreeScriptFunctionIds"), Before.EngineFreeScriptFunctionIds, After.EngineFreeScriptFunctionIds);
		CheckUInt(TEXT("EngineImportedFunctions"), Before.EngineImportedFunctions, After.EngineImportedFunctions);
		CheckUInt(TEXT("EngineImportedFunctionsOccupied"), Before.EngineImportedFunctionsOccupied, After.EngineImportedFunctionsOccupied);
		CheckUInt(TEXT("EngineFreeImportedFunctionIds"), Before.EngineFreeImportedFunctionIds, After.EngineFreeImportedFunctionIds);
		CheckUInt(TEXT("EngineFuncDefs"), Before.EngineFuncDefs, After.EngineFuncDefs);
		CheckUInt(TEXT("EngineRegisteredFuncDefs"), Before.EngineRegisteredFuncDefs, After.EngineRegisteredFuncDefs);
		CheckUInt(TEXT("EngineGlobalPropertiesOccupied"), Before.EngineGlobalPropertiesOccupied, After.EngineGlobalPropertiesOccupied);
		CheckUInt(TEXT("EngineFreeGlobalPropertyIds"), Before.EngineFreeGlobalPropertyIds, After.EngineFreeGlobalPropertyIds);
		CheckUInt(TEXT("EngineVarAddressMap"), Before.EngineVarAddressMap, After.EngineVarAddressMap);
		CheckUInt(TEXT("EngineAllScriptGlobalFunctions"), Before.EngineAllScriptGlobalFunctions, After.EngineAllScriptGlobalFunctions);
		CheckUInt(TEXT("EngineAllScriptGlobalVariables"), Before.EngineAllScriptGlobalVariables, After.EngineAllScriptGlobalVariables);
		CheckUInt(TEXT("EngineAllScriptDeclaredTypes"), Before.EngineAllScriptDeclaredTypes, After.EngineAllScriptDeclaredTypes);
		CheckUInt(TEXT("EngineListPatternTypes"), Before.EngineListPatternTypes, After.EngineListPatternTypes);
		if (Before.EngineTypeIdSequence != After.EngineTypeIdSequence)
		{
			Test.AddError(FString::Printf(
				TEXT("%s: EngineTypeIdSequence before=%d after=%d"),
				Label, Before.EngineTypeIdSequence, After.EngineTypeIdSequence));
			bOk = false;
		}
		CheckUInt(TEXT("EngineTypeIdMap"), Before.EngineTypeIdMap, After.EngineTypeIdMap);
		CheckUInt(TEXT("FunctionBehavioursInternalRefs"), Before.FunctionBehavioursInternalRefs, After.FunctionBehavioursInternalRefs);
		if (Before.Publisher != After.Publisher)
		{
			Test.AddError(FString::Printf(
				TEXT("%s: publisher before=%d after=%d"),
				Label, static_cast<int32>(Before.Publisher), static_cast<int32>(After.Publisher)));
			bOk = false;
		}
		if (Before.HasCanonicalDigest != After.HasCanonicalDigest
			|| Before.CanonicalDigest != After.CanonicalDigest)
		{
			Test.AddError(FString::Printf(
				TEXT("%s: canonical digest before=%d/%llu after=%d/%llu"),
				Label,
				Before.HasCanonicalDigest ? 1 : 0,
				static_cast<unsigned long long>(Before.CanonicalDigest),
				After.HasCanonicalDigest ? 1 : 0,
				static_cast<unsigned long long>(After.CanonicalDigest)));
			bOk = false;
		}
		if (Before.HasASTSnapshot != After.HasASTSnapshot
			|| Before.ASTSnapshotCurrent != After.ASTSnapshotCurrent
			|| Before.ASTGenerationKey != After.ASTGenerationKey)
		{
			Test.AddError(FString::Printf(
				TEXT("%s: AST snapshot before=%d/%d/%s after=%d/%d/%s"),
				Label,
				Before.HasASTSnapshot ? 1 : 0,
				Before.ASTSnapshotCurrent ? 1 : 0,
				*Before.ASTGenerationKey,
				After.HasASTSnapshot ? 1 : 0,
				After.ASTSnapshotCurrent ? 1 : 0,
				*After.ASTGenerationKey));
			bOk = false;
		}
		CheckUInt(TEXT("RuntimeBindingCount"),
			Before.RuntimeBindingCount, After.RuntimeBindingCount);
		if (Before.RuntimeBindingFingerprint != After.RuntimeBindingFingerprint)
		{
			Test.AddError(FString::Printf(
				TEXT("%s: Runtime binding fingerprint changed\nbefore=%s\nafter=%s"),
				Label,
				*Before.RuntimeBindingFingerprint,
				*After.RuntimeBindingFingerprint));
			bOk = false;
		}
		return bOk;
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTCodeGenTransactionTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.CodeGen.Transaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(EveryRelocationClassFailureKeepsCurrentGenerationUnchanged)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FTxnRelocationPod",
				sizeof(FTxnRelocationPod),
				asOBJ_VALUE | asOBJ_POD
					| asGetTypeTraits<FTxnRelocationPod>()) >= 0
			&& ScriptEngine->RegisterObjectProperty(
				"FTxnRelocationPod",
				"int Value",
				asOFFSET(FTxnRelocationPod, Value)) >= 0
			&& ScriptEngine->RegisterObjectType(
				"FTxnListPatternBox",
				0,
				asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0
			&& ScriptEngine->RegisterObjectProperty(
				"FTxnListPatternBox",
				"int Marker",
				asOFFSET(FTxnListPatternBox, Marker)) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"FTxnListPatternBox",
				asBEHAVE_LIST_FACTORY,
				"FTxnListPatternBox f(int &in) {repeat int}",
				asFUNCTION(TxnListPatternFactory),
				asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterGlobalFunction(
				"void TxnObserveWildcard(?& Value)",
				asFUNCTION(TxnObserveWildcard),
				asCALL_GENERIC) >= 0,
			TEXT("relocation-class transaction fixture must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* const PublicModule = ScriptEngine->GetModule(
			"TxnRelocationClassMatrix", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(PublicModule));
		ASSERT_THAT(AreEqual(0, PublicModule->SetASTRetentionPolicy(
			asAST_RETAIN_SNAPSHOT)));
		const std::string BaselineSource = ASTEST_AS_ANSI(R"AS(
			int BaselineRead(FTxnRelocationPod Value)
			{
				return Value.Value;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, PublicModule->AddScriptSection(
			"TxnRelocationBaseline.as",
			BaselineSource.c_str(),
			static_cast<unsigned int>(BaselineSource.size()))));
		ASSERT_THAT(AreEqual(0, PublicModule->Build(),
			TEXT("generation A must publish before relocation failure injection")));

		asCModule* const Module = static_cast<asCModule*>(PublicModule);
		const FCodeGenTableSnapshot GenerationA =
			CaptureCodeGenTables(ScriptEngine, Module);
		ASSERT_THAT(IsTrue(GenerationA.HasASTSnapshot
			&& GenerationA.ASTSnapshotCurrent
			&& !GenerationA.ASTGenerationKey.IsEmpty()
			&& GenerationA.RuntimeBindingCount > 0,
			TEXT("generation A must expose a current snapshot and non-empty immutable binding view")));

		asCModule* const CandidateParseModule = static_cast<asCModule*>(
			ScriptEngine->GetModule(
				"TxnRelocationClassMatrixCandidate", asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(CandidateParseModule));
		asCBuilder Builder(ScriptEngine, CandidateParseModule);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string CandidateSource = ASTEST_AS_ANSI(R"AS(
			int ExerciseTxnRelocations(FTxnRelocationPod Copy)
			{
				FTxnListPatternBox Box = {2};
				TxnObserveWildcard(Copy);
				return Box.Marker;
			}
			)AS");
		Code.SetCode(
			"TxnRelocationCandidate.as", CandidateSource.c_str(), true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		const int ParseResult = Parser.ParseScript(&Code);
		ASSERT_THAT(AreEqual(0, ParseResult,
			*FString::Printf(
				TEXT("relocation-class candidate must parse; messages:\n%s"),
				*AngelscriptNativeTestSupport::CollectMessages(
					Engine.GetMessages()))));
		Sema.LayoutScriptClassFields();
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("relocation-class candidate must seal")));

		struct FRelocationFailureCase
		{
			asERuntimeTypeRelocationUse Use;
			const TCHAR* Name;
		};
		const FRelocationFailureCase Cases[] =
		{
			{ asTYPE_RELOC_METADATA_ONLY, TEXT("metadata-only") },
			{ asTYPE_RELOC_RUNTIME_TYPE_TARGET, TEXT("runtime-type-target") },
			{ asTYPE_RELOC_PUBLIC_TYPE_ID, TEXT("public-type-id") },
			{ asTYPE_RELOC_PROPERTY, TEXT("property") },
			{ asTYPE_RELOC_FUNCTION, TEXT("function") },
			{ asTYPE_RELOC_LIST_PATTERN_TARGET, TEXT("list-pattern-target") },
		};

		for (const FRelocationFailureCase& Case : Cases)
		{
			const FCodeGenTableSnapshot Before =
				CaptureCodeGenTables(ScriptEngine, Module);
			asCBytecodeCodeGen CodeGen;
			CodeGen.SetTestFailureAtTypeRelocationUse(
				Case.Use, asCONTEXT_NOT_FINISHED);
			const int GenerateResult = CodeGen.Generate(Context, Module);
			const FString FailureDetail = UTF8_TO_TCHAR(
				CodeGen.GetFailureDetail().AddressOf()
					? CodeGen.GetFailureDetail().AddressOf() : "");
			ASSERT_THAT(AreEqual(
				asCONTEXT_NOT_FINISHED,
				GenerateResult,
				*FString::Printf(
					TEXT("%s relocation injection must fail before Commit; actual=%d detail=%s"),
					Case.Name,
					GenerateResult,
					*FailureDetail)));
			ASSERT_THAT(IsTrue(
				CodeGen.WasTestTypeRelocationFailureInjected(),
				*FString::Printf(
					TEXT("%s must name a relocation class actually emitted by the candidate; detail=%s"),
					Case.Name,
					*FailureDetail)));
			ASSERT_THAT(IsTrue(
				AssertCodeGenTablesEqual(
					*TestRunner,
					Case.Name,
					Before,
					CaptureCodeGenTables(ScriptEngine, Module)),
				*FString::Printf(
					TEXT("%s failure must preserve all of generation A"),
					Case.Name)));
		}

		ASSERT_THAT(IsTrue(AssertCodeGenTablesEqual(
			*TestRunner,
			TEXT("complete relocation-class matrix"),
			GenerationA,
			CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("all relocation failures together must preserve generation A")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenUnsealedFailureLeavesTablesUnchanged)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "TxnUnsealed");
		ASSERT_THAT(IsNotNull(Module, TEXT("unsealed transaction test needs a module")));

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCASTContext Open;
		asCSema Sema(nullptr, Open);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("TxnUnsealed");
		const asCQualType IntType = Open.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "F", IntType, asCSourceRange());
		AttachTxnReturnAsBody(Sema, Open, Fn, Sema.ActOnIntegerLiteral(7, asCSourceRange()));

		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION, CodeGen.Generate(Open, Module),
			TEXT("unsealed Generate must fail before install")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("unsealed"), Before, CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("unsealed failure must not mutate functions/globals/funcdefs/types")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenRejectsSealedPublicationMissingCallTargetBeforeMutation)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "TxnMissingCallTarget");
		ASSERT_THAT(IsNotNull(Module, TEXT("sealed-publication transaction test needs a module")));

		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("TxnMissingCallTarget");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTStmtId Return = Context.CreateStmt(asAST_STMT_RETURN, Fn, asCSourceRange());
		const asASTExprId Call = Context.CreateExpr(asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Fn, IntType), TEXT("set function type")));
		ASSERT_THAT(AreEqual(0, Context.SetBody(Fn, Return), TEXT("set function body")));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Return, Call), TEXT("return unresolved CALL")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("ordinary sealing must permit unresolved targets so Sema can retain recoverable diagnostics")));

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, CodeGen.Generate(Context, Module),
			TEXT("sealed publication missing a CALL target must fail before any CodeGen registration")));
		ASSERT_THAT(IsTrue(CodeGen.Failed(), TEXT("publication rejection must set CodeGen failure state")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, CodeGen.GetError(),
			TEXT("CodeGen must preserve the publication verifier error")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("sealed publication missing CALL target"), Before, CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("publication rejection must precede every function/global/funcdef/type mutation")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenRejectsForgedCallArgumentFormalBeforeMutation)
	{
		// Production mutation caught: even if verifier learns the exact formal
		// relation, Generate must run that firewall before registering any
		// function/type/global candidate. A later emitter failure is insufficient.
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "TxnForgedCallArgument");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("forged call-argument transaction test needs a module")));

		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("TxnForgedCallArgument");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Callee = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "Pack");
		const asASTDeclId A = Context.CreateDecl(
			asAST_DECL_PARAM, Callee, asCSourceRange(), "A");
		const asASTDeclId B = Context.CreateDecl(
			asAST_DECL_PARAM, Callee, asCSourceRange(), "B");
		const asASTDeclId Entry = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "Entry");
		const asASTStmtId Return = Context.CreateStmt(
			asAST_STMT_RETURN, Entry, asCSourceRange());
		const asASTExprId ArgA = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId ArgB = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId Call = Context.CreateExpr(
			asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Callee, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(A, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(B, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Entry, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(ArgA, 1)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(ArgB, 2)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Call, ArgB)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Call, ArgA)));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Call, Callee)));
		ASSERT_THAT(AreEqual(0, Context.SetExprCallDispatch(
			Call, asAST_CALL_DISPATCH_DIRECT)));

		asSASTCallArgument WrongB;
		WrongB.expression = ArgB;
		WrongB.formal = A;
		WrongB.formalIndex = 1;
		WrongB.origin = asAST_CALL_ARGUMENT_POSITIONAL;
		WrongB.sourceOrdinal = 1;
		ASSERT_THAT(AreEqual(0, Context.AddCallArgument(Call, WrongB)));
		asSASTCallArgument ExactA;
		ExactA.expression = ArgA;
		ExactA.formal = A;
		ExactA.formalIndex = 0;
		ExactA.origin = asAST_CALL_ARGUMENT_POSITIONAL;
		ExactA.sourceOrdinal = 0;
		ASSERT_THAT(AreEqual(0, Context.AddCallArgument(Call, ExactA)));
		ASSERT_THAT(AreEqual(0, Context.SetBody(Entry, Return)));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Return, Call)));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("ordinary seal must preserve forged facts for publication diagnostics")));

		const FCodeGenTableSnapshot Before =
			CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
			CodeGen.Generate(Context, Module),
			TEXT("forged call formal must fail at the publication firewall")));
		ASSERT_THAT(IsTrue(CodeGen.Failed(),
			TEXT("publication rejection must set CodeGen failure state")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, CodeGen.GetError(),
			TEXT("CodeGen must preserve the verifier category")));
		ASSERT_THAT(IsTrue(AssertCodeGenTablesEqual(
			*TestRunner,
			TEXT("forged call argument formal"),
			Before,
			CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("forged call plan must be rejected before any table mutation")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenRejectsMissingConstructTargetBeforeGlobalRegistration)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "TxnGlobal");
		ASSERT_THAT(IsNotNull(Module, TEXT("global rollback test needs a module")));

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("TxnGlobal");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		Sema.ActOnVarDecl(Tu, "G", IntType, asCSourceRange());
		const asASTDeclId OkFn = Sema.ActOnFunctionDecl(Tu, "F", IntType, asCSourceRange());
		AttachTxnReturnAsBody(Sema, Context, OkFn, Sema.ActOnIntegerLiteral(7, asCSourceRange()));
		const asCQualType Obj = Context.InternNamedType(asAST_TYPE_VALUE_OBJECT, "FValue", 0);
		const asASTDeclId BadFn = Sema.ActOnFunctionDecl(Tu, "Make", Obj, asCSourceRange());
		asCArray<asASTExprId> Args;
		AttachTxnReturnAsBody(Sema, Context, BadFn, Sema.ActOnConstruct(Obj, Args, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("global+unsupported graph should still verify")));

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, CodeGen.Generate(Context, Module),
			TEXT("a sealed construct without its Sema-resolved target must fail at the publication boundary")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("global+missing-construct-target"), Before, CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("publication rejection must precede global/function registration, not merely roll it back")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenFillSignatureFailureAfterGlobalAndPriorFunctionRollsBack)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "TxnSig");
		ASSERT_THAT(IsNotNull(Module, TEXT("signature rollback test needs a module")));

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("TxnSig");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		Sema.ActOnVarDecl(Tu, "G", IntType, asCSourceRange());
		const asASTDeclId OkFn = Sema.ActOnFunctionDecl(Tu, "F", IntType, asCSourceRange());
		AttachTxnReturnAsBody(Sema, Context, OkFn, Sema.ActOnIntegerLiteral(1, asCSourceRange()));
		const asCQualType Missing = Context.InternNamedType(asAST_TYPE_VALUE_OBJECT, "NoSuchType", 0);
		const asASTDeclId BadFn = Sema.ActOnFunctionDecl(Tu, "Bad", IntType, asCSourceRange());
		Sema.ActOnParamDecl(BadFn, "X", Missing, asCSourceRange());
		AttachTxnReturnAsBody(Sema, Context, BadFn, Sema.ActOnIntegerLiteral(0, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("missing-param-type graph should still verify")));

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(IsTrue(CodeGen.Generate(Context, Module) < 0,
			TEXT("FillFunctionSignature failure must fail closed")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("signature"), Before, CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("FillFunctionSignature fail after a global and a prior function must restore engine slots and globals")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenEmitterFailureAfterPriorFunctionLeavesNoTables)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "TxnEmitterFailure");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("emitter-failure transaction test needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class LeakedType
			{
			}

			int First()
			{
				return 7;
			}

			int Second()
			{
				return 1;
			}
			)AS");
		Code.SetCode("TxnEmitterFailure.as", ScriptSource.c_str(), true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("transaction fixture must parse both the supported and unsupported functions")));
		Sema.LayoutScriptClassFields();
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("transaction fixture must reach sealed canonical CodeGen rather than fail in parsing")));
		asCString AstDump;
		asCASTDump(Context, AstDump);
		const FString AstText = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(AstDump);
		ASSERT_THAT(IsTrue(AstText.Contains(TEXT("kind=Class name=LeakedType")),
			TEXT("the pre-CodeGen gate must retain the source class in the sealed AST")));
		ASSERT_THAT(IsTrue(AstText.Contains(TEXT("key=First()")),
			TEXT("the pre-CodeGen gate must retain the first emitted function in the sealed AST")));
		ASSERT_THAT(IsTrue(AstText.Contains(TEXT("key=Second()")),
			TEXT("the pre-CodeGen gate must retain the function at the injected emitter boundary")));

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		CodeGen.SetTestFailureBeforeFunctionEmission(1, asCONTEXT_NOT_FINISHED);
		ASSERT_THAT(AreEqual(asCONTEXT_NOT_FINISHED, CodeGen.Generate(Context, Module),
			TEXT("the injected second-function boundary must fail during canonical CodeGen")));
		ASSERT_THAT(AreEqual(asCONTEXT_NOT_FINISHED, CodeGen.GetError(),
			TEXT("the failure must be the explicit second-function emission boundary, not an earlier validation rejection")));
		ASSERT_THAT(IsTrue(CodeGen.GetLastFailLine() > 0,
			TEXT("post-seal CodeGen failure must identify an emitter failure location")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(CodeGen.GetFailureDetail()).Contains(TEXT("injected-function-emission")),
			TEXT("post-seal injected CodeGen failure must retain a stable diagnostic category")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("emitter-after-prior-function"), Before,
				CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("a later emitter failure must remove all function/global/type publication state from earlier declarations")));
		ASSERT_THAT(IsNull(ScriptEngine->GetTypeInfoByName("LeakedType"),
			TEXT("a failed canonical CodeGen attempt must remove its provisional type from the engine lookup table")));

		asCScriptCode RetryCode;
		const std::string RetrySource = ASTEST_AS_ANSI(R"AS(
			class LeakedType
			{
			}

			int Retry()
			{
				return 9;
			}
			)AS");
		RetryCode.SetCode("TxnEmitterFailureRetry.as", RetrySource.c_str(), true);
		asCASTContext RetryContext;
		asCSema RetrySema(ScriptEngine, RetryContext);
		asCBuilder RetryBuilder(ScriptEngine, Module);
		RetryBuilder.silent = true;
		asCParser RetryParser(&RetryBuilder);
		RetryParser.SetSema(&RetrySema);
		ASSERT_THAT(AreEqual(0, RetryParser.ParseScript(&RetryCode),
			TEXT("a retry after the failed type installation must parse")));
		RetrySema.LayoutScriptClassFields();
		ASSERT_THAT(AreEqual(0, RetryContext.Seal(),
			TEXT("a retry after the failed type installation must seal")));
		asCBytecodeCodeGen RetryCodeGen;
		ASSERT_THAT(AreEqual(0, RetryCodeGen.Generate(RetryContext, Module),
			TEXT("a retry must publish the same type name after the failed attempt was fully rolled back")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByName("LeakedType"),
			TEXT("the successful retry must own a fresh LeakedType declaration")));
		Module->Discard();
		ASSERT_THAT(IsNull(ScriptEngine->GetTypeInfoByName("LeakedType"),
			TEXT("discarding a successfully committed Canonical type must remove its engine lookup entry")));
	}

	TEST_METHOD(CodeGenRejectsMissingConstructTargetBeforeFuncPtrFixtureMutation)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterFuncdef("int Callback(int)") >= 0,
			TEXT("funcdef-store rollback needs host Callback")));
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "TxnFuncPtr");
		ASSERT_THAT(IsNotNull(Module, TEXT("funcdef-store rollback needs a module")));

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Double(int X)
			{
				return X + X;
			}

			int Store()
			{
				Callback L = Double;
				return L(1);
			}
			)AS");
		Code.SetCode("TxnFuncPtr", ScriptSource.c_str(), true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code), TEXT("Double+Store should parse")));

		const asASTDeclId Tu = Context.GetTranslationUnit();
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCQualType Obj = Context.InternNamedType(asAST_TYPE_VALUE_OBJECT, "FValue", 0);
		const asASTDeclId BadFn = Sema.ActOnFunctionDecl(Tu, "Make", IntType, asCSourceRange());
		asCArray<asASTExprId> Args;
		AttachTxnReturnAsBody(Sema, Context, BadFn, Sema.ActOnConstruct(Obj, Args, asCSourceRange()));
		const int SealResult = Context.Seal();
		if (SealResult != 0)
		{
			asCString Dump;
			asCASTDump(Context, Dump);
			const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
			ASSERT_THAT(AreEqual(0, SealResult,
				*FString::Printf(TEXT("funcptr store + unsupported Make should still verify. dump:\n%s"), *Text)));
		}

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, CodeGen.Generate(Context, Module),
			TEXT("FuncPtr fixture with an unresolved construct must fail at publication before lowering")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("funcptr+missing-construct-target"), Before, CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("publication rejection must leave FuncPtr/REFCPY fixture tables and references untouched")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenEmitterFailureAfterImportLeavesNoImportSlots)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "TxnImportRollback");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("import rollback transaction test needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		auto ParseAndSeal = [&](const char* Filename, const std::string& Source, asCASTContext& OutContext)
		{
			asCBuilder Builder(ScriptEngine, Module);
			Builder.silent = true;
			asCScriptCode Code;
			Code.SetCode(Filename, Source.c_str(), true);
			asCSema Sema(ScriptEngine, OutContext);
			asCParser Parser(&Builder);
			Parser.SetSema(&Sema);
			const int ParseResult = Parser.ParseScript(&Code);
			const int SealResult = ParseResult == 0 ? OutContext.Seal() : ParseResult;
			return SealResult;
		};

		asCASTContext Context;
		const std::string FailingSource = ASTEST_AS_ANSI(R"AS(
			import int ImportedValue() from "TxnImportProvider";

			int First()
			{
				return 7;
			}

			int Second()
			{
				return 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, ParseAndSeal("TxnImportRollback.as", FailingSource, Context),
			TEXT("import fixture must reach the injected emitter boundary rather than fail while parsing/sealing")));
		asCString AstDump;
		asCASTDump(Context, AstDump);
		const FString AstText = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(AstDump);
		ASSERT_THAT(IsTrue(AstText.Contains(TEXT("kind=Import name=ImportedValue")),
			TEXT("the pre-CodeGen gate must retain the import declaration in the sealed AST")));
		ASSERT_THAT(IsTrue(AstText.Contains(TEXT("key=First()")) && AstText.Contains(TEXT("key=Second()")),
			TEXT("the pre-CodeGen gate must retain both functions around the injected emitter boundary")));

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		CodeGen.SetTestFailureBeforeFunctionEmission(1, asCONTEXT_NOT_FINISHED);
		ASSERT_THAT(AreEqual(asCONTEXT_NOT_FINISHED, CodeGen.Generate(Context, Module),
			TEXT("the later injected emitter failure must occur after DECL_IMPORT has prepared its slot")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(CodeGen.GetFailureDetail()).Contains(TEXT("injected-function-emission")),
			TEXT("the import rollback failure must identify the explicit emitter injection")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("import+emitter-failure"), Before,
				CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("a failed canonical Generate must remove its module and engine import-slot publication")));

		asCASTContext RetryContext;
		const std::string RetrySource = ASTEST_AS_ANSI(R"AS(
			import int ImportedValue() from "TxnImportProvider";

			int Retry()
			{
				return 9;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, ParseAndSeal("TxnImportRollbackRetry.as", RetrySource, RetryContext),
			TEXT("a retry after import rollback must parse and seal")));
		asCBytecodeCodeGen RetryCodeGen;
		ASSERT_THAT(AreEqual(0, RetryCodeGen.Generate(RetryContext, Module),
			TEXT("a retry after the failed import installation must generate")));
		ASSERT_THAT(AreEqual(1u, Module->GetImportedFunctionCount(),
			TEXT("the successful retry must own exactly one fresh import slot, not retain the failed attempt")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenEmitterFailureAfterClassMethodsLeavesNoDanglingTypeFunctionIds)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "TxnClassMethodRollback");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("class-method rollback transaction test needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class LeakedMethodType
			{
				int FirstMethod()
				{
					return 7;
				}

				int SecondMethod()
				{
					return 1;
				}
			}
			)AS");
		Code.SetCode("TxnClassMethodRollback.as", ScriptSource.c_str(), true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("class-method emitter-failure fixture must parse")));
		Sema.LayoutScriptClassFields();
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("class-method emitter-failure fixture must seal")));
		asCString AstDump;
		asCASTDump(Context, AstDump);
		const FString AstText = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(AstDump);
		ASSERT_THAT(IsTrue(AstText.Contains(TEXT("kind=Class name=LeakedMethodType")),
			TEXT("the pre-CodeGen gate must retain the class owner in the sealed AST")));
		ASSERT_THAT(IsTrue(AstText.Contains(TEXT("key=LeakedMethodType::FirstMethod()"))
			&& AstText.Contains(TEXT("key=LeakedMethodType::SecondMethod()")),
			TEXT("the pre-CodeGen gate must retain both class methods around the injected boundary")));

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		CodeGen.SetTestFailureBeforeFunctionEmission(1, asCONTEXT_NOT_FINISHED);
		ASSERT_THAT(AreEqual(asCONTEXT_NOT_FINISHED, CodeGen.Generate(Context, Module),
			TEXT("the class method injected boundary must fail in the later emitter path")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(CodeGen.GetFailureDetail()).Contains(TEXT("injected-function-emission")),
			TEXT("the class-method rollback failure must identify the explicit emitter injection")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("class-method+emitter-failure"), Before,
				CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("class-method failure must not leave dangling method or behaviour ids during type rollback")));
		ASSERT_THAT(IsNull(ScriptEngine->GetTypeInfoByName("LeakedMethodType"),
			TEXT("a failed class-method generation must remove its provisional type")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenCommitFailureAfterFunctionPublicationRollsBackAllIndexes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "TxnCommitFunctionPublication");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("commit-publication rollback test needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class CommitType
			{
			}

			int First()
			{
				return 7;
			}

			int Second()
			{
				return 1;
			}
			)AS");
		Code.SetCode("TxnCommitFunctionPublication.as", ScriptSource.c_str(), true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("commit-publication fixture must parse from source")));
		Sema.LayoutScriptClassFields();
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("commit-publication fixture must seal before CodeGen")));
		asCString AstDump;
		asCASTDump(Context, AstDump);
		const FString AstText = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(AstDump);
		ASSERT_THAT(IsTrue(AstText.Contains(TEXT("kind=Class name=CommitType"))
			&& AstText.Contains(TEXT("key=First()"))
			&& AstText.Contains(TEXT("key=Second()")),
			TEXT("the AST-first gate must retain the source type and both functions before Commit publication")));

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		CodeGen.SetTestFailureAfterCommitStage(
			asBYTECODE_CODEGEN_TEST_COMMIT_AFTER_FUNCTIONS,
			asCONTEXT_NOT_FINISHED);
		ASSERT_THAT(AreEqual(asCONTEXT_NOT_FINISHED, CodeGen.Generate(Context, Module),
			TEXT("the injected Commit function-publication boundary must fail")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpToFString(CodeGen.GetFailureDetail()).Contains(TEXT("injected-commit-publication")),
			TEXT("the Commit failure must identify its deterministic publication stage")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("commit-after-functions"), Before,
				CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("a failure after function publication must roll back module, allocator, and all automatic-import indexes")));
		ASSERT_THAT(IsNull(ScriptEngine->GetTypeInfoByName("CommitType"),
			TEXT("a failed Commit must remove the provisional script type from engine lookup")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenCommitFailureAfterClassMethodPublicationRollsBackAllIndexes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "TxnCommitClassMethodPublication");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("class-method Commit rollback test needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class CommitMethodType
			{
				int FirstMethod()
				{
					return 7;
				}

				int SecondMethod()
				{
					return 1;
				}
			}
			)AS");
		Code.SetCode("TxnCommitClassMethodPublication.as", ScriptSource.c_str(), true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("class-method Commit fixture must parse from source")));
		Sema.LayoutScriptClassFields();
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("class-method Commit fixture must seal before CodeGen")));
		asCString AstDump;
		asCASTDump(Context, AstDump);
		const FString AstText = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(AstDump);
		ASSERT_THAT(IsTrue(AstText.Contains(TEXT("kind=Class name=CommitMethodType"))
			&& AstText.Contains(TEXT("key=CommitMethodType::FirstMethod()"))
			&& AstText.Contains(TEXT("key=CommitMethodType::SecondMethod()")),
			TEXT("the AST-first gate must retain the class owner and methods before their Commit publication")));

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		CodeGen.SetTestFailureAfterCommitStage(
			asBYTECODE_CODEGEN_TEST_COMMIT_AFTER_FUNCTIONS,
			asCONTEXT_NOT_FINISHED);
		ASSERT_THAT(AreEqual(asCONTEXT_NOT_FINISHED, CodeGen.Generate(Context, Module),
			TEXT("the injected Commit class-method publication boundary must fail")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("commit-after-class-methods"), Before,
				CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("a failure after class-method publication must remove method tables before type/function teardown")));
		ASSERT_THAT(IsNull(ScriptEngine->GetTypeInfoByName("CommitMethodType"),
			TEXT("a failed class-method Commit must remove the provisional type from engine lookup")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenCommitFailureAfterListPatternRelocationRestoresAnonymousHelperCache)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FTxnListPatternBox",
				0,
				asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0
			&& ScriptEngine->RegisterObjectProperty(
				"FTxnListPatternBox",
				"int Marker",
				asOFFSET(FTxnListPatternBox, Marker)) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"FTxnListPatternBox",
				asBEHAVE_LIST_FACTORY,
				"FTxnListPatternBox f(int &in) {repeat int}",
				asFUNCTION(TxnListPatternFactory),
				asCALL_GENERIC) >= 0,
			TEXT("list-pattern rollback fixture must register its exact Runtime owner and factory")));

		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "TxnListPatternRelocation");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("list-pattern relocation rollback test needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int ReadListPattern()
			{
				FTxnListPatternBox Box = {41};
				return Box.Marker;
			}
			)AS");
		Code.SetCode("TxnListPatternRelocation.as", ScriptSource.c_str(), true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("list-pattern relocation fixture must parse from authored source")));
		Sema.LayoutScriptClassFields();
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("list-pattern relocation fixture must seal before CodeGen")));

		asCString AstDump;
		asCASTDump(Context, AstDump);
		const FString AstText = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(AstDump);
		ASSERT_THAT(IsTrue(
			AstText.Contains(TEXT("key=ReadListPattern()"))
			&& AstText.Contains(TEXT("FTxnListPatternBox")),
			TEXT("the AST-first gate must retain the exact list owner and source function")));

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		CodeGen.SetTestFailureAfterCommitStage(
			asBYTECODE_CODEGEN_TEST_COMMIT_AFTER_FUNCTIONS,
			asCONTEXT_NOT_FINISHED);
		ASSERT_THAT(AreEqual(asCONTEXT_NOT_FINISHED, CodeGen.Generate(Context, Module),
			TEXT("the injected function-publication boundary must fail after list relocation")));
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalAstDumpToFString(
				CodeGen.GetFailureDetail()).Contains(TEXT("injected-commit-publication")),
			TEXT("the list-pattern rollback must reach Commit after relocation patching")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(
				*TestRunner,
				TEXT("commit-after-list-pattern-relocation"),
				Before,
				CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("failed publication must restore the anonymous list-pattern helper cache and every visible table")));
		ASSERT_THAT(AreEqual(Before.EngineListPatternTypes,
			ScriptEngine->listPatternTypes.GetLength(),
			TEXT("anonymous list-pattern helpers are candidate resources, not durable canonical types")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenCommitFailureAfterTypePublicationRollsBackAllIndexes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "TxnCommitTypePublication");
		ASSERT_THAT(IsNotNull(Module, TEXT("type-publication rollback test needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class CommitTypeStage
			{
			}

			int StageFunction()
			{
				return 7;
			}
			)AS");
		Code.SetCode("TxnCommitTypePublication.as", ScriptSource.c_str(), true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("type-publication fixture must parse from source")));
		Sema.LayoutScriptClassFields();
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("type-publication fixture must seal before CodeGen")));
		asCString AstDump;
		asCASTDump(Context, AstDump);
		const FString AstText = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(AstDump);
		ASSERT_THAT(IsTrue(AstText.Contains(TEXT("kind=Class name=CommitTypeStage"))
			&& AstText.Contains(TEXT("key=StageFunction()")),
			TEXT("the AST-first gate must retain the source type and trigger function before type publication")));

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		CodeGen.SetTestFailureAfterCommitStage(
			asBYTECODE_CODEGEN_TEST_COMMIT_AFTER_TYPES,
			asCONTEXT_NOT_FINISHED);
		ASSERT_THAT(AreEqual(asCONTEXT_NOT_FINISHED, CodeGen.Generate(Context, Module),
			TEXT("the injected Commit type-publication boundary must fail")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("commit-after-types"), Before,
				CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("a failure after type publication must remove module and engine type indexes")));
		ASSERT_THAT(IsNull(ScriptEngine->GetTypeInfoByName("CommitTypeStage"),
			TEXT("a failed type publication must remove the provisional type from engine lookup")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenCommitFailureAfterGlobalPublicationRollsBackAllIndexes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "TxnCommitGlobalPublication");
		ASSERT_THAT(IsNotNull(Module, TEXT("global-publication rollback test needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int __StaticType_CommitGlobal;

			int StageFunction()
			{
				return 7;
			}
			)AS");
		Code.SetCode("TxnCommitGlobalPublication.as", ScriptSource.c_str(), true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("global-publication fixture must parse from source")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("global-publication fixture must seal before CodeGen")));
		asCString AstDump;
		asCASTDump(Context, AstDump);
		const FString AstText = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(AstDump);
		ASSERT_THAT(IsTrue(AstText.Contains(TEXT("__StaticType_CommitGlobal"))
			&& AstText.Contains(TEXT("key=StageFunction()")),
			TEXT("the AST-first gate must retain the source global and trigger function before global publication")));

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		CodeGen.SetTestFailureAfterCommitStage(
			asBYTECODE_CODEGEN_TEST_COMMIT_AFTER_GLOBALS,
			asCONTEXT_NOT_FINISHED);
		ASSERT_THAT(AreEqual(asCONTEXT_NOT_FINISHED, CodeGen.Generate(Context, Module),
			TEXT("the injected Commit global-publication boundary must fail")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("commit-after-globals"), Before,
				CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("a failure after global publication must remove module, address-map, and automatic-import global indexes")));
		ASSERT_THAT(AreEqual(0u, Module->GetGlobalVarCount(),
			TEXT("a failed global publication must leave no module global property")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenCommitFailureAfterImportPublicationRollsBackAllIndexes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "TxnCommitImportPublication");
		ASSERT_THAT(IsNotNull(Module, TEXT("import-publication rollback test needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			import int CommitImportedValue() from "TxnCommitProvider";

			int StageFunction()
			{
				return 7;
			}
			)AS");
		Code.SetCode("TxnCommitImportPublication.as", ScriptSource.c_str(), true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("import-publication fixture must parse from source")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("import-publication fixture must seal before CodeGen")));
		asCString AstDump;
		asCASTDump(Context, AstDump);
		const FString AstText = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(AstDump);
		ASSERT_THAT(IsTrue(AstText.Contains(TEXT("kind=Import name=CommitImportedValue"))
			&& AstText.Contains(TEXT("key=StageFunction()")),
			TEXT("the AST-first gate must retain the source import and trigger function before import publication")));

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		CodeGen.SetTestFailureAfterCommitStage(
			asBYTECODE_CODEGEN_TEST_COMMIT_AFTER_IMPORTS,
			asCONTEXT_NOT_FINISHED);
		ASSERT_THAT(AreEqual(asCONTEXT_NOT_FINISHED, CodeGen.Generate(Context, Module),
			TEXT("the injected Commit import-publication boundary must fail")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("commit-after-imports"), Before,
				CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("a failure after import publication must remove module and engine import slots")));
		ASSERT_THAT(AreEqual(0u, Module->GetImportedFunctionCount(),
			TEXT("a failed import publication must leave no module import binding")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenRejectsDuplicateClassNamesBeforePartialTypeRegistration)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "TxnDuplicateClass");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("duplicate-class transaction test needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("TxnDuplicateClass");
		ASSERT_THAT(IsTrue(Context.CreateDecl(asAST_DECL_CLASS, Tu, asCSourceRange(), "DuplicateType").IsValid(),
			TEXT("first duplicate class declaration must be constructible for the CodeGen coverage fixture")));
		ASSERT_THAT(IsTrue(Context.CreateDecl(asAST_DECL_CLASS, Tu, asCSourceRange(), "DuplicateType").IsValid(),
			TEXT("second duplicate class declaration must reach CodeGen rather than be hidden by a parser-only oracle")));
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "F", IntType, asCSourceRange());
		AttachTxnReturnAsBody(Sema, Context, Fn, Sema.ActOnIntegerLiteral(7, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("duplicate-class coverage fixture must reach the CodeGen declaration-install boundary")));

		const FCodeGenTableSnapshot Before = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(asINVALID_DECLARATION, CodeGen.Generate(Context, Module),
			TEXT("duplicate script class names must fail closed before a later declaration is silently skipped")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("duplicate-class"), Before,
				CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("duplicate class rejection must not leave the first declaration registered")));
		ASSERT_THAT(IsNull(ScriptEngine->GetTypeInfoByName("DuplicateType"),
			TEXT("a rejected duplicate class must not publish a partial first type")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenRetryAfterPublicationRejectionSeesNoLeftoverIds)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
			TEXT("isolated retry transaction explicitly selects the LEGACY comparison pipeline")));
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "TxnRetry");
		ASSERT_THAT(IsNotNull(Module, TEXT("retry test needs a module")));

		auto SealUnsupportedWithGlobal = [&](asCASTContext& Context)
		{
			asCSema Sema(ScriptEngine, Context);
			const asASTDeclId Tu = Sema.ActOnTranslationUnit("TxnRetry");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			Sema.ActOnVarDecl(Tu, "G", IntType, asCSourceRange());
			const asASTDeclId OkFn = Sema.ActOnFunctionDecl(Tu, "F", IntType, asCSourceRange());
			AttachTxnReturnAsBody(Sema, Context, OkFn, Sema.ActOnIntegerLiteral(7, asCSourceRange()));
			const asCQualType Obj = Context.InternNamedType(asAST_TYPE_VALUE_OBJECT, "FValue", 0);
			const asASTDeclId BadFn = Sema.ActOnFunctionDecl(Tu, "Make", IntType, asCSourceRange());
			asCArray<asASTExprId> Args;
			AttachTxnReturnAsBody(Sema, Context, BadFn, Sema.ActOnConstruct(Obj, Args, asCSourceRange()));
			return Context.Seal();
		};

		asCASTContext First;
		ASSERT_THAT(AreEqual(0, SealUnsupportedWithGlobal(First), TEXT("first unsupported graph should seal")));
		const FCodeGenTableSnapshot Baseline = CaptureCodeGenTables(ScriptEngine, Module);
		asCBytecodeCodeGen FirstGen;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, FirstGen.Generate(First, Module),
			TEXT("first Generate must reject the unresolved construct at publication")));
		ASSERT_THAT(IsTrue(
			AssertCodeGenTablesEqual(*TestRunner, TEXT("retry-after-fail"), Baseline, CaptureCodeGenTables(ScriptEngine, Module)),
			TEXT("publication rejection must leave the module retry-safe")));

		asCASTContext Success;
		asCSema SuccessSema(ScriptEngine, Success);
		const asASTDeclId Tu = SuccessSema.ActOnTranslationUnit("TxnRetry");
		const asCQualType IntType = Success.InternPrimitive(ttInt, 0);
		const asCSourceRange FunctionRange = AddTxnAuthoredFunctionSource(
			Success, "TxnRetry/F.as");
		const asASTDeclId Fn = SuccessSema.ActOnFunctionDecl(Tu, "F", IntType, FunctionRange);
		AttachTxnReturnAsBody(SuccessSema, Success, Fn, SuccessSema.ActOnIntegerLiteral(7, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Success.Seal(), TEXT("retry success graph should seal")));
		asCBytecodeCodeGen SecondGen;
		const int RetryResult = SecondGen.Generate(Success, Module);
		ASSERT_THAT(AreEqual(0, RetryResult,
			*FString::Printf(
				TEXT("retry Generate on the same module must succeed without leftover names/ids: result=%d detail=%s"),
				RetryResult,
				*AngelscriptNativeTestSupport::CanonicalAstDumpToFString(
					SecondGen.GetFailureDetail()))));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 7,
			TEXT("retry success must execute F()==7")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Module->GetLastBytecodePublisher(),
			TEXT("isolated success still records CodeGen publisher; Ready must stay false until production Build")));
		ASSERT_THAT(IsFalse(ScriptEngine->IsCanonicalBytecodeCodeGenReady(),
			TEXT("isolated Generate must not make a LEGACY-selected Engine advertise canonical readiness")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenPublishesUninitializedGlobalWithDefaultValue)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
			TEXT("isolated global transaction explicitly selects the LEGACY comparison pipeline")));
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "TxnGlobalOnly");
		ASSERT_THAT(IsNotNull(Module, TEXT("global-only publication needs a module")));

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("TxnGlobalOnly");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		Sema.ActOnVarDecl(Tu, "G", IntType, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("TU with only a global should seal")));

		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(0, CodeGen.Generate(Context, Module),
			TEXT("an uninitialized Canonical global must publish rather than become a no-op success")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("global-only Canonical CodeGen must install the declared global")));
		const int GlobalIndex = Module->GetGlobalVarIndexByName("G");
		ASSERT_THAT(AreEqual(0, GlobalIndex, TEXT("published global must retain its declared name")));
		int32* const GlobalValue = static_cast<int32*>(Module->GetAddressOfGlobalVar(static_cast<asUINT>(GlobalIndex)));
		ASSERT_THAT(IsNotNull(GlobalValue, TEXT("published global must own storage")));
		ASSERT_THAT(AreEqual(0, *GlobalValue,
			TEXT("uninitialized int global must use AngelScript's default zero value")));
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN, Module->GetLastBytecodePublisher(),
			TEXT("published global must record Canonical CodeGen as its publisher")));
		ASSERT_THAT(IsFalse(ScriptEngine->IsCanonicalBytecodeCodeGenReady(),
			TEXT("isolated Generate must not make a LEGACY-selected Engine advertise readiness")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenSuccessDoesNotExtraRefPublishedFunction)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
			TEXT("isolated refcount transaction explicitly selects the LEGACY comparison pipeline")));
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "TxnCommitRef");
		ASSERT_THAT(IsNotNull(Module, TEXT("commit-refcount test needs a module")));

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("TxnCommitRef");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCSourceRange FunctionRange = AddTxnAuthoredFunctionSource(
			Context, "TxnCommitRef/F.as");
		const asASTDeclId Fn = Sema.ActOnFunctionDecl(Tu, "F", IntType, FunctionRange);
		AttachTxnReturnAsBody(Sema, Context, Fn, Sema.ActOnIntegerLiteral(7, asCSourceRange()));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("integer F graph should seal")));

		asCBytecodeCodeGen CodeGen;
		const int GenerateResult = CodeGen.Generate(Context, Module);
		ASSERT_THAT(AreEqual(0, GenerateResult,
			*FString::Printf(
				TEXT("integer F must Generate: result=%d detail=%s"),
				GenerateResult,
				*AngelscriptNativeTestSupport::CanonicalAstDumpToFString(
					CodeGen.GetFailureDetail()))));
		asIScriptFunction* const Published = Module->GetFunctionByDecl("int F()");
		ASSERT_THAT(IsNotNull(Published, TEXT("commit must publish int F()")));
		asCScriptFunction* const Func = static_cast<asCScriptFunction*>(Published);
		ASSERT_THAT(AreEqual(1, (int)Func->internalRefCount.get(),
			TEXT("legacy new-global: ctor internal 1, AddScriptFunction is a table pointer, PushLast must not extra AddRefInternal")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 7,
			TEXT("commit without extra ref must still execute F()==7")));
		ASSERT_THAT(IsFalse(ScriptEngine->IsCanonicalBytecodeCodeGenReady(),
			TEXT("isolated Generate must not make a LEGACY-selected Engine advertise readiness")));
		Module->Discard();
	}

	TEST_METHOD(CodeGenCommitPublishesScriptTypeIdProjectionAtomically)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
			TEXT("isolated type-id transaction explicitly selects the LEGACY comparison pipeline")));
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "TxnTypeIdCommit");
		ASSERT_THAT(IsNotNull(Module, TEXT("type-id commit test needs a module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class TypeIdCommitType
			{
			}
			)AS");
		Code.SetCode("TxnTypeIdCommit.as", ScriptSource.c_str(), true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("type-id commit fixture must parse from source")));
		Sema.LayoutScriptClassFields();
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("type-id commit fixture must seal before CodeGen")));

		const int SequenceBefore = ScriptEngine->typeIdSeqNbr;
		const asUINT MapSizeBefore = static_cast<asUINT>(
			ScriptEngine->mapTypeIdToTypeInfo.Num());
		asCBytecodeCodeGen CodeGen;
		ASSERT_THAT(AreEqual(asSUCCESS, CodeGen.Generate(Context, Module),
			TEXT("type-id projection must publish with the successful artifact")));

		asCTypeInfo* const PublishedType = static_cast<asCTypeInfo*>(
			Module->GetTypeInfoByName("TypeIdCommitType"));
		ASSERT_THAT(IsNotNull(PublishedType,
			TEXT("successful Commit must publish the script type")));
		const int PublicTypeId = Module->GetTypeIdByDecl("TypeIdCommitType");
		ASSERT_THAT(IsTrue(PublishedType != nullptr && PublicTypeId > 0,
			TEXT("published script type must expose a current numeric projection")));
		const int BaseTypeId = PublicTypeId
			& (asTYPEID_MASK_OBJECT | asTYPEID_MASK_SEQNBR);
		ASSERT_THAT(IsTrue(PublishedType != nullptr
			&& PublishedType->typeId == BaseTypeId,
			TEXT("the type object and public declaration projection must agree on the committed base id")));
		ASSERT_THAT(AreEqual(SequenceBefore,
			BaseTypeId & asTYPEID_MASK_SEQNBR,
			TEXT("the committed base id must use the candidate-planned sequence value")));
		ASSERT_THAT(AreEqual(SequenceBefore + 1, ScriptEngine->typeIdSeqNbr,
			TEXT("one committed script type must consume exactly one sequence value")));
		ASSERT_THAT(AreEqual(MapSizeBefore + 1,
			static_cast<asUINT>(ScriptEngine->mapTypeIdToTypeInfo.Num()),
			TEXT("one committed script type must add exactly one type-id map entry")));
		ASSERT_THAT(IsTrue(PublishedType != nullptr
			&& ScriptEngine->mapTypeIdToTypeInfo.FindRef(BaseTypeId) == PublishedType,
			TEXT("the Engine type-id map must point at the current committed type object")));
		const asCDataType RoundTrip = ScriptEngine->GetDataTypeFromTypeId(PublicTypeId);
		ASSERT_THAT(IsTrue(RoundTrip.IsValid()
			&& RoundTrip.GetTypeInfo() == PublishedType,
			TEXT("the committed public type id must round-trip to the current TypeInfo")));
		Module->Discard();
	}
};

#endif
