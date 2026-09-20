#include "CQTest.h"
#include "TypeOwnershipTestSupport.h"
#include "as_type_id_registry.h"

#include <memory>
#include <type_traits>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(TypeOwnershipDefinitionSet, "Angelscript.UnitTest.NativeEngine.TypeOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(UniqueCreate)
	{
		TUniquePtr<asCDefinitions> Created = asCDefinitions::Create();
		ASSERT_THAT(IsNotNull(Created.Get()));
		ASSERT_THAT(IsTrue(Created->GetState() == asEDefinitionState::Building));
	}

	TEST_METHOD(DiscardAfterAdopt)
	{
		using namespace TypeOwnershipTest;
		FDetachedDefinitionFixture Fixture;
		Fixture.Scope = TEXT("type-ownership-image");
		FPairMembers Pair;
		ASSERT_THAT(IsTrue(AddPairType(Fixture, TEXT("Pair"), Pair, true)));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		asCObjectType* Type = Pair.Type;
		asCScriptFunction* Sum = Pair.Sum;
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded,
			RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(IsNull(Fixture.Set.Get()));
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Type->GetEngine() == Engine.Ptr));
		ASSERT_THAT(IsTrue(Type->GetTypeId() > asTYPEID_LAST_PRIMITIVE));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, BindPairMethods(*Engine.Ptr, Pair)));
		FTypeOwnershipPair Host;
		ASSERT_THAT(AreEqual(PairSum, ExecuteThiscall(Engine.Ptr, Pair.RunSum, Pair.Sum, Host)));
		(void)Sum;
	}

	TEST_METHOD(NoSharedPtr)
	{
		static_assert(!std::is_base_of_v<std::enable_shared_from_this<asCDefinitions>, asCDefinitions>);
		static_assert(std::is_same_v<decltype(asCDefinitions::Create()), TUniquePtr<asCDefinitions>>);
		TUniquePtr<asCDefinitions> First = asCDefinitions::Create();
		TUniquePtr<asCDefinitions> Second = asCDefinitions::Create();
		ASSERT_THAT(IsTrue(First.Get() != Second.Get()));
		ASSERT_THAT(IsFalse(std::is_copy_constructible_v<asCDefinitions>));
	}

	TEST_METHOD(TwoUniqueHelpers)
	{
		using namespace TypeOwnershipTest;
		FDetachedDefinitionFixture First;
		FDetachedDefinitionFixture Second;
		First.Scope = TEXT("type-ownership-image-a");
		Second.Scope = TEXT("type-ownership-image-b");
		FPairMembers PairA;
		FPairMembers PairB;
		ASSERT_THAT(IsTrue(AddPairType(First, TEXT("Pair"), PairA, true)));
		ASSERT_THAT(IsTrue(AddPairType(Second, TEXT("Pair"), PairB, true)));
		ASSERT_THAT(IsTrue(First.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(Second.FinalizeAndFreeze()));
		FNativeEngineOwner EngineA;
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded,
			RegisterCompiledDefinitions(*EngineA.Ptr, MoveTemp(First.Set))));
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded,
			RegisterCompiledDefinitions(*EngineB.Ptr, MoveTemp(Second.Set))));
		ASSERT_THAT(IsTrue(PairA.Type != PairB.Type));
		ASSERT_THAT(IsTrue(PairA.Type->GetEngine() == EngineA.Ptr));
		ASSERT_THAT(IsTrue(PairB.Type->GetEngine() == EngineB.Ptr));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, BindPairMethods(*EngineA.Ptr, PairA)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, BindPairMethods(*EngineB.Ptr, PairB)));
		FTypeOwnershipPair Host;
		ASSERT_THAT(AreEqual(PairSum, ExecuteThiscall(EngineA.Ptr, PairA.RunSum, PairA.Sum, Host)));
		ASSERT_THAT(AreEqual(PairSum, ExecuteThiscall(EngineB.Ptr, PairB.RunSum, PairB.Sum, Host)));
	}
};

#endif
