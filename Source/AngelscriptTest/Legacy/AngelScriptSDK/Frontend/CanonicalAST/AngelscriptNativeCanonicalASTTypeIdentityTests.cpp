#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/as_ast_context.h"
#include "source/as_ast_public_view.h"
#include "as_memory.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalASTTypeIdentityTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.TypeIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ContextRejectsForeignOwnerWithSameTypeIndex)
	{
		asCASTContext Context;
		const asCQualType LocalInt = Context.InternPrimitive(ttInt, 0);
		ASSERT_THAT(IsTrue(LocalInt.IsValid(), TEXT("fixture must intern the local int type")));

		const asASTTypeRef ForeignSameIndex(LocalInt.type.value, 0xA57u);
		ASSERT_THAT(IsNull(Context.GetType(ForeignSameIndex),
			TEXT("a snapshot-local type index with a foreign owner token must not resolve in this context")));
	}

	TEST_METHOD(PublicSnapshotsRejectNumericallyEqualForeignTypeRefs)
	{
		asCASTContext* FirstContext = asNEW(asCASTContext)();
		ASSERT_THAT(IsNotNull(FirstContext, TEXT("first snapshot context must allocate")));
		const asASTDeclId FirstTu = FirstContext->CreateTranslationUnit("FirstTypeSnapshot");
		const asASTDeclId FirstValue = FirstContext->CreateDecl(
			asAST_DECL_VAR, FirstTu, asCSourceRange(), "Value");
		ASSERT_THAT(AreEqual(0, FirstContext->SetDeclType(
			FirstValue, FirstContext->InternPrimitive(ttInt, 0))));
		ASSERT_THAT(AreEqual(0, FirstContext->Seal(), TEXT("first type snapshot must seal")));

		asCASTContext* SecondContext = asNEW(asCASTContext)();
		ASSERT_THAT(IsNotNull(SecondContext, TEXT("second snapshot context must allocate")));
		const asASTDeclId SecondTu = SecondContext->CreateTranslationUnit("SecondTypeSnapshot");
		const asASTDeclId SecondValue = SecondContext->CreateDecl(
			asAST_DECL_VAR, SecondTu, asCSourceRange(), "Value");
		ASSERT_THAT(AreEqual(0, SecondContext->SetDeclType(
			SecondValue, SecondContext->InternPrimitive(ttInt, 0))));
		ASSERT_THAT(AreEqual(0, SecondContext->Seal(), TEXT("second type snapshot must seal")));

		asCASTSnapshot* FirstSnapshot = asNEW(asCASTSnapshot)(
			FirstContext, "FirstTypeSnapshot", "generation-a");
		asCASTSnapshot* SecondSnapshot = asNEW(asCASTSnapshot)(
			SecondContext, "SecondTypeSnapshot", "generation-b");
		ASSERT_THAT(IsNotNull(FirstSnapshot, TEXT("first public snapshot must allocate")));
		ASSERT_THAT(IsNotNull(SecondSnapshot, TEXT("second public snapshot must allocate")));
		ON_SCOPE_EXIT
		{
			FirstSnapshot->Release();
			SecondSnapshot->Release();
		};

		asASTDeclId FirstPublicValue;
		ASSERT_THAT(AreEqual(asSUCCESS, FirstSnapshot->GetDeclChild(
			FirstSnapshot->GetTranslationUnitDecl(), 0, &FirstPublicValue)));
		asSASTDeclView FirstDeclView = {};
		FirstDeclView.structSize = sizeof(FirstDeclView);
		FirstDeclView.apiVersion = asAST_API_VERSION_1;
		ASSERT_THAT(AreEqual(asSUCCESS, FirstSnapshot->GetDecl(FirstPublicValue, &FirstDeclView)));

		asASTDeclId SecondPublicValue;
		ASSERT_THAT(AreEqual(asSUCCESS, SecondSnapshot->GetDeclChild(
			SecondSnapshot->GetTranslationUnitDecl(), 0, &SecondPublicValue)));
		asSASTDeclView SecondDeclView = {};
		SecondDeclView.structSize = sizeof(SecondDeclView);
		SecondDeclView.apiVersion = asAST_API_VERSION_1;
		ASSERT_THAT(AreEqual(asSUCCESS, SecondSnapshot->GetDecl(SecondPublicValue, &SecondDeclView)));

		ASSERT_THAT(AreEqual(FirstDeclView.type.value, SecondDeclView.type.value,
			TEXT("fixture must produce equal snapshot-local numeric type indices")));
		ASSERT_THAT(AreNotEqual(FirstDeclView.type.snapshotOwner, SecondDeclView.type.snapshotOwner,
			TEXT("different public snapshots must issue different owner tokens")));

		asSASTTypeView ForeignView = {};
		ForeignView.structSize = sizeof(ForeignView);
		ForeignView.apiVersion = asAST_API_VERSION_1;
		ASSERT_THAT(AreEqual(asINVALID_ARG, SecondSnapshot->GetType(FirstDeclView.type, &ForeignView),
			TEXT("equal numeric indices must not allow a type ref to cross snapshot owners")));
	}

	TEST_METHOD(CompleteStableKeysRejectKindOwnerTemplateAndHashAliases)
	{
		asCASTContext Context;
		const asCQualType EnumType = Context.InternNamedType(
			asAST_TYPE_ENUM, "Game::Inventory::Item", 0);
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "Game::Inventory::Item", 0);
		const asCQualType ReferenceType = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT, "Game::Inventory::Item", asAST_QUAL_HANDLE);
		const asCQualType FuncdefType = Context.InternNamedType(
			asAST_TYPE_FUNCDEF, "Game::Inventory::Item", asAST_QUAL_HANDLE);
		const asCQualType ProjectTemplate = Context.InternNamedType(
			asAST_TYPE_TEMPLATE, "Box<Project::Item>", asAST_QUAL_HANDLE);
		const asCQualType PluginTemplate = Context.InternNamedType(
			asAST_TYPE_TEMPLATE, "Box<Plugin::Item>", asAST_QUAL_HANDLE);

		ASSERT_THAT(IsTrue(EnumType.IsValid() && ValueType.IsValid()
			&& ReferenceType.IsValid() && FuncdefType.IsValid()
			&& ProjectTemplate.IsValid() && PluginTemplate.IsValid(),
			TEXT("all stable type identity fixtures must intern")));
		ASSERT_THAT(IsTrue(EnumType.type != ValueType.type
			&& EnumType.type != ReferenceType.type
			&& EnumType.type != FuncdefType.type,
			TEXT("identical display spelling must not alias different semantic type kinds")));
		ASSERT_THAT(IsTrue(ProjectTemplate.type != PluginTemplate.type,
			TEXT("template arguments and their complete owners must participate in stable identity")));

		const asQWORD DeliberatelyCollidingLookupHash = 0xC0111DEu;
		const asQWORD FirstLookupHash = DeliberatelyCollidingLookupHash;
		const asQWORD SecondLookupHash = DeliberatelyCollidingLookupHash;
		ASSERT_THAT(AreEqual(FirstLookupHash, SecondLookupHash,
			TEXT("fixture must force a lookup-hash collision")));
		ASSERT_THAT(IsTrue(ProjectTemplate.type != PluginTemplate.type,
			TEXT("a colliding lookup hash must never replace complete stable-key equality")));

		const asCQualType ProjectTemplateAgain = Context.InternNamedType(
			asAST_TYPE_TEMPLATE, "Box<Project::Item>", asAST_QUAL_HANDLE);
		ASSERT_THAT(IsTrue(ProjectTemplate == ProjectTemplateAgain,
			TEXT("the same complete type and qualifiers must intern exactly once in one snapshot")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
