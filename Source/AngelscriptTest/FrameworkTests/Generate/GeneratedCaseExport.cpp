#include "FrameworkTests/Generate/GeneratedCaseExport.h"

#include "GenericPlatform/GenericPlatformOutputDevices.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace AngelscriptTest
{
namespace
{
bool IsValidGeneratorClass(const FStringView GeneratorClass)
{
	if (GeneratorClass.IsEmpty())
	{
		return false;
	}
	for (const TCHAR Character : GeneratorClass)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
		{
			return false;
		}
	}
	return true;
}

FString ResolvedGeneratedCasesDirectory()
{
	const FString LogFile(FGenericPlatformOutputDevices::GetAbsoluteLogFilename());
	if (LogFile.IsEmpty())
	{
		return FString();
	}
	const FString LogParent = FPaths::GetPath(LogFile);
	if (LogParent.IsEmpty())
	{
		return FString();
	}
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(LogParent, TEXT("GeneratedCases")));
}
}

FString FGeneratedCaseExport::ResolveOutputFile(const FStringView GeneratorClass)
{
	if (!IsValidGeneratorClass(GeneratorClass))
	{
		return FString();
	}
	const FString Directory = ResolvedGeneratedCasesDirectory();
	if (Directory.IsEmpty())
	{
		return FString();
	}
	return FPaths::Combine(Directory, FString(GeneratorClass) + TEXT(".as"));
}

bool FGeneratedCaseExport::WriteDump(const FStringView OutputFile, const FStringView Source, FString& OutError)
{
	OutError.Reset();
	const FString Target(OutputFile);
	if (Target.IsEmpty() || Target.Contains(TEXT("..")))
	{
		OutError = TEXT("invalid dump path");
		return false;
	}

	const FString ExpectedDirectory = ResolvedGeneratedCasesDirectory();
	if (ExpectedDirectory.IsEmpty())
	{
		OutError = TEXT("generated-cases directory is unavailable");
		return false;
	}

	const FString FullTarget = FPaths::ConvertRelativePathToFull(Target);
	const FString NormalizedTarget = FPaths::ConvertRelativePathToFull(FullTarget);
	const FString NormalizedDirectory = FPaths::ConvertRelativePathToFull(ExpectedDirectory);
	if (!NormalizedTarget.StartsWith(NormalizedDirectory, ESearchCase::IgnoreCase)
		|| NormalizedTarget.Equals(NormalizedDirectory, ESearchCase::IgnoreCase))
	{
		OutError = FString::Printf(TEXT("dump path is outside GeneratedCases: %s"), *NormalizedTarget);
		return false;
	}

	const FString Parent = FPaths::GetPath(NormalizedTarget);
	if (IFileManager::Get().FileExists(*Parent) || IFileManager::Get().FileExists(*NormalizedDirectory))
	{
		OutError = FString::Printf(TEXT("dump parent is a regular file: %s"), *Parent);
		return false;
	}
	if (!IFileManager::Get().DirectoryExists(*Parent)
		&& !IFileManager::Get().MakeDirectory(*Parent, true))
	{
		OutError = FString::Printf(TEXT("could not create dump directory: %s"), *Parent);
		return false;
	}
	if (IFileManager::Get().DirectoryExists(*NormalizedTarget))
	{
		OutError = FString::Printf(TEXT("dump path is a directory: %s"), *NormalizedTarget);
		return false;
	}
	if (!FFileHelper::SaveStringToFile(
		FString(Source),
		*NormalizedTarget,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("failed to write dump: %s"), *NormalizedTarget);
		return false;
	}
	return true;
}
}
