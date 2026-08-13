#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"
#include "StaticJIT/AngelscriptJITBindingContext.h"
#include "StaticJIT/AngelscriptJITImmutableArtifactSet.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITImmutableArtifactSetTests_Private
{
	static FAngelscriptHash256 MakeHash(const TCHAR* Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("jit-immutable-set-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static void VMEntry(FScriptExecution&, asDWORD*, asQWORD* OutValue)
	{
		if (OutValue != nullptr)
		{
			*OutValue = 701;
		}
	}

	static asDWORD RawEntry(FScriptExecution&, void*)
	{
		return 701;
	}

	static void ParmsEntry(FScriptExecution&, void*, void* Parms)
	{
		*static_cast<int32*>(Parms) = 701;
	}

	class FResolver final : public IAngelscriptJITReferenceResolver
	{
	public:
		FAngelscriptJITReferenceResolution Resolve(
			const FAngelscriptArtifactReference&) const override
		{
			return {
				EAngelscriptJITReferenceResolutionCode::Exact,
				this};
		}
	};

	struct FFixture
	{
		TSharedPtr<FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> Catalog;
		TSharedPtr<FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Snapshot;
		FResolver Resolver;
		FAngelscriptJITImmutableArtifactSetValidationInput Input;

		FFixture()
		{
			Catalog = MakeShared<FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe>();
			Catalog->ProviderId.Hash = MakeHash(TEXT("provider"));
			Catalog->ProviderGeneration.Hash = MakeHash(TEXT("generation"));
			Catalog->ArtifactSetDigest = MakeHash(TEXT("artifact-set"));
			Catalog->ArtifactProfile.Hash = MakeHash(TEXT("shipping-profile"));
			Catalog->NativeEnvironmentFingerprint =
				MakeHash(TEXT("shipping-environment"));
			Catalog->ProviderName = TEXT("Immutable Provider");
			Catalog->OwnerModuleName = TEXT("ImmutableProviderModule");

			for (int32 EntryIndex = 0; EntryIndex < 2; ++EntryIndex)
			{
				FAngelscriptJITProviderCatalogEntry& Entry =
					Catalog->Entries.AddDefaulted_GetRef();
				Entry.Flags = EntryIndex == 1
					? EAngelscriptJITArtifactEntryFlags::ImmutableDirectCallSet
					: EAngelscriptJITArtifactEntryFlags::None;
				Entry.ModuleKey.Hash = MakeHash(TEXT("module"));
				Entry.FunctionKey.Hash = MakeHash(
					EntryIndex == 0 ? TEXT("reloadable-function")
						: TEXT("immutable-function"));
				Entry.ExecutionHash = MakeHash(
					EntryIndex == 0 ? TEXT("execution-0")
						: TEXT("execution-1"));
				Entry.DebugHash = MakeHash(
					EntryIndex == 0 ? TEXT("debug-0") : TEXT("debug-1"));
				Entry.ArtifactProfile = Catalog->ArtifactProfile;
				Entry.EntryAbiHash = MakeHash(
					EntryIndex == 0 ? TEXT("abi-0") : TEXT("abi-1"));
				Entry.NativeEnvironmentFingerprint =
					Catalog->NativeEnvironmentFingerprint;
				Entry.EntryPoints.VMEntry = &VMEntry;
				Entry.EntryPoints.RawEntry =
					reinterpret_cast<asJITFunction_Raw>(&RawEntry);
				Entry.EntryPoints.ParmsEntry = &ParmsEntry;

				FAngelscriptJITReferenceSlot& Reference =
					Entry.ReferenceSlots.AddDefaulted_GetRef();
				Reference.Reference.Kind =
					EAngelscriptArtifactReferenceKind::ScriptFunction;
				Reference.Reference.StableKey = MakeHash(
					EntryIndex == 0 ? TEXT("reference-0")
						: TEXT("reference-1"));
				Reference.Reference.ExpectedAbi = MakeHash(
					EntryIndex == 0 ? TEXT("reference-abi-0")
						: TEXT("reference-abi-1"));
			}

			Snapshot = MakeShared<FAngelscriptJITProviderRegistrySnapshot,
				ESPMode::ThreadSafe>();
			Snapshot->PublicationOrdinal = 1;
			Snapshot->Providers.Add(Catalog);

			Input.Provider = Catalog;
			Input.RuntimeProfile =
				EAngelscriptJITArtifactSetRuntimeProfile::ImmutableCooked;
			Input.CurrentArtifactSetDigest = Catalog->ArtifactSetDigest;
			Input.CurrentArtifactProfile = Catalog->ArtifactProfile;
			Input.CurrentNativeEnvironmentFingerprint =
				Catalog->NativeEnvironmentFingerprint;
			RefreshMatches();
		}

		FAngelscriptJITFunctionMatchResult MatchEntry(
			const int32 EntryIndex) const
		{
			const FAngelscriptJITProviderCatalogEntry& Entry =
				Catalog->Entries[EntryIndex];
			FAngelscriptJITFunctionMatchInput MatchInput;
			MatchInput.ModuleKey = Entry.ModuleKey;
			MatchInput.Identity.FunctionKey = Entry.FunctionKey;
			MatchInput.Identity.Content.Execution = Entry.ExecutionHash;
			MatchInput.Identity.Content.Debug = Entry.DebugHash;
			MatchInput.Identity.Profile = Entry.ArtifactProfile;
			MatchInput.EntryAbiHash = Entry.EntryAbiHash;
			MatchInput.NativeEnvironmentFingerprint =
				Entry.NativeEnvironmentFingerprint;
			MatchInput.CurrentArtifactSetDigest =
				Catalog->ArtifactSetDigest;
			MatchInput.RequiredEntryPoints =
				EAngelscriptJITRequiredEntryPoints::VM
				| EAngelscriptJITRequiredEntryPoints::Raw
				| EAngelscriptJITRequiredEntryPoints::Parms;
			return FAngelscriptJITProviderMatcher::Match(
				*Snapshot,
				MatchInput,
				Resolver);
		}

		void RefreshMatches()
		{
			Input.EntryMatches.Reset();
			for (int32 EntryIndex = 0;
				EntryIndex < Catalog->Entries.Num();
				++EntryIndex)
			{
				FAngelscriptJITImmutableArtifactEntryMatch& EntryMatch =
					Input.EntryMatches.AddDefaulted_GetRef();
				EntryMatch.ProviderEntryIndex = EntryIndex;
				EntryMatch.Match = MatchEntry(EntryIndex);
			}
		}

		FAngelscriptJITImmutableArtifactSetValidationResult Validate() const
		{
			return FAngelscriptJITImmutableArtifactSetValidator::Validate(
				Input);
		}

		const FAngelscriptJITArtifactSelection& Selection(
			const int32 EntryIndex) const
		{
			return Input.EntryMatches[EntryIndex].Match.Selection.GetValue();
		}
	};

	class FAllocationCompiler final : public asIJITCompiler
	{
	public:
		FAllocationCompiler(
			const FAngelscriptJITArtifactSelection& InReloadableSelection,
			const FAngelscriptJITArtifactSelection& InImmutableSelection,
			const FAngelscriptJITImmutableArtifactSetValidationResult&
				InValidation)
			: ReloadableSelection(InReloadableSelection)
			, ImmutableSelection(InImmutableSelection)
			, Validation(InValidation)
		{
		}

		void OnFunctionReady(asIScriptFunction* Function) override
		{
			if (Function == nullptr)
			{
				return;
			}

			const FAngelscriptJITArtifactSelection* Selection = nullptr;
			const FAngelscriptJITImmutableArtifactSetValidationResult*
				ImmutableValidation = nullptr;
			if (FCStringAnsi::Strcmp(
					Function->GetName(),
					"ReloadableValue") == 0)
			{
				Selection = &ReloadableSelection;
			}
			else if (FCStringAnsi::Strcmp(
					Function->GetName(),
					"ImmutableValue") == 0)
			{
				Selection = &ImmutableSelection;
				ImmutableValidation = &Validation;
			}
			if (Selection == nullptr)
			{
				return;
			}

			TSharedPtr<FAngelscriptJITBindingContext, ESPMode::ThreadSafe>
				Context = MakeShared<FAngelscriptJITBindingContext,
					ESPMode::ThreadSafe>(
						*Selection,
						ImmutableValidation);
			const asSJITFunctionBinding Binding = Context->BuildBinding();
			Contexts.Add(Binding.UserData, Context);
			if (Function->SetJITBinding(Binding) != asSUCCESS)
			{
				Contexts.Remove(Binding.UserData);
			}
		}

		void ReleaseFunctionBinding(
			asIScriptFunction*,
			const asSJITFunctionBinding& Binding) override
		{
			Contexts.Remove(Binding.UserData);
		}

	private:
		FAngelscriptJITArtifactSelection ReloadableSelection;
		FAngelscriptJITArtifactSelection ImmutableSelection;
		FAngelscriptJITImmutableArtifactSetValidationResult Validation;
		TMap<const void*, TSharedPtr<FAngelscriptJITBindingContext,
			ESPMode::ThreadSafe>> Contexts;
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITImmutableArtifactSetTests,
	"Angelscript.TestModule.StaticJIT.ImmutableArtifactSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CompleteImmutableCookedSetIsAcceptedAsOneUnit)
	{
		using namespace AngelscriptJITImmutableArtifactSetTests_Private;
		FFixture Fixture;
		const FAngelscriptJITImmutableArtifactSetValidationResult Result =
			Fixture.Validate();
		ASSERT_THAT(IsTrue(Result.IsEligible()));
		ASSERT_THAT(IsTrue(Result.Code()
			== EAngelscriptJITImmutableArtifactSetValidationCode::Eligible));
		ASSERT_THAT(IsFalse(Result.AllowsDirectSelection(
			Fixture.Selection(0))));
		ASSERT_THAT(IsTrue(Result.AllowsDirectSelection(
			Fixture.Selection(1))));
	}

	TEST_METHOD(ProfileSetEnvironmentAndOneStaleEntryFailClosed)
	{
		using namespace AngelscriptJITImmutableArtifactSetTests_Private;
		FFixture Fixture;

		Fixture.Input.RuntimeProfile =
			EAngelscriptJITArtifactSetRuntimeProfile::Reloadable;
		ASSERT_THAT(IsTrue(Fixture.Validate().Code()
			== EAngelscriptJITImmutableArtifactSetValidationCode::ReloadableProfile));
		Fixture.Input.RuntimeProfile =
			EAngelscriptJITArtifactSetRuntimeProfile::ImmutableCooked;

		Fixture.Input.CurrentArtifactSetDigest = MakeHash(TEXT("stale-set"));
		ASSERT_THAT(IsTrue(Fixture.Validate().Code()
			== EAngelscriptJITImmutableArtifactSetValidationCode::ArtifactSetMismatch));
		Fixture.Input.CurrentArtifactSetDigest =
			Fixture.Catalog->ArtifactSetDigest;

		Fixture.Input.CurrentArtifactProfile.Hash =
			MakeHash(TEXT("wrong-profile"));
		ASSERT_THAT(IsTrue(Fixture.Validate().Code()
			== EAngelscriptJITImmutableArtifactSetValidationCode::ProfileMismatch));
		Fixture.Input.CurrentArtifactProfile = Fixture.Catalog->ArtifactProfile;

		Fixture.Input.CurrentNativeEnvironmentFingerprint =
			MakeHash(TEXT("wrong-environment"));
		ASSERT_THAT(IsTrue(Fixture.Validate().Code()
			== EAngelscriptJITImmutableArtifactSetValidationCode::EnvironmentMismatch));
		Fixture.Input.CurrentNativeEnvironmentFingerprint =
			Fixture.Catalog->NativeEnvironmentFingerprint;

		Fixture.Input.EntryMatches[0].Match.Result =
			EAngelscriptArtifactMatchResult::ContentMismatch;
		Fixture.Input.EntryMatches[0].Match.Selection.Reset();
		const FAngelscriptJITImmutableArtifactSetValidationResult Stale =
			Fixture.Validate();
		ASSERT_THAT(IsTrue(Stale.Code()
			== EAngelscriptJITImmutableArtifactSetValidationCode::EntryNotExact));
		ASSERT_THAT(AreEqual(0, Stale.FailedEntryIndex()));
		ASSERT_THAT(IsFalse(Stale.AllowsDirectSelection(Fixture.Selection(1))));
	}

	TEST_METHOD(PartialEntryPointsAndReferencesRejectTheWholeDirectSet)
	{
		using namespace AngelscriptJITImmutableArtifactSetTests_Private;
		FFixture Fixture;
		Fixture.Catalog->Entries[0].EntryPoints.ParmsEntry = nullptr;
		ASSERT_THAT(IsTrue(Fixture.Validate().Code()
			== EAngelscriptJITImmutableArtifactSetValidationCode::MissingEntryPoint));

		Fixture.Catalog->Entries[0].EntryPoints.ParmsEntry = &ParmsEntry;
		Fixture.RefreshMatches();
		TSharedPtr<FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe> MissingReferenceTable =
			MakeShared<FAngelscriptJITResolvedReferenceTable,
				ESPMode::ThreadSafe>();
		MissingReferenceTable->Slots.AddDefaulted();
		Fixture.Input.EntryMatches[0].Match.Selection.GetValue()
			.ReferenceTable = MissingReferenceTable;
		ASSERT_THAT(IsTrue(Fixture.Validate().Code()
			== EAngelscriptJITImmutableArtifactSetValidationCode::MissingRequiredReference));
	}

	TEST_METHOD(DirectProviderSelectionCannotPublishWithoutAcceptedSet)
	{
		using namespace AngelscriptJITImmutableArtifactSetTests_Private;
		using namespace AngelscriptNativeTestSupport;
		FFixture Fixture;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetEngineProperty(
				asEP_INCLUDE_JIT_INSTRUCTIONS,
				1)));
		FAngelscriptStaticJIT Compiler;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler)));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(nullptr);
		};

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 1;
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"ImmutableSetPublication",
			Source);
		ASSERT_THAT(IsTrue(Module.IsValid()));
		asIScriptFunction* const Function = GetNativeFunctionByDecl(
			Module.Get(),
			"int Entry()");
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		const FAngelscriptJITArtifactSelection& DirectSelection =
			Fixture.Selection(1);
		ASSERT_THAT(IsFalse(Compiler.PublishProviderSelection(
			Function,
			DirectSelection)));
		ASSERT_THAT(IsNull(Function->GetJITBinding().VMEntry));

		Fixture.Input.RuntimeProfile =
			EAngelscriptJITArtifactSetRuntimeProfile::Reloadable;
		const FAngelscriptJITImmutableArtifactSetValidationResult Rejected =
			Fixture.Validate();
		ASSERT_THAT(IsFalse(Compiler.PublishProviderSelection(
			Function,
			DirectSelection,
			&Rejected)));
		ASSERT_THAT(IsNull(Function->GetJITBinding().VMEntry));

		Fixture.Input.RuntimeProfile =
			EAngelscriptJITArtifactSetRuntimeProfile::ImmutableCooked;
		const FAngelscriptJITImmutableArtifactSetValidationResult Accepted =
			Fixture.Validate();
		ASSERT_THAT(IsTrue(Compiler.PublishProviderSelection(
			Function,
			DirectSelection,
			&Accepted)));
		const asSJITFunctionBinding Binding = Function->GetJITBinding();
		const FAngelscriptJITBindingContext* const Context =
			FAngelscriptJITBindingContext::FromBindingUserData(
				Binding.UserData);
		ASSERT_THAT(IsNotNull(Context));
		ASSERT_THAT(IsTrue(Context != nullptr
			&& Context->IsImmutableCookedDirectDispatch()));
	}

	TEST_METHOD(UASFunctionDirectWrapperRequiresAcceptedImmutableContext)
	{
		using namespace AngelscriptJITImmutableArtifactSetTests_Private;
		FFixture Fixture;
		const FAngelscriptJITImmutableArtifactSetValidationResult Accepted =
			Fixture.Validate();
		ASSERT_THAT(IsTrue(Accepted.IsEligible()));

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptEngine* const ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const asPWORD OriginalIncludeJIT = ScriptEngine->GetEngineProperty(
			asEP_INCLUDE_JIT_INSTRUCTIONS);
		asIJITCompiler* const OriginalCompiler = ScriptEngine->GetJITCompiler();
		FAllocationCompiler Compiler(
			Fixture.Selection(0),
			Fixture.Selection(1),
			Accepted);
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetEngineProperty(
				asEP_INCLUDE_JIT_INSTRUCTIONS,
				1)));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler)));
		const FName ModuleName(TEXT("UASFunctionImmutableSetAllocation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ScriptEngine->SetJITCompiler(OriginalCompiler);
			ScriptEngine->SetEngineProperty(
				asEP_INCLUDE_JIT_INSTRUCTIONS,
				OriginalIncludeJIT);
		};

		const FString Source = ASTEST_AS(R"AS(
			UCLASS()
			class UUASFunctionImmutableSetAllocationTarget : UObject
			{
				UFUNCTION()
				int ReloadableValue() final
				{
					return 1;
				}

				UFUNCTION()
				int ImmutableValue() final
				{
					return 2;
				}
			}
			)AS");
		UClass* const GeneratedClass =
			AngelscriptFunctionalTestUtils::CompileScriptModule(
				*TestRunner,
				Engine,
				ModuleName,
				TEXT("UASFunctionImmutableSetAllocation.as"),
				Source,
				TEXT("UUASFunctionImmutableSetAllocationTarget"));
		ASSERT_THAT(IsNotNull(GeneratedClass));
		if (GeneratedClass == nullptr)
		{
			return;
		}

		UASFunction* const Reloadable = Cast<UASFunction>(
			FindGeneratedFunction(GeneratedClass, TEXT("ReloadableValue")));
		UASFunction* const Immutable = Cast<UASFunction>(
			FindGeneratedFunction(GeneratedClass, TEXT("ImmutableValue")));
		ASSERT_THAT(IsNotNull(Reloadable));
		ASSERT_THAT(IsNotNull(Immutable));
		ASSERT_THAT(IsTrue(Reloadable != nullptr
			&& Reloadable->GetClass()
				== UASFunction_DWordReturn::StaticClass()));
		ASSERT_THAT(IsTrue(Immutable != nullptr
			&& Immutable->GetClass()
				== UASFunction_DWordReturn_JIT::StaticClass()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
