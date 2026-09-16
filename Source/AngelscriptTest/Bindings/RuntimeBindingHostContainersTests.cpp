#include "CQTest.h"
#include "Containers/ScriptArray.h"
#include "Core/AngelscriptBindsInternal.h"
#include "as_callfunc.h"
#include "as_datatype.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 HostContainersArrayNum(FScriptArray* Self) { return Self ? Self->Num() : MIN_int32; }
	void HostContainersArrayAdd(FScriptArray* Self, int32 Value)
	{
		if (!Self) return;
		const int32 Index = Self->Add(1, sizeof(int32), alignof(int32));
		*reinterpret_cast<int32*>(static_cast<uint8*>(Self->GetData()) + (sizeof(int32) * Index)) = Value;
	}
	int32* HostContainersArrayIndex(FScriptArray* Self, int32 Index)
	{
		if (!Self || Index < 0 || Index >= Self->Num())
		{
			return nullptr;
		}
		return reinterpret_cast<int32*>(static_cast<uint8*>(Self->GetData()) + (sizeof(int32) * Index));
	}

	struct FHostContainersMap
	{
		TMap<int32, int32> Data;
	};

	struct FHostContainersStringMap
	{
		TMap<FString, int32> Data;
	};

	struct FHostContainersSet
	{
		TSet<int32> Data;
	};

	struct FHostContainersOptional
	{
		TOptional<int32> Data;
	};

	void HostContainersMapAdd(FHostContainersMap* Self, int32 Key, int32 Value)
	{
		if (Self) Self->Data.Add(Key, Value);
	}
	int32 HostContainersMapFind(FHostContainersMap* Self, int32 Key)
	{
		return Self ? Self->Data.FindRef(Key) : 0;
	}
	void HostContainersStringMapAdd(FHostContainersStringMap* Self, const FString& Key, int32 Value)
	{
		if (Self) Self->Data.Add(Key, Value);
	}
	int32 HostContainersStringMapFind(FHostContainersStringMap* Self, const FString& Key)
	{
		return Self ? Self->Data.FindRef(Key) : 0;
	}
	void HostContainersSetAdd(FHostContainersSet* Self, int32 Value)
	{
		if (Self) Self->Data.Add(Value);
	}
	int32 HostContainersSetNum(FHostContainersSet* Self)
	{
		return Self ? Self->Data.Num() : MIN_int32;
	}
	void HostContainersOptionalSet(FHostContainersOptional* Self, int32 Value)
	{
		if (Self) Self->Data = Value;
	}
	int32 HostContainersOptionalGet(FHostContainersOptional* Self)
	{
		return Self ? Self->Data.Get(MIN_int32) : MIN_int32;
	}

	void HostContainersRegisterMembers(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds IntArray = Binds.SpecializeTemplateForTarget("TArray", "int");
		IntArray.Method("int Num() const", &HostContainersArrayNum);
		IntArray.Method("void Add(int Value)", &HostContainersArrayAdd);
		IntArray.Method("int& opIndex(int Index)", &HostContainersArrayIndex);
		FBindFlags Flags;
		Flags.bPOD = true;
		FAngelscriptBinds Map = Binds.ValueClassForTarget<FHostContainersMap>("HostMap", Flags);
		Map.Method("void Add(int Key, int Value)", &HostContainersMapAdd);
		Map.Method("int Find(int Key) const", &HostContainersMapFind);
		FAngelscriptBinds StringMap = Binds.ValueClassForTarget<FHostContainersStringMap>("HostStringMap", Flags);
		StringMap.Method("void Add(const FString& Key, int Value)", &HostContainersStringMapAdd);
		StringMap.Method("int Find(const FString& Key) const", &HostContainersStringMapFind);
		FAngelscriptBinds Set = Binds.ValueClassForTarget<FHostContainersSet>("HostSet", Flags);
		Set.Method("void Add(int Value)", &HostContainersSetAdd);
		Set.Method("int Num() const", &HostContainersSetNum);
		FAngelscriptBinds Optional = Binds.ValueClassForTarget<FHostContainersOptional>("HostOptional", Flags);
		Optional.Method("void Set(int Value)", &HostContainersOptionalSet);
		Optional.Method("int Get() const", &HostContainersOptionalGet);
	}

	void HostContainersRegisterMissing(FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionDirectForTarget("void Missing()", asSFuncPtr(), asCALL_CDECL, ASAutoCaller::FunctionCaller::Make());
	}

	FAngelscriptBindRecord HostContainersMakeRecord(FName BindName, EAngelscriptBindPhase Phase, FAngelscriptBindCallback Callback, int32 Line)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("AngelscriptTest");
		Record.BindName = BindName;
		Record.Phase = Phase;
		Record.SourceFile = "RuntimeBindingHostContainersTests.cpp";
		Record.SourceLine = Line;
		Record.Callback = Callback;
		return Record;
	}

	struct FHostContainersEngineOwner
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FHostContainersEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
	};

	bool HostContainersAppendNamed(UE::Angelscript::Private::FAngelscriptBindCollection& Collection, FName BindName, FString& Diagnostic)
	{
		for (const FAngelscriptBindRecord& Record : FAngelscriptBind::GetRegisteredCollection().GetRecords())
		{
			if (Record.BindName == BindName) return Collection.Append(Record, Diagnostic);
		}
		Diagnostic = FString::Printf(TEXT("missing production bind %s"), *BindName.ToString());
		return false;
	}

	bool HostContainersAppendProduction(UE::Angelscript::Private::FAngelscriptBindCollection& Collection, FString& Diagnostic)
	{
		return HostContainersAppendNamed(Collection, TEXT("TArray.Declaration"), Diagnostic)
			&& HostContainersAppendNamed(Collection, TEXT("TMap.Declaration"), Diagnostic)
			&& HostContainersAppendNamed(Collection, TEXT("TSet.Declaration"), Diagnostic)
			&& HostContainersAppendNamed(Collection, TEXT("TOptional.Declaration"), Diagnostic)
			&& HostContainersAppendNamed(Collection, TEXT("FString.TypeDeclarations"), Diagnostic)
			&& Collection.Append(HostContainersMakeRecord(TEXT("HostContainers.Members"), EAngelscriptBindPhase::ExplicitBindings, &HostContainersRegisterMembers, 10), Diagnostic);
	}

	asCScriptFunction* HostContainersFindMethod(asCObjectType* Type, const char* Name)
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

	void HostContainersCall(asCScriptFunction& Function, void** Args, void* Return)
	{
		const auto Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->caller.IsBound()) return;
		if (Interface->caller.type == 2) Interface->caller.MethodCaller(Interface->method, Args, Return);
		else Interface->caller.FunctionCaller(Interface->func, Args, Return);
	}

	struct FHostContainersSite
	{
		const TCHAR* Source;
		const TCHAR* Symbol;
		const TCHAR* Evidence;
		const TCHAR* Reason;
	};

	const FHostContainersSite GHostContainersSites[] = {
#include "RuntimeBindingHostContainersSites.inl"
	};

	asCObjectType* HostContainersIntArray(asCDefinitions& Graph)
	{
		asCObjectType* Template = static_cast<asCObjectType*>(Graph.FindType("TArray"));
		if (!Template) return nullptr;
		asCDataType IntType = Graph.Primitive(ttInt);
		return Graph.FindTemplateInstance(Template, MakeArrayView(&IntType, 1));
	}
}

