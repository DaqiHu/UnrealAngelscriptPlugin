#include "CQTest.h"

#include "AngelscriptTestEngine.h"
#include "AngelscriptTestJITProbes.h"
#include "Core/AngelscriptBinds.h"
#include "StaticJIT/AngelscriptJITReferenceResolver.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallDescriptor.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITBridgeABI.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Testing/AngelscriptScriptTestRunner.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_thread.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptTypedASTJITNativeBridgeTests_Private
{
	using namespace AngelscriptNativeTestSupport;

	static int32 AddCallCount = 0;
	static int32 OtherCallCount = 0;
	static int32 ExceptionCallCount = 0;
	static int32 VoidCallValue = 0;
	static asCScriptFunction* LastScriptFunctionMetadata = nullptr;
	static asCObjectType* LastScriptObjectTypeMetadata = nullptr;
	static int32 ScriptFunctionMetadataCallCount = 0;
	static int32 ScriptObjectTypeMetadataCallCount = 0;

	struct FFirstParamMetadataValue
	{
		int32 Base = 0;
	};

	static int32 CaptureScriptFunctionMetadata(
		asCScriptFunction* ScriptFunction,
		const int32 Value)
	{
		LastScriptFunctionMetadata = ScriptFunction;
		++ScriptFunctionMetadataCallCount;
		return Value + 1000;
	}

	static int32 CaptureScriptObjectTypeMetadata(
		FFirstParamMetadataValue& Object,
		asCObjectType* ScriptObjectType,
		const int32 Value)
	{
		LastScriptObjectTypeMetadata = ScriptObjectType;
		++ScriptObjectTypeMetadataCallCount;
		return Object.Base + Value;
	}

	static int32 NativeBridgeAdd(const int32 Left, const int32 Right)
	{
		++AddCallCount;
		return Left + Right;
	}

	static int32 NativeBridgeOther(const int32 Left, const int32 Right)
	{
		++OtherCallCount;
		return (Left * 100) + Right;
	}

	static void NativeBridgeVoid(const int32 Value)
	{
		VoidCallValue = Value;
	}

	static double NativeBridgeScalarMix(
		const double Wide,
		const float Narrow,
		const int16 SignedSmall,
		const uint8 UnsignedSmall,
		const bool bAdd)
	{
		return Wide + Narrow + SignedSmall + UnsignedSmall
			+ (bAdd ? 0.5 : -0.5);
	}

	static void NativeBridgeException(asIScriptGeneric* Generic)
	{
		++ExceptionCallCount;
		asIScriptContext* const Context = asGetActiveContext();
		if (Context != nullptr)
		{
			Context->SetException("TypedASTJIT native bridge expected exception");
		}
		Generic->SetReturnDWord(999);
	}

	static asCScriptFunction* GetRegisteredFunction(
		asIScriptEngine& Engine,
		const int32 FunctionId)
	{
		return FunctionId >= 0
			? static_cast<asCScriptFunction*>(Engine.GetFunctionById(FunctionId))
			: nullptr;
	}

	static FAngelscriptJITResolvedReferenceTable MakeTable(
		asCScriptFunction* Function,
		const EAngelscriptArtifactReferenceKind Kind =
			EAngelscriptArtifactReferenceKind::EnvironmentSymbol)
	{
		FAngelscriptJITResolvedReferenceTable Table;
		FAngelscriptJITResolvedReferenceSlot& Slot =
			Table.Slots.AddDefaulted_GetRef();
		Slot.SlotIndex = 0;
		Slot.Kind = Kind;
		Slot.Value = Function;
		return Table;
	}

	static FAngelscriptTypedASTJITBoundCallSite MakeCallSite(
		const int32 ReferenceSlotIndex = 0)
	{
		FAngelscriptTypedASTJITBoundCallSite CallSite;
		CallSite.CanonicalDeclaration =
			TEXT("int NativeBridgeAdd(int Left, int Right)");
		CallSite.RegisteredTarget = TEXT("FPrivateBinds::NativeBridgeAdd");
		CallSite.ReferenceSlotIndex = ReferenceSlotIndex;
		return CallSite;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITNativeBridgeTests,
	"Angelscript.TestModule.StaticJIT.NativeBridge.TypedASTJIT",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(FirstParameterMetadataUsesTheCurrentEngineAuthoritativeVMDispatch)
	{
		using namespace AngelscriptTypedASTJITNativeBridgeTests_Private;

		FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA =
			FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB =
			FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsTrue(EngineA.IsValid() && EngineB.IsValid()));
		if (!EngineA.IsValid() || !EngineB.IsValid())
		{
			return;
		}

		struct FRegisteredSurface
		{
			asCScriptFunction* ScriptFunctionMetadata = nullptr;
			asCScriptFunction* ScriptObjectTypeMetadata = nullptr;
			asCObjectType* ObjectType = nullptr;
		};
		auto RegisterSurface = [](FAngelscriptEngine& Engine)
		{
			FAngelscriptBinds Binds(Engine);
			FAngelscriptBoundFunction ScriptFunction =
				Binds.BindGlobalFunctionForTarget(
					"int32 TypedASTCaptureScriptFunctionMetadata(int32 Value)",
					&CaptureScriptFunctionMetadata)
				.PassScriptFunctionAsFirstParam();

			FBindFlags TypeFlags;
			TypeFlags.bPOD = true;
			FAngelscriptBinds MetadataType =
				Binds.ValueClassForTarget<FFirstParamMetadataValue>(
					"FTypedASTFirstParamMetadataValue", TypeFlags);
			FAngelscriptBoundFunction ScriptObjectType = MetadataType.Method(
				"int32 CaptureScriptObjectTypeMetadata(int32 Value)",
				&CaptureScriptObjectTypeMetadata)
				.PassScriptObjectTypeAsFirstParam();

			FRegisteredSurface Surface;
			Surface.ScriptFunctionMetadata = static_cast<asCScriptFunction*>(
				ScriptFunction.GetFunction());
			Surface.ScriptObjectTypeMetadata = static_cast<asCScriptFunction*>(
				ScriptObjectType.GetFunction());
			Surface.ObjectType = Surface.ScriptObjectTypeMetadata != nullptr
				? Surface.ScriptObjectTypeMetadata->objectType : nullptr;
			return Surface;
		};

		const FRegisteredSurface SurfaceA = RegisterSurface(*EngineA);
		const FRegisteredSurface SurfaceB = RegisterSurface(*EngineB);
		ASSERT_THAT(IsNotNull(SurfaceA.ScriptFunctionMetadata));
		ASSERT_THAT(IsNotNull(SurfaceA.ScriptObjectTypeMetadata));
		ASSERT_THAT(IsNotNull(SurfaceA.ObjectType));
		ASSERT_THAT(IsNotNull(SurfaceB.ScriptFunctionMetadata));
		ASSERT_THAT(IsNotNull(SurfaceB.ScriptObjectTypeMetadata));
		ASSERT_THAT(IsNotNull(SurfaceB.ObjectType));
		if (SurfaceA.ScriptFunctionMetadata == nullptr
			|| SurfaceA.ScriptObjectTypeMetadata == nullptr
			|| SurfaceA.ObjectType == nullptr
			|| SurfaceB.ScriptFunctionMetadata == nullptr
			|| SurfaceB.ScriptObjectTypeMetadata == nullptr
			|| SurfaceB.ObjectType == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SurfaceA.ScriptFunctionMetadata
			!= SurfaceB.ScriptFunctionMetadata));
		ASSERT_THAT(IsTrue(SurfaceA.ObjectType != SurfaceB.ObjectType));

		struct FMetadataABIExpectation
		{
			asCScriptFunction* Function = nullptr;
			asEFirstParamMetaData Metadata = asEFirstParamMetaData::None;
		};
		const FMetadataABIExpectation ABIExpectations[] = {
			{SurfaceA.ScriptFunctionMetadata,
				asEFirstParamMetaData::ScriptFunction},
			{SurfaceA.ScriptObjectTypeMetadata,
				asEFirstParamMetaData::ScriptObjectType},
		};
		for (const FMetadataABIExpectation& Expectation : ABIExpectations)
		{
			ASSERT_THAT(IsNotNull(Expectation.Function->sysFuncIntf));
			ASSERT_THAT(AreEqual(
				Expectation.Metadata,
				Expectation.Function->sysFuncIntf->passFirstParamMetaData));

			const FString MetadataToken = FString::Printf(
				TEXT("firstMetadata=%u"),
				static_cast<uint32>(Expectation.Metadata));
			FAngelscriptStaticJITNativeABIIdentity NativeABI;
			FAngelscriptTypedASTJITVMBridgeABIIdentity VMBridgeABI;
			FAngelscriptStaticJITScalarABIIdentity ScalarABI;
			FString Error;
			ASSERT_THAT(IsTrue(TryBuildAngelscriptStaticJITNativeABIIdentity(
				*Expectation.Function, NativeABI, &Error), Error));
			ASSERT_THAT(IsTrue(NativeABI.Canonical.Contains(MetadataToken)));
			ASSERT_THAT(IsTrue(
				TryBuildAngelscriptTypedASTJITVMBridgeABIIdentity(
					*Expectation.Function, VMBridgeABI, &Error), Error));
			ASSERT_THAT(IsTrue(VMBridgeABI.Canonical.Contains(MetadataToken)));
			ASSERT_THAT(IsFalse(TryBuildAngelscriptStaticJITScalarABIIdentity(
				*Expectation.Function, ScalarABI, &Error)));
			ASSERT_THAT(IsTrue(Error.Contains(
				TEXT("NativeScalarABIUnsupportedHiddenState"))));
		}

		LastScriptFunctionMetadata = nullptr;
		ScriptFunctionMetadataCallCount = 0;
		{
			asIScriptContext* const Context =
				EngineA->GetScriptEngine()->CreateContext();
			ASSERT_THAT(IsNotNull(Context));
			if (Context != nullptr)
			{
				ASSERT_THAT(AreEqual(asSUCCESS,
					Context->Prepare(SurfaceA.ScriptFunctionMetadata)));
				ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 17)));
				ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
				ASSERT_THAT(AreEqual(1017,
					static_cast<int32>(Context->GetReturnDWord())));
				Context->Release();
			}
		}
		ASSERT_THAT(IsTrue(LastScriptFunctionMetadata
			== SurfaceA.ScriptFunctionMetadata));
		ASSERT_THAT(AreEqual(1, ScriptFunctionMetadataCallCount));

		LastScriptObjectTypeMetadata = nullptr;
		ScriptObjectTypeMetadataCallCount = 0;
		{
			FFirstParamMetadataValue Object;
			Object.Base = 700;
			asIScriptContext* const Context =
				EngineA->GetScriptEngine()->CreateContext();
			ASSERT_THAT(IsNotNull(Context));
			if (Context != nullptr)
			{
				ASSERT_THAT(AreEqual(asSUCCESS,
					Context->Prepare(SurfaceA.ScriptObjectTypeMetadata)));
				ASSERT_THAT(AreEqual(asSUCCESS, Context->SetObject(&Object)));
				ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 23)));
				ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
				ASSERT_THAT(AreEqual(723,
					static_cast<int32>(Context->GetReturnDWord())));
				Context->Release();
			}
		}
		ASSERT_THAT(IsTrue(LastScriptObjectTypeMetadata == SurfaceA.ObjectType));
		ASSERT_THAT(AreEqual(1, ScriptObjectTypeMetadataCallCount));

		FAngelscriptTypedASTJITBoundCallSite CallSite = MakeCallSite();
		CallSite.CanonicalDeclaration =
			TEXT("int32 TypedASTCaptureScriptFunctionMetadata(int32 Value)");
		CallSite.RegisteredTarget =
			TEXT("CaptureScriptFunctionMetadata");

		LastScriptFunctionMetadata = nullptr;
		ScriptFunctionMetadataCallCount = 0;
		FAngelscriptJITResolvedReferenceTable TableA =
			MakeTable(SurfaceA.ScriptFunctionMetadata);
		FScriptExecution ExecutionA(asCThreadManager::GetLocalData());
		ExecutionA.jitEngine = EngineA->GetScriptEngine();
		ExecutionA.jitReferenceTable = &TableA;
		ASSERT_THAT(AreEqual(1031,
			AngelscriptTypedASTJIT::InvokeBound<int32, int32>(
				ExecutionA, CallSite, 31)));
		ASSERT_THAT(IsTrue(LastScriptFunctionMetadata
			== SurfaceA.ScriptFunctionMetadata));
		ASSERT_THAT(AreEqual(1, ScriptFunctionMetadataCallCount));
		ASSERT_THAT(IsFalse(ExecutionA.bExceptionThrown));

		LastScriptFunctionMetadata = nullptr;
		FAngelscriptJITResolvedReferenceTable TableB =
			MakeTable(SurfaceB.ScriptFunctionMetadata);
		FScriptExecution ExecutionB(asCThreadManager::GetLocalData());
		ExecutionB.jitEngine = EngineB->GetScriptEngine();
		ExecutionB.jitReferenceTable = &TableB;
		ASSERT_THAT(AreEqual(1037,
			AngelscriptTypedASTJIT::InvokeBound<int32, int32>(
				ExecutionB, CallSite, 37)));
		ASSERT_THAT(IsTrue(LastScriptFunctionMetadata
			== SurfaceB.ScriptFunctionMetadata));
		ASSERT_THAT(AreEqual(2, ScriptFunctionMetadataCallCount));
		ASSERT_THAT(IsFalse(ExecutionB.bExceptionThrown));

		LastScriptFunctionMetadata = nullptr;
		const int32 CallsBeforeRejectedNative =
			ScriptFunctionMetadataCallCount;
		FScriptExecution RejectedNativeExecution(
			asCThreadManager::GetLocalData());
		RejectedNativeExecution.jitEngine = EngineA->GetScriptEngine();
		RejectedNativeExecution.jitReferenceTable = &TableA;
		ASSERT_THAT(AreEqual(0,
			AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32>(
				RejectedNativeExecution, CallSite, 41)));
		ASSERT_THAT(IsTrue(RejectedNativeExecution.bExceptionThrown));
		ASSERT_THAT(AreEqual(
			CallsBeforeRejectedNative, ScriptFunctionMetadataCallCount));
		ASSERT_THAT(IsNull(LastScriptFunctionMetadata));
	}

	TEST_METHOD(ScalarArgumentsAndResultUseTheCurrentEngineBoundSlot)
	{
		using namespace AngelscriptTypedASTJITNativeBridgeTests_Private;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const ASAutoCaller::FunctionCaller Caller =
			ASAutoCaller::MakeFunctionCaller(NativeBridgeAdd);
		const int32 FunctionId = ScriptEngine->RegisterGlobalFunction(
			"int NativeBridgeAdd(int Left, int Right)",
			asFUNCTION(NativeBridgeAdd),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		asCScriptFunction* const Function =
			GetRegisteredFunction(*ScriptEngine, FunctionId);
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		FAngelscriptJITResolvedReferenceTable Table = MakeTable(Function);
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Execution.jitEngine = ScriptEngine;
		Execution.jitReferenceTable = &Table;
		AddCallCount = 0;
		const int32 Result = AngelscriptTypedASTJIT::InvokeBound<
			int32, int32, int32>(Execution, MakeCallSite(), 17, 25);

		ASSERT_THAT(AreEqual(42, Result));
		ASSERT_THAT(AreEqual(1, AddCallCount));
		ASSERT_THAT(IsFalse(Execution.bExceptionThrown));
	}

	TEST_METHOD(ExportedNativeCallLoadsTheActualFunctionFromTheCurrentSlot)
	{
		using namespace AngelscriptTypedASTJITNativeBridgeTests_Private;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const ASAutoCaller::FunctionCaller AddCaller =
			ASAutoCaller::MakeFunctionCaller(NativeBridgeAdd);
		const ASAutoCaller::FunctionCaller OtherCaller =
			ASAutoCaller::MakeFunctionCaller(NativeBridgeOther);
		const int32 AddId = ScriptEngine->RegisterGlobalFunction(
			"int CurrentNativeAdd(int Left, int Right)",
			asFUNCTION(NativeBridgeAdd),
			asCALL_CDECL,
			*(asFunctionCaller*)&AddCaller);
		const int32 OtherId = ScriptEngine->RegisterGlobalFunction(
			"int CurrentNativeOther(int Left, int Right)",
			asFUNCTION(NativeBridgeOther),
			asCALL_CDECL,
			*(asFunctionCaller*)&OtherCaller);
		asCScriptFunction* const AddFunction =
			GetRegisteredFunction(*ScriptEngine, AddId);
		asCScriptFunction* const OtherFunction =
			GetRegisteredFunction(*ScriptEngine, OtherId);
		ASSERT_THAT(IsNotNull(AddFunction));
		ASSERT_THAT(IsNotNull(OtherFunction));
		if (AddFunction == nullptr || OtherFunction == nullptr)
		{
			return;
		}

		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Execution.jitEngine = ScriptEngine;
		FAngelscriptJITResolvedReferenceTable Table = MakeTable(AddFunction);
		Execution.jitReferenceTable = &Table;
		AddCallCount = 0;
		OtherCallCount = 0;
		ASSERT_THAT(AreEqual(
			42,
			AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>(
				Execution, MakeCallSite(), 17, 25)));
		ASSERT_THAT(AreEqual(1, AddCallCount));
		ASSERT_THAT(AreEqual(0, OtherCallCount));

		Table = MakeTable(OtherFunction);
		Execution.jitReferenceTable = &Table;
		ASSERT_THAT(AreEqual(
			1725,
			AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>(
				Execution, MakeCallSite(), 17, 25)));
		ASSERT_THAT(AreEqual(1, AddCallCount));
		ASSERT_THAT(AreEqual(1, OtherCallCount));
		ASSERT_THAT(IsFalse(Execution.bExceptionThrown));
	}

	TEST_METHOD(VoidExportedNativeCallUsesTheActualFunctionFromTheCurrentSlot)
	{
		using namespace AngelscriptTypedASTJITNativeBridgeTests_Private;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const ASAutoCaller::FunctionCaller Caller =
			ASAutoCaller::MakeFunctionCaller(NativeBridgeVoid);
		const int32 FunctionId = ScriptEngine->RegisterGlobalFunction(
			"void CurrentNativeVoid(int Value)",
			asFUNCTION(NativeBridgeVoid),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		asCScriptFunction* const Function =
			GetRegisteredFunction(*ScriptEngine, FunctionId);
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		FAngelscriptJITResolvedReferenceTable Table = MakeTable(Function);
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Execution.jitEngine = ScriptEngine;
		Execution.jitReferenceTable = &Table;
		VoidCallValue = 0;
		AngelscriptTypedASTJIT::InvokeBoundNative<void, int32>(
			Execution, MakeCallSite(), 73);

		ASSERT_THAT(AreEqual(73, VoidCallValue));
		ASSERT_THAT(IsFalse(Execution.bExceptionThrown));
	}

	TEST_METHOD(SeparateTestJITModuleImportsTheFixedRuntimeBridgeCore)
	{
		using namespace AngelscriptTypedASTJITNativeBridgeTests_Private;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const ASAutoCaller::FunctionCaller Caller =
			ASAutoCaller::MakeFunctionCaller(NativeBridgeAdd);
		const int32 FunctionId = ScriptEngine->RegisterGlobalFunction(
			"int NativeBridgeAdd(int Left, int Right)",
			asFUNCTION(NativeBridgeAdd),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		asCScriptFunction* const Function =
			GetRegisteredFunction(*ScriptEngine, FunctionId);
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		FAngelscriptJITResolvedReferenceTable Table = MakeTable(Function);
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Execution.jitEngine = ScriptEngine;
		Execution.jitReferenceTable = &Table;
		AddCallCount = 0;
		const int32 Result =
			AngelscriptTestJITProbes::ExecuteTypedASTNativeBridge(
				Execution, MakeCallSite(), 19, 23);

		ASSERT_THAT(AreEqual(42, Result));
		ASSERT_THAT(AreEqual(1, AddCallCount));
		ASSERT_THAT(IsFalse(Execution.bExceptionThrown));
	}

	TEST_METHOD(ScalarKindsPreserveTheirExactAngelScriptABI)
	{
		using namespace AngelscriptTypedASTJITNativeBridgeTests_Private;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const ASAutoCaller::FunctionCaller Caller =
			ASAutoCaller::MakeFunctionCaller(NativeBridgeScalarMix);
		const int32 FunctionId = ScriptEngine->RegisterGlobalFunction(
			"float64 NativeBridgeScalarMix(float64 Wide, float32 Narrow, int16 SignedSmall, uint8 UnsignedSmall, bool bAdd)",
			asFUNCTION(NativeBridgeScalarMix),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		ASSERT_THAT(IsTrue(FunctionId >= 0,
			*FString::Printf(
				TEXT("Mixed scalar target registration failed. Result=%d Messages={%s}"),
				FunctionId,
				*Engine.GetMessagesText())));
		asCScriptFunction* const Function =
			GetRegisteredFunction(*ScriptEngine, FunctionId);
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		FAngelscriptJITResolvedReferenceTable Table = MakeTable(Function);
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Execution.jitEngine = ScriptEngine;
		Execution.jitReferenceTable = &Table;
		FAngelscriptTypedASTJITBoundCallSite CallSite = MakeCallSite();
		CallSite.CanonicalDeclaration = TEXT(
			"float64 NativeBridgeScalarMix(float64, float32, int16, uint8, bool)");
		const double Result = AngelscriptTypedASTJIT::InvokeBound<
			double, double, float, int16, uint8, bool>(
				Execution, CallSite, 10.25, 1.5f, int16(-2), uint8(4), true);

		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, 14.25)));
		ASSERT_THAT(IsFalse(Execution.bExceptionThrown));
	}

	TEST_METHOD(DiagnosticStringsCannotRedirectTheResolvedSlot)
	{
		using namespace AngelscriptTypedASTJITNativeBridgeTests_Private;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const ASAutoCaller::FunctionCaller AddCaller =
			ASAutoCaller::MakeFunctionCaller(NativeBridgeAdd);
		const ASAutoCaller::FunctionCaller OtherCaller =
			ASAutoCaller::MakeFunctionCaller(NativeBridgeOther);
		const int32 AddId = ScriptEngine->RegisterGlobalFunction(
			"int NativeBridgeAdd(int Left, int Right)",
			asFUNCTION(NativeBridgeAdd), asCALL_CDECL,
			*(asFunctionCaller*)&AddCaller);
		const int32 OtherId = ScriptEngine->RegisterGlobalFunction(
			"int NativeBridgeOther(int Left, int Right)",
			asFUNCTION(NativeBridgeOther), asCALL_CDECL,
			*(asFunctionCaller*)&OtherCaller);
		asCScriptFunction* const AddFunction =
			GetRegisteredFunction(*ScriptEngine, AddId);
		asCScriptFunction* const OtherFunction =
			GetRegisteredFunction(*ScriptEngine, OtherId);
		ASSERT_THAT(IsNotNull(AddFunction));
		ASSERT_THAT(IsNotNull(OtherFunction));
		if (AddFunction == nullptr || OtherFunction == nullptr)
		{
			return;
		}

		FAngelscriptJITResolvedReferenceTable Table = MakeTable(AddFunction);
		FAngelscriptJITResolvedReferenceSlot& OtherSlot =
			Table.Slots.AddDefaulted_GetRef();
		OtherSlot.SlotIndex = 1;
		OtherSlot.Kind = EAngelscriptArtifactReferenceKind::EnvironmentSymbol;
		OtherSlot.Value = OtherFunction;
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Execution.jitEngine = ScriptEngine;
		Execution.jitReferenceTable = &Table;
		FAngelscriptTypedASTJITBoundCallSite Misleading = MakeCallSite();
		Misleading.CanonicalDeclaration =
			TEXT("int NativeBridgeOther(int Left, int Right)");
		Misleading.RegisteredTarget = TEXT("FPrivateBinds::NativeBridgeOther");
		AddCallCount = OtherCallCount = 0;
		const int32 Result = AngelscriptTypedASTJIT::InvokeBound<
			int32, int32, int32>(Execution, Misleading, 2, 3);
		FAngelscriptTypedASTJITBoundCallSite DuplicateDisplay = Misleading;
		DuplicateDisplay.ReferenceSlotIndex = 1;
		const int32 OtherResult = AngelscriptTypedASTJIT::InvokeBound<
			int32, int32, int32>(Execution, DuplicateDisplay, 2, 3);

		ASSERT_THAT(AreEqual(5, Result));
		ASSERT_THAT(AreEqual(203, OtherResult));
		ASSERT_THAT(AreEqual(1, AddCallCount));
		ASSERT_THAT(AreEqual(1, OtherCallCount));
		ASSERT_THAT(IsFalse(Execution.bExceptionThrown));
	}

	TEST_METHOD(MissingWrongKindAndWrongEngineSlotsFailClosedBeforeCalling)
	{
		using namespace AngelscriptTypedASTJITNativeBridgeTests_Private;
		FNativeTestEngine FirstEngine;
		FNativeTestEngine OtherEngine;
		FirstEngine.Create(*TestRunner);
		OtherEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			OtherEngine.Destroy();
			FirstEngine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = FirstEngine.Get();
		asIScriptEngine* const DifferentScriptEngine = OtherEngine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		ASSERT_THAT(IsNotNull(DifferentScriptEngine));
		if (ScriptEngine == nullptr || DifferentScriptEngine == nullptr)
		{
			return;
		}

		const ASAutoCaller::FunctionCaller Caller =
			ASAutoCaller::MakeFunctionCaller(NativeBridgeAdd);
		const int32 FunctionId = ScriptEngine->RegisterGlobalFunction(
			"int NativeBridgeAdd(int Left, int Right)",
			asFUNCTION(NativeBridgeAdd), asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		asCScriptFunction* const Function =
			GetRegisteredFunction(*ScriptEngine, FunctionId);
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}
		AddCallCount = 0;

		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Execution.jitEngine = ScriptEngine;
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITBridgeResult::MissingReferenceTable,
			AngelscriptTypedASTJIT::InvokeBoundViaVM(
				Execution,
				MakeCallSite(),
				nullptr,
				0,
				EAngelscriptTypedASTJITScalarKind::Void,
				nullptr)));
		ASSERT_THAT(IsTrue(Execution.bExceptionThrown));

		Execution.bExceptionThrown = false;
		FAngelscriptJITResolvedReferenceTable Table = MakeTable(Function);
		Execution.jitReferenceTable = &Table;
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITBridgeResult::MissingReferenceSlot,
			AngelscriptTypedASTJIT::InvokeBoundViaVM(
				Execution, MakeCallSite(7), nullptr, 0,
				EAngelscriptTypedASTJITScalarKind::Void, nullptr)));

		Execution.bExceptionThrown = false;
		Table = MakeTable(nullptr);
		Execution.jitReferenceTable = &Table;
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITBridgeResult::MissingFunction,
			AngelscriptTypedASTJIT::InvokeBoundViaVM(
				Execution, MakeCallSite(), nullptr, 0,
				EAngelscriptTypedASTJITScalarKind::Void, nullptr)));

		Execution.bExceptionThrown = false;
		Table = MakeTable(Function);
		Execution.jitReferenceTable = &Table;
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITBridgeResult::SignatureMismatch,
			AngelscriptTypedASTJIT::InvokeBoundViaVM(
				Execution, MakeCallSite(), nullptr, 0,
				EAngelscriptTypedASTJITScalarKind::Void, nullptr)));

		Execution.bExceptionThrown = false;
		Table = MakeTable(Function,
			EAngelscriptArtifactReferenceKind::ScriptFunction);
		Execution.jitReferenceTable = &Table;
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITBridgeResult::WrongReferenceKind,
			AngelscriptTypedASTJIT::InvokeBoundViaVM(
				Execution, MakeCallSite(), nullptr, 0,
				EAngelscriptTypedASTJITScalarKind::Void, nullptr)));

		Execution.bExceptionThrown = false;
		Table = MakeTable(Function);
		Execution.jitReferenceTable = &Table;
		Execution.jitEngine = DifferentScriptEngine;
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITBridgeResult::WrongEngine,
			AngelscriptTypedASTJIT::InvokeBoundViaVM(
				Execution, MakeCallSite(), nullptr, 0,
				EAngelscriptTypedASTJITScalarKind::Void, nullptr)));
		ASSERT_THAT(AreEqual(
			0,
			AddCallCount,
			TEXT("Missing, unbound, mismatched and wrong-Engine slots must fail before invoking a retained caller")));
	}

	TEST_METHOD(NestedVMExceptionStopsTheTypedCallerAndReturnsNoValue)
	{
		using namespace AngelscriptTypedASTJITNativeBridgeTests_Private;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const int32 FunctionId = ScriptEngine->RegisterGlobalFunction(
			"int NativeBridgeException(int Value)",
			asFUNCTION(NativeBridgeException),
			asCALL_GENERIC);
		asCScriptFunction* const Function =
			GetRegisteredFunction(*ScriptEngine, FunctionId);
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		FAngelscriptJITResolvedReferenceTable Table = MakeTable(Function);
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		Execution.jitEngine = ScriptEngine;
		Execution.jitReferenceTable = &Table;
		ExceptionCallCount = 0;
		int32 Result = 0;
		{
			FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
			Result = AngelscriptTypedASTJIT::InvokeBound<int32, int32>(
				Execution, MakeCallSite(), 123);
		}

		ASSERT_THAT(AreEqual(0, Result,
			TEXT("A failing bridge must not expose the callee's return storage")));
		ASSERT_THAT(AreEqual(1, ExceptionCallCount));
		ASSERT_THAT(IsTrue(Execution.bExceptionThrown));
		const asSJITFailureRecord* const Failure = Execution.FailureRecord;
		ASSERT_THAT(IsNotNull(Failure));
		if (Failure == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Failure->bHasFailure));
		ASSERT_THAT(IsTrue(Failure->bAdopted));
		ASSERT_THAT(IsTrue(Failure->bReported));
		ASSERT_THAT(AreEqual(Function->GetId(), Failure->FunctionId));
		ASSERT_THAT(AreEqual(
			FString(TEXT("TypedASTJIT native bridge expected exception")),
			FString(UTF8_TO_TCHAR(Failure->Message.AddressOf()))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("context-adoption")),
			FString(UTF8_TO_TCHAR(Failure->RouteOrigin.AddressOf()))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("vm")),
			FString(UTF8_TO_TCHAR(Failure->BackendOrigin.AddressOf()))));
	}
};

#endif
