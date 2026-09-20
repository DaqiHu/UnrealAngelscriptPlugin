#include "CQTest.h"
#include "as_scriptengine.h"
#include "as_definitions.h"
#include "as_engine_compile_registration.h"
#include "NativeEngine/SourceExecution/NativeSourceExecutionTestSupport.h"
#include "frontend/AST/as_type_context.h"
#include <atomic>
#include <thread>
#include <vector>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(EngineRegistration, "Angelscript.UnitTest.NativeEngine.Registration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FEngineOwner
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
		void Reset() { if (Ptr) Ptr->ShutDownAndRelease(); Ptr = nullptr; }
	};
	struct FDefinition
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context;
		TUniquePtr<asCDefinitions> Set;
		asCObjectType* Type = nullptr;
		asCScriptFunction* Function = nullptr;
		bool Initialized = false;
		FDefinition(const char* Name, const char* Namespace = "", const char* Scope = "RegistrationTests", bool Freeze = true)
			: Context(MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>()), Set(asCDefinitions::Create())
		{
			auto& Registry = Context->GetIdentityRegistry();
			asSNominalTypeDescriptor Descriptor;
			Descriptor.StableScope = UTF8_TO_TCHAR(Scope);
			Descriptor.Name = UTF8_TO_TCHAR(Name);
			Descriptor.Kind = asETypeDeclKind::Struct;
			FString(UTF8_TO_TCHAR(Namespace)).ParseIntoArray(Descriptor.NamespacePath, TEXT("::"), true);
			asSStableKey TypeKey;
			if (!(Registry.InternNominalType(Descriptor, TypeKey) == asETypeIdentityStatus::Succeeded)) return;
			if (!(Set->SetIdentityContext(Context) == asEDefinitionResult::Succeeded)) return;
			if (!(Set->CreateObjectType(TypeKey, Name, Namespace, asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type) == asEDefinitionResult::Succeeded)) return;
			asSFunctionIdentityDescriptor Method;
			Method.StableScope = Descriptor.StableScope;
			Method.NamespacePath = Descriptor.NamespacePath;
			Method.Owner = TypeKey;
			Method.Kind = asEFunctionIdentityKind::Method;
			Method.Name = TEXT("Read");
			if (!(Context->InternPrimitiveTypeUse(asEPrimitiveType::Int32, Method.ReturnType) == asETypeIdentityStatus::Succeeded)) return;
			asSStableKey FunctionKey;
			if (!(Registry.InternFunction(Method, FunctionKey) == asETypeIdentityStatus::Succeeded)) return;
			if (!(Set->CreateFunction(FunctionKey, "Read", Namespace, asFUNC_SCRIPT, Function) == asEDefinitionResult::Succeeded)) return;
			if (!(Set->DefineFunction(*Function, Set->Primitive(ttInt), {}, Type) == asEDefinitionResult::Succeeded)) return;
			if (Freeze)
			{
				if (!(Set->FinalizeLayouts() == asEDefinitionResult::Succeeded)) return;
				if (!(Set->Freeze() == asEDefinitionResult::Succeeded)) return;
			}
			Initialized = true;
		}
	};
	static bool Success(asEDefinitionResult Result) { return Result == asEDefinitionResult::Succeeded; }
	static bool Registered(asERegistrationResult Result) { return Result == asERegistrationResult::Succeeded; }

