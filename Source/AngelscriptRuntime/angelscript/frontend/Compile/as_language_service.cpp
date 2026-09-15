#include "frontend/Compile/as_language_service.h"

#include "frontend/Basic/as_diagnostic_renderer.h"

BEGIN_AS_NAMESPACE

asCLanguageService::asCLanguageService(asDWORD InFeatures)
	: Features(InFeatures)
{
}

std::shared_ptr<asCLanguageService> asCLanguageService::Create(asDWORD Features)
{
	return std::shared_ptr<asCLanguageService>(new asCLanguageService(Features));
}

asELanguageServiceStatus asCLanguageService::AttachCompilation(
	TSharedRef<const asCDiagnosticResult, ESPMode::ThreadSafe> Result)
{
	if (!HasFeature(asLANGUAGE_SERVICE_DIAGNOSTICS))
	{
		return asELanguageServiceStatus::Unavailable;
	}
	Attached = Result;
	return asELanguageServiceStatus::Succeeded;
}

TConstArrayView<asSDiagnosticGroup> asCLanguageService::GetGroups() const
{
	if (!Attached.IsValid())
	{
		return {};
	}
	return Attached->GetGroups();
}

bool asCLanguageService::OwnsGroup(const asSDiagnosticGroup& Group) const
{
	if (!Attached.IsValid())
	{
		return false;
	}
	for (const asSDiagnosticGroup& Candidate : Attached->GetGroups())
	{
		if (&Candidate == &Group)
		{
			return true;
		}
	}
	return false;
}

bool asCLanguageService::OwnsFix(const asSDiagnosticFixAlternative& Alternative) const
{
	if (!Attached.IsValid())
	{
		return false;
	}
	for (const asSDiagnosticGroup& Group : Attached->GetGroups())
	{
		for (const asSDiagnosticFixAlternative& Candidate : Group.Fixes)
		{
			if (&Candidate == &Alternative)
			{
				return true;
			}
		}
	}
	return false;
}

asELanguageServiceStatus asCLanguageService::Format(const asSDiagnosticGroup& Group, FString& OutText) const
{
	if (!HasFeature(asLANGUAGE_SERVICE_DIAGNOSTICS))
	{
		return asELanguageServiceStatus::Unavailable;
	}
	if (!Attached.IsValid())
	{
		return asELanguageServiceStatus::NotAttached;
	}
	if (!OwnsGroup(Group))
	{
		return asELanguageServiceStatus::OwnerMismatch;
	}
	int32 Index = INDEX_NONE;
	const TConstArrayView<asSDiagnosticGroup> Groups = Attached->GetGroups();
	for (int32 Candidate = 0; Candidate < Groups.Num(); ++Candidate)
	{
		if (&Groups[Candidate] == &Group)
		{
			Index = Candidate;
			break;
		}
	}
	const asCDiagnosticRenderer Renderer;
	OutText = Renderer.Format(*Attached, Index);
	return asELanguageServiceStatus::Succeeded;
}

asELanguageServiceStatus asCLanguageService::ApplyFix(
	const asSDiagnosticFixAlternative& Alternative,
	TSharedPtr<const asCSourceSnapshot, ESPMode::ThreadSafe>& OutSnapshot)
{
	if (!HasFeature(asLANGUAGE_SERVICE_FIXES))
	{
		return asELanguageServiceStatus::Unavailable;
	}
	if (!Attached.IsValid())
	{
		return asELanguageServiceStatus::NotAttached;
	}
	if (!OwnsFix(Alternative))
	{
		return asELanguageServiceStatus::OwnerMismatch;
	}
	const asCSourceEditApplier Applier;
	if (!Applier.Apply(*Attached, Alternative, OutSnapshot) || !OutSnapshot.IsValid())
	{
		return asELanguageServiceStatus::InvalidArgument;
	}
	return asELanguageServiceStatus::Succeeded;
}

asELanguageServiceStatus asCLanguageService::QueryCompletion() const
{
	return asELanguageServiceStatus::Unavailable;
}

asELanguageServiceStatus asCLanguageService::QuerySignatureHelp() const
{
	return asELanguageServiceStatus::Unavailable;
}

asELanguageServiceStatus asCLanguageService::QueryHover() const
{
	return asELanguageServiceStatus::Unavailable;
}

asELanguageServiceStatus asCLanguageService::QueryDefinition() const
{
	return asELanguageServiceStatus::Unavailable;
}

END_AS_NAMESPACE
