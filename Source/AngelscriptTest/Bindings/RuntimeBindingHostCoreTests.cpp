#include "CQTest.h"
#include "Core/AngelscriptBindsInternal.h"
#include "as_callfunc.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GHostCoreCounter = 7;
	int32 GHostCoreCounterCallbacks = 0;

	void HostCoreRegisterCounter(FAngelscriptBinds& Binds)
	{
		++GHostCoreCounterCallbacks;
		FAngelscriptBinds::FNamespace Namespace(Binds, "N");
		Binds.BindGlobalVariableForTarget("int Counter", &GHostCoreCounter);
	}

	void HostCoreRegisterMissing(FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionDirectForTarget("void Missing()", asSFuncPtr(), asCALL_CDECL, ASAutoCaller::FunctionCaller::Make());
	}

	FString HostCoreJoinThisAndOther(FString* Self, const FString& Other, const FString& Separator)
	{
		TArray<FString> Parts;
		if (Self)
		{
			Parts.Add(*Self);
		}
		Parts.Add(Other);
		return FString::Join(Parts, *Separator);
	}

	void HostCoreRegisterJoin(FAngelscriptBinds& Binds)
	{
		Binds.ExistingClassForTarget("FString").Method(
			"FString Join(const FString& Other, const FString& Separator)",
			&HostCoreJoinThisAndOther);
	}

	FAngelscriptBindRecord HostCoreMakeRecord(FName BindName, EAngelscriptBindPhase Phase, FAngelscriptBindCallback Callback, int32 Line)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("AngelscriptTest");
		Record.BindName = BindName;
		Record.Phase = Phase;
		Record.SourceFile = "RuntimeBindingHostCoreTests.cpp";
		Record.SourceLine = Line;
		Record.Callback = Callback;
		return Record;
	}

	struct FHostCoreEngineOwner
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FHostCoreEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
	};

	struct FHostCoreSite
	{
		const TCHAR* Source;
		const TCHAR* Symbol;
		const TCHAR* Evidence;
		const TCHAR* Reason;
	};

	const FHostCoreSite GHostCoreSites[] = {
		{ TEXT("Bind_ConfigEnums.cpp"), TEXT("Bind_ConfigEnums"), TEXT("metadata_only"), TEXT("Host enum TypeDeclarations; collision-profile values are not a Core callable control.") },
		{ TEXT("Bind_CoreGlobals.cpp"), TEXT("Bind_CoreGlobals"), TEXT("metadata_only"), TEXT("Commandlet globals need UClass and remain engine-replay.") },
		{ TEXT("Bind_FDateTime.cpp"), TEXT("Bind_FDateTime_ToStringContribution"), TEXT("metadata_only"), TEXT("ToString TypeDatabase sidecar; host AddToString is a no-op.") },
		{ TEXT("Bind_FDateTime.cpp"), TEXT("Bind_FDateTime"), TEXT("metadata_only"), TEXT("ExplicitBindings host subset not required for Len/Append/Counter.") },
		{ TEXT("Bind_FFormatArgumentValue_Type.cpp"), TEXT(""), TEXT("companion_no_output"), TEXT("Companion header has no FAngelscriptBind output.") },
		{ TEXT("Bind_FFormatArgumentValue.cpp"), TEXT("Bind_FFormatArgumentValue_Type"), TEXT("executable"), TEXT("Host ValueClass TypeDeclarations.") },
		{ TEXT("Bind_FFormatArgumentValue.cpp"), TEXT("Bind_FFormatArgumentValue_Infrastructure"), TEXT("metadata_only"), TEXT("RegisterTypeForTarget is a host no-op.") },
		{ TEXT("Bind_FFormatArgumentValue.cpp"), TEXT("Bind_FFormatArgumentValue"), TEXT("metadata_only"), TEXT("Format-argument constructors stay engine-replay.") },
		{ TEXT("Bind_FFrameTime.cpp"), TEXT("Bind_FFrameTime"), TEXT("metadata_only"), TEXT("Frame-time members stay engine-replay.") },
		{ TEXT("Bind_FGuid.cpp"), TEXT("Bind_EGuidFormats"), TEXT("metadata_only"), TEXT("Host enum TypeDeclarations.") },
		{ TEXT("Bind_FGuid.cpp"), TEXT("Bind_FGuid"), TEXT("metadata_only"), TEXT("FGuid members stay engine-replay.") },
		{ TEXT("Bind_FName_Type.cpp"), TEXT(""), TEXT("companion_no_output"), TEXT("Companion header has no FAngelscriptBind output.") },
		{ TEXT("Bind_FName.cpp"), TEXT("Bind_FName_Type"), TEXT("executable"), TEXT("Host ValueClass TypeDeclarations.") },
		{ TEXT("Bind_FName.cpp"), TEXT("Bind_FName_Infrastructure"), TEXT("metadata_only"), TEXT("RegisterType/ToString sidecars are host no-ops.") },
		{ TEXT("Bind_FName.cpp"), TEXT("Bind_FName"), TEXT("metadata_only"), TEXT("FName constructors/globals stay engine-replay; typed NAME_None is not an int host global.") },
		{ TEXT("Bind_FNumberFormattingOptions_Type.cpp"), TEXT(""), TEXT("companion_no_output"), TEXT("Companion header has no FAngelscriptBind output.") },
		{ TEXT("Bind_FNumberFormattingOptions.cpp"), TEXT("Bind_FNumberFormattingOptions_Type"), TEXT("executable"), TEXT("Host ValueClass TypeDeclarations.") },
		{ TEXT("Bind_FNumberFormattingOptions.cpp"), TEXT("Bind_FNumberFormattingOptions"), TEXT("metadata_only"), TEXT("Formatting options stay engine-replay.") },
		{ TEXT("Bind_FString_Functions.cpp"), TEXT(""), TEXT("companion_no_output"), TEXT("Companion header has no FAngelscriptBind output.") },
		{ TEXT("Bind_FString_Type.cpp"), TEXT(""), TEXT("companion_no_output"), TEXT("Companion header has no FAngelscriptBind output.") },
		{ TEXT("Bind_FString.cpp"), TEXT("Bind_FString_TypeDeclarations"), TEXT("executable"), TEXT("Host ValueClass TypeDeclarations.") },
		{ TEXT("Bind_FString.cpp"), TEXT("Bind_FString_TypeInfrastructure"), TEXT("metadata_only"), TEXT("String factory and TypeDatabase remain engine sidecars.") },
		{ TEXT("Bind_FString.cpp"), TEXT("Bind_FString_ManualBindings"), TEXT("executable"), TEXT("Host constructors plus Len/Append; remaining FString members stay engine-replay.") },
		{ TEXT("Bind_FStringTableRegistry.cpp"), TEXT("Bind_FStringTableRegistry_Types"), TEXT("metadata_only"), TEXT("Host enum TypeDeclarations.") },
		{ TEXT("Bind_FStringTableRegistry.cpp"), TEXT("Bind_FStringTableRegistry"), TEXT("metadata_only"), TEXT("LOCTABLE globals stay engine-replay.") },
		{ TEXT("Bind_FText_Functions.cpp"), TEXT(""), TEXT("companion_no_output"), TEXT("Companion header has no FAngelscriptBind output.") },
		{ TEXT("Bind_FText_Type.cpp"), TEXT(""), TEXT("companion_no_output"), TEXT("Companion header has no FAngelscriptBind output.") },
		{ TEXT("Bind_FText.cpp"), TEXT("Bind_FText_Type"), TEXT("executable"), TEXT("Host ValueClass and enum TypeDeclarations.") },
		{ TEXT("Bind_FText.cpp"), TEXT("Bind_FText_Infrastructure"), TEXT("metadata_only"), TEXT("Static type info and ToString remain engine sidecars.") },
		{ TEXT("Bind_FText.cpp"), TEXT("Bind_FText"), TEXT("metadata_only"), TEXT("FText members stay engine-replay.") },
		{ TEXT("Bind_FTimespan.cpp"), TEXT("Bind_FTimespan"), TEXT("metadata_only"), TEXT("Timespan members stay engine-replay.") },
		{ TEXT("Bind_Hash.cpp"), TEXT("Bind_Hash"), TEXT("metadata_only"), TEXT("Hash globals stay engine-replay.") },
		{ TEXT("Bind_Primitives.cpp"), TEXT("Bind_PrimitiveTypes_TypeInfrastructure"), TEXT("metadata_only"), TEXT("Primitive TypeDatabase adapters remain engine sidecars.") },
		{ TEXT("Bind_Primitives.cpp"), TEXT("Bind_PrimitiveTypes_ToStringContribution"), TEXT("metadata_only"), TEXT("ToString TypeDatabase sidecar; host AddToString is a no-op.") },
		{ TEXT("Bind_Primitives.cpp"), TEXT("Bind_PrimitiveTypes"), TEXT("metadata_only"), TEXT("Typed numeric constants beyond int stay engine-replay.") },
	};

	const FName GHostCoreExecutableBinds[] = {
		TEXT("FFormatArgumentValue.Type"),
		TEXT("FFormatArgumentValue.Infrastructure"),
		TEXT("EGuidFormats"),
		TEXT("FName.Type"),
		TEXT("FName.Infrastructure"),
		TEXT("FNumberFormattingOptions.Type"),
		TEXT("FString.TypeDeclarations"),
		TEXT("FString.TypeInfrastructure"),
		TEXT("FString.ExplicitBindings"),
		TEXT("FStringTableRegistry.Types"),
		TEXT("FText.Type"),
		TEXT("FText.Infrastructure"),
		TEXT("FDateTime.ToStringContribution"),
		TEXT("PrimitiveTypes.TypeInfrastructure"),
		TEXT("PrimitiveTypes.ToStringContribution"),
		TEXT("PrimitiveTypes.Constants"),
		TEXT("ConfigEnums"),
	};

	bool HostCoreAppendNamed(
		UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
		FName BindName,
		FString& Diagnostic)
	{
		for (const FAngelscriptBindRecord& Record : FAngelscriptBind::GetRegisteredCollection().GetRecords())
		{
			if (Record.BindName == BindName)
			{
				return Collection.Append(Record, Diagnostic);
			}
		}
		Diagnostic = FString::Printf(TEXT("missing production bind %s"), *BindName.ToString());
		return false;
	}

	bool HostCoreAppendProduction(UE::Angelscript::Private::FAngelscriptBindCollection& Collection, FString& Diagnostic)
	{
		for (const FName BindName : GHostCoreExecutableBinds)
		{
			if (!HostCoreAppendNamed(Collection, BindName, Diagnostic))
			{
				return false;
			}
		}
		return Collection.Append(
			HostCoreMakeRecord(TEXT("HostCore.Counter"), EAngelscriptBindPhase::ExplicitBindings, &HostCoreRegisterCounter, 40),
			Diagnostic);
	}

	asCScriptFunction* HostCoreFindMethod(asCObjectType* Type, const char* Name)
	{
		if (!Type || !Name) return nullptr;
		for (asUINT Index = 0; Index < Type->GetMethodCount(); ++Index)
		{
			asIScriptFunction* Method = Type->GetMethodByIndex(Index);
			if (Method && Method->GetName() && FCStringAnsi::Strcmp(Method->GetName(), Name) == 0)
			{
				return static_cast<asCScriptFunction*>(Method);
			}
		}
		return nullptr;
	}

	int32 HostCoreCallLen(asCScriptFunction& Function, FString& Text)
	{
		const auto Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->caller.IsBound()) return MIN_int32;
		int32 Result = 0;
		void* Args[1] = { &Text };
		if (Interface->caller.type == 2)
		{
			Interface->caller.MethodCaller(Interface->method, Args, &Result);
		}
		else
		{
			Interface->caller.FunctionCaller(Interface->func, Args, &Result);
		}
		return Result;
	}

	void HostCoreCallAppend(asCScriptFunction& Function, FString& Text, const FString& Suffix)
	{
		const auto Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->caller.IsBound()) return;
		void* Args[2] = { &Text, const_cast<FString*>(&Suffix) };
		FString* Returned = nullptr;
		if (Interface->caller.type == 2)
		{
			Interface->caller.MethodCaller(Interface->method, Args, &Returned);
		}
		else
		{
			Interface->caller.FunctionCaller(Interface->func, Args, &Returned);
		}
	}

	FString HostCoreCallJoin(
		asCScriptFunction& Function,
		const FString& First,
		const FString& Second,
		const FString& Separator)
	{
		FString Result;
		const auto Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->caller.IsBound())
		{
			return Result;
		}
		void* Args[3] = {
			const_cast<FString*>(&First),
			const_cast<FString*>(&Second),
			const_cast<FString*>(&Separator)
		};
		if (Interface->caller.type == 2)
		{
			Interface->caller.MethodCaller(Interface->method, Args, &Result);
		}
		else
		{
			Interface->caller.FunctionCaller(Interface->func, Args, &Result);
		}
		return Result;
	}
}

