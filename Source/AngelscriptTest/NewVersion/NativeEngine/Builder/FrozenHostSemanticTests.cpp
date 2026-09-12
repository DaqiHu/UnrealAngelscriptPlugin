#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "as_builder.h"
#include "as_module_definition_set.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "frontend/as_ast_cast.h"
#include "frontend/as_ast_visitor.h"
#include "frontend/as_type_context.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(FrozenHostSemantics, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FHost
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCModuleDefinitionSet> Set = MakeUnique<asCModuleDefinitionSet>();
		asCScriptFunction* Add = nullptr;
		asCObjectType* Item = nullptr;
		asCScriptFunction* Read = nullptr;
		asCScriptFunction* Mutate = nullptr;
		AS_NAMESPACE_QUALIFIER asSStableKey IntType;
		bool Initialize()
		{
if (Set->SetIdentityContext(Context) != asEMetadataResult::Succeeded) return false;
			Context->InternPrimitiveTypeUse(asEPrimitiveType::Int32, IntType);
			asSFunctionIdentityDescriptor AddIdentity;
			AddIdentity.StableScope = TEXT("frozen-host");
			AddIdentity.NamespacePath = {TEXT("Host")};
			AddIdentity.Name = TEXT("Add");
			AddIdentity.Kind = asEFunctionIdentityKind::Global;
			AddIdentity.ReturnType = IntType;
			AddIdentity.Parameters.Add({IntType});
			asSStableKey AddKey;
			asSMetadataParameter Parameter;
			Parameter.Name = "Value";
			Parameter.Type = Set->Primitive(ttInt);
			if (Context->GetIdentityRegistry().InternFunction(AddIdentity, AddKey) != asETypeIdentityStatus::Succeeded
				|| Set->CreateFunction(AddKey, "Add", "Host", asFUNC_SCRIPT, Add) != asEMetadataResult::Succeeded
				|| Set->DefineFunction(*Add, Set->Primitive(ttInt), {Parameter}) != asEMetadataResult::Succeeded
				|| Set->SetFunctionDeclarationFacts(*Add, asEFunctionIdentityKind::Global) != asEMetadataResult::Succeeded)
				return false;
			asSNominalTypeDescriptor ItemIdentity;
			ItemIdentity.StableScope = TEXT("frozen-host");
			ItemIdentity.Name = TEXT("Item");
			ItemIdentity.Kind = asETypeDeclKind::Class;
			asSStableKey ItemKey;
			if (Context->GetIdentityRegistry().InternNominalType(ItemIdentity, ItemKey) != asETypeIdentityStatus::Succeeded
				|| Set->CreateObjectType(ItemKey, "Item", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Item) != asEMetadataResult::Succeeded)
				return false;
			asSFunctionIdentityDescriptor ReadIdentity;
			ReadIdentity.StableScope = TEXT("frozen-host");
			ReadIdentity.Name = TEXT("Read");
			ReadIdentity.Kind = asEFunctionIdentityKind::Method;
			ReadIdentity.Owner = ItemKey;
			ReadIdentity.ReturnType = IntType;
			ReadIdentity.Modifiers = asEFunctionIdentityModifiers::ConstMethod;
			asSStableKey ReadKey;
			if (Context->GetIdentityRegistry().InternFunction(ReadIdentity, ReadKey) != asETypeIdentityStatus::Succeeded
				|| Set->CreateFunction(ReadKey, "Read", "", asFUNC_SCRIPT, Read) != asEMetadataResult::Succeeded
				|| Set->DefineFunction(*Read, Set->Primitive(ttInt), {}, Item) != asEMetadataResult::Succeeded
				|| Set->SetFunctionDeclarationFacts(*Read, asEFunctionIdentityKind::Method, Item) != asEMetadataResult::Succeeded)
				return false;
			Read->SetReadOnly(true);
			asSFunctionIdentityDescriptor MutateIdentity = ReadIdentity;
			MutateIdentity.Name = TEXT("Mutate");
			MutateIdentity.ReturnType = IntType;
			MutateIdentity.Modifiers = asEFunctionIdentityModifiers::None;
			asSStableKey MutateKey;
			return Context->GetIdentityRegistry().InternFunction(MutateIdentity, MutateKey) == asETypeIdentityStatus::Succeeded
				&& Set->CreateFunction(MutateKey, "Mutate", "", asFUNC_SCRIPT, Mutate) == asEMetadataResult::Succeeded
				&& Set->DefineFunction(*Mutate, Set->Primitive(ttInt), {}, Item) == asEMetadataResult::Succeeded
				&& Set->SetFunctionDeclarationFacts(*Mutate, asEFunctionIdentityKind::Method, Item) == asEMetadataResult::Succeeded
				&& Set->FinalizeLayouts() == asEMetadataResult::Succeeded
				&& Set->Freeze() == asEMetadataResult::Succeeded
				&& Set->ValidateFrozenDefinitions();
		}
	};
	struct FBuilderInput
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot = MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
		AS_NAMESPACE_QUALIFIER asCSourceManager Sources{Snapshot};
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics{Sources};
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
		explicit FBuilderInput(const FString& Source, TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> TypeContext,
			asCModuleDefinitionSet* Host)
		{
			const auto Input = AngelscriptNativeEngineTest::FSourceInput::FromText(TEXT("Main.as"), Source);
			Snapshot->AddFile(TEXT("Main.as"), Input.GetUtf8Bytes());
			Snapshot->Freeze();
			Options.StableScope = TEXT("frozen-host.source");
			Options.TypeContext = TypeContext;
			if (Host) Options.Dependencies.Add(Host);
		}
		FString DescribeFailure(const asCBuilder& Builder) const
		{
			FString Out = FString::Printf(TEXT("stage=%d"), int32(Builder.GetStage()));
			for (uint8 Stage = 0; Stage < uint8(AS_NAMESPACE_QUALIFIER asEBuilderStage::Failed); ++Stage)
			{
				if (const auto* Result = Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage(Stage)))
					Out += FString::Printf(TEXT(" | %d succ=%d err=%s"), int32(Stage), Result->Succeeded ? 1 : 0, *Result->Error);
			}
			return Out;
		}
	};
	static const AS_NAMESPACE_QUALIFIER asCCallExpr* FindCall(const AS_NAMESPACE_QUALIFIER asCASTNode* Root)
	{
		const AS_NAMESPACE_QUALIFIER asCCallExpr* Call = nullptr;
		AS_NAMESPACE_QUALIFIER asCASTVisitor::Walk(Root, [&Call](const AS_NAMESPACE_QUALIFIER asCASTNode& Node)
		{
			if (!Call) Call = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(&Node);
		});
		return Call;
	}
