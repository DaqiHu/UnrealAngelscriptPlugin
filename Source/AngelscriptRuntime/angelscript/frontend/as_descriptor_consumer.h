/* Maintained Unreal AngelScript fork — typed Sema to concrete UE descriptor projection. */
#pragma once

#include "frontend/as_preprocess_result.h"

BEGIN_AS_NAMESPACE

struct asSCollectedParameterDescription
	{
		FString Name;
		asSTypeSyntax Type;
		asCSourceRange Range;
	};

	// An observation of authored declarations, never a resolved/materializable descriptor.
	// Type syntax and source anchors are copied; this result owns no AST or Engine pointers.
	struct asSCollectedDeclarationDescription
	{
		asEDeclKind Kind = asEDeclKind::Recovery;
		FString QualifiedName;
		FAngelscriptResolvedSourceAnchor Source;
		asSTypeSyntax ReturnType;
		TArray<asSCollectedParameterDescription> Parameters;
	};

	class ANGELSCRIPTRUNTIME_API FAngelscriptDescriptorConsumer
	{
	public:
		bool Collect(
			const asCCompilationSession& Session,
			const asCSourceManager& SourceManager,
			TArray<asSCollectedDeclarationDescription>& Out) const;
		bool Project(
			const asCCompilationSession& Session,
			const asCSourceManager& SourceManager,
			FAngelscriptPreprocessResult& InOutResult,
			bool bIncludeUnattributedRecords = false) const;
	};

END_AS_NAMESPACE