TEST_CLASS_WITH_FLAGS(HostCore, "Angelscript.UnitTest.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CoreBoundBehavior)
	{
		GHostCoreCounter = 7;
		GHostCoreCounterCallbacks = 0;
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostCoreAppendProduction(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(
			Collection.ExecuteToHost(Diagnostic),
			Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("FString.TypeDeclarations"))));
		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("FString.ExplicitBindings"))));
		ASSERT_THAT(AreEqual(1, GHostCoreCounterCallbacks));
		const asCDefinitions* Graph = Collection.GetHostDefinitions();
		ASSERT_THAT(IsNotNull(Graph));
		asCObjectType* StringType = static_cast<asCObjectType*>(Graph->FindType("FString"));
		ASSERT_THAT(IsNotNull(StringType));
		asCScriptFunction* Len = HostCoreFindMethod(StringType, "Len");
		asCScriptFunction* Append = HostCoreFindMethod(StringType, "Append");
		ASSERT_THAT(IsNotNull(Len));
		ASSERT_THAT(IsNotNull(Append));
		FString Text(TEXT("abc"));
		ASSERT_THAT(AreEqual(3, HostCoreCallLen(*Len, Text)));
		HostCoreCallAppend(*Append, Text, FString(TEXT("d")));
		ASSERT_THAT(AreEqual(4, HostCoreCallLen(*Len, Text)));
		ASSERT_THAT(AreEqual(FString(TEXT("abcd")), Text));
		TArray<FAngelscriptHostDeclarationView> Views;
		Collection.InspectHostDeclarations(Views);
		bool bFoundCounter = false;
		for (const FAngelscriptHostDeclarationView& View : Views)
		{
			if (View.Name == TEXT("Counter") && View.Kind == TEXT("Global"))
			{
				bFoundCounter = true;
				ASSERT_THAT(AreEqual(TEXT("N"), View.Namespace));
				ASSERT_THAT(AreEqual(7, View.IntValue));
			}
		}
		ASSERT_THAT(IsTrue(bFoundCounter));
		FHostCoreEngineOwner A;
		FHostCoreEngineOwner B;
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("FString.ExplicitBindings"))));
		FString Other(TEXT("abc"));
		HostCoreCallAppend(*Append, Other, FString(TEXT("d")));
		ASSERT_THAT(AreEqual(FString(TEXT("abcd")), Other));
	}

	TEST_METHOD(CompleteFamilyAccounting)
	{
		ASSERT_THAT(AreEqual(35, static_cast<int32>(UE_ARRAY_COUNT(GHostCoreSites))));
		int32 Executable = 0;
		int32 MetadataOnly = 0;
		int32 Companion = 0;
		for (const FHostCoreSite& Site : GHostCoreSites)
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
		ASSERT_THAT(AreEqual(6, Executable));
		ASSERT_THAT(AreEqual(22, MetadataOnly));
		ASSERT_THAT(AreEqual(7, Companion));
		GHostCoreCounter = 7;
		GHostCoreCounterCallbacks = 0;
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostCoreAppendProduction(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), *Diagnostic));
		const int32 StringDecl = Collection.GetCallbackInvocationCount(TEXT("FString.TypeDeclarations"));
		const int32 StringBind = Collection.GetCallbackInvocationCount(TEXT("FString.ExplicitBindings"));
		FHostCoreEngineOwner A;
		FHostCoreEngineOwner B;
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(AreEqual(StringDecl, Collection.GetCallbackInvocationCount(TEXT("FString.TypeDeclarations"))));
		ASSERT_THAT(AreEqual(StringBind, Collection.GetCallbackInvocationCount(TEXT("FString.ExplicitBindings"))));
		ASSERT_THAT(AreEqual(1, GHostCoreCounterCallbacks));
		ASSERT_THAT(IsNotNull(Collection.GetHostDefinitions()->FindType("FString")));
		ASSERT_THAT(IsNotNull(Collection.GetHostDefinitions()->FindType("FName")));
		ASSERT_THAT(IsNotNull(Collection.GetHostDefinitions()->FindType("FText")));
	}

	TEST_METHOD(OwnerAndFailureBoundaries)
	{
		GHostCoreCounter = 7;
		UE::Angelscript::Private::FAngelscriptBindCollection Healthy;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostCoreAppendProduction(Healthy, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.ExecuteToHost(Diagnostic), *Diagnostic));
		FHostCoreEngineOwner B;
		asCDefinitionGraphLease Lease = Healthy.RetainHostGraph();
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		asCObjectType* StringType = static_cast<asCObjectType*>(Healthy.GetHostDefinitions()->FindType("FString"));
		asCScriptFunction* Len = HostCoreFindMethod(StringType, "Len");
		ASSERT_THAT(IsNotNull(Len));
		FString Text(TEXT("abc"));
		ASSERT_THAT(AreEqual(3, HostCoreCallLen(*Len, Text)));
		UE::Angelscript::Private::FAngelscriptBindCollection Failed;
		ASSERT_THAT(IsTrue(Failed.Append(HostCoreMakeRecord(TEXT("Missing"), EAngelscriptBindPhase::ExplicitBindings, &HostCoreRegisterMissing, 80), Diagnostic)));
		ASSERT_THAT(IsTrue(Failed.Finalize(Diagnostic)));
		ASSERT_THAT(IsFalse(Failed.ExecuteToHost(Diagnostic)));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("native target"))));
		ASSERT_THAT(IsNull(Failed.GetHostDefinitions()));
		ASSERT_THAT(AreEqual(3, HostCoreCallLen(*Len, Text)));
		ASSERT_THAT(AreEqual(FString(TEXT("abc")), Text));
	}

	TEST_METHOD(CopyThenAppendProducesAbcdWithoutChangingOriginal)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostCoreAppendProduction(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), *Diagnostic));
		asCObjectType* StringType = static_cast<asCObjectType*>(Collection.GetHostDefinitions()->FindType("FString"));
		asCScriptFunction* Append = HostCoreFindMethod(StringType, "Append");
		ASSERT_THAT(IsNotNull(Append));
		FString Original(TEXT("ab"));
		FString Copy = Original;
		HostCoreCallAppend(*Append, Copy, FString(TEXT("cd")));
		ASSERT_THAT(AreEqual(FString(TEXT("abcd")), Copy));
		ASSERT_THAT(AreEqual(FString(TEXT("ab")), Original));
	}

	TEST_METHOD(JoinAAndBWithDash)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostCoreAppendProduction(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Append(
			HostCoreMakeRecord(TEXT("FString.HostJoin"), EAngelscriptBindPhase::ExplicitBindings, &HostCoreRegisterJoin, 50),
			Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(
			Collection.ExecuteToHost(Diagnostic),
			Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		asCObjectType* StringType = static_cast<asCObjectType*>(Collection.GetHostDefinitions()->FindType("FString"));
		asCScriptFunction* Join = HostCoreFindMethod(StringType, "Join");
		ASSERT_THAT(IsNotNull(Join, TEXT("host FString.Join(Other, Separator)")));
		const FString Joined = HostCoreCallJoin(*Join, TEXT("a"), TEXT("b"), TEXT("-"));
		ASSERT_THAT(AreEqual(FString(TEXT("a-b")), Joined));
	}
};

#endif
