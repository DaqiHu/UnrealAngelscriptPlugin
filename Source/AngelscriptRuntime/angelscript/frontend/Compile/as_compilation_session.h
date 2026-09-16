/* Maintained Unreal AngelScript fork — explicit declaration compilation phases. */
#pragma once

#include "frontend/Parser/as_parser.h"
#include "frontend/Sema/as_list_initializer.h"
#include "frontend/Sema/as_external_semantics.h"
#include <memory>

BEGIN_AS_NAMESPACE

class asCDefinitions;

enum class asECompilationPhase : uint8 { Collecting, DeclarationsCollected, DeclarationsResolved, BodiesAnalyzed, ASTVerified, Failed };

	struct asSBodyAnalysisStatistics
	{
		int32 RequestedWorkers = 0;
		int32 StartedWorkers = 0;
		int32 AnalyzedFragments = 0;
	};

	class ANGELSCRIPTRUNTIME_API asCCompilationSession
	{
	public:
		asCCompilationSession(asCSourceManager Sources, asCDiagnosticsEngine& Diagnostics, FString StableScope);
		asCCompilationSession(asCSourceManager Sources, asCDiagnosticsEngine& Diagnostics,
			FString StableScope, TSharedRef<asCTypeContext, ESPMode::ThreadSafe> TypeContext);
		bool CollectSource(asCPreprocessor& Preprocessor, const asSDirectiveProcessResult& Input, FString LogicalSourceKey);
		bool AddExternalDefinitions(asCDefinitions* Set);
		asCQualType ResolveTypeSyntax(const asSTypeSyntax& Syntax, const asCDecl& Scope);
		bool ResolveListInitializer(asCQualType Type, asSListInitialization& Out);
		asCRecordDecl* FindRecordDeclaration(const asSStableKey& Identity) const;
		asCNamedDecl* FindNominalDeclaration(const asSStableKey& Identity) const;
		bool CloseDeclarationBarrier();
		bool ResolveDeclarations(int32 WorkerCount);
		bool AnalyzeBodies(int32 WorkerCount);
		bool VerifyAST();
		const FString& GetVerificationError() const { return VerificationError; }
		const asSBodyAnalysisStatistics& GetBodyAnalysisStatistics() const { return BodyStatistics; }
		bool CollectSourceAfterBarrierForTesting() const { return false; }
		asECompilationPhase GetPhase() const { return Phase; }
		TConstArrayView<asCDeclarationFragment> GetFragments() const { return MakeArrayView(Fragments); }
		asCTranslationUnitDecl* GetTranslationUnit() const { return TranslationUnit; }
		TConstArrayView<asCDecl*> Lookup(const FString& Name) const;
		TConstArrayView<asCDecl*> LookupInScope(const FString& Name, const asCDecl& Declaration) const;
		TConstArrayView<asSExternalCallable> FindExternalCallables(const FString& Name) const;
		TConstArrayView<asCBodyFragment> GetBodyFragments() const { return MakeArrayView(BodyFragments); }
		FString SerializeStableProjection() const;
		FString SerializeStableBodyProjection() const;
		FString SerializeStableBodyDiagnostics() const;
		FString SerializeStableDiagnostics() const;
		bool IsPublishable() const { return (Phase == asECompilationPhase::DeclarationsResolved || Phase == asECompilationPhase::BodiesAnalyzed || Phase == asECompilationPhase::ASTVerified) && !bHasErrors; }
		bool AreBodiesAnalyzed() const { return (Phase == asECompilationPhase::BodiesAnalyzed || Phase == asECompilationPhase::ASTVerified) && !bHasErrors; }
		bool IsASTVerified() const { return Phase == asECompilationPhase::ASTVerified && !bHasErrors; }
		bool HasRecoveryNodes() const { return bHasRecoveryNodes; }
		asCASTContext& GetASTContext() { return Context; }
		const asCASTContext& GetASTContext() const { return Context; }
		asSStableKey GetModuleIdentity(const FString& LogicalSourceKey) const { const auto* Key = ModuleIdentities.Find(LogicalSourceKey); return Key ? *Key : asSStableKey{}; }
		asCType* GetDeclaredType(const asSStableKey& Identity) const { auto* Type = DeclaredTypes.Find(Identity); return Type ? *Type : nullptr; }
		asCSema& GetSema() { return Sema; }
		asCDiagnosticsEngine& GetDiagnosticsEngine() const { return Diagnostics; }
		const asCDefinitions* GetFrozenDefinitions() const { return FrozenDefinitions; }
		void BindFrozenDefinitions(const asCDefinitions* Set);

	private:
		bool ResolveConstantDeclarations();
		bool ResolveInferredDeclarations();
		bool ResolveAccessPolicies();
		bool ValidateRecordDeclarations();
		asSDirectiveProcessResult GetRetainedBodyInput(const asCSourceRange& Range) const;
		TConstArrayView<asCDecl*> LookupInBodyScope(const FString& Name, const asCDecl& Declaration) const;
		asCQualType ResolveStructuredType(const asSTypeSyntax& Syntax,
			const TFunction<asCType*(const FString&, bool)>& LookupType, int32 Depth = 0,
			FString* OutReason = nullptr);
		void SubmitSemanticGroup(const asCSourceRange& Range, FStringView CatalogReason, FStringView FragmentKey,
			TConstArrayView<asCSourceRange> RelatedRanges = {});

		asECompilationPhase Phase = asECompilationPhase::Collecting;
		asCASTContext Context;
		asCDiagnosticsEngine& Diagnostics;
		asCSema Sema;
		FString StableScope;
		TArray<asSDirectiveProcessResult> SourceInputs;
		TArray<asCDeclarationFragment> Fragments;
		TArray<asCBodyFragment> BodyFragments;
		asCTranslationUnitDecl* TranslationUnit = nullptr;
		TMap<FString, TArray<asCDecl*>> CanonicalLookup;
		TMap<FString, TArray<asCDecl*>> QualifiedLookup;
		TMap<FString, asSStableKey> ModuleIdentities;
		TMap<asSStableKey, asCType*> DeclaredTypes;
		TMap<asSStableKey, asCRecordDecl*> RecordDeclarations;
		TMap<asSStableKey, asCNamedDecl*> NominalDeclarations;
		TMap<FString, asCType*> ExternalTypes;
		TArray<asCDefinitions*> ExternalDefinitions;
		TMap<FString, TArray<asSExternalCallable>> ExternalCallables;
		TArray<FString> StableProjection;
		TArray<FString> StableDiagnostics;
		TArray<FString> StableBodyProjection;
		TArray<FString> StableBodyDiagnostics;
		bool bHasErrors = false;
		bool bHasRecoveryNodes = false;
		FString VerificationError;
		asSBodyAnalysisStatistics BodyStatistics;
		const asCDefinitions* FrozenDefinitions = nullptr;
	};

END_AS_NAMESPACE
