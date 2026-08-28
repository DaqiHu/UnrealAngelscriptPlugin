#include "StaticJIT/AngelscriptJITProviderRegistry.h"

#include "Features/IModularFeatures.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformStackWalk.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
#if !PLATFORM_WINDOWS
	static void* RetainCodeImage(
		const FString& LoadedImageName)
	{
		// dlopen-based implementations acquire a matching reference for an
		// already loaded image, paired with FreeDllHandle() at lease release.
		return FPlatformProcess::GetDllHandle(*LoadedImageName);
	}
#endif
}

FAngelscriptJITProviderLifetime::FAngelscriptJITProviderLifetime(
	const FAngelscriptJITProviderId& InProviderId,
	const FAngelscriptJITProviderGeneration& InProviderGeneration,
	const FStringView InOwnerModuleName)
	: ProviderId(InProviderId)
	, ProviderGeneration(InProviderGeneration)
	, OwnerModuleName(InOwnerModuleName)
{
}

FAngelscriptJITProviderLifetime::~FAngelscriptJITProviderLifetime()
{
	for (void* PinnedModuleHandle : PinnedModuleHandles)
	{
		if (PinnedModuleHandle != nullptr)
		{
			FPlatformProcess::FreeDllHandle(PinnedModuleHandle);
		}
	}
	PinnedModuleHandles.Reset();
}

TSharedRef<const FAngelscriptJITProviderLifetime, ESPMode::ThreadSafe>
FAngelscriptJITProviderLifetime::CreateLogical(
	const FAngelscriptJITProviderId& ProviderId,
	const FAngelscriptJITProviderGeneration& ProviderGeneration,
	const FStringView OwnerModuleName)
{
	return MakeShareable(new FAngelscriptJITProviderLifetime(
		ProviderId, ProviderGeneration, OwnerModuleName));
}

TSharedRef<const FAngelscriptJITProviderLifetime, ESPMode::ThreadSafe>
FAngelscriptJITProviderLifetime::AcquireForLoadedProvider(
	const FAngelscriptJITProviderView& View)
{
	TSharedRef<FAngelscriptJITProviderLifetime, ESPMode::ThreadSafe> Lifetime =
		MakeShareable(new FAngelscriptJITProviderLifetime(
			View.ProviderId,
			View.ProviderGeneration,
			View.OwnerModuleName));

	const FName ModuleName(*Lifetime->OwnerModuleName);
	FModuleManager& ModuleManager = FModuleManager::Get();
	if (!ModuleManager.IsModuleLoaded(ModuleName))
	{
		return Lifetime;
	}

#if IS_MONOLITHIC
	Lifetime->CodeLifetime =
		EAngelscriptJITProviderCodeLifetime::ProcessLifetimeImage;
	Lifetime->bKeepsCodeMapped = true;
#else
	TSet<UPTRINT> RetainedImageBases;

#if PLATFORM_WINDOWS
	// Live++ patch images are ordinary loaded PE images, but they are not
	// guaranteed to appear in FPlatformStackWalk's cached module signatures.
	// The Windows loader is authoritative for an address-to-image lookup, so
	// resolve and retain the image directly from every published entry point.
	// First query without changing the reference count for deduplication, then
	// acquire exactly one owned reference per unique non-executable image.
	const HMODULE ExecutableModule = ::GetModuleHandleW(nullptr);
	auto RetainEntryPointImage = [&](const UPTRINT EntryPoint) -> bool
	{
		if (EntryPoint == 0)
		{
			return true;
		}

		HMODULE ModuleHandle = nullptr;
		if (::GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
					| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(EntryPoint),
				&ModuleHandle) == 0
			|| ModuleHandle == nullptr)
		{
			return false;
		}

		const UPTRINT ImageBase = reinterpret_cast<UPTRINT>(ModuleHandle);
		if (RetainedImageBases.Contains(ImageBase))
		{
			return true;
		}
		if (ModuleHandle == ExecutableModule)
		{
			RetainedImageBases.Add(ImageBase);
			return true;
		}

		HMODULE RetainedHandle = nullptr;
		if (::GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
				reinterpret_cast<LPCWSTR>(EntryPoint),
				&RetainedHandle) == 0
			|| RetainedHandle == nullptr)
		{
			return false;
		}

		TCHAR RetainedPath[32768] = {};
		const DWORD RetainedPathLength = ::GetModuleFileNameW(
			RetainedHandle,
			RetainedPath,
			UE_ARRAY_COUNT(RetainedPath));
		FString RetainedFilename = RetainedPathLength > 0
			? FString(RetainedPath)
			: FString::Printf(TEXT("LoadedImage@%p"), RetainedHandle);

		RetainedImageBases.Add(ImageBase);
		Lifetime->PinnedModuleHandles.Add(RetainedHandle);
		Lifetime->PinnedModuleFilenames.Add(MoveTemp(RetainedFilename));
		return true;
	};
