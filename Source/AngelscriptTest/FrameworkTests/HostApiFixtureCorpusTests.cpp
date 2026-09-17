#include "CQTest.h"
#include "Framework/Catalog/AngelscriptTestCode.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

static bool ContainsAscii(const TConstArrayView<uint8> Bytes, const char* Needle)
{
	const int32 NeedleLen = FCStringAnsi::Strlen(Needle);
	if (NeedleLen <= 0 || Bytes.Num() < NeedleLen)
	{
		return false;
	}
	for (int32 Index = 0; Index + NeedleLen <= Bytes.Num(); ++Index)
	{
		bool bMatch = true;
		for (int32 NeedleIndex = 0; NeedleIndex < NeedleLen; ++NeedleIndex)
		{
			if (Bytes[Index + NeedleIndex] != static_cast<uint8>(Needle[NeedleIndex]))
			{
				bMatch = false;
				break;
			}
		}
		if (bMatch)
		{
			return true;
		}
	}
	return false;
}

TEST_CLASS_WITH_FLAGS(
	HostApiFixtureCorpus,
	"Angelscript.UnitTest.Framework",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CorpusHasTArrayPrefix)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		ASSERT_THAT(IsTrue(Code.IsActivated()));
		ASSERT_THAT(AreEqual(0, Code.GetRegistrationErrors().Num()));

		TArray<AngelscriptTest::FAngelscriptTestFileMeta> ContainerFiles;
		const TArray<FString> ContainersTopic = {FString(TEXT("Containers"))};
		const auto ContainerStatus = Code.FindFiles(ContainersTopic, ContainerFiles);
		ASSERT_THAT(IsTrue(ContainerStatus.IsSuccess()));

		TArray<AngelscriptTest::FAngelscriptTestFileMeta> UnrealFiles;
		const TArray<FString> UnrealTopic = {FString(TEXT("Unreal"))};
		const auto UnrealStatus = Code.FindFiles(UnrealTopic, UnrealFiles);
		ASSERT_THAT(IsTrue(UnrealStatus.IsSuccess()));

		bool bHasTArrayPrefix = false;
		bool bHasFlatTArray = false;
		for (const AngelscriptTest::FAngelscriptTestFileMeta& File : ContainerFiles)
		{
			bHasTArrayPrefix = bHasTArrayPrefix || File.Tag.StartsWith(TEXT("Containers/TArray/"));
			bHasFlatTArray = bHasFlatTArray || File.Tag == TEXT("Containers/TArray");
		}
		bool bHasFMath = false;
		for (const AngelscriptTest::FAngelscriptTestFileMeta& File : UnrealFiles)
		{
			bHasFMath = bHasFMath || File.Tag == TEXT("Unreal/FMath");
		}
		ASSERT_THAT(IsTrue(bHasTArrayPrefix));
		ASSERT_THAT(IsFalse(bHasFlatTArray));
		ASSERT_THAT(IsTrue(bHasFMath));
	}

	TEST_METHOD(CorpusHasTArrayAddAndOrder)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		ASSERT_THAT(IsTrue(Code.IsActivated()));
		ASSERT_THAT(AreEqual(0, Code.GetRegistrationErrors().Num()));

		const auto AddAndOrder = Code.Get(TEXT("Containers/TArray/AddAndOrder"), TEXT("AddAndOrder"));
		ASSERT_THAT(IsTrue(AddAndOrder.IsSuccess()));
		ASSERT_THAT(IsFalse(Code.Get(TEXT("Containers/TArray"), TEXT("AddAndOrder")).IsSuccess()));

		const auto IndexOutOfBounds = Code.Get(
			TEXT("Containers/TArray/RuntimeFail/IndexOutOfBounds"),
			TEXT("IndexOutOfBounds"));
		ASSERT_THAT(IsTrue(IndexOutOfBounds.IsSuccess()));
	}

	TEST_METHOD(CorpusHasTArrayTypeAndDirection)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		ASSERT_THAT(IsTrue(Code.IsActivated()));
		ASSERT_THAT(AreEqual(0, Code.GetRegistrationErrors().Num()));

		const auto AddAndOrderFString = Code.Get(
			TEXT("Containers/TArray/AddAndOrderFString"),
			TEXT("AddAndOrderFString"));
		ASSERT_THAT(IsTrue(AddAndOrderFString.IsSuccess()));
		if (AddAndOrderFString.IsSuccess())
		{
			const AngelscriptTest::FAngelscriptTestSource Source = AddAndOrderFString.GetCase()->GetSource();
			const TConstArrayView<uint8> Bytes = Source.GetBytes();
			ASSERT_THAT(IsTrue(ContainsAscii(Bytes, "TArray<FString>")));
			ASSERT_THAT(IsTrue(ContainsAscii(Bytes, ".Add(")));
		}

		const auto FillByAdd = Code.Get(TEXT("Containers/TArray/FillByAdd"), TEXT("FillByAdd"));
		ASSERT_THAT(IsTrue(FillByAdd.IsSuccess()));
		if (FillByAdd.IsSuccess())
		{
			const AngelscriptTest::FAngelscriptTestSource Source = FillByAdd.GetCase()->GetSource();
			const TConstArrayView<uint8> Bytes = Source.GetBytes();
			ASSERT_THAT(IsTrue(ContainsAscii(Bytes, "TArray<int32>&out")));
		}

		const auto AddAndOrder = Code.Get(TEXT("Containers/TArray/AddAndOrder"), TEXT("AddAndOrder"));
		ASSERT_THAT(IsTrue(AddAndOrder.IsSuccess()));
	}

	TEST_METHOD(CorpusHasTMapTSetOptionalTypeAndDirection)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		ASSERT_THAT(IsTrue(Code.IsActivated()));
		ASSERT_THAT(AreEqual(0, Code.GetRegistrationErrors().Num()));

		const auto AddPairFString = Code.Get(
			TEXT("Containers/TMap/AddPairInsertsKeyValueFString"),
			TEXT("AddPairInsertsKeyValueFString"));
		ASSERT_THAT(IsTrue(AddPairFString.IsSuccess()));
		if (AddPairFString.IsSuccess())
		{
			const AngelscriptTest::FAngelscriptTestSource Source = AddPairFString.GetCase()->GetSource();
			const TConstArrayView<uint8> Bytes = Source.GetBytes();
			ASSERT_THAT(IsTrue(ContainsAscii(Bytes, "TMap<FString, int>")));
			ASSERT_THAT(IsTrue(ContainsAscii(Bytes, ".Add(")));
		}

		const auto FillByAddPair = Code.Get(
			TEXT("Containers/TMap/FillByAddPairInsertsKeyValue"),
			TEXT("FillByAddPairInsertsKeyValue"));
		ASSERT_THAT(IsTrue(FillByAddPair.IsSuccess()));
		if (FillByAddPair.IsSuccess())
		{
			const AngelscriptTest::FAngelscriptTestSource Source = FillByAddPair.GetCase()->GetSource();
			const TConstArrayView<uint8> Bytes = Source.GetBytes();
			ASSERT_THAT(IsTrue(ContainsAscii(Bytes, "TMap<int, int>&out")));
		}

		const auto AddElementFString = Code.Get(
			TEXT("Containers/TSet/AddElementIsContainedFString"),
			TEXT("AddElementIsContainedFString"));
		ASSERT_THAT(IsTrue(AddElementFString.IsSuccess()));
		if (AddElementFString.IsSuccess())
		{
			const AngelscriptTest::FAngelscriptTestSource Source = AddElementFString.GetCase()->GetSource();
			const TConstArrayView<uint8> Bytes = Source.GetBytes();
			ASSERT_THAT(IsTrue(ContainsAscii(Bytes, "TSet<FString>")));
			ASSERT_THAT(IsTrue(ContainsAscii(Bytes, ".Add(")));
		}

		const auto FillByAddElement = Code.Get(
			TEXT("Containers/TSet/FillByAddElementIsContained"),
			TEXT("FillByAddElementIsContained"));
		ASSERT_THAT(IsTrue(FillByAddElement.IsSuccess()));
		if (FillByAddElement.IsSuccess())
		{
			const AngelscriptTest::FAngelscriptTestSource Source = FillByAddElement.GetCase()->GetSource();
			const TConstArrayView<uint8> Bytes = Source.GetBytes();
			ASSERT_THAT(IsTrue(ContainsAscii(Bytes, "TSet<int32>&out")));
		}

		const auto SetValueFString = Code.Get(
			TEXT("Containers/TOptional/SetValueFString"),
			TEXT("SetValueFString"));
		ASSERT_THAT(IsTrue(SetValueFString.IsSuccess()));
		if (SetValueFString.IsSuccess())
		{
			const AngelscriptTest::FAngelscriptTestSource Source = SetValueFString.GetCase()->GetSource();
			const TConstArrayView<uint8> Bytes = Source.GetBytes();
			ASSERT_THAT(IsTrue(ContainsAscii(Bytes, "TOptional<FString>")));
			ASSERT_THAT(IsTrue(ContainsAscii(Bytes, ".Set(")));
		}

		const auto FillBySetValue = Code.Get(
			TEXT("Containers/TOptional/FillBySetValue"),
			TEXT("FillBySetValue"));
		ASSERT_THAT(IsTrue(FillBySetValue.IsSuccess()));
		if (FillBySetValue.IsSuccess())
		{
			const AngelscriptTest::FAngelscriptTestSource Source = FillBySetValue.GetCase()->GetSource();
			const TConstArrayView<uint8> Bytes = Source.GetBytes();
			ASSERT_THAT(IsTrue(ContainsAscii(Bytes, "TOptional<int32>&out")));
		}

		ASSERT_THAT(IsTrue(
			Code.Get(TEXT("Containers/TMap/AddPairInsertsKeyValue"), TEXT("AddPairInsertsKeyValue")).IsSuccess()));
		ASSERT_THAT(IsTrue(
			Code.Get(TEXT("Containers/TSet/AddElementIsContained"), TEXT("AddElementIsContained")).IsSuccess()));
		ASSERT_THAT(IsTrue(Code.Get(TEXT("Containers/TOptional/SetValue"), TEXT("SetValue")).IsSuccess()));
		ASSERT_THAT(IsTrue(Code.Get(TEXT("Containers/TMap/ContainsKeyIn"), TEXT("ContainsKeyIn")).IsSuccess()));
		ASSERT_THAT(IsTrue(
			Code.Get(TEXT("Containers/TArray/AddAndOrderFString"), TEXT("AddAndOrderFString")).IsSuccess()));
	}

	TEST_METHOD(CorpusHasPendingMathFunctionIn)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		ASSERT_THAT(IsTrue(Code.IsActivated()));
		ASSERT_THAT(AreEqual(0, Code.GetRegistrationErrors().Num()));

		const auto FunctionIn = Code.Get(TEXT("Unreal/FVector"), TEXT("function-parameters-in"));
		ASSERT_THAT(IsTrue(FunctionIn.IsSuccess()));
		ASSERT_THAT(IsFalse(Code.Get(TEXT("Unreal/FVector"), TEXT("root")).IsSuccess()));
		if (!FunctionIn.IsSuccess())
		{
			return;
		}

		const AngelscriptTest::FAngelscriptTestSource Source = FunctionIn.GetCase()->GetSource();
		const TConstArrayView<uint8> Bytes = Source.GetBytes();
		bool bHasInRef = false;
		for (int32 Index = 0; Index + 2 < Bytes.Num(); ++Index)
		{
			if (Bytes[Index] == static_cast<uint8>('&')
				&& Bytes[Index + 1] == static_cast<uint8>('i')
				&& Bytes[Index + 2] == static_cast<uint8>('n'))
			{
				bHasInRef = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bHasInRef));
	}

	TEST_METHOD(LanguageTopicOmitsHostApi)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		ASSERT_THAT(IsTrue(Code.IsActivated()));

		TArray<AngelscriptTest::FAngelscriptTestFileMeta> LanguageFiles;
		const TArray<FString> LanguageTopic = {FString(TEXT("Language"))};
		const auto Status = Code.FindFiles(LanguageTopic, LanguageFiles);
		ASSERT_THAT(IsTrue(Status.IsSuccess()));

		for (const AngelscriptTest::FAngelscriptTestFileMeta& File : LanguageFiles)
		{
			ASSERT_THAT(IsFalse(File.Tag.StartsWith(TEXT("Containers/"))));
			ASSERT_THAT(IsFalse(File.Tag == TEXT("Unreal/FMath")));
		}
	}
};

#endif
