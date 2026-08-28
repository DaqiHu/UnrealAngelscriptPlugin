#include "Dump/AngelscriptASTDiagnostics.h"

#include "AngelscriptEngine.h"

#include "HAL/IConsoleManager.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_diagnostics.h"
#include "EndAngelscriptHeaders.h"

DEFINE_LOG_CATEGORY_STATIC(LogAngelscriptASTDiagnostics, Log, All);

namespace
{
	bool CopyAnsi(const FString& Value, asCString& OutValue)
	{
		FTCHARToUTF8 UTF8(*Value);
		OutValue.Assign(UTF8.Get(), static_cast<asUINT>(UTF8.Length()));
		return true;
	}

	bool ParseNodeSelector(
		const FString& Value,
		asSASTStructuredDumpFilter& OutFilter,
		FString& OutError)
	{
		if (Value.Len() < 2)
		{
			OutError = TEXT("--node requires D/T/S/E followed by a positive snapshot-local id.");
			return false;
		}
		switch (FChar::ToUpper(Value[0]))
		{
		case TEXT('D'): OutFilter.nodeClass = asAST_NODE_DECL; break;
		case TEXT('T'): OutFilter.nodeClass = asAST_NODE_TYPE; break;
		case TEXT('S'): OutFilter.nodeClass = asAST_NODE_STMT; break;
		case TEXT('E'): OutFilter.nodeClass = asAST_NODE_EXPR; break;
		default:
			OutError = TEXT("--node class must be D, T, S, or E.");
			return false;
		}
		int32 NodeId = 0;
		if (!LexTryParseString(NodeId, *Value.Mid(1)) || NodeId <= 0)
		{
			OutError = TEXT("--node id must be a positive snapshot-local integer.");
			return false;
		}
		OutFilter.nodeId = static_cast<asUINT>(NodeId);
		return true;
	}

	bool ParseDiagnosticRequest(
		const TArray<FString>& Args,
		asSASTDiagnosticRequest& OutRequest,
		FString& OutError)
	{
		OutError.Reset();
		if (Args.IsEmpty())
		{
			OutError = TEXT("Read-only AST operation required: list, dump, verify, query, or diff.");
			return false;
		}

		const FString Operation = Args[0].ToLower();
		if (Operation == TEXT("list")) OutRequest.operation = asAST_DIAGNOSTIC_LIST;
		else if (Operation == TEXT("dump")) OutRequest.operation = asAST_DIAGNOSTIC_DUMP;
		else if (Operation == TEXT("verify")) OutRequest.operation = asAST_DIAGNOSTIC_VERIFY;
		else if (Operation == TEXT("query")) OutRequest.operation = asAST_DIAGNOSTIC_QUERY;
		else if (Operation == TEXT("diff")) OutRequest.operation = asAST_DIAGNOSTIC_DIFF;
		else
		{
			OutError = FString::Printf(
				TEXT("Operation '%s' is unavailable: as.AST is a read-only list/dump/verify/query/diff surface."),
				*Args[0]);
			return false;
		}

		int32 PositionalIndex = 0;
		for (int32 Index = 1; Index < Args.Num(); ++Index)
		{
			FString Arg = Args[Index];
			if (Arg.Equals(TEXT("--json"), ESearchCase::IgnoreCase)
				|| Arg.Equals(TEXT("-json"), ESearchCase::IgnoreCase))
			{
				OutRequest.format = asAST_DIAGNOSTIC_JSON;
				continue;
			}
			if (Arg.Equals(TEXT("--text"), ESearchCase::IgnoreCase)
				|| Arg.Equals(TEXT("-text"), ESearchCase::IgnoreCase))
			{
				OutRequest.format = asAST_DIAGNOSTIC_TEXT;
				continue;
			}
			if (Arg.Equals(TEXT("--snippets"), ESearchCase::IgnoreCase)
				|| Arg.Equals(TEXT("-snippets"), ESearchCase::IgnoreCase))
			{
				OutRequest.includeSourceSnippets = true;
				continue;
			}

			FString Normalized = Arg;
			while (Normalized.StartsWith(TEXT("-"))) Normalized.RightChopInline(1);
			FString Key;
			FString Value;
			if (Normalized.Split(TEXT("="), &Key, &Value))
			{
				if (Key.Equals(TEXT("key"), ESearchCase::IgnoreCase))
				{
					CopyAnsi(Value, OutRequest.filter.stableDeclKey);
					continue;
				}
				if (Key.Equals(TEXT("kind"), ESearchCase::IgnoreCase))
				{
					CopyAnsi(Value, OutRequest.filter.nodeKind);
					continue;
				}
				if (Key.Equals(TEXT("source"), ESearchCase::IgnoreCase))
				{
					CopyAnsi(Value, OutRequest.filter.logicalSource);
					continue;
				}
				if (Key.Equals(TEXT("node"), ESearchCase::IgnoreCase))
				{
					if (!ParseNodeSelector(Value, OutRequest.filter, OutError)) return false;
					continue;
				}
				if (Key.Equals(TEXT("snippet-bytes"), ESearchCase::IgnoreCase))
				{
					int32 ByteCount = 0;
					if (!LexTryParseString(ByteCount, *Value) || ByteCount <= 0 || ByteCount > 4096)
					{
						OutError = TEXT("--snippet-bytes must be between 1 and 4096.");
						return false;
					}
					OutRequest.maxSnippetBytes = static_cast<asUINT>(ByteCount);
					continue;
				}
				OutError = FString::Printf(TEXT("Unknown read-only AST option '%s'."), *Arg);
				return false;
			}

			if (Arg.StartsWith(TEXT("-")))
			{
				OutError = FString::Printf(TEXT("Unknown read-only AST option '%s'."), *Arg);
				return false;
			}
			if (OutRequest.operation == asAST_DIAGNOSTIC_LIST)
			{
				OutError = TEXT("list does not accept a module name.");
				return false;
			}
			if (PositionalIndex == 0)
			{
				CopyAnsi(Arg, OutRequest.moduleName);
			}
			else if (PositionalIndex == 1 && OutRequest.operation == asAST_DIAGNOSTIC_DIFF)
			{
				CopyAnsi(Arg, OutRequest.compareModuleName);
			}
			else
			{
				OutError = FString::Printf(TEXT("Unexpected positional AST argument '%s'."), *Arg);
				return false;
			}
			++PositionalIndex;
		}

		if (OutRequest.operation != asAST_DIAGNOSTIC_LIST
			&& OutRequest.moduleName.GetLength() == 0)
		{
			OutError = TEXT("The selected AST operation requires a module name.");
			return false;
		}
		if (OutRequest.operation == asAST_DIAGNOSTIC_DIFF
			&& OutRequest.compareModuleName.GetLength() == 0)
		{
			OutError = TEXT("diff requires left and right module names.");
			return false;
		}
		if (OutRequest.operation == asAST_DIAGNOSTIC_QUERY
			&& !OutRequest.filter.HasNodePredicates())
		{
			OutError = TEXT("query requires at least one of --key, --node, --kind, or --source.");
			return false;
		}
		return true;
	}

