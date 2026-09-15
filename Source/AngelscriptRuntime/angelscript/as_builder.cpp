/* Maintained Unreal AngelScript fork — detached staged compilation driver. */
#include "as_builder.h"
#include "as_module_definition_set.h"
#include "Containers/Set.h"
#include "as_namespace.h"
#include "as_property.h"
#include "frontend/AST/as_ast_projection.h"
#include "frontend/Compile/as_definition_consumer.h"
#include "frontend/Compile/as_descriptor_consumer.h"
#include "as_bytecode_emitter.h"
#include "as_scriptfunction.h"
#include "frontend/Basic/as_identifier_table.h"
#include "frontend/Lexer/as_frontend_options.h"
#include "frontend/Lexer/as_preprocess_result.h"
#include "frontend/Lexer/as_tokenizer.h"
#include "Async/Async.h"
#include "Async/Future.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTLS.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Optional.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <atomic>

BEGIN_AS_NAMESPACE

namespace
{
	asSBuilderOptions NormalizeBuilderOptions(asSBuilderOptions Options)
	{
		Options.WorkerCount = FMath::Max(1, Options.WorkerCount);
		Options.LexBatchSize = FMath::Max(1, Options.LexBatchSize);
		return Options;
	}
}

class asCBuilderState
	{
	public:
		asCBuilderState(
			asCSourceManager InSources,
			const asSBuilderOptions& InOptions,
			bool bValid,
			FString AdmissionError)
			: Sources(MoveTemp(InSources))
			, OwnedDiagnostics(MakeUnique<asCDiagnosticsEngine>(Sources, InOptions.Diagnostics))
			, Diagnostics(*OwnedDiagnostics)
			, Options(NormalizeBuilderOptions(InOptions))
			, Configuration(Options.PreprocessorFlags)
			, Preprocessor(Sources, Configuration, Diagnostics)
			, bPendingAdmissionValid(bValid)
			, PendingAdmissionError(MoveTemp(AdmissionError))
		{
			Results.Reserve(uint8(asEBuilderStage::Failed));
		}

		asCSourceManager Sources;
		TUniquePtr<asCDiagnosticsEngine> OwnedDiagnostics;
		asCDiagnosticsEngine& Diagnostics;
		const asSBuilderOptions Options;
		asCPreprocessorConfiguration Configuration;
		asCPreprocessor Preprocessor;
		asCIdentifierTable Identifiers;
		TArray<asSSemanticSourceInput> Inputs;
		TArray<asSDirectiveProcessResult> Preprocessed;
		TArray<FString> PreprocessedKeys;
		TUniquePtr<asCCompilationSession> Session;
		TUniquePtr<asCModuleDefinitionSet> ModuleDefinitions;
		TUniquePtr<asCCompileOutput> CompileOutput;
		asEBuilderStage Stage = asEBuilderStage::SourceReady;
		TArray<asSBuilderStageResult> Results;
		TArray<asIBuilderCallbacks*> Callbacks;
		bool bAdmissionPublished = false;
		bool bPendingAdmissionValid = false;
		FString PendingAdmissionError;
#if WITH_ANGELSCRIPT_TESTS
		TSet<FString> DebugForceLexFalseKeys;
		TSet<FString> DebugForceAcquisitionFailKeys;
		int32 DebugPeakWorkers = 0;
		int32 DebugProcessInvocationCount = 0;
		uint32 DebugLexCallerThreadId = 0;
		TArray<FString> DebugProcessedKeys;
		TArray<uint32> DebugWorkThreadIds;
#endif

		bool PublishAdmission()
		{
			if (bAdmissionPublished)
			{
				return Stage != asEBuilderStage::Failed;
			}
			bAdmissionPublished = true;
			return Record(
				asEBuilderStage::SourceReady,
				bPendingAdmissionValid,
				Options.StableScope,
				bPendingAdmissionValid ? FString() : PendingAdmissionError);
		}

		bool HasErrors() const
		{
			return Diagnostics.HasLanguageFailure();
		}

		FString TokenText(TConstArrayView<asCToken> Tokens) const
		{
			FString Text;
			for (const asCToken& Token : Tokens)
			{
				TConstArrayView<uint8> Bytes;
				FString Spelling;
				if (Sources.TryGetBytes(Token.Range, Bytes) && !Bytes.IsEmpty())
				{
					FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
					Spelling = FString(Converted.Length(), Converted.Get());
				}
				Spelling.ReplaceInline(TEXT("\n"), TEXT("\\n"));
				Spelling.ReplaceInline(TEXT("\r"), TEXT("\\r"));
				Text += FString::Printf(TEXT("%u|%u:%u|%s\n"), uint16(Token.Kind),
					Token.Range.Begin.ByteOffset, Token.Range.End.ByteOffset, *Spelling);
			}
			return Text;
		}

		asCModuleDefinitionSet* Definitions() const
		{
			return ModuleDefinitions.Get();
		}

		TArray<asCModuleDefinitionSet*> CollectExternalSets(bool& Success) const
		{
			TArray<asCModuleDefinitionSet*> Sets;
			for (asCModuleDefinitionSet* Set : Options.Dependencies)
			{
				if (!Set || !Set->IsImmutable())
				{
					Success = false;
					continue;
				}
				Sets.Add(Set);
			}
			return Sets;
		}

		FString DefinitionText() const
		{
			TArray<FString> Lines;
			asCModuleDefinitionSet* Definitions = this->Definitions();
			if (!Definitions) return {};
			auto TypeText = [](const asCDataType& Type)
			{
				return FString::Printf(TEXT("token:%u;nominal:%s;reference:%u;handle:%u;const:%u;handle-const:%u;nullable:%u"),
					uint32(Type.GetTokenType()), Type.GetTypeInfo() ? *Type.GetTypeInfo()->GetStableKey().ToHexString() : TEXT(""),
					uint32(Type.IsReference()), uint32(Type.IsObjectHandle()), uint32(Type.IsReadOnly()),
					uint32(Type.IsHandleToConst()), uint32(Type.IsMetadataNullable()));
			};
			for (const asCTypeInfo* Type : Definitions->GetTypes())
			{
				Lines.Add(FString::Printf(TEXT("type|%s|%s::%s|size:%u|flags:%llu"),
					*Type->GetStableKey().ToHexString(), UTF8_TO_TCHAR(Type->GetNamespace()), UTF8_TO_TCHAR(Type->GetName()),
					Type->GetSize(), static_cast<unsigned long long>(Type->GetFlags())));
				if (const asCObjectType* Object = CastToObjectType(const_cast<asCTypeInfo*>(Type)))
				{
					if (Object->derivedFrom) Lines.Add(TEXT("base|") + Type->GetStableKey().ToHexString() + TEXT("|") + Object->derivedFrom->GetStableKey().ToHexString());
					for (asUINT Index = 0; Index < Object->interfaces.GetLength(); ++Index)
						Lines.Add(TEXT("interface|") + Type->GetStableKey().ToHexString() + TEXT("|") + Object->interfaces[Index]->GetStableKey().ToHexString());
					for (asUINT Index = 0; Index < Object->GetPropertyCount(); ++Index)
					{
						const char* Name = nullptr;
						int Offset = 0;
						Object->GetProperty(Index, &Name, nullptr, nullptr, nullptr, &Offset, nullptr, nullptr, nullptr, nullptr);
						Lines.Add(FString::Printf(TEXT("property|%s|%s|offset:%d|%s"), *Type->GetStableKey().ToHexString(), UTF8_TO_TCHAR(Name), Offset,
							*TypeText(Object->properties[Index]->type)));
					}
				}
				for (asUINT Index = 0; Index < Type->GetEnumValueCount(); ++Index)
				{
					int Value = 0;
					const char* Name = Type->GetEnumValueByIndex(Index, &Value);
					Lines.Add(FString::Printf(TEXT("enum|%s|%s|%d"), *Type->GetStableKey().ToHexString(), UTF8_TO_TCHAR(Name), Value));
				}
			}
			for (const asCScriptFunction* Function : Definitions->GetFunctions())
			{
				Lines.Add(FString::Printf(TEXT("function|%s|%s::%s|parameters:%u|callable:%u|return:%s"),
					*Function->GetStableKey().ToHexString(), UTF8_TO_TCHAR(Function->GetNamespace()), UTF8_TO_TCHAR(Function->GetName()),
					Function->GetParamCount(), uint32(Function->IsMetadataCallableSignature()), *TypeText(Function->GetReturnDataType())));
				for (asUINT Index = 0; Index < Function->GetParamCount(); ++Index)
				{
					asDWORD Flags = 0;
					Function->GetParam(Index, nullptr, &Flags);
					Lines.Add(FString::Printf(TEXT("parameter|%s|%u|flags:%u|%s"), *Function->GetStableKey().ToHexString(), Index, Flags,
						*TypeText(*Function->GetParameterDataType(Index))));
				}
			}
			for (const asCGlobalProperty* Global : Definitions->GetGlobalProperties())
			{
				Lines.Add(FString::Printf(TEXT("global|%s|%s::%s|%s"), *Global->GetStableKey().ToHexString(),
					UTF8_TO_TCHAR(Global->nameSpace->name.AddressOf()), UTF8_TO_TCHAR(Global->name.AddressOf()), *TypeText(Global->type)));
			}
			Lines.Sort();
			return FString::Join(Lines, TEXT("\n"));
		}

		bool Record(asEBuilderStage Attempt, bool Success, FString Text, FString Error = {}, FString ASTJson = {}, TOptional<asEMetadataResult> MetadataStatus = {}, const asSByteCodeEmissionResult* Emission = nullptr)
		{
			if (!Success && !Diagnostics.HasLanguageFailure())
			{
				auto Fragment = Diagnostics.CreateFragment(TEXT("builder-stage"));
				asSDiagnosticRecord Diagnostic;
				Diagnostic.DiagnosticID = 5001;
				Diagnostic.Severity = asEDiagnosticSeverity::Error;
				if (!Inputs.IsEmpty() && !Inputs[0].RawTokens.IsEmpty()) Diagnostic.PrimaryRange = Inputs[0].RawTokens[0].Range;
				Diagnostic.Arguments.Add(asSDiagnosticArgument::FromText(Error));
				Fragment.Report(MoveTemp(Diagnostic));
				Diagnostics.Submit(MoveTemp(Fragment));
			}
			asSBuilderStageResult Result;
			Result.Stage = Attempt;
			Result.Succeeded = Success;
			Result.Error = MoveTemp(Error);
			Result.Text = MoveTemp(Text);
			Result.MetadataStatus = MetadataStatus;
			Result.DiagnosticResult = Diagnostics.CaptureResult();
			if (Emission)
			{
				asIngestByteCodeEmissionResult(Result, *Emission);
			}
			asCCollectingDiagnosticConsumer Consumer;
			Diagnostics.EmitTo(Consumer);
			Result.Diagnostics.Append(Consumer.GetRecords().GetData(), Consumer.GetRecords().Num());
			// Explicit writer order is stable; no process pointers, SnapshotID,
			// local allocation ID, scheduling or wall-clock data enter these dumps.
			auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result.Json);
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("stage"), int32(Attempt));
			Writer->WriteValue(TEXT("succeeded"), Success);
			Writer->WriteValue(TEXT("error"), Result.Error);
			Writer->WriteValue(TEXT("text"), Result.Text);
			Writer->WriteValue(TEXT("diagnostics"), asCDiagnosticRenderer::SerializeStable(Result.Diagnostics));
			if (MetadataStatus.IsSet()) Writer->WriteValue(TEXT("metadataStatus"), int32(MetadataStatus.GetValue()));
			if (!ASTJson.IsEmpty()) Writer->WriteRawJSONValue(TEXT("ast"), ASTJson);
			Writer->WriteObjectEnd();
			Writer->Close();
			Results.Add(MoveTemp(Result));
			Stage = Success ? Attempt : asEBuilderStage::Failed;
			if (!Success) ModuleDefinitions.Reset();
			RefreshCompileOutput();
			return Success;
		}

		void RefreshCompileOutput()
		{
			if (!CompileOutput) CompileOutput = MakeUnique<asCCompileOutput>();
			CompileOutput->ReplaceDiagnosticResult(Diagnostics.CaptureResult());
			if (ModuleDefinitions)
			{
				TArray<TSharedRef<FAngelscriptModuleDesc>> Modules;
				TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
				Module->ModuleName = Inputs.Num() ? Inputs[0].LogicalSourceKey : TEXT("Main.as");
				for (asCTypeInfo* Type : ModuleDefinitions->GetTypes())
				{
					if (!Type || (Type->nameSpace && Type->nameSpace->name.GetLength())) continue;
					TSharedRef<FAngelscriptClassDesc> Class = MakeShared<FAngelscriptClassDesc>();
					Class->ClassName = UTF8_TO_TCHAR(Type->GetName());
					Class->ScriptType = nullptr;
					for (asCScriptFunction* Function : ModuleDefinitions->GetFunctions())
					{
						if (!Function || Function->objectType != Type) continue;
						TSharedRef<FAngelscriptFunctionDesc> Method = MakeShared<FAngelscriptFunctionDesc>();
						Method->FunctionName = UTF8_TO_TCHAR(Function->GetName());
						Method->ScriptFunction = nullptr;
						Class->Methods.Add(Method);
					}
					Module->Classes.Add(Class);
				}
				Modules.Add(Module);
				CompileOutput->GetDefinitionOutput().ReplaceModules(MoveTemp(Modules));
				return;
			}
			if (!Session || !Session->IsPublishable()) return;
			FAngelscriptPreprocessResult Projected;
			FAngelscriptDescriptorConsumer DescriptorConsumer;
			if (DescriptorConsumer.Project(*Session, Sources, Projected, true))
			{
				CompileOutput->GetDefinitionOutput().ReplaceModules(MoveTemp(Projected.Modules));
			}
		}
	};

