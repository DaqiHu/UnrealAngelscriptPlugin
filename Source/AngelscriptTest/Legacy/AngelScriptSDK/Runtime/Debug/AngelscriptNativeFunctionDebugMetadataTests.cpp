#include "../../Support/AngelscriptNativeDebugTestSupport.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_public_view.h"
#include "source/as_bytecode_codegen.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeFunctionDebugMetadataTests,
	"Angelscript.TestModule.AngelScriptSDK.NativeDebug.FunctionDebugMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:

	static FString BuildSource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("namespace NativeDebugMetadata"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Inspectable(int Parameter)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tint Local = Parameter + 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Local;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunMetadataEntry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn NativeDebugMetadata::Inspectable(41);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("void MetadataNoop()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildImportedProviderSource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int MetadataImportedValue()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 73;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildImportedConsumerSource(const FString& ProviderName)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("import int MetadataImportedValue() from \"%s\";"), *ProviderName));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunImportedMetadata()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn MetadataImportedValue();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static int NativeSystemMetadata()
	{
		return 61;
	}

	static void ReportGeneratedModule(FAutomationTestBase& Test, const FString& ModuleName, const FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(Test, TEXT("DBG-FUNCTION-METADATA"), ModuleName, Source);
	}

	static bool HasNamedVariable(asIScriptFunction* Function, const ANSICHAR* ExpectedName, const ANSICHAR* ExpectedDeclaration)
	{
		if (Function == nullptr || ExpectedName == nullptr || ExpectedDeclaration == nullptr)
		{
			return false;
		}

		for (asUINT VariableIndex = 0; VariableIndex < Function->GetVarCount(); ++VariableIndex)
		{
			const char* Name = nullptr;
			if (Function->GetVar(VariableIndex, &Name) < 0 || Name == nullptr)
			{
				continue;
			}
			const char* const Declaration = Function->GetVarDecl(VariableIndex, true);
			if (FCStringAnsi::Strcmp(Name, ExpectedName) == 0
				&& Declaration != nullptr
				&& FCStringAnsi::Strcmp(Declaration, ExpectedDeclaration) == 0)
			{
				return true;
			}
		}
		return false;
	}

