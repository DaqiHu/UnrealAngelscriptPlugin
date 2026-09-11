#include "StaticJIT/AngelscriptNativeFormCatalog.h"

#if AS_CAN_GENERATE_JIT

#include "AngelscriptEngine.h"

#include "StartAngelscriptHeaders.h"
#include "as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptNativeFormCatalog_Private
{
	bool DescriptionsMatch(
		const FAngelscriptNativeFormDescription& Left,
		const FAngelscriptNativeFormDescription& Right)
	{
		return Left.Kind == Right.Kind
			&& Left.Name == Right.Name
			&& Left.CustomForm == Right.CustomForm
			&& Left.TargetType == Right.TargetType
			&& Left.Header == Right.Header
			&& Left.UnrealFunctionPath == Right.UnrealFunctionPath
			&& Left.UFunctionRouteFlags == Right.UFunctionRouteFlags
			&& Left.bTrivial == Right.bTrivial
			&& Left.bGuaranteed == Right.bGuaranteed
			&& Left.bNeedsCompare == Right.bNeedsCompare
			&& Left.bNeedsCopy == Right.bNeedsCopy;
	}

	bool RecipesMatch(
		const FAngelscriptNativeFormRecipe& Left,
		const FAngelscriptNativeFormRecipe& Right)
	{
		return Left.Kind == Right.Kind
			&& DescriptionsMatch(Left.Description, Right.Description)
			&& Left.Descriptor.DumpNormalized()
				== Right.Descriptor.DumpNormalized();
	}

	bool CanCompleteRecipe(
		const FAngelscriptNativeFormRecipe& Existing,
		const FAngelscriptNativeFormRecipe& Incoming)
	{
		if (!Existing.Descriptor.IsAbsent()
			|| Incoming.Descriptor.IsAbsent())
		{
			return false;
		}
		FAngelscriptNativeFormRecipe Completed = Existing;
		Completed.Descriptor = Incoming.Descriptor;
		Completed.Kind = FAngelscriptNativeFormCatalog::InferKind(Completed);
		if (Incoming.Description.Kind != EAngelscriptNativeFormKind::Unknown
			&& !DescriptionsMatch(Existing.Description, Incoming.Description)
			&& Existing.Description.Kind != EAngelscriptNativeFormKind::Unknown)
		{
			return false;
		}
		if (Incoming.Description.Kind != EAngelscriptNativeFormKind::Unknown)
		{
			Completed.Description = Incoming.Description;
			Completed.Kind = FAngelscriptNativeFormCatalog::InferKind(Completed);
		}
		return RecipesMatch(Completed, Incoming)
			|| (Completed.Kind == Incoming.Kind
				&& Completed.Descriptor.DumpNormalized()
					== Incoming.Descriptor.DumpNormalized());
	}
}

FAngelscriptNativeFormCatalog& FAngelscriptNativeFormCatalog::Get()
{
	static FAngelscriptNativeFormCatalog Catalog;
	return Catalog;
}

EAngelscriptJITTargetProfile FAngelscriptNativeFormCatalog::InferTargetProfile(
	const FAngelscriptEngine& Engine)
{
	const FAngelscriptEngineConfig& Config = Engine.GetRuntimeConfig();
	if (Config.bOverrideTargetPreprocessorFlags)
	{
		if (Config.bTargetEditor)
		{
			return EAngelscriptJITTargetProfile::EditorDevelopment;
		}
		if (Config.bTargetRelease)
		{
			return EAngelscriptJITTargetProfile::GameShipping;
		}
		return EAngelscriptJITTargetProfile::GameDevelopment;
	}
	if (Config.bIsEditor)
	{
		return EAngelscriptJITTargetProfile::EditorDevelopment;
	}
	if (Config.bDevelopmentMode)
	{
		return EAngelscriptJITTargetProfile::GameDevelopment;
	}
	return EAngelscriptJITTargetProfile::GameShipping;
}

FAngelscriptNativeFormCatalogKey FAngelscriptNativeFormCatalog::MakeKey(
	const asIScriptFunction& Function,
	const EAngelscriptJITTargetProfile TargetProfile)
{
	FAngelscriptNativeFormCatalogKey Key;
	Key.StableDeclarationIdentity = UTF8_TO_TCHAR(
		Function.GetDeclaration(true, true, true, true));
	Key.TargetProfile = TargetProfile;
	return Key;
}

FAngelscriptNativeFormCatalogKey FAngelscriptNativeFormCatalog::MakeKey(
	const FAngelscriptEngine& Engine,
	const asIScriptFunction& Function)
{
	return MakeKey(Function, InferTargetProfile(Engine));
}