asCBuilder::asCBuilder(asCScriptEngine *_engine, asCModule *_module)
	: silent(false)
	, engine(_engine)
	, module(_module)
{
}

asCBuilder::~asCBuilder() = default;

namespace
{
	struct asSLexFileProduct
	{
		FString Key;
		asSSemanticSourceInput Input;
		TOptional<asSDirectiveProcessResult> Preprocessed;
		bool bHardFail = false;
		uint32 ThreadId = 0;
	};

	struct asSLexWorkContext
	{
		asCBuilderState* State = nullptr;
		FCriticalSection QueueLock;
		TArray<asCSourceFileID> Remaining;
		int32 BatchSize = 1;
		std::atomic<int32> Active{0};
		std::atomic<int32> Peak{0};
		std::atomic<int32> ProcessCount{0};
		FCriticalSection ObservationLock;
		TArray<FString> ProcessedKeys;
		TArray<uint32> WorkThreadIds;
		TArray<TArray<asSLexFileProduct>> Locals;
	};

	void LexOneFile(asSLexWorkContext& Context, asCSourceFileID File, asSLexFileProduct& Out)
	{
		asCBuilderState& State = *Context.State;
		Out.ThreadId = FPlatformTLS::GetCurrentThreadId();
		FStringView KeyView;
		TConstArrayView<uint8> Bytes;
		if (!State.Sources.TryGetLogicalSourceKey(File, KeyView))
		{
			Out.bHardFail = true;
			return;
		}
		Out.Key = FString(KeyView);
		Out.Input.LogicalSourceKey = Out.Key;

#if WITH_ANGELSCRIPT_TESTS
		if (State.DebugForceAcquisitionFailKeys.Contains(Out.Key))
		{
			Out.bHardFail = true;
			return;
		}
#endif
		if (!State.Sources.TryGetBytes(File, Bytes))
		{
			Out.bHardFail = true;
			return;
		}

#if WITH_ANGELSCRIPT_TESTS
		if (State.DebugForceLexFalseKeys.Contains(Out.Key))
		{
			Out.bHardFail = true;
			return;
		}
#endif

		asCTokenizer Lexer(
			State.Sources,
			State.Sources.MakeRange(File, 0, Bytes.Num()),
			asSLexOptions(asEUnicodeIdentifierPolicy::AsciiOnly, asETriviaMode::RetainTrivia),
			State.Identifiers,
			State.Diagnostics);
		for (;;)
		{
			asCToken Token;
			if (!Lexer.Lex(Token))
			{
				Out.bHardFail = true;
				break;
			}
			Out.Input.RawTokens.Add(Token);
			if (Token.Kind == asETokenKind::EndOfFile)
			{
				break;
			}
		}
		Lexer.FlushDiagnostics();
		if (Out.bHardFail || Lexer.HasErrors())
		{
			return;
		}

		asSDirectiveProcessResult Processed = State.Preprocessor.Process(Out.Input.RawTokens);
		Context.ProcessCount.fetch_add(1, std::memory_order_relaxed);
		{
			FScopeLock Lock(&Context.ObservationLock);
			Context.ProcessedKeys.Add(Out.Key);
		}
		Out.Preprocessed = MoveTemp(Processed);
	}

