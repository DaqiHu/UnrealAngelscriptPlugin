#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "Core/AngelscriptEngine.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "as_generic.h"
#include "as_module.h"
#include "frontend/Sema/as_sema.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

// Focused ProductionCodeGen slice: sealed named/default/hidden argument facts
// must reach the Runtime ABI without positional reconstruction or LEGACY use.

static void ProdCallArgumentPack(asIScriptGeneric* Generic)
{
	if (Generic == nullptr)
	{
		return;
	}
	const asDWORD A = Generic->GetArgDWord(0);
	const asDWORD Hidden = Generic->GetArgDWord(1);
	const asDWORD C = Generic->GetArgDWord(2);
	Generic->SetReturnDWord(A * 100u + Hidden * 10u + C);
}

static void ProdCallArgumentDefaultPack(asIScriptGeneric* Generic)
{
	if (Generic == nullptr)
	{
		return;
	}
	const asDWORD A = Generic->GetArgDWord(0);
	const asDWORD B = Generic->GetArgDWord(1);
	Generic->SetReturnDWord(A * 10u + B);
}

struct FProdCallArgumentDefaultReceiver
{
	int32 Padding = 0;
};

static FProdCallArgumentDefaultReceiver GProdCallArgumentDefaultReceiver;

struct FProdCallArgumentRegisteredDefaultValue
{
	int32 Value = 0;
};

static FProdCallArgumentRegisteredDefaultValue
	GProdCallArgumentRegisteredDefaultValue{42};

static void ProdCallArgumentDefaultMethod(asIScriptGeneric* Generic)
{
	if (Generic == nullptr || Generic->GetObject() == nullptr)
	{
		return;
	}
	const asDWORD A = Generic->GetArgDWord(0);
	const asDWORD B = Generic->GetArgDWord(1);
	Generic->SetReturnDWord(A * 10u + B);
}

static void ProdCallArgumentFloatDefault(asIScriptGeneric* Generic)
{
	if (Generic == nullptr)
	{
		return;
	}
	const double Scale = Generic->GetArgDouble(0);
	const asDWORD B = Generic->GetArgDWord(1);
	Generic->SetReturnDWord(static_cast<asDWORD>(Scale * 10.0) + B);
}

static void ProdCallArgumentRegisteredValueDefault(asIScriptGeneric* Generic)
{
	if (Generic == nullptr)
	{
		return;
	}
	const FProdCallArgumentRegisteredDefaultValue* const Value =
		static_cast<const FProdCallArgumentRegisteredDefaultValue*>(
			Generic->GetArgAddress(0));
	Generic->SetReturnDWord(Value != nullptr
		? static_cast<asDWORD>(Value->Value) : 0u);
}

static void ProdCallArgumentAmbientNameDefault(asIScriptGeneric* Generic)
{
	if (Generic == nullptr)
	{
		return;
	}
	const FName* const Name = static_cast<const FName*>(
		Generic->GetArgAddress(0));
	Generic->SetReturnDWord(Name != nullptr && Name->IsNone() ? 42u : 0u);
}

struct FProdCallArgumentEnumObservation
{
	bool bCalled = false;
	asDWORD Mode = 0;
	asDWORD B = 0;
};

static FProdCallArgumentEnumObservation GProdCallArgumentEnumObservation;

static void ProdCallArgumentObservedEnumDefault(asIScriptGeneric* Generic)
{
	GProdCallArgumentEnumObservation = FProdCallArgumentEnumObservation();
	if (Generic == nullptr)
	{
		return;
	}
	GProdCallArgumentEnumObservation.bCalled = true;
	GProdCallArgumentEnumObservation.Mode = Generic->GetArgByte(0);
	GProdCallArgumentEnumObservation.B = Generic->GetArgDWord(1);
	Generic->SetReturnDWord(
		GProdCallArgumentEnumObservation.Mode * 10u
		+ GProdCallArgumentEnumObservation.B);
}

