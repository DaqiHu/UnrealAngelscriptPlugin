#include "CQTest.h"

#include "AngelscriptTestEngine.h"
#include "Core/AngelscriptBinds.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptNativeFormCatalog.h"
#include "StaticJIT/BytecodeJIT/StaticJITBinds.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallDescriptor.h"

#include "StartAngelscriptHeaders.h"
#include "as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeFormCatalogTests,
	"Angelscript.TestModule.StaticJIT.NativeFormCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 CatalogHeaderInlineProbe(int32 Value)
	{
		return Value + 1;
	}

	static int32 CatalogModuleExportedProbe(int32 Value)
	{
		return Value + 2;
	}

	static int32 CatalogBridgeProbe(int32 Value)
	{
		return Value + 3;
	}

	static int32 CatalogLambdaProbe()
	{
		return 7;
	}

	static TUniquePtr<FAngelscriptEngine> CreateCatalogEngine(
		const EAngelscriptJITTargetProfile Profile)
	{
		FAngelscriptEngineConfig Config;
		Config.Purpose = EAngelscriptEnginePurpose::StaticJITGeneration;
		Config.bCollectStaticJITCompatibilityBinds = true;
		Config.bSkipThreadedInitialize = true;
		Config.bRunningCommandlet = true;
		Config.bIsUnattended = true;
		Config.bDisableCacheV2Persistence = true;
		Config.bForceSerialCacheV2Preparation = true;
		Config.bOverrideTargetPreprocessorFlags = true;
		switch (Profile)
		{
		case EAngelscriptJITTargetProfile::GameShipping:
			Config.bIsEditor = false;
			Config.bDevelopmentMode = false;
			Config.bTargetEditor = false;
			Config.bTargetEditorOnlyData = false;
			Config.bTargetRelease = true;
			break;
		case EAngelscriptJITTargetProfile::GameDevelopment:
			Config.bIsEditor = false;
			Config.bDevelopmentMode = true;
			Config.bTargetEditor = false;
			Config.bTargetEditorOnlyData = false;
			Config.bTargetRelease = false;
			break;
		default:
			Config.bIsEditor = true;
			Config.bDevelopmentMode = true;
			Config.bTargetEditor = true;
			Config.bTargetEditorOnlyData = true;
			Config.bTargetRelease = false;
			break;
		}
		return FAngelscriptTestEngine::Create(
			Config,
			FAngelscriptEngineDependencies::CreateDefault());
	}

	static FAngelscriptStaticJITNativeCallDescriptor MakeBridgeDescriptor(
		const asIScriptFunction& Function)
	{
		FAngelscriptStaticJITNativeCallDescriptor Descriptor;
		Descriptor.Linkage =
			EAngelscriptStaticJITNativeCallLinkage::ProviderPrivate;
		Descriptor.Visibility =
			EAngelscriptStaticJITNativeCallVisibility::ProviderPrivate;
		Descriptor.Symbol = TEXT("CatalogBridgeProbe");
		Descriptor.OwningModule = FName(TEXT("AngelscriptRuntime"));
		TryBuildAngelscriptStaticJITNativeABIIdentity(
			Function, Descriptor.NativeABI);
		TryBuildAngelscriptStaticJITScalarABIIdentity(
			Function, Descriptor.ScalarABI);
		Descriptor.CppCallableSignature = TEXT("int32(int32)");
		Descriptor.ABIDomain =
			EAngelscriptStaticJITNativeCallABIDomain::VMScalarBridge;
		Descriptor.ExceptionPolicy =
			EAngelscriptStaticJITNativeCallExceptionPolicy::MaySetScriptException;
		Descriptor.RouteFlags =
			EAngelscriptStaticJITNativeCallRouteFlags::ScalarBridge
			| EAngelscriptStaticJITNativeCallRouteFlags::CurrentNativeCall;
		Descriptor.LifetimeFlags =
			EAngelscriptStaticJITNativeCallLifetimeFlags::ByValueScalarOnly
			| EAngelscriptStaticJITNativeCallLifetimeFlags::NoRetainedReferences
			| EAngelscriptStaticJITNativeCallLifetimeFlags::NoEngineLocalCapture;
		return Descriptor;
	}