	void RunLexWorker(asSLexWorkContext& Context, int32 WorkerIndex)
	{
		for (;;)
		{
			TArray<asCSourceFileID> Batch;
			{
				FScopeLock Lock(&Context.QueueLock);
				for (int32 Index = 0; Index < Context.BatchSize && Context.Remaining.Num() > 0; ++Index)
				{
					Batch.Add(Context.Remaining.Pop(EAllowShrinking::No));
				}
			}
			if (Batch.IsEmpty())
			{
				break;
			}

			const int32 Now = Context.Active.fetch_add(1, std::memory_order_relaxed) + 1;
			int32 Peak = Context.Peak.load(std::memory_order_relaxed);
			while (Now > Peak
				&& !Context.Peak.compare_exchange_weak(Peak, Now, std::memory_order_relaxed))
			{
			}

			for (const asCSourceFileID File : Batch)
			{
				asSLexFileProduct Product;
				LexOneFile(Context, File, Product);
				{
					FScopeLock Lock(&Context.ObservationLock);
					Context.WorkThreadIds.Add(Product.ThreadId);
				}
				Context.Locals[WorkerIndex].Add(MoveTemp(Product));
			}
			Context.Active.fetch_sub(1, std::memory_order_relaxed);
		}
	}

	bool RunLexWave(asCBuilderState& State)
	{
		State.Inputs.Reset();
		State.Preprocessed.Reset();
		State.PreprocessedKeys.Reset();

		const int32 WorkerCount = FMath::Max(1, State.Options.WorkerCount);
		const int32 BatchSize = FMath::Max(1, State.Options.LexBatchSize);
		const asUINT FileCount = State.Sources.GetFileCount();
#if WITH_ANGELSCRIPT_TESTS
		State.DebugLexCallerThreadId = FPlatformTLS::GetCurrentThreadId();
		State.DebugPeakWorkers = 0;
		State.DebugProcessInvocationCount = 0;
		State.DebugProcessedKeys.Reset();
		State.DebugWorkThreadIds.Reset();
#endif
		if (FileCount == 0)
		{
			return !State.HasErrors();
		}

		asSLexWorkContext Context;
		Context.State = &State;
		Context.BatchSize = BatchSize;
		Context.Locals.SetNum(WorkerCount);
		for (asUINT Index = 1; Index <= FileCount; ++Index)
		{
			Context.Remaining.Add(asCSourceFileID(Index));
		}

		if (WorkerCount == 1)
		{
			RunLexWorker(Context, 0);
		}
		else
		{
			TArray<TFuture<void>> Workers;
			Workers.Reserve(WorkerCount);
			for (int32 Index = 0; Index < WorkerCount; ++Index)
			{
				Workers.Add(Async(EAsyncExecution::Thread, [&Context, Index]()
				{
					RunLexWorker(Context, Index);
				}));
			}
			for (TFuture<void>& Worker : Workers)
			{
				Worker.Wait();
			}
		}

		TArray<asSLexFileProduct> Joined;
		for (TArray<asSLexFileProduct>& Local : Context.Locals)
		{
			Joined.Append(MoveTemp(Local));
		}
		Joined.Sort([](const asSLexFileProduct& Left, const asSLexFileProduct& Right)
		{
			return Left.Key < Right.Key;
		});

		bool Success = true;
		for (asSLexFileProduct& Product : Joined)
		{
			Success &= !Product.bHardFail;
			State.Inputs.Add(MoveTemp(Product.Input));
			if (Product.Preprocessed.IsSet())
			{
				Success &= Product.Preprocessed->IsValid();
				State.PreprocessedKeys.Add(Product.Key);
				State.Preprocessed.Add(MoveTemp(*Product.Preprocessed));
			}
		}
		Success &= !State.HasErrors();

#if WITH_ANGELSCRIPT_TESTS
		State.DebugPeakWorkers = Context.Peak.load(std::memory_order_relaxed);
		State.DebugProcessInvocationCount = Context.ProcessCount.load(std::memory_order_relaxed);
		State.DebugProcessedKeys = MoveTemp(Context.ProcessedKeys);
		State.DebugWorkThreadIds = MoveTemp(Context.WorkThreadIds);
#endif
		return Success;
	}

