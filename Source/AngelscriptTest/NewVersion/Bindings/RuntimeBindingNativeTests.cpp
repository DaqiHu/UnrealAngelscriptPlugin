#include "CQTest.h"
#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptTypeBindInfoApply.h"
#include "as_scriptengine.h"
#include "as_context.h"
#include "as_property.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
BEGIN_AS_NAMESPACE
namespace BindingNativeTests
{
struct FPair
{
	int32 X = 0, Y = 0;
	int32 Sum() const { return X + Y; }
	FPair Value() const { return *this; }
	FPair& Ref() { return *this; }
};
static int32 Add(int32 A, int32 B) { return A + B; }
static int32 First(FPair* Self, int32 Extra) { return Self->Sum() + Extra; }
static int32 Last(int32 Extra, FPair* Self) { return Self->Sum() + Extra; }
static void Fill(int32& Out, int32& InOut) { Out = 7; InOut += 5; }
static void* ObservedAuxiliary = nullptr;
static void Generic(asIScriptGeneric* Call)
{
	ObservedAuxiliary = Call->GetAuxiliary();
	if (ObservedAuxiliary) Call->SetReturnDWord(*static_cast<int32*>(ObservedAuxiliary) + Call->GetArgDWord(0));
}
struct FRebindAuxiliary
{
	asCScriptEngine* Engine = nullptr;
	asSStableKey Key;
	int32 NextValue = 14;
	bool Rebound = false;
	bool KeptOriginal = false;
};
static void GenericMarker(asIScriptGeneric* Call)
{
	ObservedAuxiliary = Call->GetAuxiliary();
	if (ObservedAuxiliary) Call->SetReturnDWord(*static_cast<int32*>(ObservedAuxiliary));
}
static void GenericRebind(asIScriptGeneric* Call)
{
	auto* State = static_cast<FRebindAuxiliary*>(Call->GetAuxiliary());
	if (!State) return;
	State->Rebound = State->Engine->BindNativeFunction(State->Key, asFUNCTION(GenericMarker), asCALL_GENERIC, nullptr, &State->NextValue) == asEByteCodeLinkStatus::Succeeded;
	State->KeptOriginal = Call->GetAuxiliary() == State;
	Call->SetReturnDWord(10);
}
static asIScriptFunction* ObservedFunction = nullptr;
static asITypeInfo* ObservedType = nullptr;
static int32 WithFunction(asIScriptFunction* Function, int32 Value) { ObservedFunction = Function; return Value + 1; }
static int32 WithType(FPair* Self, asITypeInfo* Type) { ObservedType = Type; return Self->Sum(); }
static int32 Constructs = 0, Copies = 0, Destructs = 0;
static void Construct(FPair* Self, int32 X, int32 Y) { new(Self) FPair{X, Y}; ++Constructs; }
static void Copy(FPair* Self, const FPair& Other) { new(Self) FPair(Other); ++Copies; }
static void Destruct(FPair* Self) { Self->~FPair(); ++Destructs; }

TEST_CLASS_WITH_FLAGS(Native, "Angelscript.UnitTest.RuntimeBindings.Calls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	struct FFixture
	{
		TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
		FAngelscriptBinds Binds{*Store};
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		TUniquePtr<FAngelscriptTypeBindInfoInstallation> Installed;
		FString Diagnostic;
		~FFixture() { Installed.Reset(); Engine->ShutDownAndRelease(); }
		FAngelscriptBinds Pair(const ANSICHAR* Name = "Pair")
		{
			FBindFlags Flags; Flags.bPOD = true; return Binds.ValueClassForTarget<FPair>(Name, Flags);
		}
		bool Install() { return Store->Seal(Diagnostic) && FAngelscriptTypeBindInfoApply::Install(Store, *Engine, Installed, Diagnostic); }
		bool Connect() { return FAngelscriptTypeBindInfoApply::ConnectNative(*Installed, *Engine, Diagnostic); }
		asCScriptFunction* Function(const TCHAR* Declaration, const TCHAR* Owner = TEXT(""))
		{
			for (const auto& Member : Installed->GetMembers())
			{
				const auto* Record = Store->GetMember(Member.Record);
				if (Record->Declaration.Equals(Declaration, ESearchCase::CaseSensitive) &&
					Store->GetType(Member.Record.Type)->Declaration.Name.Equals(Owner, ESearchCase::CaseSensitive)) return Member.Function;
			}
			return nullptr;
		}
	};
	struct FContext
	{
		asIScriptContext* Ptr;
		explicit FContext(FFixture& Fixture) : Ptr(Fixture.Engine->CreateContext()) {}
		~FContext() { Ptr->Release(); }
		asIScriptContext* operator->() const { return Ptr; }
	};
	TEST_METHOD(FreeFunctionAddsThroughContext)
	{
		FFixture F; F.Binds.BindGlobalFunctionForTarget("int Add(int A, int B)", &Add);
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect()));
		FContext C(F); ASSERT_THAT(AreEqual(0, C->Prepare(F.Function(TEXT("int Add(int A, int B)")))));
		ASSERT_THAT(AreEqual(0, C->SetArgDWord(0, 2))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(1, 3)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(asDWORD(5), C->GetReturnDWord()));
	}
	TEST_METHOD(MemberFunctionUsesReceiver)
	{
		FFixture F; F.Pair().Method("int Sum() const", &FPair::Sum);
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect())); FPair Value{3, 4}; FContext C(F);
		ASSERT_THAT(AreEqual(0, C->Prepare(F.Function(TEXT("int Sum() const"), TEXT("Pair")))));
		ASSERT_THAT(AreEqual(0, C->SetObject(&Value))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		ASSERT_THAT(AreEqual(asDWORD(7), C->GetReturnDWord()));
	}
	TEST_METHOD(MemberAuxiliaryDoesNotReplaceTheReceiver)
	{
		FFixture F; int32 Auxiliary = 21; F.Pair().Method("int Sum() const", &FPair::Sum, &Auxiliary);
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect()));
		auto* Function = F.Function(TEXT("int Sum() const"), TEXT("Pair")); auto Binding = F.Engine->AcquireSystemInterface(Function);
		ASSERT_THAT(IsTrue(bool(Binding))); ASSERT_THAT(IsTrue(Binding->auxiliary == &Auxiliary));
		FPair Value{3, 4}; FContext C(F); ASSERT_THAT(AreEqual(0, C->Prepare(Function))); ASSERT_THAT(AreEqual(0, C->SetObject(&Value)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(asDWORD(7), C->GetReturnDWord()));
	}
	TEST_METHOD(ObjectFirstAndObjectLastKeepExplicitArgumentOrder)
	{
		FFixture F; auto Pair = F.Pair(); Pair.Method("int First(int Extra) const", &First);
		Pair.BindMethodDirectForTarget("Pair", "int Last(int Extra) const", asFUNCTION(Last), asCALL_CDECL_OBJLAST, ASAutoCaller::MakeFunctionCaller(&Last));
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect())); FPair Value{3, 4}; FContext C(F);
		for (const auto* Declaration : {TEXT("int First(int Extra) const"), TEXT("int Last(int Extra) const")})
		{
			ASSERT_THAT(AreEqual(0, C->Prepare(F.Function(Declaration, TEXT("Pair"))))); ASSERT_THAT(AreEqual(0, C->SetObject(&Value)));
			ASSERT_THAT(AreEqual(0, C->SetArgDWord(0, 5))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
			ASSERT_THAT(AreEqual(asDWORD(12), C->GetReturnDWord()));
		}
	}
	TEST_METHOD(ValueReturnPreservesBothFields)
	{
		FFixture F; F.Pair().Method("Pair Value() const", &FPair::Value);
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect())); FPair Value{3, 4}; FContext C(F);
		ASSERT_THAT(AreEqual(0, C->Prepare(F.Function(TEXT("Pair Value() const"), TEXT("Pair")))));
		ASSERT_THAT(AreEqual(0, C->SetObject(&Value))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		auto* Returned = static_cast<FPair*>(C->GetReturnObject()); ASSERT_THAT(IsNotNull(Returned));
		ASSERT_THAT(AreEqual(3, Returned->X)); ASSERT_THAT(AreEqual(4, Returned->Y)); ASSERT_THAT(IsTrue(Returned != &Value));
	}
	TEST_METHOD(ReferenceReturnAliasesReceiver)
	{
		FFixture F; F.Pair().Method("Pair& Ref()", &FPair::Ref);
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect())); FPair Value{3, 4}; FContext C(F);
		ASSERT_THAT(AreEqual(0, C->Prepare(F.Function(TEXT("Pair& Ref()"), TEXT("Pair")))));
		ASSERT_THAT(AreEqual(0, C->SetObject(&Value))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		ASSERT_THAT(IsTrue(C->GetReturnAddress() == &Value));
	}
	TEST_METHOD(OutAndInOutWriteCallerStorage)
	{
		FFixture F; F.Binds.BindGlobalFunctionForTarget("void Fill(int&out Out, int&inout Value)", &Fill);
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect())); int32 Out = 0, InOut = 3; FContext C(F);
		ASSERT_THAT(AreEqual(0, C->Prepare(F.Function(TEXT("void Fill(int&out Out, int&inout Value)")))));
		ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Out))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(1, &InOut)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(7, Out)); ASSERT_THAT(AreEqual(8, InOut));
	}
	TEST_METHOD(GenericCallbackObservesAuxiliary)
	{
		FFixture F; int32 Auxiliary = 6; F.Binds.BindGlobalGenericFunctionForTarget("int Generic(int Value)", &Generic, &Auxiliary);
		ObservedAuxiliary = nullptr;
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect())); FContext C(F);
		ASSERT_THAT(AreEqual(0, C->Prepare(F.Function(TEXT("int Generic(int Value)"))))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(0, 4)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(ObservedAuxiliary == &Auxiliary));
		ASSERT_THAT(AreEqual(asDWORD(10), C->GetReturnDWord()));
	}
	TEST_METHOD(FirstParameterMetadataUsesInstalledFunctionAndOwner)
	{
		FFixture F; ObservedFunction = nullptr; ObservedType = nullptr;
		F.Binds.BindGlobalFunctionForTarget("int WithFunction(int Value)", &WithFunction).PassScriptFunctionAsFirstParam();
		F.Pair().Method("int WithType() const", &WithType).PassScriptObjectTypeAsFirstParam();
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect())); FContext C(F); FPair Value{3, 4};
		auto* Function = F.Function(TEXT("int WithFunction(int Value)"));
		ASSERT_THAT(AreEqual(0, C->Prepare(Function))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(0, 8)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(ObservedFunction == Function));
		ASSERT_THAT(AreEqual(asDWORD(9), C->GetReturnDWord()));
		ASSERT_THAT(AreEqual(0, C->Prepare(F.Function(TEXT("int WithType() const"), TEXT("Pair"))))); ASSERT_THAT(AreEqual(0, C->SetObject(&Value)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(ObservedType == F.Installed->FindType(TEXT("Pair"))));
		ASSERT_THAT(AreEqual(asDWORD(7), C->GetReturnDWord()));
	}
	TEST_METHOD(GenericAuxiliaryKeepsExecutingGenerationDuringRebind)
	{
		FFixture F; FRebindAuxiliary State; F.Binds.BindGlobalGenericFunctionForTarget("int Rebind()", &GenericRebind, &State);
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect()));
		auto* Function = F.Function(TEXT("int Rebind()")); State.Engine = F.Engine; State.Key = Function->GetStableKey(); FContext C(F);
		ASSERT_THAT(AreEqual(0, C->Prepare(Function))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		ASSERT_THAT(IsTrue(State.Rebound)); ASSERT_THAT(IsTrue(State.KeptOriginal)); ASSERT_THAT(AreEqual(asDWORD(10), C->GetReturnDWord()));
		ASSERT_THAT(AreEqual(0, C->Unprepare())); ASSERT_THAT(AreEqual(0, C->Prepare(Function)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(IsTrue(ObservedAuxiliary == &State.NextValue));
		ASSERT_THAT(AreEqual(asDWORD(14), C->GetReturnDWord()));
	}
	TEST_METHOD(ValueConstructionCopyAndDestructionBalance)
	{
		FFixture F; auto Pair = F.Pair(); Constructs = Copies = Destructs = 0;
		Pair.Constructor("void f(int X, int Y)", &Construct); Pair.Constructor("void f(const Pair&in Other)", &Copy); Pair.Destructor("void f()", &Destruct);
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect()));
		alignas(FPair) uint8 FirstStorage[sizeof(FPair)], SecondStorage[sizeof(FPair)]; FContext C(F);
		ASSERT_THAT(AreEqual(0, C->Prepare(F.Function(TEXT("void f(int X, int Y)"), TEXT("Pair"))))); ASSERT_THAT(AreEqual(0, C->SetObject(FirstStorage)));
		ASSERT_THAT(AreEqual(0, C->SetArgDWord(0, 3))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(1, 4))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		ASSERT_THAT(AreEqual(0, C->Prepare(F.Function(TEXT("void f(const Pair&in Other)"), TEXT("Pair"))))); ASSERT_THAT(AreEqual(0, C->SetObject(SecondStorage)));
		ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, FirstStorage))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		ASSERT_THAT(AreEqual(7, reinterpret_cast<FPair*>(SecondStorage)->Sum()));
		for (auto* Storage : {FirstStorage, SecondStorage})
		{
			ASSERT_THAT(AreEqual(0, C->Prepare(F.Function(TEXT("void f()"), TEXT("Pair"))))); ASSERT_THAT(AreEqual(0, C->SetObject(Storage)));
			ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		}
		ASSERT_THAT(AreEqual(1, Constructs)); ASSERT_THAT(AreEqual(1, Copies)); ASSERT_THAT(AreEqual(2, Destructs));
	}
	TEST_METHOD(GlobalPropertyConnectsOriginalStorage)
	{
		FFixture F; int32 Counter = 3; F.Binds.BindGlobalVariableForTarget("int Counter", &Counter);
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect()));
		const auto Members = F.Installed->GetMembers(); ASSERT_THAT(AreEqual(1, Members.Num()));
		auto* Address = static_cast<int32*>(F.Engine->FindBoundGlobalStorage(Members[0].Global->GetStableKey()));
		ASSERT_THAT(IsTrue(Address == &Counter)); *Address = 8; ASSERT_THAT(AreEqual(8, Counter));
	}
	TEST_METHOD(MissingTargetRejectsWholeConnection)
	{
		FFixture F; F.Binds.BindGlobalFunctionForTarget("int Add(int A, int B)", &Add);
		F.Binds.BindGlobalFunctionDirectForTarget("void Missing()", asSFuncPtr(), asCALL_CDECL, ASAutoCaller::FunctionCaller::Make());
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsFalse(F.Connect())); ASSERT_THAT(IsFalse(F.Diagnostic.IsEmpty()));
		ASSERT_THAT(IsFalse(bool(F.Engine->AcquireSystemInterface(F.Function(TEXT("int Add(int A, int B)"))))));
	}
	TEST_METHOD(MissingGlobalStorageRejectsFunctionsAndEarlierAddresses)
	{
		FFixture F; int32 Counter = 3; F.Binds.BindGlobalFunctionForTarget("int Add(int A, int B)", &Add);
		F.Binds.BindGlobalVariableForTarget("int Counter", &Counter); F.Binds.BindGlobalVariableForTarget("int Missing", nullptr);
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsFalse(F.Connect())); ASSERT_THAT(IsFalse(F.Diagnostic.IsEmpty()));
		ASSERT_THAT(IsFalse(bool(F.Engine->AcquireSystemInterface(F.Function(TEXT("int Add(int A, int B)"))))));
		for (const auto& Member : F.Installed->GetMembers()) if (Member.Global)
			ASSERT_THAT(IsNull(F.Engine->FindBoundGlobalStorage(Member.Global->GetStableKey())));
		ASSERT_THAT(AreEqual(3, Counter));
	}
	TEST_METHOD(PureConstantConnectsWithoutExternalStorage)
	{
		FFixture F; F.Binds.BindGlobalVariableForTarget("const int Answer", nullptr);
		const FAngelscriptTypeBindMemberHandle Handle{F.Store->FindScope(TEXT("")), 0};
		auto Record = *F.Store->GetMember(Handle); Record.bPureConstant = true; Record.ConstantValue = 42;
		ASSERT_THAT(IsTrue(F.Store->UpdateMember(Handle, Record, F.Diagnostic)));
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect()));
		const auto Members = F.Installed->GetMembers(); ASSERT_THAT(AreEqual(1, Members.Num()));
		ASSERT_THAT(IsTrue(Members[0].Global->isPureConstant)); ASSERT_THAT(AreEqual(asQWORD(42), Members[0].Global->storage));
		ASSERT_THAT(IsNull(F.Engine->FindBoundGlobalStorage(Members[0].Global->GetStableKey())));
	}
	TEST_METHOD(CaseSensitiveGlobalNamesKeepDistinctAddresses)
	{
		FFixture F; int32 Upper = 3, Lower = 8;
		F.Binds.BindGlobalVariableForTarget("int Counter", &Upper); F.Binds.BindGlobalVariableForTarget("int counter", &Lower);
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect())); const auto Members = F.Installed->GetMembers();
		ASSERT_THAT(AreEqual(2, Members.Num())); ASSERT_THAT(IsTrue(Members[0].Global != Members[1].Global));
		ASSERT_THAT(IsTrue(F.Engine->FindBoundGlobalStorage(Members[0].Global->GetStableKey()) == &Upper));
		ASSERT_THAT(IsTrue(F.Engine->FindBoundGlobalStorage(Members[1].Global->GetStableKey()) == &Lower));
	}
	TEST_METHOD(IncompatibleCallableOwnerRejectsWholeConnection)
	{
		FFixture F; F.Binds.BindGlobalFunctionForTarget("int Add(int A, int B)", &Add);
		F.Binds.BindGlobalFunctionDirectForTarget("int Wrong()", asMETHOD(FPair, Sum), asCALL_THISCALL, ASAutoCaller::MakeFunctionCaller(&FPair::Sum));
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsFalse(F.Connect())); ASSERT_THAT(IsFalse(F.Diagnostic.IsEmpty()));
		ASSERT_THAT(IsFalse(bool(F.Engine->AcquireSystemInterface(F.Function(TEXT("int Add(int A, int B)"))))));
	}
	TEST_METHOD(ForeignEngineRejectsInstalledIdentity)
	{
		FFixture F, Other; F.Binds.BindGlobalFunctionForTarget("int Add(int A, int B)", &Add);
		Other.Binds.BindGlobalFunctionForTarget("int Add(int A, int B)", &Add);
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(Other.Install()));
		ASSERT_THAT(IsFalse(FAngelscriptTypeBindInfoApply::ConnectNative(*F.Installed, *Other.Engine, F.Diagnostic)));
		ASSERT_THAT(IsFalse(bool(Other.Engine->AcquireSystemInterface(Other.Function(TEXT("int Add(int A, int B)"))))));
		ASSERT_THAT(IsTrue(F.Connect()));
	}
	TEST_METHOD(WrongSignatureRejectsWholeConnection)
	{
		FFixture F; F.Binds.BindGlobalFunctionForTarget("int Add(int A, int B)", &Add);
		F.Binds.BindGlobalFunctionForTarget("int Wrong(int A)", &Add);
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsFalse(F.Connect())); ASSERT_THAT(IsFalse(F.Diagnostic.IsEmpty()));
		ASSERT_THAT(IsFalse(bool(F.Engine->AcquireSystemInterface(F.Function(TEXT("int Add(int A, int B)"))))));
	}
	TEST_METHOD(CaseSensitiveOwnersDispatchDistinctFunctions)
	{
		FFixture F; F.Pair("Pair").Method("int Sum() const", &FPair::Sum); F.Pair("pair").Method("int Sum() const", &FPair::Sum);
		ASSERT_THAT(IsTrue(F.Install())); ASSERT_THAT(IsTrue(F.Connect())); FPair A{3, 4}, B{8, 9}; FContext C(F);
		ASSERT_THAT(IsTrue(F.Installed->FindType(TEXT("Pair")) != F.Installed->FindType(TEXT("pair"))));
		auto* Upper = F.Function(TEXT("int Sum() const"), TEXT("Pair")); auto* Lower = F.Function(TEXT("int Sum() const"), TEXT("pair"));
		ASSERT_THAT(IsTrue(Upper != Lower)); ASSERT_THAT(IsTrue(Upper->objectType == F.Installed->FindType(TEXT("Pair"))));
		ASSERT_THAT(IsTrue(Lower->objectType == F.Installed->FindType(TEXT("pair"))));
		ASSERT_THAT(AreEqual(0, C->Prepare(F.Function(TEXT("int Sum() const"), TEXT("Pair"))))); ASSERT_THAT(AreEqual(0, C->SetObject(&A)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(asDWORD(7), C->GetReturnDWord()));
		ASSERT_THAT(AreEqual(0, C->Prepare(F.Function(TEXT("int Sum() const"), TEXT("pair"))))); ASSERT_THAT(AreEqual(0, C->SetObject(&B)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); ASSERT_THAT(AreEqual(asDWORD(17), C->GetReturnDWord()));
	}
};
}
END_AS_NAMESPACE
#endif
