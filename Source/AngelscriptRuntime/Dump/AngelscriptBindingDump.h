#pragma once

#include "Core/AngelscriptTypeBindInfoInspection.h"

struct ANGELSCRIPTRUNTIME_API FAngelscriptBindingDumpResult
{
	FString Directory;
	TArray<FString> Files;
	FString SemanticJson;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptBindingDump final
{
public:
	static bool Serialize(const FAngelscriptBindingInspection& Inspection,
		FString& OutJson, TMap<FString, FString>& OutTables, FString& Diagnostic);
	static bool ExportToDirectory(const FAngelscriptBindingInspection& Inspection,
		const FString& Directory, FAngelscriptBindingDumpResult& Out, FString& Diagnostic);
	static bool ExportStoreToDirectory(TSharedRef<const FAngelscriptTypeBindInfoStore> Store,
		EAngelscriptBindingInspectionScope Scope, const FString& Directory,
		FAngelscriptBindingDumpResult& Out, FString& Diagnostic);
};
