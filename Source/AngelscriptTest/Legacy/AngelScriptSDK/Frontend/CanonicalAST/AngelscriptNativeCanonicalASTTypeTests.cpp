#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/as_ast_context.h"
#include "source/as_ast_type.h"
#include "source/as_runtime_type_bridge.h"
#include "as_scriptengine.h"
#include "as_tokendef.h"
#include "as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	asCDataType MakeHostDataType(asIScriptEngine* Engine, const char* Decl, bool bConst = false)
	{
		asITypeInfo* Info = Engine->GetTypeInfoByDecl(Decl);
		if (Info == nullptr)
		{
			Info = Engine->GetTypeInfoByName(Decl);
		}
		return asCDataType::CreateType(static_cast<asCTypeInfo*>(Info), bConst);
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTTypeTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.Type",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(PrimitiveEnumObjectAndQualifierCanonicalization)
	{
		asCASTContext Context;
		const asCQualType IntA = Context.InternPrimitive(ttInt, 0);
		const asCQualType IntB = Context.InternPrimitive(ttInt, 0);
		const asCQualType ConstInt = Context.InternPrimitive(ttInt, asAST_QUAL_CONST);
		const asCQualType HandleObj = Context.InternNamedType(asAST_TYPE_REFERENCE_OBJECT, "UObject", asAST_QUAL_HANDLE);
		const asCQualType ValueObj = Context.InternNamedType(asAST_TYPE_VALUE_OBJECT, "FVector", 0);
		const asCQualType EnumTy = Context.InternNamedType(asAST_TYPE_ENUM, "ETeam", 0);
		const asCQualType FuncDef = Context.InternNamedType(asAST_TYPE_FUNCDEF, "FCallback", 0);
		const asCQualType Tmpl = Context.InternNamedType(asAST_TYPE_TEMPLATE, "TArray<int>", 0);

		ASSERT_THAT(IsTrue(IntA.IsValid() && IntA == IntB, TEXT("equivalent primitives should intern to one canonical type")));
		ASSERT_THAT(IsTrue(IntA != ConstInt, TEXT("const should distinguish QualType identity")));
		ASSERT_THAT(IsTrue(HandleObj.IsHandle(), TEXT("handle qualifier bit should be visible")));
		ASSERT_THAT(IsTrue(ValueObj.IsValid() && EnumTy.IsValid() && FuncDef.IsValid() && Tmpl.IsValid(),
			TEXT("enum, funcdef, value object, and template keys should intern")));

		const asCQualType InOut = Context.InternPrimitive(ttInt, asAST_QUAL_INOUT | asAST_QUAL_REFERENCE);
		ASSERT_THAT(IsTrue(InOut.IsValid() && InOut.IsReference(), TEXT("in/out/inout + reference should be a valid qualifier set")));
		const asCQualType BadDir = Context.InternPrimitive(ttInt, asAST_QUAL_IN);
		ASSERT_THAT(IsTrue(!BadDir.IsValid(), TEXT("in-only param dir without reference is invalid")));
		const asCQualType NullLike = Context.InternPrimitive(ttVoid, asAST_QUAL_HANDLE);
		ASSERT_THAT(IsTrue(NullLike.IsValid() && NullLike.IsHandle(),
			TEXT("nullptr encoding remains void with handle")));
	}

	TEST_METHOD(RejectsDirectionWithoutReference)
	{
		asCASTContext Context;
		const asCQualType InOnly = Context.InternPrimitive(ttInt, asAST_QUAL_IN);
		const asCQualType OutOnly = Context.InternPrimitive(ttInt, asAST_QUAL_OUT);
		const asCQualType InOutOnly = Context.InternPrimitive(ttInt, asAST_QUAL_INOUT);
		ASSERT_THAT(IsTrue(!InOnly.IsValid(), TEXT("in without reference is illegal")));
		ASSERT_THAT(IsTrue(!OutOnly.IsValid(), TEXT("out without reference is illegal")));
		ASSERT_THAT(IsTrue(!InOutOnly.IsValid(), TEXT("inout without reference is illegal")));
		ASSERT_THAT(IsTrue(!asASTQualifiersAreValid(asAST_QUAL_IN), TEXT("qualifier predicate rejects in without reference")));
	}

	TEST_METHOD(RejectsAutoHandleWithoutHandle)
	{
		asCASTContext Context;
		const asCQualType AutoOnly = Context.InternPrimitive(ttInt, asAST_QUAL_AUTO_HANDLE);
		ASSERT_THAT(IsTrue(!AutoOnly.IsValid(), TEXT("auto-handle requires handle")));
		ASSERT_THAT(IsTrue(!asASTQualifiersAreValid(asAST_QUAL_AUTO_HANDLE), TEXT("qualifier predicate rejects auto-handle without handle")));
		const asCQualType AutoHandle = Context.InternNamedType(asAST_TYPE_REFERENCE_OBJECT, "UObject", asAST_QUAL_HANDLE | asAST_QUAL_AUTO_HANDLE);
		ASSERT_THAT(IsTrue(AutoHandle.IsValid() && AutoHandle.IsAutoHandle(), TEXT("handle + auto-handle stays valid")));
	}

	TEST_METHOD(RejectsIllegalVoidQualifiers)
	{
		asCASTContext Context;
		const asCQualType VoidRef = Context.InternPrimitive(ttVoid, asAST_QUAL_REFERENCE);
		const asCQualType VoidIn = Context.InternPrimitive(ttVoid, asAST_QUAL_IN | asAST_QUAL_REFERENCE);
		const asCQualType VoidConst = Context.InternPrimitive(ttVoid, asAST_QUAL_CONST);
		ASSERT_THAT(IsTrue(!VoidRef.IsValid(), TEXT("void reference is illegal")));
		ASSERT_THAT(IsTrue(!VoidIn.IsValid(), TEXT("void in-reference is illegal")));
		ASSERT_THAT(IsTrue(!VoidConst.IsValid(), TEXT("const void is illegal")));
		const asCQualType NullLike = Context.InternPrimitive(ttVoid, asAST_QUAL_HANDLE);
		ASSERT_THAT(IsTrue(NullLike.IsValid(), TEXT("void handle remains the nullptr encoding")));
	}

	TEST_METHOD(RejectsUnknownQualifierBits)
	{
		asCASTContext Context;
		const asDWORD UnknownBit = 1u << 16;
		const asCQualType Unknown = Context.InternPrimitive(ttInt, UnknownBit);
		ASSERT_THAT(IsTrue(!Unknown.IsValid(), TEXT("unknown qualifier bits must not intern")));
		ASSERT_THAT(IsTrue(!asASTQualifiersAreValid(UnknownBit), TEXT("qualifier predicate rejects unknown bits")));
	}

	TEST_METHOD(RuntimeBridgeDoesNotEmbedEnginePointersInQualType)
	{
		asCASTContext Context;
		const asCQualType IntType = Context.InternPrimitive(ttInt, asAST_QUAL_CONST);
		asCRuntimeTypeBridge Bridge(nullptr);
		const asCDataType DataType = Bridge.Resolve(Context, IntType);
		ASSERT_THAT(IsTrue(DataType.IsPrimitive(), TEXT("bridge should resolve interned int to Engine asCDataType")));
		ASSERT_THAT(IsTrue(DataType.IsReadOnly(), TEXT("const qualifier should map to read-only datatype")));
		ASSERT_THAT(IsTrue(IntType.type.IsValid(), TEXT("public type identity stays a snapshot-local ID")));
	}

	TEST_METHOD(RuntimeBridgeResolvesTypedefToUnderlyingRuntimeDataType)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterTypedef("NativeCount", "int") >= 0,
			TEXT("host typedef registration should succeed")));

		asCASTContext Context;
		const asCQualType Alias = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "NativeCount", 0);
		const asCType* const AliasType = Context.GetType(Alias.type);
		ASSERT_THAT(IsTrue(AliasType != nullptr
			&& AliasType->stableKey.Equals("NativeCount"),
			TEXT("Canonical AST should retain the stable typedef spelling")));

		asCRuntimeTypeBridge Bridge(ScriptEngine);
		const asCDataType Resolved = Bridge.Resolve(Context, Alias);
		ASSERT_THAT(IsTrue(Resolved.IsPrimitive(),
			TEXT("Runtime bridge must lower a typedef to its underlying VM datatype")));
		ASSERT_THAT(AreEqual(static_cast<int32>(ttInt),
			static_cast<int32>(Resolved.GetTokenType()),
			TEXT("NativeCount should use the int frame ABI")));
		ASSERT_THAT(AreEqual(
			ScriptEngine->GetTypeIdByDecl("NativeCount"),
			ScriptEngine->GetTypeIdFromDataType(Resolved),
			TEXT("resolved typedef should retain the public Runtime type-id contract")));
	}

	TEST_METHOD(RuntimeBridgeInternsHostEnumAsEnumKind)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("ETeam") >= 0, TEXT("host RegisterEnum(ETeam)")));
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetDefaultNamespace("Game"), TEXT("select Game namespace")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("ETeam") >= 0, TEXT("namespace-local ETeam is a different Engine type")));
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetDefaultNamespace(""), TEXT("restore global namespace")));

		asCASTContext Context;
		asCRuntimeTypeBridge Bridge(ScriptEngine);
		const asCDataType GlobalDt = MakeHostDataType(Engine.Get(), "ETeam");
		const asCQualType GlobalQual = Bridge.FromDataType(Context, GlobalDt);
		const asCType* GlobalTy = Context.GetType(GlobalQual.type);
		ASSERT_THAT(IsTrue(GlobalQual.IsValid() && GlobalTy != nullptr, TEXT("enum FromDataType must intern")));
		ASSERT_THAT(AreEqual((int)asAST_TYPE_ENUM, (int)GlobalTy->kind, TEXT("host enum kind is ENUM, not primitive/VALUE_OBJECT")));
		ASSERT_THAT(IsTrue(GlobalTy->stableKey.Equals("ETeam"), TEXT("global enum key is the name, not typeId")));

		const asCQualType Again = Bridge.FromDataType(Context, GlobalDt);
		ASSERT_THAT(IsTrue(GlobalQual == Again, TEXT("equivalent enum datatypes intern to one QualType")));

		const asCDataType ConstDt = asCDataType::CreateType(GlobalDt.GetTypeInfo(), true);
		const asCQualType ConstQual = Bridge.FromDataType(Context, ConstDt);
		ASSERT_THAT(IsTrue(ConstQual.IsValid() && ConstQual.IsConst(), TEXT("const is a qualifier")));
		ASSERT_THAT(AreEqual((int)asAST_TYPE_ENUM, (int)Context.GetType(ConstQual.type)->kind, TEXT("const does not change enum kind")));
		ASSERT_THAT(IsTrue(ConstQual.type == GlobalQual.type, TEXT("const enum shares the interned asCType")));

		const asCDataType NsDt = MakeHostDataType(Engine.Get(), "Game::ETeam");
		const asCQualType NsQual = Bridge.FromDataType(Context, NsDt);
		const asCType* NsTy = Context.GetType(NsQual.type);
		ASSERT_THAT(IsTrue(NsQual.IsValid() && NsTy != nullptr, TEXT("namespaced enum FromDataType must intern")));
		ASSERT_THAT(AreEqual((int)asAST_TYPE_ENUM, (int)NsTy->kind, TEXT("namespaced enum is still ENUM")));
		ASSERT_THAT(IsTrue(NsTy->stableKey.Equals("Game::ETeam"), TEXT("namespace is part of the stable key")));
		ASSERT_THAT(IsTrue(NsQual != GlobalQual, TEXT("Game::ETeam must not collide with global ETeam")));

		const asCDataType Resolved = Bridge.Resolve(Context, NsQual);
		ASSERT_THAT(IsTrue(Resolved.GetTypeInfo() == NsDt.GetTypeInfo(),
			TEXT("Resolve uses stable key, not a stored typeId, and binds the current Engine type")));
	}

	TEST_METHOD(RuntimeBridgeInternsHostFuncdefAsFuncdefKind)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterFuncdef("void FCallback()") >= 0, TEXT("host RegisterFuncdef")));

		asCASTContext Context;
		asCRuntimeTypeBridge Bridge(static_cast<asCScriptEngine*>(ScriptEngine));
		const asCDataType DataType = MakeHostDataType(ScriptEngine, "FCallback");
		ASSERT_THAT(IsTrue(DataType.GetTypeInfo() != nullptr, TEXT("funcdef type info exists without Build")));
		ASSERT_THAT(IsTrue((DataType.GetTypeInfo()->GetFlags() & asOBJ_FUNCDEF) != 0, TEXT("host funcdef carries asOBJ_FUNCDEF")));

		const asCQualType Qual = Bridge.FromDataType(Context, DataType);
		const asCType* Ty = Context.GetType(Qual.type);
		ASSERT_THAT(IsTrue(Qual.IsValid() && Ty != nullptr, TEXT("funcdef FromDataType must intern")));
		ASSERT_THAT(AreEqual((int)asAST_TYPE_FUNCDEF, (int)Ty->kind,
			TEXT("asOBJ_REF|FUNCDEF must intern FUNCDEF, not REFERENCE_OBJECT")));
		ASSERT_THAT(IsTrue(Ty->stableKey.Equals("FCallback"), TEXT("funcdef key is the name, not typeId or pointer")));
		ASSERT_THAT(AreEqual((int)asAST_TYPE_FUNCDEF, (int)Context.GetType(Bridge.FromDataType(Context, DataType).type)->kind,
			TEXT("second FromDataType intern-hits the same FUNCDEF type")));
	}

	TEST_METHOD(RuntimeBridgeInternsArrayIntAsTemplateKind)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType("array<class T>", 0, asOBJ_REF | asOBJ_TEMPLATE | asOBJ_NOCOUNT) >= 0,
			TEXT("host array<T> template must register without Build")));

		asITypeInfo* const Inst = ScriptEngine->GetTypeInfoByDecl("array<int>");
		ASSERT_THAT(IsNotNull(Inst, TEXT("GetTypeInfoByDecl(array<int>) must instantiate without module Build")));
		ASSERT_THAT(IsTrue((Inst->GetFlags() & asOBJ_TEMPLATE) != 0, TEXT("instance keeps asOBJ_TEMPLATE")));

		asCASTContext Context;
		asCRuntimeTypeBridge Bridge(static_cast<asCScriptEngine*>(ScriptEngine));
		const asCDataType DataType = asCDataType::CreateType(static_cast<asCTypeInfo*>(Inst), false);
		const asCQualType Qual = Bridge.FromDataType(Context, DataType);
		const asCType* Ty = Context.GetType(Qual.type);
		ASSERT_THAT(IsTrue(Qual.IsValid() && Ty != nullptr, TEXT("array<int> FromDataType must intern")));
		ASSERT_THAT(AreEqual((int)asAST_TYPE_TEMPLATE, (int)Ty->kind,
			TEXT("asOBJ_REF|TEMPLATE must intern TEMPLATE, not REFERENCE_OBJECT")));
		ASSERT_THAT(IsTrue(Ty->stableKey.Equals("array<int>"),
			TEXT("template instance key is array<int>, not bare array and not typeId")));
		ASSERT_THAT(IsTrue(!Ty->stableKey.Equals("array"), TEXT("bare template name is not the instance key")));

		const asCDataType Resolved = Bridge.Resolve(Context, Qual);
		ASSERT_THAT(IsTrue(Resolved.GetTypeInfo() == Inst,
			TEXT("Resolve(array<int>) must use GetTypeInfoByDecl, not GetTypeInfoByName(array)")));
	}

	TEST_METHOD(RuntimeBridgeResolvesDefaultArrayAliasPerEngineGeneration)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine EngineA;
		FNativeTestEngine EngineB;
		EngineA.Create(*TestRunner);
		EngineB.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			EngineB.Destroy();
			EngineA.Destroy();
		};

		asIScriptEngine* const ScriptEngineA = EngineA.Get();
		asIScriptEngine* const ScriptEngineB = EngineB.Get();
		ASSERT_THAT(IsTrue(
			ScriptEngineA->RegisterObjectType("TArray<class T>", 0, asOBJ_REF | asOBJ_TEMPLATE | asOBJ_NOCOUNT) >= 0,
			TEXT("Engine A TArray<T> template must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngineA->RegisterDefaultArrayType("TArray<T>") >= 0,
			TEXT("Engine A TArray<T> must become the default array")));

		// Perturb Engine B's registration order so the same semantic array type has
		// a different public numeric typeId in the second Engine generation.
		ASSERT_THAT(IsTrue(
			ScriptEngineB->RegisterObjectType("FDummy", 0, asOBJ_REF | asOBJ_NOCOUNT) >= 0,
			TEXT("Engine B dummy type must perturb the public typeId order")));
		ASSERT_THAT(IsTrue(
			ScriptEngineB->RegisterObjectType("TArray<class T>", 0, asOBJ_REF | asOBJ_TEMPLATE | asOBJ_NOCOUNT) >= 0,
			TEXT("Engine B TArray<T> template must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngineB->RegisterDefaultArrayType("TArray<T>") >= 0,
			TEXT("Engine B TArray<T> must become the default array")));

		asITypeInfo* const ArrayA = ScriptEngineA->GetTypeInfoByDecl("TArray<int>");
		asITypeInfo* const ArrayB = ScriptEngineB->GetTypeInfoByDecl("TArray<int>");
		ASSERT_THAT(IsNotNull(ArrayA, TEXT("Engine A TArray<int> instance")));
		ASSERT_THAT(IsNotNull(ArrayB, TEXT("Engine B TArray<int> instance")));
		ASSERT_THAT(IsTrue(ArrayA->GetTypeId() != ArrayB->GetTypeId(),
			TEXT("registration-order perturbation must prove public typeId is Engine-local")));

		asCASTContext Context;
		const asCQualType CanonicalArray = Context.InternNamedType(
			asAST_TYPE_TEMPLATE, "TArray<int>", 0);
		ASSERT_THAT(IsTrue(CanonicalArray.IsValid(), TEXT("stable TArray<int> identity must intern")));

		asCRuntimeTypeBridge BridgeA(static_cast<asCScriptEngine*>(ScriptEngineA));
		asCRuntimeTypeBridge BridgeB(static_cast<asCScriptEngine*>(ScriptEngineB));
		const asCDataType ResolvedA = BridgeA.Resolve(Context, CanonicalArray);
		const asCDataType ResolvedB = BridgeB.Resolve(Context, CanonicalArray);
		ASSERT_THAT(IsTrue(ResolvedA.GetTypeInfo() == ArrayA,
			TEXT("stable TArray<int> must late-bind to Engine A's current default-array instance")));
		ASSERT_THAT(IsTrue(ResolvedB.GetTypeInfo() == ArrayB,
			TEXT("stable TArray<int> must late-bind to Engine B's current default-array instance")));
		ASSERT_THAT(IsTrue(ResolvedA.GetTypeInfo() != ResolvedB.GetTypeInfo(),
			TEXT("canonical identity must not retain a publishing-Engine type pointer")));
	}

	TEST_METHOD(RuntimeBridgeNamedTypeIdentityHasNoEnginePointerOrTypeId)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("ETeam") >= 0, TEXT("host enum")));
		asITypeInfo* const Info = ScriptEngine->GetTypeInfoByName("ETeam");
		ASSERT_THAT(IsNotNull(Info, TEXT("ETeam type info")));
		const int EngineTypeId = Info->GetTypeId();
		const uintptr_t InfoAddr = reinterpret_cast<uintptr_t>(Info);

		asCASTContext Context;
		asCRuntimeTypeBridge Bridge(static_cast<asCScriptEngine*>(ScriptEngine));
		const asCQualType Qual = Bridge.FromDataType(Context, asCDataType::CreateType(static_cast<asCTypeInfo*>(Info), false));
		const asCType* Ty = Context.GetType(Qual.type);
		ASSERT_THAT(IsTrue(Qual.IsValid() && Ty != nullptr, TEXT("named FromDataType must intern")));
		ASSERT_THAT(AreEqual(sizeof(asASTTypeRef) + sizeof(asDWORD), sizeof(asCQualType),
			TEXT("QualType layout is opaque TypeRef (id + snapshot owner) + quals; no engine pointer word")));
		ASSERT_THAT(IsTrue(Qual.type.value != static_cast<asUINT>(EngineTypeId),
			TEXT("snapshot-local TypeRef must not be the Engine typeId")));
		ASSERT_THAT(IsTrue(reinterpret_cast<uintptr_t>(Ty) != InfoAddr, TEXT("interned asCType is not asCTypeInfo")));
		ASSERT_THAT(IsTrue(Ty->stableKey.Equals("ETeam"), TEXT("identity is the stable key string")));
		ASSERT_THAT(IsTrue(FCStringAnsi::Strstr(Ty->stableKey.AddressOf(), "0x") == nullptr,
			TEXT("stableKey must not spell an address")));

		asCString KeyCopy = Ty->stableKey;
		const asEASTTypeKind KindCopy = Ty->kind;
		const asASTTypeRef IdCopy = Qual.type;
		Engine.Destroy();

		const asCType* After = Context.GetType(IdCopy);
		ASSERT_THAT(IsTrue(After != nullptr && After->kind == KindCopy && After->stableKey.Equals(KeyCopy),
			TEXT("QualType/asCType must remain valid after Engine.Destroy — no dangling typeInfo*")));
	}

	TEST_METHOD(RuntimeBridgeNullHandleIsVoidWithHandle)
	{
		asCASTContext Context;
		asCRuntimeTypeBridge Bridge(nullptr);
		const asCQualType Qual = Bridge.FromDataType(Context, asCDataType::CreateNullHandle());
		const asCType* Ty = Context.GetType(Qual.type);
		ASSERT_THAT(IsTrue(Qual.IsValid() && Qual.IsHandle() && !Qual.IsConst() && !Qual.IsReference(),
			TEXT("null handle intern is void+handle, not const void")));
		ASSERT_THAT(IsTrue(Ty != nullptr && Ty->kind == asAST_TYPE_VOID && Ty->primitiveToken == ttVoid,
			TEXT("nullptr encoding is the void primitive, not a named type")));
		const asCQualType Direct = Context.InternPrimitive(ttVoid, asAST_QUAL_HANDLE);
		ASSERT_THAT(IsTrue(Qual == Direct, TEXT("bridge null handle matches the interned nullptr encoding")));
	}

	TEST_METHOD(RuntimeBridgeHandleIsQualifierNotKindForRefAndValue)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType("CObj", 0, asOBJ_REF | asOBJ_NOCOUNT) >= 0, TEXT("host ref type")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType("FValue", 4, asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE) >= 0,
			TEXT("host value type")));

		asCASTContext Context;
		asCRuntimeTypeBridge Bridge(static_cast<asCScriptEngine*>(ScriptEngine));
		asCDataType RefDt = MakeHostDataType(ScriptEngine, "CObj");
		const asCQualType RefQual = Bridge.FromDataType(Context, RefDt);
		RefDt.MakeHandle(true);
		const asCQualType HandleQual = Bridge.FromDataType(Context, RefDt);
		ASSERT_THAT(AreEqual((int)asAST_TYPE_REFERENCE_OBJECT, (int)Context.GetType(RefQual.type)->kind,
			TEXT("asOBJ_REF interned as REFERENCE_OBJECT")));
		ASSERT_THAT(AreEqual((int)asAST_TYPE_REFERENCE_OBJECT, (int)Context.GetType(HandleQual.type)->kind,
			TEXT("handle does not change kind")));
		ASSERT_THAT(IsTrue(RefQual.type == HandleQual.type, TEXT("handle vs non-handle share asCType")));
		ASSERT_THAT(IsTrue(!RefQual.IsHandle() && HandleQual.IsHandle(), TEXT("handle is a QualType bit")));
		ASSERT_THAT(IsTrue(RefQual != HandleQual, TEXT("handle distinguishes QualType identity")));

		const asCQualType ValueQual = Bridge.FromDataType(Context, MakeHostDataType(ScriptEngine, "FValue"));
		ASSERT_THAT(AreEqual((int)asAST_TYPE_VALUE_OBJECT, (int)Context.GetType(ValueQual.type)->kind,
			TEXT("asOBJ_VALUE interned as VALUE_OBJECT, not from missing handle")));
		ASSERT_THAT(IsTrue(!ValueQual.IsHandle(), TEXT("value object is not a handle by default")));
	}

	TEST_METHOD(RuntimeBridgeSeparatesConstHandleQualifierFromStableTypeIdentity)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType("CObj", 0, asOBJ_REF | asOBJ_NOCOUNT) >= 0,
			TEXT("host ref type")));

		asCDataType ConstHandle = MakeHostDataType(ScriptEngine, "CObj");
		ASSERT_THAT(AreEqual(0, ConstHandle.MakeHandle(true)));
		ASSERT_THAT(AreEqual(0, ConstHandle.MakeHandleToConst(true)));
		ASSERT_THAT(IsTrue(ConstHandle.IsObjectHandle()
			&& ConstHandle.IsObjectConst(),
			TEXT("fixture must be a handle to const object")));

		asCASTContext Context;
		asCRuntimeTypeBridge Bridge(static_cast<asCScriptEngine*>(ScriptEngine));
		const asCQualType Qual = Bridge.FromDataType(Context, ConstHandle);
		const asCType* const Type = Context.GetType(Qual.type);
		ASSERT_THAT(IsTrue(Qual.IsValid() && Qual.IsHandle() && Qual.IsConst(),
			TEXT("handle-to-const must retain both QualType bits")));
		ASSERT_THAT(IsTrue(Type != nullptr && Type->stableKey.Equals("CObj"),
			TEXT("const/handle spelling must never enter stable canonical type identity")));

		const asCDataType RoundTrip = Bridge.Resolve(Context, Qual);
		ASSERT_THAT(IsTrue(RoundTrip.IsObjectHandle()
			&& RoundTrip.IsObjectConst()
			&& RoundTrip.GetTypeInfo() == ConstHandle.GetTypeInfo(),
			TEXT("const handle must round-trip without changing host type identity")));
	}

	TEST_METHOD(DifferentEngineTypeIdsDoNotChangeStableTypeIdentity)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine FirstEngine;
		FNativeTestEngine SecondEngine;
		FirstEngine.Create(*TestRunner);
		SecondEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			SecondEngine.Destroy();
			FirstEngine.Destroy();
		};

		ASSERT_THAT(IsTrue(FirstEngine.Get()->RegisterEnum("ECanonicalIdentity") >= 0,
			TEXT("first Engine target enum must register")));
		ASSERT_THAT(IsTrue(SecondEngine.Get()->RegisterEnum("EPaddingIdentity") >= 0,
			TEXT("second Engine padding enum must register")));
		ASSERT_THAT(IsTrue(SecondEngine.Get()->RegisterEnum("ECanonicalIdentity") >= 0,
			TEXT("second Engine target enum must register")));

		const int PaddingTypeId = SecondEngine.Get()->GetTypeIdByDecl("EPaddingIdentity");
		const int FirstTypeId = FirstEngine.Get()->GetTypeIdByDecl("ECanonicalIdentity");
		const int SecondTypeId = SecondEngine.Get()->GetTypeIdByDecl("ECanonicalIdentity");
		ASSERT_THAT(IsTrue(PaddingTypeId >= 0, TEXT("padding type must consume a dynamic Engine-local ID")));
		ASSERT_THAT(AreNotEqual(FirstTypeId, SecondTypeId,
			TEXT("registration/access order must demonstrate that public object type IDs are Engine-local")));

		asCASTContext FirstContext;
		asCASTContext SecondContext;
		asCRuntimeTypeBridge FirstBridge(static_cast<asCScriptEngine*>(FirstEngine.Get()));
		asCRuntimeTypeBridge SecondBridge(static_cast<asCScriptEngine*>(SecondEngine.Get()));
		const asCQualType FirstQual = FirstBridge.FromDataType(
			FirstContext, MakeHostDataType(FirstEngine.Get(), "ECanonicalIdentity"));
		const asCQualType SecondQual = SecondBridge.FromDataType(
			SecondContext, MakeHostDataType(SecondEngine.Get(), "ECanonicalIdentity"));
		const asCType* const FirstType = FirstContext.GetType(FirstQual.type);
		const asCType* const SecondType = SecondContext.GetType(SecondQual.type);
		ASSERT_THAT(IsNotNull(FirstType, TEXT("first Engine type must lower to a canonical type")));
		ASSERT_THAT(IsNotNull(SecondType, TEXT("second Engine type must lower to a canonical type")));
		ASSERT_THAT(AreEqual(static_cast<int32>(FirstType->kind), static_cast<int32>(SecondType->kind),
			TEXT("Engine-local ID allocation must not affect semantic type kind")));
		ASSERT_THAT(IsTrue(FirstType->stableKey.Equals(SecondType->stableKey),
			TEXT("Engine-local numeric IDs must not participate in the durable stable type key")));
	}
};

#endif