#else
	const int32 ModuleCount = FPlatformStackWalk::GetProcessModuleCount();
	if (ModuleCount <= 0)
	{
		return Lifetime;
	}

	TArray<FStackWalkModuleInfo> Modules;
	Modules.SetNumZeroed(ModuleCount);
	const int32 SignatureCount = FPlatformStackWalk::GetProcessModuleSignatures(
		Modules.GetData(), Modules.Num());
	if (SignatureCount <= 0)
	{
		return Lifetime;
	}
	Modules.SetNum(SignatureCount, EAllowShrinking::No);

	const FString ExecutablePath = FPaths::ConvertRelativePathToFull(
		FPlatformProcess::ExecutablePath());
	const FString OwnerModuleFilename = FPaths::ConvertRelativePathToFull(
		ModuleManager.GetModuleFilename(ModuleName));

	auto RetainEntryPointImage = [&](const UPTRINT EntryPoint) -> bool
	{
		if (EntryPoint == 0)
		{
			return true;
		}

		const FStackWalkModuleInfo* EntryModule = nullptr;
		for (const FStackWalkModuleInfo& Module : Modules)
		{
			if (EntryPoint >= Module.BaseOfImage
				&& EntryPoint - Module.BaseOfImage < Module.ImageSize)
			{
				EntryModule = &Module;
				break;
			}
		}
		if (EntryModule == nullptr)
		{
			return false;
		}
		if (RetainedImageBases.Contains(EntryModule->BaseOfImage))
		{
			return true;
		}

		FString LoadedImageName(EntryModule->LoadedImageName);
		if (LoadedImageName.IsEmpty())
		{
			LoadedImageName = EntryModule->ImageName;
		}
		const FString FullImageName = FPaths::ConvertRelativePathToFull(
			LoadedImageName);
		const bool bIsExecutable = FPaths::IsSamePath(
			FullImageName, ExecutablePath)
			|| FPaths::GetCleanFilename(LoadedImageName).Equals(
				FPaths::GetCleanFilename(ExecutablePath),
				ESearchCase::IgnoreCase);
		if (bIsExecutable)
		{
			RetainedImageBases.Add(EntryModule->BaseOfImage);
			return true;
		}

		void* Handle = RetainCodeImage(LoadedImageName);
		FString RetainedFilename = LoadedImageName;
		if (Handle == nullptr
			&& FPaths::GetCleanFilename(LoadedImageName).Equals(
				FPaths::GetCleanFilename(OwnerModuleFilename),
				ESearchCase::IgnoreCase))
		{
			Handle = RetainCodeImage(OwnerModuleFilename);
			RetainedFilename = OwnerModuleFilename;
		}
		if (Handle == nullptr)
		{
			return false;
		}

		RetainedImageBases.Add(EntryModule->BaseOfImage);
		Lifetime->PinnedModuleHandles.Add(Handle);
		Lifetime->PinnedModuleFilenames.Add(MoveTemp(RetainedFilename));
		return true;
	};
#endif

	bool bAllEntryPointsRetained = true;
	for (uint32 EntryIndex = 0;
		EntryIndex < View.EntryCount && bAllEntryPointsRetained;
		++EntryIndex)
	{
		const FAngelscriptJITEntryPoints& EntryPoints =
			View.Entries[EntryIndex].EntryPoints;
		bAllEntryPointsRetained = RetainEntryPointImage(
			reinterpret_cast<UPTRINT>(EntryPoints.VMEntry));
		bAllEntryPointsRetained = bAllEntryPointsRetained
			&& RetainEntryPointImage(
				reinterpret_cast<UPTRINT>(EntryPoints.RawEntry));
		bAllEntryPointsRetained = bAllEntryPointsRetained
			&& RetainEntryPointImage(
				reinterpret_cast<UPTRINT>(EntryPoints.ParmsEntry));
	}
	if (bAllEntryPointsRetained)
	{
		Lifetime->bKeepsCodeMapped = true;
		Lifetime->CodeLifetime = Lifetime->PinnedModuleHandles.IsEmpty()
			? EAngelscriptJITProviderCodeLifetime::ProcessLifetimeImage
			: EAngelscriptJITProviderCodeLifetime::PinnedDynamicModule;
	}
#endif

	return Lifetime;
}

namespace
{
	static FAngelscriptJITProviderCatalogDiagnosticSource CopyDiagnosticSource(
		const FAngelscriptJITProviderDiagnosticSource& Source)
	{
		FAngelscriptJITProviderCatalogDiagnosticSource Copy;
		Copy.Section = Source.Section;
		Copy.Row = Source.Row;
		Copy.Column = Source.Column;
		return Copy;
	}

