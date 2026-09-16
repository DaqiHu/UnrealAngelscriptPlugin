#include "CQTest.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptEngine.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FHostObjectsParent
	{
		int32 X = 7;
	};

	struct FHostObjectsChild : FHostObjectsParent
	{
		int32 Y = 11;
	};

	void HostObjectsRegisterTypes(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FHostObjectsParent>("P", Flags);
		Binds.ValueClassForTarget<FHostObjectsChild>("C", Flags);
	}

	void HostObjectsRegisterMembers(FAngelscriptBinds& Binds)
	{
		Binds.ExistingClassForTarget("P").Property("int X", &FHostObjectsParent::X);
		FAngelscriptBinds Child = Binds.ExistingClassForTarget("C");
		Child.SetHostBaseTypeForTarget("P");
		Child.Property("int Y", &FHostObjectsChild::Y);
	}

	void HostObjectsRegisterMissing(FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionDirectForTarget("void Missing()", asSFuncPtr(), asCALL_CDECL, ASAutoCaller::FunctionCaller::Make());
	}

	FAngelscriptBindRecord HostObjectsMakeRecord(FName BindName, EAngelscriptBindPhase Phase, FAngelscriptBindCallback Callback, int32 Line)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("AngelscriptTest");
		Record.BindName = BindName;
		Record.Phase = Phase;
		Record.SourceFile = "RuntimeBindingHostObjectsTests.cpp";
		Record.SourceLine = Line;
		Record.Callback = Callback;
		return Record;
	}

	struct FHostObjectsEngineOwner
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FHostObjectsEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
	};

	asIScriptFunction* HostObjectsFindNamed(asITypeInfo* Type, const char* Name)
	{
		if (!Type || !Name) return nullptr;
		for (asUINT Index = 0; Index < Type->GetMethodCount(); ++Index)
		{
			asIScriptFunction* Method = Type->GetMethodByIndex(Index);
			if (Method && Method->GetName() && FCStringAnsi::Strcmp(Method->GetName(), Name) == 0)
			{
				return Method;
			}
		}
		return nullptr;
	}

	struct FHostObjectsTransientWorld
	{
		UWorld* World = nullptr;
		FHostObjectsTransientWorld()
		{
			const FName Name(*FString::Printf(TEXT("HostObjectsWorld_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			World = UWorld::CreateWorld(EWorldType::Game, false, Name, GetTransientPackage());
			if (World && GEngine)
			{
				GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			}
		}
		~FHostObjectsTransientWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) GEngine->DestroyWorldContext(World);
			}
		}
	};

	int32 HostObjectsCountProperty(asCObjectType* Type, const char* Name)
	{
		int32 Count = 0;
		if (!Type || !Name) return 0;
		for (asUINT Index = 0; Index < Type->GetPropertyCount(); ++Index)
		{
			const char* PropertyName = nullptr;
			Type->GetProperty(Index, &PropertyName, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
			if (PropertyName && FCStringAnsi::Strcmp(PropertyName, Name) == 0) ++Count;
		}
		return Count;
	}

	struct FHostObjectsSite
	{
		const TCHAR* Source;
		const TCHAR* Symbol;
		const TCHAR* Evidence;
		const TCHAR* Reason;
	};

	const FHostObjectsSite GHostObjectsSites[] = {
#include "RuntimeBindingHostObjectsSites.inl"
	};

	bool HostObjectsAppend(UE::Angelscript::Private::FAngelscriptBindCollection& Collection, FString& Diagnostic)
	{
		return Collection.Append(HostObjectsMakeRecord(TEXT("HostObjects.Types"), EAngelscriptBindPhase::TypeDeclarations, &HostObjectsRegisterTypes, 10), Diagnostic)
			&& Collection.Append(HostObjectsMakeRecord(TEXT("HostObjects.Members"), EAngelscriptBindPhase::ExplicitBindings, &HostObjectsRegisterMembers, 20), Diagnostic);
	}
}

