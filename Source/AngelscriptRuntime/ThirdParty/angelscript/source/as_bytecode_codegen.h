#ifndef AS_BYTECODE_CODEGEN_H
#define AS_BYTECODE_CODEGEN_H

#include "as_ast_context.h"
#include "as_runtime_type_binding.h"
#include "as_string.h"

BEGIN_AS_NAMESPACE

class asCScriptFunction;
class asCModule;
class asCBuilder;

#if WITH_ANGELSCRIPT_UNITTESTS
// Internal test-only Commit boundaries. Values are intentionally stable so
// focused transaction tests can name the publication edge they exercise.
enum asEBytecodeCodeGenTestCommitStage
{
	asBYTECODE_CODEGEN_TEST_COMMIT_AFTER_TYPES,
	asBYTECODE_CODEGEN_TEST_COMMIT_AFTER_GLOBALS,
	asBYTECODE_CODEGEN_TEST_COMMIT_AFTER_IMPORTS,
	asBYTECODE_CODEGEN_TEST_COMMIT_AFTER_FUNCTIONS,
};
#endif

class ANGELSCRIPTRUNTIME_API asCBytecodeCodeGen
{
public:
	asCBytecodeCodeGen();

	// Accepts only a sealed verified AST. Failure must not mutate functions, globals, funcdefs, or types.
	int Generate(const asCASTContext& context, asCModule* module);
	// CompileFunction entry point. The sealed context must contain exactly one
	// direct source function (nested lambda declarations are allowed). Attached
	// publication commits the complete emitted function closure to the module;
	// detached publication installs executable Engine ids without changing the
	// module inventory. On success OutFunction carries one temporary internal
	// reference owned by the caller in both modes.
	int GenerateFunction(
		const asCASTContext& context,
		asCModule* module,
		bool addToModule,
		asCScriptFunction** outFunction);
	// UE's staged compiler has already registered and laid out the module's
	// Runtime type/function/global shells before body generation. Lower the
	// sealed graph into those exact objects without replacing their identity.
	// Publication is transactional: failure leaves every prepared shell in its
	// pre-CodeGen state and success records the canonical publisher/digest.
	int GeneratePreparedModule(
		const asCASTContext& context,
		asCBuilder& preparedBuilder);
	bool Failed() const { return failed; }
	int GetError() const { return error; }
	int GetLastFailLine() const { return lastFailLine; }
	const asCString& GetFailureSection() const { return failureSection; }
	int GetFailureRow() const { return failureRow; }
	int GetFailureColumn() const { return failureColumn; }
	// Stable human-readable reason for the first terminal Generate failure.
	// This is intentionally diagnostic-only; callers must still branch on GetError().
	const asCString& GetFailureDetail() const { return failureDetail; }

#if WITH_ANGELSCRIPT_UNITTESTS
	// Test-only deterministic failure boundary. It fires before the requested
	// function's body emission, after the candidate artifact has prepared the
	// preceding declarations. This keeps rollback tests independent from the
	// temporary set of language forms that happen to be unsupported by CodeGen.
	void SetTestFailureBeforeFunctionEmission(asUINT functionOrdinal, int failureCode);
	// Prepared constructor factories are emitted after authored and generated
	// function bodies. Address this boundary in its own ordinal domain so a new
	// generated accessor cannot silently retarget a factory rollback test.
	void SetTestFailureBeforePreparedFactoryEmission(
		asUINT factoryOrdinal, int failureCode);
	// Test-only deterministic publication boundary. It fires after the named
	// Commit stage has made its entries visible, so Abandon() must prove its
	// cleanup is symmetric with production publication.
	void SetTestFailureAfterCommitStage(asUINT stage, int failureCode);
	// Test-only detached-publication boundary. It fires after the complete
	// generation-local object dispatch projection has been prepared and
	// validated, but before relocations or Commit can publish candidate state.
	void SetTestFailureAfterInterfaceDispatchPreparation(int failureCode);
	bool WasTestInterfaceDispatchPreparationFailureInjected() const
	{
		return testInterfaceDispatchPreparationFailureInjected;
	}
	// Test-only deterministic relocation boundary. Resolution of the complete
	// frozen table succeeds first; the named use then fails before any candidate
	// bytecode patch or Commit publication.
	void SetTestFailureAtTypeRelocationUse(
		asERuntimeTypeRelocationUse use, int failureCode);
	bool WasTestTypeRelocationFailureInjected() const
	{
		return testTypeRelocationFailureInjected;
	}
	// Test-only prepared-authority allocation boundary. It simulates the same
	// no-growth result as asCArray::PushLast on OOM, before candidate state can
	// be consumed by the Runtime type bridge.
	void SetTestFailureBeforePreparedRuntimeTypeAppend(
		asUINT typeOrdinal, int failureCode);
	bool WasTestPreparedRuntimeTypeAppendFailureInjected() const
	{
		return testPreparedRuntimeTypeAppendFailureInjected;
	}
	// Test-only prepared-global transaction boundary. It fires before the
	// rollback record is appended, so no semantic flag may be made visible yet.
	void SetTestFailureBeforePreparedGlobalStateRegistration(
		asUINT globalOrdinal, int failureCode);
	bool WasTestPreparedGlobalStateRegistrationFailureInjected() const
	{
		return testPreparedGlobalStateRegistrationFailureInjected;
	}
#endif

private:
	int GenerateInternal(
		const asCASTContext& context,
		asCModule* module,
		bool singleFunction,
		bool addToModule,
		asCScriptFunction** outFunction);
	bool failed;
	int  error;
	int  lastFailLine;
	asCString failureDetail;
	asCString failureSection;
	int failureRow;
	int failureColumn;
#if WITH_ANGELSCRIPT_UNITTESTS
	asUINT testFailureBeforeFunctionEmission;
	int testFailureCode;
	asUINT testFailureBeforePreparedFactoryEmission;
	int testPreparedFactoryFailureCode;
	asUINT testFailureAfterCommitStage;
	int testCommitFailureCode;
	bool testFailureAfterInterfaceDispatchPreparation;
	int testInterfaceDispatchPreparationFailureCode;
	bool testInterfaceDispatchPreparationFailureInjected;
	asUINT testFailureAtTypeRelocationUse;
	int testTypeRelocationFailureCode;
	bool testTypeRelocationFailureInjected;
	asUINT testFailureBeforePreparedRuntimeTypeAppend;
	int testPreparedRuntimeTypeAppendFailureCode;
	bool testPreparedRuntimeTypeAppendFailureInjected;
	asUINT testFailureBeforePreparedGlobalStateRegistration;
	int testPreparedGlobalStateRegistrationFailureCode;
	bool testPreparedGlobalStateRegistrationFailureInjected;
#endif
};

ANGELSCRIPTRUNTIME_API void asCBytecodeCodeGenDumpFunction(const asCScriptFunction* func, asCString& out);
ANGELSCRIPTRUNTIME_API void asCBytecodeCodeGenDumpModule(const asCModule* module, asCString& out);

END_AS_NAMESPACE

#endif
