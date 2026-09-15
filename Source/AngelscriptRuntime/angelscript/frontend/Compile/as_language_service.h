/* Maintained Unreal AngelScript fork — in-process compile-time diagnostic facade. */
#pragma once

#include "frontend/Basic/as_diagnostics.h"
#include "frontend/Basic/as_source_edit.h"

#include <memory>

BEGIN_AS_NAMESPACE

enum asELanguageServiceFeature : asDWORD
{
	asLANGUAGE_SERVICE_DIAGNOSTICS = 1u << 0,
	asLANGUAGE_SERVICE_FIXES = 1u << 1
};

enum class asELanguageServiceStatus : uint8
{
	Succeeded,
	Unavailable,
	NotAttached,
	OwnerMismatch,
	InvalidArgument
};

class ANGELSCRIPTRUNTIME_API asCLanguageService
{
public:
	static std::shared_ptr<asCLanguageService> Create(
		asDWORD Features = asLANGUAGE_SERVICE_DIAGNOSTICS | asLANGUAGE_SERVICE_FIXES);

	asELanguageServiceStatus AttachCompilation(
		TSharedRef<const asCDiagnosticResult, ESPMode::ThreadSafe> Result);
	TConstArrayView<asSDiagnosticGroup> GetGroups() const;
	asELanguageServiceStatus Format(const asSDiagnosticGroup& Group, FString& OutText) const;
	asELanguageServiceStatus ApplyFix(
		const asSDiagnosticFixAlternative& Alternative,
		TSharedPtr<const asCSourceSnapshot, ESPMode::ThreadSafe>& OutSnapshot);
	asELanguageServiceStatus QueryCompletion() const;
	asELanguageServiceStatus QuerySignatureHelp() const;
	asELanguageServiceStatus QueryHover() const;
	asELanguageServiceStatus QueryDefinition() const;

private:
	explicit asCLanguageService(asDWORD InFeatures);

	bool HasFeature(asDWORD Feature) const { return (Features & Feature) != 0; }
	bool OwnsGroup(const asSDiagnosticGroup& Group) const;
	bool OwnsFix(const asSDiagnosticFixAlternative& Alternative) const;

	asDWORD Features = 0;
	TSharedPtr<const asCDiagnosticResult, ESPMode::ThreadSafe> Attached;
};

END_AS_NAMESPACE
