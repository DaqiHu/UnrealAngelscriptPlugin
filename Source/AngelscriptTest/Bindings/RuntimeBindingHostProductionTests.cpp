#include "CQTest.h"
#include "Containers/ScriptArray.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptRuntimeModule.h"
#include "Core/AngelscriptSubsystem.h"
#include "as_callfunc.h"
#include "as_datatype.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 HostProductionArrayNum(FScriptArray* Self) { return Self ? Self->Num() : MIN_int32; }
	void HostProductionArrayAdd(FScriptArray* Self, int32 Value)
	{
		if (!Self) return;
		const int32 Index = Self->Add(1, sizeof(int32), alignof(int32));
		*reinterpret_cast<int32*>(static_cast<uint8*>(Self->GetData()) + (sizeof(int32) * Index)) = Value;
	}

	int32 HostProductionExtensionTag(FString* Self)
	{
		return Self ? 17 : 0;
	}

	void HostProductionRegisterContainers(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds IntArray = Binds.SpecializeTemplateForTarget("TArray", "int");
		IntArray.Method("int Num() const", &HostProductionArrayNum);
		IntArray.Method("void Add(int Value)", &HostProductionArrayAdd);
	}

	void HostProductionRegisterExtension(FAngelscriptBinds& Binds)
	{
		Binds.ExistingClassForTarget("FString").Method("int HostProductionTag() const", &HostProductionExtensionTag);
	}

	void HostProductionRegisterMissing(FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionDirectForTarget("void Missing()", asSFuncPtr(), asCALL_CDECL, ASAutoCaller::FunctionCaller::Make());
	}

	FAngelscriptBindRecord HostProductionMakeRecord(FName BindName, EAngelscriptBindPhase Phase, FAngelscriptBindCallback Callback, int32 Line)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("AngelscriptTest");
		Record.BindName = BindName;
		Record.Phase = Phase;
		Record.SourceFile = "RuntimeBindingHostProductionTests.cpp";
		Record.SourceLine = Line;
		Record.Callback = Callback;
		return Record;
	}

	bool HostProductionAppendNamed(
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

	const FName GHostProductionEligibleBinds[] = {
		TEXT("FString.TypeDeclarations"),
		TEXT("FString.ExplicitBindings"),
		TEXT("FVector.TypeDeclarations"),
		TEXT("FVector"),
		TEXT("TArray.Declaration"),
		TEXT("TMap.Declaration"),
		TEXT("TSet.Declaration"),
		TEXT("TOptional.Declaration"),
	};

	bool HostProductionAppendEligible(UE::Angelscript::Private::FAngelscriptBindCollection& Collection, FString& Diagnostic)
	{
		for (const FName BindName : GHostProductionEligibleBinds)
		{
			if (!HostProductionAppendNamed(Collection, BindName, Diagnostic))
			{
				return false;
			}
		}
		return Collection.Append(
			HostProductionMakeRecord(TEXT("HostProduction.Members"), EAngelscriptBindPhase::ExplicitBindings, &HostProductionRegisterContainers, 20),
			Diagnostic);
	}

	asCScriptFunction* HostProductionFindMethod(asCObjectType* Type, const char* Name, int32 ParamCount = -1)
	{
		if (!Type || !Name) return nullptr;
		for (asUINT Index = 0; Index < Type->GetMethodCount(); ++Index)
		{
			asIScriptFunction* Method = Type->GetMethodByIndex(Index);
			if (!Method || !Method->GetName() || FCStringAnsi::Strcmp(Method->GetName(), Name) != 0)
			{
				continue;
			}
			if (ParamCount >= 0 && static_cast<int32>(Method->GetParamCount()) != ParamCount)
			{
				continue;
			}
			return static_cast<asCScriptFunction*>(Method);
		}
		return nullptr;
	}

	void HostProductionCall(asCScriptFunction& Function, void** Args, void* Return)
	{
		const auto Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->caller.IsBound()) return;
		if (Interface->caller.type == 2) Interface->caller.MethodCaller(Interface->method, Args, Return);
		else Interface->caller.FunctionCaller(Interface->func, Args, Return);
	}

	int32 HostProductionCallLen(asCScriptFunction& Function, FString& Text)
	{
		int32 Result = MIN_int32;
		void* Args[1] = { &Text };
		HostProductionCall(Function, Args, &Result);
		return Result;
	}

	FVector HostProductionCallAdd(asCScriptFunction& Function, FVector& Left, const FVector& Right)
	{
		FVector Result(ForceInitToZero);
		void* Args[2] = { &Left, const_cast<FVector*>(&Right) };
		HostProductionCall(Function, Args, &Result);
		return Result;
	}

	asCObjectType* HostProductionIntArray(asCDefinitions& Graph)
	{
		asCObjectType* Template = static_cast<asCObjectType*>(Graph.FindType("TArray"));
		if (!Template) return nullptr;
		asCDataType IntType = Graph.Primitive(ttInt);
		return Graph.FindTemplateInstance(Template, MakeArrayView(&IntType, 1));
	}

	asCTypeInfo* HostProductionEngineType(asCScriptEngine* Engine, const char* Name)
	{
		return Engine ? Engine->FindType("", Name) : nullptr;
	}

	struct FHostProductionReasonedSite
	{
		const TCHAR* Source;
		const TCHAR* Symbol;
		const TCHAR* Evidence;
		const TCHAR* Reason;
	};

	struct FHostProductionMathSite
	{
		const TCHAR* Source;
		const TCHAR* Symbol;
		const TCHAR* Evidence;
	};

	const FHostProductionReasonedSite GHostProductionCoreSites[] = {
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
	const FHostProductionMathSite GHostProductionMathSites[] = {
#include "RuntimeBindingHostMathSites.inl"
	};
	const FHostProductionReasonedSite GHostProductionContainerSites[] = {
#include "RuntimeBindingHostContainersSites.inl"
	};
	const FHostProductionReasonedSite GHostProductionObjectSites[] = {
#include "RuntimeBindingHostObjectsSites.inl"
	};
	const FHostProductionReasonedSite GHostProductionGameplaySites[] = {
#include "RuntimeBindingHostGameplaySites.inl"
	};
	const FHostProductionReasonedSite GHostProductionServiceSites[] = {
#include "RuntimeBindingHostServicesSites.inl"
	};

	void HostProductionCountSites(const FHostProductionReasonedSite* Sites, int32 Count, int32& Executable, int32& MetadataOnly, int32& Companion)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (FCString::Strcmp(Sites[Index].Evidence, TEXT("executable")) == 0) ++Executable;
			else if (FCString::Strcmp(Sites[Index].Evidence, TEXT("metadata_only")) == 0) ++MetadataOnly;
			else ++Companion;
		}
	}
}

