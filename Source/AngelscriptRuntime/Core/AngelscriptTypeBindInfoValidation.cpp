#include "AngelscriptTypeBindInfoValidation.h"
#include "AngelscriptTypeBindInfoApply.h"
#include "AngelscriptTypeBindInfoStore.h"

namespace
{
	bool HasSource(const FAngelscriptTypeBindSource& Source)
	{
		return !Source.OwnerModule.IsNone() && !Source.Provider.IsNone()
			&& !Source.File.IsEmpty() && Source.Line > 0;
	}

	FString SourceText(const FAngelscriptTypeBindSource& Source)
	{
		return FString::Printf(TEXT("provider '%s' at %s:%d"), *Source.Provider.ToString(), *Source.File, Source.Line);
	}

	FAngelscriptTypeBindValidationResult Failure(EAngelscriptBindingStage Stage, FString Message)
	{
		FAngelscriptTypeBindValidationResult Result;
		Result.Stage = Stage;
		Result.Message = MoveTemp(Message);
		return Result;
	}
}

FAngelscriptTypeBindValidationResult FAngelscriptTypeBindInfoValidation::ValidateSealed(
	TSharedRef<const FAngelscriptTypeBindInfoStore> Store)
{
	if (Store->HasFailed())
		return Failure(EAngelscriptBindingStage::Recording, Store->GetFailureDiagnostic());
	if (!Store->IsSealed())
		return Failure(EAngelscriptBindingStage::Recording, TEXT("Binding validation requires a sealed recording snapshot."));

	for (const auto& Provider : Store->GetProviders())
		if (!HasSource(Provider.Source))
			return Failure(EAngelscriptBindingStage::Recording, TEXT("Recorded provider has incomplete source provenance."));

	const auto Types = Store->GetTypes();
	for (const auto& Type : Types)
	{
		if (!HasSource(Type.Declaration.Source))
			return Failure(EAngelscriptBindingStage::Recording,
				FString::Printf(TEXT("Type '%s' has incomplete source provenance."), *Type.Declaration.Name));
		for (const auto& AdditionalSource : Type.Sources)
			if (!HasSource(AdditionalSource))
				return Failure(EAngelscriptBindingStage::Recording,
					FString::Printf(TEXT("Type '%s' has incomplete source provenance."), *Type.Declaration.Name));
		for (const auto& Member : Type.Members)
		{
			const auto& Source = HasSource(Member.Source) ? Member.Source : Type.Declaration.Source;
			if (!HasSource(Source))
				return Failure(EAngelscriptBindingStage::Recording,
					FString::Printf(TEXT("Member '%s' has incomplete source provenance."), *Member.Declaration));
			if (Member.bPureConstant && Member.GlobalStorageLifetime != EAngelscriptBindHostLifetime::CopiedRecord)
				return Failure(EAngelscriptBindingStage::Recording,
					FString::Printf(TEXT("Pure constant '%s' does not own its recorded storage (%s)."),
						*Member.Declaration, *SourceText(Source)));
		}
	}

	TUniquePtr<FAngelscriptTypeBindInfoInstallation> Prepared;
	FString Diagnostic;
	EAngelscriptBindingStage Stage = EAngelscriptBindingStage::None;
	if (!FAngelscriptTypeBindInfoApply::Prepare(Store, Prepared, Diagnostic, &Stage))
		return Failure(Stage, MoveTemp(Diagnostic));

	for (const auto& Type : Types)
	{
		for (const auto& Member : Type.Members)
		{
			const auto& Source = HasSource(Member.Source) ? Member.Source : Type.Declaration.Source;
			const bool bCallable = Member.Kind == EAngelscriptTypeBindMemberKind::Constructor
				|| Member.Kind == EAngelscriptTypeBindMemberKind::Destructor
				|| Member.Kind == EAngelscriptTypeBindMemberKind::Method
				|| Member.Kind == EAngelscriptTypeBindMemberKind::Behaviour
				|| Member.Kind == EAngelscriptTypeBindMemberKind::GlobalFunction;
			const bool bInterfaceMethod = Type.Declaration.Kind == EAngelscriptTypeBindKind::Interface
				&& Member.Kind == EAngelscriptTypeBindMemberKind::Method;
			if (bCallable && !bInterfaceMethod && Member.Native.Function.flag == 0 && Member.NativeRecipes.IsEmpty())
				return Failure(EAngelscriptBindingStage::Native,
					FString::Printf(TEXT("Native target is missing for '%s' (%s)."),
						*Member.Declaration, *SourceText(Source)));
			if (Member.Kind == EAngelscriptTypeBindMemberKind::GlobalProperty
				&& !Member.bPureConstant && Member.GlobalAddress == nullptr)
				return Failure(EAngelscriptBindingStage::Native,
					FString::Printf(TEXT("Native storage is missing for '%s' (%s)."),
						*Member.Declaration, *SourceText(Source)));
		}
	}

	FAngelscriptTypeBindValidationResult Result;
	Result.bValid = true;
	return Result;
}
