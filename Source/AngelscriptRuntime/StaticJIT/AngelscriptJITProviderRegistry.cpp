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
			if (Source.ReferenceSlotCount > 0)
			{
				Entry.ReferenceSlots.Append(
					Source.ReferenceSlots,
					static_cast<int32>(Source.ReferenceSlotCount));
			}
		}
		return Catalog;
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
				|| A.EntryPoints.VMEntry != B.EntryPoints.VMEntry
				|| A.EntryPoints.RawEntry != B.EntryPoints.RawEntry
				|| A.EntryPoints.ParmsEntry != B.EntryPoints.ParmsEntry
				|| A.ReferenceSlots.Num() != B.ReferenceSlots.Num())
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
				if (CatalogsEquivalent(*ExistingCatalog, *Candidate))
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
				Result.Code =
					EAngelscriptJITProviderRegistrationCode::AmbiguousGeneration;
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
