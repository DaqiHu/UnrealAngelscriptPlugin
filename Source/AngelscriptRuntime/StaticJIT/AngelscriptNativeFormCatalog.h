#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/BytecodeJIT/StaticJITBinds.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallDescriptor.h"
#include "StaticJIT/StaticJITConfig.h"

#if AS_CAN_GENERATE_JIT

struct FAngelscriptEngine;
class asIScriptFunction;

enum class EAngelscriptNativeFormRecipeKind : uint8
{
	HeaderInline = 0,
	ModuleExported,
	Bridge,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptNativeFormCatalogKey
{
	FString StableDeclarationIdentity;
	EAngelscriptJITTargetProfile TargetProfile =
		EAngelscriptJITTargetProfile::Invalid;

	friend bool operator==(
		const FAngelscriptNativeFormCatalogKey& Left,
		const FAngelscriptNativeFormCatalogKey& Right)
	{
		return Left.StableDeclarationIdentity == Right.StableDeclarationIdentity
			&& Left.TargetProfile == Right.TargetProfile;
	}
};

inline uint32 GetTypeHash(const FAngelscriptNativeFormCatalogKey& Key)
{
	return HashCombine(
		GetTypeHash(Key.StableDeclarationIdentity),
		GetTypeHash(static_cast<uint8>(Key.TargetProfile)));
}

/**
 * Pointer-free native-form recipe. Matching-profile Generate looks this up by
 * stable declaration identity and target profile instead of engine-local
 * function pointers or bind lambdas.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptNativeFormRecipe
{
	EAngelscriptNativeFormRecipeKind Kind =
		EAngelscriptNativeFormRecipeKind::HeaderInline;
	FAngelscriptNativeFormDescription Description;
	FAngelscriptStaticJITNativeCallDescriptor Descriptor;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptNativeFormCatalog
{
public:
	static FAngelscriptNativeFormCatalog& Get();

	static EAngelscriptJITTargetProfile InferTargetProfile(
		const FAngelscriptEngine& Engine);

	static FAngelscriptNativeFormCatalogKey MakeKey(
		const asIScriptFunction& Function,
		EAngelscriptJITTargetProfile TargetProfile);

	static FAngelscriptNativeFormCatalogKey MakeKey(
		const FAngelscriptEngine& Engine,
		const asIScriptFunction& Function);

	static EAngelscriptNativeFormRecipeKind InferKind(
		const FAngelscriptNativeFormRecipe& Recipe);

	bool InsertOrValidate(
		const FAngelscriptNativeFormCatalogKey& Key,
		const FAngelscriptNativeFormRecipe& Recipe,
		FString* OutError = nullptr);

	void PublishFromRegistration(
		FAngelscriptEngine& Engine,
		asIScriptFunction* Function);

	const FAngelscriptNativeFormRecipe* Find(
		const FAngelscriptNativeFormCatalogKey& Key) const;

	int32 NumRecipes() const;

#if WITH_ANGELSCRIPT_UNITTESTS
	void ResetForTests();
#endif

private:
	mutable FCriticalSection Mutex;
	TMap<FAngelscriptNativeFormCatalogKey, FAngelscriptNativeFormRecipe> Recipes;
};

#endif