	FString ValidateSharedBuilderInput(
		TConstArrayView<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources,
		const asSBuilderOptions& Options)
	{
		if (Sources.Num() == 0)
		{
			return TEXT("empty-source-batch");
		}
		if (Options.StableScope.IsEmpty())
		{
			return TEXT("empty-stable-scope");
		}
		TArray<FString> Paths;
		for (const TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>& Source : Sources)
		{
			if (!Source->bHasSourceText)
			{
				return TEXT("unprepared-source");
			}
			const FString Path(Source->GetPath());
			if (Path.IsEmpty())
			{
				return TEXT("empty-logical-path");
			}
			asUINT Length = 0;
			if (!asTryConvertSourceByteLength(static_cast<uint64>(Source->GetSourceText().Len()), Length))
			{
				return TEXT("source-length-overflow");
			}
			for (const FString& Existing : Paths)
			{
				if (FStringView(Existing).Equals(Path, ESearchCase::CaseSensitive))
				{
					return TEXT("duplicate-logical-path");
				}
			}
			Paths.Add(Path);
		}
		return {};
	}
}

asCBuilder::asCBuilder(
	TConstArrayView<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources,
	const asSBuilderOptions& Options,
	TConstArrayView<asIBuilderCallbacks*> Callbacks)
	: asCBuilder(nullptr, nullptr)
{
	const FString AdmissionError = ValidateSharedBuilderInput(Sources, Options);
	const bool bValid = AdmissionError.IsEmpty();
	asCSourceManager Manager;
	if (bValid)
	{
		Manager = asCSourceManager(Sources);
		if (!Manager.IsReady())
		{
			frontendState = std::make_shared<asCBuilderState>(
				asCSourceManager(),
				Options,
				false,
				TEXT("invalid-builder-input"));
			frontendState->Callbacks.Append(Callbacks.GetData(), Callbacks.Num());
			return;
		}
	}
	frontendState = std::make_shared<asCBuilderState>(
		MoveTemp(Manager),
		Options,
		bValid,
		AdmissionError);
	frontendState->Callbacks.Append(Callbacks.GetData(), Callbacks.Num());
}

