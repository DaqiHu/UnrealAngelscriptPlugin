#include "AngelscriptBinds.h"
#include "AngelscriptBindWriteWorkers.h"
#include "AngelscriptBindsInternal.h"
#include "AngelscriptTypeBindInfoStore.h"

#include "AngelscriptEngine.h"
#include "AngelscriptDocs.h"
#include "AngelscriptMemoryTags.h"
#include "AngelscriptPerformanceStats.h"
#include "HAL/MallocLeakDetection.h"
#include "AngelscriptSettings.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Testing/AngelscriptBindExecutionObservation.h"
#include "Testing/AngelscriptEnumTableBaselineProbe.h"
#include "Binds/Helper_ToString.h"

#include "AngelscriptInclude.h"
#include "AngelscriptSettings.h"
//#include "as_property.h"
//#include "as_scriptfunction.h"
#include "as_property.h"
#include "as_scriptfunction.h"
#include "as_definitions.h"
#include "as_datatype.h"

#include "StartAngelscriptHeaders.h"
//#include "as_scriptfunction.h"
//#include "as_objecttype.h"
//#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_typeinfo.h"
#include "frontend/AST/as_type_context.h"
#include "EndAngelscriptHeaders.h"

namespace UE::Angelscript::Private
{
	static bool IsValidBindPhase(const EAngelscriptBindPhase Phase)
	{
		return Phase >= EAngelscriptBindPhase::TypeDeclarations && Phase <= EAngelscriptBindPhase::Finalization;
	}

	static const TCHAR* GetBindPhaseName(const EAngelscriptBindPhase Phase)
	{
		switch (Phase)
		{
		case EAngelscriptBindPhase::TypeDeclarations:
			return TEXT("TypeDeclarations");
		case EAngelscriptBindPhase::TypeInfrastructure:
			return TEXT("TypeInfrastructure");
		case EAngelscriptBindPhase::ExplicitBindings:
			return TEXT("ExplicitBindings");
		case EAngelscriptBindPhase::GeneratedBindings:
			return TEXT("GeneratedBindings");
		case EAngelscriptBindPhase::ReflectionBindings:
			return TEXT("ReflectionBindings");
		case EAngelscriptBindPhase::PostReflectionBindings:
			return TEXT("PostReflectionBindings");
		case EAngelscriptBindPhase::Finalization:
			return TEXT("Finalization");
		default:
			return TEXT("Invalid");
		}
	}

	static FString NormalizeGenerationBindSourcePath(const ANSICHAR* SourceFile)
	{
		if (SourceFile == nullptr || SourceFile[0] == '\0')
		{
			return FString();
		}
		FString Result = ANSI_TO_TCHAR(SourceFile);
		Result.ReplaceInline(TEXT("\\"), TEXT("/"),
			ESearchCase::CaseSensitive);
		const int32 SourceRoot = Result.Find(
			TEXT("/Source/"), ESearchCase::IgnoreCase,
			ESearchDir::FromEnd);
		if (SourceRoot != INDEX_NONE)
		{
			Result.RightChopInline(SourceRoot + 1, EAllowShrinking::No);
		}
		while (Result.StartsWith(TEXT("./")))
		{
			Result.RightChopInline(2, EAllowShrinking::No);
		}
		return Result;
	}

	static FString DescribeBindRecord(const FAngelscriptBindRecord& Record)
	{
		return FString::Printf(
			TEXT("owner='%s', bind='%s', phase='%s', source='%s:%d'"),
			*Record.OwnerModule.ToString(),
			*Record.BindName.ToString(),
			GetBindPhaseName(Record.Phase),
			Record.SourceFile != nullptr ? ANSI_TO_TCHAR(Record.SourceFile) : TEXT("<null>"),
			Record.SourceLine);
	}

	bool FAngelscriptBindCollection::Append(FAngelscriptBindRecord Record, FString& OutDiagnostic)
	{
		if (bSealed)
		{
			if (LateRegistrationFailureDiagnostic.IsEmpty())
			{
				LateRegistrationFailureDiagnostic = FString::Printf(
					TEXT("Cannot append an AngelScript bind after collection finalization; restart the process to load %s."),
					*DescribeBindRecord(Record));
			}
			OutDiagnostic = LateRegistrationFailureDiagnostic;
			#if WITH_DEV_AUTOMATION_TESTS
			FAngelscriptBindExecutionObservation::RecordLateRegistration(OutDiagnostic);
			#endif
			return false;
		}

		Records.Add(MoveTemp(Record));
		OutDiagnostic.Reset();
		return true;
	}

	bool FAngelscriptBindCollection::Finalize(FString& OutDiagnostic)
	{
		if (!LateRegistrationFailureDiagnostic.IsEmpty())
		{
			OutDiagnostic = LateRegistrationFailureDiagnostic;
			return false;
		}
		if (bSealed)
		{
			OutDiagnostic.Reset();
			return true;
		}

		#if WITH_DEV_AUTOMATION_TESTS
		const double FinalizationStartTime = FPlatformTime::Seconds();
		bool bFinalizationSucceeded = false;
		ON_SCOPE_EXIT
		{
			TArray<int32> PhaseProviderCounts;
			PhaseProviderCounts.Init(0, static_cast<int32>(EAngelscriptBindPhase::Finalization) + 1);
			TArray<FName> ProviderOrder;
			ProviderOrder.Reserve(Records.Num());
			for (const FAngelscriptBindRecord& ObservedRecord : Records)
			{
				const int32 PhaseIndex = static_cast<int32>(ObservedRecord.Phase);
				if (PhaseProviderCounts.IsValidIndex(PhaseIndex))
				{
					++PhaseProviderCounts[PhaseIndex];
				}
				ProviderOrder.Add(ObservedRecord.BindName);
			}
			FAngelscriptBindExecutionObservation::RecordCollectionFinalization(
				bFinalizationSucceeded,
				PhaseProviderCounts,
				ProviderOrder,
				FPlatformTime::Seconds() - FinalizationStartTime,
				OutDiagnostic);
		};
		#endif

		for (const FAngelscriptBindRecord& Record : Records)
		{
			if (Record.OwnerModule.IsNone())
			{
				OutDiagnostic = FString::Printf(TEXT("AngelScript bind owner module is required: %s."), *DescribeBindRecord(Record));
				return false;
			}
			if (Record.BindName.IsNone())
			{
				OutDiagnostic = FString::Printf(TEXT("AngelScript bind name is required: %s."), *DescribeBindRecord(Record));
				return false;
			}
			if (!IsValidBindPhase(Record.Phase))
			{
				OutDiagnostic = FString::Printf(TEXT("AngelScript bind phase is invalid: %s."), *DescribeBindRecord(Record));
				return false;
			}
			if (Record.SourceFile == nullptr || *Record.SourceFile == '\0' || Record.SourceLine <= 0)
			{
				OutDiagnostic = FString::Printf(TEXT("AngelScript bind source provenance is required: %s."), *DescribeBindRecord(Record));
				return false;
			}
			if (Record.Callback == nullptr)
			{
				OutDiagnostic = FString::Printf(TEXT("AngelScript bind callback is required: %s."), *DescribeBindRecord(Record));
				return false;
			}
		}

		Records.Sort([](const FAngelscriptBindRecord& Left, const FAngelscriptBindRecord& Right)
		{
			if (Left.Phase != Right.Phase)
			{
				return static_cast<uint8>(Left.Phase) < static_cast<uint8>(Right.Phase);
			}
			if (Left.OwnerModule != Right.OwnerModule)
			{
				return Left.OwnerModule.LexicalLess(Right.OwnerModule);
			}
			if (Left.BindName != Right.BindName)
			{
				return Left.BindName.LexicalLess(Right.BindName);
			}

			const int32 SourceFileComparison = FCStringAnsi::Strcmp(Left.SourceFile, Right.SourceFile);
			if (SourceFileComparison != 0)
			{
				return SourceFileComparison < 0;
			}
			return Left.SourceLine < Right.SourceLine;
		});

		for (int32 RecordIndex = 1; RecordIndex < Records.Num(); ++RecordIndex)
		{
			const FAngelscriptBindRecord& Previous = Records[RecordIndex - 1];
			const FAngelscriptBindRecord& Current = Records[RecordIndex];
			if (Previous.OwnerModule == Current.OwnerModule && Previous.BindName == Current.BindName && Previous.Phase == Current.Phase)
			{
				OutDiagnostic = FString::Printf(
					TEXT("Duplicate AngelScript bind identity (owner='%s', bind='%s', phase='%s') declared at '%s:%d' and '%s:%d'."),
					*Current.OwnerModule.ToString(),
					*Current.BindName.ToString(),
					GetBindPhaseName(Current.Phase),
					ANSI_TO_TCHAR(Previous.SourceFile),
					Previous.SourceLine,
					ANSI_TO_TCHAR(Current.SourceFile),
					Current.SourceLine);
				return false;
			}
		}

		bSealed = true;
		OutDiagnostic.Reset();
		#if WITH_DEV_AUTOMATION_TESTS
		bFinalizationSucceeded = true;
		#endif
		return true;
	}

	asSStableKey FAngelscriptBindCollection::MakeHostKey(const char* Kind, const char* Namespace, const char* Name)
	{
		uint8 Bytes[32] = {};
		const uint32 Serial = HostIdentitySerial++;
		const uint32 Words[8] = {
			FCrc::StrCrc32(Kind ? Kind : ""),
			FCrc::StrCrc32(Namespace ? Namespace : ""),
			FCrc::StrCrc32(Name ? Name : ""),
			Serial,
			0x484F5354,
			0,
			0,
			0
		};
		FMemory::Memcpy(Bytes, Words, sizeof(Words));
		asSStableKey Key;
		check(asSStableKey::TryCreate(MakeArrayView(Bytes), Key));
		return Key;
	}

	void FAngelscriptBindCollection::CaptureHostInspection()
	{
		HostInspection.Reset();
		const asCDefinitions* Graph = GetHostDefinitions();
		if (!Graph) return;
		for (asCTypeInfo* Type : Graph->GetTypes())
		{
			if (!Type) continue;
			FAngelscriptHostDeclarationView View;
			View.Namespace = UTF8_TO_TCHAR(Type->GetNamespace());
			View.Name = UTF8_TO_TCHAR(Type->GetName());
			View.Kind = TEXT("Type");
			HostInspection.Add(MoveTemp(View));
		}
		for (asCScriptFunction* Function : Graph->GetFunctions())
		{
			if (!Function) continue;
			FAngelscriptHostDeclarationView View;
			View.Namespace = UTF8_TO_TCHAR(Function->GetNamespace());
			View.Name = UTF8_TO_TCHAR(Function->GetName());
			View.Kind = TEXT("Function");
			HostInspection.Add(MoveTemp(View));
		}
		for (asCGlobalProperty* Global : Graph->GetGlobals())
		{
			if (!Global) continue;
			FAngelscriptHostDeclarationView View;
			View.Namespace = UTF8_TO_TCHAR(Global->nameSpace ? Global->nameSpace->name.AddressOf() : "");
			View.Name = UTF8_TO_TCHAR(Global->name.AddressOf());
			View.Kind = TEXT("Global");
			if (const void* Address = Global->GetRegisteredAddress())
			{
				if (Global->type.GetTokenType() == ttInt)
				{
					View.IntValue = *static_cast<const int32*>(Address);
				}
			}
			HostInspection.Add(MoveTemp(View));
		}
	}

	bool FAngelscriptBindCollection::ExecuteToHost(FString& OutDiagnostic)
	{
		if (!bSealed)
		{
			OutDiagnostic = TEXT("Cannot execute AngelScript binds before collection finalization.");
			return false;
		}
		if (!LateRegistrationFailureDiagnostic.IsEmpty())
		{
			OutDiagnostic = LateRegistrationFailureDiagnostic;
			return false;
		}
		if (FrozenHost)
		{
			OutDiagnostic.Reset();
			return true;
		}
		asSDefinitionOptions Options;
		Options.Kind = asETypeInfoKind::HostProcess;
		HostBindState.HostWriteWorkerCount = FAngelscriptBindWriteWorkers::ResolveCount();
		BuildingHost = asCDefinitions::Create(Options);
		if (!BuildingHost)
		{
			OutDiagnostic = TEXT("Failed to create HostProcess definitions.");
			return false;
		}
		static TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> ProcessHostTypeContext =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		if (BuildingHost->SetIdentityContext(ProcessHostTypeContext) != asEDefinitionResult::Succeeded)
		{
			OutDiagnostic = TEXT("Failed to attach HostProcess identity context.");
			BuildingHost.Reset();
			return false;
		}
		FAngelscriptBinds HostBinds(*BuildingHost, HostBindState);
		for (const FAngelscriptBindRecord& Record : Records)
		{
			TGuardValue<FName> ActiveOwnerGuard(HostBindState.ActiveBindOwnerModule, Record.OwnerModule);
			TGuardValue<FName> ActiveProviderGuard(HostBindState.ActiveBindProvider, Record.BindName);
			TGuardValue<EAngelscriptBindPhase> ActivePhaseGuard(HostBindState.ActiveBindPhase, Record.Phase);
			TGuardValue<const ANSICHAR*> ActiveSourceFileGuard(HostBindState.ActiveBindSourceFile, Record.SourceFile);
			TGuardValue<int32> ActiveSourceLineGuard(HostBindState.ActiveBindSourceLine, Record.SourceLine);
			++CallbackInvocationCounts.FindOrAdd(Record.BindName);
			Record.Callback(HostBinds);
			if (HostBinds.HasRegistrationFailure())
			{
				OutDiagnostic = HostBinds.GetRegistrationFailureDiagnostic();
				if (OutDiagnostic.IsEmpty())
				{
					OutDiagnostic = FString::Printf(
						TEXT("HostProcess bind '%s' failed without a diagnostic."),
						*Record.BindName.ToString());
				}
				BuildingHost.Reset();
				return false;
			}
		}
		if (BuildingHost->FinalizeLayouts() != asEDefinitionResult::Succeeded
			|| BuildingHost->Freeze() != asEDefinitionResult::Succeeded)
		{
			OutDiagnostic = TEXT("HostProcess graph failed to freeze.");
			BuildingHost.Reset();
			return false;
		}
		FrozenHost = asCDefinitions::AdoptGraph(MoveTemp(BuildingHost));
		CaptureHostInspection();
		OutDiagnostic.Reset();
		return true;
	}

	const asCDefinitions* FAngelscriptBindCollection::GetHostDefinitions() const
	{
		return FrozenHost ? FrozenHost.Get() : BuildingHost.Get();
	}

	asCDefinitionGraphLease FAngelscriptBindCollection::RetainHostGraph() const
	{
		return FrozenHost ? FrozenHost.Get()->RetainGraph() : asCDefinitionGraphLease();
	}

	int32 FAngelscriptBindCollection::GetCallbackInvocationCount(FName BindName) const
	{
		if (const int32* Count = CallbackInvocationCounts.Find(BindName)) return *Count;
		return 0;
	}

	void FAngelscriptBindCollection::InspectHostDeclarations(TArray<FAngelscriptHostDeclarationView>& Out) const
	{
		Out = HostInspection;
	}

	bool FAngelscriptBindCollection::PrepareForEngineInitialization(
		const TConstArrayView<FString> ModuleNames,
		const TFunctionRef<bool(FName, FString&)> LoadModule,
		FString& OutDiagnostic)
	{
		if (!LateRegistrationFailureDiagnostic.IsEmpty())
		{
			OutDiagnostic = LateRegistrationFailureDiagnostic;
			return false;
		}
		if (bSealed)
		{
			OutDiagnostic.Reset();
			return true;
		}
		if (bPreparing)
		{
			OutDiagnostic = TEXT("AngelScript bind collection preparation is already in progress.");
			return false;
		}

		TGuardValue<bool> PreparingGuard(bPreparing, true);
		for (const FString& RawModuleName : ModuleNames)
		{
			const FString ModuleNameString = RawModuleName.TrimStartAndEnd();
			if (ModuleNameString.IsEmpty())
			{
				continue;
			}

			const FName ModuleName(*ModuleNameString);
			FString LoadDiagnostic;
			if (!LoadModule(ModuleName, LoadDiagnostic))
			{
				OutDiagnostic = FString::Printf(
					TEXT("Failed to load generated AngelScript bind module '%s' before collection finalization: %s"),
					*ModuleName.ToString(),
					LoadDiagnostic.IsEmpty() ? TEXT("module loader returned failure") : *LoadDiagnostic);
				return false;
			}
		}

		return Finalize(OutDiagnostic);
	}

