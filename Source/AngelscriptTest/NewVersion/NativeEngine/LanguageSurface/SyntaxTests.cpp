#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "../Compiler/NativeSourceExecutionTestSupport.h"
#include "frontend/AST/as_type_context.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(Syntax, "Angelscript.UnitTest.NativeEngine.LanguageSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	static bool HasFeatureDiagnostic(const FSessionRun& Run, const TCHAR* Feature, const TCHAR* Marker)
	{
		asCCollectingDiagnosticConsumer Consumer;
		Run.Diagnostics.EmitTo(Consumer);
		for (const asSDiagnosticRecord& Record : Consumer.GetRecords())
		{
			if (Record.DiagnosticID != 3008 || Record.Severity != asEDiagnosticSeverity::Error
				|| Record.Arguments.Num() != 1 || Record.Arguments[0].TextValue != Feature) continue;
			TConstArrayView<uint8> Bytes;
			if (!Run.Snapshot->TryGetBytes(Record.PrimaryRange.Begin.FileID, Bytes)
				|| Record.PrimaryRange.End.ByteOffset > static_cast<asUINT>(Bytes.Num())) continue;
			FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + Record.PrimaryRange.Begin.ByteOffset),
				Record.PrimaryRange.Length());
			if (FString(Text.Length(), Text.Get()) == Marker) return true;
		}
		return false;
	}

	struct FHostTypes
	{
		TSharedRef<asCTypeContext, ESPMode::ThreadSafe> Types = MakeShared<asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCModuleDefinitionSet> Set = MakeUnique<asCModuleDefinitionSet>();

		bool Initialize()
		{
			if (Set->SetIdentityContext(Types) != asEMetadataResult::Succeeded) return false;
			for (const TPair<FString, uint32>& Entry : TArray<TPair<FString, uint32>>{{TEXT("TArray"), 1}, {TEXT("TMap"), 2}})
			{
				asSNominalTypeDescriptor Descriptor;
				Descriptor.StableScope = TEXT("language-surface-host");
				Descriptor.Kind = asETypeDeclKind::Struct;
				Descriptor.Name = Entry.Key;
				Descriptor.GenericArity = Entry.Value;
				asSStableKey Key;
				asCObjectType* Type = nullptr;
				const auto Name = StringCast<ANSICHAR>(*Entry.Key);
				if (Types->GetIdentityRegistry().InternNominalType(Descriptor, Key) != asETypeIdentityStatus::Succeeded
					|| Set->CreateObjectType(Key, Name.Get(), "", asOBJ_VALUE | asOBJ_TEMPLATE, Type) != asEMetadataResult::Succeeded
					|| Set->SetTypeDeclarationFacts(*Type, asETypeDeclKind::Struct, Entry.Value) != asEMetadataResult::Succeeded
					|| Set->SetFixedTemplateLayout(*Type, 16, 8) != asEMetadataResult::Succeeded) return false;
			}
			asSStableKey IntKey, YieldKey;
			Types->InternPrimitiveTypeUse(asEPrimitiveType::Int32, IntKey);
			asSFunctionIdentityDescriptor Yield;
			Yield.StableScope = TEXT("language-surface-host");
			Yield.Name = TEXT("yield");
			Yield.Kind = asEFunctionIdentityKind::Global;
			Yield.ReturnType = IntKey;
			asCScriptFunction* Function = nullptr;
			return Types->GetIdentityRegistry().InternFunction(Yield, YieldKey) == asETypeIdentityStatus::Succeeded
				&& Set->CreateFunction(YieldKey, "yield", "", asFUNC_SYSTEM, Function) == asEMetadataResult::Succeeded
				&& Set->DefineFunction(*Function, Set->Primitive(ttInt), {}) == asEMetadataResult::Succeeded
				&& Set->SetFunctionDeclarationFacts(*Function, asEFunctionIdentityKind::Global) == asEMetadataResult::Succeeded
				&& Set->FinalizeLayouts() == asEMetadataResult::Succeeded
				&& Set->Freeze() == asEMetadataResult::Succeeded;
		}
	};