static void ProdCallArgumentNullDefault(asIScriptGeneric* Generic)
{
	if (Generic == nullptr)
	{
		return;
	}
	const bool bIsNull = Generic->GetArgObject(0) == nullptr;
	const asDWORD B = Generic->GetArgDWord(1);
	Generic->SetReturnDWord((bIsNull ? 40u : 90u) + B);
}

struct FProdCallArgumentDefaultFactory
{
	int32 ReferenceCount = 1;
	int32 Value = 0;
};

static void ProdCallArgumentDefaultFactoryCreate(asIScriptGeneric* Generic)
{
	if (Generic == nullptr)
	{
		return;
	}
	FProdCallArgumentDefaultFactory* const Object =
		new FProdCallArgumentDefaultFactory();
	Object->Value = static_cast<int32>(
		Generic->GetArgDWord(0) * 10u + Generic->GetArgDWord(1));
	Generic->SetReturnAddress(Object);
}

static void ProdCallArgumentDefaultFactoryAddRef(asIScriptGeneric* Generic)
{
	FProdCallArgumentDefaultFactory* const Object = Generic != nullptr
		? static_cast<FProdCallArgumentDefaultFactory*>(Generic->GetObject())
		: nullptr;
	if (Object != nullptr)
	{
		++Object->ReferenceCount;
	}
}

static void ProdCallArgumentDefaultFactoryRelease(asIScriptGeneric* Generic)
{
	FProdCallArgumentDefaultFactory* const Object = Generic != nullptr
		? static_cast<FProdCallArgumentDefaultFactory*>(Generic->GetObject())
		: nullptr;
	if (Object != nullptr && --Object->ReferenceCount == 0)
	{
		delete Object;
	}
}