	static TSharedPtr<const FAngelscriptJITProviderCatalog,
		ESPMode::ThreadSafe> CopyCatalog(
		const FAngelscriptJITProviderView& View,
		const TSharedRef<const FAngelscriptJITProviderLifetime,
			ESPMode::ThreadSafe>& Lifetime)
	{
		TSharedRef<FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> Catalog =
			MakeShared<FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe>();
		Catalog->ProviderId = View.ProviderId;
		Catalog->ProviderGeneration = View.ProviderGeneration;
		Catalog->ArtifactSetDigest = View.ArtifactSetDigest;
		Catalog->ArtifactProfile = View.ArtifactProfile;
		Catalog->NativeEnvironmentFingerprint =
			View.NativeEnvironmentFingerprint;
		Catalog->ProviderName = View.ProviderName;
		Catalog->OwnerModuleName = View.OwnerModuleName;
		Catalog->Lifetime = Lifetime;
		Catalog->Entries.Reserve(View.EntryCount);
		for (uint32 EntryIndex = 0; EntryIndex < View.EntryCount; ++EntryIndex)
		{
			const FAngelscriptJITArtifactEntry& Source = View.Entries[EntryIndex];
			FAngelscriptJITProviderCatalogEntry& Entry =
				Catalog->Entries.AddDefaulted_GetRef();
			Entry.Flags = Source.Flags;
			Entry.ModuleKey = Source.ModuleKey;
			Entry.FunctionKey = Source.FunctionKey;
			Entry.ExecutionHash = Source.ExecutionHash;
			Entry.DebugHash = Source.DebugHash;
			Entry.ArtifactProfile = Source.ArtifactProfile;
			Entry.EntryAbiHash = Source.EntryAbiHash;
			Entry.NativeEnvironmentFingerprint =
				Source.NativeEnvironmentFingerprint;
			Entry.EntryPoints = Source.EntryPoints;
			Entry.AvailableExecutionCapabilities =
				Source.AvailableExecutionCapabilities;
			Entry.ExecutionCapabilityProfileHash =
				Source.ExecutionCapabilityProfileHash;
			Entry.RuntimeExecutionRequirementTriggers =
				Source.RuntimeExecutionRequirementTriggers;
			if (Source.ReferenceSlotCount > 0)
			{
				Entry.ReferenceSlots.Append(
					Source.ReferenceSlots,
					static_cast<int32>(Source.ReferenceSlotCount));
			}
			if (Source.SemanticDependencyCount > 0
				&& View.SemanticDependencies != nullptr)
			{
				const uint32 Start = Source.SemanticDependencyStartIndex;
				const uint32 Count = Source.SemanticDependencyCount;
				if (Start + Count <= View.SemanticDependencyCount)
				{
					Entry.SemanticDependencies.Append(
						View.SemanticDependencies + Start,
						static_cast<int32>(Count));
				}
			}
			Entry.NativeCallSites.Reserve(Source.NativeCallSiteCount);
			for (uint32 CallSiteIndex = 0;
				CallSiteIndex < Source.NativeCallSiteCount;
				++CallSiteIndex)
			{
				const FAngelscriptJITNativeCallSite& SourceCallSite =
					Source.NativeCallSites[CallSiteIndex];
				FAngelscriptJITProviderCatalogNativeCallSite& CallSite =
					Entry.NativeCallSites.AddDefaulted_GetRef();
				CallSite.CallSiteIndex = SourceCallSite.CallSiteIndex;
				CallSite.Route = SourceCallSite.Route;
				CallSite.CanonicalDeclaration =
					SourceCallSite.CanonicalDeclaration;
				CallSite.EmittedCppCallee = SourceCallSite.EmittedCppCallee;
				CallSite.RuntimeDllCore = SourceCallSite.RuntimeDllCore != nullptr
					? SourceCallSite.RuntimeDllCore : TEXT("");
				CallSite.MetadataRowSymbol =
					SourceCallSite.MetadataRowSymbol != nullptr
						? SourceCallSite.MetadataRowSymbol : TEXT("");
				CallSite.RegisteredTarget = SourceCallSite.RegisteredTarget;
				CallSite.StableTargetKey = SourceCallSite.StableTargetKey;
				CallSite.ExpectedAbi = SourceCallSite.ExpectedAbi;
				CallSite.ReferenceSlotIndex =
					SourceCallSite.ReferenceSlotIndex;
			}
		}

		if (View.Diagnostics != nullptr)
		{
			const FAngelscriptJITProviderDiagnostics& Source = *View.Diagnostics;
			FAngelscriptJITProviderCatalogDiagnostics& Diagnostics =
				Catalog->Diagnostics.Emplace();
			Diagnostics.SchemaRevision = Source.SchemaRevision;
			Diagnostics.DiagnosticDigest = Source.DiagnosticDigest;
			Diagnostics.RequestKind = Source.RequestKind;
			Diagnostics.RequestedBackendId = Source.RequestedBackendId;
			Diagnostics.CaptureState = Source.CaptureState;
			Diagnostics.FallbackChain.Reserve(Source.FallbackChainCount);
			for (uint32 Index = 0; Index < Source.FallbackChainCount; ++Index)
			{
				Diagnostics.FallbackChain.Add(Source.FallbackChain[Index]);
			}
			Diagnostics.SourceCompilePassCount = Source.SourceCompilePassCount;
			Diagnostics.CompiledModuleCount = Source.CompiledModuleCount;
			Diagnostics.CompiledFunctionCount = Source.CompiledFunctionCount;
			Diagnostics.CompiledTypeCount = Source.CompiledTypeCount;
			Diagnostics.CompiledGlobalCount = Source.CompiledGlobalCount;
			Diagnostics.CompiledDescriptorCount = Source.CompiledDescriptorCount;
			Diagnostics.CompiledDependencyCount = Source.CompiledDependencyCount;
			Diagnostics.CompiledExternalNativeCallCount =
				Source.CompiledExternalNativeCallCount;
			Diagnostics.EmitModuleCount = Source.EmitModuleCount;
			Diagnostics.bHasVerifiedCanonicalAST =
				Source.bHasVerifiedCanonicalAST != 0;
			Diagnostics.TaskError = Source.TaskError;

			Diagnostics.Functions.Reserve(Source.FunctionCount);
			for (uint32 Index = 0; Index < Source.FunctionCount; ++Index)
			{
				const FAngelscriptJITProviderFunctionDiagnostic& SourceFunction =
					Source.Functions[Index];
				FAngelscriptJITProviderCatalogFunctionDiagnostic& Function =
					Diagnostics.Functions.AddDefaulted_GetRef();
				Function.FunctionKey = SourceFunction.FunctionKey;
				Function.ActualBackendId = SourceFunction.ActualBackendId;
				Function.Disposition = SourceFunction.Disposition;
				Function.Detail = SourceFunction.Detail;
				Function.BackendAttemptStart =
					SourceFunction.BackendAttemptStart;
				Function.BackendAttemptCount =
					SourceFunction.BackendAttemptCount;
			}

			Diagnostics.BackendAttempts.Reserve(Source.BackendAttemptCount);
			for (uint32 Index = 0; Index < Source.BackendAttemptCount; ++Index)
			{
				const FAngelscriptJITProviderBackendAttemptDiagnostic& SourceAttempt =
					Source.BackendAttempts[Index];
				FAngelscriptJITProviderCatalogBackendAttemptDiagnostic& Attempt =
					Diagnostics.BackendAttempts.AddDefaulted_GetRef();
				Attempt.BackendId = SourceAttempt.BackendId;
				Attempt.Disposition = SourceAttempt.Disposition;
				Attempt.Detail = SourceAttempt.Detail;
				Attempt.Flags = SourceAttempt.Flags;
				Attempt.FallbackReason = SourceAttempt.FallbackReason;
				Attempt.TypedDetail = SourceAttempt.TypedDetail;
				Attempt.ProcessedSource =
					CopyDiagnosticSource(SourceAttempt.ProcessedSource);
				Attempt.PrimarySemanticFunctionIndex =
					SourceAttempt.PrimarySemanticFunctionIndex;
				Attempt.SemanticFunctionStart =
					SourceAttempt.SemanticFunctionStart;
				Attempt.SemanticFunctionCount =
					SourceAttempt.SemanticFunctionCount;
				Attempt.CallStart = SourceAttempt.CallStart;
				Attempt.CallCount = SourceAttempt.CallCount;
				Attempt.RequiredCapabilities = SourceAttempt.RequiredCapabilities;
				Attempt.AvailableCapabilities = SourceAttempt.AvailableCapabilities;
				Attempt.ExecutionRequirementsIdentity =
					SourceAttempt.ExecutionRequirementsIdentity;
				Attempt.AvailableCapabilityProfileIdentity =
					SourceAttempt.AvailableCapabilityProfileIdentity;
				Attempt.ProfileKind = SourceAttempt.ProfileKind;
				Attempt.ExceptionControlState =
					SourceAttempt.ExceptionControlState;
				Attempt.ExceptionPayloadState =
					SourceAttempt.ExceptionPayloadState;
				Attempt.bRecursionGuardAvailable =
					SourceAttempt.bRecursionGuardAvailable != 0;
			}

			Diagnostics.SemanticFunctions.Reserve(Source.SemanticFunctionCount);
			for (uint32 Index = 0; Index < Source.SemanticFunctionCount; ++Index)
			{
				const FAngelscriptJITProviderCanonicalFunctionDiagnostic&
					SourceFunction = Source.SemanticFunctions[Index];
				FAngelscriptJITProviderCatalogCanonicalFunctionDiagnostic&
					Function = Diagnostics.SemanticFunctions.AddDefaulted_GetRef();
				Function.FunctionKey = SourceFunction.FunctionKey;
				Function.Role = SourceFunction.Role;
				Function.RawTraitBits = SourceFunction.RawTraitBits;
				Function.UnknownTraitBits = SourceFunction.UnknownTraitBits;
				Function.InvocationKind = SourceFunction.InvocationKind;
				Function.ReceiverKind = SourceFunction.ReceiverKind;
				Function.ReceiverParameterIndex =
					SourceFunction.ReceiverParameterIndex;
				Function.CanonicalASTState =
					SourceFunction.CanonicalASTState;
				Function.CleanupPlanState = SourceFunction.CleanupPlanState;
				Function.bHasSuspendState = SourceFunction.bHasSuspendState != 0;
				Function.bHasExceptionCleanup =
					SourceFunction.bHasExceptionCleanup != 0;
				Function.bCleanupPlanCoversAllTransfers =
					SourceFunction.bCleanupPlanCoversAllTransfers != 0;
				Function.ComponentIndex = SourceFunction.ComponentIndex;
				Function.bRecursiveComponent =
					SourceFunction.bRecursiveComponent != 0;
				Function.EligibilityReason = SourceFunction.EligibilityReason;
				Function.EligibilityDetail = SourceFunction.EligibilityDetail;
				Function.EligibilitySource =
					CopyDiagnosticSource(SourceFunction.EligibilitySource);
			}

			Diagnostics.Calls.Reserve(Source.CallCount);
			for (uint32 Index = 0; Index < Source.CallCount; ++Index)
			{
				const FAngelscriptJITProviderTypedCallDiagnostic& SourceCall =
					Source.Calls[Index];
				FAngelscriptJITProviderCatalogTypedCallDiagnostic& Call =
					Diagnostics.Calls.AddDefaulted_GetRef();
				Call.CallerFunctionKey = SourceCall.CallerFunctionKey;
				Call.CalleeFunctionKey = SourceCall.CalleeFunctionKey;
				Call.ExpressionIndex = SourceCall.ExpressionIndex;
				Call.SourceFlags = SourceCall.SourceFlags;
				Call.ProcessedSource =
					CopyDiagnosticSource(SourceCall.ProcessedSource);
				Call.AuthoredSource =
					CopyDiagnosticSource(SourceCall.AuthoredSource);
				Call.GeneratedSource =
					CopyDiagnosticSource(SourceCall.GeneratedSource);
				Call.TargetKind = SourceCall.TargetKind;
				Call.Disposition = SourceCall.Disposition;
				Call.CalleeEligibilityReason = SourceCall.CalleeEligibilityReason;
				Call.CalleeEligibilityDetail = SourceCall.CalleeEligibilityDetail;
				Call.CalleeEligibilitySource =
					CopyDiagnosticSource(SourceCall.CalleeEligibilitySource);
				Call.bHasNativeLinkageValidation =
					SourceCall.bHasNativeLinkageValidation != 0;
				Call.NativeLinkageValidation = SourceCall.NativeLinkageValidation;
				Call.NativeLinkageValidationDetail =
					SourceCall.NativeLinkageValidationDetail;
				Call.CanonicalDeclaration = SourceCall.CanonicalDeclaration;
				Call.EmittedRoute = SourceCall.EmittedRoute;
				Call.EmittedCppCallee = SourceCall.EmittedCppCallee;
				Call.RuntimeDllCore = SourceCall.RuntimeDllCore;
				Call.MetadataRowSymbol = SourceCall.MetadataRowSymbol;
				Call.RegisteredTarget = SourceCall.RegisteredTarget;
				Call.DirectCppSymbol = SourceCall.DirectCppSymbol;
				Call.StableTargetKey = SourceCall.StableTargetKey;
				Call.ExpectedAbi = SourceCall.ExpectedAbi;
				Call.ReferenceSlotIndex = SourceCall.ReferenceSlotIndex;
			}
		}
		return Catalog;
	}