asEBuilderStage asCBuilder::GetStage() const
{
	return frontendState ? frontendState->Stage : asEBuilderStage::Failed;
}

const asSBuilderStageResult* asCBuilder::GetStageResult(asEBuilderStage Stage) const
{
	if (frontendState)
		for (const auto& Result : frontendState->Results) if (Result.Stage == Stage) return &Result;
	return nullptr;
}

const asCSourceManager* asCBuilder::GetSourceManager() const
{
	return frontendState ? &frontendState->Sources : nullptr;
}

const asCCompilationSession* asCBuilder::GetCompilationSession() const
{
	return frontendState ? frontendState->Session.Get() : nullptr;
}

asCCompilationSession* asCBuilder::GetCompilationSession()
{
	return frontendState ? frontendState->Session.Get() : nullptr;
}

TConstArrayView<asSDirectiveProcessResult> asCBuilder::GetPreprocessedSources() const
{
	return frontendState ? MakeArrayView(frontendState->Preprocessed) : TConstArrayView<asSDirectiveProcessResult>{};
}

TSharedRef<const asCDiagnosticResult, ESPMode::ThreadSafe> asCBuilder::CaptureDiagnosticResult() const
{
	if (frontendState)
	{
		return frontendState->Diagnostics.CaptureResult();
	}
	return MakeShared<asCDiagnosticResult, ESPMode::ThreadSafe>();
}

