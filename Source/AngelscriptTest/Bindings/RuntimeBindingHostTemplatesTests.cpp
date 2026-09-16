#include "CQTest.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Containers/ScriptArray.h"
#include "Math/Vector.h"
#include "as_callfunc.h"
#include "as_datatype.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 HostTemplatesArrayNum(FScriptArray* Self)
	{
		return Self ? Self->Num() : MIN_int32;
	}

	void HostTemplatesArrayAddInt(FScriptArray* Self, int32 Value)
	{
		if (!Self) return;
		const int32 Index = Self->Add(1, sizeof(int32), alignof(int32));
		*reinterpret_cast<int32*>(static_cast<uint8*>(Self->GetData()) + (sizeof(int32) * Index)) = Value;
	}

	void HostTemplatesArrayAddVector(FScriptArray* Self, const FVector& Value)
	{
		if (!Self) return;
		const int32 Index = Self->Add(1, sizeof(FVector), alignof(FVector));
		*reinterpret_cast<FVector*>(static_cast<uint8*>(Self->GetData()) + (sizeof(FVector) * Index)) = Value;
	}

	struct FHostTemplatesPair
	{
		int32 X = 0;
		int32 Y = 0;
	};

	void HostTemplatesRegisterContainers(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds IntArray = Binds.SpecializeTemplateForTarget("TArray", "int");
		IntArray.Method("int Num() const", &HostTemplatesArrayNum);
		IntArray.Method("void Add(int Value)", &HostTemplatesArrayAddInt);
		FAngelscriptBinds VectorArray = Binds.SpecializeTemplateForTarget("TArray", "FVector");
		VectorArray.Method("int Num() const", &HostTemplatesArrayNum);
		VectorArray.Method("void Add(const FVector&in Value)", &HostTemplatesArrayAddVector);
		Binds.SpecializeTemplateForTarget("TArray", "TArray<int>");
	}

	bool HostTemplatesAppendNamed(
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

	FAngelscriptBindRecord HostTemplatesMakeRecord(FName BindName, EAngelscriptBindPhase Phase, FAngelscriptBindCallback Callback, int32 Line)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("AngelscriptTest");
		Record.BindName = BindName;
		Record.Phase = Phase;
		Record.SourceFile = "RuntimeBindingHostTemplatesTests.cpp";
		Record.SourceLine = Line;
		Record.Callback = Callback;
		return Record;
	}

	struct FHostTemplatesEngineOwner
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FHostTemplatesEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
	};

	asCScriptFunction* FindHostMethod(asCObjectType* Type, const char* Name)
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

	int32 CallHostNum(asCScriptFunction& Function, FScriptArray& Array)
	{
		const auto Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->caller.IsBound()) return MIN_int32;
		int32 Result = 0;
		void* Args[1] = { &Array };
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

	void CallHostAddInt(asCScriptFunction& Function, FScriptArray& Array, int32 Value)
	{
		const auto Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->caller.IsBound()) return;
		void* Args[2] = { &Array, &Value };
		if (Interface->caller.type == 2)
		{
			Interface->caller.MethodCaller(Interface->method, Args, nullptr);
		}
		else
		{
			Interface->caller.FunctionCaller(Interface->func, Args, nullptr);
		}
	}

	void CallHostAddVector(asCScriptFunction& Function, FScriptArray& Array, const FVector& Value)
	{
		const auto Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->caller.IsBound()) return;
		void* Args[2] = { &Array, const_cast<FVector*>(&Value) };
		if (Interface->caller.type == 2)
		{
			Interface->caller.MethodCaller(Interface->method, Args, nullptr);
		}
		else
		{
			Interface->caller.FunctionCaller(Interface->func, Args, nullptr);
		}
	}

	asCObjectType* HostArrayInstance(asCDefinitions& Graph, const char* Argument)
	{
		asCObjectType* Template = static_cast<asCObjectType*>(Graph.FindType("TArray"));
		if (!Template) return nullptr;
		if (FCStringAnsi::Strcmp(Argument, "int") == 0)
		{
			asCDataType IntType = Graph.Primitive(ttInt);
			return Graph.FindTemplateInstance(Template, MakeArrayView(&IntType, 1));
		}
		if (FCStringAnsi::Strcmp(Argument, "FVector") == 0)
		{
			asCTypeInfo* Vector = Graph.FindType("FVector");
			if (!Vector) return nullptr;
			asCDataType VectorType = asCDataType::CreateType(Vector, false);
			return Graph.FindTemplateInstance(Template, MakeArrayView(&VectorType, 1));
		}
		if (FCStringAnsi::Strcmp(Argument, "TArray<int>") == 0)
		{
			asCDataType IntType = Graph.Primitive(ttInt);
			asCObjectType* Inner = Graph.FindTemplateInstance(Template, MakeArrayView(&IntType, 1));
			if (!Inner) return nullptr;
			asCDataType InnerType = asCDataType::CreateType(Inner, false);
			return Graph.FindTemplateInstance(Template, MakeArrayView(&InnerType, 1));
		}
		return nullptr;
	}

	bool BuildHostTemplates(UE::Angelscript::Private::FAngelscriptBindCollection& Collection, FString& Diagnostic)
	{
		if (!HostTemplatesAppendNamed(Collection, TEXT("TArray.Declaration"), Diagnostic))
		{
			if (Diagnostic.IsEmpty()) Diagnostic = TEXT("append TArray.Declaration failed");
			return false;
		}
		if (!HostTemplatesAppendNamed(Collection, TEXT("FVector.TypeDeclarations"), Diagnostic))
		{
			if (Diagnostic.IsEmpty()) Diagnostic = TEXT("append FVector.TypeDeclarations failed");
			return false;
		}
		if (!Collection.Append(HostTemplatesMakeRecord(TEXT("HostTemplates"), EAngelscriptBindPhase::ExplicitBindings,
				&HostTemplatesRegisterContainers, 10), Diagnostic))
		{
			if (Diagnostic.IsEmpty()) Diagnostic = TEXT("append HostTemplates members failed");
			return false;
		}
		if (!Collection.Finalize(Diagnostic))
		{
			if (Diagnostic.IsEmpty()) Diagnostic = TEXT("finalize HostTemplates failed");
			return false;
		}
		if (!Collection.ExecuteToHost(Diagnostic))
		{
			if (Diagnostic.IsEmpty()) Diagnostic = TEXT("execute HostTemplates failed");
			return false;
		}
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(HostTemplates, "Angelscript.UnitTest.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(HostClosedArray)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		const bool bHostReady = BuildHostTemplates(Collection, Diagnostic);
		ASSERT_THAT(IsTrue(bHostReady, Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		asCDefinitions* Graph = const_cast<asCDefinitions*>(Collection.GetHostDefinitions());
		ASSERT_THAT(IsNotNull(Graph));
		asCObjectType* IntArray = HostArrayInstance(*Graph, "int");
		asCObjectType* VectorArray = HostArrayInstance(*Graph, "FVector");
		ASSERT_THAT(IsNotNull(IntArray));
		ASSERT_THAT(IsNotNull(VectorArray));
		ASSERT_THAT(IsTrue(IntArray->GetTypeInfoKind() == asETypeInfoKind::HostProcess));
		ASSERT_THAT(IsTrue(VectorArray->GetTypeInfoKind() == asETypeInfoKind::HostProcess));
		asCScriptFunction* IntAdd = FindHostMethod(IntArray, "Add");
		asCScriptFunction* IntNum = FindHostMethod(IntArray, "Num");
		asCScriptFunction* VectorAdd = FindHostMethod(VectorArray, "Add");
		asCScriptFunction* VectorNum = FindHostMethod(VectorArray, "Num");
		ASSERT_THAT(IsNotNull(IntAdd));
		ASSERT_THAT(IsNotNull(IntNum));
		ASSERT_THAT(IsNotNull(VectorAdd));
		ASSERT_THAT(IsNotNull(VectorNum));
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		FHostTemplatesEngineOwner A;
		FHostTemplatesEngineOwner B;
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		asCDataType IntType = Graph->Primitive(ttInt);
		asCDataType VectorType = asCDataType::CreateType(Graph->FindType("FVector"), false);
		asCObjectType* Template = static_cast<asCObjectType*>(Graph->FindType("TArray"));
		ASSERT_THAT(IsTrue(A.Ptr->FindTemplateInstance(Template, MakeArrayView(&IntType, 1)) == IntArray));
		ASSERT_THAT(IsTrue(B.Ptr->FindTemplateInstance(Template, MakeArrayView(&IntType, 1)) == IntArray));
		ASSERT_THAT(IsTrue(A.Ptr->FindTemplateInstance(Template, MakeArrayView(&VectorType, 1)) == VectorArray));
		ASSERT_THAT(IsTrue(B.Ptr->FindTemplateInstance(Template, MakeArrayView(&VectorType, 1)) == VectorArray));
		FScriptArray IntsA;
		FScriptArray IntsB;
		CallHostAddInt(*IntAdd, IntsA, 1);
		CallHostAddInt(*IntAdd, IntsA, 2);
		for (int32 Index = 0; Index < 5; ++Index)
		{
			CallHostAddInt(*IntAdd, IntsB, Index);
		}
		ASSERT_THAT(AreEqual(2, CallHostNum(*IntNum, IntsA)));
		ASSERT_THAT(AreEqual(5, CallHostNum(*IntNum, IntsB)));
		FScriptArray Vectors;
		CallHostAddVector(*VectorAdd, Vectors, FVector(1.0, 2.0, 3.0));
		CallHostAddVector(*VectorAdd, Vectors, FVector(4.0, 5.0, 6.0));
		ASSERT_THAT(AreEqual(2, CallHostNum(*VectorNum, Vectors)));
		IntsA.Empty(0, sizeof(int32), alignof(int32));
		IntsB.Empty(0, sizeof(int32), alignof(int32));
		Vectors.Empty(0, sizeof(FVector), alignof(FVector));
	}

	TEST_METHOD(HostClosedTemplateMembersAreCallable)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		const bool bHostReady = BuildHostTemplates(Collection, Diagnostic);
		ASSERT_THAT(IsTrue(bHostReady, Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		asCDefinitions* Graph = const_cast<asCDefinitions*>(Collection.GetHostDefinitions());
		asCObjectType* IntArray = HostArrayInstance(*Graph, "int");
		asCScriptFunction* IntAdd = FindHostMethod(IntArray, "Add");
		asCScriptFunction* IntNum = FindHostMethod(IntArray, "Num");
		ASSERT_THAT(IsNotNull(IntAdd));
		ASSERT_THAT(IsNotNull(IntNum));
		FScriptArray Values;
		CallHostAddInt(*IntAdd, Values, 4);
		ASSERT_THAT(AreEqual(1, CallHostNum(*IntNum, Values)));
		Values.Empty(0, sizeof(int32), alignof(int32));
	}

	TEST_METHOD(PrivateArguments)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		const bool bHostReady = BuildHostTemplates(Collection, Diagnostic);
		ASSERT_THAT(IsTrue(bHostReady, Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		FHostTemplatesEngineOwner A;
		FHostTemplatesEngineOwner B;
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(A.Ptr->RegisterObjectType("Pair", sizeof(FHostTemplatesPair), asOBJ_VALUE | asOBJ_APP_CLASS | asOBJ_POD) >= 0));
		ASSERT_THAT(IsTrue(B.Ptr->RegisterObjectType("Pair", sizeof(FHostTemplatesPair), asOBJ_VALUE | asOBJ_APP_CLASS | asOBJ_POD) >= 0));
		asCObjectType* PairA = static_cast<asCObjectType*>(A.Ptr->GetTypeInfoByName("Pair"));
		asCObjectType* PairB = static_cast<asCObjectType*>(B.Ptr->GetTypeInfoByName("Pair"));
		ASSERT_THAT(IsNotNull(PairA));
		ASSERT_THAT(IsNotNull(PairB));
		ASSERT_THAT(IsFalse(PairA == PairB));
		asCObjectType* Template = static_cast<asCObjectType*>(Collection.GetHostDefinitions()->FindType("TArray"));
		ASSERT_THAT(IsNotNull(Template));
		asCDataType TypeA = asCDataType::CreateType(PairA, false);
		asCDataType TypeB = asCDataType::CreateType(PairB, false);
		asCObjectType* ArrayA = nullptr;
		asCObjectType* ArrayB = nullptr;
		asCObjectType* Cross = nullptr;
		ASSERT_THAT(IsTrue(A.Ptr->SpecializeTemplate(*Template, MakeArrayView(&TypeA, 1), ArrayA) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->SpecializeTemplate(*Template, MakeArrayView(&TypeB, 1), ArrayB) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsNotNull(ArrayA));
		ASSERT_THAT(IsNotNull(ArrayB));
		ASSERT_THAT(IsFalse(ArrayA == ArrayB));
		ASSERT_THAT(IsTrue(ArrayA->GetTypeInfoKind() == asETypeInfoKind::LiveRegister));
		ASSERT_THAT(IsTrue(ArrayB->GetTypeInfoKind() == asETypeInfoKind::LiveRegister));
		ASSERT_THAT(IsTrue(A.Ptr->SpecializeTemplate(*Template, MakeArrayView(&TypeB, 1), Cross) == asERegistrationResult::ForeignEngine));
		ASSERT_THAT(IsNull(Cross));
		ASSERT_THAT(IsTrue(A.Ptr->FindTemplateInstance(Template, MakeArrayView(&TypeB, 1)) == nullptr));
		ASSERT_THAT(IsTrue(B.Ptr->FindTemplateInstance(Template, MakeArrayView(&TypeA, 1)) == nullptr));
	}

	TEST_METHOD(LateNestedAndRollback)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		const bool bHostReady = BuildHostTemplates(Collection, Diagnostic);
		ASSERT_THAT(IsTrue(bHostReady, Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		asCDefinitions* Graph = const_cast<asCDefinitions*>(Collection.GetHostDefinitions());
		ASSERT_THAT(IsNotNull(Graph));
		asCObjectType* IntArray = HostArrayInstance(*Graph, "int");
		asCObjectType* Nested = HostArrayInstance(*Graph, "TArray<int>");
		ASSERT_THAT(IsNotNull(IntArray));
		ASSERT_THAT(IsNotNull(Nested));
		ASSERT_THAT(IsTrue(IntArray->GetTypeInfoKind() == asETypeInfoKind::HostProcess));
		asCScriptFunction* IntAdd = FindHostMethod(IntArray, "Add");
		asCScriptFunction* IntNum = FindHostMethod(IntArray, "Num");
		ASSERT_THAT(IsNotNull(IntAdd));
		ASSERT_THAT(IsNotNull(IntNum));
		asCObjectType* Missing = nullptr;
		asCDataType VoidType = Graph->Primitive(ttVoid);
		asCObjectType* Template = static_cast<asCObjectType*>(Graph->FindType("TArray"));
		ASSERT_THAT(IsNotNull(Template));
		ASSERT_THAT(IsTrue(Graph->CreateHostTemplateInstance(*Template, Template->GetStableKey(),
			MakeArrayView(&VoidType, 1), Missing) == asEDefinitionResult::InvalidState));
		ASSERT_THAT(IsNull(Missing));
		ASSERT_THAT(IsNull(Graph->FindTemplateInstance(Template, MakeArrayView(&VoidType, 1))));
		FScriptArray Values;
		CallHostAddInt(*IntAdd, Values, 9);
		CallHostAddInt(*IntAdd, Values, 8);
		ASSERT_THAT(AreEqual(2, CallHostNum(*IntNum, Values)));
		Values.Empty(0, sizeof(int32), alignof(int32));
	}
};

#endif
