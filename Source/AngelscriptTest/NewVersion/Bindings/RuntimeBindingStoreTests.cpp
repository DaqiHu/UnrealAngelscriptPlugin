#include "CQTest.h"
#include "Core/AngelscriptTypeBindInfoStore.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(Store, "Angelscript.UnitTest.RuntimeBindings.Recording",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FPair { int32 X; int32 Y; };

	static FAngelscriptTypeBindDeclaration Pair(const TCHAR* Provider = TEXT("Pair.Type"))
	{
		FAngelscriptTypeBindDeclaration Result;
		Result.Name = TEXT("Pair");
		Result.Namespace = TEXT("Fixture");
		Result.NativeSize = sizeof(FPair);
		Result.NativeAlignment = alignof(FPair);
		Result.Source.OwnerModule = TEXT("AngelscriptTest");
		Result.Source.Provider = Provider;
		Result.Source.File = TEXT("RuntimeBindingStoreTests.cpp");
		Result.Source.Line = 20;
		return Result;
	}

public:
	TEST_METHOD(StoreOwnsInputLifetime)
	{
		FAngelscriptTypeBindInfoStore Records;
		FString Diagnostic;
		auto Input = Pair();
		FAngelscriptTypeBindHandle Type;
		ASSERT_THAT(IsTrue(Records.RecordType(Input, Type, Diagnostic)));
		FAngelscriptTypeBindInfoMember Member;
		Member.Kind = EAngelscriptTypeBindMemberKind::Property;
		Member.Declaration = TEXT("int X");
		Member.PropertyOffset = STRUCT_OFFSET(FPair, X);
		Member.Source = Input.Source;
		FAngelscriptTypeBindMemberHandle Property;
		ASSERT_THAT(IsTrue(Records.RecordMember(Type, Member, Property, Diagnostic)));
		Input.Name = TEXT("Changed");
		Input.Source.File = TEXT("Changed.cpp");
		Member.Declaration = TEXT("int Changed");
		ASSERT_THAT(IsTrue(Records.Seal(Diagnostic)));
		const auto* Recorded = Records.GetType(Type);
		ASSERT_THAT(IsNotNull(Recorded));
		ASSERT_THAT(AreEqual(FString(TEXT("Pair")), Recorded->Declaration.Name));
		ASSERT_THAT(AreEqual(uint32(sizeof(FPair)), Recorded->Declaration.NativeSize));
		ASSERT_THAT(AreEqual(FString(TEXT("RuntimeBindingStoreTests.cpp")), Recorded->Declaration.Source.File));
		const auto* RecordedProperty = Records.GetMember(Property);
		ASSERT_THAT(IsNotNull(RecordedProperty));
		ASSERT_THAT(AreEqual(FString(TEXT("int X")), RecordedProperty->Declaration));
	}

	TEST_METHOD(EmptyStoreSeals)
	{
		FAngelscriptTypeBindInfoStore Records;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Records.Seal(Diagnostic)));
		ASSERT_THAT(IsTrue(Records.IsSealed()));
		ASSERT_THAT(AreEqual(0, Records.GetTypes().Num()));
	}

	TEST_METHOD(RepeatedSealPreservesIdentity)
	{
		FAngelscriptTypeBindInfoStore Records;
		FString Diagnostic;
		FAngelscriptTypeBindHandle Type;
		ASSERT_THAT(IsTrue(Records.RecordType(Pair(), Type, Diagnostic)));
		ASSERT_THAT(IsTrue(Records.Seal(Diagnostic)));
		const auto* Before = Records.GetType(Type);
		ASSERT_THAT(IsTrue(Records.Seal(Diagnostic)));
		ASSERT_THAT(IsTrue(Records.FindType(TEXT("Pair"), TEXT("Fixture")) == Type));
		ASSERT_THAT(IsTrue(Before == Records.GetType(Type)));
		ASSERT_THAT(AreEqual(1, Records.GetTypes().Num()));
	}

	TEST_METHOD(SealedMutationRejected)
	{
		FAngelscriptTypeBindInfoStore Records;
		FString Diagnostic;
		FAngelscriptTypeBindHandle Type;
		ASSERT_THAT(IsTrue(Records.RecordType(Pair(), Type, Diagnostic)));
		FAngelscriptTypeBindInfoMember Member;
		Member.Declaration = TEXT("int Sum() const");
		FAngelscriptTypeBindMemberHandle Method;
		ASSERT_THAT(IsTrue(Records.RecordMember(Type, Member, Method, Diagnostic)));
		ASSERT_THAT(IsTrue(Records.Seal(Diagnostic)));
		Member.Declaration = TEXT("int Corrupted()");
		ASSERT_THAT(IsFalse(Records.UpdateMember(Method, Member, Diagnostic)));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("sealed"))));
		FAngelscriptTypeBindMemberHandle ExtraMethod;
		ASSERT_THAT(IsFalse(Records.RecordMember(Type, Member, ExtraMethod, Diagnostic)));
		auto Extra = Pair(); Extra.Name = TEXT("Extra");
		FAngelscriptTypeBindHandle ExtraType;
		ASSERT_THAT(IsFalse(Records.RecordType(Extra, ExtraType, Diagnostic)));
		ASSERT_THAT(IsTrue(Records.IsSealed()));
		ASSERT_THAT(AreEqual(1, Records.GetTypes().Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("int Sum() const")), Records.GetMember(Method)->Declaration));
		ASSERT_THAT(AreEqual(1, Records.GetType(Type)->Members.Num()));
	}

	TEST_METHOD(ConflictingNativeDefinitionReportsBothSources)
	{
		for (int32 Conflict = 0; Conflict != 3; ++Conflict)
		{
			FAngelscriptTypeBindInfoStore Records;
			FString Diagnostic;
			FAngelscriptTypeBindHandle Original, Rejected;
			ASSERT_THAT(IsTrue(Records.RecordType(Pair(TEXT("First.Provider")), Original, Diagnostic)));
			auto Incompatible = Pair(TEXT("Second.Provider"));
			if (Conflict == 0) Incompatible.NativeSize += 4;
			if (Conflict == 1) Incompatible.NativeAlignment *= 2;
			if (Conflict == 2) Incompatible.Kind = EAngelscriptTypeBindKind::Enum;
			ASSERT_THAT(IsFalse(Records.RecordType(Incompatible, Rejected, Diagnostic)));
			ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("First.Provider"))));
			ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Second.Provider"))));
			ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Pair"))));
			ASSERT_THAT(IsFalse(Records.Seal(Diagnostic)));
			ASSERT_THAT(AreEqual(1, Records.GetTypes().Num()));
		}
	}

	TEST_METHOD(CompatibleDeclarationsCoalesce)
	{
		FAngelscriptTypeBindInfoStore Records;
		FString Diagnostic;
		FAngelscriptTypeBindHandle First, Second;
		ASSERT_THAT(IsTrue(Records.RecordType(Pair(TEXT("First.Provider")), First, Diagnostic)));
		ASSERT_THAT(IsTrue(Records.RecordType(Pair(TEXT("Second.Provider")), Second, Diagnostic)));
		ASSERT_THAT(IsTrue(Records.Seal(Diagnostic)));
		ASSERT_THAT(IsTrue(First == Second));
		ASSERT_THAT(AreEqual(1, Records.GetTypes().Num()));
		ASSERT_THAT(AreEqual(2, Records.GetType(First)->Sources.Num()));
	}

	TEST_METHOD(RecordHandlesSurviveGrowthAndRejectForeignStores)
	{
		FAngelscriptTypeBindInfoStore Records, Other;
		FString Diagnostic;
		FAngelscriptTypeBindHandle First;
		ASSERT_THAT(IsTrue(Records.RecordType(Pair(), First, Diagnostic)));
		FAngelscriptTypeBindInfoMember Member;
		Member.Declaration = TEXT("int Sum() const");
		FAngelscriptTypeBindMemberHandle Method;
		ASSERT_THAT(IsTrue(Records.RecordMember(First, Member, Method, Diagnostic)));
		for (int32 Index = 0; Index != 64; ++Index)
		{
			auto Extra = Pair(); Extra.Name = FString::Printf(TEXT("Extra%d"), Index);
			FAngelscriptTypeBindHandle ExtraType;
			ASSERT_THAT(IsTrue(Records.RecordType(Extra, ExtraType, Diagnostic)));
		}
		Member.Declaration = TEXT("int Updated() const");
		ASSERT_THAT(IsTrue(Records.UpdateMember(Method, Member, Diagnostic)));
		ASSERT_THAT(AreEqual(FString(TEXT("int Updated() const")), Records.GetMember(Method)->Declaration));
		ASSERT_THAT(IsNull(Other.GetType(First)));
		ASSERT_THAT(IsNull(Other.GetMember(Method)));
		ASSERT_THAT(IsFalse(Other.UpdateMember(Method, Member, Diagnostic)));
		ASSERT_THAT(AreEqual(FString(TEXT("Pair")), Records.GetType(First)->Declaration.Name));
	}
};

#endif