TConstArrayView<asSSemanticSourceInput> asCBuilder::GetLexedSources() const
{
	return frontendState ? MakeArrayView(frontendState->Inputs) : TConstArrayView<asSSemanticSourceInput>{};
}

const asCModuleDefinitionSet* asCBuilder::GetModuleDefinitionSet() const
{
	return frontendState ? frontendState->ModuleDefinitions.Get() : nullptr;
}

TUniquePtr<asCModuleDefinitionSet> asCBuilder::TakeModuleDefinitionSet()
{
	return frontendState ? MoveTemp(frontendState->ModuleDefinitions) : nullptr;
}

asCModuleDefinitionSet* asCBuilder::GetModuleDefinitionSet()
{
	return frontendState ? frontendState->ModuleDefinitions.Get() : nullptr;
}

const asCCompileOutput* asCBuilder::GetCompileOutput() const
{
	return frontendState ? frontendState->CompileOutput.Get() : nullptr;
}

TUniquePtr<asCCompileOutput> asCBuilder::TakeCompileOutput()
{
	return frontendState ? MoveTemp(frontendState->CompileOutput) : nullptr;
}

#if WITH_ANGELSCRIPT_TESTS
namespace
{
	int32 GDebugDeclarationsResolvedEntries = 0;
}

void asCBuilder::DebugResetDeclarationsResolvedEntries()
{
	GDebugDeclarationsResolvedEntries = 0;
}

int32 asCBuilder::DebugGetDeclarationsResolvedEntries()
{
	return GDebugDeclarationsResolvedEntries;
}

void asCBuilder::DebugForceLexFalse(const FString& LogicalSourceKey)
{
	if (frontendState)
	{
		frontendState->DebugForceLexFalseKeys.Add(LogicalSourceKey);
	}
}

void asCBuilder::DebugForceAcquisitionFailure(const FString& LogicalSourceKey)
{
	if (frontendState)
	{
		frontendState->DebugForceAcquisitionFailKeys.Add(LogicalSourceKey);
	}
}

int32 asCBuilder::DebugGetPeakWorkers() const
{
	return frontendState ? frontendState->DebugPeakWorkers : 0;
}

int32 asCBuilder::DebugGetProcessInvocationCount() const
{
	return frontendState ? frontendState->DebugProcessInvocationCount : 0;
}

uint32 asCBuilder::DebugGetLexCallerThreadId() const
{
	return frontendState ? frontendState->DebugLexCallerThreadId : 0;
}

TArray<FString> asCBuilder::DebugGetProcessedLogicalKeys() const
{
	return frontendState ? frontendState->DebugProcessedKeys : TArray<FString>{};
}

TArray<uint32> asCBuilder::DebugGetWorkThreadIds() const
{
	return frontendState ? frontendState->DebugWorkThreadIds : TArray<uint32>{};
}
#endif

bool asCBuilder::RunThrough(asEBuilderStage Target)
{
	if (!frontendState || !frontendState->PublishAdmission()) return false;
	if (GetStage() == asEBuilderStage::Failed || Target >= asEBuilderStage::Failed || Target < GetStage()) return false;
	while (GetStage() < Target) if (!RunStage(static_cast<asEBuilderStage>(uint8(GetStage()) + 1))) return false;
	return true;
}

