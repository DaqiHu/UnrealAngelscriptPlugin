#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/as_ast_context.h"
#include "as_objecttype.h"
#include "as_runtime_type_binding.h"
#include "source/as_runtime_type_bridge.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalRuntimeTypeBindingTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.RuntimeTypeBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	struct FRuntimeBindingCarrier
	{
		int32 Value = 0;
	};

	static void GenericRead(asIScriptGeneric* Generic)
	{
		const FRuntimeBindingCarrier* const Carrier =
			static_cast<const FRuntimeBindingCarrier*>(Generic->GetObject());
		Generic->SetReturnDWord(Carrier ? static_cast<asDWORD>(Carrier->Value) : 0);
	}

	static asSRuntimeTypeBindingEnvironment MakeEnvironment()
	{
		asSRuntimeTypeBindingEnvironment Environment;
		Environment.targetProfileKey = "Win64.Development";
		Environment.nativeEnvironmentKey = "UE5.8-MSVC14.44";
		return Environment;
	}

	static bool RegisterBoundValueType(asCScriptEngine* ScriptEngine)
	{
		return ScriptEngine->RegisterObjectType(
			"FBoundValue",
			sizeof(int32),
			asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE) >= 0;
	}

	TEST_METHOD(RejectsSameKeyWithWrongRuntimeKind)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterEnum("ETarget") >= 0,
			TEXT("runtime enum fixture must register")));

		asCASTContext Context;
		const asCQualType CanonicalValue = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT,
			"ETarget",
			0);
		ASSERT_THAT(IsTrue(CanonicalValue.IsValid(),
			TEXT("canonical wrong-kind fixture must intern")));

		asCRuntimeTypeBridge Bridge(ScriptEngine);
		const asCDataType Resolved = Bridge.Resolve(Context, CanonicalValue);
		ASSERT_THAT(IsNull(Resolved.GetTypeInfo(),
			TEXT("stable spelling alone must not bind an enum to a canonical value-object identity")));
	}

	TEST_METHOD(ExactBindingFreezesCurrentEngineProjection)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(RegisterBoundValueType(ScriptEngine),
			TEXT("value-type fixture must register")));

		asCASTContext Context;
		const asCQualType CanonicalType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FBoundValue", 0);
		const asSRuntimeTypeBindingEnvironment Environment = MakeEnvironment();
		asSTypeABIKey Expected;
		asCString Detail;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_OK),
			static_cast<int32>(asCRuntimeTypeBindingTable::CaptureExpectedABI(
				ScriptEngine, Context, CanonicalType, Environment, Expected, nullptr, &Detail)),
			TEXT("candidate ABI capture must succeed")));

		asSTypeRelocation Relocation;
		Relocation.expectedType = Expected;
		Relocation.use = asTYPE_RELOC_PUBLIC_TYPE_ID;
		Relocation.functionArtifactIndex = 7;
		Relocation.bytecodeDwordOffset = 13;
		asCArray<asSTypeRelocation> Relocations;
		Relocations.PushLast(Relocation);

		asCRuntimeTypeBindingTable Table;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_OK),
			static_cast<int32>(Table.Build(
				ScriptEngine, Context, Relocations, Environment, nullptr, &Detail)),
			TEXT("exact stable identity and complete ABI must bind")));
		ASSERT_THAT(IsTrue(Table.IsFrozen(),
			TEXT("successful candidate validation must freeze before publication")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1), Table.GetBindingCount(),
			TEXT("one relocation must publish one immutable binding slot")));
		const asSResolvedRuntimeTypeBinding* const Binding = Table.GetBinding(0);
		ASSERT_THAT(IsNotNull(Binding, TEXT("frozen table must expose the installed slot")));
		ASSERT_THAT(IsTrue(Binding != nullptr
			&& Binding->typeInfo == ScriptEngine->GetTypeInfoByDecl("FBoundValue"),
			TEXT("binding owns the candidate Engine's current TypeInfo projection")));
		ASSERT_THAT(IsTrue(Binding != nullptr
			&& Binding->publicTypeId == ScriptEngine->GetTypeIdByDecl("FBoundValue"),
			TEXT("numeric typeId is captured only as a generation-local projection")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_ALREADY_FROZEN),
			static_cast<int32>(Table.Build(
				ScriptEngine, Context, Relocations, Environment, nullptr, &Detail)),
			TEXT("published bindings must never be retargeted in place")));
	}

	TEST_METHOD(MissingTypeFailsWithoutPartialFreeze)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());

		asCASTContext Context;
		Context.InternNamedType(asAST_TYPE_VALUE_OBJECT, "FMissingValue", 0);
		const asSRuntimeTypeBindingEnvironment Environment = MakeEnvironment();
		asSTypeRelocation Relocation;
		Relocation.expectedType.stableTypeKey = "FMissingValue";
		Relocation.expectedType.targetProfileKey = Environment.targetProfileKey;
		Relocation.expectedType.nativeEnvironmentKey = Environment.nativeEnvironmentKey;
		Relocation.expectedType.typeKind = asAST_TYPE_VALUE_OBJECT;
		asCArray<asSTypeRelocation> Relocations;
		Relocations.PushLast(Relocation);

		asCRuntimeTypeBindingTable Table;
		asCString Detail;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_MISSING),
			static_cast<int32>(Table.Build(
				ScriptEngine, Context, Relocations, Environment, nullptr, &Detail)),
			TEXT("missing candidate type must fail closed")));
		ASSERT_THAT(IsFalse(Table.IsFrozen(),
			TEXT("failed validate-all pass must not freeze a partial table")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0), Table.GetBindingCount(),
			TEXT("failed build must commit no slots")));
	}

	TEST_METHOD(AmbiguousTransientCandidatesFailClosed)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCObjectType CandidateA(ScriptEngine);
		asCObjectType CandidateB(ScriptEngine);
		CandidateA.name = "FAmbiguousValue";
		CandidateB.name = "FAmbiguousValue";
		CandidateA.flags = asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE;
		CandidateB.flags = CandidateA.flags;
		CandidateA.size = sizeof(int32);
		CandidateB.size = sizeof(int32);
		asCArray<asCTypeInfo*> TransientTypes;
		TransientTypes.PushLast(&CandidateA);
		TransientTypes.PushLast(&CandidateB);

		asCASTContext Context;
		Context.InternNamedType(asAST_TYPE_VALUE_OBJECT, "FAmbiguousValue", 0);
		const asSRuntimeTypeBindingEnvironment Environment = MakeEnvironment();
		asSTypeRelocation Relocation;
		Relocation.expectedType.stableTypeKey = "FAmbiguousValue";
		Relocation.expectedType.targetProfileKey = Environment.targetProfileKey;
		Relocation.expectedType.nativeEnvironmentKey = Environment.nativeEnvironmentKey;
		Relocation.expectedType.typeKind = asAST_TYPE_VALUE_OBJECT;
		asCArray<asSTypeRelocation> Relocations;
		Relocations.PushLast(Relocation);

		asCRuntimeTypeBindingTable Table;
		asCString Detail;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_AMBIGUOUS),
			static_cast<int32>(Table.Build(
				ScriptEngine, Context, Relocations, Environment, &TransientTypes, &Detail)),
			TEXT("two distinct candidate pointers with one stable identity are ambiguous")));
		ASSERT_THAT(IsFalse(Table.IsFrozen(),
			TEXT("ambiguous candidates must not publish any table")));
	}

	TEST_METHOD(BindingTableRejectsSameKeyWithWrongRuntimeKind)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("EWrongKind") >= 0,
			TEXT("enum fixture must register")));

		asCASTContext Context;
		Context.InternNamedType(asAST_TYPE_VALUE_OBJECT, "EWrongKind", 0);
		const asSRuntimeTypeBindingEnvironment Environment = MakeEnvironment();
		asSTypeRelocation Relocation;
		Relocation.expectedType.stableTypeKey = "EWrongKind";
		Relocation.expectedType.targetProfileKey = Environment.targetProfileKey;
		Relocation.expectedType.nativeEnvironmentKey = Environment.nativeEnvironmentKey;
		Relocation.expectedType.typeKind = asAST_TYPE_VALUE_OBJECT;
		asCArray<asSTypeRelocation> Relocations;
		Relocations.PushLast(Relocation);

		asCRuntimeTypeBindingTable Table;
		asCString Detail;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_WRONG_KIND),
			static_cast<int32>(Table.Build(
				ScriptEngine, Context, Relocations, Environment, nullptr, &Detail)),
			TEXT("same spelling cannot override the expected canonical type kind")));
	}

	TEST_METHOD(ProfileAndNativeEnvironmentMismatchHaveDistinctResults)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(RegisterBoundValueType(ScriptEngine),
			TEXT("value-type fixture must register")));

		asCASTContext Context;
		const asCQualType CanonicalType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FBoundValue", 0);
		const asSRuntimeTypeBindingEnvironment ExpectedEnvironment = MakeEnvironment();
		asSTypeABIKey Expected;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_OK),
			static_cast<int32>(asCRuntimeTypeBindingTable::CaptureExpectedABI(
				ScriptEngine, Context, CanonicalType, ExpectedEnvironment, Expected)),
			TEXT("expected ABI capture must succeed")));
		asSTypeRelocation Relocation;
		Relocation.expectedType = Expected;
		asCArray<asSTypeRelocation> Relocations;
		Relocations.PushLast(Relocation);

		asSRuntimeTypeBindingEnvironment WrongProfile = ExpectedEnvironment;
		WrongProfile.targetProfileKey = "Linux.Development";
		asCRuntimeTypeBindingTable ProfileTable;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_WRONG_PROFILE),
			static_cast<int32>(ProfileTable.Build(
				ScriptEngine, Context, Relocations, WrongProfile)),
			TEXT("target-profile mismatch must be diagnosable before Runtime lookup")));

		asSRuntimeTypeBindingEnvironment WrongNative = ExpectedEnvironment;
		WrongNative.nativeEnvironmentKey = "UE5.8-Clang";
		asCRuntimeTypeBindingTable NativeTable;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_WRONG_NATIVE_ENVIRONMENT),
			static_cast<int32>(NativeTable.Build(
				ScriptEngine, Context, Relocations, WrongNative)),
			TEXT("native-environment mismatch must not collapse into profile mismatch")));
		ASSERT_THAT(IsFalse(ProfileTable.IsFrozen() || NativeTable.IsFrozen(),
			TEXT("environment mismatches must publish no immutable state")));
	}

	TEST_METHOD(CompleteLayoutSignatureBeatsMatchingHash)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(RegisterBoundValueType(ScriptEngine),
			TEXT("value-type fixture must register")));

		asCASTContext Context;
		const asCQualType CanonicalType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FBoundValue", 0);
		const asSRuntimeTypeBindingEnvironment Environment = MakeEnvironment();
		asSTypeABIKey Expected;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_OK),
			static_cast<int32>(asCRuntimeTypeBindingTable::CaptureExpectedABI(
				ScriptEngine, Context, CanonicalType, Environment, Expected)),
			TEXT("expected ABI capture must succeed")));
		const asQWORD OriginalHash = Expected.layoutHash;
		Expected.layoutSignature += ";property=Injected@0:type=int";
		ASSERT_THAT(IsTrue(Expected.layoutHash == OriginalHash,
			TEXT("fixture deliberately keeps the old hash to model a collision/stale checksum")));

		asSTypeRelocation Relocation;
		Relocation.expectedType = Expected;
		asCArray<asSTypeRelocation> Relocations;
		Relocations.PushLast(Relocation);
		asCRuntimeTypeBindingTable Table;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_ABI_MISMATCH),
			static_cast<int32>(Table.Build(
				ScriptEngine, Context, Relocations, Environment)),
			TEXT("equal hash cannot authorize unequal complete layout/member ABI")));
		ASSERT_THAT(IsFalse(Table.IsFrozen(),
			TEXT("ABI mismatch must commit no candidate bindings")));
	}

	TEST_METHOD(NonAuthoritativeHashCannotSplitEqualCompleteABIKeys)
	{
		asSTypeABIKey First;
		First.stableTypeKey = "FHashIndependent";
		First.targetProfileKey = "Win64.Development";
		First.nativeEnvironmentKey = "UE5.8-MSVC14.44";
		First.typeKind = asAST_TYPE_VALUE_OBJECT;
		First.objectFlags = asOBJ_VALUE | asOBJ_POD;
		First.byteSize = 4;
		First.byteAlignment = 4;
		First.layoutSignature = "size=4;align=4;flags=value|pod";
		First.behaviourSignature = "construct=trivial;destroy=trivial";
		First.layoutHash = 0x11111111ull;
		asSTypeABIKey Second = First;
		Second.layoutHash = 0x22222222ull;

		ASSERT_THAT(IsTrue(First.Equals(Second),
			TEXT("complete ABI equality must not depend on a cached/index hash value")));
	}

	TEST_METHOD(LaterRelocationFailureCommitsNoEarlierBinding)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(RegisterBoundValueType(ScriptEngine),
			TEXT("value-type fixture must register")));

		asCASTContext Context;
		const asCQualType BoundType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FBoundValue", 0);
		Context.InternNamedType(asAST_TYPE_VALUE_OBJECT, "FLateMissing", 0);
		const asSRuntimeTypeBindingEnvironment Environment = MakeEnvironment();
		asSTypeABIKey BoundABI;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_OK),
			static_cast<int32>(asCRuntimeTypeBindingTable::CaptureExpectedABI(
				ScriptEngine, Context, BoundType, Environment, BoundABI)),
			TEXT("first relocation ABI capture must succeed")));

		asSTypeRelocation First;
		First.expectedType = BoundABI;
		asSTypeRelocation Second;
		Second.expectedType.stableTypeKey = "FLateMissing";
		Second.expectedType.targetProfileKey = Environment.targetProfileKey;
		Second.expectedType.nativeEnvironmentKey = Environment.nativeEnvironmentKey;
		Second.expectedType.typeKind = asAST_TYPE_VALUE_OBJECT;
		asCArray<asSTypeRelocation> Relocations;
		Relocations.PushLast(First);
		Relocations.PushLast(Second);

		asCRuntimeTypeBindingTable Table;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_MISSING),
			static_cast<int32>(Table.Build(
				ScriptEngine, Context, Relocations, Environment)),
			TEXT("later failure must reject the complete candidate set")));
		ASSERT_THAT(IsFalse(Table.IsFrozen(),
			TEXT("later validation failure must not freeze")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0), Table.GetBindingCount(),
			TEXT("validate-all/commit-none must hide an earlier resolved slot")));
	}

	TEST_METHOD(PropertyAndFunctionRelocationsResolveBeforeFreeze)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"CBindingCarrier", 0, asOBJ_REF | asOBJ_NOCOUNT) >= 0
			&& ScriptEngine->RegisterObjectProperty(
				"CBindingCarrier", "int Value", asOFFSET(FRuntimeBindingCarrier, Value)) >= 0
			&& ScriptEngine->RegisterObjectMethod(
				"CBindingCarrier", "int Read() const", asFUNCTION(GenericRead), asCALL_GENERIC) >= 0,
			TEXT("property/function Runtime fixture must register")));

		asCASTContext Context;
		const asCQualType CanonicalType = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT, "CBindingCarrier", 0);
		const asSRuntimeTypeBindingEnvironment Environment = MakeEnvironment();
		asSTypeABIKey Expected;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_OK),
			static_cast<int32>(asCRuntimeTypeBindingTable::CaptureExpectedABI(
				ScriptEngine, Context, CanonicalType, Environment, Expected)),
			TEXT("object ABI capture must include registered members/behaviours")));

		asSTypeRelocation PropertyRelocation;
		PropertyRelocation.expectedType = Expected;
		PropertyRelocation.stableMemberKey = "CBindingCarrier::Value";
		PropertyRelocation.use = asTYPE_RELOC_PROPERTY;
		asSTypeRelocation FunctionRelocation;
		FunctionRelocation.expectedType = Expected;
		FunctionRelocation.stableMemberKey = "CBindingCarrier::Read";
		FunctionRelocation.use = asTYPE_RELOC_FUNCTION;
		asCArray<asSTypeRelocation> Relocations;
		Relocations.PushLast(PropertyRelocation);
		Relocations.PushLast(FunctionRelocation);

		asCRuntimeTypeBindingTable Table;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asRUNTIME_TYPE_BIND_OK),
			static_cast<int32>(Table.Build(
				ScriptEngine, Context, Relocations, Environment)),
			TEXT("complete member relocation set must resolve atomically")));
		const asSResolvedRuntimeTypeBinding* const Property = Table.GetBinding(0);
		const asSResolvedRuntimeTypeBinding* const Function = Table.GetBinding(1);
		ASSERT_THAT(IsTrue(Property != nullptr
			&& Property->property != nullptr
			&& Property->propertyOffset == asOFFSET(FRuntimeBindingCarrier, Value),
			TEXT("property binding must capture the current generation offset")));
		ASSERT_THAT(IsTrue(Function != nullptr && Function->function != nullptr,
			TEXT("function binding must capture the current generation call target")));
	}
};

#endif
