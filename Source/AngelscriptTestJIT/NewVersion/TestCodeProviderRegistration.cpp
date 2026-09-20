#include "Framework/Catalog/AngelscriptTestCode.h"
#include "Framework/Catalog/AngelscriptTestCodeRegistration.h"
#include "Framework/Source/AngelscriptTestCodeBuilder.h"

#if WITH_ANGELSCRIPT_TESTS

namespace
{
const AngelscriptTest::FAngelscriptTestCode* GObservedTestCodeCenter = nullptr;

AngelscriptTest::FAngelscriptTestCodeBuildResult BuildSecondaryTestCode(
	const AngelscriptTest::FAngelscriptTestFileMeta& FileMeta)
{
	GObservedTestCodeCenter = &AngelscriptTest::FAngelscriptTestCode::GetInstance();
	AngelscriptTest::FAngelscriptTestCodeBuilder Builder(FileMeta);
	Builder.AddRoot({
		.Tag = TEXT("root"),
		.Summary = TEXT("Define the initial secondary fixture."),
		.Topics = {TEXT("Baseline")},
	}, AS_TEST_SOURCE_EXACT("class Secondary { int Value = 1; }\n"));
	Builder.AddVersion({
		.Tag = TEXT("changed"),
		.Parent = TEXT("root"),
		.Summary = TEXT("Change the complete secondary fixture."),
		.Topics = {TEXT("Reload")},
	}, AS_TEST_SOURCE_EXACT(
		"class Secondary { int Value = 2; string Label = \"changed\"; }\n"));
	return Builder.Build();
}

AngelscriptTest::FAngelscriptTestCodeRegistration SecondaryTestCodeRegistration(
	{
		.Version = TEXT("v1"),
		.Tag = TEXT("Fixture/Secondary"),
		.Summary = TEXT("A secondary module fixture."),
		.Topics = {TEXT("Adoption"), TEXT("Reload")},
	},
	&BuildSecondaryTestCode);
}

extern "C" ANGELSCRIPTTESTJIT_API const void* GetAngelscriptTestCodeCenterObservedByJIT()
{
	return GObservedTestCodeCenter;
}

#endif // WITH_ANGELSCRIPT_TESTS