public:
	BEFORE_EACH()
	{
		FAngelscriptNativeFormCatalog::Get().ResetForTests();
	}

	AFTER_EACH()
	{
		FAngelscriptNativeFormCatalog::Get().ResetForTests();
	}

	TEST_METHOD(ConflictRejectionKeepsTheFirstRecipe)
	{
		FAngelscriptNativeFormCatalog& Catalog =
			FAngelscriptNativeFormCatalog::Get();
		FAngelscriptNativeFormCatalogKey Key;
		Key.StableDeclarationIdentity = TEXT("int ConflictProbe()");
		Key.TargetProfile = EAngelscriptJITTargetProfile::EditorDevelopment;

		FAngelscriptNativeFormRecipe First;
		First.Kind = EAngelscriptNativeFormRecipeKind::HeaderInline;
		First.Description.Kind = EAngelscriptNativeFormKind::FunctionHeader;
		First.Description.Name = TEXT("ConflictProbeA");
		First.Description.Header = TEXT("ConflictA.h");
		ASSERT_THAT(IsTrue(Catalog.InsertOrValidate(Key, First)));

		FAngelscriptNativeFormRecipe Replay = First;
		ASSERT_THAT(IsTrue(Catalog.InsertOrValidate(Key, Replay)));
		ASSERT_THAT(AreEqual(1, Catalog.NumRecipes()));

		FAngelscriptNativeFormRecipe Conflict = First;
		Conflict.Description.Name = TEXT("ConflictProbeB");
		FString Error;
		ASSERT_THAT(IsFalse(Catalog.InsertOrValidate(Key, Conflict, &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("NativeFormCatalogConflict"))));
		const FAngelscriptNativeFormRecipe* Found = Catalog.Find(Key);
		ASSERT_THAT(IsNotNull(Found));
		ASSERT_THAT(AreEqual(FString(TEXT("ConflictProbeA")), Found->Description.Name));
	}

	TEST_METHOD(HeaderInlineModuleExportedAndBridgeStayDistinct)
	{
		TUniquePtr<FAngelscriptEngine> Engine = CreateCatalogEngine(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsTrue(Engine.IsValid()));
		FAngelscriptBinds Binds(*Engine);

		FAngelscriptBoundFunction HeaderFn = Binds.BindGlobalFunctionForTarget(
			"int NativeFormCatalogHeaderInline(int)",
			&CatalogHeaderInlineProbe);
		HeaderFn.NativeFunctionHeader(
			"CatalogHeaderInlineProbe",
			"NativeFormCatalogHeaderInline.h",
			true);

		FAngelscriptBoundFunction ExportedFn = Binds.BindGlobalFunctionForTarget(
			"int NativeFormCatalogModuleExported(int)",
			&CatalogModuleExportedProbe);
		ExportedFn.NativeFunction("CatalogModuleExportedProbe", true);

		FAngelscriptBoundFunction BridgeFn = Binds.BindGlobalFunctionForTarget(
			"int NativeFormCatalogBridge(int)",
			&CatalogBridgeProbe);
		asIScriptFunction* const BridgeScript = BridgeFn.GetFunction();
		ASSERT_THAT(IsNotNull(BridgeScript));
		ASSERT_THAT(IsTrue(BridgeFn.ExternalNativeCall(
			MakeBridgeDescriptor(*BridgeScript)).IsValid()));

		FAngelscriptNativeFormCatalog& Catalog =
			FAngelscriptNativeFormCatalog::Get();
		const FAngelscriptNativeFormRecipe* HeaderRecipe = Catalog.Find(
			FAngelscriptNativeFormCatalog::MakeKey(
				*Engine, *HeaderFn.GetFunction()));
		const FAngelscriptNativeFormRecipe* ExportedRecipe = Catalog.Find(
			FAngelscriptNativeFormCatalog::MakeKey(
				*Engine, *ExportedFn.GetFunction()));
		const FAngelscriptNativeFormRecipe* BridgeRecipe = Catalog.Find(
			FAngelscriptNativeFormCatalog::MakeKey(
				*Engine, *BridgeFn.GetFunction()));
		ASSERT_THAT(IsNotNull(HeaderRecipe));
		ASSERT_THAT(IsNotNull(ExportedRecipe));
		ASSERT_THAT(IsNotNull(BridgeRecipe));
		ASSERT_THAT(AreEqual(
			EAngelscriptNativeFormRecipeKind::HeaderInline,
			HeaderRecipe->Kind));
		ASSERT_THAT(AreEqual(
			EAngelscriptNativeFormRecipeKind::ModuleExported,
			ExportedRecipe->Kind));
		ASSERT_THAT(AreEqual(
			EAngelscriptNativeFormRecipeKind::Bridge,
			BridgeRecipe->Kind));
		ASSERT_THAT(AreEqual(
			FString(TEXT("NativeFormCatalogHeaderInline.h")),
			HeaderRecipe->Description.Header));
		ASSERT_THAT(IsTrue(HeaderRecipe->Description.Header.Len() > 0));
		ASSERT_THAT(IsTrue(ExportedRecipe->Description.Header.IsEmpty()));
		ASSERT_THAT(IsFalse(BridgeRecipe->Descriptor.IsAbsent()));
	}

	TEST_METHOD(StableDeclarationAndProfileKeyingDoesNotAlias)
	{
		TUniquePtr<FAngelscriptEngine> EditorEngine = CreateCatalogEngine(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		TUniquePtr<FAngelscriptEngine> ShippingEngine = CreateCatalogEngine(
			EAngelscriptJITTargetProfile::GameShipping);
		ASSERT_THAT(IsTrue(EditorEngine.IsValid() && ShippingEngine.IsValid()));

		FAngelscriptBinds EditorBinds(*EditorEngine);
		FAngelscriptBoundFunction EditorFn =
			EditorBinds.BindGlobalFunctionForTarget(
				"int NativeFormCatalogProfileKey(int)",
				&CatalogHeaderInlineProbe);
		EditorFn.NativeFunctionHeader(
			"CatalogHeaderInlineProbe",
			"NativeFormCatalogProfileKey.h",
			true);

		FAngelscriptBinds ShippingBinds(*ShippingEngine);
		FAngelscriptBoundFunction ShippingFn =
			ShippingBinds.BindGlobalFunctionForTarget(
				"int NativeFormCatalogProfileKey(int)",
				&CatalogHeaderInlineProbe);
		ShippingFn.NativeFunctionHeader(
			"CatalogHeaderInlineProbe",
			"NativeFormCatalogProfileKey.h",
			true);

		const FAngelscriptNativeFormCatalogKey EditorKey =
			FAngelscriptNativeFormCatalog::MakeKey(
				*EditorEngine, *EditorFn.GetFunction());
		const FAngelscriptNativeFormCatalogKey ShippingKey =
			FAngelscriptNativeFormCatalog::MakeKey(
				*ShippingEngine, *ShippingFn.GetFunction());
		ASSERT_THAT(AreEqual(
			EditorKey.StableDeclarationIdentity,
			ShippingKey.StableDeclarationIdentity));
		ASSERT_THAT(AreEqual(
			EAngelscriptJITTargetProfile::EditorDevelopment,
			EditorKey.TargetProfile));
		ASSERT_THAT(AreEqual(
			EAngelscriptJITTargetProfile::GameShipping,
			ShippingKey.TargetProfile));
		ASSERT_THAT(IsFalse(EditorKey == ShippingKey));
		FAngelscriptNativeFormCatalog& Catalog =
			FAngelscriptNativeFormCatalog::Get();
		ASSERT_THAT(IsNotNull(Catalog.Find(EditorKey)));
		ASSERT_THAT(IsNotNull(Catalog.Find(ShippingKey)));
	}

	TEST_METHOD(RepeatedEngineReplayValidatesWithoutGrowingTheCatalog)
	{
		TUniquePtr<FAngelscriptEngine> EngineA = CreateCatalogEngine(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsTrue(EngineA.IsValid()));
		FAngelscriptBinds BindsA(*EngineA);
		FAngelscriptBoundFunction FunctionA = BindsA.BindGlobalFunctionForTarget(
			"int NativeFormCatalogReplay(int)",
			&CatalogHeaderInlineProbe);
		FunctionA.NativeFunctionHeader(
			"CatalogHeaderInlineProbe",
			"NativeFormCatalogReplay.h",
			true);
		const FAngelscriptNativeFormCatalogKey Key =
			FAngelscriptNativeFormCatalog::MakeKey(
				*EngineA, *FunctionA.GetFunction());
		const int32 CountAfterFirst =
			FAngelscriptNativeFormCatalog::Get().NumRecipes();
		ASSERT_THAT(IsTrue(CountAfterFirst > 0));

		EngineA.Reset();

		TUniquePtr<FAngelscriptEngine> EngineB = CreateCatalogEngine(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsTrue(EngineB.IsValid()));
		FAngelscriptBinds BindsB(*EngineB);
		FAngelscriptBoundFunction FunctionB = BindsB.BindGlobalFunctionForTarget(
			"int NativeFormCatalogReplay(int)",
			&CatalogHeaderInlineProbe);
		FunctionB.NativeFunctionHeader(
			"CatalogHeaderInlineProbe",
			"NativeFormCatalogReplay.h",
			true);
		ASSERT_THAT(AreEqual(
			CountAfterFirst,
			FAngelscriptNativeFormCatalog::Get().NumRecipes()));
		const FAngelscriptNativeFormRecipe* Found =
			FAngelscriptNativeFormCatalog::Get().Find(Key);
		ASSERT_THAT(IsNotNull(Found));
		ASSERT_THAT(AreEqual(
			FString(TEXT("CatalogHeaderInlineProbe")),
			Found->Description.Name));
	}

	TEST_METHOD(SecondEngineLookupDoesNotNeedTheFirstEnginePointer)
	{
		TUniquePtr<FAngelscriptEngine> EngineA = CreateCatalogEngine(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsTrue(EngineA.IsValid()));
		FAngelscriptBinds BindsA(*EngineA);
		FAngelscriptBoundFunction FunctionA = BindsA.BindGlobalFunctionForTarget(
			"int NativeFormCatalogLease(int)",
			&CatalogHeaderInlineProbe);
		FunctionA.NativeFunctionHeader(
			"CatalogHeaderInlineProbe",
			"NativeFormCatalogLease.h",
			true);
		const FAngelscriptNativeFormCatalogKey Key =
			FAngelscriptNativeFormCatalog::MakeKey(
				*EngineA, *FunctionA.GetFunction());
		EngineA.Reset();

		TUniquePtr<FAngelscriptEngine> EngineB = CreateCatalogEngine(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsTrue(EngineB.IsValid()));
		const FAngelscriptNativeFormRecipe* Found =
			FAngelscriptNativeFormCatalog::Get().Find(Key);
		ASSERT_THAT(IsNotNull(Found));
		ASSERT_THAT(AreEqual(
			EAngelscriptNativeFormRecipeKind::HeaderInline,
			Found->Kind));
		ASSERT_THAT(AreEqual(
			FString(TEXT("NativeFormCatalogLease.h")),
			Found->Description.Header));
	}

	TEST_METHOD(CatalogDoesNotOwnFunctionPointersOrLambdas)
	{
		TUniquePtr<FAngelscriptEngine> Engine = CreateCatalogEngine(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsTrue(Engine.IsValid()));
		FAngelscriptBinds Binds(*Engine);
		auto Probe = []()
		{
			return CatalogLambdaProbe();
		};
		FAngelscriptBoundFunction Function = Binds.BindGlobalFunctionForTarget(
			"int NativeFormCatalogLambda()",
			Probe);
		Function.NativeFunction("CatalogLambdaProbe", true);
		const FAngelscriptNativeFormCatalogKey Key =
			FAngelscriptNativeFormCatalog::MakeKey(
				*Engine, *Function.GetFunction());
		Engine.Reset();

		const FAngelscriptNativeFormRecipe* Found =
			FAngelscriptNativeFormCatalog::Get().Find(Key);
		ASSERT_THAT(IsNotNull(Found));
		ASSERT_THAT(AreEqual(
			FString(TEXT("CatalogLambdaProbe")),
			Found->Description.Name));
		ASSERT_THAT(AreEqual(
			EAngelscriptNativeFormRecipeKind::ModuleExported,
			Found->Kind));
	}
};

#endif