	static bool DiagnosticCatalogsEquivalent(
		const FAngelscriptJITProviderCatalog& Left,
		const FAngelscriptJITProviderCatalog& Right)
	{
		if (Left.Diagnostics.IsSet() != Right.Diagnostics.IsSet())
		{
			return false;
		}
		return !Left.Diagnostics.IsSet()
			|| Left.Diagnostics->DiagnosticDigest
				== Right.Diagnostics->DiagnosticDigest;
	}

	static bool CatalogsEquivalent(
		const FAngelscriptJITProviderCatalog& Left,
		const FAngelscriptJITProviderCatalog& Right)
	{
		if (Left.ProviderId != Right.ProviderId
			|| Left.ProviderGeneration != Right.ProviderGeneration
			|| !(Left.ArtifactSetDigest == Right.ArtifactSetDigest)
			|| !(Left.ArtifactProfile.Hash == Right.ArtifactProfile.Hash)
			|| !(Left.NativeEnvironmentFingerprint
				== Right.NativeEnvironmentFingerprint)
			|| Left.ProviderName != Right.ProviderName
			|| Left.OwnerModuleName != Right.OwnerModuleName
			|| Left.Entries.Num() != Right.Entries.Num())
		{
			return false;
		}

		for (int32 EntryIndex = 0;
			EntryIndex < Left.Entries.Num();
			++EntryIndex)
		{
			const FAngelscriptJITProviderCatalogEntry& A =
				Left.Entries[EntryIndex];
			const FAngelscriptJITProviderCatalogEntry& B =
				Right.Entries[EntryIndex];
			if (A.Flags != B.Flags
				|| A.ModuleKey != B.ModuleKey
				|| A.FunctionKey != B.FunctionKey
				|| !(A.ExecutionHash == B.ExecutionHash)
				|| !(A.DebugHash == B.DebugHash)
				|| !(A.ArtifactProfile.Hash == B.ArtifactProfile.Hash)
				|| !(A.EntryAbiHash == B.EntryAbiHash)
				|| !(A.NativeEnvironmentFingerprint
					== B.NativeEnvironmentFingerprint)
				|| A.AvailableExecutionCapabilities
					!= B.AvailableExecutionCapabilities
				|| !(A.ExecutionCapabilityProfileHash
					== B.ExecutionCapabilityProfileHash)
				|| A.RuntimeExecutionRequirementTriggers
					!= B.RuntimeExecutionRequirementTriggers
				|| A.EntryPoints.VMEntry != B.EntryPoints.VMEntry
				|| A.EntryPoints.RawEntry != B.EntryPoints.RawEntry
				|| A.EntryPoints.ParmsEntry != B.EntryPoints.ParmsEntry
				|| A.ReferenceSlots.Num() != B.ReferenceSlots.Num()
				|| A.NativeCallSites.Num() != B.NativeCallSites.Num())
			{
				return false;
			}
			for (int32 ReferenceIndex = 0;
				ReferenceIndex < A.ReferenceSlots.Num();
				++ReferenceIndex)
			{
				const FAngelscriptJITReferenceSlot& AReference =
					A.ReferenceSlots[ReferenceIndex];
				const FAngelscriptJITReferenceSlot& BReference =
					B.ReferenceSlots[ReferenceIndex];
				if (AReference.SlotIndex != BReference.SlotIndex
					|| AReference.Flags != BReference.Flags
					|| AReference.Reference != BReference.Reference)
				{
					return false;
				}
			}
			for (int32 CallSiteIndex = 0;
				CallSiteIndex < A.NativeCallSites.Num();
				++CallSiteIndex)
			{
				const FAngelscriptJITProviderCatalogNativeCallSite& ACallSite =
					A.NativeCallSites[CallSiteIndex];
				const FAngelscriptJITProviderCatalogNativeCallSite& BCallSite =
					B.NativeCallSites[CallSiteIndex];
				if (ACallSite.CallSiteIndex != BCallSite.CallSiteIndex
					|| ACallSite.Route != BCallSite.Route
					|| ACallSite.CanonicalDeclaration
						!= BCallSite.CanonicalDeclaration
					|| ACallSite.EmittedCppCallee
						!= BCallSite.EmittedCppCallee
					|| ACallSite.RuntimeDllCore != BCallSite.RuntimeDllCore
					|| ACallSite.MetadataRowSymbol
						!= BCallSite.MetadataRowSymbol
					|| ACallSite.RegisteredTarget
						!= BCallSite.RegisteredTarget
					|| !(ACallSite.StableTargetKey
						== BCallSite.StableTargetKey)
					|| !(ACallSite.ExpectedAbi == BCallSite.ExpectedAbi)
					|| ACallSite.ReferenceSlotIndex
						!= BCallSite.ReferenceSlotIndex)
				{
					return false;
				}
			}
		}
		return true;
	}
}

