#include "Bind_FActorSpawnParameters.h"

#include "AngelscriptBinds.h"

/**
 * Actor-spawn parameter binding surface.
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                   |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | struct FActorSpawnParameters;                                                                        | Declares the common Unreal options used when spawning an Actor.                                             |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | FActorSpawnParameters Params();                                                                      | Constructs default spawn parameters.                                                                        |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | FActorSpawnParameters Params(const FActorSpawnParameters& Other);                                   | Copy-constructs spawn parameters.                                                                          |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | Params = Other;                                                                                      | Assigns spawn parameters.                                                                                   |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | FName Params.Name;                                                                                   | Requests an Actor name; NameMode controls name-collision behavior.                                          |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | AActor Params.Template;                                                                              | Copies initial Actor state from this template instead of the class default object.                          |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | AActor Params.Owner;                                                                                 | Sets the spawned Actor's ownership relationship.                                                            |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | APawn Params.Instigator;                                                                              | Sets the Pawn credited as the Actor's instigator.                                                           |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | ULevel Params.OverrideLevel;                                                                         | Overrides the destination level; nullptr allows the spawn helper's normal level resolution.                 |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | ESpawnActorCollisionHandlingMethod Params.SpawnCollisionHandlingOverride;                           | Overrides the class collision policy at the requested spawn transform.                                     |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | ESpawnActorNameMode Params.NameMode;                                                                 | Controls how a non-empty requested Name is handled when it already exists.                                  |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | bool Params.GetbNoFail() const; / void Params.SetbNoFail(bool Value);                               | Gets or sets Unreal's no-fail spawn flag.                                                                   |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | bool Params.GetbDeferConstruction() const; / void Params.SetbDeferConstruction(bool Value);         | Defers construction; call Actor::FinishSpawningActor after configuring the returned Actor.                  |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | bool Params.GetbAllowDuringConstructionScript() const;                                           | Returns whether spawning is allowed while a construction script executes.                     |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | void Params.SetbAllowDuringConstructionScript(bool Value);                                        | Controls whether spawning is allowed while a construction script executes.                     |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 * | enum ESpawnActorNameMode;                                                                            | Declares requested Actor-name conflict handling modes.                                                      |
 * +------------------------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------------------------------+
 */
