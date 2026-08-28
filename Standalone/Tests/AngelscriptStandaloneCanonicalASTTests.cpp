#include "angelscript.h"
#include "as_ast_diagnostics.h"
#include "as_ast_dump.h"
#include "as_ast_public_view.h"
#include "as_module.h"
#include "as_scriptengine.h"
#include "as_string.h"

#include <cstring>
#include <iostream>
#include <string>

namespace
{
	void MessageCallback(const asSMessageInfo* Message, void*)
	{
		if (Message == nullptr)
		{
			return;
		}
		std::cerr
			<< (Message->section != nullptr ? Message->section : "<unknown>")
			<< ':' << Message->row << ':' << Message->col
			<< ": "
			<< (Message->message != nullptr ? Message->message : "<no message>")
			<< '\n';
	}

	bool Require(bool bCondition, const char* Message)
	{
		if (!bCondition)
		{
			std::cerr << Message << '\n';
		}
		return bCondition;
	}

	int ExecuteInt(asIScriptEngine* Engine, asIScriptModule* Module, const char* Declaration)
	{
		asIScriptFunction* Function = Module != nullptr
			? Module->GetFunctionByDecl(Declaration)
			: nullptr;
		asIScriptContext* Context = Engine != nullptr ? Engine->CreateContext() : nullptr;
		int Value = 0;
		if (Function != nullptr && Context != nullptr
			&& Context->Prepare(Function) >= 0
			&& Context->Execute() == asEXECUTION_FINISHED)
		{
			Value = static_cast<int>(Context->GetReturnDWord());
		}
		if (Context != nullptr)
		{
			Context->Release();
		}
		return Value;
	}

	bool SameCString(const asCString& Left, const asCString& Right)
	{
		const char* LeftText = Left.AddressOf() != nullptr ? Left.AddressOf() : "";
		const char* RightText = Right.AddressOf() != nullptr ? Right.AddressOf() : "";
		return std::strcmp(LeftText, RightText) == 0;
	}
}

