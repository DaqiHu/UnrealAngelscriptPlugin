#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/AST/as_ast_context.h"
#include "as_runtime_type_binding.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalTypeRelocationTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.TypeRelocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	struct FNamespacedCarrier
	{
		int32 Value = 0;
	};

	static void GenericReadNoArgs(asIScriptGeneric* Generic)
	{
		const FNamespacedCarrier* const Carrier =
			static_cast<const FNamespacedCarrier*>(Generic->GetObject());
		Generic->SetReturnDWord(Carrier ? static_cast<asDWORD>(Carrier->Value) : 0);
	}

	static void GenericReadOneArg(asIScriptGeneric* Generic)
	{
		const FNamespacedCarrier* const Carrier =
			static_cast<const FNamespacedCarrier*>(Generic->GetObject());
		const int32 Addend = static_cast<int32>(Generic->GetArgDWord(0));
		Generic->SetReturnDWord(
			Carrier ? static_cast<asDWORD>(Carrier->Value + Addend) : 0);
	}

	static asSRuntimeTypeBindingEnvironment MakeEnvironment()
	{
		asSRuntimeTypeBindingEnvironment Environment;
		Environment.targetProfileKey = "Win64.Development";
		Environment.nativeEnvironmentKey = "UE5.8-MSVC14.44";
		return Environment;
	}

	TEST_METHOD(NamespacedPropertyRelocationUsesCompleteOwnerIdentity)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetDefaultNamespace("RelocNS")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"CRelocCarrier", 0, asOBJ_REF | asOBJ_NOCOUNT) >= 0
			&& ScriptEngine->RegisterObjectProperty(
				"CRelocCarrier", "int Value",
				asOFFSET(FNamespacedCarrier, Value)) >= 0,
			TEXT("namespaced property relocation fixture must register")));
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetDefaultNamespace("")));

		asCASTContext Context;
		const asCQualType CanonicalType = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT,
			"RelocNS::CRelocCarrier",
			0);
		const asSRuntimeTypeBindingEnvironment Environment = MakeEnvironment();
		asSTypeABIKey Expected;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_OK),
			static_cast<int32>(asCRuntimeTypeBindingTable::CaptureExpectedABI(
				ScriptEngine, Context, CanonicalType, Environment, Expected)),
			TEXT("namespaced owner ABI must be capturable")));

		asSTypeRelocation Relocation;
		Relocation.expectedType = Expected;
		Relocation.stableMemberKey = "RelocNS::CRelocCarrier::Value";
		Relocation.use = asTYPE_RELOC_PROPERTY;
		asCArray<asSTypeRelocation> Relocations;
		Relocations.PushLast(Relocation);

		asCRuntimeTypeBindingTable Table;
		asCString Detail;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_OK),
			static_cast<int32>(Table.Build(
				ScriptEngine, Context, Relocations, Environment, nullptr, &Detail)),
			*FString::Printf(
				TEXT("complete namespaced property key must resolve: %s"),
				UTF8_TO_TCHAR(Detail.AddressOf() ? Detail.AddressOf() : ""))));
		const asSResolvedRuntimeTypeBinding* const Binding = Table.GetBinding(0);
		ASSERT_THAT(IsTrue(Binding != nullptr
			&& Binding->property != nullptr
			&& Binding->propertyOffset == asOFFSET(FNamespacedCarrier, Value),
			TEXT("relocation must bind the exact property owned by the complete type key")));
	}

	TEST_METHOD(OverloadedFunctionRelocationRequiresCompleteDeclaration)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetDefaultNamespace("RelocNS")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"CRelocCarrier", 0, asOBJ_REF | asOBJ_NOCOUNT) >= 0
			&& ScriptEngine->RegisterObjectMethod(
				"CRelocCarrier", "int Read() const",
				asFUNCTION(GenericReadNoArgs), asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectMethod(
				"CRelocCarrier", "int Read(int Addend) const",
				asFUNCTION(GenericReadOneArg), asCALL_GENERIC) >= 0,
			TEXT("overloaded function relocation fixture must register")));
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetDefaultNamespace("")));

		asCASTContext Context;
		const asCQualType CanonicalType = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT,
			"RelocNS::CRelocCarrier",
			0);
		const asSRuntimeTypeBindingEnvironment Environment = MakeEnvironment();
		asSTypeABIKey Expected;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_OK),
			static_cast<int32>(asCRuntimeTypeBindingTable::CaptureExpectedABI(
				ScriptEngine, Context, CanonicalType, Environment, Expected))));

		asSTypeRelocation Ambiguous;
		Ambiguous.expectedType = Expected;
		Ambiguous.stableMemberKey = "RelocNS::CRelocCarrier::Read";
		Ambiguous.use = asTYPE_RELOC_FUNCTION;
		asCArray<asSTypeRelocation> AmbiguousRelocations;
		AmbiguousRelocations.PushLast(Ambiguous);
		asCRuntimeTypeBindingTable AmbiguousTable;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_MEMBER_MISSING),
			static_cast<int32>(AmbiguousTable.Build(
				ScriptEngine, Context, AmbiguousRelocations, Environment)),
			TEXT("a bare overloaded member name must not select the first Runtime function")));

		asSTypeRelocation Exact = Ambiguous;
		Exact.stableMemberKey =
			"int RelocNS::CRelocCarrier::Read(int) const";
		asCArray<asSTypeRelocation> ExactRelocations;
		ExactRelocations.PushLast(Exact);
		asCRuntimeTypeBindingTable ExactTable;
		asCString Detail;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_OK),
			static_cast<int32>(ExactTable.Build(
				ScriptEngine, Context, ExactRelocations, Environment, nullptr, &Detail)),
			*FString::Printf(
				TEXT("complete overloaded declaration must resolve exactly: %s"),
				UTF8_TO_TCHAR(Detail.AddressOf() ? Detail.AddressOf() : ""))));
		const asSResolvedRuntimeTypeBinding* const Binding =
			ExactTable.GetBinding(0);
		ASSERT_THAT(IsTrue(Binding != nullptr
			&& Binding->function != nullptr
			&& Binding->function->parameterTypes.GetLength() == 1,
			TEXT("complete declaration must bind the one-parameter overload")));
	}

	TEST_METHOD(DetachedFunctionRelocationResolvesCandidateBeforeEnginePublication)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());

		asCASTContext Context;
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asSRuntimeTypeBindingEnvironment Environment = MakeEnvironment();
		asSTypeABIKey Expected;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_OK),
			static_cast<int32>(asCRuntimeTypeBindingTable::CaptureExpectedABI(
				ScriptEngine, Context, IntType, Environment, Expected))));

		asCScriptFunction* const Candidate = asNEW(asCScriptFunction)(
			ScriptEngine, nullptr, asFUNC_SCRIPT);
		ASSERT_THAT(IsNotNull(Candidate,
			TEXT("detached function fixture must allocate a candidate shell")));
		if (Candidate == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Candidate->ReleaseInternal(); };
		Candidate->name = "Detached";
		Candidate->canonicalASTStableDeclKey = "Detached(int)";
		Candidate->returnType = asCDataType::CreatePrimitive(ttInt, false);
		Candidate->parameterTypes.PushLast(
			asCDataType::CreatePrimitive(ttInt, false));
		Candidate->inOutFlags.PushLast(asTM_NONE);
		Candidate->CalculateParameterOffsets();
		ASSERT_THAT(IsTrue(ScriptEngine->GetGlobalFunctionByDecl(
			"int Detached(int)") == nullptr,
			TEXT("candidate must remain absent from Engine global-function lookup")));

		asSTypeRelocation Relocation;
		Relocation.expectedType = Expected;
		Relocation.stableMemberKey = "Detached(int)";
		Relocation.use = asTYPE_RELOC_FUNCTION;
		asCArray<asSTypeRelocation> Relocations;
		Relocations.PushLast(Relocation);
		asCArray<asCScriptFunction*> CandidateFunctions;
		CandidateFunctions.PushLast(Candidate);

		asCRuntimeTypeBindingTable Table;
		asCString Detail;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_OK),
			static_cast<int32>(Table.Build(
				ScriptEngine,
				Context,
				Relocations,
				Environment,
				nullptr,
				&Detail,
				&CandidateFunctions)),
			*FString::Printf(
				TEXT("detached stable declaration must resolve before Engine publication: %s"),
				UTF8_TO_TCHAR(Detail.AddressOf() ? Detail.AddressOf() : ""))));
		const asSResolvedRuntimeTypeBinding* const Binding = Table.GetBinding(0);
		ASSERT_THAT(IsTrue(Binding != nullptr && Binding->function == Candidate,
			TEXT("frozen relocation binding must retain the exact candidate function")));
	}
};

#endif