AS_FORCE_LINK const FAngelscriptBind Bind_FActorSpawnParameters_TypeDeclarations(
	TEXT("FActorSpawnParameters.TypeDeclarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		if (Binds.IsHostTarget()) return;
		auto NameMode = Binds.EnumForTarget("ESpawnActorNameMode");
		NameMode["Required_Fatal"] = FActorSpawnParameters::ESpawnActorNameMode::Required_Fatal;
		NameMode["Required_ErrorAndReturnNull"] = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
		NameMode["Required_ReturnNull"] = FActorSpawnParameters::ESpawnActorNameMode::Required_ReturnNull;
		NameMode["Requested"] = FActorSpawnParameters::ESpawnActorNameMode::Requested;

		FBindFlags SpawnParametersFlags;
		Binds.ValueClassForTarget<FActorSpawnParameters>("FActorSpawnParameters", SpawnParametersFlags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FActorSpawnParameters_TypeInfrastructure(
	TEXT("FActorSpawnParameters.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		if (Binds.IsHostTarget()) return;
		Binds.RegisterTypeForTarget(MakeShared<FActorSpawnParametersType>());
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FActorSpawnParameters(
	TEXT("FActorSpawnParameters.ExplicitBindings"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		if (Binds.IsHostTarget()) return;
		auto SpawnParameters = Binds.ExistingClassForTarget("FActorSpawnParameters");
		SpawnParameters.Constructor("void f()", &FAngelscriptActorSpawnParametersBinds::Construct)
			.NoDiscard()
			.NativeConstructor("FActorSpawnParameters", true);
		SpawnParameters.Constructor("void f(const FActorSpawnParameters& Other)", &FAngelscriptActorSpawnParametersBinds::CopyConstruct)
			.NoDiscard()
			.NativeConstructor("FActorSpawnParameters", true);
		SpawnParameters.Method(
			"FActorSpawnParameters& opAssign(const FActorSpawnParameters& Other)",
			METHODPR_TRIVIAL(
				FActorSpawnParameters&,
				FActorSpawnParameters,
				operator=,
				(const FActorSpawnParameters&)));
		SpawnParameters.Property("FName Name", &FActorSpawnParameters::Name);
		SpawnParameters.Method("AActor GetTemplate() const", &FAngelscriptActorSpawnParametersBinds::GetTemplate);
		SpawnParameters.Method("void SetTemplate(AActor Value)", &FAngelscriptActorSpawnParametersBinds::SetTemplate);
		SpawnParameters.Method("AActor GetOwner() const", &FAngelscriptActorSpawnParametersBinds::GetOwner);
		SpawnParameters.Method("void SetOwner(AActor Value)", &FAngelscriptActorSpawnParametersBinds::SetOwner);
		SpawnParameters.Method("APawn GetInstigator() const", &FAngelscriptActorSpawnParametersBinds::GetInstigator);
		SpawnParameters.Method("void SetInstigator(APawn Value)", &FAngelscriptActorSpawnParametersBinds::SetInstigator);
		SpawnParameters.Method("ULevel GetOverrideLevel() const", &FAngelscriptActorSpawnParametersBinds::GetOverrideLevel);
		SpawnParameters.Method("void SetOverrideLevel(ULevel Value)", &FAngelscriptActorSpawnParametersBinds::SetOverrideLevel);
		SpawnParameters.Method("ESpawnActorCollisionHandlingMethod GetSpawnCollisionHandlingOverride() const", &FAngelscriptActorSpawnParametersBinds::GetSpawnCollisionHandlingOverride);
		SpawnParameters.Method("void SetSpawnCollisionHandlingOverride(ESpawnActorCollisionHandlingMethod Value)", &FAngelscriptActorSpawnParametersBinds::SetSpawnCollisionHandlingOverride);
		SpawnParameters.Method("ESpawnActorNameMode GetNameMode() const", &FAngelscriptActorSpawnParametersBinds::GetNameMode);
		SpawnParameters.Method("void SetNameMode(ESpawnActorNameMode Value)", &FAngelscriptActorSpawnParametersBinds::SetNameMode);
		SpawnParameters.Method("bool GetbNoFail() const", &FAngelscriptActorSpawnParametersBinds::GetNoFail);
		SpawnParameters.Method("void SetbNoFail(bool Value)", &FAngelscriptActorSpawnParametersBinds::SetNoFail);
		SpawnParameters.Method("bool GetbDeferConstruction() const", &FAngelscriptActorSpawnParametersBinds::GetDeferConstruction);
		SpawnParameters.Method("void SetbDeferConstruction(bool Value)", &FAngelscriptActorSpawnParametersBinds::SetDeferConstruction);
		SpawnParameters.Method("bool GetbAllowDuringConstructionScript() const", &FAngelscriptActorSpawnParametersBinds::GetAllowDuringConstructionScript);
		SpawnParameters.Method("void SetbAllowDuringConstructionScript(bool Value)", &FAngelscriptActorSpawnParametersBinds::SetAllowDuringConstructionScript);
	});

FString FActorSpawnParametersType::GetAngelscriptTypeName() const
{
	return TEXT("FActorSpawnParameters");
}

bool FActorSpawnParametersType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}

void FAngelscriptActorSpawnParametersBinds::Construct(FActorSpawnParameters* Address)
{
	new (Address) FActorSpawnParameters();
}

void FAngelscriptActorSpawnParametersBinds::CopyConstruct(FActorSpawnParameters* Address, const FActorSpawnParameters& Other)
{
	new (Address) FActorSpawnParameters(Other);
}

AActor* FAngelscriptActorSpawnParametersBinds::GetTemplate(const FActorSpawnParameters* P) { return P->Template; }
void FAngelscriptActorSpawnParametersBinds::SetTemplate(FActorSpawnParameters* P, AActor* V) { P->Template = V; }
AActor* FAngelscriptActorSpawnParametersBinds::GetOwner(const FActorSpawnParameters* P) { return P->Owner; }
void FAngelscriptActorSpawnParametersBinds::SetOwner(FActorSpawnParameters* P, AActor* V) { P->Owner = V; }
APawn* FAngelscriptActorSpawnParametersBinds::GetInstigator(const FActorSpawnParameters* P) { return P->Instigator; }
void FAngelscriptActorSpawnParametersBinds::SetInstigator(FActorSpawnParameters* P, APawn* V) { P->Instigator = V; }
ULevel* FAngelscriptActorSpawnParametersBinds::GetOverrideLevel(const FActorSpawnParameters* P) { return P->OverrideLevel; }
void FAngelscriptActorSpawnParametersBinds::SetOverrideLevel(FActorSpawnParameters* P, ULevel* V) { P->OverrideLevel = V; }
ESpawnActorCollisionHandlingMethod FAngelscriptActorSpawnParametersBinds::GetSpawnCollisionHandlingOverride(const FActorSpawnParameters* P) { return P->SpawnCollisionHandlingOverride; }
void FAngelscriptActorSpawnParametersBinds::SetSpawnCollisionHandlingOverride(FActorSpawnParameters* P, ESpawnActorCollisionHandlingMethod V) { P->SpawnCollisionHandlingOverride = V; }
FActorSpawnParameters::ESpawnActorNameMode FAngelscriptActorSpawnParametersBinds::GetNameMode(const FActorSpawnParameters* P) { return P->NameMode; }
void FAngelscriptActorSpawnParametersBinds::SetNameMode(FActorSpawnParameters* P, FActorSpawnParameters::ESpawnActorNameMode V) { P->NameMode = V; }

bool FAngelscriptActorSpawnParametersBinds::GetNoFail(const FActorSpawnParameters* Parameters)
{
	return Parameters->bNoFail;
}

void FAngelscriptActorSpawnParametersBinds::SetNoFail(FActorSpawnParameters* Parameters, bool bNoFail)
{
	Parameters->bNoFail = bNoFail;
}

bool FAngelscriptActorSpawnParametersBinds::GetDeferConstruction(const FActorSpawnParameters* Parameters)
{
	return Parameters->bDeferConstruction;
}

void FAngelscriptActorSpawnParametersBinds::SetDeferConstruction(FActorSpawnParameters* Parameters, bool bDeferConstruction)
{
	Parameters->bDeferConstruction = bDeferConstruction;
}

bool FAngelscriptActorSpawnParametersBinds::GetAllowDuringConstructionScript(const FActorSpawnParameters* Parameters)
{
	return Parameters->bAllowDuringConstructionScript;
}

void FAngelscriptActorSpawnParametersBinds::SetAllowDuringConstructionScript(
	FActorSpawnParameters* Parameters,
	bool bAllowDuringConstructionScript)
{
	Parameters->bAllowDuringConstructionScript = bAllowDuringConstructionScript;
}