TEST_CLASS_WITH_FLAGS(HostProduction, "Angelscript.UnitTest.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ProductionEntryReusesGraph)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostProductionAppendEligible(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		auto A = FAngelscriptEngine::CreateForBindings(Collection, BindingDiagnostic);
		auto B = FAngelscriptEngine::CreateForBindings(Collection, BindingDiagnostic);
		ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid(), BindingDiagnostic.Message.IsEmpty() ? TEXT("<empty create diagnostic>") : *BindingDiagnostic.Message));
		ASSERT_THAT(IsTrue(A->IsBindingEngine()));
		ASSERT_THAT(IsTrue(B->IsBindingEngine()));
		ASSERT_THAT(IsNull(A->GetBindingInstallation()));
		ASSERT_THAT(IsNull(B->GetBindingInstallation()));
		ASSERT_THAT(IsTrue(A->Engine != B->Engine));
		asCDefinitions* Graph = const_cast<asCDefinitions*>(Collection.GetHostDefinitions());
		ASSERT_THAT(IsNotNull(Graph));
		asCObjectType* StringType = static_cast<asCObjectType*>(Graph->FindType("FString"));
		asCObjectType* VectorType = static_cast<asCObjectType*>(Graph->FindType("FVector"));
		asCObjectType* ArrayType = HostProductionIntArray(*Graph);
		ASSERT_THAT(IsNotNull(StringType));
		ASSERT_THAT(IsNotNull(VectorType));
		ASSERT_THAT(IsNotNull(ArrayType));
		ASSERT_THAT(IsTrue(HostProductionEngineType(A->Engine, "FString") == StringType));
		ASSERT_THAT(IsTrue(HostProductionEngineType(B->Engine, "FString") == StringType));
		ASSERT_THAT(IsTrue(HostProductionEngineType(A->Engine, "FVector") == VectorType));
		ASSERT_THAT(IsTrue(HostProductionEngineType(B->Engine, "FVector") == VectorType));
		ASSERT_THAT(IsNull(StringType->GetEngine()));
		ASSERT_THAT(IsNull(VectorType->GetEngine()));
		asCScriptFunction* Len = HostProductionFindMethod(StringType, "Len", 0);
		asCScriptFunction* CtorXyz = HostProductionFindMethod(VectorType, "f", 3);
		asCScriptFunction* Add = HostProductionFindMethod(VectorType, "opAdd", 1);
		asCScriptFunction* ArrayAdd = HostProductionFindMethod(ArrayType, "Add");
		asCScriptFunction* ArrayNum = HostProductionFindMethod(ArrayType, "Num");
		ASSERT_THAT(IsNotNull(Len));
		ASSERT_THAT(IsNotNull(CtorXyz));
		ASSERT_THAT(IsNotNull(Add));
		ASSERT_THAT(IsNotNull(ArrayAdd));
		ASSERT_THAT(IsNotNull(ArrayNum));
		FString Text(TEXT("abc"));
		ASSERT_THAT(AreEqual(3, HostProductionCallLen(*Len, Text)));
		FVector Left(ForceInitToZero);
		FVector Right(ForceInitToZero);
		double X = 1.0, Y = 2.0, Z = 3.0;
		void* LeftArgs[4] = { &Left, &X, &Y, &Z };
		HostProductionCall(*CtorXyz, LeftArgs, nullptr);
		double X2 = 4.0, Y2 = 5.0, Z2 = 6.0;
		void* RightArgs[4] = { &Right, &X2, &Y2, &Z2 };
		HostProductionCall(*CtorXyz, RightArgs, nullptr);
		const FVector Sum = HostProductionCallAdd(*Add, Left, Right);
		ASSERT_THAT(IsTrue(Sum.Equals(FVector(5.0, 7.0, 9.0))));
		FScriptArray Array;
		int32 Two = 2, Five = 5;
		void* Add2[2] = { &Array, &Two };
		void* Add5[2] = { &Array, &Five };
		HostProductionCall(*ArrayAdd, Add2, nullptr);
		HostProductionCall(*ArrayAdd, Add5, nullptr);
		int32 ArrayCount = 0;
		void* ArraySelf[1] = { &Array };
		HostProductionCall(*ArrayNum, ArraySelf, &ArrayCount);
		ASSERT_THAT(AreEqual(2, ArrayCount));
		Array.Empty(0, sizeof(int32), alignof(int32));
		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("FString.TypeDeclarations"))));
		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("FString.ExplicitBindings"))));
		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("FVector"))));
		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("HostProduction.Members"))));
		ASSERT_THAT(IsTrue(FAngelscriptBind::EnsureProcessHostCollection(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(FAngelscriptBind::EnsureProcessHostCollection(Diagnostic), *Diagnostic));
		ASSERT_THAT(AreEqual(1, FAngelscriptBind::GetProcessHostCaptureCountForTesting()));
		ASSERT_THAT(IsNotNull(FAngelscriptBind::GetProcessHostCollection()));
		ASSERT_THAT(IsNotNull(FAngelscriptBind::GetProcessHostCollection()->GetHostDefinitions()));
	}

	TEST_METHOD(WholeSourceReconciliation)
	{
		ASSERT_THAT(AreEqual(35, static_cast<int32>(UE_ARRAY_COUNT(GHostProductionCoreSites))));
		ASSERT_THAT(AreEqual(118, static_cast<int32>(UE_ARRAY_COUNT(GHostProductionMathSites))));
		ASSERT_THAT(AreEqual(16, static_cast<int32>(UE_ARRAY_COUNT(GHostProductionContainerSites))));
		ASSERT_THAT(AreEqual(49, static_cast<int32>(UE_ARRAY_COUNT(GHostProductionObjectSites))));
		ASSERT_THAT(AreEqual(63, static_cast<int32>(UE_ARRAY_COUNT(GHostProductionGameplaySites))));
		ASSERT_THAT(AreEqual(51, static_cast<int32>(UE_ARRAY_COUNT(GHostProductionServiceSites))));
		int32 Executable = 0, MetadataOnly = 0, Companion = 0;
		HostProductionCountSites(GHostProductionCoreSites, UE_ARRAY_COUNT(GHostProductionCoreSites), Executable, MetadataOnly, Companion);
		for (const FHostProductionMathSite& Site : GHostProductionMathSites)
		{
			if (FCString::Strcmp(Site.Evidence, TEXT("executable")) == 0) ++Executable;
			else if (FCString::Strcmp(Site.Evidence, TEXT("metadata_only")) == 0) ++MetadataOnly;
			else ++Companion;
		}
		HostProductionCountSites(GHostProductionContainerSites, UE_ARRAY_COUNT(GHostProductionContainerSites), Executable, MetadataOnly, Companion);
		HostProductionCountSites(GHostProductionObjectSites, UE_ARRAY_COUNT(GHostProductionObjectSites), Executable, MetadataOnly, Companion);
		HostProductionCountSites(GHostProductionGameplaySites, UE_ARRAY_COUNT(GHostProductionGameplaySites), Executable, MetadataOnly, Companion);
		HostProductionCountSites(GHostProductionServiceSites, UE_ARRAY_COUNT(GHostProductionServiceSites), Executable, MetadataOnly, Companion);
		ASSERT_THAT(AreEqual(12, Executable));
		ASSERT_THAT(AreEqual(242, MetadataOnly));
		ASSERT_THAT(AreEqual(78, Companion));
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostProductionAppendEligible(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Append(
			HostProductionMakeRecord(TEXT("HostProduction.Extension"), EAngelscriptBindPhase::ExplicitBindings, &HostProductionRegisterExtension, 40),
			Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		bool bFoundExtension = false;
		for (const FAngelscriptBindRecord& Record : Collection.GetRecords())
		{
			if (Record.BindName == TEXT("HostProduction.Extension"))
			{
				bFoundExtension = true;
				ASSERT_THAT(AreEqual(FName(TEXT("AngelscriptTest")), Record.OwnerModule));
			}
		}
		ASSERT_THAT(IsTrue(bFoundExtension));
		asCObjectType* StringType = static_cast<asCObjectType*>(Collection.GetHostDefinitions()->FindType("FString"));
		asCScriptFunction* Tag = HostProductionFindMethod(StringType, "HostProductionTag", 0);
		ASSERT_THAT(IsNotNull(Tag));
		FString Text(TEXT("ok"));
		int32 TagValue = 0;
		void* TagArgs[1] = { &Text };
		HostProductionCall(*Tag, TagArgs, &TagValue);
		ASSERT_THAT(AreEqual(17, TagValue));
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		auto Owner = FAngelscriptEngine::CreateForBindings(Collection, BindingDiagnostic);
		ASSERT_THAT(IsTrue(Owner.IsValid(), *BindingDiagnostic.Message));
		ASSERT_THAT(IsNull(Owner->GetBindingInstallation()));
		ASSERT_THAT(IsTrue(HostProductionEngineType(Owner->Engine, "FString") == StringType));
	}

	TEST_METHOD(IsolationAndFailedCreate)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Healthy;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostProductionAppendEligible(Healthy, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.ExecuteToHost(Diagnostic), Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		auto B = FAngelscriptEngine::CreateForBindings(Healthy, BindingDiagnostic);
		ASSERT_THAT(IsTrue(B.IsValid(), BindingDiagnostic.Message.IsEmpty() ? TEXT("<empty create diagnostic>") : *BindingDiagnostic.Message));
		asCObjectType* StringType = static_cast<asCObjectType*>(Healthy.GetHostDefinitions()->FindType("FString"));
		asCScriptFunction* Len = HostProductionFindMethod(StringType, "Len", 0);
		ASSERT_THAT(IsNotNull(Len));
		FString Text(TEXT("abc"));
		ASSERT_THAT(AreEqual(3, HostProductionCallLen(*Len, Text)));
		UE::Angelscript::Private::FAngelscriptBindCollection Failed;
		ASSERT_THAT(IsTrue(Failed.Append(
			HostProductionMakeRecord(TEXT("HostProduction.Missing"), EAngelscriptBindPhase::ExplicitBindings, &HostProductionRegisterMissing, 80),
			Diagnostic)));
		ASSERT_THAT(IsTrue(Failed.Finalize(Diagnostic)));
		FAngelscriptBindingDiagnostic FailedDiagnostic;
		auto Broken = FAngelscriptEngine::CreateForBindings(Failed, FailedDiagnostic);
		ASSERT_THAT(IsFalse(Broken.IsValid()));
		ASSERT_THAT(IsFalse(FailedDiagnostic.Message.IsEmpty()));
		ASSERT_THAT(IsNull(Failed.GetHostDefinitions()));
		ASSERT_THAT(AreEqual(3, HostProductionCallLen(*Len, Text)));
		ASSERT_THAT(AreEqual(FString(TEXT("abc")), Text));
		ASSERT_THAT(IsTrue(HostProductionEngineType(B->Engine, "FString") == StringType));
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
		ASSERT_THAT(IsFalse(FAngelscriptRuntimeModule::IsLegacyRuntimeEnabled()));
		auto* Subsystem = UAngelscriptSubsystem::Get();
		ASSERT_THAT(IsNotNull(Subsystem));
		ASSERT_THAT(IsNull(Subsystem->GetEngine()));
		ASSERT_THAT(IsFalse(Subsystem->IsAllowedToTick()));
		ASSERT_THAT(IsNull(B->GetCacheService()));
		ASSERT_THAT(IsNull(B->JITCoordinator));
		ASSERT_THAT(IsFalse(B->bHotReloadThreadStarted));
		ASSERT_THAT(IsNull(B->GetSourceProvider()));
		ASSERT_THAT(AreEqual(0, B->GetActiveModules().Num()));
		ASSERT_THAT(IsFalse(B->HasDebugServerForTesting()));
	}
};

#endif
