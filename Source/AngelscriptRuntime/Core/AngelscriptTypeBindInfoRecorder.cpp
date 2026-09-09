#include "AngelscriptTypeBindInfoRecorder.h"
#include "AngelscriptBindsInternal.h"
#include "AngelscriptTypeBindInfoCatalog.h"

bool FAngelscriptTypeBindInfoRecorder::Capture(
	const UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
	FAngelscriptBindRecordingPolicy Policy, TSharedPtr<const FAngelscriptTypeBindInfoStore>& OutStore,
	FString& OutDiagnostic)
{
	OutStore.Reset();
	OutDiagnostic.Reset();
	if (!IsInGameThread())
	{
		OutDiagnostic = TEXT("Binding providers must be captured serially on the GameThread.");
		return false;
	}
	if (!Collection.IsSealed())
	{
		OutDiagnostic = TEXT("A binding provider collection must be sealed before capture.");
		return false;
	}
	if (!Collection.GetCollectionFailureDiagnostic().IsEmpty())
	{
		OutDiagnostic = Collection.GetCollectionFailureDiagnostic();
		return false;
	}

	auto Store = MakeShared<FAngelscriptTypeBindInfoStore>(MoveTemp(Policy));
	// Declaration providers have a detached, declaration-only path. Running their
	// legacy Engine-side branches during capture would query mutable Engine state.
	if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(
		Collection, *Store, OutDiagnostic))
	{
		return false;
	}
	FAngelscriptBinds Binds(*Store);
	for (const FAngelscriptBindRecord& Record : Collection.GetRecords())
	{
		if (Record.Phase == EAngelscriptBindPhase::TypeDeclarations)
		{
			continue;
		}
		FAngelscriptRecordedProvider Provider;
		Provider.Source.OwnerModule = Record.OwnerModule;
		Provider.Source.Provider = Record.BindName;
		Provider.Source.Phase = Record.Phase;
		Provider.Source.File = ANSI_TO_TCHAR(Record.SourceFile);
		Provider.Source.Line = Record.SourceLine;
		Provider.bEligible = (!Store->GetPolicy().bRuntimeModuleOnly
				|| Record.OwnerModule == TEXT("AngelscriptRuntime"))
			&& (Record.Condition == nullptr || Record.Condition(Store->GetPolicy()));
		if (!Collection.GetCollectionFailureDiagnostic().IsEmpty())
		{
			OutDiagnostic = Collection.GetCollectionFailureDiagnostic();
			return false;
		}
		const bool bAlreadyRecorded = Store->GetProviders().ContainsByPredicate(
			[&Provider](const FAngelscriptRecordedProvider& Existing)
			{
				return Existing.Source.OwnerModule == Provider.Source.OwnerModule
					&& Existing.Source.Provider == Provider.Source.Provider
					&& Existing.Source.Phase == Provider.Source.Phase;
			});
		if (!bAlreadyRecorded && !Store->RecordProvider(Provider, OutDiagnostic)) return false;
		if (!Provider.bEligible) continue;

		Store->SetSource(Provider.Source);
		Store->SetNamespace(FString());
		Record.Callback(Binds);
		if (Store->HasFailed())
		{
			OutDiagnostic = FString::Printf(
				TEXT("Binding provider capture failed (owner='%s', provider='%s', phase=%u, source='%s:%d'): %s"),
				*Record.OwnerModule.ToString(), *Record.BindName.ToString(), static_cast<uint32>(Record.Phase),
				*Provider.Source.File, Provider.Source.Line, *Store->GetFailureDiagnostic());
			return false;
		}
		if (!Collection.GetCollectionFailureDiagnostic().IsEmpty())
		{
			OutDiagnostic = Collection.GetCollectionFailureDiagnostic();
			return false;
		}
	}
	Store->SetSource({});
	Store->SetNamespace(FString());
	if (!Store->Seal(OutDiagnostic)) return false;
	OutStore = Store;
	return true;
}
