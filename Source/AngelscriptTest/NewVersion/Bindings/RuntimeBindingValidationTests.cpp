#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptTypeBindInfoValidation.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
using namespace RuntimeBindingTestSupport;

TEST_CLASS_WITH_FLAGS(Validation, "Angelscript.UnitTest.RuntimeBindings.Recording",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptTypeBindSource Source(const TCHAR* Provider)
	{
		return {TEXT("AngelscriptTest"), Provider, TEXT("RuntimeBindingValidationTests.cpp"), 23};
	}

	static TSharedRef<FAngelscriptTypeBindInfoStore> SealTypes(
		TArray<FAngelscriptTypeBindDeclaration> Types,
		TArray<TPair<FString, TPair<FString, SIZE_T>>> Properties = {})
	{
		auto Store = MakeShared<FAngelscriptTypeBindInfoStore>();
		TMap<FString, FAngelscriptTypeBindHandle> Handles;
		FString Diagnostic;
		for (auto& Type : Types)
		{
			FAngelscriptTypeBindHandle Handle;
			check(Store->RecordType(Type, Handle, Diagnostic));
			Handles.Add(Type.Name, Handle);
		}
		for (const auto& Property : Properties)
		{
			FAngelscriptTypeBindInfoMember Member;
			Member.Kind = EAngelscriptTypeBindMemberKind::Property;
			Member.Declaration = Property.Value.Key;
			Member.PropertyOffset = Property.Value.Value;
			Member.Source = Store->GetType(Handles[Property.Key])->Declaration.Source;
			FAngelscriptTypeBindMemberHandle Handle;
			check(Store->RecordMember(Handles[Property.Key], Member, Handle, Diagnostic));
		}
		check(Store->Seal(Diagnostic));
		return Store;
	}

	static FAngelscriptTypeBindDeclaration Value(const TCHAR* Name, uint32 Size = 4, uint32 Alignment = 4)
	{
		FAngelscriptTypeBindDeclaration Result;
		Result.Name = Name;
		Result.Kind = EAngelscriptTypeBindKind::Value;
		Result.NativeSize = Size;
		Result.NativeAlignment = Alignment;
		Result.ObjectFlags = asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS;
		Result.Source = Source(*FString::Printf(TEXT("Validation.%s"), Name));
		return Result;
	}

	static TPair<FString, TPair<FString, SIZE_T>> Property(
		const TCHAR* Owner, const TCHAR* Declaration, SIZE_T Offset)
	{
		return MakeTuple(FString(Owner), MakeTuple(FString(Declaration), Offset));
	}

public:
	TEST_METHOD(ValidPairAndGlobalsValidateWithoutEngineOrProviderExecution)
	{
		auto Store = RecordFixture();
		const uint64 Before = FAngelscriptEngine::GetBindingNativeEngineAllocationCountForTesting();
		const auto Result = FAngelscriptTypeBindInfoValidation::ValidateSealed(Store);
		ASSERT_THAT(IsTrue(Result.bValid));
		ASSERT_THAT(IsTrue(Result.Stage == EAngelscriptBindingStage::None));
		ASSERT_THAT(IsTrue(Result.Message.IsEmpty()));
		ASSERT_THAT(AreEqual(Before, FAngelscriptEngine::GetBindingNativeEngineAllocationCountForTesting()));
	}

	TEST_METHOD(MissingNominalAndMalformedMemberNameTheirSource)
	{
		for (const FString Declaration : {FString(TEXT("int Wrong(MissingType Value)")), FString(TEXT("int Broken("))})
		{
			auto Store = RecordFixture(false, false);
			Store->SetSource(Source(TEXT("Validation.Member")));
			FAngelscriptBinds Binds(*Store);
			Binds.BindGlobalFunctionForTarget(Declaration, &Add);
			FString Diagnostic; ASSERT_THAT(IsTrue(Store->Seal(Diagnostic)));
			const auto Result = FAngelscriptTypeBindInfoValidation::ValidateSealed(Store);
			ASSERT_THAT(IsFalse(Result.bValid));
			ASSERT_THAT(IsTrue(Result.Stage == EAngelscriptBindingStage::Members));
			ASSERT_THAT(IsTrue(Result.Message.Contains(Declaration.Contains(TEXT("Missing")) ? TEXT("MissingType") : TEXT("Broken"))));
			ASSERT_THAT(IsTrue(Result.Message.Contains(TEXT("Validation.Member"))));
		}
	}

	TEST_METHOD(BaseAndByValueCyclesFailWhileForwardBaseAndHandleCyclePass)
	{
		auto A = Value(TEXT("A")); auto B = Value(TEXT("B"));
		A.BaseType = TEXT("B"); B.BaseType = TEXT("A");
		auto BaseCycle = FAngelscriptTypeBindInfoValidation::ValidateSealed(SealTypes({A, B}));
		ASSERT_THAT(IsFalse(BaseCycle.bValid));
		ASSERT_THAT(IsTrue(BaseCycle.Message.Contains(TEXT("A")) && BaseCycle.Message.Contains(TEXT("B"))));

		A = Value(TEXT("A"), 8, 4); B = Value(TEXT("B"), 8, 4);
		auto ValueCycleStore = SealTypes({A, B}, {Property(TEXT("A"), TEXT("B Other"), 0), Property(TEXT("B"), TEXT("A Other"), 0)});
		auto ValueCycle = FAngelscriptTypeBindInfoValidation::ValidateSealed(ValueCycleStore);
		ASSERT_THAT(IsFalse(ValueCycle.bValid)); ASSERT_THAT(IsTrue(ValueCycle.Message.Contains(TEXT("cycle"))));

		auto Base = Value(TEXT("Base")); auto Derived = Value(TEXT("Derived")); Derived.BaseType = TEXT("Base");
		auto Forward = FAngelscriptTypeBindInfoValidation::ValidateSealed(SealTypes({Derived, Base}));
		ASSERT_THAT(IsTrue(Forward.bValid));

		FAngelscriptTypeBindDeclaration OA = Value(TEXT("OA"), sizeof(void*), alignof(void*));
		FAngelscriptTypeBindDeclaration OB = Value(TEXT("OB"), sizeof(void*), alignof(void*));
		OA.Kind = OB.Kind = EAngelscriptTypeBindKind::ObjectHandle;
		OA.ObjectFlags = OB.ObjectFlags = asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE;
		auto Handles = SealTypes({OA, OB}, {Property(TEXT("OA"), TEXT("OB@ Other"), 0), Property(TEXT("OB"), TEXT("OA@ Other"), 0)});
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoValidation::ValidateSealed(Handles).bValid));
	}

	TEST_METHOD(LayoutAndPropertyBoundsFailAtTypesStage)
	{
		auto BadAlignment = FAngelscriptTypeBindInfoValidation::ValidateSealed(SealTypes({Value(TEXT("BadAlignment"), 8, 3)}));
		ASSERT_THAT(IsFalse(BadAlignment.bValid));
		ASSERT_THAT(IsTrue(BadAlignment.Stage == EAngelscriptBindingStage::Types));
		ASSERT_THAT(IsTrue(BadAlignment.Message.Contains(TEXT("BadAlignment"))));

		auto Owner = Value(TEXT("Owner"), 4, 4);
		auto BadOffset = FAngelscriptTypeBindInfoValidation::ValidateSealed(
			SealTypes({Owner}, {Property(TEXT("Owner"), TEXT("int X"), 4)}));
		ASSERT_THAT(IsFalse(BadOffset.bValid));
		ASSERT_THAT(IsTrue(BadOffset.Stage == EAngelscriptBindingStage::Types));
		ASSERT_THAT(IsTrue(BadOffset.Message.Contains(TEXT("int X"))));
	}

	TEST_METHOD(MissingNativeTargetFailsBeforeEngineAllocation)
	{
		auto Store = RecordFixture(true);
		const uint64 Before = FAngelscriptEngine::GetBindingNativeEngineAllocationCountForTesting();
		const auto Result = FAngelscriptTypeBindInfoValidation::ValidateSealed(Store);
		ASSERT_THAT(IsFalse(Result.bValid));
		ASSERT_THAT(IsTrue(Result.Stage == EAngelscriptBindingStage::Native));
		ASSERT_THAT(IsTrue(Result.Message.Contains(TEXT("Missing"))));
		FAngelscriptBindingDiagnostic Diagnostic;
		auto Owner = FAngelscriptEngine::CreateForBindings(Store, Diagnostic);
		ASSERT_THAT(IsFalse(Owner.IsValid()));
		ASSERT_THAT(AreEqual(Before, FAngelscriptEngine::GetBindingNativeEngineAllocationCountForTesting()));
	}

	TEST_METHOD(InvalidProvenanceAndRecordingFailureAreRejected)
	{
		auto Store = MakeShared<FAngelscriptTypeBindInfoStore>();
		auto Type = Value(TEXT("NoSource")); Type.Source = {};
		FAngelscriptTypeBindHandle Handle; FString Diagnostic;
		ASSERT_THAT(IsTrue(Store->RecordType(Type, Handle, Diagnostic)));
		ASSERT_THAT(IsTrue(Store->Seal(Diagnostic)));
		auto Result = FAngelscriptTypeBindInfoValidation::ValidateSealed(Store);
		ASSERT_THAT(IsFalse(Result.bValid)); ASSERT_THAT(IsTrue(Result.Message.Contains(TEXT("source"), ESearchCase::IgnoreCase)));

		auto Failed = MakeShared<FAngelscriptTypeBindInfoStore>();
		ASSERT_THAT(IsFalse(Failed->ReportFailure(TEXT("provider failed"))));
		ASSERT_THAT(IsFalse(Failed->Seal(Diagnostic)));
		Result = FAngelscriptTypeBindInfoValidation::ValidateSealed(Failed);
		ASSERT_THAT(IsFalse(Result.bValid)); ASSERT_THAT(IsTrue(Result.Stage == EAngelscriptBindingStage::Recording));
		ASSERT_THAT(IsTrue(Result.Message.Contains(TEXT("provider failed"))));
	}

	TEST_METHOD(RepeatedValidationAndRejectedMutationPreserveTheStore)
	{
		auto Store = RecordFixture();
		const auto First = FAngelscriptTypeBindInfoValidation::ValidateSealed(Store);
		const int32 TypeCount = Store->GetTypes().Num();
		const FString Declaration = Store->GetTypes()[0].Members[0].Declaration;
		FAngelscriptTypeBindInfoMember Changed = Store->GetTypes()[0].Members[0]; Changed.Declaration = TEXT("void Changed()");
		FString Diagnostic;
		ASSERT_THAT(IsFalse(Store->UpdateMember({{&Store.Get(), 0}, 0}, Changed, Diagnostic)));
		const auto Second = FAngelscriptTypeBindInfoValidation::ValidateSealed(Store);
		ASSERT_THAT(IsTrue(First.bValid && Second.bValid));
		ASSERT_THAT(IsTrue(First.Stage == Second.Stage));
		ASSERT_THAT(AreEqual(First.Message, Second.Message));
		ASSERT_THAT(AreEqual(TypeCount, Store->GetTypes().Num()));
		ASSERT_THAT(AreEqual(Declaration, Store->GetTypes()[0].Members[0].Declaration));
	}
};
#endif