FAngelscriptJITProviderRegistry::FAngelscriptJITProviderRegistry()
{
	TSharedRef<FAngelscriptJITProviderRegistrySnapshot,
		ESPMode::ThreadSafe> Empty =
		MakeShared<FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe>();
	Snapshot = Empty;
}

FAngelscriptJITProviderRegistry::~FAngelscriptJITProviderRegistry()
{
	if (RegisteredHandle.IsValid() || UnregisteredHandle.IsValid())
	{
		StopModularFeatureDiscovery();
	}
}

FAngelscriptJITProviderRegistrationResult
FAngelscriptJITProviderRegistry::RegisterProvider(
	const void* Owner,
	const IAngelscriptJITArtifactProvider& Provider)
{
	FAngelscriptJITProviderRegistrationResult Result;
	if (Owner == nullptr)
	{
		Result.Code = EAngelscriptJITProviderRegistrationCode::InvalidOwner;
		return Result;
	}

	const FAngelscriptJITProviderView* View =
		Provider.GetAngelscriptJITProviderView();
	if (View == nullptr)
	{
		Result.Code = EAngelscriptJITProviderRegistrationCode::InvalidProvider;
		Result.Validation.Code =
			EAngelscriptJITProviderValidationCode::MissingProviderView;
		return Result;
	}

	Result.Validation = FAngelscriptJITProviderManifest::Validate(*View);
	if (!Result.Validation.IsValid())
	{
		Result.Code = EAngelscriptJITProviderRegistrationCode::InvalidProvider;
		return Result;
	}

	const TSharedRef<const FAngelscriptJITProviderLifetime,
		ESPMode::ThreadSafe> Lifetime =
		FAngelscriptJITProviderLifetime::AcquireForLoadedProvider(*View);
	if (!Lifetime->KeepsCodeMapped())
	{
		Result.Code =
			EAngelscriptJITProviderRegistrationCode::CodeLifetimeUnavailable;
		return Result;
	}

	const TSharedPtr<const FAngelscriptJITProviderCatalog,
		ESPMode::ThreadSafe> Candidate = CopyCatalog(*View, Lifetime);
	TSharedPtr<const FAngelscriptJITProviderCatalog,
		ESPMode::ThreadSafe> RetiredCatalog;
	TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
		ESPMode::ThreadSafe> RetiredSnapshot;
	{
		FScopeLock Lock(&Mutex);
		TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe>* ExistingForOwner = CatalogsByOwner.Find(Owner);
		if (ExistingForOwner != nullptr
			&& (*ExistingForOwner)->ProviderId != Candidate->ProviderId)
		{
			Result.Code =
				EAngelscriptJITProviderRegistrationCode::OwnerProviderIdChanged;
			return Result;
		}

		const void* PreviousIdentityOwner = nullptr;
		TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> ExistingForIdentity;
		for (const TPair<const void*,
			TSharedPtr<const FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe>>& Pair : CatalogsByOwner)
		{
			if (Pair.Key != Owner
				&& Pair.Value->ProviderId == Candidate->ProviderId)
			{
				// Live Coding can replace the generated provider object itself,
				// so its C++ address is not a stable ownership identity. Permit
				// the same ProviderId to hand off to a new object only when the
				// diagnostic provider/module identity also agrees. A genuinely
				// different provider claiming the same stable ID remains a hard
				// conflict.
				if (PreviousIdentityOwner != nullptr
					|| Pair.Value->ProviderName != Candidate->ProviderName
					|| Pair.Value->OwnerModuleName != Candidate->OwnerModuleName)
				{
					Result.Code =
						EAngelscriptJITProviderRegistrationCode::ProviderIdConflict;
					return Result;
				}
				PreviousIdentityOwner = Pair.Key;
				ExistingForIdentity = Pair.Value;
			}
		}

		TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> ExistingCatalog = ExistingForOwner != nullptr
			? *ExistingForOwner
			: ExistingForIdentity;
		const bool bTransfersIdentityOwner = ExistingForOwner == nullptr
			&& PreviousIdentityOwner != nullptr;
		if (ExistingCatalog.IsValid())
		{
			if (ExistingCatalog->ProviderGeneration
				== Candidate->ProviderGeneration)
			{
				if (!CatalogsEquivalent(*ExistingCatalog, *Candidate))
				{
					Result.Code =
						EAngelscriptJITProviderRegistrationCode::AmbiguousGeneration;
					return Result;
				}
				if (DiagnosticCatalogsEquivalent(
						*ExistingCatalog, *Candidate))
				{
					if (bTransfersIdentityOwner)
					{
						CatalogsByOwner.Remove(PreviousIdentityOwner);
						CatalogsByOwner.Add(Owner, ExistingCatalog);
						ModularFeatureOwners.Remove(PreviousIdentityOwner);
					}
					Result.Code =
						EAngelscriptJITProviderRegistrationCode::Unchanged;
					return Result;
				}

				RetiredCatalog = ExistingCatalog;
				if (bTransfersIdentityOwner)
				{
					CatalogsByOwner.Remove(PreviousIdentityOwner);
					CatalogsByOwner.Add(Owner, Candidate);
					ModularFeatureOwners.Remove(PreviousIdentityOwner);
				}
				else
				{
					*ExistingForOwner = Candidate;
				}
				Result.Code =
					EAngelscriptJITProviderRegistrationCode::Replaced;
				RetiredSnapshot = PublishSnapshotLocked();
				return Result;
			}
			RetiredCatalog = ExistingCatalog;
			if (bTransfersIdentityOwner)
			{
				CatalogsByOwner.Remove(PreviousIdentityOwner);
				CatalogsByOwner.Add(Owner, Candidate);
				ModularFeatureOwners.Remove(PreviousIdentityOwner);
			}
			else
			{
				*ExistingForOwner = Candidate;
			}
			Result.Code = EAngelscriptJITProviderRegistrationCode::Replaced;
		}
		else
		{
			CatalogsByOwner.Add(Owner, Candidate);
			Result.Code = EAngelscriptJITProviderRegistrationCode::Registered;
		}
		RetiredSnapshot = PublishSnapshotLocked();
	}
	return Result;
}