public:
	TEST_METHOD(FrozenHostGlobalCallSelectsAuthenticatedKey)
	{
		FHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FBuilderInput Input(TEXT("int F() { return Host::Add(42); }"), Host.Context, Host.Set.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		const bool Built = Builder.RunThrough();
		ASSERT_THAT(IsTrue(Built, Input.DescribeFailure(Builder)));
		const AS_NAMESPACE_QUALIFIER asCFunctionDecl* Function = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(
			Builder.GetCompilationSession()->Lookup(TEXT("F"))[0]);
		ASSERT_THAT(IsNotNull(Function));
		const AS_NAMESPACE_QUALIFIER asCCallExpr* Call = FindCall(Function->GetBody());
		ASSERT_THAT(IsNotNull(Call));
		const auto* Callee = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Call->GetCallee());
		ASSERT_THAT(IsNotNull(Callee));
		ASSERT_THAT(IsTrue(Callee->GetExternalIdentity() == Host.Add->GetStableKey()));
		ASSERT_THAT(IsTrue(Callee->GetExternalFunction() == Host.Add));
	}

	TEST_METHOD(ConstHostReceiverRejectsMutableMethod)
	{
		FHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FBuilderInput Allowed(TEXT("int F(const Item Value) { return Value.Read(); }"), Host.Context, Host.Set.Get());
		asCBuilder AllowedBuilder(Allowed.Snapshot, Allowed.Diagnostics, Allowed.Options);
		const bool AllowedBuilt = AllowedBuilder.RunThrough();
		ASSERT_THAT(IsTrue(AllowedBuilt, Allowed.DescribeFailure(AllowedBuilder)));
		FBuilderInput Denied(TEXT("int F(const Item Value) { return Value.Mutate(); }"), Host.Context, Host.Set.Get());
		asCBuilder DeniedBuilder(Denied.Snapshot, Denied.Diagnostics, Denied.Options);
		ASSERT_THAT(IsFalse(DeniedBuilder.RunThrough()));
	}

	TEST_METHOD(UnfrozenOrForeignImageCannotEnterLookup)
	{
		FHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		auto Draft = MakeUnique<asCModuleDefinitionSet>();
		ASSERT_THAT(IsTrue(Draft->SetIdentityContext(Host.Context) == asEMetadataResult::Succeeded));
		auto Snapshot = MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
		Snapshot->Freeze();
		AS_NAMESPACE_QUALIFIER asCSourceManager Sources(Snapshot);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Sources);
		AS_NAMESPACE_QUALIFIER asCCompilationSession Session(Snapshot, Diagnostics, TEXT("frozen-host"), Host.Context);
		ASSERT_THAT(IsFalse(Session.AddExternalDefinitions(Draft.Get())));
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Foreign = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		AS_NAMESPACE_QUALIFIER asCCompilationSession ForeignSession(Snapshot, Diagnostics, TEXT("frozen-host-foreign"), Foreign);
		ASSERT_THAT(IsFalse(ForeignSession.AddExternalDefinitions(Host.Set.Get())));
		Host.Add->name = "Tampered";
		ASSERT_THAT(IsFalse(Session.AddExternalDefinitions(Host.Set.Get())));
	}

	TEST_METHOD(OmittedHostDefaultWithoutTypedValueIsUnsupported)
	{
		FHost Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		asCString* Default = asNEW(asCString)("7");
		Host.Add->defaultArgs.PushLast(Default);
		FBuilderInput Input(TEXT("int F() { return Host::Add(); }"), Host.Context, Host.Set.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		const bool Built = Builder.RunThrough();
		FString Errors;
		for (uint8 Stage = 0; Stage < uint8(AS_NAMESPACE_QUALIFIER asEBuilderStage::Failed); ++Stage)
			if (const auto* Result = Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage(Stage)))
				Errors += Result->Error + TEXT(" ") + Result->Text;
		ASSERT_THAT(IsFalse(Built, Errors));
	}

	TEST_METHOD(ReleasedCallerImageDoesNotInvalidateSelectedHostFunction)
	{
		TUniquePtr<asCModuleDefinitionSet> HostSet;
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		{
			FHost Host;
			ASSERT_THAT(IsTrue(Host.Initialize()));
			Key = Host.Add->GetStableKey();
			FBuilderInput Input(TEXT("int F() { return Host::Add(42); }"), Host.Context, Host.Set.Get());
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			const bool Built = Builder.RunThrough();
			ASSERT_THAT(IsTrue(Built, Input.DescribeFailure(Builder)));
			HostSet = MoveTemp(Host.Set);
		}
		ASSERT_THAT(IsTrue(bool(HostSet)));
		ASSERT_THAT(IsNotNull(HostSet->FindFunction(Key)));
	}
};
#endif
