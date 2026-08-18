#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITProviderMatcher.h"

/**
 * Runtime-owned immutable payload carried by asSJITFunctionBinding::UserData.
 * It retains copied Provider metadata and the Engine-local resolved reference
 * table; provider-owned view memory is never stored here.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptJITBindingContext final
{
public:
	enum class EEntryKind : uint8
	{
		VM = 0,
		Raw,
		Parms,
	};

	explicit FAngelscriptJITBindingContext(
		const FAngelscriptJITArtifactSelection& InSelection,
		const class FAngelscriptJITImmutableArtifactSetValidationResult*
			InImmutableValidation = nullptr);
	~FAngelscriptJITBindingContext();

	bool IsValid() const;
	asSJITFunctionBinding BuildBinding() const;

	const FAngelscriptJITResolvedReferenceTable* GetReferenceTable() const
	{
		return Selection.ReferenceTable.Get();
	}

	bool IsImmutableCookedDirectDispatch() const
	{
		return bImmutableCookedDirectDispatch;
	}

	const TSharedPtr<const FAngelscriptJITProviderCatalog,
		ESPMode::ThreadSafe>& GetProvider() const
	{
		return Selection.Provider;
	}

	int32 GetEntryIndex() const
	{
		return Selection.EntryIndex;
	}

	bool HasExecutionCapabilityProfile() const;
	EAngelscriptJITExecutionCapability GetAvailableExecutionCapabilities() const;
	EAngelscriptJITExecutionCapability
		GetRuntimeExecutionRequirementTriggers() const;
	bool AllowsExecutionCapabilities(
		EAngelscriptJITExecutionCapability RequiredCapabilities) const;

	void MarkExecution(EEntryKind Kind) const;
	uint64 GetExecutionCount(EEntryKind Kind) const;

	static const FAngelscriptJITBindingContext* FromBindingUserData(
		const void* UserData);

private:
	static constexpr uint64 LiveMagic = 0x41534A4954424E44ull; // ASJITBND
	uint64 Magic = LiveMagic;
	FAngelscriptJITArtifactSelection Selection;
	bool bImmutableCookedDirectDispatch = false;
	mutable TAtomic<uint64> VMExecutionCount{0};
	mutable TAtomic<uint64> RawExecutionCount{0};
	mutable TAtomic<uint64> ParmsExecutionCount{0};
};
