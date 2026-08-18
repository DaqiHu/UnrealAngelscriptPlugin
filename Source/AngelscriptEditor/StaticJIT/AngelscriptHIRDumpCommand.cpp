#include "StaticJIT/AngelscriptHIRDumpCommand.h"

#include "AngelscriptEngine.h"
#include "Cache/AngelscriptCacheEnvironment.h"
#include "StaticJIT/AngelscriptProjectSourceGraph.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

#include "StartAngelscriptHeaders.h"
#include "angelscript.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptEditor::StaticJIT
{
	namespace AngelscriptHIRDumpCommand_Private
	{
		static constexpr uint32 DumpSchemaRevision = 2;

		struct FSerializedFunction
		{
			FAngelscriptHIRDumpFunctionResult Result;
			FAngelscriptHash256 ExecutionHash;
			FAngelscriptHash256 DebugHash;
			FString NormalizedHIR;
		};

		const TCHAR* FormatName(const EAngelscriptHIRDumpFormat Format)
		{
			switch (Format)
			{
			case EAngelscriptHIRDumpFormat::Text: return TEXT("Text");
			case EAngelscriptHIRDumpFormat::Json: return TEXT("Json");
			case EAngelscriptHIRDumpFormat::Both: return TEXT("Both");
			default: return TEXT("Invalid");
			}
		}

		const TCHAR* CaptureStateName(
			const EAngelscriptHIRDumpCaptureState State)
		{
			return State == EAngelscriptHIRDumpCaptureState::Verified
				? TEXT("Verified") : TEXT("Missing");
		}

		bool ParseFormat(
			const FStringView Value,
			EAngelscriptHIRDumpFormat& OutFormat)
		{
			if (Value.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
			{
				OutFormat = EAngelscriptHIRDumpFormat::Text;
				return true;
			}
			if (Value.Equals(TEXT("Json"), ESearchCase::IgnoreCase))
			{
				OutFormat = EAngelscriptHIRDumpFormat::Json;
				return true;
			}
			if (Value.Equals(TEXT("Both"), ESearchCase::IgnoreCase))
			{
				OutFormat = EAngelscriptHIRDumpFormat::Both;
				return true;
			}
			return false;
		}

		bool NormalizeStableFilter(
			FString& InOutFilter,
			const TCHAR* Name,
			FString& OutError)
		{
			InOutFilter.TrimStartAndEndInline();
			if (InOutFilter.IsEmpty())
			{
				return true;
			}
			if (InOutFilter.Len() != 64)
			{
				OutError = FString::Printf(
					TEXT("AngelscriptHIRDump %s filter must be one complete 64-digit stable key."),
					Name);
				return false;
			}
			for (const TCHAR Character : InOutFilter)
			{
				if (!FChar::IsHexDigit(Character))
				{
					OutError = FString::Printf(
						TEXT("AngelscriptHIRDump %s filter contains a non-hex digit."),
						Name);
					return false;
				}
			}
			InOutFilter.ToLowerInline();
			return true;
		}

		FString OneLine(FString Value)
		{
			Value.ReplaceInline(TEXT("\r"), TEXT("\\r"));
			Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));
			return Value;
		}

		FAngelscriptHash256 BuildCompleteGraphIdentity(
			const EAngelscriptJITTargetProfile Profile,
			const FAngelscriptStaticJITGenerationSnapshot& Snapshot)
		{
			FAngelscriptArtifactCanonicalWriter Writer(
				TEXT("developer-hir-dump-complete-graph"));
			Writer.WriteUInt32(DumpSchemaRevision);
			Writer.WriteUInt8(static_cast<uint8>(Profile));
			Writer.WriteUInt8(static_cast<uint8>(Snapshot.CaptureProfile));
			Writer.WriteUInt32(Snapshot.SourceCompilePassCount);
			Writer.WriteHash(Snapshot.ArtifactProfile.Hash);

			TArray<int32> ModuleOrder;
			for (int32 Index = 0; Index < Snapshot.Modules.Num(); ++Index)
			{
				ModuleOrder.Add(Index);
			}
			ModuleOrder.Sort([&Snapshot](const int32 Left, const int32 Right)
			{
				return Snapshot.Modules[Left].ModuleKey.Hash
					< Snapshot.Modules[Right].ModuleKey.Hash;
			});
			Writer.WriteUInt32(ModuleOrder.Num());
			for (const int32 Index : ModuleOrder)
			{
				const FAngelscriptStaticJITGenerationModule& Module =
					Snapshot.Modules[Index];
				Writer.WriteHash(Module.ModuleKey.Hash);
				Writer.WriteString(Module.CanonicalModuleName);
				Writer.WriteString(Module.VirtualSourcePath);
			}

			TArray<int32> FunctionOrder;
			for (int32 Index = 0; Index < Snapshot.Functions.Num(); ++Index)
			{
				FunctionOrder.Add(Index);
			}
			FunctionOrder.Sort([&Snapshot](const int32 Left, const int32 Right)
			{
				return Snapshot.Functions[Left].FunctionKey.Hash
					< Snapshot.Functions[Right].FunctionKey.Hash;
			});
			Writer.WriteUInt32(FunctionOrder.Num());
			for (const int32 Index : FunctionOrder)
			{
				const FAngelscriptStaticJITGenerationFunction& Function =
					Snapshot.Functions[Index];
				Writer.WriteHash(Function.ModuleKey.Hash);
				Writer.WriteHash(Function.FunctionKey.Hash);
				Writer.WriteHash(Function.ExecutionHash);
				Writer.WriteHash(Function.DebugHash);
				Writer.WriteHash(Function.EntryAbiHash);
				Writer.WriteString(Function.CanonicalDeclaration);
				Writer.WriteBool(Function.VerifiedTypedHIR != nullptr);
			}

			TArray<int32> TypeOrder;
			for (int32 Index = 0; Index < Snapshot.Types.Num(); ++Index)
			{
				TypeOrder.Add(Index);
			}
			TypeOrder.Sort([&Snapshot](const int32 Left, const int32 Right)
			{
				return Snapshot.Types[Left].TypeKey.Hash
					< Snapshot.Types[Right].TypeKey.Hash;
			});
			Writer.WriteUInt32(TypeOrder.Num());
			for (const int32 Index : TypeOrder)
			{
				const FAngelscriptStaticJITGenerationType& Type =
					Snapshot.Types[Index];
				Writer.WriteHash(Type.ModuleKey.Hash);
				Writer.WriteHash(Type.TypeKey.Hash);
				Writer.WriteString(Type.CanonicalDeclaration);
				Writer.WriteBool(Type.bTargetMaterializesScriptReflection);
			}

			TArray<int32> GlobalOrder;
			for (int32 Index = 0; Index < Snapshot.Globals.Num(); ++Index)
			{
				GlobalOrder.Add(Index);
			}
			GlobalOrder.Sort([&Snapshot](const int32 Left, const int32 Right)
			{
				return Snapshot.Globals[Left].GlobalKey.Hash
					< Snapshot.Globals[Right].GlobalKey.Hash;
			});
			Writer.WriteUInt32(GlobalOrder.Num());
			for (const int32 Index : GlobalOrder)
			{
				const FAngelscriptStaticJITGenerationGlobal& Global =
					Snapshot.Globals[Index];
				Writer.WriteHash(Global.ModuleKey.Hash);
				Writer.WriteHash(Global.GlobalKey.Hash);
				Writer.WriteString(Global.CanonicalDeclaration);
			}

			TArray<int32> DescriptorOrder;
			for (int32 Index = 0; Index < Snapshot.Descriptors.Num(); ++Index)
			{
				DescriptorOrder.Add(Index);
			}
			DescriptorOrder.Sort([&Snapshot](const int32 Left, const int32 Right)
			{
				return Snapshot.Descriptors[Left].StableDescriptorKey
					< Snapshot.Descriptors[Right].StableDescriptorKey;
			});
			Writer.WriteUInt32(DescriptorOrder.Num());
			for (const int32 Index : DescriptorOrder)
			{
				const FAngelscriptStaticJITGenerationDescriptor& Descriptor =
					Snapshot.Descriptors[Index];
				Writer.WriteHash(Descriptor.StableDescriptorKey);
				Writer.WriteUInt8(static_cast<uint8>(Descriptor.Kind));
				Writer.WriteString(Descriptor.CanonicalName);
				Writer.WriteString(Descriptor.CanonicalDeclaration);
				Writer.WriteString(Descriptor.ReceiverCanonicalName);
				Writer.WriteBool(Descriptor.bUFunctionRoot);
				Writer.WriteHash(Descriptor.EntryPlan.EntryPlanHash);
			}

			auto WriteDependencies = [&Writer](
				const TArray<FAngelscriptStaticJITGenerationDependency>& Values)
			{
				TArray<int32> Order;
				for (int32 Index = 0; Index < Values.Num(); ++Index)
				{
					Order.Add(Index);
				}
				Order.Sort([&Values](const int32 Left, const int32 Right)
				{
					const FAngelscriptStaticJITGenerationDependency& A =
						Values[Left];
					const FAngelscriptStaticJITGenerationDependency& B =
						Values[Right];
					if (!(A.OwnerFunctionKey.Hash == B.OwnerFunctionKey.Hash))
					{
						return A.OwnerFunctionKey.Hash < B.OwnerFunctionKey.Hash;
					}
					if (A.Reference != B.Reference)
					{
						return A.Reference < B.Reference;
					}
					return FAngelscriptArtifactCanonicalWriter::
						CompareCanonicalUtf8Strings(
							A.CanonicalDeclaration,
							B.CanonicalDeclaration) < 0;
				});
				Writer.WriteUInt32(Order.Num());
				for (const int32 Index : Order)
				{
					const FAngelscriptStaticJITGenerationDependency& Value =
						Values[Index];
					Writer.WriteHash(Value.OwnerFunctionKey.Hash);
					Writer.WriteUInt8(
						static_cast<uint8>(Value.Reference.Kind));
					Writer.WriteHash(Value.Reference.StableKey);
					Writer.WriteHash(Value.Reference.ExpectedAbi);
					Writer.WriteString(Value.CanonicalDeclaration);
				}
			};
			WriteDependencies(Snapshot.Dependencies);
			WriteDependencies(Snapshot.ExternalNativeCalls);
			return Writer.FinalizeHash();
		}

		bool ResolveStableCallTarget(
			FAngelscriptEngine& Engine,
			const FAngelscriptStaticJITGenerationSnapshot& Snapshot,
			const int32 FunctionId,
			FString& OutTarget,
			FString& OutError)
		{
			asIScriptFunction* const Target =
				Engine.GetScriptEngine()->GetFunctionById(FunctionId);
			if (Target == nullptr)
			{
				OutError = FString::Printf(
					TEXT("HIR resolved call target %d is absent from the generation Engine."),
					FunctionId);
				return false;
			}
			if (const FAngelscriptStaticJITGenerationFunction* Owned =
				Snapshot.Functions.FindByPredicate(
					[Target](
						const FAngelscriptStaticJITGenerationFunction& Function)
					{
						return Function.EngineLocalFunction == Target;
					}))
			{
				OutTarget = TEXT("ScriptFunction:")
					+ Owned->FunctionKey.Hash.ToHexString();
				return true;
			}
			FAngelscriptStableFunctionKey FunctionKey;
			if (Engine.TryResolveStableFunctionKey(Target, FunctionKey)
				&& !FunctionKey.Hash.IsZero())
			{
				OutTarget = TEXT("ScriptFunction:")
					+ FunctionKey.Hash.ToHexString();
				return true;
			}
			FAngelscriptArtifactReference Reference;
			if (FAngelscriptCacheEnvironmentIdentity::TryBuildFunctionReference(
					*reinterpret_cast<asCScriptFunction*>(Target), Reference))
			{
				OutTarget = FString::Printf(
					TEXT("EnvironmentSymbol:%s:%s"),
					*Reference.StableKey.ToHexString(),
					*Reference.ExpectedAbi.ToHexString());
				return true;
			}
			OutError = FString::Printf(
				TEXT("HIR resolved call target '%s' has no stable generation identity."),
				UTF8_TO_TCHAR(Target->GetDeclaration()));
			return false;
		}

		bool NormalizeHIR(
			FAngelscriptEngine& Engine,
			const FAngelscriptStaticJITGenerationSnapshot& Snapshot,
			const asCTypedSemanticFunction& HIR,
			FString& OutText,
			FString& OutError)
		{
			OutText = UTF8_TO_TCHAR(
				DumpTypedSemanticFunction(HIR).AddressOf());
			TSet<int32> NormalizedFunctionIds;
			for (asUINT Index = 0; Index < HIR.expressions.GetLength(); ++Index)
			{
				const asSTypedSemanticExpression& Expression =
					HIR.expressions[Index];
				if ((Expression.kind
						!= asETypedSemanticExpressionKind::ResolvedCall
					&& Expression.kind
						!= asETypedSemanticExpressionKind::CallRewrite)
					|| Expression.resolvedFunctionId < 0
					|| NormalizedFunctionIds.Contains(
						Expression.resolvedFunctionId))
				{
					continue;
				}
				NormalizedFunctionIds.Add(Expression.resolvedFunctionId);
				FString StableTarget;
				if (!ResolveStableCallTarget(
					Engine, Snapshot, Expression.resolvedFunctionId,
					StableTarget, OutError))
				{
					return false;
				}
				const FString Needle = FString::Printf(
					TEXT(" functionId=%d "),
					Expression.resolvedFunctionId);
				const FString Replacement = TEXT(" stableTarget=")
					+ StableTarget + TEXT(" ");
				if (OutText.ReplaceInline(
					*Needle, *Replacement, ESearchCase::CaseSensitive) == 0)
				{
					OutError = TEXT(
						"HIR normalized text did not contain its resolved-call identity token.");
					return false;
				}
			}
			return true;
		}

		FString BuildText(
			const FAngelscriptHIRDumpResult& Result,
			const TArray<FSerializedFunction>& Functions)
		{
			FString Text;
			Text += FString::Printf(TEXT("SchemaRevision=%u\n"),
				DumpSchemaRevision);
			Text += TEXT("RequestKind=DeveloperHIRDump\n");
			Text += FString::Printf(TEXT("Profile=%s\n"),
				FAngelscriptJITTargetProfiles::ToString(Result.TargetProfile));
			Text += FString::Printf(TEXT("Format=%s\n"),
				FormatName(Result.Format));
			Text += TEXT("CompleteGraphIdentity=")
				+ Result.CompleteGraphIdentity.ToHexString() + TEXT("\n");
			Text += TEXT("ArtifactProfile=")
				+ Result.ArtifactProfile.Hash.ToHexString() + TEXT("\n");
			Text += TEXT("CaptureProfile=VerifiedTypedHIR\n");
			Text += FString::Printf(TEXT("SourceCompilePassCount=%u\n"),
				Result.SourceCompilePassCount);
			Text += FString::Printf(TEXT("CompleteModuleCount=%d\n"),
				Result.CompleteModuleCount);
			Text += FString::Printf(TEXT("CompleteFunctionCount=%d\n"),
				Result.CompleteFunctionCount);
			Text += FString::Printf(TEXT("CapturedFunctionCount=%d\n"),
				Result.CapturedFunctionCount);
			Text += FString::Printf(TEXT("MissingFunctionCount=%d\n"),
				Result.MissingFunctionCount);
			Text += FString::Printf(TEXT("InvalidFunctionCount=%d\n"),
				Result.InvalidFunctionCount);
			Text += TEXT("ModuleFilter=")
				+ (Result.StableModuleFilter.IsEmpty()
					? TEXT("<none>") : Result.StableModuleFilter) + TEXT("\n");
			Text += TEXT("FunctionFilter=")
				+ (Result.StableFunctionFilter.IsEmpty()
					? TEXT("<none>") : Result.StableFunctionFilter) + TEXT("\n");
			Text += FString::Printf(TEXT("SelectedFunctionCount=%d\n"),
				Functions.Num());
			for (const FSerializedFunction& Function : Functions)
			{
				Text += TEXT("\nFunctionBegin\n");
				Text += TEXT("ModuleKey=")
					+ Function.Result.ModuleKey.Hash.ToHexString() + TEXT("\n");
				Text += TEXT("FunctionKey=")
					+ Function.Result.FunctionKey.Hash.ToHexString() + TEXT("\n");
				Text += TEXT("Declaration=")
					+ OneLine(Function.Result.CanonicalDeclaration) + TEXT("\n");
				Text += TEXT("ExecutionHash=")
					+ Function.ExecutionHash.ToHexString() + TEXT("\n");
				Text += TEXT("DebugHash=")
					+ Function.DebugHash.ToHexString() + TEXT("\n");
				Text += FString::Printf(TEXT("CaptureState=%s\n"),
					CaptureStateName(Function.Result.CaptureState));
				if (!Function.NormalizedHIR.IsEmpty())
				{
					Text += Function.NormalizedHIR;
					if (!Text.EndsWith(TEXT("\n")))
					{
						Text += TEXT("\n");
					}
				}
				Text += TEXT("FunctionEnd\n");
			}
			return Text;
		}

		bool BuildJson(
			const FAngelscriptHIRDumpResult& Result,
			const TArray<FSerializedFunction>& Functions,
			FString& OutJson)
		{
			OutJson.Reset();
			TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR,
					TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("schemaRevision"), DumpSchemaRevision);
			Writer->WriteValue(TEXT("requestKind"),
				TEXT("DeveloperHIRDump"));
			Writer->WriteValue(TEXT("profile"),
				FAngelscriptJITTargetProfiles::ToString(Result.TargetProfile));
			Writer->WriteValue(TEXT("format"), FormatName(Result.Format));
			Writer->WriteValue(TEXT("completeGraphIdentity"),
				Result.CompleteGraphIdentity.ToHexString());
			Writer->WriteValue(TEXT("artifactProfile"),
				Result.ArtifactProfile.Hash.ToHexString());
			Writer->WriteValue(TEXT("captureProfile"),
				TEXT("VerifiedTypedHIR"));
			Writer->WriteValue(TEXT("sourceCompilePassCount"),
				Result.SourceCompilePassCount);
			Writer->WriteValue(TEXT("completeModuleCount"),
				Result.CompleteModuleCount);
			Writer->WriteValue(TEXT("completeFunctionCount"),
				Result.CompleteFunctionCount);
			Writer->WriteValue(TEXT("capturedFunctionCount"),
				Result.CapturedFunctionCount);
			Writer->WriteValue(TEXT("missingFunctionCount"),
				Result.MissingFunctionCount);
			Writer->WriteValue(TEXT("invalidFunctionCount"),
				Result.InvalidFunctionCount);
			Writer->WriteValue(TEXT("moduleFilter"),
				Result.StableModuleFilter);
			Writer->WriteValue(TEXT("functionFilter"),
				Result.StableFunctionFilter);
			Writer->WriteArrayStart(TEXT("functions"));
			for (const FSerializedFunction& Function : Functions)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("moduleKey"),
					Function.Result.ModuleKey.Hash.ToHexString());
				Writer->WriteValue(TEXT("functionKey"),
					Function.Result.FunctionKey.Hash.ToHexString());
				Writer->WriteValue(TEXT("canonicalDeclaration"),
					Function.Result.CanonicalDeclaration);
				Writer->WriteValue(TEXT("executionHash"),
					Function.ExecutionHash.ToHexString());
				Writer->WriteValue(TEXT("debugHash"),
					Function.DebugHash.ToHexString());
				Writer->WriteValue(TEXT("captureState"),
					CaptureStateName(Function.Result.CaptureState));
				Writer->WriteValue(TEXT("normalizedHIR"),
					Function.NormalizedHIR);
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
			return Writer->Close();
		}

		bool SerializeSnapshot(
			const FAngelscriptHIRDumpRequest& Request,
			const FAngelscriptProjectSourceGraphCompileView& View,
			FAngelscriptHIRDumpResult& OutResult,
			FString& OutError)
		{
			const FAngelscriptStaticJITGenerationSnapshot& Snapshot =
				*View.Snapshot;
			OutResult.ArtifactProfile = Snapshot.ArtifactProfile;
			OutResult.SourceCompilePassCount = Snapshot.SourceCompilePassCount;
			OutResult.CompleteModuleCount = Snapshot.Modules.Num();
			OutResult.CompleteFunctionCount = Snapshot.Functions.Num();
			OutResult.CompleteGraphIdentity = BuildCompleteGraphIdentity(
				Request.TargetProfile, Snapshot);

			TArray<int32> FunctionOrder;
			for (int32 Index = 0; Index < Snapshot.Functions.Num(); ++Index)
			{
				FunctionOrder.Add(Index);
			}
			FunctionOrder.Sort([&Snapshot](const int32 Left, const int32 Right)
			{
				return Snapshot.Functions[Left].FunctionKey.Hash
					< Snapshot.Functions[Right].FunctionKey.Hash;
			});

			TArray<FSerializedFunction> Selected;
			for (const int32 Index : FunctionOrder)
			{
				const FAngelscriptStaticJITGenerationFunction& Function =
					Snapshot.Functions[Index];
				const bool bCaptured = Function.VerifiedTypedHIR != nullptr;
				OutResult.CapturedFunctionCount += bCaptured ? 1 : 0;
				OutResult.MissingFunctionCount += bCaptured ? 0 : 1;
				if (bCaptured)
				{
					const asSTypedSemanticVerificationResult Verification =
						VerifyTypedSemanticFunction(*Function.VerifiedTypedHIR);
					if (!Verification.IsValid())
					{
						++OutResult.InvalidFunctionCount;
						OutError = FString::Printf(
							TEXT("HIR verifier rejected '%s': %s"),
							*Function.CanonicalDeclaration,
							UTF8_TO_TCHAR(Verification.detail.AddressOf()));
						return false;
					}
				}
				const FString ModuleKey =
					Function.ModuleKey.Hash.ToHexString();
				const FString FunctionKey =
					Function.FunctionKey.Hash.ToHexString();
				if ((!Request.StableModuleFilter.IsEmpty()
						&& ModuleKey != Request.StableModuleFilter)
					|| (!Request.StableFunctionFilter.IsEmpty()
						&& FunctionKey != Request.StableFunctionFilter))
				{
					continue;
				}

				FSerializedFunction& Output = Selected.AddDefaulted_GetRef();
				Output.Result.ModuleKey = Function.ModuleKey;
				Output.Result.FunctionKey = Function.FunctionKey;
				Output.Result.CanonicalDeclaration =
					Function.CanonicalDeclaration;
				Output.Result.CaptureState = bCaptured
					? EAngelscriptHIRDumpCaptureState::Verified
					: EAngelscriptHIRDumpCaptureState::Missing;
				Output.ExecutionHash = Function.ExecutionHash;
				Output.DebugHash = Function.DebugHash;
				if (bCaptured && !NormalizeHIR(
					View.Engine, Snapshot, *Function.VerifiedTypedHIR,
					Output.NormalizedHIR, OutError))
				{
					return false;
				}
			}
			if ((!Request.StableModuleFilter.IsEmpty()
					|| !Request.StableFunctionFilter.IsEmpty())
				&& Selected.IsEmpty())
			{
				OutError = TEXT(
					"AngelscriptHIRDump filters matched no function in the complete source graph.");
				return false;
			}

			OutResult.SelectedFunctions.Reserve(Selected.Num());
			for (const FSerializedFunction& Function : Selected)
			{
				OutResult.SelectedFunctions.Add(Function.Result);
			}
			const FString Text = BuildText(OutResult, Selected);
			FString Json;
			if ((Request.Format == EAngelscriptHIRDumpFormat::Json
					|| Request.Format == EAngelscriptHIRDumpFormat::Both)
				&& !BuildJson(OutResult, Selected, Json))
			{
				OutError = TEXT("AngelscriptHIRDump failed to finalize JSON.");
				return false;
			}
			if (!IFileManager::Get().MakeDirectory(
				*Request.OutputDirectory, true))
			{
				OutError = FString::Printf(
					TEXT("AngelscriptHIRDump could not create output directory: %s"),
					*Request.OutputDirectory);
				return false;
			}
			if (Request.Format == EAngelscriptHIRDumpFormat::Text
				|| Request.Format == EAngelscriptHIRDumpFormat::Both)
			{
				OutResult.TextOutputPath = FPaths::Combine(
					Request.OutputDirectory, TEXT("Snapshot.hir.txt"));
				if (!FFileHelper::SaveStringToFile(
					Text, *OutResult.TextOutputPath,
					FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
				{
					OutError = TEXT("AngelscriptHIRDump failed to write text output.");
					return false;
				}
			}
			if (Request.Format == EAngelscriptHIRDumpFormat::Json
				|| Request.Format == EAngelscriptHIRDumpFormat::Both)
			{
				OutResult.JsonOutputPath = FPaths::Combine(
					Request.OutputDirectory, TEXT("Snapshot.hir.json"));
				if (!FFileHelper::SaveStringToFile(
					Json, *OutResult.JsonOutputPath,
					FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
				{
					OutError = TEXT("AngelscriptHIRDump failed to write JSON output.");
					return false;
				}
			}
			return true;
		}
	}

	bool FAngelscriptHIRDumpCommand::Parse(
		const FStringView Params,
		const FStringView DefaultProjectFile,
		FAngelscriptHIRDumpRequest& OutRequest,
		FString& OutError)
	{
		using namespace AngelscriptHIRDumpCommand_Private;
		OutRequest = {};
		OutError.Reset();
		const FString ParamsString(Params);

		FString ProfileText;
		FParse::Value(*ParamsString, TEXT("Profile="), ProfileText);
		if (ProfileText.IsEmpty())
		{
			OutError = TEXT(
				"AngelscriptHIRDump requires -Profile=EditorDevelopment|GameDevelopment|GameShipping.");
			return false;
		}
		if (!FAngelscriptJITTargetProfiles::TryParse(
			ProfileText, OutRequest.TargetProfile)
			|| !FAngelscriptJITTargetProfiles::GetTraits(
				OutRequest.TargetProfile).IsSet())
		{
			OutError = FString::Printf(
				TEXT("AngelscriptHIRDump requires one concrete Profile; got '%s'."),
				*ProfileText);
			return false;
		}

		FParse::Value(*ParamsString, TEXT("Project="), OutRequest.ProjectFile);
		if (OutRequest.ProjectFile.IsEmpty())
		{
			FParse::Value(*ParamsString, TEXT("ProjectFile="),
				OutRequest.ProjectFile);
		}
		if (OutRequest.ProjectFile.IsEmpty())
		{
			OutRequest.ProjectFile = FString(DefaultProjectFile);
		}
		OutRequest.ProjectFile.TrimStartAndEndInline();
		if (OutRequest.ProjectFile.IsEmpty())
		{
			OutError = TEXT(
				"AngelscriptHIRDump requires a project descriptor.");
			return false;
		}
		OutRequest.ProjectFile = FPaths::ConvertRelativePathToFull(
			OutRequest.ProjectFile);
		FPaths::NormalizeFilename(OutRequest.ProjectFile);

		FString FormatText;
		FParse::Value(*ParamsString, TEXT("Format="), FormatText);
		if (!FormatText.IsEmpty()
			&& !ParseFormat(FormatText, OutRequest.Format))
		{
			OutError = FString::Printf(
				TEXT("Invalid AngelscriptHIRDump Format='%s'; expected Text|Json|Both."),
				*FormatText);
			return false;
		}

		FParse::Value(*ParamsString, TEXT("Module="),
			OutRequest.StableModuleFilter);
		FParse::Value(*ParamsString, TEXT("Function="),
			OutRequest.StableFunctionFilter);
		if (!NormalizeStableFilter(
				OutRequest.StableModuleFilter, TEXT("Module"), OutError)
			|| !NormalizeStableFilter(
				OutRequest.StableFunctionFilter, TEXT("Function"), OutError))
		{
			return false;
		}

		FParse::Value(*ParamsString, TEXT("Output="),
			OutRequest.OutputDirectory);
		const FString ProjectDirectory =
			FPaths::GetPath(OutRequest.ProjectFile);
		if (OutRequest.OutputDirectory.IsEmpty())
		{
			OutRequest.OutputDirectory = FPaths::Combine(
				ProjectDirectory,
				TEXT("Saved/Angelscript/HIRDump"),
				FAngelscriptJITTargetProfiles::ToString(
					OutRequest.TargetProfile));
		}
		else if (FPaths::IsRelative(OutRequest.OutputDirectory))
		{
			OutRequest.OutputDirectory = FPaths::Combine(
				ProjectDirectory, OutRequest.OutputDirectory);
		}
		OutRequest.OutputDirectory = FPaths::ConvertRelativePathToFull(
			OutRequest.OutputDirectory);
		FPaths::NormalizeDirectoryName(OutRequest.OutputDirectory);
		return true;
	}

	FAngelscriptHIRDumpResult FAngelscriptHIRDumpCommand::Execute(
		const FAngelscriptHIRDumpRequest& Request)
	{
		using namespace AngelscriptHIRDumpCommand_Private;
		FAngelscriptHIRDumpResult Result;
		Result.RequestKind = Request.RequestKind;
		Result.TargetProfile = Request.TargetProfile;
		Result.Format = Request.Format;
		Result.OutputDirectory = Request.OutputDirectory;
		Result.StableModuleFilter = Request.StableModuleFilter;
		Result.StableFunctionFilter = Request.StableFunctionFilter;
		if (Request.RequestKind
				!= EAngelscriptHIRDumpRequestKind::DeveloperHIRDump
			|| !FAngelscriptJITTargetProfiles::GetTraits(
				Request.TargetProfile).IsSet()
			|| Request.ProjectFile.IsEmpty()
			|| Request.OutputDirectory.IsEmpty())
		{
			Result.ExitCode = EAngelscriptHIRDumpExitCode::InvalidArguments;
			Result.Error = TEXT(
				"AngelscriptHIRDump request is incomplete or non-concrete.");
			return Result;
		}

		FAngelscriptProjectSourceGraphCompileRequest CompileRequest;
		CompileRequest.RequestKind =
			EAngelscriptProjectSourceGraphRequestKind::DeveloperHIRDump;
		CompileRequest.ProjectFile = Request.ProjectFile;
		CompileRequest.TargetProfile = Request.TargetProfile;
		CompileRequest.CaptureProfile =
			EAngelscriptStaticJITCaptureProfile::VerifiedTypedHIR;
		FString Error;
		if (!FAngelscriptProjectSourceGraph::Compile(
			CompileRequest,
			[&Request, &Result](
				const FAngelscriptProjectSourceGraphCompileView& View,
				FString* ConsumerError)
			{
				FString Error;
				const bool bSerialized = SerializeSnapshot(
					Request, View, Result, Error);
				if (!bSerialized && ConsumerError != nullptr)
				{
					*ConsumerError = Error;
				}
				return bSerialized;
			},
			&Error))
		{
			Result.Error = Error;
			return Result;
		}
		Result.ExitCode = EAngelscriptHIRDumpExitCode::Success;
		Result.Error.Reset();
		return Result;
	}
}