int main()
{
	asIScriptEngine* Engine = asCreateScriptEngine();
	if (!Require(Engine != nullptr, "asCreateScriptEngine returned null"))
	{
		return 1;
	}
	Engine->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);

	bool bPassed = true;
	asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine);
	bPassed &= Require(
		ScriptEngine->GetCompilerPipeline() == asCOMPILER_PIPELINE_LEGACY,
		"standalone engines must default to LEGACY until sealed-AST CodeGen publishes production Bytecode");
	bPassed &= Require(
		!ScriptEngine->IsCanonicalBytecodeCodeGenReady(),
		"a LEGACY-default standalone Engine must not advertise canonical CodeGen readiness");
	bPassed &= Require(
		!ScriptEngine->SetCompilerPipeline(static_cast<asECompilerPipeline>(2)),
		"standalone must reject dual/unknown compiler selection");
	bPassed &= Require(
		ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
		"standalone must be able to explicitly select canonical CodeGen");
	bPassed &= Require(
		ScriptEngine->IsCanonicalBytecodeCodeGenReady(),
		"a CANONICAL-selected standalone Engine must report canonical CodeGen readiness");
	asIScriptModule* DiscardModule = Engine->GetModule("canonical-discard", asGM_ALWAYS_CREATE);
	const char* Source = "int main() { return 42; }";
	bPassed &= Require(DiscardModule != nullptr, "discard module is missing");
	bPassed &= Require(
		DiscardModule != nullptr
			&& DiscardModule->GetASTRetentionPolicy() == asAST_DISCARD_AFTER_CODEGEN,
		"default AST policy must discard after codegen");
	bPassed &= Require(
		DiscardModule != nullptr
			&& DiscardModule->AddScriptSection("canonical-discard.as", Source) >= 0
			&& DiscardModule->Build() >= 0,
		"canonical discard Build failed");
	bPassed &= Require(
		DiscardModule != nullptr
			&& DiscardModule->AcquireASTSnapshot(asAST_API_VERSION_1) == nullptr,
		"discard policy must not publish a public AST snapshot");
	bPassed &= Require(
		ExecuteInt(Engine, DiscardModule, "int main()") == 42,
		"canonical discard module returned the wrong result");

	asIScriptModule* RetainModule = Engine->GetModule("canonical-retain", asGM_ALWAYS_CREATE);
	bPassed &= Require(RetainModule != nullptr, "retain module is missing");
	bPassed &= Require(
		RetainModule != nullptr
			&& RetainModule->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT) == 0,
		"retain policy could not be selected before Build");
	bPassed &= Require(
		RetainModule != nullptr
			&& RetainModule->AddScriptSection("canonical-retain.as", Source) >= 0
			&& RetainModule->Build() >= 0,
		"canonical retain Build failed");
	asIASTSnapshot* Snapshot = RetainModule != nullptr
		? RetainModule->AcquireASTSnapshot(asAST_API_VERSION_1)
		: nullptr;
	bPassed &= Require(Snapshot != nullptr, "retain policy must publish AST V1");
	if (Snapshot != nullptr)
	{
		asSASTDeclView View = {};
		View.structSize = sizeof(View);
		bPassed &= Require(
			Snapshot->GetAPIVersion() == asAST_API_VERSION_1,
			"public AST API version is not V1");
		bPassed &= Require(
			Snapshot->IsCurrentGeneration(),
			"fresh retain snapshot must be current");
		bPassed &= Require(
			Snapshot->GetTranslationUnitDecl().IsValid(),
			"retain snapshot has no translation-unit id");
		bPassed &= Require(
			Snapshot->GetDecl(Snapshot->GetTranslationUnitDecl(), &View) == 0
				&& View.apiVersion == asAST_API_VERSION_1
				&& View.structSize != 0,
			"public V1 decl view failed");
		bPassed &= Require(
			RetainModule->AcquireASTSnapshot(99) == nullptr,
			"unsupported AST API version must fail closed");

		asCASTSnapshot* InternalSnapshot = static_cast<asCASTSnapshot*>(Snapshot);
		const asDWORD ReferenceCountBeforeDiagnostics =
			InternalSnapshot->GetReferenceCountForDiagnostics();
		asSASTDiagnosticRequest ListRequest;
		ListRequest.operation = asAST_DIAGNOSTIC_LIST;
		asCString ListOutput;
		bPassed &= Require(
			asCASTRunDiagnostics(Engine, ListRequest, ListOutput) == asSUCCESS
				&& std::strstr(ListOutput.AddressOf(), "module=\"canonical-retain\"") != nullptr
				&& std::strstr(ListOutput.AddressOf(), "provenance=source-build") != nullptr
				&& std::strstr(ListOutput.AddressOf(), "lease=acquired-released") != nullptr,
			"standalone list diagnostics must expose retained snapshot metadata");

		asSASTDiagnosticRequest QueryRequest;
		QueryRequest.operation = asAST_DIAGNOSTIC_QUERY;
		QueryRequest.moduleName = "canonical-retain";
		QueryRequest.filter.stableDeclKey = "main()";
		asCString QueryOutput;
		bPassed &= Require(
			asCASTRunDiagnostics(Engine, QueryRequest, QueryOutput) == asSUCCESS
				&& std::strstr(QueryOutput.AddressOf(), "matchCount=1") != nullptr
				&& std::strstr(QueryOutput.AddressOf(), "cacheInput=false") != nullptr,
			"standalone query diagnostics must return exact diagnostic-only matches");
		bPassed &= Require(
			InternalSnapshot->GetReferenceCountForDiagnostics()
				== ReferenceCountBeforeDiagnostics,
			"standalone diagnostics must release every acquired snapshot lease");
	}

	const asCASTContext* Context = Snapshot != nullptr
		? static_cast<asCASTSnapshot*>(Snapshot)->GetContext()
		: nullptr;
	bPassed &= Require(Context != nullptr && Context->IsPublishable(),
		"retain snapshot must expose a Frozen/Publishable canonical AST");
	if (Context != nullptr)
	{
		asCString FirstDump;
		asCString SecondDump;
		asCASTDump(*Context, FirstDump);
		asCASTDump(*Context, SecondDump);
		bPassed &= Require(
			FirstDump.GetLength() > 0 && SameCString(FirstDump, SecondDump),
			"canonical AST dump must be deterministic");
		bPassed &= Require(
			std::strstr(FirstDump.AddressOf() != nullptr ? FirstDump.AddressOf() : "", "0x")
				== nullptr,
			"canonical AST dump must not contain pointer addresses");
	}

	asIScriptFunction* Extra = nullptr;
	bPassed &= Require(
		RetainModule != nullptr
			&& RetainModule->CompileFunction(
				"extra.as",
				"int Extra() { return 11; }",
				0,
			asCOMP_ADD_TO_MODULE,
			&Extra) >= 0
			&& Extra != nullptr,
		"CompileFunction ADD_TO_MODULE must compile");
	bPassed &= Require(
		ExecuteInt(Engine, RetainModule, "int Extra()") == 11,
		"CompileFunction result did not execute");
	asIASTSnapshot* AfterCompileFunction = RetainModule != nullptr
		? RetainModule->AcquireASTSnapshot(asAST_API_VERSION_1)
		: nullptr;
	bPassed &= Require(
		Snapshot != nullptr && !Snapshot->IsCurrentGeneration(),
		"CompileFunction ADD_TO_MODULE must retire the retained snapshot it cannot complete");
	if (Snapshot != nullptr)
	{
		asSASTDeclView RetiredView = {};
		RetiredView.structSize = sizeof(RetiredView);
		bPassed &= Require(
			Snapshot->GetDecl(Snapshot->GetTranslationUnitDecl(), &RetiredView) == 0,
			"retired snapshot leases must remain traversable until their caller releases them");
		Snapshot->Release();
	}
	bPassed &= Require(
		AfterCompileFunction == nullptr,
		"CompileFunction ADD_TO_MODULE must not publish a stale retained snapshot");
	if (AfterCompileFunction != nullptr)
	{
		AfterCompileFunction->Release();
	}
	if (Extra != nullptr)
	{
		Extra->Release();
	}

	Engine->ShutDownAndRelease();
	return bPassed ? 0 : 1;
}