bool FAngelscriptJITProviderRegistry::UnregisterOwner(const void* Owner)
{
	if (Owner == nullptr)
	{
		return false;
	}
	TSharedPtr<const FAngelscriptJITProviderCatalog,
		ESPMode::ThreadSafe> RetiredCatalog;
	TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
		ESPMode::ThreadSafe> RetiredSnapshot;
	{
		FScopeLock Lock(&Mutex);
		TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe>* Existing = CatalogsByOwner.Find(Owner);
		if (Existing == nullptr)
		{
			return false;
		}
		RetiredCatalog = MoveTemp(*Existing);
		CatalogsByOwner.Remove(Owner);
		ModularFeatureOwners.Remove(Owner);
		RetiredSnapshot = PublishSnapshotLocked();
	}
	return true;
}

TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
	ESPMode::ThreadSafe> FAngelscriptJITProviderRegistry::GetSnapshot() const
{
	FScopeLock Lock(&Mutex);
	return Snapshot;
}

TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
	ESPMode::ThreadSafe>
FAngelscriptJITProviderRegistry::PublishSnapshotLocked()
{
	TSharedRef<FAngelscriptJITProviderRegistrySnapshot,
		ESPMode::ThreadSafe> Published =
		MakeShared<FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe>();
	Published->PublicationOrdinal = NextPublicationOrdinal++;
	CatalogsByOwner.GenerateValueArray(Published->Providers);
	Published->Providers.Sort([](
		const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe>& Left,
		const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe>& Right)
	{
		return Left->ProviderId < Right->ProviderId;
	});
	TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
		ESPMode::ThreadSafe> Previous = MoveTemp(Snapshot);
	Snapshot = Published;
	return Previous;
}

