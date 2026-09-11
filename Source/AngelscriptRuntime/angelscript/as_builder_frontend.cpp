/* Maintained Unreal AngelScript fork — detached staged compilation driver. */
#include "as_builder.h"
#include "as_namespace.h"
#include "as_property.h"
#include "frontend/as_ast_projection.h"
#include "frontend/as_definition_consumer.h"
#include "frontend/as_descriptor_consumer.h"
#include "frontend/as_identifier_table.h"
#include "frontend/as_preprocess_result.h"
#include "frontend/as_tokenizer.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

BEGIN_AS_NAMESPACE
class asCBuilderState
	{
	public:
		asCBuilderState(TSharedRef<asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot,
			asCDiagnosticsEngine& InDiagnostics, const asSBuilderOptions& InOptions)
			: Sources(Snapshot), Diagnostics(InDiagnostics), Options(InOptions), Configuration(Options.PreprocessorFlags),
			  Preprocessor(Sources, Configuration, Diagnostics) { Results.Reserve(uint8(asEBuilderStage::Failed)); }

		asCSourceManager Sources;
		asCDiagnosticsEngine& Diagnostics;
		const asSBuilderOptions Options;
		asCPreprocessorConfiguration Configuration;
		asCPreprocessor Preprocessor;
		asCIdentifierTable Identifiers;
		TArray<asSSemanticSourceInput> Inputs;
		TArray<asSDirectiveProcessResult> Preprocessed;
		TUniquePtr<asCCompilationSession> Session;
		TUniquePtr<asCMetadataImage> Definitions;
		asEBuilderStage Stage = asEBuilderStage::SourceReady;
		TArray<asSBuilderStageResult> Results;

		bool HasErrors() const
		{
			asCCollectingDiagnosticConsumer Consumer;
			Diagnostics.EmitTo(Consumer);
			for (const auto& Record : Consumer.GetRecords())
				if (Record.Severity == asEDiagnosticSeverity::Error) return true;
			return false;
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

		FString DefinitionText() const
		{
			TArray<FString> Lines;
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

		bool Record(asEBuilderStage Attempt, bool Success, FString Text, FString Error = {}, FString ASTJson = {}, TOptional<asEMetadataResult> MetadataStatus = {})
		{
			if (!Success)
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
			if (!Success) Definitions.Reset();
			return Success;
		}
	};

asCBuilder::asCBuilder(asCScriptEngine *_engine, asCModule *_module)
	: silent(false)
	, engine(_engine)
	, module(_module)
{
}

asCBuilder::~asCBuilder() = default;

asCBuilder::asCBuilder(TSharedRef<asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot,
	asCDiagnosticsEngine& Diagnostics, const asSBuilderOptions& Options)
	: asCBuilder(nullptr, nullptr)
{
frontendState = std::make_shared<asCBuilderState>(Snapshot, Diagnostics, Options);
	const bool Valid = Snapshot->IsFrozen() && Snapshot->GetFileCount() > 0 && !Options.StableScope.IsEmpty() && Options.WorkerCount > 0
		&& Diagnostics.CreateFragment(TEXT("builder-input")).GetSnapshotID() == Snapshot->GetSnapshotID();
	frontendState->Record(asEBuilderStage::SourceReady, Valid, Options.StableScope, Valid ? FString{} : TEXT("invalid-builder-input"));
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

const asCCompilationSession* asCBuilder::GetCompilationSession() const
{
	return frontendState ? frontendState->Session.Get() : nullptr;
}

TConstArrayView<asSDirectiveProcessResult> asCBuilder::GetPreprocessedSources() const
{
	return frontendState ? MakeArrayView(frontendState->Preprocessed) : TConstArrayView<asSDirectiveProcessResult>{};
}

TConstArrayView<asSSemanticSourceInput> asCBuilder::GetLexedSources() const
{
	return frontendState ? MakeArrayView(frontendState->Inputs) : TConstArrayView<asSSemanticSourceInput>{};
}

asCMetadataImage* asCBuilder::GetDefinitions() const
{
	return frontendState ? frontendState->Definitions.Get() : nullptr;
}

TUniquePtr<asCMetadataImage> asCBuilder::TakeDefinitions()
{
	return frontendState ? MoveTemp(frontendState->Definitions) : nullptr;
}

bool asCBuilder::RunThrough(asEBuilderStage Target)
{
if (!frontendState || GetStage() == asEBuilderStage::Failed || Target >= asEBuilderStage::Failed || Target < GetStage()) return false;
	while (GetStage() < Target) if (!RunStage(static_cast<asEBuilderStage>(uint8(GetStage()) + 1))) return false;
	return true;
}

bool asCBuilder::RunStage(asEBuilderStage Target)
{
if (!frontendState || Target >= asEBuilderStage::Failed || uint8(Target) != uint8(GetStage()) + 1) return false;
	asCBuilderState& State = *frontendState;
	bool Success = true;
	FString Text, Error, ASTJson;
	TOptional<asEMetadataResult> MetadataStatus;
	switch (Target)
	{
	case asEBuilderStage::Lexed:
		for (asUINT Index = 1; Index <= State.Sources.GetSnapshot()->GetFileCount(); ++Index)
		{
			const asCSourceFileID File{Index};
			FStringView Key;
			TConstArrayView<uint8> Bytes;
			if (!State.Sources.GetSnapshot()->TryGetLogicalSourceKey(File, Key) || !State.Sources.GetSnapshot()->TryGetBytes(File, Bytes)) { Success = false; break; }
			asSSemanticSourceInput& Input = State.Inputs.AddDefaulted_GetRef();
			Input.LogicalSourceKey = FString(Key);
			asCTokenizer Lexer(State.Sources, State.Sources.MakeRange(File, 0, Bytes.Num()),
				asSLexOptions(asEUnicodeIdentifierPolicy::AsciiOnly, asETriviaMode::RetainTrivia), State.Identifiers, State.Diagnostics);
			for (;;)
			{
				asCToken Token;
				if (!Lexer.Lex(Token)) { Success = false; break; }
				Input.RawTokens.Add(Token);
				if (Token.Kind == asETokenKind::EndOfFile) break;
			}
			Lexer.FlushDiagnostics();
		}
		State.Inputs.Sort([](const auto& A, const auto& B) { return A.LogicalSourceKey < B.LogicalSourceKey; });
		for (const auto& Input : State.Inputs) Text += Input.LogicalSourceKey + TEXT("\n") + State.TokenText(Input.RawTokens);
		Success &= !State.HasErrors();
		break;
	case asEBuilderStage::Preprocessed:
		for (const auto& Input : State.Inputs)
		{
			auto Processed = State.Preprocessor.Process(Input.RawTokens);
			Success &= Processed.IsValid();
			Text += Input.LogicalSourceKey + TEXT("\n") + State.TokenText(Processed.ActiveTokens);
			State.Preprocessed.Add(MoveTemp(Processed));
		}
		break;
	case asEBuilderStage::DeclarationsCollected:
		State.Session = MakeUnique<asCCompilationSession>(State.Sources.GetSnapshot(), State.Diagnostics, State.Options.StableScope,
			State.Options.TypeContext.IsValid() ? State.Options.TypeContext.ToSharedRef() : MakeShared<asCTypeContext, ESPMode::ThreadSafe>());
		for (const auto& Dependency : State.Options.Dependencies) Success &= State.Session->AddExternalDefinitions(Dependency);
		for (int32 Index = 0; Index < State.Preprocessed.Num(); ++Index)
			Success &= State.Session->CollectSource(State.Preprocessor, State.Preprocessed[Index], State.Inputs[Index].LogicalSourceKey);
		Success &= State.Session->CloseDeclarationBarrier();
		for (const auto& Input : State.Inputs) Text += Input.LogicalSourceKey + TEXT("\n");
		break;
	case asEBuilderStage::DeclarationsResolved:
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
			auto Result = Consumer.Consume(*State.Session, State.Options.Dependencies);
			Success = Result.Succeeded();
			if (Success) State.Definitions = MoveTemp(Result.Image);
			else for (const auto& Diagnostic : Result.Diagnostics) Error += Diagnostic.Message + TEXT("\n");
			Text = State.DefinitionText();
		}
		break;
	case asEBuilderStage::LayoutsFinalized:
		MetadataStatus = State.Definitions->FinalizeLayouts();
		Success = MetadataStatus.GetValue() == asEMetadataResult::Succeeded;
		Text = State.DefinitionText();
		break;
	case asEBuilderStage::DefinitionsFrozen:
		MetadataStatus = State.Definitions->Freeze();
		Success = MetadataStatus.GetValue() == asEMetadataResult::Succeeded;
		if (Success && State.Session)
		{
			State.Session->BindFrozenDefinitions(State.Definitions.Get());
		}
		Text = State.DefinitionText();
		break;
	default: return false;
	}
	if (!Success && Error.IsEmpty()) Error = FString::Printf(TEXT("stage-%u-failed"), uint8(Target));
	return State.Record(Target, Success, MoveTemp(Text), MoveTemp(Error), MoveTemp(ASTJson), MetadataStatus);
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

		Result.SemanticSession = MakeShared<asCCompilationSession>(SourceManager.GetSnapshot(), *Diagnostics, MoveTemp(StableScope));
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