	void ExecuteAngelscriptASTDiagnostics(const TArray<FString>& Args)
	{
		if (!FAngelscriptEngine::IsInitialized())
		{
			UE_LOG(LogAngelscriptASTDiagnostics, Error,
				TEXT("Cannot inspect canonical AST snapshots because no global engine is initialized."));
			return;
		}
		FString Output;
		FString Error;
		const int32 Status = FAngelscriptASTDiagnostics::Run(
			FAngelscriptEngine::Get().GetScriptEngine(), Args, Output, Error);
		if (Status != asSUCCESS)
		{
			UE_LOG(LogAngelscriptASTDiagnostics, Error, TEXT("%s"), *Error);
			if (!Output.IsEmpty()) UE_LOG(LogAngelscriptASTDiagnostics, Error, TEXT("%s"), *Output);
			return;
		}
		UE_LOG(LogAngelscriptASTDiagnostics, Display, TEXT("%s"), *Output);
	}

	FAutoConsoleCommand GAngelscriptASTDiagnosticsCommand(
		FAngelscriptASTDiagnostics::GetCommandName(),
		TEXT("Read-only retained canonical AST diagnostics. as.AST list|dump <module>|verify <module>|query <module> <filters>|diff <left> <right> [--json] [--key=] [--node=D1] [--kind=] [--source=] [--snippets]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ExecuteAngelscriptASTDiagnostics));
}

const TCHAR* FAngelscriptASTDiagnostics::GetCommandName()
{
	return TEXT("as.AST");
}

int32 FAngelscriptASTDiagnostics::Run(
	asIScriptEngine* ScriptEngine,
	const TArray<FString>& Args,
	FString& OutOutput,
	FString& OutError)
{
	OutOutput.Reset();
	OutError.Reset();
	asSASTDiagnosticRequest Request;
	if (!ParseDiagnosticRequest(Args, Request, OutError))
	{
		return asINVALID_ARG;
	}
	asCString Output;
	const int32 Status = asCASTRunDiagnostics(ScriptEngine, Request, Output);
	OutOutput = UTF8_TO_TCHAR(Output.AddressOf() != nullptr ? Output.AddressOf() : "");
	if (Status != asSUCCESS)
	{
		OutError = FString::Printf(TEXT("Read-only AST diagnostics failed with status %d."), Status);
	}
	return Status;
}
