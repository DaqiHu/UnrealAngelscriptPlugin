#ifndef AS_RUNTIME_TYPE_BRIDGE_H
#define AS_RUNTIME_TYPE_BRIDGE_H

#include "as_ast_context.h"
#include "as_array.h"
#include "as_datatype.h"

BEGIN_AS_NAMESPACE

class asCScriptEngine;
class asCTypeInfo;
class asCModule;
class asCScriptFunction;

// Deterministic result codes for binding a live VM function to an exact
// declaration in a sealed canonical AST. Keep these distinct so diagnostics
// and tests can distinguish a stale signature from an ambiguous identity.
enum asERuntimeDeclBindResult
{
	asRUNTIME_DECL_BIND_OK = 0,
	asRUNTIME_DECL_BIND_MISSING_KEY = -1,
	asRUNTIME_DECL_BIND_NOT_FOUND = -2,
	asRUNTIME_DECL_BIND_AMBIGUOUS = -3,
	asRUNTIME_DECL_BIND_SIGNATURE_MISMATCH = -4,
	asRUNTIME_DECL_BIND_INVALID = -5
};

class ANGELSCRIPTRUNTIME_API asCRuntimeTypeBridge
{
public:
	explicit asCRuntimeTypeBridge(asCScriptEngine* engine);
	// A CodeGen artifact may construct script types before publishing them to
	// engine/module lookup tables. The bridge can resolve those candidates for
	// the duration of one emission pass without exposing them to other users.
	void SetTransientTypes(const asCArray<asCTypeInfo*>* types);

	asCDataType Resolve(const asCASTContext& context, const asCQualType& qualType) const;
	// A retained Cache V2 AST is address-free. Before a later CodeGen/native
	// consumer can trust it, each named AST type must bind to the current Engine
	// (or this emission pass's explicitly supplied transient types).
	bool ValidateContextTypes(const asCASTContext& context, asCString* unresolvedStableKey = 0) const;
	// Cache V2 materializes VM function skeletons independently from the
	// address-free AST DTO. A retained source-owned canonical declaration must
	// therefore bind exactly once to that target-Engine skeleton before the
	// module can publish.
	bool ValidateContextDeclarations(
		const asCASTContext& context,
		const asCModule& module,
		asCString* unresolvedStableKey = 0) const;
	// Cache V2 publication variant. Validation is atomic: no Runtime function is
	// modified unless the complete sealed declaration graph binds one-to-one.
	// On success every source-owned function carries its exact stable Decl key.
	bool ValidateAndBindContextDeclarations(
		const asCASTContext& context,
		asCModule& module,
		asCString* unresolvedStableKey = 0) const;
	// Resolve only the producer-carried stable declaration identity. A unique
	// key is necessary but not sufficient: the selected declaration must also
	// agree structurally with the live function's owner, namespace, return type,
	// parameter types/directions, constness, and lambda captures.
	int BindFunctionDeclaration(
		const asCASTContext& context,
		const asCScriptFunction& function,
		asASTDeclId& outDeclaration,
		asCString* detail = 0) const;
	// Source identity keeps `T`/`int` by-value parameters exactly as written,
	// while maintained script-function Runtime shells freeze value objects as
	// `const T&inout` and primitive values as const. Canonical producers and
	// structural binders must share this lowering instead of independently
	// reconstructing Builder's ABI.
	static void NormalizeScriptParameterABI(
		asCDataType& type,
		asETypeModifiers& passing);
	// Recover the source-facing QualType of a Runtime function parameter. Live
	// script shells normalize primitive values to const and value objects to
	// const T&inout; those ABI-only facts must not leak into Canonical identity.
	// Explicit reference directions remain exact qualifier bits.
	asCQualType FromScriptParameterABI(
		asCASTContext& context,
		const asCDataType& dataType,
		asETypeModifiers passing) const;
	asCQualType FromDataType(asCASTContext& context, const asCDataType& dataType) const;

private:
	asCScriptEngine* engine;
	const asCArray<asCTypeInfo*>* transientTypes;
};

END_AS_NAMESPACE

#endif
