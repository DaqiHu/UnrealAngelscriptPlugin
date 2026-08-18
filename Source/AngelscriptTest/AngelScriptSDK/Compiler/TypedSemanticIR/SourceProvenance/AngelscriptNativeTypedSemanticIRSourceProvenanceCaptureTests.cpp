#include "../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace TypedSemanticIRSourceProvenanceCaptureTests
{
	static const asSTypedSemanticExpression* FindLiteral(
		const asCTypedSemanticFunction& Function,
		const char* Literal)
	{
		for (asUINT Index = 0; Index < Function.expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression =
				Function.expressions[Index];
			if (Expression.kind == asETypedSemanticExpressionKind::Literal
				&& Expression.literal == Literal)
			{
				return &Expression;
			}
		}
		return nullptr;
	}

	static const asSTypedSemanticSymbol* FindParameter(
		const asCTypedSemanticFunction& Function,
		const char* Name)
	{
		for (asUINT Index = 0; Index < Function.symbols.GetLength(); ++Index)
		{
			const asSTypedSemanticSymbol& Symbol = Function.symbols[Index];
			if (Symbol.kind == asETypedSemanticSymbolKind::Parameter
				&& Symbol.name == Name)
			{
				return &Symbol;
			}
		}
		return nullptr;
	}
}

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRSourceProvenanceCaptureTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.SourceProvenance.Capture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ScriptSectionRangesReachOnlyTheContainedHIRNodes)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIRSourceProvenanceCaptureTests;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-SOURCE-PROVENANCE-CAPTURE",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ScriptEngine->SetTypedSemanticIRCapture(true)));

		const std::string Source =
			"int AuthoredValue() { return 11; }\n"
			"int GeneratedValue() { return 29; }\n";
		const size_t AuthoredStart = Source.find("int AuthoredValue");
		const size_t GeneratedStart = Source.find("int GeneratedValue");
		ASSERT_THAT(IsTrue(AuthoredStart != std::string::npos));
		ASSERT_THAT(IsTrue(GeneratedStart != std::string::npos));

		asSTypedSemanticSourceProvenanceRange Ranges[2];
		Ranges[0].processedOffset = asUINT(AuthoredStart);
		Ranges[0].processedLength = asUINT(GeneratedStart - AuthoredStart);
		Ranges[0].hasAuthoredOrigin = true;
		Ranges[0].authoredOrigin.section = "AuthoredSource.as";
		Ranges[0].authoredOrigin.sourceOffset = 5;
		Ranges[0].authoredOrigin.sourceLength = 37;
		Ranges[0].authoredOrigin.row = 4;
		Ranges[0].authoredOrigin.column = 2;

		Ranges[1].processedOffset = asUINT(GeneratedStart);
		Ranges[1].processedLength = asUINT(Source.size() - GeneratedStart);
		Ranges[1].hasGeneratedOrigin = true;
		Ranges[1].generatedOrigin.kind =
			asETypedSemanticGeneratedSourceKind::SubsystemAccessor;
		Ranges[1].generatedOrigin.generatorName =
			"SubsystemAccessor:USourceProvenanceSubsystem";
		Ranges[1].generatedOrigin.hasAuthoredAnchor = true;
		Ranges[1].generatedOrigin.authoredAnchor.section = "AuthoredSource.as";
		Ranges[1].generatedOrigin.authoredAnchor.sourceOffset = 48;
		Ranges[1].generatedOrigin.authoredAnchor.sourceLength = 31;
		Ranges[1].generatedOrigin.authoredAnchor.row = 8;
		Ranges[1].generatedOrigin.authoredAnchor.column = 1;

		asCModule* const Module = static_cast<asCModule*>(
			ScriptEngine->GetModule("SourceProvenanceCapture", asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Module->AddScriptSectionWithSourceProvenance(
			"ProcessedSource.as",
			Source.c_str(),
			Source.size(),
			0,
			Ranges,
			2) >= 0));
		ASSERT_THAT(IsTrue(Module->Build() >= 0));

		asCScriptFunction* const AuthoredFunction =
			static_cast<asCScriptFunction*>(Module->GetFunctionByDecl(
				"int AuthoredValue()"));
		asCScriptFunction* const GeneratedFunction =
			static_cast<asCScriptFunction*>(Module->GetFunctionByDecl(
				"int GeneratedValue()"));
		ASSERT_THAT(IsNotNull(AuthoredFunction));
		ASSERT_THAT(IsNotNull(GeneratedFunction));
		if (AuthoredFunction == nullptr || GeneratedFunction == nullptr)
		{
			return;
		}

		const asCTypedSemanticFunction* const AuthoredHIR =
			AuthoredFunction->GetTypedSemanticFunction();
		const asCTypedSemanticFunction* const GeneratedHIR =
			GeneratedFunction->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(AuthoredHIR));
		ASSERT_THAT(IsNotNull(GeneratedHIR));
		if (AuthoredHIR == nullptr || GeneratedHIR == nullptr)
		{
			return;
		}

		const asSTypedSemanticExpression* const AuthoredLiteral =
			FindLiteral(*AuthoredHIR, "11");
		const asSTypedSemanticExpression* const GeneratedLiteral =
			FindLiteral(*GeneratedHIR, "29");
		ASSERT_THAT(IsNotNull(AuthoredLiteral));
		ASSERT_THAT(IsNotNull(GeneratedLiteral));
		if (AuthoredLiteral == nullptr || GeneratedLiteral == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AuthoredLiteral->span.hasAuthoredOrigin));
		ASSERT_THAT(IsFalse(AuthoredLiteral->span.hasGeneratedOrigin));
		ASSERT_THAT(AreEqual(FString(TEXT("AuthoredSource.as")),
			FString(UTF8_TO_TCHAR(
				AuthoredLiteral->span.authoredOrigin.section.AddressOf()))));
		ASSERT_THAT(IsTrue(GeneratedLiteral->span.hasGeneratedOrigin));
		ASSERT_THAT(IsFalse(GeneratedLiteral->span.hasAuthoredOrigin));
		ASSERT_THAT(IsTrue(GeneratedLiteral->span.generatedOrigin.kind
			== asETypedSemanticGeneratedSourceKind::SubsystemAccessor));
		ASSERT_THAT(AreEqual(FString(TEXT("ProcessedSource.as")),
			FString(UTF8_TO_TCHAR(
				GeneratedLiteral->span.section.AddressOf()))));
	}

	TEST_METHOD(ParameterSymbolsUseTheirOwnTokenAndRetainGeneratedSignatureOrigin)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIRSourceProvenanceCaptureTests;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-PARAMETER-SOURCE-PROVENANCE",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ScriptEngine->SetTypedSemanticIRCapture(true)));

		const std::string Source =
			"void __Init_Inventory(int Inventory)\n"
			"{\n"
			"\treturn;\n"
			"}\n";
		const size_t SignatureEnd = Source.find('\n') + 1;
		const size_t ParameterStart = Source.find("Inventory)");
		ASSERT_THAT(IsTrue(SignatureEnd != std::string::npos));
		ASSERT_THAT(IsTrue(ParameterStart != std::string::npos));

		asSTypedSemanticSourceProvenanceRange Range;
		Range.processedOffset = 0;
		Range.processedLength = asUINT(SignatureEnd);
		Range.hasGeneratedOrigin = true;
		Range.generatedOrigin.kind =
			asETypedSemanticGeneratedSourceKind::LiteralAsset;
		Range.generatedOrigin.generatorName = "LiteralAsset:Inventory";
		Range.generatedOrigin.hasAuthoredAnchor = true;
		Range.generatedOrigin.authoredAnchor.section = "AuthoredAsset.as";
		Range.generatedOrigin.authoredAnchor.sourceOffset = 17;
		Range.generatedOrigin.authoredAnchor.sourceLength = 30;
		Range.generatedOrigin.authoredAnchor.row = 3;
		Range.generatedOrigin.authoredAnchor.column = 1;

		asCModule* const Module = static_cast<asCModule*>(
			ScriptEngine->GetModule(
				"ParameterSourceProvenance", asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Module->AddScriptSectionWithSourceProvenance(
			"ProcessedAsset.as",
			Source.c_str(),
			Source.size(),
			0,
			&Range,
			1) >= 0));
		ASSERT_THAT(IsTrue(Module->Build() >= 0));

		asCScriptFunction* const Function =
			static_cast<asCScriptFunction*>(Module->GetFunctionByDecl(
				"void __Init_Inventory(int Inventory)"));
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}
		const asCTypedSemanticFunction* const HIR =
			Function->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(HIR));
		if (HIR == nullptr)
		{
			return;
		}

		const asSTypedSemanticSymbol* const Parameter =
			FindParameter(*HIR, "Inventory");
		ASSERT_THAT(IsNotNull(Parameter));
		if (Parameter == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asUINT(ParameterStart), Parameter->span.sourceOffset));
		ASSERT_THAT(AreEqual(asUINT(9), Parameter->span.sourceLength));
		ASSERT_THAT(IsTrue(Parameter->span.hasGeneratedOrigin));
		ASSERT_THAT(IsTrue(Parameter->span.generatedOrigin.kind
			== asETypedSemanticGeneratedSourceKind::LiteralAsset));
		ASSERT_THAT(AreEqual(FString(TEXT("LiteralAsset:Inventory")),
			FString(UTF8_TO_TCHAR(
				Parameter->span.generatedOrigin.generatorName.AddressOf()))));
	}
};

#endif