	bool FAngelscriptBindCollection::Execute(FAngelscriptBinds& Binds, FString& OutDiagnostic) const
	{
		return Execute(
			Binds,
			EAngelscriptBindPhase::TypeDeclarations,
			EAngelscriptBindPhase::Finalization,
			OutDiagnostic);
	}

	bool FAngelscriptBindCollection::Execute(
		FAngelscriptBinds& Binds,
		const EAngelscriptBindPhase FirstPhase,
		const EAngelscriptBindPhase LastPhase,
		FString& OutDiagnostic) const
	{
		AS_PERF_SCOPE_BINDS_EXECUTE_CALLBACKS();

		if (!bSealed)
		{
			OutDiagnostic = TEXT("Cannot execute AngelScript binds before collection finalization.");
			return false;
		}
		if (!LateRegistrationFailureDiagnostic.IsEmpty())
		{
			OutDiagnostic = LateRegistrationFailureDiagnostic;
			return false;
		}

		if (Binds.HasRegistrationFailure())
		{
			OutDiagnostic = Binds.GetRegistrationFailureDiagnostic();
			return false;
		}

		if (static_cast<uint8>(FirstPhase) > static_cast<uint8>(LastPhase))
		{
			OutDiagnostic = TEXT("Cannot execute an inverted AngelScript bind phase range.");
			return false;
		}

		#if WITH_DEV_AUTOMATION_TESTS
		bool bExecutionSucceeded = false;
		FAngelscriptBindExecutionObservation::BeginObservationPass(&Binds.GetTargetEngine());
		ON_SCOPE_EXIT
		{
			FAngelscriptBindExecutionObservation::EndObservationPass(
				bExecutionSucceeded,
				OutDiagnostic);
		};
		#endif

		for (const FAngelscriptBindRecord& Record : Records)
		{
			if (static_cast<uint8>(Record.Phase) < static_cast<uint8>(FirstPhase)
				|| static_cast<uint8>(Record.Phase) > static_cast<uint8>(LastPhase))
			{
				continue;
			}

			FAngelscriptBindState& BindState = Binds.GetTargetBindState();
			TGuardValue<FName> ActiveOwnerGuard(BindState.ActiveBindOwnerModule, Record.OwnerModule);
			TGuardValue<FName> ActiveProviderGuard(BindState.ActiveBindProvider, Record.BindName);
			TGuardValue<EAngelscriptBindPhase> ActivePhaseGuard(BindState.ActiveBindPhase, Record.Phase);
			TGuardValue<const ANSICHAR*> ActiveSourceFileGuard(BindState.ActiveBindSourceFile, Record.SourceFile);
			TGuardValue<int32> ActiveSourceLineGuard(BindState.ActiveBindSourceLine, Record.SourceLine);
			#if WITH_DEV_AUTOMATION_TESTS
			FAngelscriptBindExecutionObservation::BeginProvider(
				Record.OwnerModule,
				Record.BindName,
				Record.Phase,
				Record.SourceFile,
				Record.SourceLine);
			#endif
			Record.Callback(Binds);
			if (Binds.HasRegistrationFailure())
			{
				OutDiagnostic = Binds.GetRegistrationFailureDiagnostic();
				#if WITH_DEV_AUTOMATION_TESTS
				FAngelscriptBindExecutionObservation::EndProvider(false, OutDiagnostic);
				#endif
				return false;
			}
			#if WITH_DEV_AUTOMATION_TESTS
			FAngelscriptBindExecutionObservation::EndProvider(true);
			#endif
		}
		OutDiagnostic.Reset();
		#if WITH_DEV_AUTOMATION_TESTS
		bExecutionSucceeded = true;
		#endif
		return true;
	}
}

static UE::Angelscript::Private::FAngelscriptBindCollection& GetRegisteredBindCollection()
{
	static UE::Angelscript::Private::FAngelscriptBindCollection Collection;
	return Collection;
}

const UE::Angelscript::Private::FAngelscriptBindCollection& FAngelscriptBind::GetRegisteredCollection()
{
	return GetRegisteredBindCollection();
}

static TUniquePtr<UE::Angelscript::Private::FAngelscriptBindCollection> GProcessHostCollection;
static int32 GProcessHostCaptureCount = 0;

bool FAngelscriptBind::EnsureProcessHostCollection(FString& OutDiagnostic)
{
	if (GProcessHostCollection && GProcessHostCollection->GetHostDefinitions()
		&& GProcessHostCollection->GetHostDefinitions()->GetState() == asEDefinitionState::Frozen)
	{
		OutDiagnostic.Reset();
		return true;
	}
	GProcessHostCollection = MakeUnique<UE::Angelscript::Private::FAngelscriptBindCollection>();
	for (const FAngelscriptBindRecord& Record : GetRegisteredBindCollection().GetRecords())
	{
		if (!GProcessHostCollection->Append(Record, OutDiagnostic))
		{
			GProcessHostCollection.Reset();
			return false;
		}
	}
	if (!GProcessHostCollection->Finalize(OutDiagnostic)
		|| !GProcessHostCollection->ExecuteToHost(OutDiagnostic))
	{
		GProcessHostCollection.Reset();
		return false;
	}
	++GProcessHostCaptureCount;
	OutDiagnostic.Reset();
	return true;
}

const UE::Angelscript::Private::FAngelscriptBindCollection* FAngelscriptBind::GetProcessHostCollection()
{
	return GProcessHostCollection.Get();
}

#if WITH_DEV_AUTOMATION_TESTS
int32 FAngelscriptBind::GetProcessHostCaptureCountForTesting()
{
	return GProcessHostCaptureCount;
}
#endif

TArray<FAngelscriptBindMetadata> FAngelscriptBind::GetRegisteredBindMetadata()
{
	TArray<FAngelscriptBindMetadata> Metadata;
	const TConstArrayView<FAngelscriptBindRecord> Records =
		GetRegisteredBindCollection().GetRecords();
	Metadata.Reserve(Records.Num());
	for (const FAngelscriptBindRecord& Record : Records)
	{
		FAngelscriptBindMetadata& Entry = Metadata.AddDefaulted_GetRef();
		Entry.OwnerModule = Record.OwnerModule;
		Entry.BindName = Record.BindName;
		Entry.Phase = Record.Phase;
		Entry.SourceFile = Record.SourceFile != nullptr
			? ANSI_TO_TCHAR(Record.SourceFile)
			: FString();
		Entry.SourceLine = Record.SourceLine;
	}
	return Metadata;
}

#if WITH_DEV_AUTOMATION_TESTS
static int32 PrepareInvocationCountForTesting = 0;
#endif

static FAngelscriptBindRecord MakeDirectBindRecord(
	const FName BindName,
	const EAngelscriptBindPhase Phase,
	const FAngelscriptBindCallback Callback,
	const ANSICHAR* OwnerModule,
	const ANSICHAR* SourceFile,
	const int32 SourceLine)
{
	FAngelscriptBindRecord Record;
	Record.OwnerModule = FName(OwnerModule != nullptr ? ANSI_TO_TCHAR(OwnerModule) : TEXT(""));
	Record.BindName = BindName;
	Record.Phase = Phase;
	Record.SourceFile = SourceFile;
	Record.SourceLine = SourceLine;
	Record.Callback = Callback;
	return Record;
}

FAngelscriptBind::FAngelscriptBind(
	const FName BindName,
	const EAngelscriptBindPhase Phase,
	const FAngelscriptBindCallback Callback,
	const ANSICHAR* OwnerModule,
	const ANSICHAR* SourceFile,
	const int32 SourceLine)
{
	FString Diagnostic;
	if (!GetRegisteredBindCollection().Append(MakeDirectBindRecord(BindName, Phase, Callback, OwnerModule, SourceFile, SourceLine), Diagnostic))
	{
		UE_LOG(Angelscript, Error, TEXT("%s"), *Diagnostic);
	}
}

FAngelscriptBind::FAngelscriptBind(
	const FName BindName,
	const EAngelscriptBindPhase Phase,
	const FAngelscriptBindCallback Callback,
	bool (*Condition)(const FAngelscriptBindRecordingPolicy&),
	const ANSICHAR* OwnerModule,
	const ANSICHAR* SourceFile,
	const int32 SourceLine)
{
	FString Diagnostic;
	FAngelscriptBindRecord Record = MakeDirectBindRecord(BindName, Phase, Callback, OwnerModule, SourceFile, SourceLine);
	Record.Condition = Condition;
	if (!GetRegisteredBindCollection().Append(MoveTemp(Record), Diagnostic))
	{
		UE_LOG(Angelscript, Error, TEXT("%s"), *Diagnostic);
	}
}

