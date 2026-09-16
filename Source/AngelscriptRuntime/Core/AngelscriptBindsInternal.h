#pragma once

#include "Core/AngelscriptBinds.h"
#include "as_definitions.h"

struct FAngelscriptHostDeclarationView
{
	FString Namespace;
	FString Name;
	FString Kind;
	int32 IntValue = 0;
};

struct FAngelscriptBindRecord
{
	FName OwnerModule;
	FName BindName;
	EAngelscriptBindPhase Phase = EAngelscriptBindPhase::ExplicitBindings;
	const ANSICHAR* SourceFile = nullptr;
	int32 SourceLine = 0;
	FAngelscriptBindCallback Callback = nullptr;
	bool (*Condition)(const FAngelscriptBindRecordingPolicy&) = nullptr;
	bool bContainsTypeDeclarations = false;
};

namespace UE::Angelscript::Private
{
	class ANGELSCRIPTRUNTIME_API FAngelscriptBindCollection
	{
	public:
		bool Append(FAngelscriptBindRecord Record, FString& OutDiagnostic);
		bool Finalize(FString& OutDiagnostic);
		bool PrepareForEngineInitialization(
			TConstArrayView<FString> ModuleNames,
			TFunctionRef<bool(FName, FString&)> LoadModule,
			FString& OutDiagnostic);
		bool Execute(FAngelscriptBinds& Binds, FString& OutDiagnostic) const;
		bool Execute(
			FAngelscriptBinds& Binds,
			EAngelscriptBindPhase FirstPhase,
			EAngelscriptBindPhase LastPhase,
			FString& OutDiagnostic) const;
		bool ExecuteToHost(FString& OutDiagnostic);
		const asCDefinitions* GetHostDefinitions() const;
		asCDefinitionGraphLease RetainHostGraph() const;
		int32 GetCallbackInvocationCount(FName BindName) const;
		void InspectHostDeclarations(TArray<FAngelscriptHostDeclarationView>& Out) const;
		TConstArrayView<FAngelscriptBindRecord> GetRecords() const
		{
			return Records;
		}

		bool IsSealed() const
		{
			return bSealed;
		}
		const FString& GetCollectionFailureDiagnostic() const { return LateRegistrationFailureDiagnostic; }

	private:
		TArray<FAngelscriptBindRecord> Records;
		FString LateRegistrationFailureDiagnostic;
		bool bSealed = false;
		bool bPreparing = false;
		TUniquePtr<asCDefinitions> BuildingHost;
		asCDefinitionGraphLease FrozenHost;
		FAngelscriptBindState HostBindState;
		TMap<FName, int32> CallbackInvocationCounts;
		TArray<FAngelscriptHostDeclarationView> HostInspection;
		uint32 HostIdentitySerial = 1;
		asSStableKey MakeHostKey(const char* Kind, const char* Namespace, const char* Name);
		void CaptureHostInspection();
	};
}