EAngelscriptNativeFormRecipeKind FAngelscriptNativeFormCatalog::InferKind(
	const FAngelscriptNativeFormRecipe& Recipe)
{
	if (!Recipe.Descriptor.IsAbsent())
	{
		if (Recipe.Descriptor.Linkage
				== EAngelscriptStaticJITNativeCallLinkage::HeaderInline
			|| Recipe.Descriptor.Visibility
				== EAngelscriptStaticJITNativeCallVisibility::HeaderInline)
		{
			return EAngelscriptNativeFormRecipeKind::HeaderInline;
		}
		if (Recipe.Descriptor.Linkage
				== EAngelscriptStaticJITNativeCallLinkage::ProviderPrivate
			|| Recipe.Descriptor.ABIDomain
				== EAngelscriptStaticJITNativeCallABIDomain::VMScalarBridge
			|| EnumHasAnyFlags(
				Recipe.Descriptor.RouteFlags,
				EAngelscriptStaticJITNativeCallRouteFlags::ScalarBridge
					| EAngelscriptStaticJITNativeCallRouteFlags::RequiresProcessEvent))
		{
			return EAngelscriptNativeFormRecipeKind::Bridge;
		}
		return EAngelscriptNativeFormRecipeKind::ModuleExported;
	}
	if (Recipe.Description.Kind == EAngelscriptNativeFormKind::FunctionHeader
		|| !Recipe.Description.Header.IsEmpty())
	{
		return EAngelscriptNativeFormRecipeKind::HeaderInline;
	}
	if (Recipe.Description.Kind == EAngelscriptNativeFormKind::UFunction
		&& Recipe.Description.UFunctionRouteFlags
			!= EAngelscriptNativeUFunctionRouteFlags::None)
	{
		return EAngelscriptNativeFormRecipeKind::Bridge;
	}
	return EAngelscriptNativeFormRecipeKind::ModuleExported;
}

bool FAngelscriptNativeFormCatalog::InsertOrValidate(
	const FAngelscriptNativeFormCatalogKey& Key,
	const FAngelscriptNativeFormRecipe& Recipe,
	FString* OutError)
{
	if (Key.StableDeclarationIdentity.IsEmpty()
		|| Key.TargetProfile == EAngelscriptJITTargetProfile::Invalid
		|| Key.TargetProfile == EAngelscriptJITTargetProfile::All)
	{
		if (OutError != nullptr)
		{
			*OutError = TEXT(
				"NativeFormCatalogInvalidKey: declaration and concrete target profile are required.");
		}
		return false;
	}

	FScopeLock Lock(&Mutex);
	if (FAngelscriptNativeFormRecipe* Existing = Recipes.Find(Key))
	{
		if (AngelscriptNativeFormCatalog_Private::RecipesMatch(*Existing, Recipe))
		{
			return true;
		}
		if (AngelscriptNativeFormCatalog_Private::CanCompleteRecipe(
				*Existing, Recipe))
		{
			*Existing = Recipe;
			Existing->Kind = InferKind(*Existing);
			return true;
		}
		if (OutError != nullptr)
		{
			*OutError = FString::Printf(
				TEXT("NativeFormCatalogConflict: declaration '%s' profile '%s' already has a different reviewed recipe."),
				*Key.StableDeclarationIdentity,
				FAngelscriptJITTargetProfiles::ToString(Key.TargetProfile));
		}
		return false;
	}

	FAngelscriptNativeFormRecipe Stored = Recipe;
	Stored.Kind = InferKind(Stored);
	Recipes.Add(Key, MoveTemp(Stored));
	return true;
}

void FAngelscriptNativeFormCatalog::PublishFromRegistration(
	FAngelscriptEngine& Engine,
	asIScriptFunction* Function)
{
	if (Function == nullptr
		|| !Engine.bCollectStaticJITCompatibilityBinds)
	{
		return;
	}

	FAngelscriptNativeFormRecipe Recipe;
	if (const FScriptFunctionNativeForm* Form =
		FScriptFunctionNativeForm::GetNativeForm(Function))
	{
		Recipe.Description = Form->Describe();
	}
	if (const FAngelscriptStaticJITNativeCallDescriptor* Descriptor =
		FAngelscriptStaticJITNativeCallRegistry::Find(Function))
	{
		Recipe.Descriptor = *Descriptor;
	}
	if (Recipe.Description.Kind == EAngelscriptNativeFormKind::Unknown
		&& Recipe.Descriptor.IsAbsent())
	{
		return;
	}
	Recipe.Kind = InferKind(Recipe);
	InsertOrValidate(MakeKey(Engine, *Function), Recipe);
}

const FAngelscriptNativeFormRecipe* FAngelscriptNativeFormCatalog::Find(
	const FAngelscriptNativeFormCatalogKey& Key) const
{
	FScopeLock Lock(&Mutex);
	return Recipes.Find(Key);
}

int32 FAngelscriptNativeFormCatalog::NumRecipes() const
{
	FScopeLock Lock(&Mutex);
	return Recipes.Num();
}

#if WITH_ANGELSCRIPT_UNITTESTS
void FAngelscriptNativeFormCatalog::ResetForTests()
{
	FScopeLock Lock(&Mutex);
	Recipes.Reset();
}
#endif

#endif