bool asCBuilder::RunStage(asEBuilderStage Target)
{
	if (!frontendState || !frontendState->PublishAdmission()) return false;
	if (Target >= asEBuilderStage::Failed || uint8(Target) != uint8(GetStage()) + 1) return false;
	asCBuilderState& State = *frontendState;
	bool Success = true;
	FString Text, Error, ASTJson;
	TOptional<asEMetadataResult> MetadataStatus;
	asSByteCodeEmissionResult Emitted;
	const asSByteCodeEmissionResult* Emission = nullptr;
	switch (Target)
	{
	case asEBuilderStage::Lexed:
		Success = RunLexWave(State);
		for (const auto& Input : State.Inputs) Text += Input.LogicalSourceKey + TEXT("\n") + State.TokenText(Input.RawTokens);
		break;
	case asEBuilderStage::Preprocessed:
		for (int32 Index = 0; Index < State.Preprocessed.Num(); ++Index)
		{
			Success &= State.Preprocessed[Index].IsValid();
			const FString& Key = State.PreprocessedKeys.IsValidIndex(Index)
				? State.PreprocessedKeys[Index]
				: FString();
			Text += Key + TEXT("\n") + State.TokenText(State.Preprocessed[Index].ActiveTokens);
		}
		break;
	case asEBuilderStage::DeclarationsCollected:
		State.Session = MakeUnique<asCCompilationSession>(State.Sources, State.Diagnostics, State.Options.StableScope,
			State.Options.TypeContext.IsValid() ? State.Options.TypeContext.ToSharedRef() : MakeShared<asCTypeContext, ESPMode::ThreadSafe>());
		{
			const TArray<asCModuleDefinitionSet*> External = State.CollectExternalSets(Success);
			if (Success)
			{
				for (asCModuleDefinitionSet* Set : External) Success &= State.Session->AddExternalDefinitions(Set);
			}
			else
			{
				Error = TEXT("definition-set-dependency-must-be-immutable");
			}
		}
		for (int32 Index = 0; Index < State.Preprocessed.Num(); ++Index)
			Success &= State.Session->CollectSource(
				State.Preprocessor,
				State.Preprocessed[Index],
				State.PreprocessedKeys.IsValidIndex(Index) ? State.PreprocessedKeys[Index] : FString());
		Success &= State.Session->CloseDeclarationBarrier();
		for (const auto& Input : State.Inputs) Text += Input.LogicalSourceKey + TEXT("\n");
		break;
	case asEBuilderStage::DeclarationsResolved:
#if WITH_ANGELSCRIPT_TESTS
		++GDebugDeclarationsResolvedEntries;
#endif
		Success = State.Session->ResolveDeclarations(State.Options.WorkerCount);
		Text = State.Session->SerializeStableProjection();
		Error = State.Session->SerializeStableDiagnostics();
		break;
	case asEBuilderStage::BodiesAnalyzed:
		Success = State.Session->AnalyzeBodies(State.Options.WorkerCount);
		Text = State.Session->SerializeStableBodyProjection();
		Error = State.Session->SerializeStableBodyDiagnostics();
		break;
	case asEBuilderStage::ASTVerified:
		Success = State.Session->VerifyAST();
		Error = State.Session->GetVerificationError();
		if (Success)
		{
			asCASTProjection Projection;
			Success = asCASTProjection::Build(State.Session->GetASTContext(), State.Session->GetTranslationUnit(), Projection, Error);
			if (Success) { Text = Projection.DumpText(); ASTJson = Projection.DumpJson(); }
		}
		break;
	case asEBuilderStage::DefinitionsBuilt:
		{
			asCDefinitionConsumer Consumer;
			const TArray<asCModuleDefinitionSet*> External = State.CollectExternalSets(Success);
			if (!Success)
			{
				Error = TEXT("definition-set-dependency-must-be-immutable");
				break;
			}
			auto Result = Consumer.Consume(*State.Session, External);
			Success = Result.Succeeded();
			if (Success)
			{
				State.ModuleDefinitions = MoveTemp(Result.Definitions);
				if (State.ModuleDefinitions)
				{
					State.ModuleDefinitions->AttachDependencies(State.Options.Dependencies);
				}
				Success = State.ModuleDefinitions.IsValid();
			}
			else for (const auto& Diagnostic : Result.Diagnostics) Error += Diagnostic.Message + TEXT("\n");
			Text = State.DefinitionText();
		}
		break;
	case asEBuilderStage::LayoutsFinalized:
		{
			asCModuleDefinitionSet* Definitions = State.Definitions();
			if (!Definitions) { Success = false; break; }
			MetadataStatus = Definitions->FinalizeLayouts();
			Success = MetadataStatus.GetValue() == asEMetadataResult::Succeeded;
			Text = State.DefinitionText();
		}
		break;
	case asEBuilderStage::DefinitionsFrozen:
		{
			asCModuleDefinitionSet* Definitions = State.Definitions();
			if (!Definitions) { Success = false; break; }
			MetadataStatus = Definitions->Freeze();
			Success = MetadataStatus.GetValue() == asEMetadataResult::Succeeded;
			if (Success && State.Session)
			{
				State.Session->BindFrozenDefinitions(Definitions);
			}
			Text = State.DefinitionText();
		}
		break;
	case asEBuilderStage::ByteCodeEmitted:
		{
			Emitted = asCByteCodeEmitter::Emit(*this);
			Emission = &Emitted;
			Success = Emitted.EmissionStatus == asEByteCodeEmissionStatus::Succeeded;
			if (!Success) Error = Emitted.Failure.StableFragmentKey.IsEmpty() ? TEXT("bytecode-emit-failed") : Emitted.Failure.StableFragmentKey;
		}
		break;
	default: return false;
	}
	if (!Success && Error.IsEmpty()) Error = FString::Printf(TEXT("stage-%u-failed"), uint8(Target));
	return State.Record(Target, Success, MoveTemp(Text), MoveTemp(Error), MoveTemp(ASTJson), MetadataStatus, Emission);
}

