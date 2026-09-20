#include "CQTest.h"
#include "Framework/Catalog/AngelscriptTestCode.h"
#include "Framework/Catalog/AngelscriptTestCodeRegistration.h"
#include "Framework/Source/AngelscriptTestCodeBuilder.h"
#include "Framework/Source/AngelscriptTestSourceDescriptor.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest
{
struct FAngelscriptTestCodeRegistrationTestAccess
{
	static TUniquePtr<FAngelscriptTestCodeRegistry> CreateRegistry()
	{
		return TUniquePtr<FAngelscriptTestCodeRegistry>(new FAngelscriptTestCodeRegistry());
	}

	static TUniquePtr<FAngelscriptTestCode> CreateCode()
	{
		return TUniquePtr<FAngelscriptTestCode>(new FAngelscriptTestCode());
	}

	static TUniquePtr<FAngelscriptTestCodeRegistration> Register(
		FAngelscriptTestCodeRegistry& Registry,
		FAngelscriptTestFileMeta FileMeta,
		FAngelscriptTestCodeRegistration::FCodeFactory Factory,
		const ANSICHAR* OwnerModule = "RegistrationTests")
	{
		return TUniquePtr<FAngelscriptTestCodeRegistration>(new FAngelscriptTestCodeRegistration(
			Registry,
			MoveTemp(FileMeta),
			Factory,
			OwnerModule,
			__builtin_FILE(),
			__builtin_LINE()));
	}

	static FAngelscriptTestStatus Activate(
		FAngelscriptTestCode& Code,
		FAngelscriptTestCodeRegistry& Registry)
	{
		return Code.ActivateRegistrations(Registry);
	}
};
}

namespace
{
using FAccess = AngelscriptTest::FAngelscriptTestCodeRegistrationTestAccess;

int32 GFactoryACount = 0;
int32 GFactoryBCount = 0;
int32 GLateFactoryCount = 0;
bool GReentryFailed = false;
AngelscriptTest::FAngelscriptTestCode* GReentryCode = nullptr;
AngelscriptTest::FAngelscriptTestCodeRegistry* GReentryRegistry = nullptr;

AngelscriptTest::FAngelscriptTestCodeBuildResult BuildFromMeta(
	const AngelscriptTest::FAngelscriptTestFileMeta& FileMeta,
	const ANSICHAR* Body)
{
	AngelscriptTest::FAngelscriptTestCodeBuilder Builder(FileMeta);
	Builder.AddRoot({
		.Tag = TEXT("root"),
		.Summary = TEXT("The root version."),
	}, AngelscriptTest::FAngelscriptTestSource::FromBytes(MakeArrayView(
		reinterpret_cast<const uint8*>(Body), FCStringAnsi::Strlen(Body))));
	return Builder.Build();
}

AngelscriptTest::FAngelscriptTestCodeBuildResult FactoryA(
	const AngelscriptTest::FAngelscriptTestFileMeta& FileMeta)
{
	++GFactoryACount;
	return BuildFromMeta(FileMeta, "A");
}

AngelscriptTest::FAngelscriptTestCodeBuildResult FactoryB(
	const AngelscriptTest::FAngelscriptTestFileMeta& FileMeta)
{
	++GFactoryBCount;
	return BuildFromMeta(FileMeta, "B");
}

AngelscriptTest::FAngelscriptTestCodeBuildResult ReentrantFactory(
	const AngelscriptTest::FAngelscriptTestFileMeta& FileMeta)
{
	++GFactoryBCount;
	GReentryFailed = !FAccess::Activate(*GReentryCode, *GReentryRegistry).IsSuccess();
	return BuildFromMeta(FileMeta, "R");
}

AngelscriptTest::FAngelscriptTestCodeBuildResult InvalidFactory(
	const AngelscriptTest::FAngelscriptTestFileMeta& FileMeta)
{
	AngelscriptTest::FAngelscriptTestCodeBuilder Builder(FileMeta);
	Builder.AddRoot({.Tag = TEXT("root"), .Summary = TEXT("")}, AS_TEST_SOURCE_EXACT("bad"));
	return Builder.Build();
}

AngelscriptTest::FAngelscriptTestCodeBuildResult LateFactory(
	const AngelscriptTest::FAngelscriptTestFileMeta& FileMeta)
{
	++GLateFactoryCount;
	return BuildFromMeta(FileMeta, "L");
}

AngelscriptTest::FAngelscriptTestCodeBuildResult BadDescriptorFactory(
	const AngelscriptTest::FAngelscriptTestFileMeta& FileMeta)
{
	AngelscriptTest::FAngelscriptTestCodeBuilder Builder(FileMeta);
	AngelscriptTest::FAngelscriptTestSourceDescriptor Descriptor;
	Descriptor.AuthoredPath = TEXT("Registration/BadDescriptor.as");
	Descriptor.AuthoredLine = 1;
	Descriptor.AuthoredEnd = 2;
	Descriptor.Annotations.Points.Add({.Name = TEXT("p"), .Offset = 3});
	Descriptor.OriginSpans.Add({.CleanBegin = 0, .AuthoredBegin = 0, .Length = 2});
	Builder.AddRoot(
		{.Tag = TEXT("root"), .Summary = TEXT("Invalid point.")},
		AS_TEST_SOURCE_EXACT("ab"),
		MoveTemp(Descriptor));
	return Builder.Build();
}

AngelscriptTest::FAngelscriptTestFileMeta MakeMeta(FString Tag)
{
	return {
		.Version = TEXT("v1"),
		.Tag = MoveTemp(Tag),
		.Summary = TEXT("A registered test file."),
	};
}

bool HasErrorCode(
	const TArray<AngelscriptTest::FAngelscriptTestError>& Errors,
	FStringView Code)
{
	return Errors.ContainsByPredicate([Code](const AngelscriptTest::FAngelscriptTestError& Error)
	{
		return Error.Code == Code;
	});
}
}

