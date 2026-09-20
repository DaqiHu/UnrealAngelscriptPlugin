#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EDeclFamilyScopeOrderFamily : uint8
{
	Function,
	Method,
	Class,
	Struct,
	Field,
	Constructor,
	Destructor,
	Namespace,
	Enum,
	Typedef,
	Funcdef,
	Import,
	VirtualProperty,
	IndexedProperty,
	MixinGlobal,
};

enum class EDeclFamilyScopeOrderScope : uint8
{
	Global,
	Namespace,
	NestedNamespace,
	Member,
	MultipleSections,
	ImportedModule,
};

enum class EDeclFamilyScopeOrderOrdering : uint8
{
	BeforeUse,
	ForwardUse,
	SameSection,
	LaterSection,
	ReversedSections,
	Rebuild,
};

struct FDeclFamilyScopeOrderParams
{
	EDeclFamilyScopeOrderFamily Family = EDeclFamilyScopeOrderFamily::Function;
	EDeclFamilyScopeOrderScope Scope = EDeclFamilyScopeOrderScope::Global;
	EDeclFamilyScopeOrderOrdering Ordering = EDeclFamilyScopeOrderOrdering::BeforeUse;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FDeclFamilyScopeOrderGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildDeclarationSource(const FDeclFamilyScopeOrderParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