static void ProdCallArgumentDefaultFactoryValue(asIScriptGeneric* Generic)
{
	const FProdCallArgumentDefaultFactory* const Object = Generic != nullptr
		? static_cast<const FProdCallArgumentDefaultFactory*>(
			Generic->GetObject())
		: nullptr;
	Generic->SetReturnDWord(Object != nullptr
		? static_cast<asDWORD>(Object->Value) : 0u);
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTProductionCallArgumentTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.ProductionCodeGen.CallArguments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(NamedAndDefaultArgumentsExecuteSealedFormalPlan)
	{
		// Production mutation caught: deriving formal binding from reverse child
		// position makes F(B: 7) observe B as A instead of defaulting A to 1.
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("named/default call fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Pack(int A = 1, int B = 2)
			{
				return A * 10 + B;
			}

			int Entry()
			{
				return Pack(B: 7);
			}
			)AS");
		const int BuildResult =
			AngelscriptNativeTestSupport::CompileNativeModule(
				Engine.Get(),
				"ProdCallArgumentNamedDefault",
				ScriptSource.c_str(),
				Module);
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("named/default call must publish through Canonical CodeGen")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner,
				Engine.Get(),
				Module,
				"int Entry()",
				Value)
				&& Value == 17,
			*FString::Printf(TEXT(
				"Pack(B: 7) must retain named B and default A provenance; got=%d"),
				Value)));
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("named/default call must be published by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			InternalModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("named/default call must not invoke the retained LEGACY compiler")));
	}

	TEST_METHOD(DirectFunctionRuntimeShellOwnsAuthoredDefaultArguments)
	{
		// Production mutation caught: replacing either authored default with a
		// null parallel-array placeholder makes public Runtime metadata lose it.
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("direct default-metadata fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Pack(int A = 1, int B = 2)
			{
				return A * 10 + B;
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			AngelscriptNativeTestSupport::CompileNativeModule(
				Engine.Get(),
				"ProdCallArgumentDirectDefaults",
				ScriptSource.c_str(),
				Module),
			TEXT("direct default-metadata module must compile canonically")));
		if (Module == nullptr)
		{
			return;
		}

		asIScriptFunction* const Pack = Module->GetFunctionByDecl(
			"int Pack(int, int)");
		ASSERT_THAT(IsNotNull(Pack,
			TEXT("direct default-metadata module must publish exact Pack signature")));
		if (Pack == nullptr)
		{
			return;
		}

		const char* DefaultA = nullptr;
		const char* DefaultB = nullptr;
		ASSERT_THAT(AreEqual(asSUCCESS,
			Pack->GetParam(0, nullptr, nullptr, nullptr, &DefaultA),
			TEXT("Pack formal A metadata must be queryable")));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Pack->GetParam(1, nullptr, nullptr, nullptr, &DefaultB),
			TEXT("Pack formal B metadata must be queryable")));
		ASSERT_THAT(AreEqual(FString(TEXT("1")),
			FString(UTF8_TO_TCHAR(DefaultA != nullptr ? DefaultA : "")),
			TEXT("direct Runtime shell must own authored default A=1")));
		ASSERT_THAT(AreEqual(FString(TEXT("2")),
			FString(UTF8_TO_TCHAR(DefaultB != nullptr ? DefaultB : "")),
			TEXT("direct Runtime shell must own authored default B=2")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("default metadata must come from Canonical CodeGen publication")));
	}

	TEST_METHOD(RegisteredNativeGlobalProjectsDefaultForNamedCall)
	{
		// Production mutation caught: omitting Runtime default projection makes
		// the named B bind, but rejects the omitted defaulted A formal.
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"int NativeDefaultPack(int A = 1, int B = 2)",
			asFUNCTION(ProdCallArgumentDefaultPack),
			asCALL_GENERIC) >= 0,
			TEXT("registered-native default fixture must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("registered-native default fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return NativeDefaultPack(B: 7);
			}
			)AS");
		const int BuildResult =
			AngelscriptNativeTestSupport::CompileNativeModule(
				Engine.Get(),
				"ProdCallArgumentNativeDefaults",
				ScriptSource.c_str(),
				Module);
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("registered-native named/default call must compile canonically")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner,
				Engine.Get(),
				Module,
				"int Entry()",
				Value)
				&& Value == 17,
			*FString::Printf(TEXT(
				"NativeDefaultPack(B: 7) must execute A=1 B=7; got=%d"),
				Value)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("registered-native default call must use Canonical CodeGen")));
	}

	TEST_METHOD(RegisteredNativeGlobalProjectsTypedDefaultExpression)
	{
		// Production mutation caught: reparsing only an integer prefix (or keeping
		// the old strtoull fallback) turns the authored `40 + 2` into 40. Runtime
		// projection must attach the exact typed Canonical expression to formal A.
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"int NativeExpressionDefaultPack(int A = 40 + 2, int B = 1)",
			asFUNCTION(ProdCallArgumentDefaultPack),
			asCALL_GENERIC) >= 0,
			TEXT("registered-native expression-default fixture must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("registered-native expression-default fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return NativeExpressionDefaultPack(B: 7);
			}
			)AS");
		const int BuildResult =
			AngelscriptNativeTestSupport::CompileNativeModule(
				Engine.Get(),
				"ProdCallArgumentNativeExpressionDefault",
				ScriptSource.c_str(),
				Module);
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("typed registered-native default expression must compile canonically")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner,
				Engine.Get(),
				Module,
				"int Entry()",
				Value)
				&& Value == 427,
			*FString::Printf(TEXT(
				"typed default A=(40 + 2) and named B=7 must execute as 427; got=%d"),
				Value)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("typed registered-native default call must use Canonical CodeGen")));
	}

	TEST_METHOD(RegisteredNativeGlobalProjectsRegisteredValueDefaultExpression)
	{
		// Whole-Engine CANONICAL startup first exposed this with UE's registered
		// `const FName NAME_None`: reparsing a Runtime-owned default expression
		// must project the exact registered global property into the generated
		// expression scope before final publication.
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"FRegisteredDefaultValue",
			sizeof(FProdCallArgumentRegisteredDefaultValue),
			asOBJ_VALUE | asOBJ_POD
				| asGetTypeTraits<FProdCallArgumentRegisteredDefaultValue>()) >= 0,
			TEXT("registered-value default fixture type must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalProperty(
			"const FRegisteredDefaultValue GRegisteredDefaultValue",
			&GProdCallArgumentRegisteredDefaultValue) >= 0,
			TEXT("registered-value default fixture property must register")));
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetDefaultNamespace("RuntimeDefaultScope"),
			TEXT("registered-value default fixture namespace must select")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"int ReadRegisteredDefault(const FRegisteredDefaultValue&in Value = GRegisteredDefaultValue)",
			asFUNCTION(ProdCallArgumentRegisteredValueDefault),
			asCALL_GENERIC) >= 0,
			TEXT("registered-value default fixture function must register")));
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetDefaultNamespace(""),
			TEXT("registered-value default fixture namespace must restore")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return RuntimeDefaultScope::ReadRegisteredDefault();
			}
			)AS");
		const int BuildResult =
			AngelscriptNativeTestSupport::CompileNativeModule(
				Engine.Get(),
				"ProdCallArgumentRegisteredValueDefault",
				ScriptSource.c_str(),
				Module);
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("registered value-object global default must compile canonically")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}

		int32 Observed = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Observed)
				&& Observed == 42,
			*FString::Printf(TEXT(
				"registered value-object default must preserve value 42; got=%d"),
				Observed)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("registered value-object default must use Canonical CodeGen")));
	}

	TEST_METHOD(AmbientUEEngineProjectsNAME_NoneRuntimeDefaultExpression)
	{
		// This is the minimized whole-Engine startup failure: unlike a fresh
		// SDK Engine, the ambient UE Engine owns the real FName/NAME_None bind
		// graph and target-profile registration lifecycle.
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const asECompilerPipeline PreviousPipeline =
			ScriptEngine->GetCompilerPipeline();
		static const char* const ModuleName =
			"ProdCallArgumentAmbientNameDefault";
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetCompilerPipeline(PreviousPipeline);
			ScriptEngine->DiscardModule(ModuleName);
		};
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"int ReadAmbientNameDefault_7F83(const FName&in Value = NAME_None)",
			asFUNCTION(ProdCallArgumentAmbientNameDefault),
			asCALL_GENERIC) >= 0,
			TEXT("ambient NAME_None default fixture function must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return ReadAmbientNameDefault_7F83();
			}
			)AS");
		const int BuildResult =
			AngelscriptNativeTestSupport::CompileNativeModule(
				ScriptEngine,
				ModuleName,
				ScriptSource.c_str(),
				Module);
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("ambient UE NAME_None default must compile canonically")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}

		int32 Observed = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, ScriptEngine, Module, "int Entry()", Observed)
				&& Observed == 42,
			*FString::Printf(TEXT(
				"ambient UE NAME_None default must execute as 42; got=%d"),
				Observed)));
	}

	TEST_METHOD(AmbientUEEngineRanksUWorldAsDerivedUObject)
	{
		// The whole-Engine test-suite calls inherited native assertions whose
		// formal is UObject with values such as UWorld and UGameInstance. The
		// Runtime TypeIds are generation-local; Canonical overload ranking must
		// use the stable host ancestry projected into Decl::bases instead.
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const asECompilerPipeline PreviousPipeline =
			ScriptEngine->GetCompilerPipeline();
		static const char* const ModuleName =
			"ProdCallArgumentAmbientNativeBaseConversion";
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetCompilerPipeline(PreviousPipeline);
			ScriptEngine->DiscardModule(ModuleName);
		};
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asCArray<asSCanonicalRegisteredTypeDeclarationFact> RegisteredTypes;
		ScriptEngine->CopyCanonicalRegisteredTypeDeclarations(RegisteredTypes);
		bool bSawWorld = false;
		bool bSawWorldToObject = false;
		for (asUINT TypeIndex = 0;
			TypeIndex < RegisteredTypes.GetLength(); ++TypeIndex)
		{
			const asSCanonicalRegisteredTypeDeclarationFact& Fact =
				RegisteredTypes[TypeIndex];
			if (!Fact.stableKey.Equals("UWorld"))
			{
				continue;
			}
			bSawWorld = true;
			for (asUINT BaseIndex = 0;
				BaseIndex < Fact.baseStableKeys.GetLength(); ++BaseIndex)
			{
				bSawWorldToObject |=
					Fact.baseStableKeys[BaseIndex].Equals("UObject");
			}
		}
		ASSERT_THAT(IsTrue(bSawWorld && bSawWorldToObject,
			TEXT("ambient registration must publish UWorld -> UObject by stable names")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void AcceptAmbientObject_5E71(UObject Value)
			{
			}

			void Entry(UWorld World)
			{
				AcceptAmbientObject_5E71(World);
			}
			)AS");
		const int BuildResult =
			AngelscriptNativeTestSupport::CompileNativeModule(
				ScriptEngine,
				ModuleName,
				ScriptSource.c_str(),
				Module);
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("ambient UWorld must rank as a stable derived UObject reference")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("ambient native-base conversion must publish through Canonical CodeGen")));
	}

	TEST_METHOD(AmbientUEEngineRanksNullLiteralForNullableObjectFormal)
	{
		// Legacy accepts null only for a real object handle. Canonical overload
		// ranking must make the same decision from the sealed QualType, then retain
		// an explicit target-typed conversion for verifier and CodeGen consumers.
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ambient UE AngelScript Engine must be initialized")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const asECompilerPipeline PreviousPipeline =
			ScriptEngine->GetCompilerPipeline();
		static const char* const ModuleName =
			"ProdCallArgumentAmbientNullableObject";
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetCompilerPipeline(PreviousPipeline);
			ScriptEngine->DiscardModule(ModuleName);
		};
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* const Module = ScriptEngine->GetModule(
			ModuleName, asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0,
			Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int AcceptNullableObject_4D19(UObject Value)
			{
				return Value == nullptr ? 42 : 0;
			}

			int Entry()
			{
				return AcceptNullableObject_4D19(nullptr);
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdCallArgumentAmbientNullableObject.as",
			ScriptSource.c_str(),
			static_cast<asUINT>(ScriptSource.size()))));
		const int BuildResult = Module->Build();
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("nullptr must be a viable standard conversion for a nullable UObject formal")));
		if (BuildResult < 0)
		{
			return;
		}

		const asCASTContext* const Context =
			static_cast<asCModule*>(Module)->GetCanonicalASTContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("nullable-object fixture must retain its sealed Canonical AST")));
		if (Context == nullptr)
		{
			return;
		}

		const asCExpr* NullConversion = nullptr;
		for (asUINT ExprIndex = 1;
			ExprIndex <= Context->GetExprCount() && NullConversion == nullptr;
			++ExprIndex)
		{
			const asCExpr* const Call = Context->GetExpr(asASTExprId(ExprIndex));
			if (Call == nullptr || Call->kind != asAST_EXPR_CALL
				|| !Call->resolvedDecl.IsValid()
				|| Call->callArguments.GetLength() != 1)
			{
				continue;
			}
			const asCDecl* const Callee = Context->GetDecl(Call->resolvedDecl);
			if (Callee == nullptr
				|| !Callee->name.Equals("AcceptNullableObject_4D19"))
			{
				continue;
			}
			NullConversion = Context->GetExpr(
				Call->callArguments[0].expression);
		}

		const asCType* const TargetType = NullConversion != nullptr
			? Context->GetType(NullConversion->type.type) : nullptr;
		const asCExpr* const NullLiteral = NullConversion != nullptr
			&& NullConversion->children.GetLength() == 1
			? Context->GetExpr(NullConversion->children[0]) : nullptr;
		ASSERT_THAT(IsTrue(NullConversion != nullptr
			&& NullConversion->kind == asAST_EXPR_CONVERSION
			&& !NullConversion->resolvedDecl.IsValid()
			&& TargetType != nullptr
			&& TargetType->kind == asAST_TYPE_REFERENCE_OBJECT
			&& TargetType->stableKey.Equals("UObject")
			&& NullConversion->type.IsHandle()
			&& !NullConversion->type.IsReference()
			&& NullLiteral != nullptr
			&& NullLiteral->kind == asAST_EXPR_NULL_LITERAL,
			TEXT("Sema must seal one target-typed, builtin null-to-UObject conversion")));

		int32 Observed = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, ScriptEngine, Module, "int Entry()", Observed)
				&& Observed == 42,
			*FString::Printf(TEXT(
				"Canonical CodeGen must pass a real null UObject handle; got=%d"),
				Observed)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("nullable-object conversion must publish through Canonical CodeGen")));
	}

	TEST_METHOD(RejectsNullLiteralForReferenceQualifiedObjectFormalBeforeCodeGen)
	{
		// Production mutation caught: treating a constant null prvalue as an
		// addressable handle reference would let overload selection seal an
		// impossible &in/&out argument and defer the error to CodeGen.
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("reference-qualified null fixture selects CANONICAL")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"CNullableReference_4D19",
			0,
			asOBJ_REF | asOBJ_NOCOUNT) >= 0,
			TEXT("reference-qualified null fixture registers a real ref type")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int AcceptNullableReference_4D19(CNullableReference_4D19&in Value)
			{
				return Value == nullptr ? 42 : 0;
			}

			int Entry()
			{
				return AcceptNullableReference_4D19(nullptr);
			}
			)AS");
		const int BuildResult =
			AngelscriptNativeTestSupport::CompileNativeModule(
				ScriptEngine,
				"ProdCallArgumentRejectNullReference",
				ScriptSource.c_str(),
				Module);
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(IsTrue(BuildResult < 0,
			*FString::Printf(TEXT(
				"null literal must be rejected for a reference-qualified object formal; result=%d messages:\n%s"),
				BuildResult,
				*Messages)));
		ASSERT_THAT(IsTrue(Messages.Contains(
			TEXT("unresolved-callee:AcceptNullableReference_4D19")),
			*FString::Printf(TEXT(
				"Sema must reject the non-addressable null argument before verifier/CodeGen; messages:\n%s"),
				*Messages)));
	}

	TEST_METHOD(RegisteredNativeMethodProjectsTypedDefaultForNamedCall)
	{
		// Production mutation caught: skipping method Runtime defaults makes the
		// omitted A formal non-viable even though named B resolves exactly.
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"FNativeDefaultReceiver",
			sizeof(FProdCallArgumentDefaultReceiver),
			asOBJ_VALUE | asOBJ_POD
				| asGetTypeTraits<FProdCallArgumentDefaultReceiver>()) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectMethod(
			"FNativeDefaultReceiver",
			"int Pack(int A = 40 + 2, int B = 1)",
			asFUNCTION(ProdCallArgumentDefaultMethod),
			asCALL_GENERIC) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalProperty(
			"FNativeDefaultReceiver GNativeDefaultReceiver",
			&GProdCallArgumentDefaultReceiver) >= 0,
			TEXT("registered-native method fixture receiver must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return GNativeDefaultReceiver.Pack(B: 7);
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			AngelscriptNativeTestSupport::CompileNativeModule(
				Engine.Get(),
				"ProdCallArgumentNativeMethodDefault",
				ScriptSource.c_str(),
				Module),
			TEXT("registered-native method default must compile canonically")));
		if (Module == nullptr)
		{
			return;
		}

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 427,
			*FString::Printf(TEXT(
				"method default A=(40 + 2) and named B=7 must execute 427; got=%d"),
				Value)));
	}

	TEST_METHOD(RegisteredNativeFactoryProjectsTypedDefaultForNamedConstruction)
	{
		// Production mutation caught: a behaviour projection using the wrong
		// source/runtime ordinal drops factory defaults before constructor Sema.
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"CNativeDefaultFactory", 0,
			asOBJ_REF | asOBJ_IMPLICIT_HANDLE) >= 0,
			TEXT("registered-native factory implicit-handle type must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour(
				"CNativeDefaultFactory",
				asBEHAVE_FACTORY,
				"CNativeDefaultFactory f(int A = 40 + 2, int B = 1)",
				asFUNCTION(ProdCallArgumentDefaultFactoryCreate),
				asCALL_GENERIC) >= 0,
			TEXT("registered-native factory default behaviour must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour(
				"CNativeDefaultFactory",
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(ProdCallArgumentDefaultFactoryAddRef),
				asCALL_GENERIC) >= 0,
			TEXT("registered-native factory addref must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour(
				"CNativeDefaultFactory",
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(ProdCallArgumentDefaultFactoryRelease),
				asCALL_GENERIC) >= 0,
			TEXT("registered-native factory release must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectMethod(
				"CNativeDefaultFactory",
				"int GetValue() const",
				asFUNCTION(ProdCallArgumentDefaultFactoryValue),
				asCALL_GENERIC) >= 0,
			TEXT("registered-native factory observation method must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				CNativeDefaultFactory Object = CNativeDefaultFactory(B: 7);
				return Object.GetValue();
			}
			)AS");
		ASSERT_THAT(AreEqual(0,
			AngelscriptNativeTestSupport::CompileNativeModule(
				Engine.Get(),
				"ProdCallArgumentNativeFactoryDefault",
				ScriptSource.c_str(),
				Module),
			TEXT("registered-native factory default must compile canonically")));
		if (Module == nullptr)
		{
			return;
		}

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 427,
			*FString::Printf(TEXT(
				"factory default A=(40 + 2) and named B=7 must construct 427; got=%d"),
				Value)));
	}

	TEST_METHOD(RegisteredNativeGlobalProjectsFloatDefault)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0,
			TEXT("floating default fixture requires the fork's double-backed float ABI")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"int NativeFloatDefault(double Scale = 1.5, int B = 1)",
			asFUNCTION(ProdCallArgumentFloatDefault),
			asCALL_GENERIC) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry() { return NativeFloatDefault(B: 7); }
			)AS");
		ASSERT_THAT(AreEqual(0,
			AngelscriptNativeTestSupport::CompileNativeModule(
				Engine.Get(), "ProdCallArgumentFloatDefault",
				ScriptSource.c_str(), Module)));
		if (Module == nullptr)
		{
			return;
		}
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 22,
			*FString::Printf(TEXT(
				"float default 1.5 and named B=7 must execute 22; got=%d"),
				Value)));
	}

	TEST_METHOD(RegisteredNativeGlobalProjectsScopedEnumDefault)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterEnum("ENativeDefaultMode") >= 0
			&& ScriptEngine->RegisterEnumValue(
				"ENativeDefaultMode", "Fast", 4) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"int NativeEnumDefault(ENativeDefaultMode Mode = ENativeDefaultMode::Fast, int B = 1)",
			asFUNCTION(ProdCallArgumentObservedEnumDefault),
			asCALL_GENERIC) >= 0));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int ExplicitEntry() { return NativeEnumDefault(ENativeDefaultMode::Fast, 7); }
			int Entry() { return NativeEnumDefault(B: 7); }
			)AS");
		ASSERT_THAT(AreEqual(0,
			AngelscriptNativeTestSupport::CompileNativeModule(
				Engine.Get(), "ProdCallArgumentEnumDefault",
				ScriptSource.c_str(), Module)));
		if (Module == nullptr)
		{
			return;
		}
		GProdCallArgumentEnumObservation = FProdCallArgumentEnumObservation();
		int32 ExplicitValue = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module,
				"int ExplicitEntry()", ExplicitValue)
				&& ExplicitValue == 47
				&& GProdCallArgumentEnumObservation.bCalled
				&& GProdCallArgumentEnumObservation.Mode == 4
				&& GProdCallArgumentEnumObservation.B == 7,
			*FString::Printf(TEXT(
				"explicit enum baseline must call native target with Mode=4/B=7; value=%d called=%d mode=%u b=%u"),
				ExplicitValue,
				GProdCallArgumentEnumObservation.bCalled ? 1 : 0,
				GProdCallArgumentEnumObservation.Mode,
				GProdCallArgumentEnumObservation.B)));

		GProdCallArgumentEnumObservation = FProdCallArgumentEnumObservation();
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 47
				&& GProdCallArgumentEnumObservation.bCalled
				&& GProdCallArgumentEnumObservation.Mode == 4
				&& GProdCallArgumentEnumObservation.B == 7,
			*FString::Printf(TEXT(
				"scoped enum default must call native target with Mode=4/B=7; value=%d called=%d mode=%u b=%u"),
				Value,
				GProdCallArgumentEnumObservation.bCalled ? 1 : 0,
				GProdCallArgumentEnumObservation.Mode,
				GProdCallArgumentEnumObservation.B)));
	}

	TEST_METHOD(RegisteredNativeGlobalProjectsNullHandleDefault)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"CNativeDefaultHandle", 0,
			asOBJ_REF | asOBJ_IMPLICIT_HANDLE) >= 0,
			TEXT("null-default implicit-handle type must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour(
				"CNativeDefaultHandle", asBEHAVE_ADDREF, "void f()",
				asFUNCTION(ProdCallArgumentDefaultFactoryAddRef),
				asCALL_GENERIC) >= 0,
			TEXT("null-default implicit-handle addref must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour(
				"CNativeDefaultHandle", asBEHAVE_RELEASE, "void f()",
				asFUNCTION(ProdCallArgumentDefaultFactoryRelease),
				asCALL_GENERIC) >= 0,
			TEXT("null-default implicit-handle release must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
				"int NativeNullDefault(CNativeDefaultHandle Object = nullptr, int B = 1)",
				asFUNCTION(ProdCallArgumentNullDefault),
				asCALL_GENERIC) >= 0,
			TEXT("implicit-handle nullptr default function must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry() { return NativeNullDefault(B: 7); }
			)AS");
		ASSERT_THAT(AreEqual(0,
			AngelscriptNativeTestSupport::CompileNativeModule(
				Engine.Get(), "ProdCallArgumentNullDefault",
				ScriptSource.c_str(), Module)));
		if (Module == nullptr)
		{
			return;
		}
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value == 47,
			*FString::Printf(TEXT(
				"null handle default and named B=7 must execute 47; got=%d"),
				Value)));
	}

	TEST_METHOD(HiddenMiddleArgumentExecutesSealedFormalPlan)
	{
		// Production mutation caught: reconstructing ABI arguments from child
		// positions can bind named C or A into the injected middle Hidden slot.
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		const int NativePackId = ScriptEngine->RegisterGlobalFunction(
			"int NativePack(int A, int Hidden, int C)",
			asFUNCTION(ProdCallArgumentPack),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(NativePackId >= 0,
			TEXT("NativePack hidden-middle fixture must register")));
		asCScriptFunction* const NativePack = static_cast<asCScriptFunction*>(
			ScriptEngine->GetFunctionById(NativePackId));
		ASSERT_THAT(IsNotNull(NativePack,
			TEXT("NativePack Runtime function must be available")));
		if (NativePack == nullptr)
		{
			return;
		}
		NativePack->hiddenArgumentIndex = 1;
		NativePack->hiddenArgumentDefault = "7";
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("hidden-middle call fixture selects CANONICAL")));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return NativePack(C: 3, A: 1);
			}
			)AS");
		const int BuildResult =
			AngelscriptNativeTestSupport::CompileNativeModule(
				Engine.Get(),
				"ProdCallArgumentHiddenMiddle",
				ScriptSource.c_str(),
				Module);
		ASSERT_THAT(AreEqual(0, BuildResult,
			TEXT("hidden-middle named call must publish through Canonical CodeGen")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner,
				Engine.Get(),
				Module,
				"int Entry()",
				Value)
				&& Value == 173,
			*FString::Printf(TEXT(
				"NativePack(C: 3, A: 1) must execute A=1 Hidden=7 C=3; got=%d"),
				Value)));
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("hidden-middle call must be published by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			InternalModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("hidden-middle call must not invoke the retained LEGACY compiler")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
