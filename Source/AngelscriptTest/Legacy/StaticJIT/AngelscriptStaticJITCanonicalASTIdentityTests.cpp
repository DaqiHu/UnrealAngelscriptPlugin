#include "AngelscriptEngine.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "StaticJIT/AngelscriptStaticJITGenerationProfile.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCanonical.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEligibility.h"

#include "Containers/Set.h"
#include "HAL/FileManager.h"
#include "Templates/Function.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/AST/as_ast_context.h"
#include "source/as_ast_kind.h"
#include "frontend/AST/as_decl.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "frontend/Sema/as_sema.h"
#include "source/as_runtime_type_bridge.h"
#include "as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITCanonicalASTIdentityTests,
	"Angelscript.TestModule.StaticJIT.CanonicalASTIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FFixture
	{
		FString Root;
		TUniquePtr<FAngelscriptEngine> Engine;

		~FFixture()
		{
			Engine.Reset();
			if (Root.Contains(TEXT("/Saved/Automation/StaticJITCanonicalASTIdentity/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}
	};

	static TUniquePtr<FFixture> CreateRetainedGenerationFixture(
		FAutomationTestBase& Test,
		const FString& ScriptSource,
		TFunction<bool(asCScriptEngine&)> Setup = nullptr,
		const asECompilerPipeline CompilerPipeline =
			asCOMPILER_PIPELINE_CANONICAL)
	{
		FAngelscriptStaticJITGenerationProfile GenerationProfile;
		FString ProfileError;
		if (!Test.TestTrue(
			TEXT("identity fixture resolves the Bytecode profile"),
			FAngelscriptStaticJITGenerationProfile::TryCreate(
				FAngelscriptStaticJITBackendId::Bytecode(),
				EAngelscriptJITTargetProfile::EditorDevelopment,
				GenerationProfile,
				&ProfileError)))
		{
			Test.AddError(ProfileError);
			return nullptr;
		}

		TUniquePtr<FFixture> Fixture = MakeUnique<FFixture>();
		Fixture->Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/StaticJITCanonicalASTIdentity"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		FPaths::NormalizeDirectoryName(Fixture->Root);
		const FString ScriptRoot = FPaths::Combine(
			Fixture->Root, TEXT("Script/StaticJIT/CanonicalASTIdentity"));
		const FString ScriptPath = FPaths::Combine(
			ScriptRoot, TEXT("CanonicalASTIdentity.as"));
		if (!Test.TestTrue(TEXT("identity fixture creates Script root"),
				IFileManager::Get().MakeDirectory(*ScriptRoot, true))
			|| !Test.TestTrue(TEXT("identity fixture writes source"),
				FFileHelper::SaveStringToFile(ScriptSource, *ScriptPath)))
		{
			return nullptr;
		}

		FAngelscriptEngineConfig Config;
		Config.Purpose = EAngelscriptEnginePurpose::StaticJITGeneration;
		Config.InitialASTRetentionPolicy = asAST_RETAIN_SNAPSHOT;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.bRunningCommandlet = true;
		Config.bIsUnattended = true;
		Config.bSkipInitialCompile = true;
		Config.bDisableCacheV2Persistence = true;
		Config.bForceSerialCacheV2Preparation = true;
		Config.CacheV2RootOverride = FPaths::Combine(Fixture->Root, TEXT("CacheV2"));
		GenerationProfile.ApplyToEngineConfig(Config);

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		const FString ProjectRoot = Fixture->Root;
		Dependencies.GetProjectDir = [ProjectRoot]()
		{
			return ProjectRoot;
		};
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};
		Dependencies.HandleStartupCompileFailureExit =
			[](const FAngelscriptStartupCompileFailureExitRequest&)
			{
			};

		Fixture->Engine = FAngelscriptEngine::Create(Config, Dependencies);
		if (!Test.TestNotNull(TEXT("identity fixture creates Engine"), Fixture->Engine.Get()))
		{
			return nullptr;
		}

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Fixture->Engine->GetScriptEngine());
		if (!Test.TestNotNull(TEXT("identity fixture has ScriptEngine"), ScriptEngine)
			|| !Test.TestTrue(
				TEXT("identity fixture selects the requested compiler pipeline"),
				ScriptEngine->SetCompilerPipeline(CompilerPipeline)))
		{
			return nullptr;
		}
		if (Setup && !Test.TestTrue(
			TEXT("identity fixture host setup"),
			Setup(*ScriptEngine)))
		{
			return nullptr;
		}

		{
			FAngelscriptEngineScope EngineScope(*Fixture->Engine);
			Fixture->Engine->InitialCompile();
		}
		if (!Test.TestTrue(
			TEXT("identity fixture source compiles"),
			Fixture->Engine->DidInitialCompileSucceed()))
		{
			return nullptr;
		}
		return Fixture;
	}

	static const asCDecl* BoundDecl(
		const FAngelscriptStaticJITGenerationFunction& Function)
	{
		if (Function.SealedAST == nullptr || Function.CanonicalFunctionDeclValue == 0)
		{
			return nullptr;
		}
		return Function.SealedAST->GetDecl(
			asASTDeclId(Function.CanonicalFunctionDeclValue));
	}

	static FString DescribeRuntimeDeclarationBinding(
		const FAngelscriptStaticJITGenerationFunction& Function)
	{
		const asCScriptFunction* const RuntimeFunction =
			static_cast<const asCScriptFunction*>(Function.EngineLocalFunction);
		if (Function.SealedAST == nullptr || RuntimeFunction == nullptr)
		{
			return TEXT("sealed AST or Runtime function unavailable");
		}

		asCRuntimeTypeBridge Bridge(RuntimeFunction->engine);
		asASTDeclId Bound;
		asCString Detail;
		const int Result = Bridge.BindFunctionDeclaration(
			*Function.SealedAST, *RuntimeFunction, Bound, &Detail);
		return FString::Printf(
			TEXT("result=%d bound=%u key='%s' runtime='%s' detail='%s'"),
			Result,
			Bound.value,
			UTF8_TO_TCHAR(RuntimeFunction->canonicalASTStableDeclKey.AddressOf()),
			*Function.CanonicalDeclaration,
			UTF8_TO_TCHAR(Detail.AddressOf()));
	}

	static FString DescribeSnapshotFunctions(
		const FAngelscriptStaticJITGenerationSnapshot& Snapshot)
	{
		FString Text;
		for (const FAngelscriptStaticJITGenerationFunction& Function : Snapshot.Functions)
		{
			asIScriptFunction* const Fn = Function.EngineLocalFunction;
			const char* const Name = Fn != nullptr ? Fn->GetName() : "";
			const char* const NS = Fn != nullptr ? Fn->GetNamespace() : "";
			const char* const Obj = (Fn != nullptr && Fn->GetObjectType() != nullptr)
				? Fn->GetObjectType()->GetName() : "";
			Text += FString::Printf(
				TEXT("[name=%s ns=%s obj=%s decl=%s declId=%u sealed=%s] "),
				UTF8_TO_TCHAR(Name != nullptr ? Name : ""),
				UTF8_TO_TCHAR(NS != nullptr ? NS : ""),
				UTF8_TO_TCHAR(Obj != nullptr ? Obj : ""),
				*Function.CanonicalDeclaration,
				Function.CanonicalFunctionDeclValue,
				Function.SealedAST != nullptr ? TEXT("y") : TEXT("n"));
		}
		return Text;
	}

	static const FAngelscriptStaticJITGenerationFunction* FindNamedSnapshotFunction(
		const FAngelscriptStaticJITGenerationSnapshot& Snapshot,
		const char* Name,
		const char* Namespace,
		const TCHAR* DeclarationNeedle,
		const char* ObjectType = nullptr)
	{
		const FAngelscriptStaticJITGenerationFunction* Fallback = nullptr;
		int32 NameNsHits = 0;
		for (const FAngelscriptStaticJITGenerationFunction& Function : Snapshot.Functions)
		{
			asIScriptFunction* const Fn = Function.EngineLocalFunction;
			if (Fn == nullptr
				|| Fn->GetName() == nullptr
				|| FCStringAnsi::Strcmp(Fn->GetName(), Name) != 0)
			{
				continue;
			}
			const char* const NS = Fn->GetNamespace() != nullptr ? Fn->GetNamespace() : "";
			const char* const WantNS = Namespace != nullptr ? Namespace : "";
			if (FCStringAnsi::Strcmp(NS, WantNS) != 0)
			{
				continue;
			}
			if (ObjectType != nullptr)
			{
				const char* const Obj = Fn->GetObjectType() != nullptr
					? Fn->GetObjectType()->GetName() : "";
				if (FCStringAnsi::Strcmp(Obj != nullptr ? Obj : "", ObjectType) != 0)
				{
					continue;
				}
			}
			++NameNsHits;
			if (DeclarationNeedle == nullptr
				|| Function.CanonicalDeclaration.Contains(DeclarationNeedle))
			{
				return &Function;
			}
			Fallback = &Function;
		}
		if (NameNsHits == 1)
		{
			return Fallback;
		}
		return nullptr;
	}

public:
	TEST_METHOD(LegacyRetainedUFunctionCompletesOneCanonicalDecl)
	{
		TUniquePtr<FFixture> Fixture = CreateRetainedGenerationFixture(
			*TestRunner,
			FString(TEXT(R"AS(
UFUNCTION()
int TypedASTCapabilityShowcase(
	int A,
	int B,
	bool bEnabled,
	bool bExtraCheck)
{
	int Value = A + B;
	Value = Value - 1;
	Value += 3;
	Value -= 1;
	Value *= 2;

	int PrefixResult = ++Value;
	int PostfixResult = Value++;
	int Negated = -PrefixResult;
	int Inverted = ~PostfixResult;
	int ShiftedLeft = Value << 1;
	int ShiftedRight = Value >> 1;

	bool bMutationGate = bEnabled && (++Value > B);
	bool bNativeGate = bMutationGate && bExtraCheck;
	if (bNativeGate)
	{
		return ShiftedLeft + Inverted + Value;
	}
	if (ShiftedRight > 10)
	{
		return ShiftedRight * 2 + Value;
	}
	return Negated - 1 + Value;
}
)AS")),
			nullptr,
			asCOMPILER_PIPELINE_LEGACY);
		ASSERT_THAT(IsNotNull(Fixture.Get(),
			TEXT("retained LEGACY UFUNCTION identity fixture")));
		if (!Fixture.IsValid())
		{
			return;
		}

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid(),
			TEXT("retained LEGACY UFUNCTION snapshot")));
		if (!Snapshot.IsValid())
		{
			return;
		}

		const FAngelscriptStaticJITGenerationFunction* const Function =
			FindNamedSnapshotFunction(
				*Snapshot,
				"TypedASTCapabilityShowcase",
				"",
				TEXT("const int"));
		ASSERT_THAT(IsNotNull(Function,
			TEXT("retained LEGACY snapshot contains the UFUNCTION")));
		if (Function == nullptr || Function->SealedAST == nullptr)
		{
			return;
		}

		int32 ExactDeclCount = 0;
		const asCDecl* ExactDecl = nullptr;
		FString ExactDeclDescription;
		for (asUINT DeclIndex = 1;
			DeclIndex <= Function->SealedAST->GetDeclCount();
			++DeclIndex)
		{
			const asCDecl* const Candidate = Function->SealedAST->GetDecl(
				asASTDeclId(DeclIndex));
			if (Candidate != nullptr
				&& Candidate->kind == asAST_DECL_FUNCTION
				&& Candidate->stableKey.Equals(
					"TypedASTCapabilityShowcase(int,int,bool,bool)"))
			{
				++ExactDeclCount;
				ExactDecl = Candidate;
				ExactDeclDescription += FString::Printf(
					TEXT("[id=%u parent=%u range=%u:%u-%u body=%u origin='%s'] "),
					Candidate->id.value,
					Candidate->parent.value,
					Candidate->range.begin.fileID,
					Candidate->range.begin.offset,
					Candidate->range.end.offset,
					Candidate->body.value,
					UTF8_TO_TCHAR(Candidate->origin.AddressOf()));
			}
		}

		ASSERT_THAT(AreEqual(1, ExactDeclCount,
			*FString::Printf(
				TEXT("a provisional function declaration must be completed, not duplicated: %s"),
				*ExactDeclDescription)));
		ASSERT_THAT(IsNotNull(ExactDecl,
			TEXT("the retained AST contains the exact completed declaration")));
		if (ExactDecl != nullptr)
		{
			ASSERT_THAT(IsTrue(ExactDecl->body.IsValid(),
				TEXT("the unique retained declaration owns the parsed body")));
		}
		ASSERT_THAT(AreNotEqual(
			static_cast<asUINT>(0),
			Function->CanonicalFunctionDeclValue,
			TEXT("the frozen generation snapshot binds the unique declaration")));
	}

	TEST_METHOD(CanonicalProducerCarriesStableDeclIdentity)
	{
		TUniquePtr<FFixture> Fixture = CreateRetainedGenerationFixture(
			*TestRunner,
			FString(TEXT(R"AS(
int F(int a)
{
	return 1;
}

int F(float a)
{
	return 2;
}
)AS")));
		ASSERT_THAT(IsNotNull(Fixture.Get(), TEXT("producer-carried identity fixture")));
		if (!Fixture.IsValid())
		{
			return;
		}

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot, ESPMode::ThreadSafe>
			Snapshot = Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid(), TEXT("producer-carried identity snapshot")));
		if (!Snapshot.IsValid())
		{
			return;
		}

		const FAngelscriptStaticJITGenerationFunction* const IntFn =
			FindNamedSnapshotFunction(*Snapshot, "F", "", TEXT("const int"));
		const FAngelscriptStaticJITGenerationFunction* const FloatFn =
			FindNamedSnapshotFunction(*Snapshot, "F", "", TEXT("const float"));
		ASSERT_THAT(IsNotNull(IntFn, TEXT("producer identity fixture has F(int)")));
		ASSERT_THAT(IsNotNull(FloatFn, TEXT("producer identity fixture has F(float)")));
		if (IntFn == nullptr || FloatFn == nullptr)
		{
			return;
		}

		const asCDecl* const IntDecl = BoundDecl(*IntFn);
		asCScriptFunction* const RuntimeInt =
			static_cast<asCScriptFunction*>(IntFn->EngineLocalFunction);
		ASSERT_THAT(IsNotNull(IntDecl,
			*FString::Printf(TEXT("F(int) binds a sealed declaration: %s"),
				*DescribeRuntimeDeclarationBinding(*IntFn))));
		ASSERT_THAT(IsNotNull(RuntimeInt, TEXT("F(int) has a Runtime function")));
		if (IntDecl == nullptr || RuntimeInt == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			FString(UTF8_TO_TCHAR(IntDecl->stableKey.AddressOf())),
			FString(UTF8_TO_TCHAR(RuntimeInt->canonicalASTStableDeclKey.AddressOf())),
			TEXT("canonical CodeGen must carry the exact sealed declaration key")));
	}

	TEST_METHOD(ProducerCarriedIdentityRejectsCrossOverloadSignatureMismatch)
	{
		TUniquePtr<FFixture> Fixture = CreateRetainedGenerationFixture(
			*TestRunner,
			FString(TEXT(R"AS(
int F(int a)
{
	return 1;
}

int F(float a)
{
	return 2;
}
)AS")));
		ASSERT_THAT(IsNotNull(Fixture.Get(), TEXT("producer identity mismatch fixture")));
		if (!Fixture.IsValid())
		{
			return;
		}

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot, ESPMode::ThreadSafe>
			Snapshot = Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid(), TEXT("producer identity mismatch snapshot")));
		if (!Snapshot.IsValid())
		{
			return;
		}

		const FAngelscriptStaticJITGenerationFunction* const IntFn =
			FindNamedSnapshotFunction(*Snapshot, "F", "", TEXT("const int"));
		const FAngelscriptStaticJITGenerationFunction* const FloatFn =
			FindNamedSnapshotFunction(*Snapshot, "F", "", TEXT("const float"));
		ASSERT_THAT(IsNotNull(IntFn, TEXT("mismatch fixture has F(int)")));
		ASSERT_THAT(IsNotNull(FloatFn, TEXT("mismatch fixture has F(float)")));
		if (IntFn == nullptr || FloatFn == nullptr)
		{
			return;
		}

		const asCDecl* const FloatDecl = BoundDecl(*FloatFn);
		asCScriptFunction* const RuntimeInt =
			static_cast<asCScriptFunction*>(IntFn->EngineLocalFunction);
		ASSERT_THAT(IsNotNull(FloatDecl, TEXT("F(float) binds a sealed declaration")));
		ASSERT_THAT(IsNotNull(RuntimeInt, TEXT("F(int) has a Runtime function")));
		if (FloatDecl == nullptr || RuntimeInt == nullptr)
		{
			return;
		}

		const asCString OriginalKey = RuntimeInt->canonicalASTStableDeclKey;
		ON_SCOPE_EXIT
		{
			RuntimeInt->canonicalASTStableDeclKey = OriginalKey;
		};
		RuntimeInt->canonicalASTStableDeclKey = FloatDecl->stableKey;

		asCRuntimeTypeBridge Bridge(RuntimeInt->engine);
		asASTDeclId Bound;
		asCString Detail;
		const int BindResult = Bridge.BindFunctionDeclaration(
			*IntFn->SealedAST, *RuntimeInt, Bound, &Detail);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_DECL_BIND_SIGNATURE_MISMATCH),
			static_cast<int32>(BindResult),
			*FString::Printf(
				TEXT("F(int) carrying F(double)'s key must fail as a signature mismatch. detail=%s"),
				UTF8_TO_TCHAR(Detail.AddressOf()))));
		ASSERT_THAT(IsFalse(Bound.IsValid(),
			TEXT("a signature-mismatched carried key must not publish a DeclId")));
		ASSERT_THAT(IsTrue(
			FString(UTF8_TO_TCHAR(Detail.AddressOf())).Contains(
				TEXT("parameter[0] type mismatch")),
			*FString::Printf(
				TEXT("identity diagnostics must name the exact mismatched field; detail=%s"),
				UTF8_TO_TCHAR(Detail.AddressOf()))));
	}

	TEST_METHOD(ProducerCarriedIdentityRejectsAmbiguousStableKey)
	{
		TUniquePtr<FFixture> Fixture = CreateRetainedGenerationFixture(
			*TestRunner,
			FString(TEXT(R"AS(
int F(int a)
{
	return a;
}
)AS")));
		ASSERT_THAT(IsNotNull(Fixture.Get(), TEXT("producer identity ambiguity fixture")));
		if (!Fixture.IsValid())
		{
			return;
		}

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot, ESPMode::ThreadSafe>
			Snapshot = Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid(), TEXT("producer identity ambiguity snapshot")));
		if (!Snapshot.IsValid())
		{
			return;
		}

		const FAngelscriptStaticJITGenerationFunction* const RuntimeF =
			FindNamedSnapshotFunction(*Snapshot, "F", "", TEXT("const int"));
		ASSERT_THAT(IsNotNull(RuntimeF, TEXT("ambiguity fixture has F(int)")));
		if (RuntimeF == nullptr)
		{
			return;
		}
		asCScriptFunction* const Function =
			static_cast<asCScriptFunction*>(RuntimeF->EngineLocalFunction);
		ASSERT_THAT(IsNotNull(Function, TEXT("ambiguity fixture has a Runtime function")));
		if (Function == nullptr)
		{
			return;
		}

		asCASTContext AmbiguousContext;
		asCSema Sema(Function->engine, AmbiguousContext);
		const asASTDeclId TranslationUnit =
			Sema.ActOnTranslationUnit("AmbiguousIdentity");
		const asCQualType IntType = AmbiguousContext.InternPrimitive(ttInt, 0);
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const asASTDeclId Duplicate = Sema.ActOnStartFunctionDecl(
				TranslationUnit, "F", IntType, asCSourceRange());
			Sema.ActOnParamDecl(Duplicate, "a", IntType, asCSourceRange());
			Sema.FinishDecl(Duplicate);
		}
		ASSERT_THAT(AreEqual(0, AmbiguousContext.Seal(),
			TEXT("duplicate-key identity firewall fixture must seal")));

		asCRuntimeTypeBridge Bridge(Function->engine);
		asASTDeclId Bound;
		asCString Detail;
		const int BindResult = Bridge.BindFunctionDeclaration(
			AmbiguousContext, *Function, Bound, &Detail);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_DECL_BIND_AMBIGUOUS),
			static_cast<int32>(BindResult),
			*FString::Printf(
				TEXT("two exact stable keys must fail as ambiguous. detail=%s"),
				UTF8_TO_TCHAR(Detail.AddressOf()))));
		ASSERT_THAT(IsFalse(Bound.IsValid(),
			TEXT("an ambiguous stable key must not publish a DeclId")));
	}

	TEST_METHOD(UniqueSignatureOverloadsBindExactStableKeys)
	{
		TUniquePtr<FFixture> Fixture = CreateRetainedGenerationFixture(
			*TestRunner,
			FString(TEXT(R"AS(
int F(int a)
{
	return 1;
}

int F(float a)
{
	return 0;
}
)AS")));
		ASSERT_THAT(IsNotNull(Fixture.Get(), TEXT("unique-signature fixture")));
		if (!Fixture.IsValid())
		{
			return;
		}

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot, ESPMode::ThreadSafe>
			Snapshot = Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid(), TEXT("unique-signature snapshot")));
		if (!Snapshot.IsValid())
		{
			return;
		}

		const FAngelscriptStaticJITGenerationFunction* const IntFn =
			FindNamedSnapshotFunction(*Snapshot, "F", "", TEXT("const int"));
		const FAngelscriptStaticJITGenerationFunction* const FloatFn =
			FindNamedSnapshotFunction(*Snapshot, "F", "", TEXT("const float"));
		const FString SnapshotDump = DescribeSnapshotFunctions(*Snapshot);
		ASSERT_THAT(IsNotNull(IntFn,
			*FString::Printf(TEXT("Runtime F(int) must appear in the snapshot. seen: %s"),
				*SnapshotDump)));
		ASSERT_THAT(IsNotNull(FloatFn,
			*FString::Printf(TEXT("Runtime F(float) must appear in the snapshot. seen: %s"),
				*SnapshotDump)));
		if (IntFn == nullptr || FloatFn == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(IntFn->SealedAST, TEXT("retained sealed AST is required for identity bind")));
		const asCDecl* const IntDecl = BoundDecl(*IntFn);
		const asCDecl* const FloatDecl = BoundDecl(*FloatFn);
		ASSERT_THAT(IsNotNull(IntDecl, TEXT("F(int) must bind a unique AST decl, not DeclId 0")));
		ASSERT_THAT(IsNotNull(FloatDecl, TEXT("F(float) must bind a unique AST decl, not DeclId 0")));
		if (IntDecl == nullptr || FloatDecl == nullptr)
		{
			return;
		}

		const FString IntKey = UTF8_TO_TCHAR(IntDecl->stableKey.AddressOf());
		const FString FloatKey = UTF8_TO_TCHAR(FloatDecl->stableKey.AddressOf());
		ASSERT_THAT(AreEqual(FString(TEXT("F(int)")), IntKey,
			TEXT("Runtime F(int) must bind AST stableKey F(int), not first-name")));
		// This fixture is compiled with asEP_FLOAT_IS_FLOAT64 enabled. The
		// language spelling remains `float` in the runtime declaration, but the
		// canonical AST stores the resolved ttFloat64 semantic type as `double`.
		ASSERT_THAT(AreEqual(FString(TEXT("F(double)")), FloatKey,
			TEXT("Runtime F(float) must bind the canonical F(double) stableKey, not first-name")));
		ASSERT_THAT(AreNotEqual(IntFn->CanonicalFunctionDeclValue, FloatFn->CanonicalFunctionDeclValue,
			TEXT("int and float overloads must not share one DeclId")));
	}

	TEST_METHOD(NamespaceTwinsBindDistinctStableKeysNotZeroOrFirstName)
	{
		TUniquePtr<FFixture> Fixture = CreateRetainedGenerationFixture(
			*TestRunner,
			FString(TEXT(R"AS(
int F(int a)
{
	return 1;
}

namespace N
{
	int F(int a)
	{
		return 2;
	}
}
)AS")));
		ASSERT_THAT(IsNotNull(Fixture.Get(), TEXT("namespace-twin fixture")));
		if (!Fixture.IsValid())
		{
			return;
		}

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot, ESPMode::ThreadSafe>
			Snapshot = Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid(), TEXT("namespace-twin snapshot")));
		if (!Snapshot.IsValid())
		{
			return;
		}

		const FAngelscriptStaticJITGenerationFunction* const GlobalF =
			FindNamedSnapshotFunction(*Snapshot, "F", "", nullptr);
		const FAngelscriptStaticJITGenerationFunction* const NestedF =
			FindNamedSnapshotFunction(*Snapshot, "F", "N", nullptr);
		const FString SnapshotDump = DescribeSnapshotFunctions(*Snapshot);
		ASSERT_THAT(IsNotNull(GlobalF,
			*FString::Printf(TEXT("Runtime global F(int) must appear in the snapshot. seen: %s"),
				*SnapshotDump)));
		ASSERT_THAT(IsNotNull(NestedF,
			*FString::Printf(TEXT("Runtime N::F(int) must appear in the snapshot. seen: %s"),
				*SnapshotDump)));
		if (GlobalF == nullptr || NestedF == nullptr)
		{
			return;
		}

		const asCDecl* const GlobalDecl = BoundDecl(*GlobalF);
		const asCDecl* const NestedDecl = BoundDecl(*NestedF);
		ASSERT_THAT(IsNotNull(GlobalDecl,
			TEXT("global F(int) must bind AST F(int); name+params MatchCount=2 must not leave DeclId 0")));
		ASSERT_THAT(IsNotNull(NestedDecl,
			TEXT("N::F(int) must bind AST N::F(int); must not first-name-bind global F or DeclId 0")));
		if (GlobalDecl == nullptr || NestedDecl == nullptr)
		{
			return;
		}

		const FString GlobalKey = UTF8_TO_TCHAR(GlobalDecl->stableKey.AddressOf());
		const FString NestedKey = UTF8_TO_TCHAR(NestedDecl->stableKey.AddressOf());
		ASSERT_THAT(AreEqual(FString(TEXT("F(int)")), GlobalKey,
			TEXT("global FunctionKey must bind stableKey F(int)")));
		ASSERT_THAT(AreEqual(FString(TEXT("N::F(int)")), NestedKey,
			TEXT("N::F FunctionKey must bind stableKey N::F(int), not global F(int)")));
		ASSERT_THAT(AreNotEqual(GlobalF->CanonicalFunctionDeclValue, NestedF->CanonicalFunctionDeclValue,
			TEXT("namespace twins must not share one DeclId")));

		FAngelscriptTypedASTJITEligibilityInput NestedEligibility;
		NestedEligibility.bIsUFunctionRoot = true;
		NestedEligibility.SealedAST = NestedF->SealedAST;
		NestedEligibility.CanonicalFunctionDecl =
			asASTDeclId(NestedF->CanonicalFunctionDeclValue);
		const FAngelscriptTypedASTJITEligibility NestedResult =
			EvaluateAngelscriptTypedASTJITEligibilityFromCanonical(NestedEligibility);
		ASSERT_THAT(IsTrue(NestedResult.bEligible,
			TEXT("exact N::F DeclId must be usable for eligibility")));
		ASSERT_THAT(IsFalse(NestedResult.Detail.Contains(TEXT("return 1")),
			TEXT("N::F eligibility must not observe global F's body")));
	}

	TEST_METHOD(ZeroCanonicalDeclDoesNotWalkWholeModuleOrBindFirstFunction)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("JitZeroDecl");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId First = Sema.ActOnFunctionDecl(Tu, "First", IntType, asCSourceRange());
		Sema.FinishDecl(First);
		const asASTDeclId Second = Sema.ActOnFunctionDecl(Tu, "Second", IntType, asCSourceRange());
		Sema.FinishDecl(Second);
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("zero-decl eligibility fixture must seal")));

		FAngelscriptTypedASTJITEligibilityInput Input;
		Input.bIsUFunctionRoot = true;
		Input.SealedAST = &Context;
		Input.CanonicalFunctionDecl = asASTDeclId();
		const FAngelscriptTypedASTJITEligibility Result =
			EvaluateAngelscriptTypedASTJITEligibilityFromCanonical(Input);
		ASSERT_THAT(IsFalse(Result.bEligible,
			TEXT("DeclId 0 must fail closed, not walk every stmt / first function")));
		ASSERT_THAT(IsTrue(
			Result.Detail.Contains(TEXT("CanonicalFunctionDecl"))
				|| Result.Detail.Contains(TEXT("DeclId")),
			*FString::Printf(TEXT("reject detail must name the missing identity. got: %s"),
				*Result.Detail)));
	}

	TEST_METHOD(MethodOverloadsBindOwnerQualifiedStableKeys)
	{
		TUniquePtr<FFixture> Fixture = CreateRetainedGenerationFixture(
			*TestRunner,
			FString(TEXT(R"AS(
class T
{
	int F(int a)
	{
		return 1;
	}

	int F(float a)
	{
		return 0;
	}
}

int Entry()
{
	T v;
	return v.F(1);
}
)AS")));
		ASSERT_THAT(IsNotNull(Fixture.Get(), TEXT("method-overload fixture")));
		if (!Fixture.IsValid())
		{
			return;
		}

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot, ESPMode::ThreadSafe>
			Snapshot = Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid(), TEXT("method-overload snapshot")));
		if (!Snapshot.IsValid())
		{
			return;
		}

		const FString SnapshotDump = DescribeSnapshotFunctions(*Snapshot);
		const FAngelscriptStaticJITGenerationFunction* const IntFn =
			FindNamedSnapshotFunction(*Snapshot, "F", "", TEXT("const int"), "T");
		const FAngelscriptStaticJITGenerationFunction* const FloatFn =
			FindNamedSnapshotFunction(*Snapshot, "F", "", TEXT("const float"), "T");
		ASSERT_THAT(IsNotNull(IntFn,
			*FString::Printf(TEXT("Runtime T::F(int) must appear. seen: %s"), *SnapshotDump)));
		ASSERT_THAT(IsNotNull(FloatFn,
			*FString::Printf(TEXT("Runtime T::F(float) must appear. seen: %s"), *SnapshotDump)));
		if (IntFn == nullptr || FloatFn == nullptr)
		{
			return;
		}

		const asCDecl* const IntDecl = BoundDecl(*IntFn);
		const asCDecl* const FloatDecl = BoundDecl(*FloatFn);
		ASSERT_THAT(IsNotNull(IntDecl, TEXT("T::F(int) must bind a unique AST decl, not DeclId 0")));
		ASSERT_THAT(IsNotNull(FloatDecl, TEXT("T::F(float) must bind a unique AST decl, not DeclId 0")));
		if (IntDecl == nullptr || FloatDecl == nullptr)
		{
			return;
		}

		const FString IntKey = UTF8_TO_TCHAR(IntDecl->stableKey.AddressOf());
		const FString FloatKey = UTF8_TO_TCHAR(FloatDecl->stableKey.AddressOf());
		ASSERT_THAT(AreEqual(FString(TEXT("T::F(int)")), IntKey,
			TEXT("Runtime T::F(int) must bind AST T::F(int), not first method")));
		// See the global-overload case above: float is double-backed for this
		// engine and stable keys reflect the resolved semantic type.
		ASSERT_THAT(AreEqual(FString(TEXT("T::F(double)")), FloatKey,
			TEXT("Runtime T::F(float) must bind canonical T::F(double), not first method")));
		ASSERT_THAT(AreNotEqual(IntFn->CanonicalFunctionDeclValue, FloatFn->CanonicalFunctionDeclValue,
			TEXT("method overloads must not share one DeclId")));
	}

	TEST_METHOD(ConstructorDestructorBindDistinctStableKeys)
	{
		TUniquePtr<FFixture> Fixture = CreateRetainedGenerationFixture(
			*TestRunner,
			FString(TEXT(R"AS(
class T
{
	T() {}
	T(int a) {}
	~T() {}
}

int Entry()
{
	T a;
	return 1;
}
)AS")));
		ASSERT_THAT(IsNotNull(Fixture.Get(), TEXT("ctor-dtor fixture")));
		if (!Fixture.IsValid())
		{
			return;
		}

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot, ESPMode::ThreadSafe>
			Snapshot = Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid(), TEXT("ctor-dtor snapshot")));
		if (!Snapshot.IsValid())
		{
			return;
		}

		const FString SnapshotDump = DescribeSnapshotFunctions(*Snapshot);
		const FAngelscriptStaticJITGenerationFunction* const ZeroCtor =
			FindNamedSnapshotFunction(*Snapshot, "T", "", TEXT("T()"), "T");
		const FAngelscriptStaticJITGenerationFunction* const IntCtor =
			FindNamedSnapshotFunction(*Snapshot, "T", "", TEXT("const int"), "T");
		const FAngelscriptStaticJITGenerationFunction* const Dtor =
			FindNamedSnapshotFunction(*Snapshot, "~T", "", nullptr, "T");
		ASSERT_THAT(IsNotNull(ZeroCtor,
			*FString::Printf(TEXT("Runtime T::T() must appear. seen: %s"), *SnapshotDump)));
		ASSERT_THAT(IsNotNull(IntCtor,
			*FString::Printf(TEXT("Runtime T::T(int) must appear. seen: %s"), *SnapshotDump)));
		ASSERT_THAT(IsNotNull(Dtor,
			*FString::Printf(TEXT("Runtime T::~T() must appear. seen: %s"), *SnapshotDump)));
		if (ZeroCtor == nullptr || IntCtor == nullptr || Dtor == nullptr)
		{
			return;
		}

		const asCDecl* const ZeroDecl = BoundDecl(*ZeroCtor);
		const asCDecl* const IntDecl = BoundDecl(*IntCtor);
		const asCDecl* const DtorDecl = BoundDecl(*Dtor);
		ASSERT_THAT(IsNotNull(ZeroDecl,
			*FString::Printf(TEXT("T::T() must bind a unique AST decl: %s"),
				*DescribeRuntimeDeclarationBinding(*ZeroCtor))));
		ASSERT_THAT(IsNotNull(IntDecl,
			*FString::Printf(TEXT("T::T(int) must bind a unique AST decl: %s"),
				*DescribeRuntimeDeclarationBinding(*IntCtor))));
		ASSERT_THAT(IsNotNull(DtorDecl,
			*FString::Printf(TEXT("T::~T() must bind a unique AST decl: %s"),
				*DescribeRuntimeDeclarationBinding(*Dtor))));
		if (ZeroDecl == nullptr || IntDecl == nullptr || DtorDecl == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("T::T()")),
			FString(UTF8_TO_TCHAR(ZeroDecl->stableKey.AddressOf())),
			TEXT("zero-arg constructor must bind T::T()")));
		ASSERT_THAT(AreEqual(FString(TEXT("T::T(int)")),
			FString(UTF8_TO_TCHAR(IntDecl->stableKey.AddressOf())),
			TEXT("int constructor must bind T::T(int), not first constructor")));
		ASSERT_THAT(AreEqual(FString(TEXT("T::~T()")),
			FString(UTF8_TO_TCHAR(DtorDecl->stableKey.AddressOf())),
			TEXT("destructor must bind T::~T(), not T::T()")));
		ASSERT_THAT(AreNotEqual(ZeroCtor->CanonicalFunctionDeclValue, IntCtor->CanonicalFunctionDeclValue,
			TEXT("constructors must not share one DeclId")));
		ASSERT_THAT(AreNotEqual(ZeroCtor->CanonicalFunctionDeclValue, Dtor->CanonicalFunctionDeclValue,
			TEXT("destructor must not share T::T() DeclId")));
	}

	TEST_METHOD(OperatorMethodBindsOpAddNotFirstMethod)
	{
		TUniquePtr<FFixture> Fixture = CreateRetainedGenerationFixture(
			*TestRunner,
			FString(TEXT(R"AS(
class T
{
	int F() { return 0; }
	int opAdd(int a) { return a; }
}

int Entry()
{
	T v;
	return v + 3;
}
)AS")));
		ASSERT_THAT(IsNotNull(Fixture.Get(), TEXT("operator fixture")));
		if (!Fixture.IsValid())
		{
			return;
		}

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot, ESPMode::ThreadSafe>
			Snapshot = Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid(), TEXT("operator snapshot")));
		if (!Snapshot.IsValid())
		{
			return;
		}

		const FString SnapshotDump = DescribeSnapshotFunctions(*Snapshot);
		const FAngelscriptStaticJITGenerationFunction* const OpAdd =
			FindNamedSnapshotFunction(*Snapshot, "opAdd", "", nullptr, "T");
		ASSERT_THAT(IsNotNull(OpAdd,
			*FString::Printf(TEXT("Runtime T::opAdd(int) must appear. seen: %s"), *SnapshotDump)));
		if (OpAdd == nullptr)
		{
			return;
		}

		const asCDecl* const OpDecl = BoundDecl(*OpAdd);
		ASSERT_THAT(IsNotNull(OpDecl, TEXT("T::opAdd(int) must bind a unique AST decl, not DeclId 0")));
		if (OpDecl == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("T::opAdd(int)")),
			FString(UTF8_TO_TCHAR(OpDecl->stableKey.AddressOf())),
			TEXT("operator FunctionKey must bind T::opAdd(int), not first method T::F()")));
	}

	TEST_METHOD(MixinBindsMixinDeclNotFirstFunction)
	{
		TUniquePtr<FFixture> Fixture = CreateRetainedGenerationFixture(
			*TestRunner,
			FString(TEXT(R"AS(
class T
{
}

mixin int MixHelper(T self, int a)
{
	return a;
}

int Entry()
{
	T v;
	return v.MixHelper(3);
}
)AS")));
		ASSERT_THAT(IsNotNull(Fixture.Get(), TEXT("mixin fixture")));
		if (!Fixture.IsValid())
		{
			return;
		}

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot, ESPMode::ThreadSafe>
			Snapshot = Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid(), TEXT("mixin snapshot")));
		if (!Snapshot.IsValid())
		{
			return;
		}

		const FString SnapshotDump = DescribeSnapshotFunctions(*Snapshot);
		const FAngelscriptStaticJITGenerationFunction* const MixinFn =
			FindNamedSnapshotFunction(*Snapshot, "MixHelper", "", nullptr, "");
		ASSERT_THAT(IsNotNull(MixinFn,
			*FString::Printf(TEXT("Runtime MixHelper must appear as a non-method. seen: %s"),
				*SnapshotDump)));
		if (MixinFn == nullptr)
		{
			return;
		}

		const asCDecl* const MixinDecl = BoundDecl(*MixinFn);
		ASSERT_THAT(IsNotNull(MixinDecl,
			TEXT("MixHelper must bind a unique AST mixin decl, not DeclId 0 or first function")));
		if (MixinDecl == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(static_cast<int32>(asAST_DECL_MIXIN), static_cast<int32>(MixinDecl->kind),
			TEXT("MixHelper must bind kind Mixin, not Function/Method")));
		ASSERT_THAT(AreEqual(FString(TEXT("MixHelper(T,int)")),
			FString(UTF8_TO_TCHAR(MixinDecl->stableKey.AddressOf())),
			TEXT("mixin FunctionKey must bind MixHelper(T,int)")));
	}

	TEST_METHOD(MultipleLambdasBindDistinctStableKeysNotZeroOrFirstName)
	{
		TUniquePtr<FFixture> Fixture = CreateRetainedGenerationFixture(
			*TestRunner,
			FString(TEXT(R"AS(
int Invoke(IdentityLambdaCallback Cb, int X)
{
	return Cb(X);
}

int Entry()
{
	return Invoke(function(int x) { return 1; }, 1)
		+ Invoke(function(int y) { return 2; }, 2);
}
)AS")),
			[](asCScriptEngine& ScriptEngine)
			{
				if (ScriptEngine.RegisterFuncdef("int IdentityLambdaCallback(int)") < 0)
				{
					return false;
				}
				asITypeInfo* const Type =
					ScriptEngine.GetTypeInfoByName("IdentityLambdaCallback");
				if (Type == nullptr)
				{
					return false;
				}
				// Fork tokenizer has no `@`; UE engines also reject funcdef-by-value.
				// Implicit handle is how UObject types are named without `@`.
				static_cast<asCTypeInfo*>(Type)->flags |= asOBJ_IMPLICIT_HANDLE;
				return true;
			});
		ASSERT_THAT(IsNotNull(Fixture.Get(), TEXT("lambda fixture")));
		if (!Fixture.IsValid())
		{
			return;
		}

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot, ESPMode::ThreadSafe>
			Snapshot = Fixture->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid(), TEXT("lambda snapshot")));
		if (!Snapshot.IsValid())
		{
			return;
		}

		const FString SnapshotDump = DescribeSnapshotFunctions(*Snapshot);
		TArray<const FAngelscriptStaticJITGenerationFunction*> Lambdas;
		TSet<asUINT> LambdaDeclIds;
		TSet<FString> LambdaKeys;
		for (const FAngelscriptStaticJITGenerationFunction& Function : Snapshot->Functions)
		{
			asIScriptFunction* const Fn = Function.EngineLocalFunction;
			if (Fn == nullptr || Fn->GetName() == nullptr || Fn->GetName()[0] != '$')
			{
				continue;
			}
			Lambdas.Add(&Function);
			const asCDecl* const Decl = BoundDecl(Function);
			ASSERT_THAT(IsNotNull(Decl,
				*FString::Printf(
					TEXT("lambda %s must bind a unique AST decl, not DeclId 0 or Entry. seen: %s"),
					UTF8_TO_TCHAR(Fn->GetName()),
					*SnapshotDump)));
			if (Decl == nullptr)
			{
				return;
			}
			ASSERT_THAT(IsTrue((Decl->traits & asAST_TRAIT_LAMBDA) != 0,
				TEXT("lambda FunctionKey must bind a TRAIT_LAMBDA decl, not first function")));
			const FString Key = UTF8_TO_TCHAR(Decl->stableKey.AddressOf());
			ASSERT_THAT(IsTrue(Key.Contains(TEXT("<lambda>")) && Key.Contains(TEXT("@")),
				*FString::Printf(TEXT("lambda stableKey must be <lambda>(...)@offset. got: %s"), *Key)));
			const asCScriptFunction* const RuntimeLambda =
				static_cast<const asCScriptFunction*>(Function.EngineLocalFunction);
			ASSERT_THAT(IsNotNull(RuntimeLambda,
				TEXT("lambda snapshot member keeps its Runtime function")));
			if (RuntimeLambda == nullptr)
			{
				return;
			}
			ASSERT_THAT(AreEqual(
				Key,
				FString(UTF8_TO_TCHAR(
					RuntimeLambda->canonicalASTStableDeclKey.AddressOf())),
				*FString::Printf(
					TEXT("canonical lambda must carry its exact source-offset stable key; rank matching is forbidden. expected='%s' actual='%s' bind=%s"),
					*Key,
					UTF8_TO_TCHAR(RuntimeLambda->canonicalASTStableDeclKey.AddressOf()),
					*DescribeRuntimeDeclarationBinding(Function))));
			LambdaDeclIds.Add(Function.CanonicalFunctionDeclValue);
			LambdaKeys.Add(Key);
		}

		ASSERT_THAT(IsTrue(Lambdas.Num() >= 2,
			*FString::Printf(TEXT("two Runtime lambdas must appear. seen: %s"), *SnapshotDump)));
		ASSERT_THAT(AreEqual(Lambdas.Num(), LambdaDeclIds.Num(),
			TEXT("multiple lambdas must not share one DeclId / first-name bind")));
		ASSERT_THAT(AreEqual(Lambdas.Num(), LambdaKeys.Num(),
			TEXT("multiple lambdas must keep distinct <lambda>@offset keys")));
	}
};

#endif
