#include "CQTest.h"
#include "Containers/ScriptArray.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptEngine.h"
#include "HAL/PlatformProcess.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "as_context.h"
#include "as_definitions.h"
#include "as_datatype.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString HostPerfSamplesPath()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("openspec/changes/angelscript/refactor-bindings-host-full-inject/attachments/data/host-perf-samples.md")));
	}

	bool HostPerfWriteSection(const FString& Heading, const FString& Body, FString& OutDiagnostic)
	{
		const FString Path = HostPerfSamplesPath();
		FString Existing;
		FFileHelper::LoadFileToString(Existing, *Path);
		if (Existing.IsEmpty())
		{
			Existing = TEXT("# Host perf samples\n\n");
		}
		const FString Marker = TEXT("## ") + Heading;
		const int32 Start = Existing.Find(Marker, ESearchCase::CaseSensitive);
		FString Before;
		FString After;
		if (Start != INDEX_NONE)
		{
			Before = Existing.Left(Start);
			const int32 Next = Existing.Find(TEXT("\n## "), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start + Marker.Len());
			After = Next != INDEX_NONE ? Existing.Mid(Next + 1) : FString();
		}
		else
		{
			Before = Existing;
			if (!Before.EndsWith(TEXT("\n")))
			{
				Before += TEXT("\n");
			}
			if (!Before.EndsWith(TEXT("\n\n")))
			{
				Before += TEXT("\n");
			}
		}
		const FString Written = Before + Marker + TEXT("\n\n") + Body + TEXT("\n") + After;
		if (!FFileHelper::SaveStringToFile(Written, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutDiagnostic = FString::Printf(TEXT("could not write %s"), *Path);
			return false;
		}
		return true;
	}

	int32 HostPerfSumUniqueCallbackInvocations(
		const UE::Angelscript::Private::FAngelscriptBindCollection& Collection)
	{
		TSet<FName> Seen;
		int32 Total = 0;
		for (const FAngelscriptBindRecord& Record : Collection.GetRecords())
		{
			if (Seen.Contains(Record.BindName))
			{
				continue;
			}
			Seen.Add(Record.BindName);
			Total += Collection.GetCallbackInvocationCount(Record.BindName);
		}
		return Total;
	}

	FString HostPerfIdentityRows()
	{
		return FString::Printf(
			TEXT("| executable | `%s` |\n| engine_version | `%s` |\n| project | `%s` |\n"),
			FPlatformProcess::ExecutableName(false),
			*FEngineVersion::Current().ToString(),
			*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	}

	bool HostPerfAppendEveryRegistered(
		UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
		FString& Diagnostic)
	{
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic))
		{
			return false;
		}
		for (const FAngelscriptBindRecord& Record : FAngelscriptBind::GetRegisteredCollection().GetRecords())
		{
			if (!Collection.Append(Record, Diagnostic))
			{
				return false;
			}
		}
		return Collection.Finalize(Diagnostic);
	}

	asCScriptFunction* HostPerfFindMethod(asCObjectType* Type, const char* Name, int32 ParamCount)
	{
		if (!Type || !Name)
		{
			return nullptr;
		}
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

	asCScriptFunction* HostPerfFindConstruct(asCObjectType* Type)
	{
		if (!Type)
		{
			return nullptr;
		}
		for (asUINT Index = 0; Index < Type->GetBehaviourCount(); ++Index)
		{
			asEBehaviours Behaviour;
			asIScriptFunction* Function = Type->GetBehaviourByIndex(Index, &Behaviour);
			if (Behaviour == asBEHAVE_CONSTRUCT && Function && Function->GetParamCount() == 0)
			{
				return static_cast<asCScriptFunction*>(Function);
			}
		}
		return nullptr;
	}

	asCObjectType* HostPerfFindIntArray(asCScriptEngine* Engine, const asCDefinitions* Graph)
	{
		if (Graph)
		{
			if (asCObjectType* Template = static_cast<asCObjectType*>(Graph->FindType("TArray")))
			{
				asCDataType Inner = Graph->Primitive(ttInt);
				if (asCObjectType* Instance = Graph->FindTemplateInstance(Template, MakeArrayView(&Inner, 1)))
				{
					return Instance;
				}
			}
		}
		return Engine ? static_cast<asCObjectType*>(Engine->GetTypeInfoByDecl("TArray<int>")) : nullptr;
	}

	bool HostPerfRunCheck(asCScriptEngine& Engine, const asCDefinitions* Graph, int32& OutValue)
	{
		OutValue = 0;
		asCObjectType* StringType = static_cast<asCObjectType*>(Engine.GetTypeInfoByName("FString"));
		asCObjectType* VectorType = static_cast<asCObjectType*>(Engine.GetTypeInfoByName("FVector"));
		asCObjectType* ArrayType = HostPerfFindIntArray(&Engine, Graph);
		asCScriptFunction* Len = HostPerfFindMethod(StringType, "Len", 0);
		asCScriptFunction* AddVec = HostPerfFindMethod(VectorType, "opAdd", 1);
		asCScriptFunction* Construct = HostPerfFindConstruct(ArrayType);
		asCScriptFunction* AddArr = HostPerfFindMethod(ArrayType, "Add", 1);
		asCScriptFunction* Num = HostPerfFindMethod(ArrayType, "Num", 0);
		if (!Len || !AddVec || !Construct || !AddArr || !Num)
		{
			return false;
		}
		asIScriptContext* Context = Engine.CreateContext();
		if (!Context)
		{
			return false;
		}
		ON_SCOPE_EXIT { Context->Release(); };

		FString Text(TEXT("abc"));
		if (Context->Prepare(Len) < 0 || Context->SetObject(&Text) != 0
			|| Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		const int32 Length = static_cast<int32>(Context->GetReturnDWord());

		FVector Left(1.0, 2.0, 3.0);
		FVector Right(4.0, 5.0, 6.0);
		if (Context->Prepare(AddVec) < 0 || Context->SetObject(&Left) != 0
			|| Context->SetArgObject(0, &Right) != 0
			|| Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		const FVector* Sum = static_cast<const FVector*>(Context->GetAddressOfReturnValue());
		if (!Sum)
		{
			return false;
		}
		const FVector Added = *Sum;

		alignas(FScriptArray) uint8 Storage[sizeof(FScriptArray)] = {};
		FScriptArray* Array = reinterpret_cast<FScriptArray*>(Storage);
		if (Context->Prepare(Construct) < 0 || Context->SetObject(Array) != 0
			|| Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		int32 Value = 4;
		if (Context->Prepare(AddArr) < 0 || Context->SetObject(Array) != 0
			|| Context->SetArgAddress(0, &Value) != 0
			|| Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		if (Context->Prepare(Num) < 0 || Context->SetObject(Array) != 0
			|| Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		const int32 Count = static_cast<int32>(Context->GetReturnDWord());
		OutValue = Length + static_cast<int32>(Added.X) + static_cast<int32>(Added.Y)
			+ static_cast<int32>(Added.Z) + Count;
		return true;
	}

	double HostPerfPercentile(TArray<double>& Samples, double Fraction)
	{
		Samples.Sort();
		if (Samples.IsEmpty())
		{
			return 0.0;
		}
		const int32 Index = FMath::Clamp(
			int32(FMath::CeilToDouble(Fraction * Samples.Num()) - 1.0),
			0,
			Samples.Num() - 1);
		return Samples[Index];
	}
}

TEST_CLASS_WITH_FLAGS(HostPerf, "Angelscript.UnitTest.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RecordSevenPhaseCaptureWallClock)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostPerfAppendEveryRegistered(Collection, Diagnostic), *Diagnostic));

		const double Started = FPlatformTime::Seconds();
		const bool bCaptured = Collection.ExecuteToHost(Diagnostic);
		const double Elapsed = FPlatformTime::Seconds() - Started;
		ASSERT_THAT(IsTrue(bCaptured, Diagnostic.IsEmpty() ? TEXT("<empty ExecuteToHost diagnostic>") : *Diagnostic));
		ASSERT_THAT(IsNotNull(Collection.GetHostDefinitions()));
		ASSERT_THAT(IsTrue(Collection.GetHostDefinitions()->GetState() == asEDefinitionState::Frozen));

		const int32 CallbackInvocations = HostPerfSumUniqueCallbackInvocations(Collection);
		ASSERT_THAT(IsTrue(CallbackInvocations > 0));
		FString WriteDiagnostic;
		const FString Body = FString::Printf(
			TEXT("| Field | Value |\n|---|---|\n| wall_clock_seconds | %.6f |\n| callback_invocations | %d |\n| record_count | %d |\n%s"),
			Elapsed,
			CallbackInvocations,
			Collection.GetRecords().Num(),
			*HostPerfIdentityRows());
		ASSERT_THAT(IsTrue(HostPerfWriteSection(TEXT("P1"), Body, WriteDiagnostic), *WriteDiagnostic));
	}

	TEST_METHOD(SecondInjectDoesNotRerunCallbacks)
	{
		FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptBind::EnsureProcessHostCollection(Diagnostic), *Diagnostic));
		const UE::Angelscript::Private::FAngelscriptBindCollection* Host =
			FAngelscriptBind::GetProcessHostCollection();
		ASSERT_THAT(IsNotNull(Host));
		const asCDefinitions* Graph = Host->GetHostDefinitions();
		ASSERT_THAT(IsNotNull(Graph));
		ASSERT_THAT(IsTrue(Graph->GetState() == asEDefinitionState::Frozen));
		const int32 Before = Host->GetCallbackInvocationCount(TEXT("FString.ExplicitBindings"));
		ASSERT_THAT(IsTrue(Before > 0));

		FAngelscriptBindingDiagnostic BindingDiagnostic;
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptEngine::CreateForBindings(*Host, BindingDiagnostic);
		ASSERT_THAT(IsNotNull(
			EngineA.Get(),
			BindingDiagnostic.Message.IsEmpty()
				? TEXT("<empty EngineA CreateForBindings diagnostic>")
				: *BindingDiagnostic.Message));
		ASSERT_THAT(IsNotNull(EngineA->Engine));
		const asERegistrationResult First = EngineA->Engine->InjectDefinitions(*Graph);
		ASSERT_THAT(IsTrue(
			First == asERegistrationResult::AlreadyRegistered || First == asERegistrationResult::Succeeded,
			*FString::Printf(TEXT("first inject=%d"), int32(First))));

		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptEngine::CreateForBindings(*Host, BindingDiagnostic);
		ASSERT_THAT(IsNotNull(
			EngineB.Get(),
			BindingDiagnostic.Message.IsEmpty()
				? TEXT("<empty EngineB CreateForBindings diagnostic>")
				: *BindingDiagnostic.Message));
		ASSERT_THAT(IsNotNull(EngineB->Engine));
		const asERegistrationResult Second = EngineB->Engine->InjectDefinitions(*Graph);
		ASSERT_THAT(IsTrue(
			Second == asERegistrationResult::AlreadyRegistered || Second == asERegistrationResult::Succeeded,
			*FString::Printf(TEXT("second inject=%d"), int32(Second))));
		ASSERT_THAT(AreEqual(Before, Host->GetCallbackInvocationCount(TEXT("FString.ExplicitBindings"))));

		FString WriteDiagnostic;
		const FString Body = FString::Printf(
			TEXT("| Field | Value |\n|---|---|\n| fstring_explicit_bindings | %d |\n| first_inject | %d |\n| second_inject | %d |\n%s"),
			Before,
			int32(First),
			int32(Second),
			*HostPerfIdentityRows());
		ASSERT_THAT(IsTrue(HostPerfWriteSection(TEXT("P2"), Body, WriteDiagnostic), *WriteDiagnostic));
	}

	TEST_METHOD(DestroyOneEngineSharedGraphSurvives)
	{
		FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptBind::EnsureProcessHostCollection(Diagnostic), *Diagnostic));
		const UE::Angelscript::Private::FAngelscriptBindCollection* Host =
			FAngelscriptBind::GetProcessHostCollection();
		ASSERT_THAT(IsNotNull(Host));
		const asCDefinitions* Graph = Host->GetHostDefinitions();
		ASSERT_THAT(IsNotNull(Graph));
		asITypeInfo* HostString = Graph->FindType("FString");
		ASSERT_THAT(IsNotNull(HostString));

		FAngelscriptBindingDiagnostic BindingDiagnostic;
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptEngine::CreateForBindings(*Host, BindingDiagnostic);
		ASSERT_THAT(IsNotNull(EngineA.Get(), *BindingDiagnostic.Message));
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptEngine::CreateForBindings(*Host, BindingDiagnostic);
		ASSERT_THAT(IsNotNull(EngineB.Get(), *BindingDiagnostic.Message));
		ASSERT_THAT(IsNotNull(EngineB->Engine));
		EngineA.Reset();

		int32 Check = 0;
		ASSERT_THAT(IsTrue(HostPerfRunCheck(*EngineB->Engine, Graph, Check), TEXT("Check after destroying A")));
		ASSERT_THAT(AreEqual(25, Check));
		ASSERT_THAT(IsTrue(EngineB->Engine->GetTypeInfoByName("FString") == HostString));

		FString WriteDiagnostic;
		const FString Body = FString::Printf(
			TEXT("| Field | Value |\n|---|---|\n| check | %d |\n| host_string_shared | 1 |\n%s"),
			Check,
			*HostPerfIdentityRows());
		ASSERT_THAT(IsTrue(HostPerfWriteSection(TEXT("P3"), Body, WriteDiagnostic), *WriteDiagnostic));
	}

	TEST_METHOD(HostCallThroughputSamples)
	{
		FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptBind::EnsureProcessHostCollection(Diagnostic), *Diagnostic));
		const UE::Angelscript::Private::FAngelscriptBindCollection* Host =
			FAngelscriptBind::GetProcessHostCollection();
		ASSERT_THAT(IsNotNull(Host));
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		TUniquePtr<FAngelscriptEngine> Owner = FAngelscriptEngine::CreateForBindings(*Host, BindingDiagnostic);
		ASSERT_THAT(IsNotNull(Owner.Get(), *BindingDiagnostic.Message));
		ASSERT_THAT(IsNotNull(Owner->Engine));
		asCObjectType* StringType = static_cast<asCObjectType*>(Owner->Engine->GetTypeInfoByName("FString"));
		asCObjectType* VectorType = static_cast<asCObjectType*>(Owner->Engine->GetTypeInfoByName("FVector"));
		asCScriptFunction* Len = HostPerfFindMethod(StringType, "Len", 0);
		asCScriptFunction* Add = HostPerfFindMethod(VectorType, "opAdd", 1);
		ASSERT_THAT(IsNotNull(Len));
		ASSERT_THAT(IsNotNull(Add));
		asIScriptContext* Context = Owner->Engine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ON_SCOPE_EXIT { Context->Release(); };

		TArray<double> LenMs;
		TArray<double> AddMs;
		LenMs.Reserve(10000);
		AddMs.Reserve(10000);
		int32 LastLen = 0;
		double LastX = 0.0;
		for (int32 Index = 0; Index < 10000; ++Index)
		{
			FString Text(TEXT("abc"));
			const double LenStart = FPlatformTime::Seconds();
			ASSERT_THAT(IsTrue(Context->Prepare(Len) >= 0));
			ASSERT_THAT(AreEqual(0, Context->SetObject(&Text)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			LastLen = static_cast<int32>(Context->GetReturnDWord());
			LenMs.Add((FPlatformTime::Seconds() - LenStart) * 1000.0);
			ASSERT_THAT(AreEqual(3, LastLen));

			FVector Left(1.0, 2.0, 3.0);
			FVector Right(4.0, 5.0, 6.0);
			const double AddStart = FPlatformTime::Seconds();
			ASSERT_THAT(IsTrue(Context->Prepare(Add) >= 0));
			ASSERT_THAT(AreEqual(0, Context->SetObject(&Left)));
			ASSERT_THAT(AreEqual(0, Context->SetArgObject(0, &Right)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			const FVector* Sum = static_cast<const FVector*>(Context->GetAddressOfReturnValue());
			ASSERT_THAT(IsNotNull(Sum));
			LastX = Sum->X;
			AddMs.Add((FPlatformTime::Seconds() - AddStart) * 1000.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(LastX, 5.0)));
		}

		const int32 Checksum = LastLen ^ static_cast<int32>(LastX);
		FString WriteDiagnostic;
		const FString Body = FString::Printf(
			TEXT("| Field | Value |\n|---|---|\n| len_median_ms | %.6f |\n| len_p95_ms | %.6f |\n| add_median_ms | %.6f |\n| add_p95_ms | %.6f |\n| checksum | %d |\n%s"),
			HostPerfPercentile(LenMs, 0.50),
			HostPerfPercentile(LenMs, 0.95),
			HostPerfPercentile(AddMs, 0.50),
			HostPerfPercentile(AddMs, 0.95),
			Checksum,
			*HostPerfIdentityRows());
		ASSERT_THAT(IsTrue(HostPerfWriteSection(TEXT("P4"), Body, WriteDiagnostic), *WriteDiagnostic));
	}
};

#endif