TEST_CLASS_WITH_FLAGS(HostContainers, "Angelscript.UnitTest.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ContainersBoundBehavior)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostContainersAppendProduction(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		const bool bHostReady = Collection.ExecuteToHost(Diagnostic);
		ASSERT_THAT(IsTrue(bHostReady, Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		asCDefinitions* Graph = const_cast<asCDefinitions*>(Collection.GetHostDefinitions());
		asCObjectType* ArrayType = HostContainersIntArray(*Graph);
		asCObjectType* MapType = static_cast<asCObjectType*>(Graph->FindType("HostMap"));
		asCObjectType* SetType = static_cast<asCObjectType*>(Graph->FindType("HostSet"));
		asCObjectType* OptionalType = static_cast<asCObjectType*>(Graph->FindType("HostOptional"));
		ASSERT_THAT(IsNotNull(ArrayType));
		ASSERT_THAT(IsNotNull(MapType));
		ASSERT_THAT(IsNotNull(SetType));
		ASSERT_THAT(IsNotNull(OptionalType));
		asCScriptFunction* ArrayAdd = HostContainersFindMethod(ArrayType, "Add");
		asCScriptFunction* ArrayNum = HostContainersFindMethod(ArrayType, "Num");
		asCScriptFunction* MapAdd = HostContainersFindMethod(MapType, "Add");
		asCScriptFunction* MapFind = HostContainersFindMethod(MapType, "Find");
		asCScriptFunction* SetAdd = HostContainersFindMethod(SetType, "Add");
		asCScriptFunction* SetNum = HostContainersFindMethod(SetType, "Num");
		asCScriptFunction* OptSet = HostContainersFindMethod(OptionalType, "Set");
		asCScriptFunction* OptGet = HostContainersFindMethod(OptionalType, "Get");
		FScriptArray Array;
		int32 Two = 2, Five = 5;
		void* Add2[2] = { &Array, &Two };
		void* Add5[2] = { &Array, &Five };
		HostContainersCall(*ArrayAdd, Add2, nullptr);
		HostContainersCall(*ArrayAdd, Add5, nullptr);
		int32 ArrayCount = 0;
		void* ArraySelf[1] = { &Array };
		HostContainersCall(*ArrayNum, ArraySelf, &ArrayCount);
		ASSERT_THAT(AreEqual(2, ArrayCount));
		FHostContainersMap Map;
		int32 Key = 3, Value = 7;
		void* MapAddArgs[3] = { &Map, &Key, &Value };
		HostContainersCall(*MapAdd, MapAddArgs, nullptr);
		int32 Found = 0;
		void* MapFindArgs[2] = { &Map, &Key };
		HostContainersCall(*MapFind, MapFindArgs, &Found);
		ASSERT_THAT(AreEqual(7, Found));
		FHostContainersSet Set;
		int32 Nine = 9;
		void* SetAddArgs[2] = { &Set, &Nine };
		HostContainersCall(*SetAdd, SetAddArgs, nullptr);
		HostContainersCall(*SetAdd, SetAddArgs, nullptr);
		int32 SetCount = 0;
		void* SetSelf[1] = { &Set };
		HostContainersCall(*SetNum, SetSelf, &SetCount);
		ASSERT_THAT(AreEqual(1, SetCount));
		FHostContainersOptional Optional;
		int32 Eleven = 11;
		void* OptSetArgs[2] = { &Optional, &Eleven };
		HostContainersCall(*OptSet, OptSetArgs, nullptr);
		int32 OptValue = 0;
		void* OptSelf[1] = { &Optional };
		HostContainersCall(*OptGet, OptSelf, &OptValue);
		ASSERT_THAT(AreEqual(11, OptValue));
		Array.Empty(0, sizeof(int32), alignof(int32));
		FHostContainersEngineOwner A;
		FHostContainersEngineOwner B;
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("HostContainers.Members"))));
	}

	TEST_METHOD(TArrayAddThenNumIsOne)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostContainersAppendProduction(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		const bool bHostReady = Collection.ExecuteToHost(Diagnostic);
		ASSERT_THAT(IsTrue(bHostReady, Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		asCObjectType* ArrayType = HostContainersIntArray(*const_cast<asCDefinitions*>(Collection.GetHostDefinitions()));
		asCScriptFunction* ArrayAdd = HostContainersFindMethod(ArrayType, "Add");
		asCScriptFunction* ArrayNum = HostContainersFindMethod(ArrayType, "Num");
		asCScriptFunction* ArrayIndex = HostContainersFindMethod(ArrayType, "opIndex");
		ASSERT_THAT(IsNotNull(ArrayAdd));
		ASSERT_THAT(IsNotNull(ArrayNum));
		ASSERT_THAT(IsNotNull(ArrayIndex));
		FScriptArray Array;
		int32 Four = 4;
		void* AddArgs[2] = { &Array, &Four };
		HostContainersCall(*ArrayAdd, AddArgs, nullptr);
		int32 Count = 0;
		void* Self[1] = { &Array };
		HostContainersCall(*ArrayNum, Self, &Count);
		ASSERT_THAT(AreEqual(1, Count));
		int32 Zero = 0;
		int32* Slot = nullptr;
		void* IndexArgs[2] = { &Array, &Zero };
		HostContainersCall(*ArrayIndex, IndexArgs, &Slot);
		ASSERT_THAT(IsNotNull(Slot));
		ASSERT_THAT(AreEqual(4, *Slot));
		Array.Empty(0, sizeof(int32), alignof(int32));
	}

	TEST_METHOD(TMapAddThenFind)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostContainersAppendProduction(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		const bool bHostReady = Collection.ExecuteToHost(Diagnostic);
		ASSERT_THAT(IsTrue(bHostReady, Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		asCObjectType* MapType = static_cast<asCObjectType*>(Collection.GetHostDefinitions()->FindType("HostStringMap"));
		asCScriptFunction* MapAdd = HostContainersFindMethod(MapType, "Add");
		asCScriptFunction* MapFind = HostContainersFindMethod(MapType, "Find");
		ASSERT_THAT(IsNotNull(MapAdd));
		ASSERT_THAT(IsNotNull(MapFind));
		FHostContainersStringMap Map;
		FString Key(TEXT("k"));
		int32 Value = 7;
		void* AddArgs[3] = { &Map, &Key, &Value };
		HostContainersCall(*MapAdd, AddArgs, nullptr);
		int32 Found = 0;
		void* FindArgs[2] = { &Map, &Key };
		HostContainersCall(*MapFind, FindArgs, &Found);
		ASSERT_THAT(AreEqual(7, Found));
	}

	TEST_METHOD(CompleteFamilyAccounting)
	{
		ASSERT_THAT(AreEqual(16, static_cast<int32>(UE_ARRAY_COUNT(GHostContainersSites))));
		int32 Executable = 0, MetadataOnly = 0, Companion = 0;
		for (const FHostContainersSite& Site : GHostContainersSites)
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
		ASSERT_THAT(AreEqual(4, Executable));
		ASSERT_THAT(AreEqual(8, MetadataOnly));
		ASSERT_THAT(AreEqual(4, Companion));
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostContainersAppendProduction(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsNotNull(Collection.GetHostDefinitions()->FindType("TArray")));
		ASSERT_THAT(IsNotNull(Collection.GetHostDefinitions()->FindType("TMap")));
		ASSERT_THAT(IsNotNull(Collection.GetHostDefinitions()->FindType("TSet")));
		ASSERT_THAT(IsNotNull(Collection.GetHostDefinitions()->FindType("TOptional")));
		const int32 Before = Collection.GetCallbackInvocationCount(TEXT("TArray.Declaration"));
		FHostContainersEngineOwner A;
		FHostContainersEngineOwner B;
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(AreEqual(Before, Collection.GetCallbackInvocationCount(TEXT("TArray.Declaration"))));
	}

	TEST_METHOD(OwnerAndFailureBoundaries)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Healthy;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostContainersAppendProduction(Healthy, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.ExecuteToHost(Diagnostic), *Diagnostic));
		asCScriptFunction* SetNum = HostContainersFindMethod(static_cast<asCObjectType*>(Healthy.GetHostDefinitions()->FindType("HostSet")), "Num");
		FHostContainersSet Set;
		int32 Nine = 9;
		asCScriptFunction* SetAdd = HostContainersFindMethod(static_cast<asCObjectType*>(Healthy.GetHostDefinitions()->FindType("HostSet")), "Add");
		void* AddArgs[2] = { &Set, &Nine };
		HostContainersCall(*SetAdd, AddArgs, nullptr);
		int32 Count = 0;
		void* Self[1] = { &Set };
		HostContainersCall(*SetNum, Self, &Count);
		ASSERT_THAT(AreEqual(1, Count));
		UE::Angelscript::Private::FAngelscriptBindCollection Failed;
		ASSERT_THAT(IsTrue(Failed.Append(HostContainersMakeRecord(TEXT("HostContainers.Missing"), EAngelscriptBindPhase::ExplicitBindings, &HostContainersRegisterMissing, 40), Diagnostic)));
		ASSERT_THAT(IsTrue(Failed.Finalize(Diagnostic)));
		ASSERT_THAT(IsFalse(Failed.ExecuteToHost(Diagnostic)));
		HostContainersCall(*SetNum, Self, &Count);
		ASSERT_THAT(AreEqual(1, Count));
	}
};

#endif