void asIngestByteCodeEmissionResult(asSBuilderStageResult& Stage, const asSByteCodeEmissionResult& Emitted)
{
	Stage.bHasEmission = true;
	Stage.EmissionStatus = Emitted.EmissionStatus;
	Stage.EmissionFailure = Emitted.Failure;
}

	FAngelscriptPreprocessResult asCBuilder::CompileDeclarations(
		const asCSourceManager& SourceManager,
		const asCPreprocessorConfiguration& Configuration,
		asCDiagnosticsEngine& InDiagnostics,
		TConstArrayView<asSSemanticSourceInput> Sources,
		FString StableScope,
		int32 WorkerCount)
	{
asCDiagnosticsEngine* Diagnostics = &InDiagnostics;
		asCPreprocessor Preprocessor(SourceManager, Configuration, InDiagnostics);
		FAngelscriptPreprocessResult Result;
		Result.ConfigurationDigest = Configuration.GetDigest();

		asCSourceRange PrimaryRange;
		if (!Sources.IsEmpty() && !Sources[0].RawTokens.IsEmpty())
		{
			PrimaryRange = Sources[0].RawTokens[0].Range;
		}
		auto ReportAssemblyError = [&](FString Message)
		{
			if (!Diagnostics) return;
			asCDiagnosticFragment Fragment = Diagnostics->CreateFragment(TEXT("semantic-result"));
			asSDiagnosticRecord Record;
			Record.DiagnosticID = 3003;
			Record.Severity = asEDiagnosticSeverity::Error;
			Record.PrimaryRange = PrimaryRange;
			Record.Arguments.Add(asSDiagnosticArgument::FromText(Message));
			Fragment.Report(MoveTemp(Record));
			Diagnostics->Submit(MoveTemp(Fragment));
		};
		auto CollectDiagnostics = [&]()
		{
			if (!Diagnostics) return;
			asCCollectingDiagnosticConsumer Consumer;
			Diagnostics->EmitTo(Consumer);
			Result.Diagnostics.Reset();
			for (const asSDiagnosticRecord& Record : Consumer.GetRecords()) Result.Diagnostics.Add(Record);
		};
		auto Fail = [&](FString Message)
		{
			if (!Message.IsEmpty()) ReportAssemblyError(MoveTemp(Message));
			CollectDiagnostics();
			Result.RejectPublishableOutput();
			return MoveTemp(Result);
		};

		if (!Diagnostics || !SourceManager.IsReady() || Sources.IsEmpty() || StableScope.IsEmpty() || WorkerCount <= 0)
		{
			return Fail(TEXT("invalid-semantic-input"));
		}

		Result.SemanticSession = MakeShared<asCCompilationSession>(SourceManager, *Diagnostics, MoveTemp(StableScope));
		TSet<FString> LogicalSourceKeys;
		bool bCollected = true;
		for (const asSSemanticSourceInput& Source : Sources)
		{
			if (Source.LogicalSourceKey.IsEmpty() || LogicalSourceKeys.Contains(Source.LogicalSourceKey))
			{
				bCollected = false;
				continue;
			}
			LogicalSourceKeys.Add(Source.LogicalSourceKey);
			asSDirectiveProcessResult Processed = Preprocessor.Process(Source.RawTokens);
			TSharedPtr<asSDirectiveProcessResult> Retained = MakeShared<asSDirectiveProcessResult>(MoveTemp(Processed));
			Result.PreprocessingResults.Add(Retained);
			if (!Retained->IsValid()
				|| !Result.SemanticSession->CollectSource(Preprocessor, *Retained, Source.LogicalSourceKey))
			{
				bCollected = false;
			}
		}
		Result.State = EAngelscriptPreprocessResultState::Parsed;
		if (!bCollected) return Fail({});
		if (!Result.SemanticSession->CloseDeclarationBarrier()) return Fail(TEXT("declaration-barrier-failed"));
		if (!Result.SemanticSession->ResolveDeclarations(WorkerCount))
		{
			return Fail(Result.SemanticSession->SerializeStableDiagnostics());
		}

		FAngelscriptDescriptorConsumer DescriptorConsumer;
		if (!DescriptorConsumer.Project(*Result.SemanticSession, SourceManager, Result))
		{
			return Fail(TEXT("descriptor-projection-failed"));
		}
		if (!BuildDeclarationDependencyGraph(*Result.SemanticSession, SourceManager, Result.DeclarationDependencies))
		{
			return Fail(TEXT("declaration-dependency-finalization-failed"));
		}
		if (!Result.BodyInvalidation.Finalize()) return Fail(TEXT("body-invalidation-finalization-failed"));

		Result.State = EAngelscriptPreprocessResultState::Resolved;
		CollectDiagnostics();
		return Result;
	}

END_AS_NAMESPACE
