#pragma once

#include "CoreMinimal.h"

#include "Core/Artifacts/AngelscriptArtifactIdentity.h"
#include "StaticJIT/StaticJITConfig.h"

#if AS_CAN_GENERATE_JIT

class asCDataType;
class asCScriptFunction;

enum class EAngelscriptStaticJITEntryReceiverKind : uint8
{
	None = 0,
	NativeObjectSlot,
	DeclaredParameterAlias,
};

enum class EAngelscriptStaticJITEntryParameterRole : uint8
{
	Declared = 0,
	ReceiverAlias,
	Hidden,
};

enum class EAngelscriptStaticJITEntryParameterMarshalling : uint8
{
	ScalarValue = 0,
	BorrowedValueAddress,
	OpaqueCompatibilityValue,
};

enum class EAngelscriptStaticJITReflectedParmsLayout : uint8
{
	ScriptSignature = 0,
	AppendGeneratedWorldContext,
};

enum class EAngelscriptStaticJITEntryReturnPlacement : uint8
{
	None = 0,
	Direct,
	HiddenStack,
};

/** Pointer-free mapping for one declared AS formal parameter. */
struct FStaticJITEntryParameterPlan
{
	FString CanonicalName;
	FString BodyCppType;
	FString EntryCppType;
	FString ReflectedStorageCppType;
	int32 FormalParameterIndex = INDEX_NONE;
	int32 VMFrameOffsetDWords = 0;
	int32 ReflectedParameterIndex = INDEX_NONE;
	EAngelscriptStaticJITEntryParameterRole Role =
		EAngelscriptStaticJITEntryParameterRole::Declared;
	EAngelscriptStaticJITEntryParameterMarshalling Marshalling =
		EAngelscriptStaticJITEntryParameterMarshalling::ScalarValue;
};

/** Pointer-free return mapping shared by Static backends. */
struct FStaticJITEntryReturnPlan
{
	FString BodyCppType;
	FString EntryCppType;
	FString ReflectedStorageCppType;
	int32 VMResultOffsetDWords = 0;
	EAngelscriptStaticJITEntryReturnPlacement Placement =
		EAngelscriptStaticJITEntryReturnPlacement::None;
};

/**
 * Runtime-owned, pointer-free Entry ABI plan.
 *
 * Body analysis remains backend-specific. This value freezes only the common
 * native/VM/reflected adapter facts while the generation Engine is alive.
 */
struct FStaticJITEntryPlan
{
	FAngelscriptHash256 EntryPlanHash;
	FString CanonicalDeclaration;
	FString SourceSection;
	uint32 SourceLine = 0;
	bool bHasVMEntry = false;
	bool bHasRawEntry = false;
	bool bHasParmsEntry = false;
	bool bHasNativeObjectSlot = false;
	bool bTypedAdapterRepresentable = false;
	FString TypedAdapterFailure;
	EAngelscriptStaticJITEntryReceiverKind ReceiverKind =
		EAngelscriptStaticJITEntryReceiverKind::None;
	int32 ReceiverParameterIndex = INDEX_NONE;
	EAngelscriptStaticJITReflectedParmsLayout ReflectedParmsLayout =
		EAngelscriptStaticJITReflectedParmsLayout::ScriptSignature;
	TArray<FStaticJITEntryParameterPlan> Parameters;
	FStaticJITEntryReturnPlan Return;
	TArray<FString> RequiredIncludes;

	ANGELSCRIPTRUNTIME_API bool IsStructurallyComplete() const;
};

/** Frozen type spellings used by both the plan and the pure Typed emitter. */
ANGELSCRIPTRUNTIME_API bool TrySpellAngelscriptStaticJITBodyCppType(
	const asCDataType& Type,
	FString& OutType);

ANGELSCRIPTRUNTIME_API bool TrySpellAngelscriptStaticJITEntryCppType(
	const asCDataType& Type,
	FString& OutType);

/**
 * Freeze one exact Engine-local script function into a pointer-free plan.
 * Complex compatibility shapes may return a complete plan with
 * bTypedAdapterRepresentable=false; BytecodeJIT remains authoritative for
 * those functions.
 */
ANGELSCRIPTRUNTIME_API bool BuildAngelscriptStaticJITEntryPlan(
	const asCScriptFunction& ScriptFunction,
	bool bHasVMEntry,
	bool bHasRawEntry,
	bool bHasParmsEntry,
	FStaticJITEntryPlan& OutPlan,
	FString* OutError = nullptr);

/** Backend-neutral applicability of the reflected parameter-memory wrapper. */
ANGELSCRIPTRUNTIME_API bool CanGenerateAngelscriptStaticJITParmsEntry(
	const asCScriptFunction* ScriptFunction);

/** Recompute identity after a descriptor-owned reflected-layout overlay. */
ANGELSCRIPTRUNTIME_API void FinalizeAngelscriptStaticJITEntryPlan(
	FStaticJITEntryPlan& Plan);

#endif // AS_CAN_GENERATE_JIT