void FAngelscriptJITProviderRegistry::StartModularFeatureDiscovery()
{
	{
		FScopeLock Lock(&Mutex);
		if (RegisteredHandle.IsValid())
		{
			return;
		}
	}

	IModularFeatures& Features = IModularFeatures::Get();
	RegisteredHandle = Features.OnModularFeatureRegistered().AddRaw(
		this, &FAngelscriptJITProviderRegistry::OnModularFeatureRegistered);
	UnregisteredHandle = Features.OnModularFeatureUnregistered().AddRaw(
		this, &FAngelscriptJITProviderRegistry::OnModularFeatureUnregistered);
	const TArray<IAngelscriptJITArtifactProvider*> Providers =
		Features.GetModularFeatureImplementations<
			IAngelscriptJITArtifactProvider>(
				IAngelscriptJITArtifactProvider::FeatureName());
	for (IAngelscriptJITArtifactProvider* Provider : Providers)
	{
		if (Provider != nullptr)
		{
			OnModularFeatureRegistered(
				IAngelscriptJITArtifactProvider::FeatureName(), Provider);
		}
	}
}

void FAngelscriptJITProviderRegistry::StopModularFeatureDiscovery()
{
	IModularFeatures& Features = IModularFeatures::Get();
	if (RegisteredHandle.IsValid())
	{
		Features.OnModularFeatureRegistered().Remove(RegisteredHandle);
		RegisteredHandle.Reset();
	}
	if (UnregisteredHandle.IsValid())
	{
		Features.OnModularFeatureUnregistered().Remove(UnregisteredHandle);
		UnregisteredHandle.Reset();
	}

	TArray<const void*> Owners;
	{
		FScopeLock Lock(&Mutex);
		Owners = ModularFeatureOwners.Array();
	}
	for (const void* Owner : Owners)
	{
		UnregisterOwner(Owner);
	}
}

void FAngelscriptJITProviderRegistry::OnModularFeatureRegistered(
	const FName& Type,
	IModularFeature* Feature)
{
	if (Type != IAngelscriptJITArtifactProvider::FeatureName()
		|| Feature == nullptr)
	{
		return;
	}
	IAngelscriptJITArtifactProvider* Provider =
		static_cast<IAngelscriptJITArtifactProvider*>(Feature);
	const FAngelscriptJITProviderRegistrationResult Result =
		RegisterProvider(Feature, *Provider);
	if (Result.IsAccepted())
	{
		FScopeLock Lock(&Mutex);
		ModularFeatureOwners.Add(Feature);
	}
}

void FAngelscriptJITProviderRegistry::OnModularFeatureUnregistered(
	const FName& Type,
	IModularFeature* Feature)
{
	if (Type == IAngelscriptJITArtifactProvider::FeatureName()
		&& Feature != nullptr)
	{
		UnregisterOwner(Feature);
	}
}

FAngelscriptJITProviderRegistry& FAngelscriptJITProviderRegistry::Get()
{
	static FAngelscriptJITProviderRegistry Registry;
	return Registry;
}