TEST_CLASS_WITH_FLAGS(
	Registration,
	"Angelscript.UnitTest.Framework",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(StaticDeferred)
	{
		GFactoryACount = 0;
		auto Registry = FAccess::CreateRegistry();
		auto Code = FAccess::CreateCode();
		auto Registration = FAccess::Register(
			*Registry, MakeMeta(TEXT("Language/Deferred")), &FactoryA);

		ASSERT_THAT(AreEqual(0, GFactoryACount));
		ASSERT_THAT(IsFalse(Code->Get(TEXT("Language/Deferred"), TEXT("root")).IsSuccess()));
		ASSERT_THAT(IsTrue(FAccess::Activate(*Code, *Registry).IsSuccess()));
		ASSERT_THAT(AreEqual(1, GFactoryACount));
		ASSERT_THAT(IsTrue(Code->Get(TEXT("Language/Deferred"), TEXT("root")).IsSuccess()));
	}

	TEST_METHOD(OnceAndReentry)
	{
		GFactoryACount = 0;
		GFactoryBCount = 0;
		GReentryFailed = false;
		auto Registry = FAccess::CreateRegistry();
		auto Code = FAccess::CreateCode();
		auto A = FAccess::Register(*Registry, MakeMeta(TEXT("Language/A")), &FactoryA);
		auto B = FAccess::Register(*Registry, MakeMeta(TEXT("Language/B")), &ReentrantFactory);
		GReentryCode = Code.Get();
		GReentryRegistry = Registry.Get();

		ASSERT_THAT(IsTrue(FAccess::Activate(*Code, *Registry).IsSuccess()));
		ASSERT_THAT(IsTrue(FAccess::Activate(*Code, *Registry).IsSuccess()));
		ASSERT_THAT(AreEqual(1, GFactoryACount));
		ASSERT_THAT(AreEqual(1, GFactoryBCount));
		ASSERT_THAT(IsTrue(GReentryFailed));
		GReentryCode = nullptr;
		GReentryRegistry = nullptr;
	}

	TEST_METHOD(BarrierBranches)
	{
		int32 DeferredCount = 0;
		FSimpleMulticastDelegate DeferredDelegate;
		AngelscriptTest::FAngelscriptTestCodeActivationBarrier DeferredBarrier(
			[&DeferredCount]() { ++DeferredCount; });
		DeferredBarrier.Install(false, DeferredDelegate);
		ASSERT_THAT(AreEqual(0, DeferredCount));
		ASSERT_THAT(IsTrue(DeferredDelegate.IsBound()));
		DeferredDelegate.Broadcast();
		ASSERT_THAT(AreEqual(1, DeferredCount));
		DeferredBarrier.Shutdown();
		ASSERT_THAT(IsFalse(DeferredDelegate.IsBound()));

		int32 ImmediateCount = 0;
		FSimpleMulticastDelegate ImmediateDelegate;
		AngelscriptTest::FAngelscriptTestCodeActivationBarrier ImmediateBarrier(
			[&ImmediateCount]() { ++ImmediateCount; });
		ImmediateBarrier.Install(true, ImmediateDelegate);
		ASSERT_THAT(AreEqual(1, ImmediateCount));
		ASSERT_THAT(IsFalse(ImmediateDelegate.IsBound()));
		ImmediateBarrier.Shutdown();
		ASSERT_THAT(IsFalse(ImmediateDelegate.IsBound()));
	}

	TEST_METHOD(FailureAndLate)
	{
		GFactoryACount = 0;
		GLateFactoryCount = 0;
		auto Registry = FAccess::CreateRegistry();
		auto Code = FAccess::CreateCode();
		auto Bad = FAccess::Register(*Registry, MakeMeta(TEXT("Language/Bad")), &InvalidFactory, "BadOwner");
		auto Good = FAccess::Register(*Registry, MakeMeta(TEXT("Language/Good")), &FactoryA, "GoodOwner");

		auto Status = FAccess::Activate(*Code, *Registry);
		ASSERT_THAT(IsFalse(Status.IsSuccess()));
		ASSERT_THAT(IsTrue(Code->IsActivated()));
		ASSERT_THAT(AreEqual(1, GFactoryACount));
		ASSERT_THAT(IsTrue(Code->Get(TEXT("Language/Good"), TEXT("root")).IsSuccess()));
		ASSERT_THAT(IsFalse(Code->Get(TEXT("Language/Bad"), TEXT("root")).IsSuccess()));

		auto Late = FAccess::Register(
			*Registry, MakeMeta(TEXT("Language/Late")), &LateFactory, "LateOwner");
		ASSERT_THAT(AreEqual(0, GLateFactoryCount));
		ASSERT_THAT(IsFalse(Code->Get(TEXT("Language/Late"), TEXT("root")).IsSuccess()));
		ASSERT_THAT(IsTrue(HasErrorCode(Code->GetRegistrationErrors(), TEXT("LateRegistration"))));
		ASSERT_THAT(IsFalse(FAccess::Activate(*Code, *Registry).IsSuccess()));
		ASSERT_THAT(IsTrue(Code->Get(TEXT("Language/Good"), TEXT("root")).IsSuccess()));
	}

	TEST_METHOD(BadDescriptorBatchIsIsolated)
	{
		auto Registry = FAccess::CreateRegistry();
		auto Code = FAccess::CreateCode();
		auto Bad = FAccess::Register(
			*Registry, MakeMeta(TEXT("Registration/BadDescriptor")), &BadDescriptorFactory, "BadDescriptorOwner");
		auto Good = FAccess::Register(
			*Registry, MakeMeta(TEXT("Registration/GoodDescriptor")), &FactoryA, "GoodDescriptorOwner");

		auto Status = FAccess::Activate(*Code, *Registry);
		ASSERT_THAT(IsFalse(Status.IsSuccess()));
		ASSERT_THAT(IsTrue(Code->IsActivated()));
		ASSERT_THAT(IsTrue(HasErrorCode(Code->GetRegistrationErrors(), TEXT("InvalidAnnotationOffset"))));
		ASSERT_THAT(IsFalse(Code->Get(TEXT("Registration/BadDescriptor"), TEXT("root")).IsSuccess()));
		ASSERT_THAT(IsTrue(Code->Get(TEXT("Registration/GoodDescriptor"), TEXT("root")).IsSuccess()));
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
