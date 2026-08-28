#pragma once

#include "StaticJIT/Backends/AngelscriptStaticJITBackend.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITModel.h"

#if AS_CAN_GENERATE_JIT

/** Semantic reason one verified Canonical node requires stable artifact authority. */
enum class EAngelscriptTypedASTJITSemanticUseKind : uint8
{
	Invalid = 0,
	ScriptCallSignature,
	ScriptCallContent,
	NativeCall,
	ImportedBindingSlot,
	ReceiverType,
	ParameterType,
	ReturnType,
	ExpressionType,
	HiddenArgumentType,
	FoldedGlobalHardValue,
	MutableGlobalStorage,
	ProviderRoute,
};

ANGELSCRIPTRUNTIME_API const TCHAR* LexToString(
	EAngelscriptTypedASTJITSemanticUseKind Kind);

/** Pointer-free manifest row derived from one verified Canonical semantic use. */
struct FAngelscriptTypedASTJITSemanticUse
{
	EAngelscriptTypedASTJITSemanticUseKind Kind =
		EAngelscriptTypedASTJITSemanticUseKind::Invalid;
	EAngelscriptArtifactSemanticDependencyKind RequiredDependencyKind =
		EAngelscriptArtifactSemanticDependencyKind::Invalid;
	FAngelscriptArtifactReference Reference;
	asSSourceSpan SourceSpan;
	int32 EngineLocalCoordinate = INDEX_NONE;
	FString Target;
};

/**
 * Complete same-compilation dependency result for one TypedASTJIT body.
 *
 * ArtifactDependencies and RequiredReferences deliberately retain compiler
 * entries which have no direct Canonical node. The semantic-use rows prove coverage;
 * they are not a replacement dependency source.
 */
struct FAngelscriptTypedASTJITSemanticUseManifest
{
	TArray<FAngelscriptTypedASTJITSemanticUse> Uses;
	TArray<FAngelscriptArtifactSemanticDependency> ArtifactDependencies;
	TArray<FAngelscriptArtifactReference> RequiredReferences;
	TArray<FString> RequiredIncludes;
};

struct FAngelscriptTypedASTJITDependencyAnalysis
{
	bool bValid = false;
	EAngelscriptTypedASTJITFallbackReason FailureReason =
		EAngelscriptTypedASTJITFallbackReason::None;
	asSSourceSpan FailureSpan;
	FString FailureDetail;
	FAngelscriptTypedASTJITSemanticUseManifest Manifest;
};

/**
 * Builds and reconciles the TypedASTJIT semantic-use manifest without reading
 * Function.Bytecode or invoking any BytecodeJIT analyzer/reference scanner.
 */
#endif // AS_CAN_GENERATE_JIT
