#include "CQTest.h"
#include "RuntimeBindingTestSupport.h"
#include "Core/AngelscriptTypeBindInfoReflection.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
using namespace RuntimeBindingTestSupport;

TEST_CLASS_WITH_FLAGS(Ownership, "Angelscript.UnitTest.RuntimeBindings.Recording",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 AddOne(int32 Value) { return Value + 1; }
	static int32 AddTwo(int32 Value) { return Value + 2; }

	static const FAngelscriptTypeBindInfoMember* FindMember(
		const FAngelscriptTypeBindInfoStore& Store, const FString& Scope, const FString& Declaration)
	{
		const auto* Type = Store.GetType(Store.FindScope(Scope));
		if (Type == nullptr) return nullptr;
		return Type->Members.FindByPredicate([&](const auto& Member)
		{
			return Member.Declaration.Equals(Declaration, ESearchCase::CaseSensitive);
		});
	}

public:
	TEST_METHOD(RootNamedScopesAndNominalTypesKeepDistinctIdentity)
	{
		FAngelscriptTypeBindInfoStore Store;
		Store.SetSource({TEXT("AngelscriptTest"), TEXT("Ownership.Scopes"), TEXT("RuntimeBindingOwnershipTests.cpp"), 31});
		FAngelscriptBinds Binds(Store);
		int32 RootValue = 3;
		int32 NamedValue = 5;
		Binds.BindGlobalFunctionForTarget("int Add(int Value)", &AddOne);
		Binds.BindGlobalVariableForTarget("int Value", &RootValue);
		Binds.ValueClassForTarget<FPair>("Test", {});
		{
			FAngelscriptBinds::FNamespace Namespace(Binds, "Test");
			Binds.BindGlobalFunctionForTarget("int Add(int Value)", &AddTwo);
			Binds.BindGlobalVariableForTarget("int Value", &NamedValue);
		}
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Store.Seal(Diagnostic)));
		ASSERT_THAT(IsNotNull(Store.GetType(Store.FindType(TEXT("Test")))));
		ASSERT_THAT(IsNotNull(Store.GetType(Store.FindScope(TEXT("Test")))));
		ASSERT_THAT(IsFalse(Store.FindType(TEXT("Test")) == Store.FindScope(TEXT("Test"))));
		ASSERT_THAT(IsNotNull(FindMember(Store, TEXT(""), TEXT("int Add(int Value)"))));
		ASSERT_THAT(IsNotNull(FindMember(Store, TEXT("Test"), TEXT("int Add(int Value)"))));
		ASSERT_THAT(IsTrue(FindMember(Store, TEXT(""), TEXT("int Value"))->GlobalAddress == &RootValue));
		ASSERT_THAT(IsTrue(FindMember(Store, TEXT("Test"), TEXT("int Value"))->GlobalAddress == &NamedValue));
	}

	TEST_METHOD(CopiedConstantsAndBorrowedTargetsExposeTruthfulLifetimes)
	{
		FAngelscriptTypeBindInfoStore Store;
		Store.SetSource({TEXT("AngelscriptTest"), TEXT("Ownership.Lifetimes"), TEXT("RuntimeBindingOwnershipTests.cpp"), 57});
		FAngelscriptBinds Binds(Store);
		int32 ConstantSource = 7;
		int32 BorrowedValue = 4;
		int32 Auxiliary = 13;
		Binds.BindGlobalVariableForTarget("const int Answer", &ConstantSource).PureConstant(7);
		Binds.BindGlobalVariableForTarget("int Mutable", &BorrowedValue);
		Binds.BindGlobalFunctionForTarget("int Add(int Value)", &AddOne, &Auxiliary);
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Store.Seal(Diagnostic)));
		ConstantSource = 99;
		BorrowedValue = 9;
		const auto* Answer = FindMember(Store, TEXT(""), TEXT("const int Answer"));
		const auto* Mutable = FindMember(Store, TEXT(""), TEXT("int Mutable"));
		const auto* Add = FindMember(Store, TEXT(""), TEXT("int Add(int Value)"));
		ASSERT_THAT(IsNotNull(Answer)); ASSERT_THAT(IsNotNull(Mutable)); ASSERT_THAT(IsNotNull(Add));
		ASSERT_THAT(AreEqual(uint64(7), Answer->ConstantValue));
		ASSERT_THAT(IsTrue(Answer->GlobalStorageLifetime == EAngelscriptBindHostLifetime::CopiedRecord));
		ASSERT_THAT(IsTrue(Mutable->GlobalStorageLifetime == EAngelscriptBindHostLifetime::BorrowedHost));
		ASSERT_THAT(AreEqual(9, *static_cast<const int32*>(Mutable->GlobalAddress)));
		ASSERT_THAT(IsTrue(Add->Native.FunctionLifetime == EAngelscriptBindHostLifetime::BorrowedHost));
		ASSERT_THAT(IsTrue(Add->Native.AuxiliaryLifetime == EAngelscriptBindHostLifetime::BorrowedHost));
	}

	TEST_METHOD(DifferentPolicyCapturesDoNotShareMutableRecords)
	{
		FAngelscriptBindRecordingPolicy EditorPolicy;
		EditorPolicy.bEditor = true;
		FAngelscriptBindRecordingPolicy GamePolicy = EditorPolicy;
		GamePolicy.bEditor = false;
		FAngelscriptTypeBindInfoStore Editor(EditorPolicy);
		FAngelscriptTypeBindInfoStore Game(GamePolicy);
		FAngelscriptTypeBindDeclaration EditorType;
		EditorType.Name = TEXT("EditorOnly");
		EditorType.Source = {TEXT("AngelscriptTest"), TEXT("Ownership.Policy"), TEXT("RuntimeBindingOwnershipTests.cpp"), 89};
		FAngelscriptTypeBindHandle EditorHandle;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Editor.RecordType(EditorType, EditorHandle, Diagnostic)));
		ASSERT_THAT(IsNull(Game.GetType(Game.FindType(TEXT("EditorOnly")))));
		ASSERT_THAT(IsTrue(Editor.GetPolicy().bEditor));
		ASSERT_THAT(IsFalse(Game.GetPolicy().bEditor));
		ASSERT_THAT(IsFalse(Game.GetType(EditorHandle) != nullptr));
	}

	TEST_METHOD(TwoOwnersRetainOneSnapshotUntilTheLastOwnerReleases)
	{
		TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture({}, {}, Snapshot, Diagnostic)));
		TWeakPtr<const FAngelscriptReflectedTypeSnapshot> WeakSnapshot = Snapshot;
		TSharedPtr<FAngelscriptTypeBindInfoStore> Store = RecordFixture(false, false);
		ASSERT_THAT(IsTrue(Store->SetReflectedSnapshot(Snapshot.ToSharedRef(), Diagnostic)));
		ASSERT_THAT(IsTrue(Store->Seal(Diagnostic)));
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		auto A = FAngelscriptEngine::CreateForBindings(Store.ToSharedRef(), BindingDiagnostic);
		auto B = FAngelscriptEngine::CreateForBindings(Store.ToSharedRef(), BindingDiagnostic);
		ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid()));
		Store.Reset(); Snapshot.Reset();
		ASSERT_THAT(IsTrue(WeakSnapshot.IsValid()));
		A.Reset();
		ASSERT_THAT(IsTrue(WeakSnapshot.IsValid()));
		ASSERT_THAT(IsNotNull(Function(*B, TEXT("int Add(int A, int B)"))));
		B.Reset();
		ASSERT_THAT(IsFalse(WeakSnapshot.IsValid()));
	}

	TEST_METHOD(SealRejectsEveryMutationAndPreservesSnapshotAndProvenance)
	{
		TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture({}, {}, Snapshot, Diagnostic)));
		auto Store = RecordFixture(false, false);
		ASSERT_THAT(IsTrue(Store->SetReflectedSnapshot(Snapshot.ToSharedRef(), Diagnostic)));
		const auto Type = Store->FindType(TEXT("Pair"));
		const FAngelscriptTypeBindMemberHandle Member{Type, 0};
		const FString OriginalDeclaration = Store->GetMember(Member)->Declaration;
		const FAngelscriptTypeBindSource OriginalSource = Store->GetType(Type)->Declaration.Source;
		ASSERT_THAT(IsTrue(Store->Seal(Diagnostic)));
		FAngelscriptTypeBindDeclaration Extra; Extra.Name = TEXT("Extra"); Extra.Source = OriginalSource;
		FAngelscriptTypeBindHandle ExtraHandle;
		FAngelscriptTypeBindInfoMember Changed = *Store->GetMember(Member); Changed.Declaration = TEXT("void Changed()");
		FAngelscriptTypeBindMemberHandle ExtraMember;
		FAngelscriptRecordedProvider Provider; Provider.Source = OriginalSource;
		ASSERT_THAT(IsFalse(Store->RecordType(Extra, ExtraHandle, Diagnostic)));
		ASSERT_THAT(IsFalse(Store->RecordMember(Type, Changed, ExtraMember, Diagnostic)));
		ASSERT_THAT(IsFalse(Store->UpdateMember(Member, Changed, Diagnostic)));
		ASSERT_THAT(IsFalse(Store->RecordProvider(Provider, Diagnostic)));
		ASSERT_THAT(IsFalse(Store->SetSource({TEXT("Other"), TEXT("Other"), TEXT("Other.cpp"), 1})));
		ASSERT_THAT(IsFalse(Store->SetNamespace(TEXT("Other"))));
		ASSERT_THAT(IsFalse(Store->SetReflectedSnapshot(Snapshot.ToSharedRef(), Diagnostic)));
		ASSERT_THAT(AreEqual(2, Store->GetTypes().Num()));
		ASSERT_THAT(AreEqual(OriginalDeclaration, Store->GetMember(Member)->Declaration));
		ASSERT_THAT(AreEqual(OriginalSource.Provider, Store->GetType(Type)->Declaration.Source.Provider));
		ASSERT_THAT(IsTrue(Store->GetReflectedSnapshot().Get() == Snapshot.Get()));
	}

	TEST_METHOD(ReflectionPolicyMismatchPublishesNoAttachmentOrTypes)
	{
		FAngelscriptBindRecordingPolicy Policy;
		Policy.bSimulateCooked = true;
		TSharedPtr<const FAngelscriptReflectedTypeSnapshot> Snapshot;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoReflection::Capture({}, Policy, Snapshot, Diagnostic)));
		FAngelscriptTypeBindInfoStore Store;
		ASSERT_THAT(IsFalse(FAngelscriptTypeBindInfoReflection::RecordDeclarations(Store, Snapshot.ToSharedRef(), Diagnostic)));
		ASSERT_THAT(IsFalse(Store.GetReflectedSnapshot().IsValid()));
		ASSERT_THAT(AreEqual(0, Store.GetTypes().Num()));
	}
};
#endif
