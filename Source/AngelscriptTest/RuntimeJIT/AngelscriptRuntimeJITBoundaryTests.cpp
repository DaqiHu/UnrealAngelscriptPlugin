#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptRuntimeJITBoundaryTests,
	"Angelscript.TestModule.RuntimeJIT.Boundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(WorkerAndLoweringSurfaceHasNoEngineOrAotStorageDependency)
	{
		const FString RuntimeSourceRoot = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(
				FPaths::ProjectPluginsDir(),
				TEXT("Angelscript/Source/AngelscriptRuntime")));
		const TArray<FString> WorkerFiles = {
			TEXT("Public/JIT/AngelscriptRuntimeJITBackend.h"),
			TEXT("RuntimeJIT/AngelscriptRuntimeJITBackend.cpp"),
			TEXT("RuntimeJIT/AngelscriptRuntimeJITBackendRegistry.h"),
			TEXT("RuntimeJIT/AngelscriptRuntimeJITBackendRegistry.cpp"),
			TEXT("RuntimeJIT/AngelscriptRuntimeJITState.h"),
			TEXT("RuntimeJIT/AngelscriptRuntimeJITState.cpp"),
		};
		const TArray<FString> ForbiddenIncludeFragments = {
			TEXT("UObject/"),
			TEXT("UFunction"),
			TEXT("Blueprint"),
			TEXT("ClassGenerator/"),
			TEXT("Engine/World"),
			TEXT("GarbageCollection"),
			TEXT("Editor/"),
			TEXT("TypedHIR"),
			TEXT("TypedAST"),
			TEXT("StaticJIT/"),
		};
		const TArray<FString> ForbiddenStorageSymbols = {
			TEXT("FAngelscriptJITProviderRegistry"),
			TEXT("FAngelscriptStaticJIT"),
			TEXT("FAngelscriptStaticJITGenerator"),
			TEXT("FAngelscriptCacheService"),
			TEXT("CacheV2"),
			TEXT("PrecompiledData"),
			TEXT("TypedHIR"),
			TEXT("TypedAST"),
		};

		for (const FString& RelativePath : WorkerFiles)
		{
			const FString AbsolutePath =
				FPaths::Combine(RuntimeSourceRoot, RelativePath);
			FString Contents;
			ASSERT_THAT(IsTrue(
				FFileHelper::LoadFileToString(Contents, *AbsolutePath),
				*FString::Printf(
					TEXT("Runtime JIT boundary source must be readable: %s"),
					*AbsolutePath)));

			TArray<FString> Lines;
			Contents.ParseIntoArrayLines(Lines, false);
			for (const FString& Line : Lines)
			{
				const FString Trimmed = Line.TrimStart();
				if (!Trimmed.StartsWith(TEXT("#include")))
				{
					continue;
				}
				for (const FString& Forbidden : ForbiddenIncludeFragments)
				{
					ASSERT_THAT(IsFalse(
						Trimmed.Contains(Forbidden),
						*FString::Printf(
							TEXT("Runtime backend worker/lowering file %s must not include %s"),
							*RelativePath,
							*Forbidden)));
				}
			}

			for (const FString& Forbidden : ForbiddenStorageSymbols)
			{
				ASSERT_THAT(IsFalse(
					Contents.Contains(Forbidden),
					*FString::Printf(
						TEXT("Runtime backend worker/lowering file %s must not depend on storage surface %s"),
						*RelativePath,
						*Forbidden)));
			}
		}
	}

	TEST_METHOD(RuntimePublicationFilesHaveNoAotOrPersistentCacheSink)
	{
		const FString RuntimeSourceRoot = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(
				FPaths::ProjectPluginsDir(),
				TEXT("Angelscript/Source/AngelscriptRuntime")));
		const TArray<FString> RuntimePublicationFiles = {
			TEXT("Public/JIT/AngelscriptRuntimeJITBackend.h"),
			TEXT("RuntimeJIT/AngelscriptJITCoordinator.h"),
			TEXT("RuntimeJIT/AngelscriptJITCoordinator.cpp"),
			TEXT("RuntimeJIT/AngelscriptRuntimeJITBackendRegistry.h"),
			TEXT("RuntimeJIT/AngelscriptRuntimeJITBackendRegistry.cpp"),
			TEXT("RuntimeJIT/AngelscriptRuntimeJITBindingContext.h"),
			TEXT("RuntimeJIT/AngelscriptRuntimeJITBindingContext.cpp"),
			TEXT("RuntimeJIT/AngelscriptRuntimeJITState.h"),
			TEXT("RuntimeJIT/AngelscriptRuntimeJITState.cpp"),
		};
		const TArray<FString> ForbiddenSinkSymbols = {
			TEXT("RegisterProvider("),
			TEXT("GenerateStaticJITProviderArtifacts"),
			TEXT("FAngelscriptStaticJITGenerator"),
			TEXT("FAngelscriptJITGeneration"),
			TEXT("FAngelscriptCacheService"),
			TEXT("CacheV2"),
			TEXT("PrecompiledData"),
			TEXT("TypedHIR"),
			TEXT("TypedAST"),
		};

		for (const FString& RelativePath : RuntimePublicationFiles)
		{
			const FString AbsolutePath =
				FPaths::Combine(RuntimeSourceRoot, RelativePath);
			FString Contents;
			ASSERT_THAT(IsTrue(
				FFileHelper::LoadFileToString(Contents, *AbsolutePath),
				*FString::Printf(
					TEXT("Runtime JIT publication source must be readable: %s"),
					*AbsolutePath)));
			for (const FString& Forbidden : ForbiddenSinkSymbols)
			{
				ASSERT_THAT(IsFalse(
					Contents.Contains(Forbidden),
					*FString::Printf(
						TEXT("Runtime JIT publication file %s must not reference persistent/AOT sink %s"),
						*RelativePath,
						*Forbidden)));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