public:
	TEST_METHOD(ConstructorCreatesTypedBuiltinsWithoutLegacyRegistration)
	{
		FEngineOwner Engine;
		auto* Object = static_cast<asCTypeInfo*>(Engine.Ptr->GetTypeInfoByName("$obj"));
		auto* Function = static_cast<asCTypeInfo*>(Engine.Ptr->GetTypeInfoByName("$func"));
		ASSERT_THAT(IsNotNull(Object));
		ASSERT_THAT(IsNotNull(Function));
		ASSERT_THAT(IsNotNull(Object->GetDefinitions()));
		ASSERT_THAT(IsTrue(Object->GetDefinitions() == Function->GetDefinitions()));
		ASSERT_THAT(IsTrue(Object->GetDefinitions()->GetState() == asEDefinitionState::Attached));
		ASSERT_THAT(IsFalse(Object->GetStableKey().IsZero()));
		ASSERT_THAT(AreEqual(0, static_cast<int>(Object->GetMethodCount())));
		asEBehaviours Behaviour = asBEHAVE_MAX;
		auto* Constructor = static_cast<asCScriptFunction*>(Object->GetBehaviourByIndex(0, &Behaviour));
		ASSERT_THAT(IsNotNull(Constructor));
		ASSERT_THAT(IsTrue(Behaviour == asBEHAVE_CONSTRUCT));
		ASSERT_THAT(IsTrue(Constructor->GetDeclarationKind() == AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Constructor));
		ASSERT_THAT(IsTrue(Engine.Ptr->FindFunction(Constructor->GetStableKey()) == Constructor));
		ASSERT_THAT(IsTrue(Constructor->objectType == Object));
		ASSERT_THAT(IsFalse(Engine.Ptr->configFailed));
	}

	TEST_METHOD(RegistersOriginalPointersAndKeepsSemanticNamespace)
	{
		FDefinition Definition("Counter", "Gameplay");
		ASSERT_THAT(IsTrue(Definition.Initialized));
		const auto* OriginalNamespace = Definition.Type->nameSpace;
		const auto TypeKey = Definition.Type->GetStableKey();
		FEngineOwner Engine;
		ASSERT_THAT(IsTrue(Registered(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)))));
		ASSERT_THAT(IsTrue(Engine.Ptr->FindType("Gameplay", "Counter") == Definition.Type));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetTypeInfoById(Definition.Type->GetTypeId()) == Definition.Type));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetFunctionById(Definition.Function->GetId()) == Definition.Function));
		ASSERT_THAT(IsTrue(Definition.Type->GetEngine() == Engine.Ptr));
		ASSERT_THAT(IsTrue(Definition.Function->GetEngine() == Engine.Ptr));
		ASSERT_THAT(IsTrue(Definition.Type->nameSpace == OriginalNamespace));
		ASSERT_THAT(IsTrue(Definition.Type->GetStableKey() == TypeKey));
		ASSERT_THAT(IsTrue(Definition.Type->GetTypeId() > asTYPEID_LAST_PRIMITIVE));
		ASSERT_THAT(IsTrue(Definition.Type->engine == Engine.Ptr));
	}

	TEST_METHOD(NumericQueriesNeverAttachADetachedOrForeignType)
	{
		FDefinition Definition("Detached");
		ASSERT_THAT(IsTrue(Definition.Initialized));
		FEngineOwner Engine;
		FEngineOwner Other;
		const auto Before = Engine.Ptr->GetRegistrationCounts();
		ASSERT_THAT(IsTrue(Engine.Ptr->GetTypeIdFromDataType(asCDataType::CreateType(Definition.Type, false)) < 0));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetRegistrationCounts() == Before));
		ASSERT_THAT(IsTrue(Registered(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)))));
		ASSERT_THAT(IsTrue(Other.Ptr->GetTypeIdFromDataType(asCDataType::CreateType(Definition.Type, false)) < 0));
	}

	TEST_METHOD(DuplicateAndSecondEngineRegistrationAreExplicit)
	{
		FDefinition Definition("Owned");
		ASSERT_THAT(IsTrue(Definition.Initialized));
		FEngineOwner First;
		FEngineOwner Second;
		ASSERT_THAT(IsTrue(Registered(First.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)))));
		ASSERT_THAT(IsTrue(First.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)) == asERegistrationResult::InvalidArgument));
		ASSERT_THAT(IsTrue(Second.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)) == asERegistrationResult::InvalidArgument));
		ASSERT_THAT(IsTrue(First.Ptr->FindType("", "Owned") == Definition.Type));
		ASSERT_THAT(IsNull(Second.Ptr->FindType("", "Owned")));
	}

	TEST_METHOD(SameIdentityDoesNotMergeDistinctDefinitions)
	{
		FDefinition First("Same");
		ASSERT_THAT(IsTrue(First.Initialized));
		FDefinition Second("Same");
		ASSERT_THAT(IsTrue(Second.Initialized));
		FEngineOwner Engine;
		ASSERT_THAT(IsTrue(Registered(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(First.Set)))));
		const auto Before = Engine.Ptr->GetRegistrationCounts();
		ASSERT_THAT(IsTrue(Second.Type->GetTypeId() < 0));
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Second.Set)) == asERegistrationResult::IdentityConflict));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetRegistrationCounts() == Before));
		ASSERT_THAT(IsNull(Second.Set.Get()));
	}

	TEST_METHOD(SameNameDifferentScopeConflictsButNamespacesDoNot)
	{
		FDefinition First("Same", "A", "FirstModule");
		ASSERT_THAT(IsTrue(First.Initialized));
		FDefinition Conflict("Same", "A", "SecondModule");
		ASSERT_THAT(IsTrue(Conflict.Initialized));
		FDefinition OtherNamespace("Same", "B", "SecondModule");
		ASSERT_THAT(IsTrue(OtherNamespace.Initialized));
		FEngineOwner Engine;
		ASSERT_THAT(IsTrue(Registered(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(First.Set)))));
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Conflict.Set)) == asERegistrationResult::NameConflict));
		ASSERT_THAT(IsTrue(Registered(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(OtherNamespace.Set)))));
		ASSERT_THAT(IsTrue(Engine.Ptr->FindType("A", "Same") == First.Type));
		ASSERT_THAT(IsTrue(Engine.Ptr->FindType("B", "Same") == OtherNamespace.Type));
	}

	TEST_METHOD(MissingOrForgedIdentityContextCannotPublish)
	{
		FDefinition Definition("Authenticated");
		ASSERT_THAT(IsTrue(Definition.Initialized));
		auto Forged = asCDefinitions::Create();
		asCObjectType* Type = nullptr;
		ASSERT_THAT(IsTrue(Success(Forged->CreateObjectType(Definition.Type->GetStableKey(), "DifferentName", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type))));
		ASSERT_THAT(IsTrue(Success(Forged->SetIdentityContext(Definition.Context))));
		ASSERT_THAT(IsTrue(Success(Forged->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Forged->Freeze())));
		FEngineOwner Engine;
		const auto Before = Engine.Ptr->GetRegistrationCounts();
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Forged)) == asERegistrationResult::InvalidIdentity));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetRegistrationCounts() == Before));
		ASSERT_THAT(IsNull(Forged.Get()));
	}

	TEST_METHOD(PostFreezeLayoutCorruptionFailsWithoutPublication)
	{
		FDefinition Definition("Corrupted");
		ASSERT_THAT(IsTrue(Definition.Initialized));
		Definition.Type->size = 123;
		FEngineOwner Engine;
		const auto Before = Engine.Ptr->GetRegistrationCounts();
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)) == asERegistrationResult::InvalidLayout));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetRegistrationCounts() == Before));
		ASSERT_THAT(IsNull(Definition.Set.Get()));
	}

	TEST_METHOD(DependencyFailurePublishesNoneOfTheClosure)
	{
		FDefinition Dependency("Dependency");
		ASSERT_THAT(IsTrue(Dependency.Initialized));
		auto Root = asCDefinitions::Create();
		ASSERT_THAT(IsTrue(Success(Root->AddDependency(Dependency.Set.Get()))));
		ASSERT_THAT(IsTrue(Success(Root->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Root->Freeze())));
		FEngineOwner Engine;
		const auto Before = Engine.Ptr->GetRegistrationCounts();
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Root)) == asERegistrationResult::InvalidState));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetRegistrationCounts() == Before));
		ASSERT_THAT(IsNull(Dependency.Type->GetEngine()));
		ASSERT_THAT(IsTrue(Dependency.Set->GetState() == asEDefinitionState::Frozen));
	}

	TEST_METHOD(TypeReferenceSurvivesEngineAndCannotBeReattached)
	{
		FDefinition Definition("Retained", "RetiredNamespace");
		ASSERT_THAT(IsTrue(Definition.Initialized));
		FEngineOwner Engine;
		asCObjectType* Type = Definition.Type;
		ASSERT_THAT(IsTrue(Registered(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)))));
		ASSERT_THAT(IsTrue(Type->GetEngine() == Engine.Ptr));
		Engine.Reset();
		FEngineOwner Other;
		ASSERT_THAT(IsTrue(Other.Ptr != nullptr));
	}

	TEST_METHOD(MethodOnlyReferenceRetainsWholeGraphAfterEngineDestruction)
	{
		FDefinition Definition("Owner", "Api");
		ASSERT_THAT(IsTrue(Definition.Initialized));
		FEngineOwner Engine;
		asCScriptFunction* Function = Definition.Function;
		ASSERT_THAT(IsTrue(Registered(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)))));
		ASSERT_THAT(IsTrue(Function->GetObjectType() == Definition.Type));
		ASSERT_THAT(IsTrue(Function->GetEngine() == Engine.Ptr));
		Engine.Reset();
	}

	TEST_METHOD(CompetingEnginesHaveExactlyOneWinner)
	{
		FDefinition Definition("Raced");
		ASSERT_THAT(IsTrue(Definition.Initialized));
		FEngineOwner First;
		FEngineOwner Second;
		ASSERT_THAT(IsTrue(Registered(First.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)))));
		ASSERT_THAT(IsTrue(Second.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)) == asERegistrationResult::InvalidArgument));
	}

	TEST_METHOD(LegacyMutationAndDeclarationParsingAreExplicitlyDormant)
	{
		FDefinition Definition("Frozen");
		ASSERT_THAT(IsTrue(Definition.Initialized));
		FEngineOwner Engine;
		ASSERT_THAT(IsTrue(Registered(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)))));
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterObjectType("Legacy", 4, asOBJ_VALUE | asOBJ_POD) >= 0));
		ASSERT_THAT(IsNotNull(Engine.Ptr->GetTypeInfoByName("Legacy")));
		ASSERT_THAT(AreEqual(asNOT_SUPPORTED, Engine.Ptr->RegisterObjectProperty("Frozen", "int Field", 0)));
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterEnum("LegacyEnum") >= 0));
		ASSERT_THAT(IsNotNull(Engine.Ptr->GetTypeInfoByName("LegacyEnum")));
		ASSERT_THAT(IsFalse([]<class T>() { return requires(T& Value) { Value.RegisterFuncdef("void LegacyCallback()"); }; }.operator()<asCScriptEngine>()));
		ASSERT_THAT(IsNull(Engine.Ptr->GetTypeInfoByDecl("array<Frozen@>")));
		ASSERT_THAT(IsNull(Engine.Ptr->GetGlobalFunctionByDecl("void Legacy()")));
		asIScriptContext* Created = Engine.Ptr->CreateContext();
		ASSERT_THAT(IsNotNull(Created));
		Created->Release();
		asIScriptContext* Context = nullptr;
		ASSERT_THAT(AreEqual(0, Engine.Ptr->CreateContext(&Context, true)));
		ASSERT_THAT(IsNotNull(Context));
		Context->Release();
		ASSERT_THAT(IsTrue([]<class T>(T&) { return requires(T& Value) { Value.GetModule("Dormant"); }; }(*Engine.Ptr)));
		ASSERT_THAT(IsNull(Engine.Ptr->GetModule("Dormant")));
		ASSERT_THAT(AreEqual(0, static_cast<int>(Definition.Type->GetPropertyCount())));
	}

	TEST_METHOD(DependencyClosurePublishesTogetherAndReusesSameEngineDependency)
	{
		FDefinition Dependency("SharedDependency");
		ASSERT_THAT(IsTrue(Dependency.Initialized));
		FDefinition First("FirstRoot", "", "RegistrationTests", false);
		ASSERT_THAT(IsTrue(First.Initialized));
		FDefinition Second("SecondRoot", "", "RegistrationTests", false);
		ASSERT_THAT(IsTrue(Second.Initialized));
		ASSERT_THAT(IsTrue(Success(First.Set->AddDependency(Dependency.Set.Get()))));
		ASSERT_THAT(IsTrue(Success(Second.Set->AddDependency(Dependency.Set.Get()))));
		ASSERT_THAT(IsTrue(Success(First.Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(First.Set->Freeze())));
		ASSERT_THAT(IsTrue(Success(Second.Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Second.Set->Freeze())));
		FEngineOwner Engine;
		const auto Before = Engine.Ptr->GetRegistrationCounts();
		ASSERT_THAT(IsTrue(Registered(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Dependency.Set)))));
		ASSERT_THAT(IsTrue(Registered(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(First.Set)))));
		ASSERT_THAT(AreEqual(Before.Key + 2, Engine.Ptr->GetRegistrationCounts().Key));
		const int DependencyId = Dependency.Type->GetTypeId();
		ASSERT_THAT(IsTrue(Registered(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Second.Set)))));
		ASSERT_THAT(AreEqual(DependencyId, Dependency.Type->GetTypeId()));
		ASSERT_THAT(AreEqual(Before.Key + 3, Engine.Ptr->GetRegistrationCounts().Key));
	}

	TEST_METHOD(ForeignEngineDependencyRejectsTheUnattachedRoot)
	{
		FDefinition Dependency("ForeignDependency");
		ASSERT_THAT(IsTrue(Dependency.Initialized));
		FDefinition Root("Root", "", "RegistrationTests", false);
		ASSERT_THAT(IsTrue(Root.Initialized));
		ASSERT_THAT(IsTrue(Success(Root.Set->AddDependency(Dependency.Set.Get()))));
		ASSERT_THAT(IsTrue(Success(Root.Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Root.Set->Freeze())));
		FEngineOwner First;
		FEngineOwner Second;
		ASSERT_THAT(IsTrue(Registered(First.Ptr->RegisterExternalDefinitions(MoveTemp(Dependency.Set)))));
		const auto Before = Second.Ptr->GetRegistrationCounts();
		ASSERT_THAT(IsNull(Root.Type->GetEngine()));
		ASSERT_THAT(IsTrue(Second.Ptr->RegisterExternalDefinitions(MoveTemp(Root.Set)) == asERegistrationResult::InvalidState));
		ASSERT_THAT(IsTrue(Second.Ptr->GetRegistrationCounts() == Before));
		ASSERT_THAT(IsNull(Root.Set.Get()));
	}

	TEST_METHOD(ConcurrentQueriesObserveWholeClosureCounts)
	{
		FDefinition Root("ObservedRoot", "", "RegistrationTests", false);
		ASSERT_THAT(IsTrue(Root.Initialized));
		std::vector<std::unique_ptr<FDefinition>> Dependencies;
		for (int Index = 0; Index < 24; ++Index)
		{
			asCString Name; Name.Format("ObservedDependency%d", Index);
			Dependencies.emplace_back(new FDefinition(Name.AddressOf()));
			ASSERT_THAT(IsTrue(Dependencies.back()->Initialized));
			ASSERT_THAT(IsTrue(Success(Root.Set->AddDependency(Dependencies.back()->Set.Get()))));
		}
		ASSERT_THAT(IsTrue(Success(Root.Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Root.Set->Freeze())));
		FEngineOwner Engine;
		const auto Empty = Engine.Ptr->GetRegistrationCounts();
		for (std::unique_ptr<FDefinition>& Dependency : Dependencies)
		{
			ASSERT_THAT(IsTrue(Registered(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Dependency->Set)))));
		}
		const auto Before = Engine.Ptr->GetRegistrationCounts();
		ASSERT_THAT(AreEqual(Empty.Key + 24, Before.Key));
		std::atomic<bool> Start{false}, Finished{false}, InvalidObservation{false};
		std::atomic<int> Reads{0};
		std::thread Reader([&]
		{
			while (!Start.load()) std::this_thread::yield();
			do
			{
				const auto Counts = Engine.Ptr->GetRegistrationCounts();
				const bool Old = Counts == Before;
				const bool New = Counts.Key == Before.Key + 1 && Counts.Value == Before.Value + 1;
				if (!Old && !New) InvalidObservation.store(true);
				Reads.fetch_add(1);
			} while (!Finished.load());
		});
		Start.store(true);
		while (Reads.load() == 0) std::this_thread::yield();
		const auto Result = Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Root.Set));
		Finished.store(true);
		Reader.join();
		ASSERT_THAT(IsTrue(Registered(Result)));
		ASSERT_THAT(IsFalse(InvalidObservation.load()));
		ASSERT_THAT(IsTrue(Reads.load() > 0));
	}

	TEST_METHOD(MalformedSignatureArraysAndReturnTypeFailClosed)
	{
		FDefinition Malformed("Malformed");
		ASSERT_THAT(IsTrue(Malformed.Initialized));
		Malformed.Function->parameterTypes.PushLast(Malformed.Set->Primitive(ttInt));
		FDefinition WrongReturn("WrongReturn");
		ASSERT_THAT(IsTrue(WrongReturn.Initialized));
		WrongReturn.Function->returnType = WrongReturn.Set->Primitive(ttBool);
		FEngineOwner Engine;
		const auto Before = Engine.Ptr->GetRegistrationCounts();
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Malformed.Set)) == asERegistrationResult::InvalidIdentity));
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(WrongReturn.Set)) == asERegistrationResult::InvalidIdentity));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetRegistrationCounts() == Before));
	}

	TEST_METHOD(GenericArityMustMatchActualDefinitionFacts)
	{
		FDefinition Definition("Generic", "", "RegistrationTests", false);
		ASSERT_THAT(IsTrue(Definition.Initialized));
		ASSERT_THAT(IsTrue(Success(Definition.Set->SetTypeDeclarationFacts(*Definition.Type, AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct, 1))));
		ASSERT_THAT(IsTrue(Success(Definition.Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Definition.Set->Freeze())));
		FEngineOwner Engine;
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)) == asERegistrationResult::InvalidIdentity));
	}

	TEST_METHOD(EnumAndAliasIdsNeverShadowPrimitiveIds)
	{
auto Context = MakeShared<asCTypeContext, ESPMode::ThreadSafe>();
		auto Set = asCDefinitions::Create();
		ASSERT_THAT(IsTrue(Success(Set->SetIdentityContext(Context))));
		asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = TEXT("RegistrationTests");
		Descriptor.Kind = asETypeDeclKind::Enum;
		Descriptor.Name = TEXT("Mode");
		asSStableKey EnumKey, AliasKey;
		ASSERT_THAT(IsTrue(Context->GetIdentityRegistry().InternNominalType(Descriptor, EnumKey) == asETypeIdentityStatus::Succeeded));
		Descriptor.Kind = asETypeDeclKind::Typedef;
		Descriptor.Name = TEXT("Count");
		ASSERT_THAT(IsTrue(Context->GetIdentityRegistry().InternNominalType(Descriptor, AliasKey) == asETypeIdentityStatus::Succeeded));
		asCEnumType* Enum = nullptr;
		asCTypedefType* Alias = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateEnumType(EnumKey, "Mode", "", Enum))));
		ASSERT_THAT(IsTrue(Success(Set->AddEnumValue(*Enum, "Enabled", 1))));
		ASSERT_THAT(IsTrue(Success(Set->CreateTypedefType(AliasKey, "Count", "", Alias))));
		ASSERT_THAT(IsTrue(Success(Set->SetAlias(*Alias, Set->Primitive(ttInt)))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
		FEngineOwner Engine;
		ASSERT_THAT(IsTrue(Registered(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Set)))));
		ASSERT_THAT(IsTrue(Enum->GetTypeId() > asTYPEID_FLOAT64));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetTypeInfoById(Enum->GetTypeId()) == Enum));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetTypeInfoById(Alias->GetTypeId()) == Alias));
		ASSERT_THAT(AreEqual(asTYPEID_INT32, Alias->GetTypedefTypeId()));
		for (int Id = asTYPEID_VOID; Id <= asTYPEID_FLOAT64; ++Id)
			ASSERT_THAT(IsNull(Engine.Ptr->GetTypeInfoById(Id)));
		ASSERT_THAT(AreEqual(ttInt, Engine.Ptr->GetDataTypeFromTypeId(asTYPEID_INT32).GetTokenType()));
	}

	TEST_METHOD(ImmutableFloatProfileMismatchCannotRegister)
	{
asSTypeContextOptions ContextOptions;
		ContextOptions.FloatIsFloat64 = false;
		auto Context = MakeShared<asCTypeContext, ESPMode::ThreadSafe>(ContextOptions);
		asSDefinitionOptions ImageOptions;
		ImageOptions.FloatIsFloat64 = false;
		auto Set = asCDefinitions::Create(ImageOptions);
		ASSERT_THAT(IsTrue(Success(Set->SetIdentityContext(Context))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
		FEngineOwner Engine;
		const auto Before = Engine.Ptr->GetRegistrationCounts();
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Set)) == asERegistrationResult::InvalidLayout));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetRegistrationCounts() == Before));
		ASSERT_THAT(IsNull(Set.Get()));
		ASSERT_THAT(AreEqual(asNOT_SUPPORTED, Engine.Ptr->SetEngineProperty(asEP_FLOAT_IS_FLOAT64, 0)));
	}

	TEST_METHOD(ConcurrentRegistrationInOneEnginePublishesExactlyOnce)
	{
		FDefinition Definition("OneEngine");
		ASSERT_THAT(IsTrue(Definition.Initialized));
		FEngineOwner Engine;
		const auto Before = Engine.Ptr->GetRegistrationCounts();
		std::atomic<bool> Start{false};
		asERegistrationResult Results[2]{};
		std::thread First([&] { while (!Start.load()) {} Results[0] = Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)); });
		std::thread Second([&] { while (!Start.load()) {} Results[1] = Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)); });
		Start.store(true);
		First.join();
		Second.join();
		ASSERT_THAT(IsTrue((Registered(Results[0]) && (Results[1] == asERegistrationResult::AlreadyRegistered
			|| Results[1] == asERegistrationResult::InvalidArgument)) ||
			(Registered(Results[1]) && (Results[0] == asERegistrationResult::AlreadyRegistered
			|| Results[0] == asERegistrationResult::InvalidArgument))));
		const auto After = Engine.Ptr->GetRegistrationCounts();
		ASSERT_THAT(AreEqual(Before.Key + 1, After.Key));
		ASSERT_THAT(AreEqual(Before.Value + 1, After.Value));
	}

	TEST_METHOD(UnknownParameterModeCannotMasqueradeAsValue)
	{
auto Context = MakeShared<asCTypeContext, ESPMode::ThreadSafe>();
		auto Set = asCDefinitions::Create();
		ASSERT_THAT(IsTrue(Success(Set->SetIdentityContext(Context))));
		asSFunctionIdentityDescriptor Descriptor;
		Descriptor.StableScope = TEXT("RegistrationTests");
		Descriptor.Name = TEXT("Accept");
		Descriptor.Kind = asEFunctionIdentityKind::Global;
		ASSERT_THAT(IsTrue(Context->InternPrimitiveTypeUse(asEPrimitiveType::Int32, Descriptor.ReturnType) == asETypeIdentityStatus::Succeeded));
		Descriptor.Parameters.Add({Descriptor.ReturnType, asEParameterPassingMode::Value});
		asSStableKey Key;
		ASSERT_THAT(IsTrue(Context->GetIdentityRegistry().InternFunction(Descriptor, Key) == asETypeIdentityStatus::Succeeded));
		asCScriptFunction* Function = nullptr;
		ASSERT_THAT(IsTrue(Success(Set->CreateFunction(Key, "Accept", "", asFUNC_SCRIPT, Function))));
		asSDefinitionParameter Parameter;
		Parameter.Name = "Value";
		Parameter.Type = Set->Primitive(ttInt);
		ASSERT_THAT(IsTrue(Success(Set->DefineFunction(*Function, Set->Primitive(ttInt), MakeArrayView(&Parameter, 1)))));
		ASSERT_THAT(IsTrue(Success(Set->FinalizeLayouts())));
		ASSERT_THAT(IsTrue(Success(Set->Freeze())));
		Function->inOutFlags[0] = static_cast<asETypeModifiers>(0x80);
		FEngineOwner Engine;
		const auto Before = Engine.Ptr->GetRegistrationCounts();
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Set)) == asERegistrationResult::InvalidIdentity));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetRegistrationCounts() == Before));
	}

	TEST_METHOD(RetirementRemovesAllVisibilityAndReleasesEngineLeases)
	{
		FDefinition Definition("ExplicitRetirement");
		ASSERT_THAT(IsTrue(Definition.Initialized));
		FEngineOwner Engine;
		ASSERT_THAT(IsTrue(Registered(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(Definition.Set)))));
		const int TypeId = Definition.Type->GetTypeId();
		const int FunctionId = Definition.Function->GetId();
		Engine.Ptr->RetireExternalDefinitions();
		ASSERT_THAT(AreEqual(asUINT(0), Engine.Ptr->GetRegistrationCounts().Key));
		ASSERT_THAT(AreEqual(asUINT(0), Engine.Ptr->GetRegistrationCounts().Value));
		ASSERT_THAT(IsNull(Engine.Ptr->GetTypeInfoById(TypeId)));
		ASSERT_THAT(IsNull(Engine.Ptr->GetFunctionById(FunctionId)));
		ASSERT_THAT(IsNull(Engine.Ptr->FindType("", "ExplicitRetirement")));
		ASSERT_THAT(IsNull(Definition.Type->GetEngine()));
		ASSERT_THAT(AreEqual(asTYPEID_INT32, Definition.Function->GetReturnTypeId()));
		FDefinition AfterRetirement("AfterRetirement");
		ASSERT_THAT(IsTrue(AfterRetirement.Initialized));
		ASSERT_THAT(IsTrue(Engine.Ptr->RegisterExternalDefinitions(MoveTemp(AfterRetirement.Set)) == asERegistrationResult::Retired));
	}
};

#endif
