/*
   Maintained Unreal AngelScript fork — revision-bound in-memory source edits.
*/

#pragma once

#include "frontend/Basic/as_diagnostics.h"

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asCSourceSnapshot
{
public:
	explicit asCSourceSnapshot(asCSourceManager InManager);

	const asCSourceManager& GetManager() const { return Manager; }

private:
	asCSourceManager Manager;
};

class ANGELSCRIPTRUNTIME_API asCSourceEditApplier
{
public:
	bool Apply(
		const asCDiagnosticResult& Owner,
		const asSDiagnosticFixAlternative& Fix,
		TSharedPtr<const asCSourceSnapshot, ESPMode::ThreadSafe>& OutSnapshot) const;
};

END_AS_NAMESPACE