public:
	TEST_METHOD(FunctionsByQueryLifecycleAndOptimization)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("DBG-FUNCTION-METADATA",
			ENativeEvidence::Metadata
			| ENativeEvidence::Debug
			| ENativeEvidence::Runtime
			| ENativeEvidence::Bytecode);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		FScopedNativeDebugCallbacks DebugCallbacks;
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Function debug metadata product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString ModuleName = TEXT("NativeDebugFunctionMetadata");
		const FString Source = BuildSource();
		const FString ImportProviderName = TEXT("NativeDebugFunctionMetadataProvider");
		const FString ImportConsumerName = TEXT("NativeDebugFunctionMetadataConsumer");
		const FString RebuiltModuleName = TEXT("NativeDebugFunctionMetadataRebuilt");
		const FString LoadedModuleName = TEXT("NativeDebugFunctionMetadataLoaded");
		const FString StrippedModuleName = TEXT("NativeDebugFunctionMetadataStripped");
		const FString ImportProviderSource = BuildImportedProviderSource();
		const FString ImportConsumerSource = BuildImportedConsumerSource(ImportProviderName);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		const FTCHARToUTF8 ImportProviderNameUtf8(*ImportProviderName);
		const FTCHARToUTF8 ImportConsumerNameUtf8(*ImportConsumerName);
		const int SystemRegistrationResult = ScriptEngine->RegisterGlobalFunction(
			"int NativeSystemMetadata()", asFUNCTION(NativeSystemMetadata), asCALL_CDECL);
		ASSERT_THAT(IsTrue(SystemRegistrationResult >= 0,
			*FString::Printf(TEXT("Function debug metadata should register the system-function query target. Result=%d Messages={%s}"),
				SystemRegistrationResult, *Engine.GetMessagesText())));
		const FTCHARToUTF8 RebuiltModuleNameUtf8(*RebuiltModuleName);
		const FTCHARToUTF8 LoadedModuleNameUtf8(*LoadedModuleName);
		const FTCHARToUTF8 StrippedModuleNameUtf8(*StrippedModuleName);
		const FTCHARToUTF8 ImportProviderSourceUtf8(*ImportProviderSource);
		const FTCHARToUTF8 ImportConsumerSourceUtf8(*ImportConsumerSource);
		asIScriptModule* Module = ScriptEngine->GetModule(
			ModuleNameUtf8.Get(), asGM_ALWAYS_CREATE);
		ReportGeneratedModule(*TestRunner, ModuleName, Source);
		ASSERT_THAT(IsNotNull(Module,
			TEXT("Function debug metadata source should create its candidate module")));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(asSUCCESS,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT),
			TEXT("Function debug metadata must retain the sealed AST used by Canonical CodeGen")));
		const int AddSectionResult = Module->AddScriptSection(
			ModuleNameUtf8.Get(), SourceUtf8.Get(),
			static_cast<unsigned int>(std::strlen(SourceUtf8.Get())));
		ASSERT_THAT(AreEqual(asSUCCESS, AddSectionResult,
			TEXT("Function debug metadata source should add its candidate section")));
		const int BuildResult = AddSectionResult >= 0 ? Module->Build() : AddSectionResult;
		ASSERT_THAT(IsTrue(BuildResult >= 0, TEXT("Function debug metadata source should compile")));
		ASSERT_THAT(IsNotNull(Module, TEXT("Function debug metadata source should publish a module")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
			ScriptEngine->DiscardModule(ImportProviderNameUtf8.Get());
			ScriptEngine->DiscardModule(ImportConsumerNameUtf8.Get());
			ScriptEngine->DiscardModule(RebuiltModuleNameUtf8.Get());
			ScriptEngine->DiscardModule(LoadedModuleNameUtf8.Get());
			ScriptEngine->DiscardModule(StrippedModuleNameUtf8.Get());
		};

		asIScriptFunction* const Inspectable = GetNativeFunctionByExactDecl(Module, "int Inspectable(const int)");
		asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int RunMetadataEntry()");
		asIScriptFunction* const Noop = GetNativeFunctionByExactDecl(Module, "void MetadataNoop()");
		FString AvailableDeclarations;
		for (asUINT FunctionIndex = 0; FunctionIndex < Module->GetFunctionCount(); ++FunctionIndex)
		{
			if (asIScriptFunction* const Candidate = Module->GetFunctionByIndex(FunctionIndex))
			{
				AvailableDeclarations += UTF8_TO_TCHAR(Candidate->GetDeclaration());
				AvailableDeclarations += TEXT("; ");
			}
		}
		ASSERT_THAT(IsNotNull(Inspectable,
			*FString::Printf(TEXT("Function debug metadata should resolve the namespaced script function exactly. Available={%s}"),
				*AvailableDeclarations)));
		ASSERT_THAT(IsNotNull(Entry, TEXT("Function debug metadata should resolve the runtime entry exactly")));
		ASSERT_THAT(IsNotNull(Noop, TEXT("Function debug metadata should resolve the script noop exactly")));
		if (Inspectable == nullptr || Entry == nullptr || Noop == nullptr)
		{
			return;
		}

		asCModule* const ConcreteModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			ConcreteModule->GetLastBytecodePublisher(),
			TEXT("Function debug metadata must come from Canonical CodeGen")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ConcreteModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("Function debug metadata must not be reconstructed by the legacy compiler")));

		asIASTSnapshot* const Snapshot = Module->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(Snapshot,
			TEXT("Function debug metadata must retain the exact sealed Canonical AST generation")));
		ON_SCOPE_EXIT
		{
			if (Snapshot != nullptr)
			{
				Snapshot->Release();
			}
		};
		const asCASTContext* const AST = Snapshot != nullptr
			? static_cast<const asCASTSnapshot*>(Snapshot)->GetContext()
			: nullptr;
		ASSERT_THAT(IsNotNull(AST,
			TEXT("Function debug metadata snapshot must expose its immutable AST context")));
		const asCDecl* ASTFunction = nullptr;
		const asCDecl* ASTParameter = nullptr;
		const asCDecl* ASTLocal = nullptr;
		bool bLocalDeclStmtUsesExactDecl = false;
		if (AST != nullptr)
		{
			for (asUINT DeclIndex = 1; DeclIndex <= AST->GetDeclCount(); ++DeclIndex)
			{
				const asCDecl* const Candidate = AST->GetDecl(asASTDeclId(DeclIndex));
				if (Candidate != nullptr
					&& Candidate->kind == asAST_DECL_FUNCTION
					&& Candidate->name.Equals("Inspectable"))
				{
					ASSERT_THAT(IsNull(ASTFunction,
						TEXT("Function debug metadata AST must contain one exact Inspectable declaration")));
					ASTFunction = Candidate;
				}
			}
			if (ASTFunction != nullptr)
			{
				for (asUINT ChildIndex = 0;
					ChildIndex < ASTFunction->children.GetLength(); ++ChildIndex)
				{
					const asCDecl* const Child = AST->GetDecl(ASTFunction->children[ChildIndex]);
					if (Child != nullptr && Child->kind == asAST_DECL_PARAM
						&& Child->name.Equals("Parameter"))
					{
						ASTParameter = Child;
					}
					else if (Child != nullptr && Child->kind == asAST_DECL_VAR
						&& Child->name.Equals("Local"))
					{
						ASTLocal = Child;
					}
				}
			}
			if (ASTLocal != nullptr)
			{
				for (asUINT StmtIndex = 1; StmtIndex <= AST->GetStmtCount(); ++StmtIndex)
				{
					const asCStmt* const Statement = AST->GetStmt(asASTStmtId(StmtIndex));
					if (Statement != nullptr && Statement->kind == asAST_STMT_DECL
						&& Statement->decl == ASTLocal->id)
					{
						bLocalDeclStmtUsesExactDecl = true;
						break;
					}
				}
			}
		}
		ASSERT_THAT(IsNotNull(ASTFunction,
			TEXT("Function debug metadata AST must retain Inspectable")));
		ASSERT_THAT(IsNotNull(ASTParameter,
			TEXT("Function debug metadata AST must retain the named parameter declaration")));
		ASSERT_THAT(IsNotNull(ASTLocal,
			TEXT("Function debug metadata AST must retain the named local declaration")));
		ASSERT_THAT(IsTrue(ASTFunction != nullptr
			&& ASTParameter != nullptr && ASTLocal != nullptr
			&& ASTParameter->id != ASTLocal->id
			&& ASTParameter->parent == ASTFunction->id
			&& ASTLocal->parent == ASTFunction->id
			&& ASTParameter->type.IsValid()
			&& ASTParameter->type == ASTLocal->type,
			TEXT("Function debug metadata AST must provide distinct, typed, function-owned declaration identities")));
		ASSERT_THAT(IsTrue(bLocalDeclStmtUsesExactDecl,
			TEXT("Function debug metadata AST DeclStmt must reference the exact Local DeclId")));

		ASSERT_THAT(IsTrue(Inspectable->GetId() >= 0, TEXT("Function debug metadata should publish a stable function id")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asFUNC_SCRIPT), static_cast<int32>(Inspectable->GetFuncType()), TEXT("Function debug metadata should identify the script function kind")));
		ASSERT_THAT(AreEqual(Module, Inspectable->GetModule(), TEXT("Function debug metadata should retain its owning module")));
		ASSERT_THAT(AreEqual(ModuleName, FString(UTF8_TO_TCHAR(Inspectable->GetModuleName())), TEXT("Function debug metadata should retain its module name")));
		ASSERT_THAT(AreEqual(ModuleName, FString(UTF8_TO_TCHAR(Inspectable->GetScriptSectionName())), TEXT("Function debug metadata should retain its script section")));
		ASSERT_THAT(AreEqual(FString(TEXT("Inspectable")), FString(UTF8_TO_TCHAR(Inspectable->GetName())), TEXT("Function debug metadata should retain its function name")));
		ASSERT_THAT(AreEqual(FString(TEXT("NativeDebugMetadata")), FString(UTF8_TO_TCHAR(Inspectable->GetNamespace())), TEXT("Function debug metadata should retain its namespace")));
		ASSERT_THAT(AreEqual(FString(TEXT("int Inspectable(const int)")), FString(UTF8_TO_TCHAR(Inspectable->GetDeclaration())), TEXT("Function debug metadata should retain its exact fork-normalized declaration")));
		ASSERT_THAT(IsNull(Inspectable->GetObjectType(), TEXT("Global function debug metadata should have no receiver type")));
		ASSERT_THAT(IsTrue(Inspectable->GetObjectName() == nullptr, TEXT("Global function debug metadata should have no receiver name")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Inspectable->GetVarCount()),
			TEXT("Function debug metadata should expose exactly its named parameter and local")));
		ASSERT_THAT(IsTrue(HasNamedVariable(Inspectable, "Parameter", "const int Parameter"), TEXT("Function debug metadata should expose the fork-normalized parameter declaration")));
		ASSERT_THAT(IsTrue(HasNamedVariable(Inspectable, "Local", "int Local"), TEXT("Function debug metadata should expose the local declaration")));
		const asCScriptFunction* const ConcreteInspectable =
			static_cast<const asCScriptFunction*>(Inspectable);
		ASSERT_THAT(IsTrue(ConcreteInspectable->scriptData != nullptr
			&& ConcreteInspectable->scriptData->variables.GetLength() == 2,
			TEXT("Function debug metadata should publish one transactional ScriptFunctionData variable table")));
		if (ConcreteInspectable->scriptData != nullptr
			&& ConcreteInspectable->scriptData->variables.GetLength() == 2)
		{
			const asSScriptVariable* const ParameterVariable =
				ConcreteInspectable->scriptData->variables[0];
			const asSScriptVariable* const LocalVariable =
				ConcreteInspectable->scriptData->variables[1];
			ASSERT_THAT(IsTrue(ParameterVariable != nullptr && LocalVariable != nullptr,
				TEXT("Function debug metadata should own both variable records")));
			ASSERT_THAT(IsTrue(ParameterVariable != nullptr
				&& ParameterVariable->name.Equals("Parameter")
				&& ParameterVariable->stackOffset <= 0,
				TEXT("Function debug metadata parameter record should target the Runtime argument slot")));
			ASSERT_THAT(IsTrue(LocalVariable != nullptr
				&& LocalVariable->name.Equals("Local")
				&& LocalVariable->stackOffset > 0,
				TEXT("Function debug metadata local record should target Canonical local storage")));
			ASSERT_THAT(IsTrue(ParameterVariable != nullptr && LocalVariable != nullptr
				&& ParameterVariable->stackOffset != LocalVariable->stackOffset
				&& LocalVariable->declaredAtProgramPos >= ParameterVariable->declaredAtProgramPos,
				TEXT("Function debug metadata should preserve distinct slots and nondecreasing declaration order")));
		}
		asCString FunctionDump;
		asCBytecodeCodeGenDumpFunction(ConcreteInspectable, FunctionDump);
		const FString FunctionDumpText = UTF8_TO_TCHAR(
			FunctionDump.AddressOf() != nullptr ? FunctionDump.AddressOf() : "");
		ASSERT_THAT(IsTrue(FunctionDumpText.Contains(TEXT("debugVariables=2"))
			&& FunctionDumpText.Contains(TEXT("name=Parameter"))
			&& FunctionDumpText.Contains(TEXT("type=const int"))
			&& FunctionDumpText.Contains(TEXT("name=Local"))
			&& FunctionDumpText.Contains(TEXT("type=int"))
			&& FunctionDumpText.Contains(TEXT("declaredPc=")),
			*FString::Printf(TEXT("Function debug dump should expose variable identity, type, slot and declaration PC:\n%s"),
				*FunctionDumpText)));
		// The SDK query is relative to the function declaration; a line before the
		// declaration is intentionally rejected by the current fork.
		const int FirstExecutableLine = Inspectable->FindNextLineWithCode(3);
		ASSERT_THAT(IsTrue(FirstExecutableLine > 0,
			*FString::Printf(TEXT("Function debug metadata should find the first executable source line. Result=%d"), FirstExecutableLine)));
		ASSERT_THAT(IsTrue(Inspectable->FindNextLineWithCode(100000) < 0, TEXT("Function debug metadata should reject a source line beyond the function")));

		asUINT BytecodeLength = 0;
		asDWORD* const Bytecode = Inspectable->GetByteCode(&BytecodeLength);
		ASSERT_THAT(IsNotNull(Bytecode, TEXT("Function debug metadata should expose script bytecode")));
		ASSERT_THAT(IsTrue(BytecodeLength > 0, TEXT("Function debug metadata should expose a non-empty bytecode range")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asFUNC_SCRIPT), static_cast<int32>(Noop->GetFuncType()), TEXT("Function debug metadata noop should remain a script function")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Noop->GetParamCount()), TEXT("Function debug metadata noop should retain its empty parameter list")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Noop->GetVarCount()), TEXT("Function debug metadata noop should not fabricate local metadata")));

		asIScriptFunction* const SystemFunction = ScriptEngine->GetGlobalFunctionByDecl("int NativeSystemMetadata()");
		ASSERT_THAT(IsNotNull(SystemFunction, TEXT("Function debug metadata should resolve the system-function query target exactly")));
		if (SystemFunction != nullptr)
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asFUNC_SYSTEM), static_cast<int32>(SystemFunction->GetFuncType()), TEXT("Function debug metadata should distinguish system from script function kind")));
			ASSERT_THAT(IsNull(SystemFunction->GetModule(), TEXT("Function debug metadata system function should not claim a script module owner")));
			ASSERT_THAT(AreEqual(FString(TEXT("NativeSystemMetadata")), FString(UTF8_TO_TCHAR(SystemFunction->GetName())), TEXT("Function debug metadata system function should retain its exact name")));
		}

		FMemoryBinaryStream PreservedDebugBytecode;
		ASSERT_THAT(AreEqual(asSUCCESS, Module->SaveByteCode(&PreservedDebugBytecode, false), TEXT("Function debug metadata built module should serialize preserved debug information")));
		bool bPreservedDebugWasStripped = true;
		asIScriptModule* const LoadedModule = ScriptEngine->GetModule(LoadedModuleNameUtf8.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(LoadedModule, TEXT("Function debug metadata saved-loaded lifecycle should create a target module")));
		if (LoadedModule != nullptr)
		{
			PreservedDebugBytecode.ResetReadOffset();
			ASSERT_THAT(AreEqual(asSUCCESS, LoadedModule->LoadByteCode(&PreservedDebugBytecode, &bPreservedDebugWasStripped), TEXT("Function debug metadata saved-loaded lifecycle should load preserved bytecode")));
			ASSERT_THAT(IsFalse(bPreservedDebugWasStripped, TEXT("Function debug metadata saved-loaded lifecycle should report preserved debug information")));
			asIScriptFunction* const LoadedInspectable = GetNativeFunctionByExactDecl(LoadedModule, "int Inspectable(const int)");
			ASSERT_THAT(IsNotNull(LoadedInspectable, TEXT("Function debug metadata saved-loaded lifecycle should resolve the original declaration")));
			if (LoadedInspectable != nullptr)
			{
				ASSERT_THAT(IsTrue(LoadedInspectable->GetVarCount() >= 2, TEXT("Function debug metadata saved-loaded lifecycle should retain local-variable metadata")));
				ASSERT_THAT(IsTrue(LoadedInspectable->FindNextLineWithCode(3) > 0, TEXT("Function debug metadata saved-loaded lifecycle should retain executable line metadata")));
			}
		}

		FMemoryBinaryStream StrippedDebugBytecode;
		ASSERT_THAT(AreEqual(asSUCCESS, Module->SaveByteCode(&StrippedDebugBytecode, true), TEXT("Function debug metadata debug-stripped lifecycle should serialize stripped bytecode")));
		bool bStrippedDebugWasStripped = false;
		asIScriptModule* const StrippedModule = ScriptEngine->GetModule(StrippedModuleNameUtf8.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(StrippedModule, TEXT("Function debug metadata debug-stripped lifecycle should create a target module")));
		if (StrippedModule != nullptr)
		{
			StrippedDebugBytecode.ResetReadOffset();
			ASSERT_THAT(AreEqual(asSUCCESS, StrippedModule->LoadByteCode(&StrippedDebugBytecode, &bStrippedDebugWasStripped), TEXT("Function debug metadata debug-stripped lifecycle should load stripped bytecode")));
			ASSERT_THAT(IsTrue(bStrippedDebugWasStripped, TEXT("Function debug metadata debug-stripped lifecycle should report stripped debug information")));
			asIScriptFunction* const StrippedInspectable = GetNativeFunctionByExactDecl(StrippedModule, "int Inspectable(const int)");
			ASSERT_THAT(IsNotNull(StrippedInspectable, TEXT("Function debug metadata debug-stripped lifecycle should retain callable function identity")));
			if (StrippedInspectable != nullptr)
			{
				ASSERT_THAT(IsTrue(StrippedInspectable->GetByteCode(nullptr) != nullptr, TEXT("Function debug metadata debug-stripped lifecycle should retain executable bytecode")));
			}
		}

		const asPWORD PreviousOptimization = ScriptEngine->GetEngineProperty(asEP_OPTIMIZE_BYTECODE);
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, 0), TEXT("Function debug metadata rebuilt lifecycle should select unoptimized bytecode")));
		asIScriptModule* RebuiltModule = nullptr;
		ReportGeneratedModule(*TestRunner, RebuiltModuleName, Source);
		ASSERT_THAT(IsTrue(CompileNativeModule(ScriptEngine, RebuiltModuleNameUtf8.Get(), SourceUtf8.Get(), RebuiltModule) >= 0, TEXT("Function debug metadata rebuilt lifecycle should compile unoptimized source")));
		ASSERT_THAT(IsNotNull(RebuiltModule, TEXT("Function debug metadata rebuilt lifecycle should publish an unoptimized module")));
		if (RebuiltModule != nullptr)
		{
			asIScriptFunction* const RebuiltEntry = GetNativeFunctionByExactDecl(RebuiltModule, "int RunMetadataEntry()");
			ASSERT_THAT(IsNotNull(RebuiltEntry, TEXT("Function debug metadata rebuilt lifecycle should resolve its exact entry")));
			if (RebuiltEntry != nullptr)
			{
				asIScriptContext* const RebuiltContext = ScriptEngine->CreateContext();
				ASSERT_THAT(IsNotNull(RebuiltContext, TEXT("Function debug metadata rebuilt lifecycle should create a context")));
				if (RebuiltContext != nullptr)
				{
					ON_SCOPE_EXIT { RebuiltContext->Release(); };
					ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(RebuiltContext, RebuiltEntry), TEXT("Function debug metadata rebuilt lifecycle should execute under the selected optimization mode")));
					ASSERT_THAT(AreEqual(42, static_cast<int32>(RebuiltContext->GetReturnDWord()), TEXT("Function debug metadata rebuilt lifecycle should retain runtime semantics under the selected optimization mode")));
					ASSERT_THAT(AreEqual(asSUCCESS, RebuiltContext->Unprepare(), TEXT("Function debug metadata rebuilt lifecycle should unprepare after execution")));
				}
			}
		}
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, PreviousOptimization), TEXT("Function debug metadata rebuilt lifecycle should restore the selected optimization mode")));

		asIScriptModule* ImportProvider = nullptr;
		asIScriptModule* ImportConsumer = nullptr;
		ReportGeneratedModule(*TestRunner, ImportProviderName, ImportProviderSource);
		ASSERT_THAT(IsTrue(CompileNativeModule(ScriptEngine, ImportProviderNameUtf8.Get(), ImportProviderSourceUtf8.Get(), ImportProvider) >= 0, TEXT("Function debug metadata imported provider should compile")));
		ReportGeneratedModule(*TestRunner, ImportConsumerName, ImportConsumerSource);
		ASSERT_THAT(IsTrue(CompileNativeModule(ScriptEngine, ImportConsumerNameUtf8.Get(), ImportConsumerSourceUtf8.Get(), ImportConsumer) >= 0, TEXT("Function debug metadata imported consumer should compile")));
		ASSERT_THAT(IsNotNull(ImportProvider, TEXT("Function debug metadata imported provider should publish a module")));
		ASSERT_THAT(IsNotNull(ImportConsumer, TEXT("Function debug metadata imported consumer should publish a module")));
		if (ImportProvider != nullptr && ImportConsumer != nullptr)
		{
			asIScriptFunction* const ImportedProviderFunction = GetNativeFunctionByExactDecl(ImportProvider, "int MetadataImportedValue()");
			ASSERT_THAT(IsNotNull(ImportedProviderFunction, TEXT("Function debug metadata imported provider should expose its exact function")));
			ASSERT_THAT(AreEqual(1, static_cast<int32>(ImportConsumer->GetImportedFunctionCount()), TEXT("Function debug metadata imported consumer should retain one import declaration")));
			ASSERT_THAT(AreEqual(asSUCCESS, ImportConsumer->BindImportedFunction(0, ImportedProviderFunction), TEXT("Function debug metadata imported consumer should bind the exact provider function")));
			asIScriptFunction* const ImportedEntry = GetNativeFunctionByExactDecl(ImportConsumer, "int RunImportedMetadata()");
			ASSERT_THAT(IsNotNull(ImportedEntry, TEXT("Function debug metadata imported consumer should expose its runtime entry")));
			if (ImportedEntry != nullptr)
			{
				asIScriptContext* const ImportedContext = ScriptEngine->CreateContext();
				ASSERT_THAT(IsNotNull(ImportedContext, TEXT("Function debug metadata imported consumer should create a context")));
				if (ImportedContext != nullptr)
				{
					ON_SCOPE_EXIT { ImportedContext->Release(); };
					ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(ImportedContext, ImportedEntry), TEXT("Function debug metadata imported consumer should execute its bound provider")));
					ASSERT_THAT(AreEqual(73, static_cast<int32>(ImportedContext->GetReturnDWord()), TEXT("Function debug metadata imported consumer should retain the provider result")));
					ASSERT_THAT(AreEqual(asSUCCESS, ImportedContext->Unprepare(), TEXT("Function debug metadata imported consumer should unprepare after execution")));
				}
			}
		}

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Function debug metadata product should create a context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		FNativeDebugRecorder Recorder;
		Context->SetUserData(&Recorder, NativeDebugRecorderUserDataSlot);
		asCContext* const RawContext = static_cast<asCContext*>(Context);
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetLineCallback(CaptureNativeLine), TEXT("Function debug metadata should install its live-frame observer")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Entry), TEXT("Function debug metadata entry should finish")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Function debug metadata entry should return the inspected result")));
		ASSERT_THAT(IsTrue(Recorder.GetEvents().ContainsByPredicate([](const FNativeDebugEvent& Event)
		{
			return Event.Kind == ENativeDebugEventKind::Line
				&& Event.FunctionDeclaration == TEXT("int Inspectable(const int)");
		}), TEXT("Live line metadata should correlate with the static function declaration")));
		RawContext->ClearLineCallback();
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Function debug metadata context should unprepare after execution")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
