#include "../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptNativeTypedSemanticIRCallMetadataTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRNativeAbiTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.CallMetadata.NativeABI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(NativeOnlyInputsRemainOutsideScriptOperandsAndResultIsConcrete)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIRCallMetadataTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-NATIVE-ABI",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
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
		const FRegisteredCallMetadataFunctions Registered =
			RegisterCallMetadataFunctions(*ScriptEngine);
		ASSERT_THAT(IsTrue(Registered.HasNativeAbiFixtures()));
		ASSERT_THAT(IsTrue(Registered.HasDeterminesOutputFixture(),
			TEXT("The determines-output type fixture must register independently")));
		if (!Registered.HasNativeAbiFixtures()
			|| !Registered.HasDeterminesOutputFixture())
		{
			return;
		}

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedSemanticIRNativeABI",
			ASTEST_AS_ANSI(R"AS(
				int ScriptFunctionMetadataEntry(int Value)
				{
					return MetadataScriptFunction(Value);
				}

				int ScriptObjectTypeMetadataEntry(int Value)
				{
					return MetadataScriptObjectType(Value);
				}

				int GenericMetadataEntry(int Value)
				{
					return MetadataGeneric(Value);
				}

				FDeterminesDerived DeterminesOutputEntry(
					FDeterminesDerived Value)
				{
					return MetadataDeterminesOutput(Value);
				}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(),
			TEXT("The determines-output fixture must compile only when the final return type is specialized")));
		if (!Module.IsValid())
		{
			return;
		}

		struct FEntryCase
		{
			const char* Declaration;
			int TargetFunctionId;
			const TCHAR* RequiredDumpMarker;
			asETypedSemanticFirstParamMetadata FirstParamMetadata;
			bool bUsesGenericContext;
		};
		const FEntryCase Cases[] = {
			{
				"int ScriptFunctionMetadataEntry(int Value)",
				Registered.ScriptFunction,
				TEXT("firstParam=ScriptFunction"),
				asETypedSemanticFirstParamMetadata::ScriptFunction,
				false
			},
			{
				"int ScriptObjectTypeMetadataEntry(int Value)",
				Registered.ScriptObjectType,
				TEXT("firstParam=ScriptObjectType"),
				asETypedSemanticFirstParamMetadata::ScriptObjectType,
				false
			},
			{
				"int GenericMetadataEntry(int Value)",
				Registered.GenericWithUserData,
				TEXT("genericContext=true"),
				asETypedSemanticFirstParamMetadata::None,
				true
			},
		};

		for (const FEntryCase& Case : Cases)
		{
			asCScriptFunction* const Entry =
				static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
					Module, Case.Declaration));
			const asCTypedSemanticFunction* const HIR = Entry != nullptr
				? Entry->GetTypedSemanticFunction() : nullptr;
			ASSERT_THAT(IsNotNull(HIR));
			if (HIR == nullptr)
			{
				continue;
			}
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
			const asSTypedSemanticExpression* const Call =
				FindResolvedCall(*HIR, Case.TargetFunctionId);
			ASSERT_THAT(IsNotNull(Call));
			if (Call != nullptr)
			{
				ASSERT_THAT(AreEqual(1,
					static_cast<int32>(Call->operands.GetLength()),
					TEXT("ABI-only values must not be inserted into AS operands")));
				ASSERT_THAT(AreEqual(1,
					static_cast<int32>(Call->formalParameterIndices.GetLength()),
					TEXT("ABI-only values must not become declared formals")));
				ASSERT_THAT(AreEqual(1,
					static_cast<int32>(Call->argumentOrigins.GetLength())));
				ASSERT_THAT(IsTrue(Call->argumentOrigins[0]
					== asETypedSemanticArgumentOrigin::SourcePositional));
				ASSERT_THAT(IsTrue(Call->nativeCall.isSystemFunction));
				ASSERT_THAT(IsTrue(Call->nativeCall.firstParamMetadata
					== Case.FirstParamMetadata));
				ASSERT_THAT(AreEqual(
					Case.bUsesGenericContext,
					Call->nativeCall.usesGenericContext));
			}
			const FString Dump = UTF8_TO_TCHAR(
				DumpTypedSemanticFunction(*HIR).AddressOf());
			ASSERT_THAT(IsTrue(Dump.Contains(Case.RequiredDumpMarker)));
			ASSERT_THAT(IsTrue(Dump.Contains(
				TEXT("origins=[SourcePositional]"))));
			if (Case.TargetFunctionId == Registered.GenericWithUserData)
			{
				ASSERT_THAT(IsTrue(Call != nullptr
					&& Call->nativeCall.hasFunctionUserData));
				ASSERT_THAT(IsTrue(Dump.Contains(TEXT("userData=true"))));
			}
		}

		asCScriptFunction* const DeterminesEntry =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				Module,
				"FDeterminesDerived DeterminesOutputEntry(FDeterminesDerived Value)"));
		const asCTypedSemanticFunction* const DeterminesHIR =
			DeterminesEntry != nullptr
				? DeterminesEntry->GetTypedSemanticFunction() : nullptr;
		ASSERT_THAT(IsNotNull(DeterminesHIR));
		if (DeterminesHIR != nullptr)
		{
			const asSTypedSemanticExpression* const DeterminesCall =
				FindResolvedCall(
					*DeterminesHIR,
					Registered.DeterminesOutputType);
			ASSERT_THAT(IsNotNull(DeterminesCall));
			ASSERT_THAT(IsTrue(DeterminesCall != nullptr
				&& DeterminesCall->type.GetTypeInfo()
					== Registered.DeterminesDerivedType,
				TEXT("The call HIR must preserve the compiler-specialized concrete return type")));
			ASSERT_THAT(IsTrue(DeterminesCall != nullptr
				&& DeterminesCall->determinesOutputTypeArgumentIndex == 0));
			ASSERT_THAT(IsTrue(FString(UTF8_TO_TCHAR(
				DumpTypedSemanticFunction(*DeterminesHIR).AddressOf()))
				.Contains(TEXT("determinesOutputType=0"))));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