TEST_CLASS_WITH_FLAGS(HostObjects, "Angelscript.UnitTest.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ObjectsBoundBehavior)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostObjectsAppend(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), *Diagnostic));
		asCObjectType* Parent = static_cast<asCObjectType*>(Collection.GetHostDefinitions()->FindType("P"));
		asCObjectType* Child = static_cast<asCObjectType*>(Collection.GetHostDefinitions()->FindType("C"));
		ASSERT_THAT(IsNotNull(Parent));
		ASSERT_THAT(IsNotNull(Child));
		ASSERT_THAT(IsTrue(Child->derivedFrom == Parent));
		ASSERT_THAT(AreEqual(1, HostObjectsCountProperty(Child, "X")));
		ASSERT_THAT(AreEqual(1, HostObjectsCountProperty(Child, "Y")));
		FHostObjectsChild Value;
		ASSERT_THAT(AreEqual(7, Value.X));
		ASSERT_THAT(AreEqual(11, Value.Y));
		FHostObjectsEngineOwner A;
		FHostObjectsEngineOwner B;
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("HostObjects.Members"))));
	}

	TEST_METHOD(ActorOrWorldHostCall)
	{
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		TUniquePtr<FAngelscriptEngine> Owner = FAngelscriptEngine::CreateForBindings(BindingDiagnostic);
		ASSERT_THAT(IsNotNull(
			Owner.Get(),
			BindingDiagnostic.Message.IsEmpty()
				? TEXT("<empty CreateForBindings diagnostic>")
				: *BindingDiagnostic.Message));
		ASSERT_THAT(IsNotNull(Owner->Engine));
		FHostObjectsTransientWorld W;
		ASSERT_THAT(IsNotNull(W.World));
		AActor* Actor = W.World->SpawnActor<AActor>();
		ASSERT_THAT(IsNotNull(Actor));
		USceneComponent* Component = NewObject<USceneComponent>(Actor);
		Actor->SetRootComponent(Component);
		Component->RegisterComponent();
		asIScriptFunction* Set = HostObjectsFindNamed(Owner->Engine->GetTypeInfoByName("USceneComponent"), "SetRelativeLocation");
		ASSERT_THAT(IsNotNull(Set));
		asIScriptContext* Context = Owner->Engine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		FVector Translation(11.0, 22.0, 33.0);
		ASSERT_THAT(AreEqual(0, Context->Prepare(Set)));
		ASSERT_THAT(AreEqual(0, Context->SetObject(Component)));
		ASSERT_THAT(AreEqual(0, Context->SetArgObject(0, &Translation)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), Context->Execute()));
		ASSERT_THAT(IsTrue(Component->GetComponentLocation() == Translation));
		Context->Release();
		Actor->Destroy();
	}

	TEST_METHOD(CompleteFamilyAccounting)
	{
		ASSERT_THAT(AreEqual(49, static_cast<int32>(UE_ARRAY_COUNT(GHostObjectsSites))));
		int32 Executable = 0, MetadataOnly = 0, Companion = 0;
		for (const FHostObjectsSite& Site : GHostObjectsSites)
		{
			ASSERT_THAT(IsTrue(Site.Reason && *Site.Reason));
			if (FCString::Strcmp(Site.Evidence, TEXT("executable")) == 0) ++Executable;
			else if (FCString::Strcmp(Site.Evidence, TEXT("metadata_only")) == 0) ++MetadataOnly;
			else
			{
				ASSERT_THAT(AreEqual(FString(TEXT("companion_no_output")), FString(Site.Evidence)));
				++Companion;
			}
		}
		ASSERT_THAT(AreEqual(0, Executable));
		ASSERT_THAT(AreEqual(40, MetadataOnly));
		ASSERT_THAT(AreEqual(9, Companion));
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostObjectsAppend(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), *Diagnostic));
		const int32 Before = Collection.GetCallbackInvocationCount(TEXT("HostObjects.Types"));
		FHostObjectsEngineOwner A;
		FHostObjectsEngineOwner B;
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(AreEqual(Before, Collection.GetCallbackInvocationCount(TEXT("HostObjects.Types"))));
	}

	TEST_METHOD(OwnerAndFailureBoundaries)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Healthy;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostObjectsAppend(Healthy, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.ExecuteToHost(Diagnostic), *Diagnostic));
		asCObjectType* Child = static_cast<asCObjectType*>(Healthy.GetHostDefinitions()->FindType("C"));
		ASSERT_THAT(AreEqual(1, HostObjectsCountProperty(Child, "X")));
		UE::Angelscript::Private::FAngelscriptBindCollection Failed;
		ASSERT_THAT(IsTrue(Failed.Append(HostObjectsMakeRecord(TEXT("HostObjects.Missing"), EAngelscriptBindPhase::ExplicitBindings, &HostObjectsRegisterMissing, 30), Diagnostic)));
		ASSERT_THAT(IsTrue(Failed.Finalize(Diagnostic)));
		ASSERT_THAT(IsFalse(Failed.ExecuteToHost(Diagnostic)));
		ASSERT_THAT(AreEqual(1, HostObjectsCountProperty(Child, "Y")));
	}
};

#endif
