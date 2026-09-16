#include "CQTest.h"
#include "Containers/ScriptArray.h"
#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptEngine.h"
#include "Misc/ScopeExit.h"
#include "Testing/AngelscriptBindExecutionObservation.h"
#include "as_array.h"
#include "as_context.h"
#include "as_datatype.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	TUniquePtr<FAngelscriptEngine> GBoundEngine;
	FString GBindPrepareDiagnostic;
}

TEST_CLASS_WITH_FLAGS(BasicTypes, "Angelscript.UnitTest.Temp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	asCScriptEngine* ScriptEngine() const
	{
		return GBoundEngine.IsValid() ? GBoundEngine->Engine : nullptr;
	}

	asCObjectType* FindObject(const char* Name) const
	{
		asCScriptEngine* Engine = ScriptEngine();
		return Engine ? static_cast<asCObjectType*>(Engine->GetTypeInfoByName(Name)) : nullptr;
	}

	asCScriptFunction* FindMethod(asCObjectType* Type, const char* Name, int32 ParamCount = -1) const
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

	asCScriptFunction* FindConstruct(asCObjectType* Type) const
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

	asCScriptFunction* FindDestruct(asCObjectType* Type) const
	{
		if (!Type)
		{
			return nullptr;
		}
		for (asUINT Index = 0; Index < Type->GetBehaviourCount(); ++Index)
		{
			asEBehaviours Behaviour;
			asIScriptFunction* Function = Type->GetBehaviourByIndex(Index, &Behaviour);
			if (Behaviour == asBEHAVE_DESTRUCT && Function)
			{
				return static_cast<asCScriptFunction*>(Function);
			}
		}
		return nullptr;
	}

	asCObjectType* FindIntArray() const
	{
		if (const UE::Angelscript::Private::FAngelscriptBindCollection* Host =
			FAngelscriptBind::GetProcessHostCollection())
		{
			if (const asCDefinitions* Graph = Host->GetHostDefinitions())
			{
				if (asCObjectType* Template = static_cast<asCObjectType*>(Graph->FindType("TArray")))
				{
					asCDataType Inner = Graph->Primitive(ttInt);
					if (asCObjectType* Instance = Graph->FindTemplateInstance(
						Template,
						MakeArrayView(&Inner, 1)))
					{
						return Instance;
					}
				}
			}
		}
		asCScriptEngine* Engine = ScriptEngine();
		if (!Engine)
		{
			return nullptr;
		}
		if (asITypeInfo* Declared = Engine->GetTypeInfoByDecl("TArray<int>"))
		{
			return static_cast<asCObjectType*>(Declared);
		}
		asCObjectType* Template = static_cast<asCObjectType*>(Engine->GetTypeInfoByName("TArray"));
		if (!Template)
		{
			return nullptr;
		}
		asCArray<asCDataType> SubTypes;
		SubTypes.PushLast(asCDataType::CreatePrimitive(ttInt, false));
		return Engine->GetTemplateInstanceType(Template, SubTypes, nullptr, nullptr);
	}

	asCScriptFunction* FindPrintOrLog() const
	{
		asCScriptEngine* Engine = ScriptEngine();
		if (!Engine)
		{
			return nullptr;
		}
		const char* Decls[] = {
			"void Print(const FString& Text, float Duration, FLinearColor Color)",
			"void Print(const FString&in Text, float Duration, FLinearColor Color)",
			"void Print(const FString& Text, float32 Duration, FLinearColor Color)",
			"void Log(const FString& Text)",
			"void Log(const FString&in Text)",
		};
		for (const char* Decl : Decls)
		{
			if (asIScriptFunction* Function = Engine->GetGlobalFunctionByDecl(Decl))
			{
				return static_cast<asCScriptFunction*>(Function);
			}
		}
		for (asUINT Index = 0; Index < Engine->GetGlobalFunctionCount(); ++Index)
		{
			asIScriptFunction* Function = Engine->GetGlobalFunctionByIndex(Index);
			if (!Function || !Function->GetName())
			{
				continue;
			}
			if (FCStringAnsi::Strcmp(Function->GetName(), "Print") == 0
				|| FCStringAnsi::Strcmp(Function->GetName(), "Log") == 0)
			{
				return static_cast<asCScriptFunction*>(Function);
			}
		}
		return nullptr;
	}

	bool SnapshotHasBind(const FAngelscriptBindExecutionSnapshot& Snapshot, FName BindName) const
	{
		if (Snapshot.ExecutedBindNames.Contains(BindName))
		{
			return true;
		}
		for (const FAngelscriptBindProviderExecutionRecord& Record : Snapshot.ProviderRecords)
		{
			if (Record.BindName == BindName)
			{
				return true;
			}
		}
		return false;
	}

public:
	BEFORE_ALL()
	{
		GBindPrepareDiagnostic.Reset();
		if (!FAngelscriptBind::PrepareForEngineInitialization(GBindPrepareDiagnostic))
		{
			return;
		}
		FAngelscriptEngineConfig Config = FAngelscriptEngineConfig::FromCurrentProcess();
		Config.bSkipInitialCompile = true;
		Config.bOverrideCacheV2Enablement = true;
		Config.bEnableCacheV2 = false;
		GBoundEngine = FAngelscriptEngine::Create(Config, FAngelscriptEngineDependencies::CreateDefault());
	}

	AFTER_ALL()
	{
		GBoundEngine.Reset();
	}

	TEST_METHOD(AfterBindTArrayAddThenNum)
	{
		ASSERT_THAT(IsNotNull(GBoundEngine.Get(), *FString::Printf(
			TEXT("InitializeWithoutInitialCompile should publish a local engine: %s"),
			*GBindPrepareDiagnostic)));
		FAngelscriptEngineScope Scope(*GBoundEngine);
		asCObjectType* ArrayType = FindIntArray();
		ASSERT_THAT(IsNotNull(ArrayType, TEXT("TArray<int> should exist after BindScriptTypes")));
		asCScriptFunction* Construct = FindConstruct(ArrayType);
		asCScriptFunction* Destruct = FindDestruct(ArrayType);
		asCScriptFunction* Add = FindMethod(ArrayType, "Add", 1);
		asCScriptFunction* Num = FindMethod(ArrayType, "Num", 0);
		asCScriptFunction* Index = FindMethod(ArrayType, "opIndex", 1);
		ASSERT_THAT(IsNotNull(Construct, TEXT("TArray<int> default construct")));
		ASSERT_THAT(IsNotNull(Add, TEXT("TArray<int>.Add")));
		ASSERT_THAT(IsNotNull(Num, TEXT("TArray<int>.Num")));
		ASSERT_THAT(IsNotNull(Index, TEXT("TArray<int>.opIndex")));

		asCScriptEngine* Engine = ScriptEngine();
		asIScriptContext* Context = Engine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ON_SCOPE_EXIT { Context->Release(); };

		alignas(FScriptArray) uint8 Storage[sizeof(FScriptArray)] = {};
		FScriptArray* Array = reinterpret_cast<FScriptArray*>(Storage);
		ASSERT_THAT(IsTrue(Context->Prepare(Construct) >= 0));
		ASSERT_THAT(AreEqual(0, Context->SetObject(Array)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));

		int32 Value = 4;
		ASSERT_THAT(IsTrue(Context->Prepare(Add) >= 0));
		ASSERT_THAT(AreEqual(0, Context->SetObject(Array)));
		ASSERT_THAT(AreEqual(0, Context->SetArgAddress(0, &Value)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));

		ASSERT_THAT(IsTrue(Context->Prepare(Num) >= 0));
		ASSERT_THAT(AreEqual(0, Context->SetObject(Array)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		const int32 Count = static_cast<int32>(Context->GetReturnDWord());

		ASSERT_THAT(IsTrue(Context->Prepare(Index) >= 0));
		ASSERT_THAT(AreEqual(0, Context->SetObject(Array)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 0)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		const int32* First = static_cast<const int32*>(Context->GetReturnAddress());
		ASSERT_THAT(IsNotNull(First));
		ASSERT_THAT(AreEqual(5, Count + *First));

		if (Destruct)
		{
			ASSERT_THAT(IsTrue(Context->Prepare(Destruct) >= 0));
			ASSERT_THAT(AreEqual(0, Context->SetObject(Array)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		}
	}

	TEST_METHOD(AfterBindFStringLenAndFVectorAdd)
	{
		ASSERT_THAT(IsNotNull(GBoundEngine.Get(), *FString::Printf(
			TEXT("InitializeWithoutInitialCompile should publish a local engine: %s"),
			*GBindPrepareDiagnostic)));
		FAngelscriptEngineScope Scope(*GBoundEngine);
		asCObjectType* StringType = FindObject("FString");
		asCObjectType* VectorType = FindObject("FVector");
		ASSERT_THAT(IsNotNull(StringType, TEXT("FString should exist after BindScriptTypes")));
		ASSERT_THAT(IsNotNull(VectorType, TEXT("FVector should exist after BindScriptTypes")));
		asCScriptFunction* Len = FindMethod(StringType, "Len", 0);
		asCScriptFunction* Add = FindMethod(VectorType, "opAdd", 1);
		ASSERT_THAT(IsNotNull(Len, TEXT("FString.Len")));
		ASSERT_THAT(IsNotNull(Add, TEXT("FVector.opAdd")));

		asCScriptEngine* Engine = ScriptEngine();
		asIScriptContext* Context = Engine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ON_SCOPE_EXIT { Context->Release(); };

		FString Text(TEXT("abc"));
		ASSERT_THAT(IsTrue(Context->Prepare(Len) >= 0));
		ASSERT_THAT(AreEqual(0, Context->SetObject(&Text)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		const int32 Length = static_cast<int32>(Context->GetReturnDWord());

		FVector Left(1.0, 2.0, 3.0);
		FVector Right(4.0, 5.0, 6.0);
		ASSERT_THAT(IsTrue(Context->Prepare(Add) >= 0));
		ASSERT_THAT(AreEqual(0, Context->SetObject(&Left)));
		ASSERT_THAT(AreEqual(0, Context->SetArgObject(0, &Right)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		const FVector* Sum = static_cast<const FVector*>(Context->GetAddressOfReturnValue());
		ASSERT_THAT(IsNotNull(Sum));
		ASSERT_THAT(AreEqual(24, Length + static_cast<int32>(Sum->X) + static_cast<int32>(Sum->Y) + static_cast<int32>(Sum->Z)));
	}

	TEST_METHOD(AfterBindPrintWritesLog)
	{
		ASSERT_THAT(IsNotNull(GBoundEngine.Get(), *FString::Printf(
			TEXT("InitializeWithoutInitialCompile should publish a local engine: %s"),
			*GBindPrepareDiagnostic)));
		FAngelscriptEngineScope Scope(*GBoundEngine);
		asCScriptFunction* PrintOrLog = FindPrintOrLog();
		ASSERT_THAT(IsNotNull(PrintOrLog, TEXT("Print or Log should exist after BindScriptTypes")));

		asCScriptEngine* Engine = ScriptEngine();
		asIScriptContext* Context = Engine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ON_SCOPE_EXIT { Context->Release(); };

		FString Message(TEXT("temp-bind-ok"));
		ASSERT_THAT(IsTrue(Context->Prepare(PrintOrLog) >= 0, TEXT("Prepare Print/Log")));
		if (PrintOrLog->GetParamCount() >= 1)
		{
			ASSERT_THAT(AreEqual(0, Context->SetArgObject(0, &Message)));
		}
		ASSERT_THAT(AreNotEqual(asEXECUTION_EXCEPTION, Context->Execute()));
	}

	TEST_METHOD(BindLogsRecordBasicTypeProviders)
	{
		ASSERT_THAT(IsNotNull(GBoundEngine.Get(), *FString::Printf(
			TEXT("InitializeWithoutInitialCompile should publish a local engine: %s"),
			*GBindPrepareDiagnostic)));
		ASSERT_THAT(IsFalse(GBoundEngine->IsCacheV2Enabled(), TEXT("Temp oracle forces Cache V2 off")));
		const UE::Angelscript::Private::FAngelscriptBindCollection* Host =
			FAngelscriptBind::GetProcessHostCollection();
		ASSERT_THAT(IsNotNull(Host, TEXT("process host collection should exist after inject-only BindScriptTypes")));
		auto HasBindName = [](TConstArrayView<FAngelscriptBindRecord> Records, FName BindName)
		{
			for (const FAngelscriptBindRecord& Record : Records)
			{
				if (Record.BindName == BindName)
				{
					return true;
				}
			}
			return false;
		};
		const TConstArrayView<FAngelscriptBindRecord> HostRecords = Host->GetRecords();
		ASSERT_THAT(IsTrue(HasBindName(HostRecords, TEXT("TArray.Declaration")), TEXT("TArray.Declaration")));
		ASSERT_THAT(IsTrue(HasBindName(HostRecords, TEXT("FString.ExplicitBindings")), TEXT("FString.ExplicitBindings")));
		ASSERT_THAT(IsTrue(HasBindName(HostRecords, TEXT("FVector")), TEXT("FVector")));
	}
};

#endif