FAngelscriptBind::FAngelscriptBind(
	const FName BindName,
	const EAngelscriptBindPhase Phase,
	const FAngelscriptBindCallback Callback,
	const bool bContainsTypeDeclarations,
	const ANSICHAR* OwnerModule,
	const ANSICHAR* SourceFile,
	const int32 SourceLine)
{
	FString Diagnostic;
	FAngelscriptBindRecord Record = MakeDirectBindRecord(BindName, Phase, Callback, OwnerModule, SourceFile, SourceLine);
	Record.bContainsTypeDeclarations = bContainsTypeDeclarations;
	if (!GetRegisteredBindCollection().Append(MoveTemp(Record), Diagnostic))
	{
		UE_LOG(Angelscript, Error, TEXT("%s"), *Diagnostic);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
FAngelscriptBind::FAngelscriptBind(
	UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
	const FName BindName,
	const EAngelscriptBindPhase Phase,
	const FAngelscriptBindCallback Callback,
	const ANSICHAR* OwnerModule,
	const ANSICHAR* SourceFile,
	const int32 SourceLine)
{
	FString Diagnostic;
	if (!Collection.Append(MakeDirectBindRecord(BindName, Phase, Callback, OwnerModule, SourceFile, SourceLine), Diagnostic))
	{
		UE_LOG(Angelscript, Error, TEXT("%s"), *Diagnostic);
	}
}
#endif

bool FAngelscriptBind::FinalizeRegisteredBinds(FString& OutDiagnostic)
{
	return GetRegisteredBindCollection().Finalize(OutDiagnostic);
}

bool FAngelscriptBind::PrepareForEngineInitialization(FString& OutDiagnostic)
{
#if WITH_DEV_AUTOMATION_TESTS
	++PrepareInvocationCountForTesting;
#endif

	if (!IsInGameThread())
	{
		OutDiagnostic = TEXT("AngelScript bind modules must load and finalize on the Game Thread before engine initialization.");
		return false;
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Angelscript"));
	if (!Plugin.IsValid())
	{
		OutDiagnostic = TEXT("Cannot locate the Angelscript plugin while preparing direct binds.");
		return false;
	}

	TArray<FString> ModuleNames;
	const FString CachePath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("BindModules.Cache"));
	if (IFileManager::Get().FileExists(*CachePath) && !FFileHelper::LoadFileToStringArray(ModuleNames, *CachePath))
	{
		OutDiagnostic = FString::Printf(TEXT("Failed to read generated AngelScript bind module cache '%s'."), *CachePath);
		return false;
	}

	return GetRegisteredBindCollection().PrepareForEngineInitialization(
		ModuleNames,
		[](const FName ModuleName, FString& LoadDiagnostic)
		{
			if (FModuleManager::Get().LoadModule(ModuleName) == nullptr)
			{
				LoadDiagnostic = TEXT("Unreal ModuleManager could not load the module");
				return false;
			}
			return true;
		},
		OutDiagnostic);
}

#if WITH_DEV_AUTOMATION_TESTS
void FAngelscriptBind::ResetPrepareInvocationCountForTesting()
{
	PrepareInvocationCountForTesting = 0;
}

int32 FAngelscriptBind::GetPrepareInvocationCountForTesting()
{
	return PrepareInvocationCountForTesting;
}

const void* FAngelscriptBind::GetRegisteredCollectionIdentityForTesting()
{
	return &GetRegisteredBindCollection();
}

int32 FAngelscriptBind::GetRegisteredBindCountForTesting()
{
	return GetRegisteredBindCollection().GetRecords().Num();
}

bool FAngelscriptBind::IsRegisteredCollectionSealedForTesting()
{
	return GetRegisteredBindCollection().IsSealed();
}
#endif

bool FAngelscriptBind::ExecuteRegisteredBinds(FAngelscriptBinds& Binds, FString& OutDiagnostic)
{
	return GetRegisteredBindCollection().Execute(Binds, OutDiagnostic);
}

bool FAngelscriptBind::ExecuteRegisteredBindPhases(
	FAngelscriptBinds& Binds,
	const EAngelscriptBindPhase FirstPhase,
	const EAngelscriptBindPhase LastPhase,
	FString& OutDiagnostic)
{
	return GetRegisteredBindCollection().Execute(Binds, FirstPhase, LastPhase, OutDiagnostic);
}

#if WITH_DEV_AUTOMATION_TESTS
static void CDECL InjectedDirectBindPublicationFailure()
{
}

static void ExecuteDirectBindArchitectureProbe(FAngelscriptBinds& Binds)
{
	if (Binds.IsRecording())
	{
		Binds.AddDirectBindArchitectureProbeEffect();
		return;
	}
	if (Binds.IsHostTarget())
	{
		return;
	}
	if (Binds.GetTargetEngine().ShouldInjectDirectBindFailureForTesting())
	{
		Binds.BindGlobalFunctionForTarget(
			"void __InjectedDirectBindPublicationFailure()",
			&InjectedDirectBindPublicationFailure);
		Binds.BindGlobalFunctionForTarget(
			"void __InjectedDirectBindPublicationFailure()",
			&InjectedDirectBindPublicationFailure);
		return;
	}
	++Binds.GetTargetBindState().DirectCallbackExecutionCountForTesting;
}

AS_FORCE_LINK const FAngelscriptBind Bind_DirectBindArchitectureProbe(
	TEXT("DirectBindArchitectureProbe"),
	EAngelscriptBindPhase::Finalization,
	&ExecuteDirectBindArchitectureProbe);
#endif

static FAngelscriptBindState& GetBindState()
{
	FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
	FAngelscriptBindState* State = Engine.GetBindState();
	checkf(State != nullptr, TEXT("AngelScript bind state is unavailable for the current engine."));
	return *State;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::EditRecord(
	TFunctionRef<void(FAngelscriptTypeBindInfoMember&)> Edit)
{
	if (const auto* Existing = RecordingStore->GetMember(RecordingHandle))
	{
		auto Member = *Existing;
		Edit(Member);
		FString Diagnostic;
		if (!RecordingStore->UpdateMember(RecordingHandle, Member, Diagnostic)) RecordingHandle = {};
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::RecordTrait(
	EAngelscriptBindFunctionTrait Trait, bool bEnabled)
{
	return EditRecord([&](auto& Member)
	{
		if (bEnabled) Member.Traits |= Trait;
		else Member.Traits &= ~Trait;
	});
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::RecordRecipe(FAngelscriptNativeBindRecipe Recipe)
{
	return EditRecord([&](auto& Member) { Member.NativeRecipes.Add(MoveTemp(Recipe)); });
}

bool FAngelscriptBoundFunction::IsValid() const
{
	if (RecordingStore) return RecordingStore->GetMember(RecordingHandle) != nullptr;
	return TargetEngine != nullptr && FunctionId >= 0 && GetFunction() != nullptr;
}

FAngelscriptEngine& FAngelscriptBoundFunction::GetTargetEngine() const
{
	check(TargetEngine != nullptr);
	return *TargetEngine;
}

asIScriptFunction* FAngelscriptBoundFunction::GetFunction() const
{
	if (TargetEngine == nullptr || FunctionId < 0 || TargetEngine->GetScriptEngine() == nullptr)
	{
		return nullptr;
	}
	return TargetEngine->GetScriptEngine()->GetFunctionById(FunctionId);
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::EditorOnly(const bool bEditorOnly)
{
	if (RecordingStore) return RecordTrait(EAngelscriptBindFunctionTrait::EditorOnly, bEditorOnly);
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_EDITOR_ONLY, bEditorOnly);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::Deprecated(const ANSICHAR* DeprecationMessage)
{
	if (RecordingStore) return EditRecord([&](auto& Member) { Member.Traits |= EAngelscriptBindFunctionTrait::Deprecated; Member.DeprecationMessage = DeprecationMessage ? ANSI_TO_TCHAR(DeprecationMessage) : TEXT(""); });
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_DEPRECATED, true);
#if WITH_EDITOR
		Function->deprecationMessage = DeprecationMessage != nullptr ? DeprecationMessage : "";
#endif
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::PropertyAccessor(const bool bPropertyAccessor)
{
	if (RecordingStore) return RecordTrait(EAngelscriptBindFunctionTrait::PropertyAccessor, bPropertyAccessor);
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->SetProperty(bPropertyAccessor);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::GeneratedAccessor(const bool bGeneratedAccessor)
{
	if (RecordingStore) return RecordTrait(EAngelscriptBindFunctionTrait::GeneratedAccessor, bGeneratedAccessor);
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_GENERATED_FUNCTION, bGeneratedAccessor);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NoDiscard(const bool bNoDiscard)
{
	if (RecordingStore) return RecordTrait(EAngelscriptBindFunctionTrait::NoDiscard, bNoDiscard);
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_NODISCARD, bNoDiscard);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::WorldContext(const bool bRequiresWorldContext)
{
	if (RecordingStore) return RecordTrait(EAngelscriptBindFunctionTrait::WorldContext, bRequiresWorldContext);
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_USES_WORLDCONTEXT, bRequiresWorldContext);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::Callable(const bool bCallable)
{
	if (RecordingStore) return RecordTrait(EAngelscriptBindFunctionTrait::NotCallable, !bCallable);
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_NOT_CALLABLE, !bCallable);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::ImplicitConstructor(const bool bImplicitConstructor)
{
	if (RecordingStore) return RecordTrait(EAngelscriptBindFunctionTrait::ImplicitConstructor, bImplicitConstructor);
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_IMPLICITCONSTRUCTOR, bImplicitConstructor);
		if (asCObjectType* ObjectType = Function->objectType)
		{
			ObjectType->hasImplicitConstructors = bImplicitConstructor;
		}
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::ForceConstArgumentExpressions(const bool bForceConst)
{
	if (RecordingStore) return RecordTrait(EAngelscriptBindFunctionTrait::ForceConstArguments, bForceConst);
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS, bForceConst);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::DeterminesOutputType(const int32 ArgumentIndex)
{
	if (RecordingStore) return EditRecord([&](auto& Member) { Member.DeterminesOutputTypeArgument = ArgumentIndex; });
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->determinesOutputTypeArgumentIndex = static_cast<int8>(ArgumentIndex);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::PassScriptFunctionAsFirstParam()
{
	if (RecordingStore) return EditRecord([&](auto& Member) { Member.FirstParameter = EAngelscriptBindFirstParameter::ScriptFunction; });
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()); Function != nullptr && Function->sysFuncIntf != nullptr)
	{
		Function->sysFuncIntf->passFirstParamMetaData = asEFirstParamMetaData::ScriptFunction;
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::PassScriptObjectTypeAsFirstParam()
{
	if (RecordingStore) return EditRecord([&](auto& Member) { Member.FirstParameter = EAngelscriptBindFirstParameter::ScriptObjectType; });
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()); Function != nullptr && Function->sysFuncIntf != nullptr)
	{
		Function->sysFuncIntf->passFirstParamMetaData = asEFirstParamMetaData::ScriptObjectType;
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::Documentation(FStringView Documentation, FStringView Category, UFunction* UnrealFunction)
{
	if (RecordingStore) return EditRecord([&](auto& Member) { Member.Documentation = FString(Documentation); Member.Category = FString(Category); Member.DocumentationFunction = UnrealFunction; });
#if WITH_EDITOR
	if (IsValid())
	{
		FAngelscriptDocs::AddUnrealDocumentation(GetTargetEngine(), FunctionId, Documentation, Category, UnrealFunction);
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeConstructor(const ANSICHAR* Name, const bool bTrivial, const ANSICHAR* CustomForm)
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::Constructor;
		Recipe.Name = ANSI_TO_TCHAR(Name); Recipe.bTrivial = bTrivial; Recipe.CustomForm = CustomForm ? ANSI_TO_TCHAR(CustomForm) : TEXT("");
		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindNativeConstructor(GetTargetEngine(), GetFunction(), Name, bTrivial, CustomForm);
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeDestructor(const ANSICHAR* Name, const bool bTrivial)
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::Destructor;
		Recipe.Name = ANSI_TO_TCHAR(Name); Recipe.bTrivial = bTrivial;
		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindNativeDestructor(GetTargetEngine(), GetFunction(), Name, bTrivial);
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeAssignment(const ANSICHAR* Name, const bool bTrivial)
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::Assignment;
		Recipe.Name = ANSI_TO_TCHAR(Name); Recipe.bTrivial = bTrivial;
		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindNativeAssignment(GetTargetEngine(), GetFunction(), Name, bTrivial);
	}
#else
	(void)Name;
	(void)bTrivial;
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeUObjectCast(const FString& TargetType, const bool bGuaranteed)
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::UObjectCast;
		Recipe.Name = TargetType; Recipe.bGuaranteed = bGuaranteed;
		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindNativeUObjectCast(GetTargetEngine(), GetFunction(), TargetType, bGuaranteed);
	}
#else
	(void)TargetType;
	(void)bGuaranteed;
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeMethod(const ANSICHAR* Name, const bool bTrivial)
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::Method;
		Recipe.Name = ANSI_TO_TCHAR(Name); Recipe.bTrivial = bTrivial;
		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindNativeMethod(GetTargetEngine(), GetFunction(), Name, bTrivial);
	}
#else
	(void)Name;
	(void)bTrivial;
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeFunction(const ANSICHAR* Name, const bool bTrivial)
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::Function;
		Recipe.Name = ANSI_TO_TCHAR(Name); Recipe.bTrivial = bTrivial;
		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindNativeFunction(GetTargetEngine(), GetFunction(), Name, bTrivial);
	}
#else
	(void)Name;
	(void)bTrivial;
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeFunctionHeader(const ANSICHAR* Name, const ANSICHAR* Header, const bool bTrivial)
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::FunctionHeader;
		Recipe.Name = ANSI_TO_TCHAR(Name); Recipe.Header = ANSI_TO_TCHAR(Header); Recipe.bTrivial = bTrivial;
		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindNativeFunctionHeader(GetTargetEngine(), GetFunction(), Name, bTrivial, Header);
	}
#else
	(void)Name;
	(void)Header;
	(void)bTrivial;
#endif
	return *this;
}

#if AS_CAN_GENERATE_JIT
FAngelscriptBoundFunction& FAngelscriptBoundFunction::ExternalNativeCall(
	const FAngelscriptStaticJITNativeCallDescriptor& Descriptor)
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::ExternalCall;
		Recipe.ExternalCall = MakeShared<FAngelscriptStaticJITNativeCallDescriptor>(Descriptor);
		return RecordRecipe(MoveTemp(Recipe));
	}
	if (IsValid())
	{
		FAngelscriptStaticJITNativeCallRegistry::Attach(
			GetTargetEngine(), GetFunction(), Descriptor);
	}

	return *this;
}
#endif

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeUFunction(UFunction* Function, const FString& Name, const bool bTrivial)
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::UFunction;
		Recipe.UnrealFunction = Function; Recipe.Name = Name; Recipe.bTrivial = bTrivial;
		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindUFunction(GetTargetEngine(), GetFunction(), Function, Name, bTrivial);
	}
#else
	(void)Function;
	(void)Name;
	(void)bTrivial;
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeTArrayIndex()
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::ArrayIndex;

		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindTArrayIndex(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeTArrayIteratorCreate()
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::ArrayIteratorCreate;

		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindTArrayIteratorCreate(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeTArrayIteratorProceed()
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::ArrayIteratorProceed;

		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindTArrayIteratorProceed(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeTemplateInstantiatedCall(
	const ANSICHAR* Name,
	const bool bTrivial,
	const bool bNeedsCompare,
	const bool bNeedsCopy)
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::TemplateCall;
		Recipe.Name = ANSI_TO_TCHAR(Name); Recipe.bTrivial = bTrivial; Recipe.bNeedsCompare = bNeedsCompare; Recipe.bNeedsCopy = bNeedsCopy;
		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindTemplateInstantiatedCall(
			GetTargetEngine(),
			GetFunction(),
			Name,
			bTrivial,
			bNeedsCompare,
			bNeedsCopy);
	}
#else
	(void)Name;
	(void)bTrivial;
	(void)bNeedsCompare;
	(void)bNeedsCopy;
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeDelegateExecute()
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::DelegateExecute;

		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindDelegateExecute(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeMulticastExecute()
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::MulticastExecute;

		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindMulticastExecute(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeEventFunctionExecute()
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::EventExecute;

		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindEventFunctionExecute(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativePushArgument()
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::PushArgument;

		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindPushArg(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativePushArgumentRef()
{
	if (RecordingStore)
	{
		FAngelscriptNativeBindRecipe Recipe;
		Recipe.Kind = EAngelscriptNativeBindRecipeKind::PushArgumentRef;

		return RecordRecipe(MoveTemp(Recipe));
	}
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindPushArgRef(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::CompileOutEntirely()
{
	if (RecordingStore) return EditRecord([&](auto& Member) { Member.CompileOut = EAngelscriptBindCompileOut::Entirely; });
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->compileOutType = asECompileOutType::CompileOutEntirely;
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::CompileOutAsMethodChain()
{
	if (RecordingStore) return EditRecord([&](auto& Member) { Member.CompileOut = EAngelscriptBindCompileOut::MethodChain; });
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->compileOutType = asECompileOutType::CompileOutAsMethodChain;
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::CompileOutInTest()
{
	if (RecordingStore) return EditRecord([&](auto& Member) { Member.CompileOut = EAngelscriptBindCompileOut::InTest; });
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		if (UE_BUILD_TEST || UE_BUILD_SHIPPING || (WITH_EDITOR && TargetEngine->bSimulateCooked))
		{
			Function->compileOutType = asECompileOutType::CompileOutEntirely;
		}
		if (TargetEngine->ConfigSettings != nullptr && TargetEngine->ConfigSettings->bForceConstWithinDevelopmentOnlyFunctions)
		{
			Function->traits.SetTrait(asEFuncTrait::asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS, true);
		}
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::CompileOutIfNoLog()
{
	if (RecordingStore) return EditRecord([&](auto& Member) { Member.CompileOut = EAngelscriptBindCompileOut::IfNoLog; });
	return CompileOutInTest();
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::CompileOutAsEnsure()
{
	if (RecordingStore) return EditRecord([&](auto& Member) { Member.CompileOut = EAngelscriptBindCompileOut::Ensure; Member.Traits |= EAngelscriptBindFunctionTrait::NoDiscard; });
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_NODISCARD, true);
		if (UE_BUILD_SHIPPING || (WITH_EDITOR && TargetEngine->bSimulateCooked))
		{
			Function->compileOutType = asECompileOutType::ReplaceWithFirstParam;
		}
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::CompileOutAsCheck()
{
	if (RecordingStore) return EditRecord([&](auto& Member) { Member.CompileOut = EAngelscriptBindCompileOut::Check; });
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		if (UE_BUILD_SHIPPING || (WITH_EDITOR && TargetEngine->bSimulateCooked))
		{
			Function->compileOutType = asECompileOutType::CompileOutEntirely;
		}
		if (TargetEngine->ConfigSettings != nullptr && TargetEngine->ConfigSettings->bForceConstWithinDevelopmentOnlyFunctions)
		{
			Function->traits.SetTrait(asEFuncTrait::asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS, true);
		}
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::ReplaceWithFirstArgInTest()
{
	if (RecordingStore) return EditRecord([&](auto& Member) { Member.CompileOut = EAngelscriptBindCompileOut::ReplaceFirstArgumentInTest; });
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		if (UE_BUILD_TEST || UE_BUILD_SHIPPING || (WITH_EDITOR && TargetEngine->bSimulateCooked))
		{
			Function->compileOutType = asECompileOutType::ReplaceWithFirstParam;
		}
	}
	return *this;
}

bool FAngelscriptBoundProperty::IsValid() const
{
	if (RecordingStore) return RecordingStore->GetMember(RecordingHandle) != nullptr;
	if (TargetEngine == nullptr || PropertyId < 0 || TargetEngine->GetScriptEngine() == nullptr)
	{
		return false;
	}
	if (!bGlobal)
	{
		return true;
	}
	return static_cast<asUINT>(PropertyId) < TargetEngine->Engine->globalProperties.GetLength()
		&& TargetEngine->Engine->globalProperties[PropertyId] != nullptr;
}

FAngelscriptEngine& FAngelscriptBoundProperty::GetTargetEngine() const
{
	check(TargetEngine != nullptr);
	return *TargetEngine;
}

FAngelscriptBoundProperty& FAngelscriptBoundProperty::PureConstant(const asQWORD ConstantValue)
{
	if (RecordingStore)
	{
		if (const auto* Existing = RecordingStore->GetMember(RecordingHandle))
		{
			auto Member = *Existing;
			if (Member.Kind != EAngelscriptTypeBindMemberKind::GlobalProperty) return *this;
			Member.bPureConstant = true;
			Member.ConstantValue = ConstantValue;
			Member.GlobalStorageLifetime = EAngelscriptBindHostLifetime::CopiedRecord;
			FString Diagnostic;
			if (!RecordingStore->UpdateMember(RecordingHandle, Member, Diagnostic)) RecordingHandle = {};
		}
		return *this;
	}
	if (IsValid() && bGlobal)
	{
		asCGlobalProperty* Property = TargetEngine->Engine->globalProperties[PropertyId];
		Property->isPureConstant = true;
		Property->storage = ConstantValue;
	}
	return *this;
}

TMap<FString, TArray<TObjectPtr<UClass>>>& FAngelscriptBinds::GetRuntimeClassDB()
{
	return GetBindState().RuntimeClassDB;
}

#if WITH_EDITOR
TMap<FString, TArray<TObjectPtr<UClass>>>& FAngelscriptBinds::GetEditorClassDB()
{
	return GetBindState().EditorClassDB;
}
#endif

TMap<UClass*, TMap<FString, FAngelscriptFunctionBinding>>& FAngelscriptBinds::GetClassFunctionBindings()
{
	return GetBindState().ClassFunctionBindings;
}

TMap<UClass*, TSet<FString>>& FAngelscriptBinds::GetSkipBinds()
{
	return GetBindState().SkipBinds;
}

TSet<TTuple<FName, FName>>& FAngelscriptBinds::GetSkipBindNames()
{
	return GetBindState().SkipBindNames;
}

TSet<FName>& FAngelscriptBinds::GetSkipBindClasses()
{
	return GetBindState().SkipBindClasses;
}

const FAngelscriptRegisteredFunctionProvenance*
FAngelscriptBinds::FindFunctionProvenance(const int32 FunctionId)
{
	return GetBindState().FunctionProvenance.Find(FunctionId);
}

const FAngelscriptRegisteredFunctionProvenance*
FAngelscriptBinds::FindFunctionProvenance(
	const asIScriptFunction& Function)
{
	if (const FAngelscriptRegisteredFunctionProvenance* ByPointer =
		GetBindState().FunctionProvenanceByPointer.Find(&Function))
	{
		return ByPointer;
	}
	return FindFunctionProvenance(Function.GetId());
}

bool FAngelscriptBinds::ShouldSkipBlueprintCallableFunction(const UFunction* Function)
{
	if (Function == nullptr)
	{
		return true;
	}

	if (!Function->HasAnyFunctionFlags(FUNC_Native))
	{
		return true;
	}

#if WITH_EDITORONLY_DATA
	// UFunction metadata is editor-only and stripped from cooked builds. In packaged
	// builds these gates fall through; the bind database baked at cook time already
	// reflects the editor-time decisions.
	static const FName NAME_Function_NotInAngelscript(TEXT("NotInAngelscript"));
	static const FName NAME_Function_BlueprintInternalUseOnly(TEXT("BlueprintInternalUseOnly"));
	static const FName NAME_Function_UsableInAngelscript(TEXT("UsableInAngelscript"));

	if (Function->HasMetaData(NAME_Function_NotInAngelscript))
	{
		return true;
	}

	if (Function->HasMetaData(NAME_Function_BlueprintInternalUseOnly) && !Function->HasMetaData(NAME_Function_UsableInAngelscript))
	{
		return true;
	}
#endif

	if (const UClass* OwningClass = Function->GetOuterUClass())
	{
		if (OwningClass == UActorComponent::StaticClass() && Function->GetFName() == FName(TEXT("GetOwner")))
		{
			return true;
		}
	}

	return false;
}

FAngelscriptBinds FAngelscriptBinds::ReferenceClassForTarget(FBindString Name, UClass* UnrealClass) const
{
	if (HostDefinitions)
	{
		FAngelscriptBinds Binds(*HostDefinitions, *HostBindState);
		Binds.HostNamespace = HostNamespace;
		Binds.HostIdentitySerial = HostIdentitySerial;
		Binds.ClassName = Name;
		if (UnrealClass == nullptr)
		{
			Binds.RecordRegistrationFailure(TEXT("host reference class"), Name, asINVALID_ARG);
			return Binds;
		}
		FString TypeName = Name.ToFString();
		int32 TemplatePos = INDEX_NONE;
		if (TypeName.FindChar(TEXT('<'), TemplatePos))
		{
			TypeName.LeftInline(TemplatePos);
		}
		const auto TypeNameAnsi = StringCast<ANSICHAR>(*TypeName);
		if (asCTypeInfo* Existing = HostDefinitions->FindType(TypeNameAnsi.Get()))
		{
			Binds.ClassName = TypeName;
			Binds.ScriptType = Existing;
			return Binds;
		}
		constexpr asQWORD ReferenceFlags = asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE;
		const int32 Size = UnrealClass->GetStructureSize();
		asUINT Alignment = static_cast<asUINT>(FMath::Max(UnrealClass->GetMinAlignment(), 1));
		while (Alignment > 1 && (static_cast<asUINT>(Size) % Alignment) != 0)
		{
			Alignment >>= 1;
		}
		asSStableKey TypeKey = Binds.MakeHostBindKey("type", TypeNameAnsi.Get());
		if (TSharedPtr<const AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> HostContext =
			HostDefinitions->GetIdentityContext())
		{
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
			Descriptor.StableScope = TEXT("host-process");
			Descriptor.Name = TypeName;
			Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
			AS_NAMESPACE_QUALIFIER asCTypeContext* MutableContext =
				const_cast<AS_NAMESPACE_QUALIFIER asCTypeContext*>(HostContext.Get());
			if (MutableContext->GetIdentityRegistry().InternNominalType(Descriptor, TypeKey)
				!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
			{
				Binds.RecordRegistrationFailure(TEXT("host reference class identity"), Name, asINVALID_ARG);
				return Binds;
			}
		}
		asCObjectType* Type = nullptr;
		const char* Ns = TCHAR_TO_UTF8(*HostNamespace);
		if (HostDefinitions->CreateObjectType(TypeKey, TypeNameAnsi.Get(), Ns, ReferenceFlags, Type)
			!= asEDefinitionResult::Succeeded)
		{
			Binds.RecordRegistrationFailure(TEXT("host reference class"), Name, asINVALID_ARG);
			return Binds;
		}
		if (HostDefinitions->SetNativeLayout(*Type, static_cast<asUINT>(Size), Alignment)
			!= asEDefinitionResult::Succeeded)
		{
			Binds.RecordRegistrationFailure(TEXT("host reference class"), Name, asINVALID_ARG);
			return Binds;
		}
		Type->size = Size;
		Type->alignment = static_cast<int>(Alignment);
		Binds.ClassName = TypeName;
		Binds.ScriptType = Type;
		return Binds;
	}
	if (IsRecording())
	{
		FAngelscriptTypeBindDeclaration Declaration;
		Declaration.Name = Name.ToFString();
		Declaration.Kind = EAngelscriptTypeBindKind::ObjectHandle;
		Declaration.UnrealType = UnrealClass;
		Declaration.ObjectFlags = asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE;
		if (UnrealClass)
		{
			Declaration.NativeSize = UnrealClass->GetStructureSize();
			Declaration.NativeAlignment = UnrealClass->GetMinAlignment();
			if (UClass* Super = UnrealClass->GetSuperClass())
			{
				for (const auto& Type : RecordingStore->GetTypes())
				{
					if (Type.Declaration.UnrealType == Super)
					{
						Declaration.BaseType = Type.Declaration.Namespace.IsEmpty() ? Type.Declaration.Name
							: Type.Declaration.Namespace + TEXT("::") + Type.Declaration.Name;
						break;
					}
				}
			}
		}
		else RecordingStore->ReportFailure(TEXT("A recorded reference class requires a UClass."));
		return RecordClass(MoveTemp(Declaration));
	}
	if (HasRegistrationFailure())
	{
		return FAngelscriptBinds(GetTargetEngine(), Name);
	}

	if (UnrealClass == nullptr)
	{
		FAngelscriptBinds Binds(GetTargetEngine(), Name);
		Binds.RecordRegistrationFailure(TEXT("reference class"), Name, asINVALID_ARG);
		return Binds;
	}

	constexpr asQWORD ReferenceFlags = asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE;
	auto RegisterCanonicalAncestry = [this, Name, UnrealClass]()
	{
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(GetTargetEngine().GetScriptEngine());
		if (ScriptEngine == nullptr)
		{
			return;
		}

		// Registered AngelScript reference types intentionally do not use
		// asCObjectType::derivedFrom for Unreal inheritance. Publish the nearest
		// visible native superclass by stable spelling for Canonical Sema without
		// changing the maintained LEGACY Runtime model or retaining a TypeId/UClass.
		for (UClass* SuperClass = UnrealClass->GetSuperClass();
			SuperClass != nullptr;
			SuperClass = SuperClass->GetSuperClass())
		{
			const FString SuperName =
				FAngelscriptType::GetBoundClassName(SuperClass);
			if (ScriptEngine->GetTypeInfoByName(TCHAR_TO_ANSI(*SuperName)) != nullptr)
			{
				ScriptEngine->RegisterCanonicalTypeBaseRelation(
					Name.ToCString(), TCHAR_TO_ANSI(*SuperName));
				break;
			}
		}

		for (const FImplementedInterface& ImplementedInterface : UnrealClass->Interfaces)
		{
			if (ImplementedInterface.Class == nullptr)
			{
				continue;
			}
			const FString InterfaceName =
				FAngelscriptType::GetBoundClassName(ImplementedInterface.Class);
			if (ScriptEngine->GetTypeInfoByName(TCHAR_TO_ANSI(*InterfaceName)) != nullptr)
			{
				ScriptEngine->RegisterCanonicalTypeBaseRelation(
					Name.ToCString(), TCHAR_TO_ANSI(*InterfaceName));
			}
		}
	};
	asITypeInfo* ExistingType = GetTargetScriptEngine().GetTypeInfoByName(Name.ToCString());
	if (ExistingType != nullptr)
	{
		FAngelscriptBinds Binds(GetTargetEngine(), Name);
		Binds.ScriptType = ExistingType;

		constexpr asQWORD ProviderSemanticFlags = asOBJ_EDITOR_ONLY | asOBJ_DISALLOW_INSTANTIATION;
		const asQWORD ExistingRegistrationFlags = ExistingType->GetFlags() & ~ProviderSemanticFlags;
		if (ExistingRegistrationFlags != ReferenceFlags
			|| ExistingType->GetSize() != UnrealClass->GetStructureSize())
		{
			Binds.RecordRegistrationFailure(
				TEXT("reference class compatibility"),
				Name,
				asALREADY_REGISTERED);
			Binds.ScriptType = nullptr;
			return Binds;
		}

		if (ExistingType->alignment != UnrealClass->GetMinAlignment())
		{
			Binds.RecordRegistrationFailure(
				TEXT("reference class alignment compatibility"),
				Name,
				asALREADY_REGISTERED);
			Binds.ScriptType = nullptr;
			return Binds;
		}

		void* AssociatedClass = ExistingType->GetUserData();
		if (AssociatedClass != nullptr && AssociatedClass != UnrealClass)
		{
			Binds.RecordRegistrationFailure(
				TEXT("reference class association compatibility"),
				Name,
				asALREADY_REGISTERED);
			Binds.ScriptType = nullptr;
			return Binds;
		}

		ExistingType->SetUserData(UnrealClass);
		RegisterCanonicalAncestry();
		return Binds;
	}

	FAngelscriptBinds Binds(
		GetTargetEngine(),
		Name,
		ReferenceFlags,
		UnrealClass->GetStructureSize());
	if (Binds.ScriptType == nullptr)
	{
		return Binds;
	}

	Binds.ScriptType->alignment = UnrealClass->GetMinAlignment();
	Binds.ScriptType->SetUserData(UnrealClass);
	RegisterCanonicalAncestry();
	return Binds;
}

FAngelscriptBinds FAngelscriptBinds::ExistingClassForTarget(FBindString Name) const
{
	if (IsRecording())
	{
		FAngelscriptBinds Binds(*RecordingStore, bDeclarationOnly);
		FString TypeName = Name.ToFString();
		FString Namespace = RecordingStore->GetNamespace();
		int32 TemplatePosition;
		if (TypeName.FindChar(TEXT('<'), TemplatePosition)) TypeName.LeftInline(TemplatePosition);
		const int32 Separator = TypeName.Find(TEXT("::"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (Separator != INDEX_NONE)
		{
			Namespace = TypeName.Left(Separator);
			TypeName.RightChopInline(Separator + 2);
		}
		Binds.ClassName = TypeName;
		Binds.RecordingType = RecordingStore->FindType(TypeName, Namespace);
		if (!RecordingStore->GetType(Binds.RecordingType))
			RecordingStore->ReportFailure(FString::Printf(TEXT("Recorded type '%s::%s' has not been declared."), *Namespace, *TypeName));
		return Binds;
	}
	if (HostDefinitions)
	{
		FAngelscriptBinds Binds(*HostDefinitions, *HostBindState);
		Binds.HostNamespace = HostNamespace;
		Binds.HostIdentitySerial = HostIdentitySerial;
		FString TypeName = Name.ToFString();
		int32 TemplatePosition;
		if (TypeName.FindChar(TEXT('<'), TemplatePosition))
		{
			TypeName.LeftInline(TemplatePosition);
		}
		Binds.ClassName = TypeName;
		Binds.ScriptType = HostDefinitions->FindType(Binds.ClassName.ToCString());
		if (!Binds.ScriptType && Binds.IsHostExplicitAuthoringEnabled())
		{
			Binds.RecordRegistrationFailure(TEXT("host existing type"), Name, asINVALID_TYPE);
		}
		return Binds;
	}
	return FAngelscriptBinds(GetTargetEngine(), Name);
}

namespace
{
	void SkipHostSpaces(const char*& Cursor)
	{
		while (*Cursor == ' ' || *Cursor == '\t') ++Cursor;
	}

	asCString TakeHostToken(const char*& Cursor)
	{
		SkipHostSpaces(Cursor);
		const char* Start = Cursor;
		while (*Cursor && *Cursor != ' ' && *Cursor != '\t' && *Cursor != '(' && *Cursor != ')'
			&& *Cursor != ',' && *Cursor != '<' && *Cursor != '>' && *Cursor != '&')
		{
			++Cursor;
		}
		return asCString(Start, static_cast<int>(Cursor - Start));
	}

	bool ParseHostDataType(asCDefinitions& Graph, const char*& Cursor, asCDataType& Out)
	{
		SkipHostSpaces(Cursor);
		if (FCStringAnsi::Strncmp(Cursor, "const", 5) == 0 && (Cursor[5] == ' ' || Cursor[5] == '\t'))
		{
			Cursor += 5;
			SkipHostSpaces(Cursor);
		}
		if (FCStringAnsi::Strncmp(Cursor, "TArray<", 7) == 0)
		{
			Cursor += 7;
			asCDataType Inner;
			if (!ParseHostDataType(Graph, Cursor, Inner)) return false;
			SkipHostSpaces(Cursor);
			if (*Cursor != '>') return false;
			++Cursor;
			asCObjectType* Template = CastToObjectType(Graph.FindType("TArray"));
			if (!Template) return false;
			asCObjectType* Instance = Graph.FindTemplateInstance(Template, MakeArrayView(&Inner, 1));
			if (!Instance) return false;
			Out = asCDataType::CreateType(Instance, false);
			return true;
		}
		const asCString Token = TakeHostToken(Cursor);
		if (Token == "int" || Token == "int32")
		{
			Out = Graph.Primitive(ttInt);
		}
		else if (Token == "int8")
		{
			Out = Graph.Primitive(ttInt8);
		}
		else if (Token == "int16")
		{
			Out = Graph.Primitive(ttInt16);
		}
		else if (Token == "int64")
		{
			Out = Graph.Primitive(ttInt64);
		}
		else if (Token == "uint" || Token == "uint32")
		{
			Out = Graph.Primitive(ttUInt);
		}
		else if (Token == "uint8")
		{
			Out = Graph.Primitive(ttUInt8);
		}
		else if (Token == "uint16")
		{
			Out = Graph.Primitive(ttUInt16);
		}
		else if (Token == "uint64")
		{
			Out = Graph.Primitive(ttUInt64);
		}
		else if (Token == "bool")
		{
			Out = Graph.Primitive(ttBool);
		}
		else if (Token == "float" || Token == "float32")
		{
			Out = Graph.Primitive(ttFloat);
		}
		else if (Token == "float64" || Token == "double")
		{
			Out = Graph.Primitive(ttDouble);
		}
		else if (Token == "void")
		{
			Out = Graph.Primitive(ttVoid);
		}
		else
		{
			asCTypeInfo* Type = Graph.FindType(Token.AddressOf());
			if (!Type) return false;
			Out = asCDataType::CreateType(Type, false);
		}
		SkipHostSpaces(Cursor);
		if (*Cursor == '&')
		{
			++Cursor;
			Out.MakeReference(true);
			SkipHostSpaces(Cursor);
			if (FCStringAnsi::Strncmp(Cursor, "in", 2) == 0) Cursor += 2;
			else if (FCStringAnsi::Strncmp(Cursor, "out", 3) == 0) Cursor += 3;
			else if (FCStringAnsi::Strncmp(Cursor, "inout", 5) == 0) Cursor += 5;
		}
		return true;
	}

	bool ParseHostDataTypeDecl(asCDefinitions& Graph, const char* Decl, asCDataType& Out)
	{
		const char* Cursor = Decl;
		return ParseHostDataType(Graph, Cursor, Out);
	}

	bool ParseHostMethodSignature(asCDefinitions& Graph, const char* Decl, asCDataType& ReturnType, asCString& Name,
		TArray<asSDefinitionParameter>& Parameters)
	{
		const char* Cursor = Decl;
		if (!ParseHostDataType(Graph, Cursor, ReturnType)) return false;
		SkipHostSpaces(Cursor);
		Name = TakeHostToken(Cursor);
		if (Name.GetLength() == 0) return false;
		SkipHostSpaces(Cursor);
		if (*Cursor != '(') return false;
		++Cursor;
		SkipHostSpaces(Cursor);
		if (*Cursor == ')') return true;
		while (*Cursor && *Cursor != ')')
		{
			asSDefinitionParameter Parameter;
			if (!ParseHostDataType(Graph, Cursor, Parameter.Type)) return false;
			SkipHostSpaces(Cursor);
			if (*Cursor && *Cursor != ',' && *Cursor != ')' && *Cursor != '=')
			{
				Parameter.Name = TakeHostToken(Cursor);
			}
			SkipHostSpaces(Cursor);
			if (*Cursor == '=')
			{
				++Cursor;
				int32 Depth = 0;
				while (*Cursor)
				{
					if (*Cursor == '(' || *Cursor == '<') ++Depth;
					else if ((*Cursor == ')' || *Cursor == '>') && Depth > 0) --Depth;
					else if ((*Cursor == ',' || *Cursor == ')') && Depth == 0) break;
					++Cursor;
				}
			}
			Parameters.Add(Parameter);
			SkipHostSpaces(Cursor);
			if (*Cursor == ',')
			{
				++Cursor;
				continue;
			}
			break;
		}
		return *Cursor == ')';
	}
}

FAngelscriptBinds FAngelscriptBinds::SpecializeTemplateForTarget(FBindString TemplateName, FBindString ArgumentDecl)
{
	if (!HostDefinitions)
	{
		FAngelscriptBinds Failed(GetTargetEngine(), TemplateName);
		Failed.RecordRegistrationFailure(TEXT("host template specialization"), TemplateName, asNOT_SUPPORTED);
		return Failed;
	}
	FAngelscriptBinds Binds(*HostDefinitions, *HostBindState);
	Binds.HostNamespace = HostNamespace;
	Binds.HostIdentitySerial = HostIdentitySerial;
	Binds.ClassName = TemplateName;
	asCObjectType* Template = CastToObjectType(HostDefinitions->FindType(TemplateName.ToCString()));
	asCDataType Argument;
	if (!Template || !ParseHostDataTypeDecl(*HostDefinitions, ArgumentDecl.ToCString(), Argument))
	{
		RecordRegistrationFailure(TEXT("host template argument"), ArgumentDecl, asINVALID_TYPE);
		Binds.RecordRegistrationFailure(TEXT("host template argument"), ArgumentDecl, asINVALID_TYPE);
		return Binds;
	}
	asCObjectType* Instance = nullptr;
	asSStableKey Key = Binds.MakeHostBindKey("tmpl", TCHAR_TO_UTF8(*(TemplateName.ToFString() + TEXT("<") + ArgumentDecl.ToFString() + TEXT(">"))));
	if (TSharedPtr<const AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> HostContext =
		HostDefinitions->GetIdentityContext())
	{
		AS_NAMESPACE_QUALIFIER asCTypeContext* MutableContext =
			const_cast<AS_NAMESPACE_QUALIFIER asCTypeContext*>(HostContext.Get());
		AS_NAMESPACE_QUALIFIER asSStableKey ArgumentKey;
		if (Argument.GetTokenType() == ttInt)
		{
			MutableContext->InternPrimitiveTypeUse(
				AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, ArgumentKey);
		}
		else if (asCTypeInfo* ArgumentType = Argument.GetTypeInfo())
		{
			if (asCObjectType* ArgumentObject = CastToObjectType(ArgumentType);
				ArgumentObject && ArgumentObject->GetTemplateDefinition())
			{
				ArgumentKey = ArgumentObject->GetStableKey();
			}
			else
			{
				AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor ArgumentUse;
				ArgumentUse.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Nominal;
				ArgumentUse.Declaration = ArgumentType->GetStableKey();
				MutableContext->GetIdentityRegistry().InternTypeUse(ArgumentUse, ArgumentKey);
			}
		}
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Use;
		Use.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Nominal;
		Use.Declaration = Template->GetStableKey();
		if (!ArgumentKey.IsZero())
		{
			Use.Arguments.Add(ArgumentKey);
		}
		AS_NAMESPACE_QUALIFIER asSStableKey InternedKey;
		if (MutableContext->GetIdentityRegistry().InternTypeUse(Use, InternedKey)
			== AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			Key = InternedKey;
		}
	}
	if (HostDefinitions->CreateHostTemplateInstance(*Template, Key, MakeArrayView(&Argument, 1), Instance)
		!= asEDefinitionResult::Succeeded || !Instance)
	{
		RecordRegistrationFailure(TEXT("host template specialization"), ArgumentDecl, asINVALID_DECLARATION);
		Binds.RecordRegistrationFailure(TEXT("host template specialization"), ArgumentDecl, asINVALID_DECLARATION);
		return Binds;
	}
	Binds.ScriptType = Instance;
	HostIdentitySerial = Binds.HostIdentitySerial;
	return Binds;
}

void FAngelscriptBinds::SetHostBaseTypeForTarget(FBindString BaseName)
{
	if (!HostDefinitions)
	{
		return;
	}
	asCObjectType* Derived = ScriptType
		? CastToObjectType(static_cast<asCTypeInfo*>(ScriptType))
		: CastToObjectType(HostDefinitions->FindType(ClassName.ToCString()));
	asCObjectType* Base = CastToObjectType(HostDefinitions->FindType(BaseName.ToCString()));
	if (!Derived || !Base
		|| HostDefinitions->SetBaseType(*Derived, *Base) != asEDefinitionResult::Succeeded)
	{
		RecordRegistrationFailure(TEXT("host base type"), BaseName, asINVALID_TYPE);
	}
}

bool FAngelscriptBinds::IsHostExplicitAuthoringEnabled() const
{
	return true;
}

FAngelscriptBinds::FAngelscriptBinds(FAngelscriptEngine& InTargetEngine)
	: TargetEngine(&InTargetEngine)
{
}

FAngelscriptBinds::FAngelscriptBinds(asCDefinitions& InHostDefinitions, FAngelscriptBindState& InHostBindState)
	: HostDefinitions(&InHostDefinitions)
	, HostBindState(&InHostBindState)
{
}

asSStableKey FAngelscriptBinds::MakeHostBindKey(const char* Kind, const char* Name) const
{
	uint8 Bytes[32] = {};
	uint32& SerialCounter = HostBindState
		? HostBindState->HostIdentitySerial
		: const_cast<FAngelscriptBinds*>(this)->HostIdentitySerial;
	const uint32 Serial = SerialCounter++;
	const auto Ns = StringCast<ANSICHAR>(*HostNamespace);
	const uint32 Words[8] = {
		FCrc::StrCrc32(Kind ? Kind : ""),
		FCrc::StrCrc32(Ns.Get()),
		FCrc::StrCrc32(Name ? Name : ""),
		Serial,
		0x484F5354,
		0,
		0,
		0
	};
	FMemory::Memcpy(Bytes, Words, sizeof(Words));
	asSStableKey Key;
	check(asSStableKey::TryCreate(MakeArrayView(Bytes), Key));
	return Key;
}

FAngelscriptEngine& FAngelscriptBinds::GetTargetEngine() const
{
	if (TargetEngine == nullptr && HostBindState)
	{
		UE_LOG(
			Angelscript,
			Error,
			TEXT("HostProcess bind '%s' requested a live engine."),
			*HostBindState->ActiveBindProvider.ToString());
	}
	check(TargetEngine != nullptr);
	return *TargetEngine;
}

FAngelscriptBindState& FAngelscriptBinds::GetTargetBindState() const
{
	if (HostBindState) return *HostBindState;
	FAngelscriptBindState* BindState = GetTargetEngine().GetBindState();
	check(BindState != nullptr);
	return *BindState;
}

asIScriptEngine& FAngelscriptBinds::GetTargetScriptEngine() const
{
	asIScriptEngine* ScriptEngine = GetTargetEngine().GetScriptEngine();
	check(ScriptEngine != nullptr);
	return *ScriptEngine;
}

FAngelscriptTypeDatabase& FAngelscriptBinds::GetTargetTypeDatabase() const
{
	FAngelscriptTypeDatabase* TypeDatabase = GetTargetEngine().GetTypeDatabase();
	check(TypeDatabase != nullptr);
	return *TypeDatabase;
}

FAngelscriptBindDatabase& FAngelscriptBinds::GetTargetBindDatabase() const
{
	FAngelscriptBindDatabase* BindDatabase = GetTargetEngine().GetBindDatabase();
	check(BindDatabase != nullptr);
	return *BindDatabase;
}

TArray<FToStringType>& FAngelscriptBinds::GetTargetToStringList() const
{
	TArray<FToStringType>* ToStringList = GetTargetEngine().GetToStringList();
	check(ToStringList != nullptr);
	return *ToStringList;
}

void FAngelscriptBinds::AddToStringEffect(const FString& TypeName,
	void (*ToString)(void*, FString&), const bool bImplicitConversion, const bool bIsHandleType) const
{
	if (IsHostTarget())
	{
		return;
	}
	if (IsRecording())
	{
		FAngelscriptBindEffect Effect;
		Effect.Kind = EAngelscriptBindEffectKind::ToString; Effect.Name = TypeName;
		Effect.ToString = ToString; Effect.bImplicitConversion = bImplicitConversion;
		Effect.bHandleType = bIsHandleType; Effect.Source = RecordingStore->GetSource();
		FString Diagnostic; RecordingStore->RecordEffect(MoveTemp(Effect), Diagnostic);
		return;
	}
	GetTargetToStringList().Add({TypeName, nullptr, ToString, bImplicitConversion, bIsHandleType});
}

void FAngelscriptBinds::AddSkipFunctionEffect(const FName InClassName, const FName FunctionName) const
{
	if (IsRecording())
	{
		FAngelscriptBindEffect Effect; Effect.Kind = EAngelscriptBindEffectKind::SkipFunction;
		Effect.Name = InClassName.ToString(); Effect.SecondaryName = FunctionName.ToString(); Effect.Source = RecordingStore->GetSource();
		FString Diagnostic; RecordingStore->RecordEffect(MoveTemp(Effect), Diagnostic); return;
	}
	GetTargetBindState().SkipBindNames.Add(MakeTuple(InClassName, FunctionName));
}

void FAngelscriptBinds::AddSkipClassEffect(const FName InClassName) const
{
	if (IsRecording())
	{
		FAngelscriptBindEffect Effect; Effect.Kind = EAngelscriptBindEffectKind::SkipClass;
		Effect.Name = InClassName.ToString(); Effect.Source = RecordingStore->GetSource();
		FString Diagnostic; RecordingStore->RecordEffect(MoveTemp(Effect), Diagnostic); return;
	}
	GetTargetBindState().SkipBindClasses.Add(InClassName);
}

void FAngelscriptBinds::AddDeprecationEffect(const FString& FunctionPath, const FString& Message) const
{
	if (IsRecording())
	{
		FAngelscriptBindEffect Effect; Effect.Kind = EAngelscriptBindEffectKind::DeprecateFunction;
		Effect.Name = FunctionPath; Effect.SecondaryName = Message; Effect.Source = RecordingStore->GetSource();
		FString Diagnostic; RecordingStore->RecordEffect(MoveTemp(Effect), Diagnostic); return;
	}
#if WITH_EDITOR
	if (UFunction* Function = FindObject<UFunction>(nullptr, *FunctionPath))
	{
		Function->SetMetaData(TEXT("DeprecatedFunction"), TEXT(""));
		Function->SetMetaData(TEXT("DeprecationMessage"), *Message);
	}
#endif
}

void FAngelscriptBinds::AddDirectBindArchitectureProbeEffect() const
{
	if (IsRecording())
	{
		FAngelscriptBindEffect Effect; Effect.Kind = EAngelscriptBindEffectKind::DirectBindArchitectureProbe;
		Effect.Source = RecordingStore->GetSource(); FString Diagnostic;
		RecordingStore->RecordEffect(MoveTemp(Effect), Diagnostic); return;
	}
#if WITH_DEV_AUTOMATION_TESTS
	++GetTargetBindState().DirectCallbackExecutionCountForTesting;
#endif
}

FBlueprintEventSignatureRegistry& FAngelscriptBinds::GetTargetBlueprintEventSignatureRegistry() const
{
	FBlueprintEventSignatureRegistry* Registry = GetTargetEngine().GetBlueprintEventSignatureRegistry();
	check(Registry != nullptr);
	return *Registry;
}

void FAngelscriptBinds::RegisterTypeForTarget(TSharedRef<FAngelscriptType> Type) const
{
	if (IsHostTarget())
	{
		return;
	}
	if (IsRecording())
	{
		const FString Name = Type->GetAngelscriptTypeName();
		FAngelscriptTypeBindHandle Handle = RecordingStore->FindType(Name);
		if (const FAngelscriptTypeBindInfo* Existing = RecordingStore->GetType(Handle))
		{
			FAngelscriptTypeBindDeclaration Declaration = Existing->Declaration;
			Declaration.AdapterName = Name + TEXT("Type");
			if (Declaration.NativeOwner.IsEmpty()) Declaration.NativeOwner = Name;
			FString Diagnostic; RecordingStore->UpdateTypeDeclaration(Handle, Declaration, Diagnostic);
		}
		return;
	}
	FAngelscriptType::Register(GetTargetTypeDatabase(), Type);
}

void FAngelscriptBinds::ConfigureTemplateContainerForTarget(EAngelscriptTemplateContainerKind Kind) const
{
	if (!IsRecording() || RecordingType.Store != RecordingStore) return;
	if (const FAngelscriptTypeBindInfo* Existing = RecordingStore->GetType(RecordingType))
	{
		FAngelscriptTypeBindDeclaration Declaration = Existing->Declaration;
		Declaration.TemplateContainer = Kind;
		FString Diagnostic;
		RecordingStore->UpdateTypeDeclaration(RecordingType, Declaration, Diagnostic);
	}
}

void FAngelscriptBinds::RegisterTypeFinderForTarget(FAngelscriptType::FTypeFinder Finder) const
{
	// Property finders belong to a mutable legacy TypeDatabase. Detached snapshots
	// retain the adapter identity recorded by RegisterTypeForTarget instead.
	if (IsRecording() || IsHostTarget()) return;
	FAngelscriptType::RegisterTypeFinder(GetTargetTypeDatabase(), MoveTemp(Finder));
}

void FAngelscriptBinds::RegisterFunctionBindingForTarget(
	UClass* Class,
	const FString& Name,
	const FAngelscriptFunctionBinding& Binding) const
{
	// Generated UHT batches populate a mutable lookup for the legacy reflection
	// pass. Detached capture imports the same callable transport from the native
	// module function-map provider, which owns stable provenance and lifetimes.
	if (IsRecording()) return;
	if (Class == nullptr)
	{
		return;
	}

	TMap<FString, FAngelscriptFunctionBinding>& FunctionMap = GetTargetBindState().ClassFunctionBindings.FindOrAdd(Class);
	if (FAngelscriptFunctionBinding* ExistingBinding = FunctionMap.Find(Name))
	{
		if (!ExistingBinding->FunctionPointer.IsBound()
			&& !ExistingBinding->bReflectiveFallbackBound
			&& Binding.FunctionPointer.IsBound())
		{
			*ExistingBinding = Binding;
		}
		return;
	}

	FunctionMap.Add(Name, Binding);
}

void FAngelscriptBinds::RegisterGeneratedFunctionBindingForTarget(
	UClass* Class,
	const FString& Name,
	FAngelscriptFunctionBinding Binding) const
{
	Binding.Origin = EAngelscriptFunctionBindingOrigin::Generated;
	RegisterFunctionBindingForTarget(Class, Name, Binding);
}

bool FAngelscriptBinds::HasRegistrationFailure() const
{
	if (IsRecording()) return RecordingStore->HasFailed();
	return GetTargetBindState().bDirectBindFailed;
}

const FString& FAngelscriptBinds::GetRegistrationFailureDiagnostic() const
{
	if (IsRecording()) return RecordingStore->GetFailureDiagnostic();
	return GetTargetBindState().DirectBindFailureDiagnostic;
}

void FAngelscriptBinds::RecordRegistrationFailure(const TCHAR* Operation, const FBindString Declaration, const int32 Result)
{
	FAngelscriptBindState& BindState = GetTargetBindState();
	if (BindState.bDirectBindFailed)
	{
		return;
	}

	BindState.bDirectBindFailed = true;
	BindState.DirectBindFailureDiagnostic = FString::Printf(
		TEXT("Direct AngelScript %s registration failed with result %d (owner='%s', bind='%s', phase='%s', source='%s:%d', declaration='%s')."),
		Operation,
		Result,
		*BindState.ActiveBindOwnerModule.ToString(),
		*BindState.ActiveBindProvider.ToString(),
		UE::Angelscript::Private::GetBindPhaseName(BindState.ActiveBindPhase),
		BindState.ActiveBindSourceFile != nullptr ? ANSI_TO_TCHAR(BindState.ActiveBindSourceFile) : TEXT("<unknown>"),
		BindState.ActiveBindSourceLine,
		*Declaration.ToFString());
}

FAngelscriptEngine& FAngelscriptBinds::ResolveTargetEngine() const
{
	check(TargetEngine != nullptr);
	return *TargetEngine;
}

FAngelscriptBinds FAngelscriptBinds::RecordClass(FAngelscriptTypeBindDeclaration Declaration) const
{
	FAngelscriptBinds Binds(*RecordingStore, bDeclarationOnly);
	Declaration.Namespace = RecordingStore->GetNamespace();
	Declaration.Source = RecordingStore->GetSource();
	Binds.ClassName = Declaration.Name;
	FString Diagnostic;
	RecordingStore->RecordType(Declaration, Binds.RecordingType, Diagnostic);
	return Binds;
}

FAngelscriptTypeBindMemberHandle FAngelscriptBinds::RecordMember(
	FAngelscriptTypeBindInfoMember Member, bool bGlobal)
{
	auto Owner = RecordingType;
	FString Diagnostic;
	if (bGlobal)
	{
		FAngelscriptTypeBindDeclaration Scope;
		Scope.Kind = EAngelscriptTypeBindKind::Namespace;
		Scope.Namespace = RecordingStore->GetNamespace();
		Scope.Source = RecordingStore->GetSource();
		if (!RecordingStore->RecordType(Scope, Owner, Diagnostic)) return {};
	}
	Member.Source = RecordingStore->GetSource();
	FAngelscriptTypeBindMemberHandle Handle;
	RecordingStore->RecordMember(Owner, Member, Handle, Diagnostic);
	return Handle;
}

namespace
{
	void CopyRecordedExposure(FAngelscriptTypeBindInfoMember& Member,
		const FAngelscriptType::FBindParams* Params)
	{
		if (!Params) return;
		Member.bCanRead = Params->bCanRead;
		Member.bCanWrite = Params->bCanWrite;
		Member.bCanEdit = Params->bCanEdit;
		Member.bProtected = Params->bProtected;
	}
}

FAngelscriptBoundFunction FAngelscriptBinds::RecordCallable(FBindString Signature,
	asSFuncPtr Function, ASAutoCaller::FunctionCaller Caller, asECallConvTypes CallConv,
	void* UserData, asEBehaviours Behaviour, const FAngelscriptType::FBindParams* Params, bool bGlobal)
{
	if (bDeclarationOnly) return {};
	FAngelscriptTypeBindInfoMember Member;
	Member.Kind = bGlobal ? EAngelscriptTypeBindMemberKind::GlobalFunction : EAngelscriptTypeBindMemberKind::Method;
	if (Behaviour == asBEHAVE_CONSTRUCT) Member.Kind = EAngelscriptTypeBindMemberKind::Constructor;
	else if (Behaviour == asBEHAVE_DESTRUCT) Member.Kind = EAngelscriptTypeBindMemberKind::Destructor;
	else if (Behaviour != asBEHAVE_MAX) Member.Kind = EAngelscriptTypeBindMemberKind::Behaviour;
	Member.Declaration = Signature.ToFString();
	Member.Native.Function = Function;
	Member.Native.Caller = Caller;
	Member.Native.CallConv = CallConv;
	Member.Native.Auxiliary = UserData;
	Member.Native.Behaviour = Behaviour;
	Member.Native.FunctionLifetime = Function.flag == 0
		? EAngelscriptBindHostLifetime::None
		: EAngelscriptBindHostLifetime::BorrowedHost;
	Member.Native.AuxiliaryLifetime = UserData == nullptr
		? EAngelscriptBindHostLifetime::None
		: EAngelscriptBindHostLifetime::BorrowedHost;
	CopyRecordedExposure(Member, Params);
	return FAngelscriptBoundFunction(RecordingStore, RecordMember(MoveTemp(Member), bGlobal));
}

FAngelscriptBoundProperty FAngelscriptBinds::RecordProperty(FBindString Signature,
	SIZE_T Offset, const FAngelscriptType::FBindParams* Params, const void* Address, bool bGlobal)
{
	if (bDeclarationOnly) return {};
	FAngelscriptTypeBindInfoMember Member;
	Member.Kind = bGlobal ? EAngelscriptTypeBindMemberKind::GlobalProperty : EAngelscriptTypeBindMemberKind::Property;
	Member.Declaration = Signature.ToFString();
	Member.PropertyOffset = Offset;
	Member.GlobalAddress = Address;
	Member.GlobalStorageLifetime = Address == nullptr
		? EAngelscriptBindHostLifetime::None
		: EAngelscriptBindHostLifetime::BorrowedHost;
	CopyRecordedExposure(Member, Params);
	return FAngelscriptBoundProperty(RecordingStore, RecordMember(MoveTemp(Member), bGlobal));
}

FAngelscriptBinds FAngelscriptBinds::ValueClassForTarget(FBindString Name, FBindFlags Flags, int32 Size)
{
	if (HostDefinitions)
	{
		FAngelscriptBinds Binds(*HostDefinitions, *HostBindState);
		Binds.HostNamespace = HostNamespace;
		Binds.HostIdentitySerial = HostIdentitySerial;
		Binds.ClassName = Name;
		auto AsFlags = asOBJ_VALUE | asOBJ_APP_CLASS | Flags.ExtraFlags;
		if (Flags.bPOD) AsFlags |= asOBJ_POD;
		if (Flags.bTemplate) AsFlags |= asOBJ_TEMPLATE;
		asCObjectType* Type = nullptr;
		const char* Ns = TCHAR_TO_UTF8(*HostNamespace);
		FString TypeName = Name.ToFString();
		int32 TemplatePos = INDEX_NONE;
		if (TypeName.FindChar(TEXT('<'), TemplatePos)) TypeName.LeftInline(TemplatePos);
		const auto TypeNameAnsi = StringCast<ANSICHAR>(*TypeName);
		if (HostDefinitions->FindType(TypeNameAnsi.Get()))
		{
			Binds.RecordRegistrationFailure(TEXT("host object type"), Name, asALREADY_REGISTERED);
			return Binds;
		}
		asUINT Alignment = Flags.Alignment > 0 ? static_cast<asUINT>(Flags.Alignment) : 8;
		while (Alignment > 1 && (static_cast<asUINT>(Size) % Alignment) != 0) Alignment >>= 1;
		asUINT Arity = 0;
		if (Flags.bTemplate)
		{
			Arity = 1;
			if (!Flags.TemplateType.IsEmpty())
			{
				FString Inner = Flags.TemplateType.ToFString();
				Inner.RemoveFromStart(TEXT("<"));
				Inner.RemoveFromEnd(TEXT(">"));
				TArray<FString> Parts;
				Inner.ParseIntoArray(Parts, TEXT(","), true);
				if (Parts.Num() > 0) Arity = static_cast<asUINT>(Parts.Num());
			}
		}
		asSStableKey TypeKey = Binds.MakeHostBindKey("type", TypeNameAnsi.Get());
		if (TSharedPtr<const AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> HostContext =
			HostDefinitions->GetIdentityContext())
		{
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
			Descriptor.StableScope = TEXT("host-process");
			Descriptor.Name = TypeName;
			Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
			Descriptor.GenericArity = Arity;
			AS_NAMESPACE_QUALIFIER asCTypeContext* MutableContext =
				const_cast<AS_NAMESPACE_QUALIFIER asCTypeContext*>(HostContext.Get());
			if (MutableContext->GetIdentityRegistry().InternNominalType(Descriptor, TypeKey)
				!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
			{
				Binds.RecordRegistrationFailure(TEXT("host object type identity"), Name, asINVALID_ARG);
				return Binds;
			}
		}
		if (HostDefinitions->CreateObjectType(TypeKey,
			TypeNameAnsi.Get(), Ns, AsFlags, Type) != asEDefinitionResult::Succeeded)
		{
			Binds.RecordRegistrationFailure(TEXT("host object type"), Name, asINVALID_ARG);
			return Binds;
		}
		if (Flags.bTemplate)
		{
			if (HostDefinitions->SetTypeDeclarationFacts(*Type, asETypeDeclKind::Struct, Arity) != asEDefinitionResult::Succeeded
				|| HostDefinitions->SetFixedTemplateLayout(*Type, static_cast<asUINT>(Size), Alignment)
					!= asEDefinitionResult::Succeeded)
			{
				Binds.RecordRegistrationFailure(TEXT("host template type"), Name, asINVALID_ARG);
				return Binds;
			}
		}
		else if (HostDefinitions->SetNativeLayout(*Type, static_cast<asUINT>(Size), Alignment)
			!= asEDefinitionResult::Succeeded)
		{
			Binds.RecordRegistrationFailure(TEXT("host object type"), Name, asINVALID_ARG);
			return Binds;
		}
		Type->size = Size;
		Type->alignment = static_cast<int>(Alignment);
		Binds.ClassName = TypeName;
		Binds.ScriptType = Type;
		HostIdentitySerial = Binds.HostIdentitySerial;
		return Binds;
	}
	if (IsRecording())
	{
		FAngelscriptTypeBindDeclaration Declaration;
		Declaration.Name = Name.ToFString();
		Declaration.Kind = Flags.bTemplate ? EAngelscriptTypeBindKind::Template : EAngelscriptTypeBindKind::Value;
		Declaration.NativeSize = Size;
		Declaration.NativeAlignment = Flags.Alignment == -1 ? 0 : Flags.Alignment;
		Declaration.ObjectFlags = asOBJ_VALUE | asOBJ_APP_CLASS | Flags.ExtraFlags;
		Declaration.NativeOwner = Declaration.Name;
		if (Flags.bPOD) Declaration.ObjectFlags |= asOBJ_POD;
		if (Flags.bTemplate)
		{
			Declaration.ObjectFlags |= asOBJ_TEMPLATE;
			Declaration.TemplateDeclaration = Declaration.Name;
			Declaration.TemplateType = Flags.TemplateType.ToFString();
			int32 Position;
			if (Declaration.Name.FindChar(TEXT('<'), Position)) Declaration.Name.LeftInline(Position);
		}
		return RecordClass(MoveTemp(Declaration));
	}
	if (HasRegistrationFailure())
	{
		return FAngelscriptBinds(GetTargetEngine(), Name);
	}

	auto AsFlags = asOBJ_VALUE | asOBJ_APP_CLASS | Flags.ExtraFlags;
	if (Flags.bPOD)
	{
		AsFlags |= asOBJ_POD;
	}
	if (Flags.bTemplate)
	{
		AsFlags |= asOBJ_TEMPLATE;
	}

	FAngelscriptBinds Binds(GetTargetEngine(), Name, AsFlags, Size);
	if (Flags.bTemplate && !Flags.TemplateType.IsEmpty() && Binds.ScriptType != nullptr)
	{
		int32 TemplatePos = Binds.ClassName.ToFString().Find(TEXT("<"));
		Binds.ClassName = Binds.ClassName.ToFString().Left(TemplatePos);
		Binds.ScriptType = GetTargetScriptEngine().GetTypeInfoByName(Binds.ClassName.ToCString());
		Binds.ClassName = Binds.ClassName.ToFString() + Flags.TemplateType.ToFString();
	}

	if (Flags.Alignment != -1 && Binds.ScriptType != nullptr)
	{
		if (Binds.ScriptType->alignment != 8 && Binds.ScriptType->alignment != Flags.Alignment)
		{
			Binds.RecordRegistrationFailure(TEXT("object type alignment"), Name, asINVALID_CONFIGURATION);
		}
		else
		{
			Binds.ScriptType->alignment = Flags.Alignment;
		}
	}
	return Binds;
}

FAngelscriptBinds::FAngelscriptBinds(FAngelscriptEngine& InTargetEngine, FBindString Name, const asQWORD Flags, const int32 Size)
	: TargetEngine(&InTargetEngine)
	, ClassName(Name)
{
	if (HasRegistrationFailure())
	{
		return;
	}

	asIScriptEngine& ScriptEngine = GetTargetScriptEngine();
	const int32 TypeId = ScriptEngine.RegisterObjectType(ClassName.ToCString(), Size, Flags);
	if (TypeId == asALREADY_REGISTERED)
	{
		ScriptType = ScriptEngine.GetTypeInfoByName(ClassName.ToCString());
		if (ScriptType == nullptr || ScriptType->GetSize() != Size || ScriptType->GetFlags() != Flags)
		{
			RecordRegistrationFailure(TEXT("object type compatibility"), ClassName, asALREADY_REGISTERED);
			ScriptType = nullptr;
		}
	}
	else if (TypeId < 0)
	{
		RecordRegistrationFailure(TEXT("object type"), ClassName, TypeId);
	}
	else
	{
		ScriptType = ScriptEngine.GetTypeInfoById(TypeId);
	}

	if (ScriptType == nullptr && TypeId >= 0 && ((Flags & asOBJ_TEMPLATE) == 0))
	{
		RecordRegistrationFailure(TEXT("object type lookup"), ClassName, asINVALID_TYPE);
	}
}

FAngelscriptBinds::FAngelscriptBinds(FAngelscriptEngine& InTargetEngine, FBindString Name)
	: TargetEngine(&InTargetEngine)
	, ClassName(Name)
{
}

FAngelscriptBoundFunction FAngelscriptBinds::GenericMethod(FBindString Signature, void(CDECL *Fun)(asIScriptGeneric*), void* UserData)
{
	if (IsRecording()) return RecordCallable(Signature, asFUNCTION(Fun), ASAutoCaller::FunctionCaller::Make(), asCALL_GENERIC, UserData);
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	auto& Manager = ResolveTargetEngine();
	int FunctionId = Manager.Engine->RegisterObjectMethod(ClassName.ToCString(), Signature.ToCString(), asFUNCTION(Fun), asCALL_GENERIC, nullptr);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("generic method"), Signature);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller)
{
	if (IsRecording()) return RecordCallable(Signature, Ptr, Caller, asCALL_THISCALL, nullptr, Beh);
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	auto& Manager = ResolveTargetEngine();
	int FunctionId = Manager.Engine->RegisterObjectBehaviour(ClassName.ToCString(), Beh, Signature.ToCString(), Ptr, asCALL_THISCALL, *(asFunctionCaller*)&Caller);
	return OnBindForTarget(FunctionId, nullptr, nullptr,
		TEXT("object behaviour"), Signature, Beh);
}

FAngelscriptBoundFunction FAngelscriptBinds::OnBindForTarget(
	const int32 FunctionId,
	void* UserData,
	const FAngelscriptType::FBindParams* BindParams,
	const TCHAR* Operation,
	const FBindString Declaration,
	const asEBehaviours RegisteredBehaviour)
{
	FAngelscriptEngine& Manager = ResolveTargetEngine();
	if (TargetEngine != nullptr && FunctionId < 0)
	{
		RecordRegistrationFailure(Operation, Declaration, FunctionId);
		return FAngelscriptBoundFunction(&Manager, FunctionId);
	}

	FAngelscriptBindState& BindState = GetTargetBindState();
	asCScriptFunction* ScriptFunction = static_cast<asCScriptFunction*>(Manager.Engine->GetFunctionById(FunctionId));
	if (ScriptFunction != nullptr)
	{
		FAngelscriptRegisteredFunctionProvenance& Provenance = BindState.FunctionProvenance.FindOrAdd(FunctionId);
		if (Provenance.Origin == EAngelscriptFunctionBindingOrigin::Unknown)
		{
			Provenance.Provider = BindState.ActiveBindProvider;
			if (BindState.ActiveBindPhase
					== EAngelscriptBindPhase::GeneratedBindings
				|| (!Provenance.Provider.IsNone()
					&& Provenance.Provider.ToString().StartsWith(
						TEXT("UHT.FunctionBinding."))))
			{
				Provenance.Origin = EAngelscriptFunctionBindingOrigin::Generated;
			}
			else if (BindState.ActiveBindPhase
					== EAngelscriptBindPhase::ReflectionBindings
				|| BindState.ActiveBindPhase
					== EAngelscriptBindPhase::PostReflectionBindings
				|| (!Provenance.Provider.IsNone()
					&& Provenance.Provider.ToString().StartsWith(
						TEXT("Bind_BlueprintType"))))
			{
				Provenance.Origin = EAngelscriptFunctionBindingOrigin::Reflective;
			}
			else
			{
				Provenance.Origin = EAngelscriptFunctionBindingOrigin::Manual;
			}
		}
		if (Manager.GetPurpose()
			== EAngelscriptEnginePurpose::StaticJITGeneration)
		{
			if (!BindState.GenerationFunctionProvenance.IsValid())
			{
				BindState.GenerationFunctionProvenance =
					MakeUnique<FAngelscriptGenerationFunctionProvenanceState>();
			}
			FAngelscriptGenerationFunctionProvenance GenerationProvenance;
			GenerationProvenance.Origin = Provenance.Origin;
			GenerationProvenance.OwnerModule =
				BindState.ActiveBindOwnerModule;
			GenerationProvenance.Provider = Provenance.Provider;
			GenerationProvenance.Phase = BindState.ActiveBindPhase;
			GenerationProvenance.RegisteredBehaviour =
				RegisteredBehaviour;
			GenerationProvenance.ProviderSourceFile =
				UE::Angelscript::Private::NormalizeGenerationBindSourcePath(
					BindState.ActiveBindSourceFile);
			GenerationProvenance.ProviderSourceLine =
				BindState.ActiveBindSourceLine;
			if (!BindState.GenerationFunctionProvenance->TryAddFunction(
				FunctionId, MoveTemp(GenerationProvenance)))
			{
				RecordRegistrationFailure(
					TEXT("generation function provenance"),
					Declaration, asALREADY_REGISTERED);
				return FAngelscriptBoundFunction(
					&Manager, asALREADY_REGISTERED);
			}
		}
		BindState.FunctionProvenanceByPointer.Add(ScriptFunction, Provenance);
		if (UserData != nullptr)
		{
			ScriptFunction->SetUserData(UserData, 0);
		}
		if (BindParams != nullptr && BindParams->bProtected)
		{
			ScriptFunction->SetProtected(true);
		}
	}
	return FAngelscriptBoundFunction(&Manager, FunctionId);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindExternBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	if (IsRecording()) return RecordCallable(Signature, Ptr, Caller, asCALL_CDECL_OBJFIRST, UserData, Beh);
	if (HostDefinitions)
	{
		if (!IsHostExplicitAuthoringEnabled()) return FAngelscriptBoundFunction(TargetEngine, 0);
		if (HasRegistrationFailure()) return FAngelscriptBoundFunction(TargetEngine, asERROR);
		asCObjectType* Type = ScriptType
			? CastToObjectType(static_cast<asCTypeInfo*>(ScriptType))
			: CastToObjectType(HostDefinitions->FindType(ClassName.ToCString()));
		if (!Type)
		{
			RecordRegistrationFailure(TEXT("host object behaviour target"), Signature, asINVALID_TYPE);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		if (!Caller.IsBound() && !Ptr.ptr.f.func)
		{
			RecordRegistrationFailure(TEXT("host object behaviour native target"), Signature, asINVALID_ARG);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		asCDataType ReturnType = HostDefinitions->Primitive(ttVoid);
		asCString Name;
		TArray<asSDefinitionParameter> Parameters;
		if (!ParseHostMethodSignature(*HostDefinitions, Signature.ToCString(), ReturnType, Name, Parameters))
		{
			RecordRegistrationFailure(TEXT("host object behaviour"), Signature, asINVALID_DECLARATION);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		asCScriptFunction* Function = nullptr;
		if (HostDefinitions->CreateFunction(MakeHostBindKey("behaviour", Signature.ToCString()),
			Name.AddressOf(), TCHAR_TO_UTF8(*HostNamespace), asFUNC_SYSTEM, Function) != asEDefinitionResult::Succeeded
			|| HostDefinitions->DefineFunction(*Function, ReturnType, Parameters, Type) != asEDefinitionResult::Succeeded
			|| HostDefinitions->BindNativeFunction(*Function, Ptr, asCALL_CDECL_OBJFIRST, *(asFunctionCaller*)&Caller, UserData)
				!= asEDefinitionResult::Succeeded
			|| HostDefinitions->AddBehaviour(*Type, Beh, *Function) != asEDefinitionResult::Succeeded)
		{
			RecordRegistrationFailure(TEXT("host object behaviour"), Signature, asINVALID_DECLARATION);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		return FAngelscriptBoundFunction(TargetEngine, 0);
	}
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	auto& Manager = ResolveTargetEngine();
	int FunctionId = Manager.Engine->RegisterObjectBehaviour(ClassName.ToCString(), Beh, Signature.ToCString(), Ptr, asCALL_CDECL_OBJFIRST, *(asFunctionCaller*)&Caller);
	return OnBindForTarget(FunctionId, UserData, nullptr,
		TEXT("object behaviour"), Signature, Beh);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindStaticBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	if (IsRecording()) return RecordCallable(Signature, Ptr, Caller, asCALL_CDECL, UserData, Beh);
	if (HostDefinitions)
	{
		if (!IsHostExplicitAuthoringEnabled()) return FAngelscriptBoundFunction(TargetEngine, 0);
		RecordRegistrationFailure(TEXT("host static behaviour"), Signature, asNOT_SUPPORTED);
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	auto& Manager = ResolveTargetEngine();
	int FunctionId = Manager.Engine->RegisterObjectBehaviour(ClassName.ToCString(), Beh, Signature.ToCString(), Ptr, asCALL_CDECL, *(asFunctionCaller*)&Caller);
	return OnBindForTarget(FunctionId, UserData, nullptr,
		TEXT("static behaviour"), Signature, Beh);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindMethod(FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	if (IsRecording()) return RecordCallable(Signature, Ptr, Caller, asCALL_THISCALL, UserData);
	if (HostDefinitions)
	{
		if (!IsHostExplicitAuthoringEnabled()) return FAngelscriptBoundFunction(TargetEngine, 0);
		if (HasRegistrationFailure()) return FAngelscriptBoundFunction(TargetEngine, asERROR);
		asCObjectType* Type = ScriptType
			? CastToObjectType(static_cast<asCTypeInfo*>(ScriptType))
			: CastToObjectType(HostDefinitions->FindType(ClassName.ToCString()));
		if (!Type)
		{
			RecordRegistrationFailure(TEXT("host object method target"), Signature, asINVALID_TYPE);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		if (!Caller.IsBound() && Ptr.flag != 1)
		{
			RecordRegistrationFailure(TEXT("host object method native target"), Signature, asINVALID_ARG);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		asCDataType ReturnType = HostDefinitions->Primitive(ttInt);
		asCString Name;
		TArray<asSDefinitionParameter> Parameters;
		if (!ParseHostMethodSignature(*HostDefinitions, Signature.ToCString(), ReturnType, Name, Parameters))
		{
			RecordRegistrationFailure(TEXT("host object method"), Signature, asINVALID_DECLARATION);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		asCScriptFunction* Function = nullptr;
		if (HostDefinitions->CreateFunction(MakeHostBindKey("method", Name.AddressOf()),
			Name.AddressOf(), TCHAR_TO_UTF8(*HostNamespace), asFUNC_SYSTEM, Function) != asEDefinitionResult::Succeeded
			|| HostDefinitions->DefineFunction(*Function, ReturnType, Parameters, Type) != asEDefinitionResult::Succeeded
			|| HostDefinitions->BindNativeFunction(*Function, Ptr, asCALL_THISCALL, *(asFunctionCaller*)&Caller, UserData)
				!= asEDefinitionResult::Succeeded)
		{
			RecordRegistrationFailure(TEXT("host object method"), Signature, asINVALID_DECLARATION);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		return FAngelscriptBoundFunction(TargetEngine, 0);
	}
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	auto& Manager = ResolveTargetEngine();
	int FunctionId = Manager.Engine->RegisterObjectMethod(ClassName.ToCString(), Signature.ToCString(), Ptr, asCALL_THISCALL, *(asFunctionCaller*)&Caller, nullptr);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("object method"), Signature);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindExternMethod(FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	if (IsRecording()) return RecordCallable(Signature, Ptr, Caller, asCALL_CDECL_OBJFIRST, UserData);
	if (HostDefinitions)
	{
		if (!IsHostExplicitAuthoringEnabled()) return FAngelscriptBoundFunction(TargetEngine, 0);
		if (HasRegistrationFailure()) return FAngelscriptBoundFunction(TargetEngine, asERROR);
		asCObjectType* Type = ScriptType
			? CastToObjectType(static_cast<asCTypeInfo*>(ScriptType))
			: CastToObjectType(HostDefinitions->FindType(ClassName.ToCString()));
		if (!Type)
		{
			RecordRegistrationFailure(TEXT("host object method target"), Signature, asINVALID_TYPE);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		if (!Caller.IsBound() && !Ptr.ptr.f.func)
		{
			RecordRegistrationFailure(TEXT("host object method native target"), Signature, asINVALID_ARG);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		asCDataType ReturnType = HostDefinitions->Primitive(ttVoid);
		asCString Name;
		TArray<asSDefinitionParameter> Parameters;
		if (!ParseHostMethodSignature(*HostDefinitions, Signature.ToCString(), ReturnType, Name, Parameters))
		{
			RecordRegistrationFailure(TEXT("host object method"), Signature, asINVALID_DECLARATION);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		asCScriptFunction* Function = nullptr;
		if (HostDefinitions->CreateFunction(MakeHostBindKey("method", Name.AddressOf()),
			Name.AddressOf(), TCHAR_TO_UTF8(*HostNamespace), asFUNC_SYSTEM, Function) != asEDefinitionResult::Succeeded
			|| HostDefinitions->DefineFunction(*Function, ReturnType, Parameters, Type) != asEDefinitionResult::Succeeded
			|| HostDefinitions->BindNativeFunction(*Function, Ptr, asCALL_CDECL_OBJFIRST, *(asFunctionCaller*)&Caller, UserData)
				!= asEDefinitionResult::Succeeded)
		{
			RecordRegistrationFailure(TEXT("host object method"), Signature, asINVALID_DECLARATION);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		return FAngelscriptBoundFunction(TargetEngine, 0);
	}
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	auto& Manager = ResolveTargetEngine();
	int FunctionId = Manager.Engine->RegisterObjectMethod(ClassName.ToCString(), Signature.ToCString(), Ptr, asCALL_CDECL_OBJFIRST, *(asFunctionCaller*)&Caller, nullptr);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("object method"), Signature);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindExternMethod(FBindString Signature, asSFuncPtr Ptr, const FAngelscriptType::FBindParams& BindParams, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	if (IsRecording()) return RecordCallable(Signature, Ptr, Caller, asCALL_CDECL_OBJFIRST, UserData, asBEHAVE_MAX, &BindParams);
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	auto& Manager = ResolveTargetEngine();
	const asUINT Access = asCScriptFunction::GenerateExposedType(BindParams.bCanEdit, BindParams.bCanRead, BindParams.bCanWrite);
	int FunctionId = Manager.Engine->RegisterObjectMethod(ClassName.ToCString(), Signature.ToCString(), Ptr, asCALL_CDECL_OBJFIRST, *(asFunctionCaller*)&Caller, nullptr, 0, false, Access);
	return OnBindForTarget(FunctionId, UserData, &BindParams, TEXT("object method"), Signature);
}

FAngelscriptBoundProperty FAngelscriptBinds::BindProperty(FBindString Signature, size_t Offset)
{
	if (IsRecording()) return RecordProperty(Signature, Offset);
	if (HostDefinitions)
	{
		if (!IsHostExplicitAuthoringEnabled()) return FAngelscriptBoundProperty(TargetEngine, 0, false);
		if (HasRegistrationFailure()) return FAngelscriptBoundProperty(TargetEngine, asERROR, false);
		asCObjectType* Type = ScriptType
			? CastToObjectType(static_cast<asCTypeInfo*>(ScriptType))
			: CastToObjectType(HostDefinitions->FindType(ClassName.ToCString()));
		if (!Type)
		{
			RecordRegistrationFailure(TEXT("host object property target"), Signature, asINVALID_TYPE);
			return FAngelscriptBoundProperty(TargetEngine, asERROR, false);
		}
		const char* Cursor = Signature.ToCString();
		asCDataType PropertyType;
		if (!ParseHostDataType(*HostDefinitions, Cursor, PropertyType))
		{
			RecordRegistrationFailure(TEXT("host object property"), Signature, asINVALID_DECLARATION);
			return FAngelscriptBoundProperty(TargetEngine, asERROR, false);
		}
		SkipHostSpaces(Cursor);
		const asCString Name = TakeHostToken(Cursor);
		if (Name.GetLength() == 0)
		{
			RecordRegistrationFailure(TEXT("host object property"), Signature, asINVALID_DECLARATION);
			return FAngelscriptBoundProperty(TargetEngine, asERROR, false);
		}
		asSNativePropertyAddress Address;
		Address.Mode = asENativeAddressMode::Direct;
		Address.Offset = static_cast<int>(Offset);
		asCObjectProperty* Property = nullptr;
		if (HostDefinitions->DefineNativeProperty(*Type, Name.AddressOf(), PropertyType, Address, Property)
			!= asEDefinitionResult::Succeeded || !Property)
		{
			RecordRegistrationFailure(TEXT("host object property"), Signature, asINVALID_DECLARATION);
			return FAngelscriptBoundProperty(TargetEngine, asERROR, false);
		}
		return FAngelscriptBoundProperty(TargetEngine, 0, false);
	}
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundProperty(TargetEngine, asERROR, false);
	}
	auto& Manager = ResolveTargetEngine();
	const int32 PropertyId = Manager.Engine->RegisterObjectProperty(ClassName.ToCString(), Signature.ToCString(), Offset);
	if (TargetEngine != nullptr && PropertyId < 0)
	{
		RecordRegistrationFailure(TEXT("object property"), Signature, PropertyId);
	}
	return FAngelscriptBoundProperty(&Manager, PropertyId, false);
}

FAngelscriptBoundProperty FAngelscriptBinds::BindProperty(FBindString Signature, size_t Offset, const FAngelscriptType::FBindParams& BindParams)
{
	if (IsRecording()) return RecordProperty(Signature, Offset, &BindParams);
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundProperty(TargetEngine, asERROR, false);
	}
	auto& Manager = ResolveTargetEngine();
	const asUINT Access = asCObjectProperty::GenerateExposedType(BindParams.bCanEdit, BindParams.bCanRead, BindParams.bCanWrite);
	const int32 PropertyId = Manager.Engine->RegisterObjectProperty(ClassName.ToCString(), Signature.ToCString(), Offset, 0, false, Access, BindParams.bProtected);
	if (TargetEngine != nullptr && PropertyId < 0)
	{
		RecordRegistrationFailure(TEXT("object property"), Signature, PropertyId);
	}
	return FAngelscriptBoundProperty(&Manager, PropertyId, false);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindGlobalFunctionForTarget(FBindString Signature, asSFuncPtr Function, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	if (IsRecording()) return RecordCallable(Signature, Function, Caller, asCALL_CDECL, UserData, asBEHAVE_MAX, nullptr, true);
	if (HostDefinitions)
	{
		if (!IsHostExplicitAuthoringEnabled()) return FAngelscriptBoundFunction(TargetEngine, 0);
		if (HasRegistrationFailure()) return FAngelscriptBoundFunction(TargetEngine, asERROR);
		if (!Caller.IsBound() && !Function.ptr.f.func)
		{
			RecordRegistrationFailure(TEXT("host global function native target"), Signature, asINVALID_ARG);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		asCDataType ReturnType = HostDefinitions->Primitive(ttVoid);
		asCString Name;
		TArray<asSDefinitionParameter> Parameters;
		if (!ParseHostMethodSignature(*HostDefinitions, Signature.ToCString(), ReturnType, Name, Parameters))
		{
			RecordRegistrationFailure(TEXT("host global function"), Signature, asINVALID_DECLARATION);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		asCScriptFunction* Created = nullptr;
		if (HostDefinitions->CreateFunction(MakeHostBindKey("global-fn", Signature.ToCString()),
			Name.AddressOf(), TCHAR_TO_UTF8(*HostNamespace), asFUNC_SYSTEM, Created) != asEDefinitionResult::Succeeded
			|| HostDefinitions->DefineFunction(*Created, ReturnType, Parameters, nullptr) != asEDefinitionResult::Succeeded
			|| HostDefinitions->BindNativeFunction(*Created, Function, asCALL_CDECL, *(asFunctionCaller*)&Caller, UserData)
				!= asEDefinitionResult::Succeeded)
		{
			RecordRegistrationFailure(TEXT("host global function"), Signature, asINVALID_DECLARATION);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		return FAngelscriptBoundFunction(TargetEngine, 0);
	}
	if (HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	const int32 FunctionId = GetTargetScriptEngine().RegisterGlobalFunction(Signature.ToCString(), Function, asCALL_CDECL, *(asFunctionCaller*)&Caller, nullptr);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("global function"), Signature);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindGlobalFunctionDirectForTarget(
	FBindString Signature,
	asSFuncPtr Function,
	asECallConvTypes CallConv,
	ASAutoCaller::FunctionCaller Caller,
	void* UserData)
{
	if (IsRecording()) return RecordCallable(Signature, Function, Caller, CallConv, UserData, asBEHAVE_MAX, nullptr, true);
	if (HostDefinitions)
	{
		if (!IsHostExplicitAuthoringEnabled()) return FAngelscriptBoundFunction(TargetEngine, 0);
		if (!Function.ptr.f.func)
		{
			RecordRegistrationFailure(TEXT("host global function native target"), Signature, asINVALID_ARG);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		if (HasRegistrationFailure()) return FAngelscriptBoundFunction(TargetEngine, asERROR);
		asCDataType ReturnType = HostDefinitions->Primitive(ttVoid);
		asCString Name;
		TArray<asSDefinitionParameter> Parameters;
		if (!ParseHostMethodSignature(*HostDefinitions, Signature.ToCString(), ReturnType, Name, Parameters))
		{
			RecordRegistrationFailure(TEXT("host global function"), Signature, asINVALID_DECLARATION);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		asCScriptFunction* Created = nullptr;
		if (HostDefinitions->CreateFunction(MakeHostBindKey("global-fn", Signature.ToCString()),
			Name.AddressOf(), TCHAR_TO_UTF8(*HostNamespace), asFUNC_SYSTEM, Created) != asEDefinitionResult::Succeeded
			|| HostDefinitions->DefineFunction(*Created, ReturnType, Parameters, nullptr) != asEDefinitionResult::Succeeded
			|| HostDefinitions->BindNativeFunction(*Created, Function, CallConv, *(asFunctionCaller*)&Caller, UserData)
				!= asEDefinitionResult::Succeeded)
		{
			RecordRegistrationFailure(TEXT("host global function"), Signature, asINVALID_DECLARATION);
			return FAngelscriptBoundFunction(TargetEngine, asERROR);
		}
		return FAngelscriptBoundFunction(TargetEngine, 0);
	}
	if (HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}

	const int32 FunctionId = GetTargetScriptEngine().RegisterGlobalFunction(
		Signature.ToCString(),
		Function,
		CallConv,
		*(asFunctionCaller*)&Caller);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("global function"), Signature);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindMethodDirectForTarget(
	FBindString ObjectTypeName,
	FBindString Signature,
	asSFuncPtr Function,
	asECallConvTypes CallConv,
	ASAutoCaller::FunctionCaller Caller,
	void* UserData)
{
	if (IsRecording()) return ExistingClassForTarget(ObjectTypeName).RecordCallable(Signature, Function, Caller, CallConv, UserData);
	if (HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}

	const int32 FunctionId = GetTargetScriptEngine().RegisterObjectMethod(
		ObjectTypeName.ToCString(),
		Signature.ToCString(),
		Function,
		CallConv,
		*(asFunctionCaller*)&Caller);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("object method"), Signature);
}

FAngelscriptBoundProperty FAngelscriptBinds::BindGlobalVariableForTarget(FBindString Signature, const void* Address)
{
	if (IsRecording()) return RecordProperty(Signature, 0, nullptr, Address, true);
	if (HostDefinitions)
	{
		if (!IsHostExplicitAuthoringEnabled()) return FAngelscriptBoundProperty(TargetEngine, 0, true);
		if (!Address)
		{
			RecordRegistrationFailure(TEXT("host global property native target"), Signature, asINVALID_ARG);
			return FAngelscriptBoundProperty(TargetEngine, asERROR, true);
		}
		const char* Cursor = Signature.ToCString();
		while (*Cursor == ' ') ++Cursor;
		const char* Start = Cursor;
		while (*Cursor && *Cursor != ' ') ++Cursor;
		asCString TypeName(Start, static_cast<int>(Cursor - Start));
		while (*Cursor == ' ') ++Cursor;
		Start = Cursor;
		while (*Cursor && *Cursor != ' ') ++Cursor;
		asCString Name(Start, static_cast<int>(Cursor - Start));
		asCDataType Type = HostDefinitions->Primitive(ttInt);
		if (TypeName != "int" && TypeName != "int32")
		{
			RecordRegistrationFailure(TEXT("host global property"), Signature, asINVALID_TYPE);
			return FAngelscriptBoundProperty(TargetEngine, asERROR, true);
		}
		asCGlobalProperty* Property = nullptr;
		if (HostDefinitions->CreateNativeGlobalProperty(MakeHostBindKey("global", Name.AddressOf()),
			MakeHostBindKey("global-use", Name.AddressOf()), Name.AddressOf(), TCHAR_TO_UTF8(*HostNamespace),
			Type, Property) != asEDefinitionResult::Succeeded)
		{
			RecordRegistrationFailure(TEXT("host global property"), Signature, asINVALID_DECLARATION);
			return FAngelscriptBoundProperty(TargetEngine, asERROR, true);
		}
		Property->SetRegisteredAddress(const_cast<void*>(Address));
		return FAngelscriptBoundProperty(TargetEngine, 0, true);
	}
	if (HasRegistrationFailure())
	{
		return FAngelscriptBoundProperty(TargetEngine, asERROR, true);
	}
	const int32 PropertyId = GetTargetScriptEngine().RegisterGlobalProperty(Signature.ToCString(), const_cast<void*>(Address));
	if (PropertyId < 0)
	{
		RecordRegistrationFailure(TEXT("global property"), Signature, PropertyId);
		return FAngelscriptBoundProperty(TargetEngine, PropertyId, true);
	}
	return FAngelscriptBoundProperty(TargetEngine, PropertyId, true);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindGlobalGenericFunctionForTarget(
	FBindString Signature,
	void(CDECL* Function)(asIScriptGeneric*),
	void* UserData)
{
	if (IsRecording()) return RecordCallable(Signature, asFUNCTION(Function), ASAutoCaller::FunctionCaller::Make(), asCALL_GENERIC, UserData, asBEHAVE_MAX, nullptr, true);
	if (HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}

	const int32 FunctionId = GetTargetScriptEngine().RegisterGlobalFunction(
		Signature.ToCString(),
		asFUNCTION(Function),
		asCALL_GENERIC,
		nullptr,
		nullptr);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("global generic function"), Signature);
}

FAngelscriptBinds::FEnumBind::FEnumBind(FAngelscriptBinds& InTargetBinds, FBindString Name)
	: EnumName(Name)
	, TypeId(asERROR)
	, TargetBinds(&InTargetBinds)
{
	if (TargetBinds->IsRecording())
	{
		FAngelscriptTypeBindDeclaration Declaration;
		Declaration.Name = Name.ToFString();
		Declaration.Kind = EAngelscriptTypeBindKind::Enum;
		Declaration.NativeSize = sizeof(int32);
		Declaration.NativeAlignment = alignof(int32);
		RecordingType = TargetBinds->RecordClass(MoveTemp(Declaration)).RecordingType;
		return;
	}
	if (TargetBinds->IsHostTarget())
	{
		asCDefinitions* Graph = TargetBinds->HostDefinitions;
		if (Graph->FindType(Name.ToCString()))
		{
			TypeId = 0;
			return;
		}
		asCEnumType* Enum = nullptr;
		const auto Ns = StringCast<ANSICHAR>(*TargetBinds->HostNamespace);
		if (Graph->CreateEnumType(TargetBinds->MakeHostBindKey("enum", Name.ToCString()),
			Name.ToCString(), Ns.Get(), Enum) != asEDefinitionResult::Succeeded)
		{
			TargetBinds->RecordRegistrationFailure(TEXT("host enum"), Name, asINVALID_ARG);
		}
		TypeId = 0;
		return;
	}
	asIScriptEngine& ScriptEngine = ResolveScriptEngine();
	TypeId = ScriptEngine.RegisterEnum(Name.ToCString());
	if (TypeId == asALREADY_REGISTERED)
	{
		if (asITypeInfo* ExistingType = ScriptEngine.GetTypeInfoByName(Name.ToCString()))
		{
			TypeId = ExistingType->GetTypeId();
		}
	}
	else if (TypeId < 0)
	{
		TargetBinds->RecordRegistrationFailure(TEXT("enum"), Name, TypeId);
	}
}

asIScriptEngine& FAngelscriptBinds::FEnumBind::ResolveScriptEngine() const
{
	check(TargetBinds != nullptr);
	return TargetBinds->GetTargetScriptEngine();
}

asITypeInfo* FAngelscriptBinds::FEnumBind::GetTypeInfo()
{
	if (TargetBinds->IsRecording()) return nullptr;
	if (TargetBinds->IsHostTarget())
	{
		return TargetBinds->HostDefinitions->FindType(EnumName.ToCString());
	}
	return ResolveScriptEngine().GetTypeInfoById(TypeId);
}

void FAngelscriptBinds::FEnumBind::FEnumElement::operator=(int32 Value)
{
	if (Bind->TargetBinds->IsRecording())
	{
		auto& Store = *Bind->TargetBinds->RecordingStore;
		FAngelscriptTypeBindInfoMember Member;
		Member.Kind = EAngelscriptTypeBindMemberKind::EnumValue;
		Member.Declaration = Name.ToFString();
		Member.EnumValue = Value;
		Member.Source = Store.GetSource();
		if (const auto* Type = Store.GetType(Bind->RecordingType))
		{
			for (const auto& Existing : Type->Members)
			{
				if (Existing.Declaration.Equals(Member.Declaration, ESearchCase::CaseSensitive))
				{
					if (Existing.EnumValue != Value)
						Store.ReportFailure(FString::Printf(TEXT("Conflicting recorded enum value '%s'."), *Member.Declaration));
					return;
				}
			}
		}
		FAngelscriptTypeBindMemberHandle Handle;
		FString Diagnostic;
		Store.RecordMember(Bind->RecordingType, Member, Handle, Diagnostic);
		return;
	}
	if (Bind->TargetBinds->IsHostTarget())
	{
		asCEnumType* Enum = CastToEnumType(static_cast<asCTypeInfo*>(Bind->GetTypeInfo()));
		if (!Enum)
		{
			Bind->TargetBinds->RecordRegistrationFailure(TEXT("host enum value target"), Name, asINVALID_TYPE);
			return;
		}
		const asEDefinitionResult Result = Bind->TargetBinds->HostDefinitions->AddEnumValue(
			*Enum, Name.ToCString(), Value);
		if (Result != asEDefinitionResult::Succeeded && Result != asEDefinitionResult::DuplicateName)
		{
			Bind->TargetBinds->RecordRegistrationFailure(TEXT("host enum value"), Name, asINVALID_ARG);
		}
		return;
	}
	asIScriptEngine& ScriptEngine = Bind->ResolveScriptEngine();

#if WITH_DEV_AUTOMATION_TESTS
	const double DedupeStartSeconds = FPlatformTime::Seconds();
	int32 DedupeStrcmpCount = 0;
	bool bSkippedByDedupe = false;
#endif

	if (asITypeInfo* ExistingEnum = Bind->GetTypeInfo())
	{
		for (asUINT Index = 0, Count = ExistingEnum->GetEnumValueCount(); Index < Count; ++Index)
		{
			int ExistingValue = 0;
			const char* ExistingName = ExistingEnum->GetEnumValueByIndex(Index, &ExistingValue);
#if WITH_DEV_AUTOMATION_TESTS
			if (ExistingName != nullptr)
			{
				++DedupeStrcmpCount;
			}
#endif
			if (ExistingName != nullptr && FCStringAnsi::Strcmp(ExistingName, Name.ToCString()) == 0)
			{
#if WITH_DEV_AUTOMATION_TESTS
				bSkippedByDedupe = true;
				const double DedupeElapsedSeconds = FPlatformTime::Seconds() - DedupeStartSeconds;
				FAngelscriptEnumTableBaselineProbe::RecordEnumValueRegister(
					Bind->EnumName.ToCString(),
					DedupeElapsedSeconds,
					DedupeStrcmpCount,
					0.0,
					true);
#endif
				return;
			}
		}
	}

#if WITH_DEV_AUTOMATION_TESTS
	const double DedupeElapsedSeconds = FPlatformTime::Seconds() - DedupeStartSeconds;
	const double RegisterStartSeconds = FPlatformTime::Seconds();
#endif

	auto AnsiEnumName = Bind->EnumName.ToCString();
	const int Result = ScriptEngine.RegisterEnumValue(AnsiEnumName, Name.ToCString(), Value);

#if WITH_DEV_AUTOMATION_TESTS
	const double RegisterElapsedSeconds = FPlatformTime::Seconds() - RegisterStartSeconds;
	FAngelscriptEnumTableBaselineProbe::RecordEnumValueRegister(
		AnsiEnumName,
		DedupeElapsedSeconds,
		DedupeStrcmpCount,
		RegisterElapsedSeconds,
		Result == asALREADY_REGISTERED);
#endif

	if (Result == asALREADY_REGISTERED)
	{
		return;
	}
	if (Result < 0 && Bind->TargetBinds != nullptr)
	{
		Bind->TargetBinds->RecordRegistrationFailure(TEXT("enum value"), Name, Result);
	}
}

FAngelscriptBinds::FNamespace::FNamespace(FAngelscriptEngine& InTargetEngine, FBindString Name)
	: TargetEngine(&InTargetEngine)
{
	asIScriptEngine* ScriptEngine = InTargetEngine.GetScriptEngine();
	check(ScriptEngine != nullptr);
	PrevNamespace.SetDynamic(ScriptEngine->GetDefaultNamespace());
	ScriptEngine->SetDefaultNamespace(Name.ToCString());
}

FAngelscriptBinds::FNamespace::FNamespace(FAngelscriptBinds& InBinds, FBindString Name)
{
	if (InBinds.IsRecording())
	{
		RecordingStore = InBinds.RecordingStore;
		PrevNamespace = RecordingStore->GetNamespace();
		RecordingStore->SetNamespace(Name.ToFString());
		return;
	}
	if (InBinds.IsHostTarget())
	{
		HostDefinitions = InBinds.HostDefinitions;
		HostNamespace = &InBinds.HostNamespace;
		PrevNamespace.SetDynamic(TCHAR_TO_ANSI(*InBinds.HostNamespace));
		InBinds.HostNamespace = Name.ToFString();
		return;
	}
	TargetEngine = &InBinds.GetTargetEngine();
	asIScriptEngine& ScriptEngine = InBinds.GetTargetScriptEngine();
	PrevNamespace.SetDynamic(ScriptEngine.GetDefaultNamespace());
	ScriptEngine.SetDefaultNamespace(Name.ToCString());
}

FAngelscriptBinds::FNamespace::~FNamespace()
{
	if (RecordingStore != nullptr)
	{
		RecordingStore->SetNamespace(PrevNamespace.ToFString());
		return;
	}
	if (HostNamespace)
	{
		*HostNamespace = PrevNamespace.ToFString();
		return;
	}
	check(TargetEngine != nullptr);
	asIScriptEngine* ScriptEngine = TargetEngine->GetScriptEngine();
	check(ScriptEngine != nullptr);
	ScriptEngine->SetDefaultNamespace(PrevNamespace.ToCString());
}

asITypeInfo* FAngelscriptBinds::GetTypeInfo()
{
	if (IsRecording()) return nullptr;
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return nullptr;
	}

	if (ScriptType == nullptr && !ClassName.IsEmpty())
	{
		auto& Manager = ResolveTargetEngine();
		ScriptType = Manager.Engine->GetTypeInfoByName(ClassName.ToCString());
	}
	return ScriptType;
}

bool FAngelscriptBinds::HasMethod(const FString& MethodName)
{
	if (IsRecording())
	{
		const auto* Type = RecordingStore->GetType(RecordingType);
		return Type && Type->Members.ContainsByPredicate([&](const auto& Member)
		{
			if (Member.Kind != EAngelscriptTypeBindMemberKind::Method) return false;
			int32 Parenthesis;
			if (!Member.Declaration.FindChar(TEXT('('), Parenthesis)) return false;
			const FString Prefix = Member.Declaration.Left(Parenthesis).TrimEnd();
			return Prefix.EndsWith(TEXT(" ") + MethodName, ESearchCase::CaseSensitive)
				|| Prefix.EndsWith(TEXT("&") + MethodName, ESearchCase::CaseSensitive);
		});
	}
	auto* Type = GetTypeInfo();
	if (!ensure(Type != nullptr))
		return false;
	return Type->GetMethodByName(TCHAR_TO_ANSI(*MethodName)) != nullptr;
}

bool FAngelscriptBinds::HasGetter(const FString& PropertyName)
{
	if (IsRecording()) return HasMethod(TEXT("Get") + PropertyName);
	TArray<ANSICHAR, TInlineAllocator<64>> FuncName;
	FuncName.SetNumUninitialized(PropertyName.Len() + 4);
	FuncName[0] = 'G';
	FuncName[1] = 'e';
	FuncName[2] = 't';
	for (int32 i = 0, Count = PropertyName.Len(); i < Count; ++i)
		FuncName[i + 3] = (ANSICHAR)PropertyName[i];
	FuncName[FuncName.Num() - 1] = '\0';

	auto* Type = GetTypeInfo();
	if (!ensure(Type != nullptr))
		return false;
	return Type->GetMethodByName(&FuncName[0]) != nullptr;
}

bool FAngelscriptBinds::HasSetter(const FString& PropertyName)
{
	if (IsRecording()) return HasMethod(TEXT("Set") + PropertyName);
	TArray<ANSICHAR, TInlineAllocator<64>> FuncName;
	FuncName.SetNumUninitialized(PropertyName.Len() + 4);
	FuncName[0] = 'S';
	FuncName[1] = 'e';
	FuncName[2] = 't';
	for (int32 i = 0, Count = PropertyName.Len(); i < Count; ++i)
		FuncName[i + 3] = (ANSICHAR)PropertyName[i];
	FuncName[FuncName.Num() - 1] = '\0';

	auto* Type = GetTypeInfo();
	if (!ensure(Type != nullptr))
		return false;
	return Type->GetMethodByName(&FuncName[0]) != nullptr;
}
