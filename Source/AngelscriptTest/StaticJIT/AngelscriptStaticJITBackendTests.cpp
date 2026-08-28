#include "StaticJIT/Backends/AngelscriptStaticJITBackend.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"
#include "StaticJIT/AngelscriptStaticJITGenerationProfile.h"

#include "AngelscriptEngine.h"

#include "CQTest.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

struct FAngelscriptRuntimeJITBackendId;

namespace AngelscriptStaticJITBackendTests_Private
{
	static FAngelscriptHash256 RepeatedByteHash(const uint8 Value)
	{
		FBlake3Hash::ByteArray Bytes{};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Bytes); ++Index)
		{
			Bytes[Index] = Value;
		}
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FAngelscriptJITGenerationModule MakeModule(
		const uint8 ModuleByte,
		const TCHAR* Name)
	{
		FAngelscriptJITGenerationModule Module;
		Module.ModuleKey.Hash = RepeatedByteHash(ModuleByte);
		Module.CanonicalModuleName = Name;
		Module.VirtualSourcePath = FString::Printf(
			TEXT("/Angelscript/Game/%s.as"), Name);
		return Module;
	}

	static FAngelscriptJITGenerationFunction MakeFunction(
		const uint8 ModuleByte,
		const uint8 FunctionByte,
		const TCHAR* Name)
	{
		FAngelscriptJITGenerationFunction Function;
		Function.ModuleKey.Hash = RepeatedByteHash(ModuleByte);
		Function.FunctionKey.Hash = RepeatedByteHash(FunctionByte);
		Function.ExecutionHash = RepeatedByteHash(FunctionByte + 1);
		Function.DebugHash = RepeatedByteHash(FunctionByte + 2);
		Function.EntryAbiHash = RepeatedByteHash(FunctionByte + 3);
		Function.CanonicalDeclaration = FString::Printf(
			TEXT("void %s()"), Name);
		Function.SourceSection = FString::Printf(
			TEXT("/Angelscript/Game/%s.as"), Name);
		Function.SourceLine = 7;
		Function.bHasVMEntry = true;
		Function.ImplementationTemplate =
			TEXT("{VM_ENTRY_COMMENT}void {VM_SYMBOL}(FScriptExecution&, asDWORD*, asQWORD*) {}\n");
		return Function;
	}

	static FAngelscriptJITProviderGenerationSettings MakeSettings()
	{
		FAngelscriptJITProviderGenerationSettings Settings;
		Settings.TargetProfile =
			EAngelscriptJITTargetProfile::EditorDevelopment;
		Settings.ProviderId.Hash = RepeatedByteHash(0x91);
		Settings.ProviderName = TEXT("BackendContractProvider");
		Settings.OwnerModuleName = TEXT("AngelscriptTest");
		Settings.ArtifactProfile.Hash = RepeatedByteHash(0xa1);
		Settings.NativeEnvironmentFingerprint = RepeatedByteHash(0xb1);
		return Settings;
	}

	class FNullFactory final : public IAngelscriptStaticJITBackendFactory
	{
	public:
		explicit FNullFactory(FAngelscriptStaticJITBackendId InBackendId)
			: BackendId(MoveTemp(InBackendId))
		{
		}

		FAngelscriptStaticJITBackendId GetBackendId() const override
		{
			return BackendId;
		}

		TUniquePtr<IAngelscriptStaticJITBackend> CreateBackend() const override
		{
			return nullptr;
		}

	private:
		FAngelscriptStaticJITBackendId BackendId;
	};

	class FRecordingFactory;

	class FRecordingBackend final : public IAngelscriptStaticJITBackend
	{
	public:
		explicit FRecordingBackend(const FRecordingFactory& InFactory)
			: Factory(InFactory)
		{
		}

		bool Generate(
			const FAngelscriptStaticJITBackendRequest& Request,
			FAngelscriptStaticJITBackendOutput& OutOutput,
			FString* OutError) override;

	private:
		const FRecordingFactory& Factory;
	};

	class FRecordingFactory final : public IAngelscriptStaticJITBackendFactory
	{
	public:
		explicit FRecordingFactory(FAngelscriptStaticJITBackendId InBackendId)
			: BackendId(MoveTemp(InBackendId))
		{
		}

		FAngelscriptStaticJITBackendId GetBackendId() const override
		{
			return BackendId;
		}

		TUniquePtr<IAngelscriptStaticJITBackend> CreateBackend() const override
		{
			++CreateCount;
			return MakeUnique<FRecordingBackend>(*this);
		}

		FAngelscriptStaticJITBackendId BackendId;
		FAngelscriptStaticJITBackendOutput Output;
		bool bReturnSuccess = true;
		mutable int32 CreateCount = 0;
		mutable int32 GenerateCount = 0;
		mutable int32 SeenFunctionCount = 0;
		mutable int32 SeenEmitModuleCount = 0;
		mutable FAngelscriptStaticJITBackendId SeenBackendId;
	};

	bool FRecordingBackend::Generate(
		const FAngelscriptStaticJITBackendRequest& Request,
		FAngelscriptStaticJITBackendOutput& OutOutput,
		FString* OutError)
	{
		++Factory.GenerateCount;
		Factory.SeenFunctionCount =
			Request.CompiledSourceGraph.Functions.Num();
		Factory.SeenEmitModuleCount = Request.EmitModuleSet.Num();
		Factory.SeenBackendId = Request.RequestedBackendId;
		OutOutput = Factory.Output;
		if (!Factory.bReturnSuccess && OutError != nullptr)
		{
			*OutError = TEXT("recording backend failure");
		}
		return Factory.bReturnSuccess;
	}

	static FAngelscriptStaticJITCompiledSourceGraph MakeGraph(
		const TConstArrayView<FAngelscriptStaticJITCompiledFunctionView> Functions)
	{
		FAngelscriptStaticJITCompiledSourceGraph Graph;
		Graph.Settings = MakeSettings();
		Graph.Functions = Functions;
		return Graph;
	}

	static FAngelscriptStaticJITFunctionResult Emitted(
		const FAngelscriptStaticJITBackendId BackendId,
		FAngelscriptJITGenerationFunction Function)
	{
		FAngelscriptStaticJITFunctionResult Result;
		Result.Disposition =
			EAngelscriptStaticJITFunctionDisposition::Emitted;
		Result.ActualBackendId = BackendId;
		Result.FunctionKey = Function.FunctionKey;
		Result.EmittedFunction = MoveTemp(Function);
		return Result;
	}

	static FAngelscriptStaticJITFunctionResult Unsupported(
		const FAngelscriptStaticJITBackendId BackendId,
		const FAngelscriptStableFunctionKey& FunctionKey,
		const FStringView Detail = TEXTVIEW("not supported by fake backend"))
	{
		FAngelscriptStaticJITFunctionResult Result;
		Result.Disposition =
			EAngelscriptStaticJITFunctionDisposition::Unsupported;
		Result.ActualBackendId = BackendId;
		Result.FunctionKey = FunctionKey;
		Result.Detail = Detail;
		return Result;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITBackendTests,
	"Angelscript.TestModule.StaticJIT.Backend.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ReservedIdsHaveExactStableSpelling)
	{
		const FAngelscriptStaticJITBackendId Bytecode =
			FAngelscriptStaticJITBackendId::Bytecode();
		const FAngelscriptStaticJITBackendId TypedAST =
			FAngelscriptStaticJITBackendId::TypedAST();

		ASSERT_THAT(IsTrue(Bytecode.IsValid()));
		ASSERT_THAT(IsTrue(TypedAST.IsValid()));
		ASSERT_THAT(AreEqual(FString(TEXT("bytecode")), Bytecode.ToString()));
		ASSERT_THAT(AreEqual(FString(TEXT("typed-ast")), TypedAST.ToString()));
		ASSERT_THAT(IsTrue(Bytecode != TypedAST));

		static_assert(!std::is_same_v<
			FAngelscriptStaticJITBackendId,
			FAngelscriptRuntimeJITBackendId>);
		static_assert(std::is_const_v<std::remove_reference_t<decltype(
			std::declval<FAngelscriptStaticJITBackendRequest>().
				CompiledSourceGraph)>>);
	}

	TEST_METHOD(EmptyUnknownCaseVariantsAndDualAreRejected)
	{
		FAngelscriptStaticJITBackendId Parsed =
			FAngelscriptStaticJITBackendId::Bytecode();
		ASSERT_THAT(IsFalse(FAngelscriptStaticJITBackendId::TryParse(
			TEXT(""), Parsed)));
		ASSERT_THAT(IsFalse(Parsed.IsValid()));
		ASSERT_THAT(IsFalse(FAngelscriptStaticJITBackendId::TryParse(
			TEXT("Bytecode"), Parsed)));
		ASSERT_THAT(IsFalse(FAngelscriptStaticJITBackendId::TryParse(
			TEXT("typed_ast"), Parsed)));
		ASSERT_THAT(IsFalse(FAngelscriptStaticJITBackendId::TryParse(
			TEXT("dual"), Parsed)));
		ASSERT_THAT(IsFalse(FAngelscriptStaticJITBackendId::TryParse(
			TEXT("unknown"), Parsed)));
	}

	TEST_METHOD(BackendSelectionFreezesOneRequiredCaptureProfile)
	{
		FAngelscriptStaticJITGenerationProfile BytecodeProfile;
		FString Error;
		ASSERT_THAT(IsTrue(
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::Bytecode(),
				EAngelscriptJITTargetProfile::EditorDevelopment,
				BytecodeProfile, &Error), Error));
		ASSERT_THAT(IsTrue(BytecodeProfile.BackendId
			== FAngelscriptStaticJITBackendId::Bytecode()));
		ASSERT_THAT(IsTrue(BytecodeProfile.CaptureProfile
			== EAngelscriptStaticJITCaptureProfile::Bytecode));
		ASSERT_THAT(IsFalse(
			BytecodeProfile.RequiresCanonicalASTSnapshot()));
		FAngelscriptEngineConfig BytecodeConfig;
		BytecodeProfile.ApplyToEngineConfig(BytecodeConfig);
		ASSERT_THAT(IsTrue(
			BytecodeConfig.bCollectStaticJITCompatibilityBinds,
			TEXT("A generation Engine must collect exact Engine-local native-call metadata without enabling cache persistence")));

		FAngelscriptStaticJITGenerationProfile TypedASTProfile;
		ASSERT_THAT(IsTrue(
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::TypedAST(),
				EAngelscriptJITTargetProfile::EditorDevelopment,
				TypedASTProfile, &Error), Error));
		ASSERT_THAT(IsTrue(TypedASTProfile.BackendId
			== FAngelscriptStaticJITBackendId::TypedAST()));
		ASSERT_THAT(IsTrue(TypedASTProfile.CaptureProfile
			== EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST));
		ASSERT_THAT(IsTrue(
			TypedASTProfile.RequiresCanonicalASTSnapshot(),
			TEXT("TypedASTJIT must freeze one retained canonical AST profile")));
		const EAngelscriptTypedASTJITDirectCapability
			ExpectedTypedASTCapabilities =
				EAngelscriptTypedASTJITDirectCapability::FramePosition
				| EAngelscriptTypedASTJITDirectCapability::RecursionBudget;
		ASSERT_THAT(AreEqual(
			ExpectedTypedASTCapabilities,
			TypedASTProfile.TypedASTJITCapabilityProfile.
				AvailableCapabilities,
			TEXT("Every emitted TypedASTJIT root now owns position metadata and the shared native recursion guard")));
		FAngelscriptEngineConfig TypedASTConfig;
		TypedASTProfile.ApplyToEngineConfig(TypedASTConfig);
		ASSERT_THAT(IsTrue(
			TypedASTConfig.bRequireCanonicalASTForStaticJITGeneration,
			TEXT("The generation Engine must carry an explicit canonical AST requirement")));
		ASSERT_THAT(IsTrue(
			TypedASTConfig.bCollectStaticJITCompatibilityBinds,
			TEXT("Typed AST capture must freeze the matching registered native-call descriptors")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_RETAIN_SNAPSHOT),
			TypedASTConfig.InitialASTRetentionPolicy,
			TEXT("VerifiedCanonicalAST generation must retain a public sealed AST snapshot for its immutable generation lease")));

		FAngelscriptStaticJITGenerationProfile GameDevelopmentProfile;
		ASSERT_THAT(IsTrue(
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::TypedAST(),
				EAngelscriptJITTargetProfile::GameDevelopment,
				GameDevelopmentProfile, &Error), Error));
		ASSERT_THAT(AreEqual(
			ExpectedTypedASTCapabilities,
			GameDevelopmentProfile.TypedASTJITCapabilityProfile.
				AvailableCapabilities,
			TEXT("GameDevelopment retains the non-Shipping position frame and recursion budget")));

		FAngelscriptStaticJITGenerationProfile GameShippingProfile;
		ASSERT_THAT(IsTrue(
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::TypedAST(),
				EAngelscriptJITTargetProfile::GameShipping,
				GameShippingProfile, &Error), Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITDirectCapability::RecursionBudget,
			GameShippingProfile.TypedASTJITCapabilityProfile.
				AvailableCapabilities,
			TEXT("Shipping preserves the native recursion guard without advertising the compiled-out debug position frame")));

		FAngelscriptStaticJITGenerationProfile InvalidTargetProfile;
		ASSERT_THAT(IsFalse(
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::TypedAST(),
				EAngelscriptJITTargetProfile::All,
				InvalidTargetProfile, &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("concrete target profile"))));

		FAngelscriptStaticJITGenerationProfile InvalidProfile;
		ASSERT_THAT(IsFalse(
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId(),
				EAngelscriptJITTargetProfile::EditorDevelopment,
				InvalidProfile, &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("bytecode"))));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("typed-ast"))));
	}

	TEST_METHOD(FactoryRegistrationIsDeterministicAndRejectsDuplicates)
	{
		using namespace AngelscriptStaticJITBackendTests_Private;
		FAngelscriptStaticJITBackendRegistry Registry;
		FNullFactory TypedAST(FAngelscriptStaticJITBackendId::TypedAST());
		FNullFactory Bytecode(FAngelscriptStaticJITBackendId::Bytecode());
		FNullFactory DuplicateBytecode(
			FAngelscriptStaticJITBackendId::Bytecode());
		FNullFactory Invalid{FAngelscriptStaticJITBackendId()};

		ASSERT_THAT(IsTrue(Registry.RegisterFactory(TypedAST).IsAccepted()));
		ASSERT_THAT(IsTrue(Registry.RegisterFactory(Bytecode).IsAccepted()));
		const FAngelscriptStaticJITBackendRegistrationResult DuplicateResult =
			Registry.RegisterFactory(DuplicateBytecode);
		ASSERT_THAT(IsTrue(DuplicateResult.Code
			== EAngelscriptStaticJITBackendRegistrationCode::DuplicateBackendId));
		ASSERT_THAT(IsTrue(Registry.RegisterFactory(Invalid).Code
			== EAngelscriptStaticJITBackendRegistrationCode::InvalidBackendId));

		ASSERT_THAT(IsTrue(Registry.FindFactory(
			FAngelscriptStaticJITBackendId::Bytecode()) == &Bytecode));
		ASSERT_THAT(IsTrue(Registry.FindFactory(
			FAngelscriptStaticJITBackendId::TypedAST()) == &TypedAST));
		const TArray<FAngelscriptStaticJITBackendId> RegisteredIds =
			Registry.GetRegisteredBackendIds();
		ASSERT_THAT(AreEqual(2, RegisteredIds.Num()));
		ASSERT_THAT(IsTrue(RegisteredIds[0]
			== FAngelscriptStaticJITBackendId::Bytecode()));
		ASSERT_THAT(IsTrue(RegisteredIds[1]
			== FAngelscriptStaticJITBackendId::TypedAST()));
	}

	TEST_METHOD(OneBackendInstanceSeesTheWholeGraphAndEmitSetFiltersPackaging)
	{
		using namespace AngelscriptStaticJITBackendTests_Private;
		const FAngelscriptJITGenerationModule FirstModule =
			MakeModule(0x11, TEXT("FirstModule"));
		const FAngelscriptJITGenerationModule SecondModule =
			MakeModule(0x22, TEXT("SecondModule"));
		const FAngelscriptJITGenerationFunction FirstFunction =
			MakeFunction(0x11, 0x31, TEXT("FirstFunction"));
		const FAngelscriptJITGenerationFunction SecondFunction =
			MakeFunction(0x22, 0x32, TEXT("SecondFunction"));
		TArray<FAngelscriptStaticJITCompiledFunctionView> FunctionViews;
		FunctionViews.Add({nullptr, FirstFunction.ModuleKey,
			FirstFunction.FunctionKey, FirstFunction.ExecutionHash,
			FirstFunction.DebugHash, FirstFunction.EntryAbiHash,
			FirstFunction.CanonicalDeclaration});
		FunctionViews.Add({nullptr, SecondFunction.ModuleKey,
			SecondFunction.FunctionKey, SecondFunction.ExecutionHash,
			SecondFunction.DebugHash, SecondFunction.EntryAbiHash,
			SecondFunction.CanonicalDeclaration});
		const FAngelscriptStaticJITCompiledSourceGraph Graph =
			MakeGraph(FunctionViews);
		const FAngelscriptStableModuleKey EmitModules[] = {
			SecondModule.ModuleKey};
		const FAngelscriptStaticJITBackendRequest Request{
			FAngelscriptStaticJITBackendId::Bytecode(),
			Graph,
			EmitModules};

		FRecordingFactory Bytecode(
			FAngelscriptStaticJITBackendId::Bytecode());
		Bytecode.Output.Modules = {FirstModule, SecondModule};
		Bytecode.Output.Functions.Add(Emitted(
			Bytecode.BackendId, FirstFunction));
		Bytecode.Output.Functions.Add(Emitted(
			Bytecode.BackendId, SecondFunction));
		FAngelscriptStaticJITBackendRegistry Registry;
		ASSERT_THAT(IsTrue(Registry.RegisterFactory(Bytecode).IsAccepted()));

		FAngelscriptStaticJITGenerator Generator(Registry);
		FAngelscriptStaticJITGeneratorOutput Output;
		FString Error;
		ASSERT_THAT(IsTrue(Generator.Generate(Request, Output, &Error), Error));
		ASSERT_THAT(AreEqual(1, Bytecode.CreateCount));
		ASSERT_THAT(AreEqual(1, Bytecode.GenerateCount));
		ASSERT_THAT(AreEqual(2, Bytecode.SeenFunctionCount));
		ASSERT_THAT(AreEqual(1, Bytecode.SeenEmitModuleCount));
		ASSERT_THAT(AreEqual(1, Output.ProviderOutput.Modules.Num()));
		ASSERT_THAT(IsTrue(Output.ProviderOutput.Modules[0].ModuleKey
			== SecondModule.ModuleKey));
		ASSERT_THAT(AreEqual(1, Output.ProviderOutput.Functions.Num()));
		ASSERT_THAT(IsTrue(Output.ProviderOutput.Functions[0].FunctionKey
			== SecondFunction.FunctionKey));
	}

	TEST_METHOD(TypedASTFallsBackPerFunctionAndSharesOneModuleTranslationUnit)
	{
		using namespace AngelscriptStaticJITBackendTests_Private;
		const FAngelscriptJITGenerationModule Module =
			MakeModule(0x41, TEXT("MixedModule"));
		const FAngelscriptJITGenerationFunction TypedFunction =
			MakeFunction(0x41, 0x51, TEXT("EligibleTypedRoot"));
		const FAngelscriptJITGenerationFunction BytecodeFunction =
			MakeFunction(0x41, 0x52,
				TEXT("RequiredCalleeFallbackRoot"));
		const FAngelscriptJITGenerationFunction VmFunction =
			MakeFunction(0x41, 0x53, TEXT("VmFallbackRoot"));
		TArray<FAngelscriptStaticJITCompiledFunctionView> FunctionViews;
		for (const FAngelscriptJITGenerationFunction* Function :
			{&TypedFunction, &BytecodeFunction, &VmFunction})
		{
			FunctionViews.Add({nullptr, Function->ModuleKey,
				Function->FunctionKey, Function->ExecutionHash,
				Function->DebugHash, Function->EntryAbiHash,
				Function->CanonicalDeclaration});
		}
		FAngelscriptStaticJITCompiledSourceGraph Graph =
			MakeGraph(FunctionViews);
		Graph.CaptureProfile =
			EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST;
		const FAngelscriptStableModuleKey EmitModules[] = {Module.ModuleKey};
		const FAngelscriptStaticJITBackendRequest Request{
			FAngelscriptStaticJITBackendId::TypedAST(), Graph, EmitModules};

		FRecordingFactory TypedAST(
			FAngelscriptStaticJITBackendId::TypedAST());
		TypedAST.Output.Modules = {Module};
		TypedAST.Output.Functions.Add(Emitted(
			TypedAST.BackendId, TypedFunction));
		TypedAST.Output.Functions.Add(Unsupported(
			TypedAST.BackendId, BytecodeFunction.FunctionKey,
			TEXTVIEW("UnsupportedCall: RequiredCalleeUnsupported at 42:13")));
		TypedAST.Output.Functions.Add(Unsupported(
			TypedAST.BackendId, VmFunction.FunctionKey,
			TEXTVIEW("UnsupportedSignature: object parameter")));
		TypedAST.Output.ModuleSourcePreamble =
			TEXT("#include \"StaticJIT/StaticJITConfig.h\"\n")
			TEXT("#ifndef AS_SKIP_JITTED_CODE\n")
			TEXT("#include \"TypedBackend.h\"\n");
		TypedAST.Output.ModuleSourceFooter = TEXT("#endif\n");

		FRecordingFactory Bytecode(
			FAngelscriptStaticJITBackendId::Bytecode());
		Bytecode.Output.Modules = {Module};
		Bytecode.Output.Functions.Add(Emitted(
			Bytecode.BackendId, BytecodeFunction));
		Bytecode.Output.Functions.Add(Unsupported(
			Bytecode.BackendId, VmFunction.FunctionKey));
		Bytecode.Output.ModuleSourcePreamble =
			TEXT("#include \"StaticJIT/StaticJITConfig.h\"\n")
			TEXT("#ifndef AS_SKIP_JITTED_CODE\n")
			TEXT("#include \"BytecodeBackend.h\"\n");
		Bytecode.Output.ModuleSourceFooter = TEXT("#endif\n");

		FAngelscriptStaticJITBackendRegistry Registry;
		ASSERT_THAT(IsTrue(Registry.RegisterFactory(Bytecode).IsAccepted()));
		ASSERT_THAT(IsTrue(Registry.RegisterFactory(TypedAST).IsAccepted()));
		FAngelscriptStaticJITGenerator Generator(Registry);
		FAngelscriptStaticJITGeneratorOutput Output;
		FString Error;
		ASSERT_THAT(IsTrue(Generator.Generate(Request, Output, &Error), Error));

		ASSERT_THAT(AreEqual(1, TypedAST.CreateCount));
		ASSERT_THAT(AreEqual(1, Bytecode.CreateCount));
		ASSERT_THAT(IsTrue(TypedAST.SeenBackendId
			== FAngelscriptStaticJITBackendId::TypedAST()));
		ASSERT_THAT(IsTrue(Bytecode.SeenBackendId
			== FAngelscriptStaticJITBackendId::Bytecode()));
		ASSERT_THAT(AreEqual(1, Output.ProviderOutput.Modules.Num()));
		ASSERT_THAT(AreEqual(2, Output.ProviderOutput.Functions.Num()));
		ASSERT_THAT(IsTrue(Output.ProviderOutput.ProviderId
			== Graph.Settings.ProviderId));
		ASSERT_THAT(IsTrue(Output.Diagnostics.RequestedBackendId
			== FAngelscriptStaticJITBackendId::TypedAST()));
		ASSERT_THAT(IsTrue(Output.Diagnostics.CaptureProfile
			== EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST));
		ASSERT_THAT(AreEqual(2, Output.Diagnostics.FallbackChain.Num()));
		ASSERT_THAT(IsTrue(Output.Diagnostics.FallbackChain[0]
			== FAngelscriptStaticJITBackendId::TypedAST()));
		ASSERT_THAT(IsTrue(Output.Diagnostics.FallbackChain[1]
			== FAngelscriptStaticJITBackendId::Bytecode()));
		ASSERT_THAT(AreEqual(3, Output.Functions.Num()));
		ASSERT_THAT(IsTrue(Output.Functions[0].ActualBackendId
			== FAngelscriptStaticJITBackendId::TypedAST()));
		ASSERT_THAT(IsTrue(Output.Functions[0].Disposition
			== EAngelscriptStaticJITFunctionDisposition::Emitted));
		ASSERT_THAT(AreEqual(1,
			Output.Functions[0].BackendAttempts.Num()));
		ASSERT_THAT(IsTrue(
			Output.Functions[0].BackendAttempts[0].BackendId
				== FAngelscriptStaticJITBackendId::TypedAST()));
		ASSERT_THAT(IsTrue(Output.Functions[1].ActualBackendId
			== FAngelscriptStaticJITBackendId::Bytecode()));
		ASSERT_THAT(AreEqual(2, Output.Functions[1].BackendAttempts.Num()));
		ASSERT_THAT(IsTrue(
			Output.Functions[1].BackendAttempts[0].BackendId
				== FAngelscriptStaticJITBackendId::TypedAST()));
		ASSERT_THAT(IsTrue(
			Output.Functions[1].BackendAttempts[0].Disposition
				== EAngelscriptStaticJITFunctionDisposition::Unsupported));
		ASSERT_THAT(IsTrue(
			Output.Functions[1].BackendAttempts[0].Detail.Contains(
				TEXT("UnsupportedCall: RequiredCalleeUnsupported"))));
		ASSERT_THAT(IsTrue(
			Output.Functions[1].BackendAttempts[0].Detail.Contains(
				TEXT("42:13"))));
		ASSERT_THAT(IsTrue(
			Output.Functions[1].BackendAttempts[1].BackendId
				== FAngelscriptStaticJITBackendId::Bytecode()));
		ASSERT_THAT(IsTrue(
			Output.Functions[1].BackendAttempts[1].Disposition
				== EAngelscriptStaticJITFunctionDisposition::Emitted));
		ASSERT_THAT(IsTrue(Output.Functions[2].Disposition
			== EAngelscriptStaticJITFunctionDisposition::Unsupported));
		ASSERT_THAT(IsTrue(Output.Functions[2].ActualBackendId
			== FAngelscriptStaticJITBackendId::Bytecode()));
		ASSERT_THAT(IsFalse(Output.Functions[2].EmittedFunction.IsSet()));
		ASSERT_THAT(AreEqual(2, Output.Functions[2].BackendAttempts.Num()));
		ASSERT_THAT(IsTrue(
			Output.Functions[2].BackendAttempts[0].Detail.Contains(
				TEXT("UnsupportedSignature"))));
		int32 ModuleSourceCount = 0;
		for (const FAngelscriptJITGeneratedFile& File :
			Output.ProviderOutput.Files)
		{
			if (File.Kind == EAngelscriptJITGeneratedFileKind::ModuleSource)
			{
				++ModuleSourceCount;
				auto CountToken = [&File](const FStringView Token)
				{
					int32 Count = 0;
					int32 SearchFrom = 0;
					while (SearchFrom < File.Content.Len())
					{
						const int32 Found = File.Content.Find(
							FString(Token), ESearchCase::CaseSensitive,
							ESearchDir::FromStart, SearchFrom);
						if (Found == INDEX_NONE)
						{
							break;
						}
						++Count;
						SearchFrom = Found + Token.Len();
					}
					return Count;
				};
				ASSERT_THAT(AreEqual(1, CountToken(
					TEXTVIEW("#ifndef AS_SKIP_JITTED_CODE")),
					TEXT("Mixed backends must share one module guard")));
				ASSERT_THAT(AreEqual(2, CountToken(TEXTVIEW("#endif")),
					TEXT("The shared backend guard and target guard must balance")));
				ASSERT_THAT(IsTrue(File.Content.Contains(
					TEXT("#include \"TypedBackend.h\""))));
				ASSERT_THAT(IsTrue(File.Content.Contains(
					TEXT("#include \"BytecodeBackend.h\""))));
			}
		}
		ASSERT_THAT(AreEqual(1, ModuleSourceCount));
	}

	TEST_METHOD(FatalPrimaryTaskDoesNotConstructFallback)
	{
		using namespace AngelscriptStaticJITBackendTests_Private;
		TArray<FAngelscriptStaticJITCompiledFunctionView> Functions;
		FAngelscriptStaticJITCompiledSourceGraph Graph =
			MakeGraph(Functions);
		Graph.CaptureProfile =
			EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST;
		const FAngelscriptStaticJITBackendRequest Request{
			FAngelscriptStaticJITBackendId::TypedAST(), Graph, {}};

		FRecordingFactory TypedAST(
			FAngelscriptStaticJITBackendId::TypedAST());
		TypedAST.Output.Disposition =
			EAngelscriptStaticJITTaskDisposition::FatalError;
		TypedAST.Output.Detail = TEXT("fatal fake analysis error");
		FRecordingFactory Bytecode(
			FAngelscriptStaticJITBackendId::Bytecode());
		FAngelscriptStaticJITBackendRegistry Registry;
		ASSERT_THAT(IsTrue(Registry.RegisterFactory(TypedAST).IsAccepted()));
		ASSERT_THAT(IsTrue(Registry.RegisterFactory(Bytecode).IsAccepted()));

		FAngelscriptStaticJITGenerator Generator(Registry);
		FAngelscriptStaticJITGeneratorOutput Output;
		FString Error;
		ASSERT_THAT(IsFalse(Generator.Generate(Request, Output, &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("fatal fake analysis error"))));
		ASSERT_THAT(AreEqual(1, TypedAST.CreateCount));
		ASSERT_THAT(AreEqual(0, Bytecode.CreateCount));
	}

	TEST_METHOD(TypedASTCaptureMismatchFailsBeforeBackendConstruction)
	{
		using namespace AngelscriptStaticJITBackendTests_Private;
		TArray<FAngelscriptStaticJITCompiledFunctionView> Functions;
		const FAngelscriptStaticJITCompiledSourceGraph Graph =
			MakeGraph(Functions);
		const FAngelscriptStaticJITBackendRequest Request{
			FAngelscriptStaticJITBackendId::TypedAST(), Graph, {}};

		FRecordingFactory TypedAST(
			FAngelscriptStaticJITBackendId::TypedAST());
		FRecordingFactory Bytecode(
			FAngelscriptStaticJITBackendId::Bytecode());
		FAngelscriptStaticJITBackendRegistry Registry;
		ASSERT_THAT(IsTrue(Registry.RegisterFactory(TypedAST).IsAccepted()));
		ASSERT_THAT(IsTrue(Registry.RegisterFactory(Bytecode).IsAccepted()));

		FAngelscriptStaticJITGenerator Generator(Registry);
		FAngelscriptStaticJITGeneratorOutput Output;
		FString Error;
		ASSERT_THAT(IsFalse(Generator.Generate(Request, Output, &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("CaptureProfileMismatch"))));
		ASSERT_THAT(IsTrue(Output.Diagnostics.TaskError.Contains(
			TEXT("CaptureProfileMismatch"))));
		ASSERT_THAT(AreEqual(0, TypedAST.CreateCount));
		ASSERT_THAT(AreEqual(0, Bytecode.CreateCount));
	}

	TEST_METHOD(PackagingFailureIsReportedThroughGeneratorDiagnostics)
	{
		using namespace AngelscriptStaticJITBackendTests_Private;
		const FAngelscriptJITGenerationModule Module =
			MakeModule(0x61, TEXT("InvalidPackageModule"));
		const FAngelscriptJITGenerationFunction Function =
			MakeFunction(0x61, 0x71, TEXT("InvalidPackageFunction"));
		TArray<FAngelscriptStaticJITCompiledFunctionView> FunctionViews;
		FunctionViews.Add({nullptr, Function.ModuleKey,
			Function.FunctionKey, Function.ExecutionHash,
			Function.DebugHash, Function.EntryAbiHash,
			Function.CanonicalDeclaration});
		FAngelscriptStaticJITCompiledSourceGraph Graph =
			MakeGraph(FunctionViews);
		Graph.Settings.ProviderId = {};
		const FAngelscriptStableModuleKey EmitModules[] = {Module.ModuleKey};
		const FAngelscriptStaticJITBackendRequest Request{
			FAngelscriptStaticJITBackendId::Bytecode(), Graph, EmitModules};

		FRecordingFactory Bytecode(
			FAngelscriptStaticJITBackendId::Bytecode());
		Bytecode.Output.Modules = {Module};
		Bytecode.Output.Functions.Add(Emitted(
			Bytecode.BackendId, Function));
		FAngelscriptStaticJITBackendRegistry Registry;
		ASSERT_THAT(IsTrue(Registry.RegisterFactory(Bytecode).IsAccepted()));

		FAngelscriptStaticJITGenerator Generator(Registry);
		FAngelscriptStaticJITGeneratorOutput Output;
		FString Error;
		ASSERT_THAT(IsFalse(Generator.Generate(Request, Output, &Error)));
		ASSERT_THAT(IsFalse(Error.IsEmpty()));
		ASSERT_THAT(AreEqual(Error, Output.Diagnostics.TaskError));
		ASSERT_THAT(IsFalse(Output.bHasProviderOutput));
		ASSERT_THAT(IsTrue(Output.ProviderOutput.Files.IsEmpty()));
	}
};

#endif
