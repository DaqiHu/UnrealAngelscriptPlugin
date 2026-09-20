#include "CQTest.h"
#include "AngelscriptEngine.h"
#include "NativeEngine/Compile/DelegateNativeFixtures.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(DelegateNativeInterop, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(NativeRetainedCallback)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateNativeFixtures::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TSharedPtr<FAngelscriptModuleDesc> Module;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateNativeFixtures::Compile(*Engine, TEXT("Retain"),
			TEXT("int AddOffset(int Value, int Offset) { return Value + Offset; }\n"),
			Module, Error), *Error));
		asIScriptFunction* AddOffset = DelegateNativeFixtures::FindGlobal(Engine->GetScriptEngine(), "AddOffset");
		ASSERT_THAT(IsNotNull(AddOffset, TEXT("AddOffset")));

		TDelegate<int32(int32)> Native;
		FString BindError;
		ASSERT_THAT(IsTrue(DelegateNativeFixtures::BindIntTransform(Native, Engine->GetScriptEngine(), AddOffset, 40, BindError), *BindError));
		ASSERT_THAT(IsTrue(Native.IsBound(), TEXT("native unbound")));
		ASSERT_THAT(AreEqual(42, Native.Execute(2)));
	}

	TEST_METHOD(SignatureMismatch)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateNativeFixtures::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TSharedPtr<FAngelscriptModuleDesc> Module;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateNativeFixtures::Compile(*Engine, TEXT("Mismatch"),
			TEXT("void Wrong(int Value) {}\n"),
			Module, Error), *Error));
		asIScriptFunction* Wrong = DelegateNativeFixtures::FindGlobal(Engine->GetScriptEngine(), "Wrong");
		ASSERT_THAT(IsNotNull(Wrong, TEXT("Wrong")));

		TDelegate<int32(int32)> Native;
		FString BindError;
		ASSERT_THAT(IsFalse(DelegateNativeFixtures::BindIntTransform(Native, Engine->GetScriptEngine(), Wrong, 40, BindError), TEXT("mismatch accepted")));
		ASSERT_THAT(IsFalse(Native.IsBound(), TEXT("native bound after mismatch")));
		ASSERT_THAT(IsTrue(BindError.Contains(TEXT("signature-mismatch")), *BindError));
	}

	TEST_METHOD(NativeMulticastRemove)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateNativeFixtures::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TSharedPtr<FAngelscriptModuleDesc> Module;
		FString Error;
		FDelegateNativeOwner Owner;
		ASSERT_THAT(IsTrue(DelegateNativeFixtures::Compile(*Engine, TEXT("Multicast"),
			TEXT("void Listen(int Value) {}\n"),
			Module, Error), *Error));
		asIScriptFunction* Listen = DelegateNativeFixtures::FindGlobal(Engine->GetScriptEngine(), "Listen");
		ASSERT_THAT(IsNotNull(Listen, TEXT("Listen")));

		FDelegateHandle Handle;
		FString BindError;
		ASSERT_THAT(IsTrue(DelegateNativeFixtures::AddIntListener(
			Owner.Event, Engine->GetScriptEngine(), Listen, Handle, BindError, &Owner.Hits), *BindError));
		ASSERT_THAT(IsTrue(Handle.IsValid(), TEXT("handle")));

		Owner.Event.Broadcast(42);
		ASSERT_THAT(AreEqual(42, Owner.Hits, TEXT("first broadcast")));

		Owner.Event.Remove(Handle);
		Owner.Hits = 0;
		Owner.Event.Broadcast(99);
		ASSERT_THAT(AreEqual(0, Owner.Hits, TEXT("removed subscriber still observed")));
	}
};

#endif
