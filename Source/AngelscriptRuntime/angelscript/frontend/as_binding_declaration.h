/* Detached host declaration parsing using the language frontend. */
#pragma once

#include "frontend/as_type_context.h"
#include "frontend/as_source_manager.h"
#include "Templates/Function.h"

BEGIN_AS_NAMESPACE

struct asSBindingDeclarationContext
{
	FString StableScope;
	FString Namespace;
	asSStableKey Owner;
	asEFunctionIdentityKind FunctionKind = asEFunctionIdentityKind::Global;
	asEFunctionDefinitionModifiers DefinitionModifiers = asEFunctionDefinitionModifiers::None;
};

struct asSBindingParameter
{
	asSStableParameter Identity;
	FString Name;
	FString DefaultText;
	asCSourceRange Range;
};

struct asSBindingDeclaration
{
	TSharedPtr<asCSourceManager, ESPMode::ThreadSafe> Source;
	asSStableKey Type;
	asSStableKey Function;
	asSFunctionIdentityDescriptor Signature;
	FString Name;
	TArray<asSBindingParameter> Parameters;
	asCSourceRange Range;
};

struct asSBindingDeclarationDiagnostic
{
	FString Message;
	asCSourceRange Range;
};

// Lookup receives authored qualified spelling (including a leading ::) and
// the current namespace. It must return a nominal identity owned by Context.
using asFBindingNominalLookup = TFunctionRef<asSStableKey(const FString&, const FString&)>;

class ANGELSCRIPTRUNTIME_API asCBindingDeclarationParser final
{
public:
	static bool Parse(const FString& Text, const asSBindingDeclarationContext& DeclarationContext,
		asCTypeContext& Context, asFBindingNominalLookup Lookup,
		asSBindingDeclaration& Out, asSBindingDeclarationDiagnostic& Diagnostic);
};

END_AS_NAMESPACE
