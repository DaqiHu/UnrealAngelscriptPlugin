#pragma once

#include "NativeEngine/NativeEngineTestSupport.h"
#include "../VM/NativeVMTestSupport.h"
#include "as_bytecode_emitter.h"
#include "as_bytecode_image.h"
#include "as_bytecode_opcode.h"
#include "as_builder.h"
#include "as_definitions.h"
#include "as_scriptfunction.h"

#include <memory>

struct FSourceExecutionInput
{
	TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
	AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
	explicit FSourceExecutionInput(const FString& Source)
	{
		Initialize({{TEXT("Main.as"), Source}}, nullptr, nullptr);
	}
	explicit FSourceExecutionInput(
		const FString& Source,
		TSharedPtr<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> TypeContext,
		asCDefinitions* Host)
	{
		Initialize({{TEXT("Main.as"), Source}}, TypeContext, Host);
	}
	explicit FSourceExecutionInput(
		TArray<TPair<FString, FString>> Files,
		TSharedPtr<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> TypeContext = nullptr,
		asCDefinitions* Host = nullptr)
	{
		Initialize(MoveTemp(Files), TypeContext, Host);
	}

private:
	void Initialize(
		TArray<TPair<FString, FString>> Files,
		TSharedPtr<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> TypeContext,
		asCDefinitions* Host)
	{
		for (const TPair<FString, FString>& File : Files)
		{
			Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(File.Key, File.Value));
		}
		Options.StableScope = TEXT("vm-source");
		if (TypeContext.IsValid())
		{
			Options.TypeContext = TypeContext;
		}
		if (Host)
		{
			Options.Dependencies.Add(Host);
		}
	}
};

inline asCScriptFunction* FindSourceFunction(asCBuilder& Builder, const char* Name)
{
	if (asCDefinitions* Set = Builder.GetDefinitions())
	{
		for (asCScriptFunction* Candidate : Set->GetFunctions())
		{
			if (Candidate && Candidate->name == Name)
			{
				return Candidate;
			}
		}
	}
	return nullptr;
}

inline FString DescribeBuilderFailure(const asCBuilder& Builder)
{
	FString Out = FString::Printf(TEXT("stage=%d"), int32(Builder.GetStage()));
	for (uint8 Stage = 0; Stage < uint8(AS_NAMESPACE_QUALIFIER asEBuilderStage::Failed); ++Stage)
	{
		if (const auto* Result = Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage(Stage)))
		{
			Out += FString::Printf(TEXT(" | %d succ=%d err=%s text=%s"),
				int32(Stage), Result->Succeeded ? 1 : 0, *Result->Error, *Result->Text);
		}
	}
	return Out;
}

struct FSessionEmission
{
	TUniquePtr<asCByteCodeImage> Image;
	asEByteCodeImageStatus Status = asEByteCodeImageStatus::InvalidArgument;
	asEByteCodeEmissionStatus EmissionStatus = asEByteCodeEmissionStatus::InvalidInput;
	asSByteCodeEmissionFailure Failure;
};

inline FSessionEmission MakeSessionEmission(
	const asSByteCodeEmissionResult& Result,
	TUniquePtr<asCByteCodeImage> Image)
{
	FSessionEmission Emitted;
	Emitted.Image = MoveTemp(Image);
	Emitted.EmissionStatus = Result.EmissionStatus;
	Emitted.Failure = Result.Failure;
	Emitted.Status = Emitted.Image
		? asEByteCodeImageStatus::Succeeded
		: (Result.EmissionStatus == asEByteCodeEmissionStatus::Succeeded
			? asEByteCodeImageStatus::Succeeded
			: asEByteCodeImageStatus::InvalidArgument);
	return Emitted;
}

inline FSessionEmission EmitSessionImage(
	const asCCompilationSession* Session,
	const asCDefinitions* FrozenDefinitions,
	const asSByteCodeEmissionOptions& Options = asSByteCodeEmissionOptions())
{
	TUniquePtr<asCByteCodeImage> Image;
	const asSByteCodeEmissionResult Result = asEmitByteCodeImage(Session, FrozenDefinitions, Options, Image);
	return MakeSessionEmission(Result, MoveTemp(Image));
}

inline FSessionEmission EmitFromSession(
	asCBuilder& Builder,
	const asSByteCodeEmissionOptions& Options = asSByteCodeEmissionOptions())
{
	TUniquePtr<asCByteCodeImage> Image;
	const asSByteCodeEmissionResult Result = asEmitByteCodeImage(
		Builder.GetCompilationSession(),
		Builder.GetDefinitions(),
		Options,
		Image);
	return MakeSessionEmission(Result, MoveTemp(Image));
}

inline bool FunctionStableHasOpcode(const asCScriptFunction& Function, asEBCInstr Opcode)
{
	for (const asDWORD Word : Function.GetStableByteCode())
	{
		if (Word == static_cast<asDWORD>(Opcode))
		{
			return true;
		}
	}
	return false;
}

inline asERegistrationResult RegisterCompiledDefinitions(
	asCScriptEngine& Engine,
	asCBuilder& Builder,
	bool bPublishAdoptedByteCode = false)
{
	asCDefinitions* Set = Builder.GetDefinitions();
	if (!Set)
	{
		return asERegistrationResult::InvalidArgument;
	}
	if (asCScriptEngine* Bound = FindBoundEngine(*Set))
	{
		return Bound == &Engine
			? asERegistrationResult::AlreadyRegistered
			: asERegistrationResult::ForeignEngine;
	}
	if (!bPublishAdoptedByteCode)
	{
		(void)Set->TakeEmittedByteCode();
	}
	const asCCompileOutput* Output = Builder.GetCompileOutput();
	asCEngineCompileRegistration Registration(Engine);
	TArray<TUniquePtr<asCDefinitions>> Units;
	Units.Add(Builder.TakeDefinitions());
	return Output
		? Registration.Register(MoveTemp(Units), *Output)
		: Registration.Register(MoveTemp(Units));
}
