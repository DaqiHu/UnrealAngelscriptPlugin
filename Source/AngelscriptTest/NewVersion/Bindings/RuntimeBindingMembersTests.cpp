#include "CQTest.h"
#include "Core/AngelscriptTypeBindInfoApply.h"
#include "as_scriptengine.h"
#include "as_objecttype.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
BEGIN_AS_NAMESPACE
namespace BindingMemberTests
{
TEST_CLASS_WITH_FLAGS(Members, "Angelscript.UnitTest.RuntimeBindings.Types",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	struct FEngine
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FEngine() { if (Ptr) Ptr->ShutDownAndRelease(); }
	};
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptTypeBindInfoInstallation> Installed;
	FString Diagnostic;
	FAngelscriptTypeBindHandle Type(const TCHAR* Name = TEXT("Pair"))
	{
		FAngelscriptTypeBindDeclaration D; D.Name = Name; D.Namespace = TEXT("Fixture");
		D.NativeSize = 8; D.NativeAlignment = 4; D.ObjectFlags = asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS;
		D.Source = {TEXT("FixtureModule"), TEXT("Types"), TEXT("Fixture/Types.cpp"), 21};
		FAngelscriptTypeBindHandle Handle; check(Store->RecordType(D, Handle, Diagnostic)); return Handle;
	}
	FAngelscriptTypeBindHandle Scope()
	{
		FAngelscriptTypeBindDeclaration D; D.Kind = EAngelscriptTypeBindKind::Namespace; D.Namespace = TEXT("Fixture");
		D.Source = {TEXT("FixtureModule"), TEXT("Globals"), TEXT("Fixture/Globals.cpp"), 31};
		FAngelscriptTypeBindHandle Handle; check(Store->RecordType(D, Handle, Diagnostic)); return Handle;
	}
	FAngelscriptTypeBindMemberHandle Add(FAngelscriptTypeBindHandle Owner, const TCHAR* Text,
		EAngelscriptTypeBindMemberKind Kind = EAngelscriptTypeBindMemberKind::Method)
	{
		FAngelscriptTypeBindInfoMember M; M.Declaration = Text; M.Kind = Kind;
		M.Source = {TEXT("FixtureModule"), TEXT("MembersProvider"), TEXT("Fixture/Members.cpp"), 57};
		FAngelscriptTypeBindMemberHandle Handle; check(Store->RecordMember(Owner, M, Handle, Diagnostic)); return Handle;
	}
	void Edit(FAngelscriptTypeBindMemberHandle Handle, TFunctionRef<void(FAngelscriptTypeBindInfoMember&)> Change)
	{
		auto M = *Store->GetMember(Handle); Change(M); ASSERT_THAT(IsTrue(Store->UpdateMember(Handle, M, Diagnostic)));
	}
	bool Install(FEngine& Engine)
	{
		return Store->Seal(Diagnostic) && FAngelscriptTypeBindInfoApply::Install(Store, *Engine.Ptr, Installed, Diagnostic);
	}
	TEST_METHOD(MethodAndGlobalOverloadsAreVisibleAfterWholeImageInstall)
	{
		const auto Pair = Type(); const auto Sum = Add(Pair, TEXT("int Sum() const")); const auto Globals = Scope();
		const auto IntAdd = Add(Globals, TEXT("int Add(int A, int B = 2)"), EAngelscriptTypeBindMemberKind::GlobalFunction);
		const auto DoubleAdd = Add(Globals, TEXT("double Add(double A, double B)"), EAngelscriptTypeBindMemberKind::GlobalFunction);
		FEngine Engine; ASSERT_THAT(IsTrue(Install(Engine)));
		ASSERT_THAT(IsTrue(Installed->GetSet()->GetState() == asEDefinitionSetState::Attached));
		auto* PairType = static_cast<asCObjectType*>(Engine.Ptr->FindMetadataType("Fixture", "Pair")); ASSERT_THAT(IsNotNull(PairType));
		ASSERT_THAT(IsTrue(PairType == Installed->FindType(TEXT("Pair"), TEXT("Fixture"))));
		auto* Method = Installed->FindFunction(Sum); ASSERT_THAT(IsNotNull(Method));
		ASSERT_THAT(IsTrue(Method->objectType == PairType)); ASSERT_THAT(IsTrue(Method->IsReadOnly()));
		auto* First = Installed->FindFunction(IntAdd); auto* Second = Installed->FindFunction(DoubleAdd);
		ASSERT_THAT(IsNotNull(First)); ASSERT_THAT(IsNotNull(Second));
		ASSERT_THAT(IsTrue(First->GetStableKey() != Second->GetStableKey()));
		ASSERT_THAT(IsTrue(Engine.Ptr->FindMetadataFunction(First->GetStableKey()) == First));
		ASSERT_THAT(IsTrue(Engine.Ptr->FindMetadataFunction(Second->GetStableKey()) == Second));
		ASSERT_THAT(AreEqual(3, Installed->GetSet()->GetFunctions().Num()));
	}
	TEST_METHOD(DefaultsDirectionsAndNativeFunctionTraitsSurviveFreeze)
	{
		const auto Method = Add(Type(), TEXT("void Fill(int&out Value, int&inout Other, int Count = 2) const"));
		Edit(Method, [](auto& M) { M.Traits = EAngelscriptBindFunctionTrait::NoDiscard | EAngelscriptBindFunctionTrait::Deprecated |
			EAngelscriptBindFunctionTrait::WorldContext | EAngelscriptBindFunctionTrait::ForceConstArguments;
			M.DeprecationMessage = TEXT("Use FillNew"); M.bProtected = true; M.DeterminesOutputTypeArgument = 1; });
		FEngine Engine; ASSERT_THAT(IsTrue(Install(Engine)));
		auto* Function = Installed->FindFunction(Method); ASSERT_THAT(IsNotNull(Function));
		ASSERT_THAT(AreEqual(asUINT(3), Function->parameterTypes.GetLength()));
		ASSERT_THAT(IsTrue(Function->inOutFlags[0] == asTM_OUTREF)); ASSERT_THAT(IsTrue(Function->inOutFlags[1] == asTM_INOUTREF));
		ASSERT_THAT(IsTrue(Function->parameterTypes[0].IsReference()));
		ASSERT_THAT(IsTrue(Function->parameterNames[0] == "Value"));
		ASSERT_THAT(IsNotNull(Function->defaultArgs[2])); ASSERT_THAT(IsTrue(*Function->defaultArgs[2] == "2"));
		ASSERT_THAT(IsTrue(Function->traits.GetTrait(asTRAIT_NODISCARD)));
		ASSERT_THAT(IsTrue(Function->traits.GetTrait(asTRAIT_DEPRECATED)));
		ASSERT_THAT(IsTrue(Function->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT)));
		ASSERT_THAT(IsTrue(Function->traits.GetTrait(asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS)));
		ASSERT_THAT(IsTrue(Function->IsProtected())); ASSERT_THAT(AreEqual(int8(1), Function->determinesOutputTypeArgumentIndex));
#if WITH_EDITOR
		ASSERT_THAT(IsTrue(Function->deprecationMessage == "Use FillNew"));
#endif
	}
	TEST_METHOD(PropertyAccessAndOffsetsAreCompleteBeforePublication)
	{
		const auto Property = Add(Type(), TEXT("int X"), EAngelscriptTypeBindMemberKind::Property);
		Edit(Property, [](auto& M) { M.PropertyOffset = 4; M.bCanWrite = false; M.bCanEdit = false; M.bProtected = true;
			M.Traits = EAngelscriptBindFunctionTrait::EditorOnly | EAngelscriptBindFunctionTrait::Deprecated; M.DeprecationMessage = TEXT("Old X"); });
		FEngine Engine; ASSERT_THAT(IsTrue(Install(Engine)));
		auto* Pair = static_cast<asCObjectType*>(Installed->FindType(TEXT("Pair"), TEXT("Fixture")));
		auto* X = Pair->GetFirstProperty("X"); ASSERT_THAT(IsNotNull(X));
		ASSERT_THAT(AreEqual(4, X->byteOffset)); ASSERT_THAT(IsTrue(X->Readable())); ASSERT_THAT(IsFalse(X->Writeable()));
		ASSERT_THAT(IsFalse(X->Editable())); ASSERT_THAT(IsTrue(X->isProtected)); ASSERT_THAT(IsTrue(X->isAppBindProperty));
#if WITH_EDITOR
		ASSERT_THAT(IsTrue(X->isEditorOnly)); ASSERT_THAT(IsTrue(X->isDeprecated)); ASSERT_THAT(IsTrue(X->DeprecationMessage == "Old X"));
#endif
	}
	TEST_METHOD(ConstructorsAndDestructorKeepDistinctBehaviourIdentities)
	{
		const auto Pair = Type();
		const auto Ctor = Add(Pair, TEXT("void f()"), EAngelscriptTypeBindMemberKind::Constructor);
		const auto Copy = Add(Pair, TEXT("void f(const Pair&in Other)"), EAngelscriptTypeBindMemberKind::Constructor);
		const auto Dtor = Add(Pair, TEXT("void f()"), EAngelscriptTypeBindMemberKind::Destructor);
		Edit(Ctor, [](auto& M) { M.Native.Behaviour = asBEHAVE_CONSTRUCT; });
		Edit(Copy, [](auto& M) { M.Native.Behaviour = asBEHAVE_CONSTRUCT; });
		Edit(Dtor, [](auto& M) { M.Native.Behaviour = asBEHAVE_DESTRUCT; });
		FEngine Engine; ASSERT_THAT(IsTrue(Install(Engine)));
		auto* C = Installed->FindFunction(Ctor); auto* D = Installed->FindFunction(Dtor); ASSERT_THAT(IsNotNull(C)); ASSERT_THAT(IsNotNull(D));
		ASSERT_THAT(IsTrue(C->GetStableKey() != D->GetStableKey()));
		ASSERT_THAT(IsTrue(C->traits.GetTrait(asTRAIT_CONSTRUCTOR))); ASSERT_THAT(IsTrue(D->traits.GetTrait(asTRAIT_DESTRUCTOR)));
		auto* PairType = static_cast<asCObjectType*>(Installed->FindType(TEXT("Pair"), TEXT("Fixture")));
		ASSERT_THAT(AreEqual(asUINT(3), PairType->GetBehaviourCount()));
	}
	TEST_METHOD(ConstAndMutableGlobalDeclarationsHaveMetadataWithoutStorage)
	{
		const auto Globals = Scope(); const auto Constant = Add(Globals, TEXT("const int Answer"), EAngelscriptTypeBindMemberKind::GlobalProperty);
		Edit(Constant, [](auto& M) { M.bPureConstant = true; M.ConstantValue = 42; M.Traits = EAngelscriptBindFunctionTrait::EditorOnly; });
		Add(Globals, TEXT("int Counter"), EAngelscriptTypeBindMemberKind::GlobalProperty);
		FEngine Engine; ASSERT_THAT(IsTrue(Install(Engine)));
		auto* Answer = Engine.Ptr->FindMetadataGlobalProperty("Fixture", "Answer");
		auto* Counter = Engine.Ptr->FindMetadataGlobalProperty("Fixture", "Counter");
		ASSERT_THAT(IsNotNull(Answer)); ASSERT_THAT(IsNotNull(Counter));
		ASSERT_THAT(IsTrue(Answer->type.IsReadOnly())); ASSERT_THAT(IsFalse(Counter->type.IsReadOnly()));
		ASSERT_THAT(IsTrue(Answer->isPureConstant)); ASSERT_THAT(AreEqual(asQWORD(42), Answer->storage));
#if WITH_EDITOR
		ASSERT_THAT(IsTrue(Answer->isEditorOnly));
#endif
		ASSERT_THAT(IsNull(Answer->GetAddressOfValue())); ASSERT_THAT(IsNull(Counter->GetAddressOfValue()));
		ASSERT_THAT(AreEqual(2, Installed->GetSet()->GetGlobalProperties().Num()));
		ASSERT_THAT(IsTrue(Installed->GetSet()->SetNativeGlobalFacts(*Answer, {}) == asEMetadataResult::InvalidState));
		ASSERT_THAT(AreEqual(asQWORD(42), Answer->storage));
	}
	TEST_METHOD(PureConstantRejectsMutableAndObjectStorageBeforePublication)
	{
		for (const TCHAR* Declaration : {TEXT("int Invalid"), TEXT("const Pair Invalid")})
		{
			Store = MakeShared<FAngelscriptTypeBindInfoStore>(); Type();
			const auto Constant = Add(Scope(), Declaration, EAngelscriptTypeBindMemberKind::GlobalProperty);
			Edit(Constant, [](auto& M) { M.bPureConstant = true; M.ConstantValue = 42; });
			FEngine Engine; ASSERT_THAT(IsFalse(Install(Engine))); ASSERT_THAT(IsFalse(Installed.IsValid()));
			ASSERT_THAT(IsNull(Engine.Ptr->FindMetadataType("Fixture", "Pair")));
			ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Invalid")))); ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("MembersProvider"))));
		}
	}
	TEST_METHOD(MissingParameterTypePublishesNoImageAndReportsMemberSource)
	{
		const auto Pair = Type(); Add(Pair, TEXT("int Sum()")); Add(Pair, TEXT("void Bad(MissingType Value)"));
		FEngine Engine; ASSERT_THAT(IsFalse(Install(Engine))); ASSERT_THAT(IsFalse(Installed.IsValid()));
		ASSERT_THAT(IsNull(Engine.Ptr->FindMetadataType("Fixture", "Pair")));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("MissingType")))); ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("MembersProvider"))));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Fixture/Members.cpp:57"))));
	}
	TEST_METHOD(IncompatibleDuplicateSignatureFailsBeforeRegistration)
	{
		const auto Globals = Scope(); Add(Globals, TEXT("int Value(int X)"), EAngelscriptTypeBindMemberKind::GlobalFunction);
		Add(Globals, TEXT("float Value(int X)"), EAngelscriptTypeBindMemberKind::GlobalFunction); Type();
		FEngine Engine; ASSERT_THAT(IsFalse(Install(Engine))); ASSERT_THAT(IsFalse(Installed.IsValid()));
		ASSERT_THAT(IsNull(Engine.Ptr->FindMetadataType("Fixture", "Pair")));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Value")))); ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("MembersProvider"))));
	}
	TEST_METHOD(SecondEngineOwnsDistinctImageWithEqualStableIdentities)
	{
		const auto Method = Add(Type(), TEXT("int Sum() const")); FEngine First; FEngine Second;
		ASSERT_THAT(IsTrue(Install(First))); auto Original = MoveTemp(Installed);
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoApply::Install(Store, *Second.Ptr, Installed, Diagnostic)));
		ASSERT_THAT(IsTrue(Original->GetSet() != Installed->GetSet()));
		ASSERT_THAT(IsTrue(Original->FindType(TEXT("Pair"), TEXT("Fixture")) != Installed->FindType(TEXT("Pair"), TEXT("Fixture"))));
		ASSERT_THAT(IsTrue(Original->FindFunction(Method)->GetStableKey() == Installed->FindFunction(Method)->GetStableKey()));
		First.Ptr->ShutDownAndRelease(); First.Ptr = nullptr;
		ASSERT_THAT(IsTrue(Installed->GetSet()->GetBoundEngine() == Second.Ptr));
		ASSERT_THAT(IsTrue(Second.Ptr->FindMetadataFunction(Installed->FindFunction(Method)->GetStableKey()) == Installed->FindFunction(Method)));
	}
	TEST_METHOD(FrozenInstallationRejectsLaterMemberMutation)
	{
		const auto Method = Add(Type(), TEXT("int Sum() const")); FEngine Engine; ASSERT_THAT(IsTrue(Install(Engine)));
		auto Set = Installed->GetSet(); const auto Count = Set->GetFunctions().Num();
		asCScriptFunction* Rejected = nullptr;
		ASSERT_THAT(IsTrue(Set->CreateFunction(Installed->FindFunction(Method)->GetStableKey(), "Later", "Fixture", asFUNC_SYSTEM, Rejected) == asEMetadataResult::InvalidState));
		ASSERT_THAT(IsNull(Rejected)); ASSERT_THAT(AreEqual(Count, Set->GetFunctions().Num()));
		const auto OriginalTraits = Installed->FindFunction(Method)->traits.traits;
		ASSERT_THAT(IsTrue(Set->SetNativeFunctionFacts(*Installed->FindFunction(Method), {}) == asEMetadataResult::InvalidState));
		ASSERT_THAT(AreEqual(OriginalTraits, Installed->FindFunction(Method)->traits.traits));
	}
	TEST_METHOD(CompilePoliciesUseCapturedBuildFacts)
	{
		for (int32 Mode = 0; Mode < 4; ++Mode)
		{
			FAngelscriptBindRecordingPolicy Policy;
			Policy.bTest = Mode == 0; Policy.bShipping = Mode == 1; Policy.bSimulateCooked = Mode == 2;
			Policy.bLogging = Mode != 3; Policy.bForceConstWithinDevelopmentOnlyFunctions = true;
			Store = MakeShared<FAngelscriptTypeBindInfoStore>(Policy); const auto Globals = Scope();
			struct FExpected { const TCHAR* Declaration; EAngelscriptBindCompileOut Policy; asECompileOutType Result; bool ForceConst; };
			const FExpected Cases[] = {
				{TEXT("bool InTest(bool Value)"), EAngelscriptBindCompileOut::InTest,
					Mode != 3 ? asECompileOutType::CompileOutEntirely : asECompileOutType::CompileCalls, true},
				{TEXT("bool Check(bool Value)"), EAngelscriptBindCompileOut::Check,
					Mode == 1 || Mode == 2 ? asECompileOutType::CompileOutEntirely : asECompileOutType::CompileCalls, true},
				{TEXT("bool Ensure(bool Value)"), EAngelscriptBindCompileOut::Ensure,
					Mode == 1 || Mode == 2 ? asECompileOutType::ReplaceWithFirstParam : asECompileOutType::CompileCalls, false},
				{TEXT("bool Replace(bool Value)"), EAngelscriptBindCompileOut::ReplaceFirstArgumentInTest,
					Mode != 3 ? asECompileOutType::ReplaceWithFirstParam : asECompileOutType::CompileCalls, false},
				{TEXT("bool IfNoLog(bool Value)"), EAngelscriptBindCompileOut::IfNoLog, asECompileOutType::CompileOutEntirely, true},
			};
			TArray<FAngelscriptTypeBindMemberHandle> Handles;
			for (const auto& Expected : Cases)
			{
				const auto Handle = Add(Globals, Expected.Declaration, EAngelscriptTypeBindMemberKind::GlobalFunction);
				Edit(Handle, [&](auto& M) { M.CompileOut = Expected.Policy; }); Handles.Add(Handle);
			}
			FEngine Engine; ASSERT_THAT(IsTrue(Install(Engine)));
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Cases); ++Index)
			{
				auto* Function = Installed->FindFunction(Handles[Index]); ASSERT_THAT(IsNotNull(Function));
				ASSERT_THAT(IsTrue(Function->compileOutType == Cases[Index].Result));
				ASSERT_THAT(AreEqual(Cases[Index].ForceConst, Function->traits.GetTrait(asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS)));
			}
		}
	}
};
}
END_AS_NAMESPACE
#endif