public:
	TEST_METHOD(ModuleModifiersReportFeatureAndKeepFollowingDeclaration)
	{
		for (const TCHAR* Source : {TEXT("shared class C {}"), TEXT("external class C {}"),
			TEXT("shared interface I {}"), TEXT("shared int Bad(){return 1;}"), TEXT("external int Bad();")})
		{
			FSessionRun Run({{TEXT("Removed.as"), FString(Source) + TEXT(" int Good(){return 7;}")}});
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			ASSERT_THAT(IsNotNull(Run.Function(TEXT("Good"))));
			ASSERT_THAT(IsTrue(HasFeatureDiagnostic(Run, TEXT("ModuleSharing"),
				FString(Source).StartsWith(TEXT("shared")) ? TEXT("shared") : TEXT("external")), Source));
		}
	}

	TEST_METHOD(FuncdefReportsItsDeclarationFeature)
	{
		FSessionRun Run({{TEXT("Funcdef.as"), TEXT("funcdef int Callback(int); int Good(){return 7;}")}});
		ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
		ASSERT_THAT(IsNotNull(Run.Function(TEXT("Good"))));
		ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
		ASSERT_THAT(IsTrue(HasFeatureDiagnostic(Run, TEXT("FuncdefDeclaration"), TEXT("funcdef"))));
	}

	TEST_METHOD(ExceptionStatementsReportFeatureAtKeyword)
	{
		for (const TPair<FString, FString>& Case : TArray<TPair<FString, FString>>{
			{TEXT("try { Work(); } catch {}"), TEXT("try")}, {TEXT("catch {}"), TEXT("catch")},
			{TEXT("throw 1;"), TEXT("throw")}})
		{
			FSessionRun Run({{TEXT("Exceptions.as"), TEXT("void Work(){} void Bad(){") + Case.Key + TEXT("} int Good(){return 7;}")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			ASSERT_THAT(IsNotNull(Run.Function(TEXT("Good"))));
			ASSERT_THAT(IsTrue(HasFeatureDiagnostic(Run, TEXT("ScriptException"), *Case.Value), Case.Key));
		}
	}

	TEST_METHOD(CoroutineStatementsReportFeatureAtKeyword)
	{
		for (const TPair<FString, FString>& Case : TArray<TPair<FString, FString>>{
			{TEXT("yield;"), TEXT("yield")}, {TEXT("yield 1;"), TEXT("yield")},
			{TEXT("coroutine { Work(); }"), TEXT("coroutine")}})
		{
			FSessionRun Run({{TEXT("Coroutine.as"), TEXT("void Work(){} void Bad(){") + Case.Key + TEXT("} int Good(){return 7;}")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			ASSERT_THAT(IsNotNull(Run.Function(TEXT("Good"))));
			ASSERT_THAT(IsTrue(HasFeatureDiagnostic(Run, TEXT("ScriptCoroutine"), *Case.Value), Case.Key));
		}
	}

	TEST_METHOD(UserTemplatesReportFeatureAndRecoverPastBodies)
	{
		for (const TCHAR* Source : {TEXT("template<class T> class Box { T Value; }"),
			TEXT("template<class T> T Identity(T Value){return Value;}"), TEXT("template<> class Box<int> {}")})
		{
			FSessionRun Run({{TEXT("Template.as"), FString(Source) + TEXT(" int Good(){return 7;}")}});
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			ASSERT_THAT(IsNotNull(Run.Function(TEXT("Good"))));
			ASSERT_THAT(IsTrue(HasFeatureDiagnostic(Run, TEXT("UserTemplate"), TEXT("template")), Source));
		}
	}

	TEST_METHOD(VirtualPropertiesReportFeatureWithoutSynthesizingMembers)
	{
		for (const TPair<FString, FString>& Case : TArray<TPair<FString, FString>>{
			{TEXT("int Value { get { return 1; } set {} }"), TEXT("{")},
			{TEXT("int get_Value() property {return 1;}"), TEXT("property")}})
		{
			FSessionRun Run({{TEXT("Property.as"), TEXT("class Item {") + Case.Key + TEXT("} int Good(){return 7;}")}});
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			ASSERT_THAT(IsNotNull(Run.Function(TEXT("Good"))));
			ASSERT_THAT(IsTrue(HasFeatureDiagnostic(Run, TEXT("VirtualProperty"), *Case.Value), Case.Key));
			auto* Item = cast<asCRecordDecl>(Run.Session->GetTranslationUnit()->GetDecls()[0]);
			ASSERT_THAT(AreEqual(0, Item->Lookup(TEXT("Value")).Num()));
			ASSERT_THAT(AreEqual(0, Item->Lookup(TEXT("get_Value")).Num()));
		}
	}

	TEST_METHOD(InactiveRemovedSyntaxProducesNoFeatureErrors)
	{
		const FString Source = TEXT("#if REMOVED\nshared class C {}\nexternal int F();\nfuncdef int Callback();\n")
			TEXT("template<class T> class Box {}\nint Value {get{return 1;}}\nvoid Bad(){try{}catch{} throw 1; yield;}\n")
			TEXT("#endif\nint Good(){return 7;}");
		FSessionRun Run({{TEXT("Inactive.as"), Source}}, {{TEXT("REMOVED"), false}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		ASSERT_THAT(AreEqual(asUINT(0), Run.Diagnostics.GetPendingCount()));
	}

	TEST_METHOD(OrdinaryNamesAndCrossFileCallsExecuteSeven)
	{
		FSourceExecutionInput Input(TArray<TPair<FString, FString>>{
			{TEXT("Calls.as"), TEXT("int Good(){return get()+set()+get_Value()+shared()+external();}")},
			{TEXT("Names.as"), TEXT("int get(){return 1;} int set(){return 2;} int get_Value(){return 3;} int shared(){return 1;} int external(){return 0;}")}});
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Good = FindSourceFunction(Builder, "Good");
		ASSERT_THAT(IsNotNull(Good));
		auto Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		ON_SCOPE_EXIT { Snapshot->Release(); };
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(0, Context->Prepare(Good)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord())));
	}

	TEST_METHOD(OrdinaryGetterDoesNotCreateImplicitProperty)
	{
		FSessionRun Run({{TEXT("NoImplicitProperty.as"), TEXT("class Item {int get_Value(){return 7;} int Bad(){return Value;}}")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
	}

	TEST_METHOD(RegisteredNestedHostTypesAndYieldNamedCallbackRemainAvailable)
	{
		FHostTypes Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		FSourceExecutionInput Input(TEXT("struct Grid {TMap<int,TArray<TArray<int>>> Values;} int Good(){return yield();}"), Host.Types, Host.Set.Get());
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		ASSERT_THAT(IsTrue(Builder.GetModuleDefinitionSet()->ValidateFrozenDefinitions()));
		ASSERT_THAT(IsNotNull(FindSourceFunction(Builder, "Good")));
	}

	TEST_METHOD(UnknownHostProviderAndWrongArityDoNotPublish)
	{
		FHostTypes Host;
		ASSERT_THAT(IsTrue(Host.Initialize()));
		for (const TCHAR* Type : {TEXT("Unknown<int>"), TEXT("TArray<int,int>"), TEXT("TMap<int>")})
		{
			FSourceExecutionInput Input(FString::Printf(TEXT("struct Bad {%s Value;}"), Type), Host.Types, Host.Set.Get());
			asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
			ASSERT_THAT(IsFalse(Builder.RunThrough(), Type));
		}
	}

	TEST_METHOD(CastIntrinsicRetainsExplicitTypedConversion)
	{
		FSessionRun Run({{TEXT("Cast.as"), TEXT("float64 Good(){return Cast<float64>(7);}")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		auto* Body = cast<asCCompoundStmt>(Run.Function(TEXT("Good"))->GetBody());
		auto* Return = cast<asCReturnStmt>(Body->GetStatements()[0]);
		ASSERT_THAT(IsNotNull(dyn_cast<asCExplicitCastExpr>(Return->GetValue())));
	}
};

#endif
