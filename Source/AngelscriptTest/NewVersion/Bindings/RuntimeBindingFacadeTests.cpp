#include "CQTest.h"
#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptTypeBindInfoStore.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallDescriptor.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(Facade, "Angelscript.UnitTest.RuntimeBindings.Recording",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FPair
	{
		int32 X = 0;
		int32 Y = 0;
		int32 Sum() const { return X + Y; }
	};
	static void Construct(FPair* Value, int32 X, int32 Y) { new(Value) FPair{X, Y}; }
	static void Destruct(FPair* Value) { Value->~FPair(); }
	static int32 Add(int32 A, int32 B) { return A + B; }
	static int32 Zero() { return 0; }
	static int32 ExternalSum(FPair* Value) { return Value->Sum(); }
	static void Generic(asIScriptGeneric*) {}
	static bool TemplateCallback(asITypeInfo*, bool&) { return true; }

	static const FAngelscriptTypeBindInfoMember* FindMember(const FAngelscriptTypeBindInfoStore& Records,
		const TCHAR* Owner, const TCHAR* Declaration, const TCHAR* Namespace = TEXT(""))
	{
		const auto Handle = *Owner ? Records.FindType(Owner, Namespace) : Records.FindScope(Namespace);
		const auto* Type = Records.GetType(Handle);
		return Type ? Type->Members.FindByPredicate([Declaration](const auto& Member)
			{ return Member.Declaration == Declaration; }) : nullptr;
	}

public:
	TEST_METHOD(ValueLifecycleAndMembersNeedNoEngine)
	{
		FAngelscriptTypeBindInfoStore Records;
		FAngelscriptBinds Binds(Records);
		FBindFlags Flags; Flags.bPOD = true;
		auto Pair = Binds.ValueClassForTarget<FPair>("Pair", Flags);
		auto Constructor = Pair.Constructor("void f(int X, int Y)", &Construct);
		Pair.Destructor("void f()", &Destruct);
		auto Method = Pair.Method("int Sum() const", &FPair::Sum).NoDiscard();
		auto Property = Pair.Property("int X", &FPair::X);
		ASSERT_THAT(IsTrue(Pair.IsRecording()));
		ASSERT_THAT(IsNull(Pair.GetTypeInfo()));
		ASSERT_THAT(IsTrue(Constructor.IsValid()));
		ASSERT_THAT(IsTrue(Method.IsValid()));
		ASSERT_THAT(IsTrue(Property.IsValid()));
		ASSERT_THAT(IsNull(Method.GetFunction()));
		const auto* Type = Records.GetType(Records.FindType(TEXT("Pair")));
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(AreEqual(uint32(sizeof(FPair)), Type->Declaration.NativeSize));
		ASSERT_THAT(AreEqual(uint32(alignof(FPair)), Type->Declaration.NativeAlignment));
		ASSERT_THAT(IsTrue((Type->Declaration.ObjectFlags & asOBJ_POD) != 0));
		ASSERT_THAT(AreEqual(4, Type->Members.Num()));
		ASSERT_THAT(IsTrue(Type->Members[0].Kind == EAngelscriptTypeBindMemberKind::Constructor));
		ASSERT_THAT(IsTrue(Type->Members[0].Native.Behaviour == asBEHAVE_CONSTRUCT));
		ASSERT_THAT(IsTrue(Type->Members[0].Native.CallConv == asCALL_CDECL_OBJFIRST));
		ASSERT_THAT(IsTrue(Type->Members[1].Kind == EAngelscriptTypeBindMemberKind::Destructor));
		ASSERT_THAT(IsTrue(Type->Members[2].Native.CallConv == asCALL_THISCALL));
		ASSERT_THAT(IsTrue(Type->Members[2].Native.Caller.IsBound()));
		ASSERT_THAT(IsTrue(EnumHasAnyFlags(Type->Members[2].Traits, EAngelscriptBindFunctionTrait::NoDiscard)));
		ASSERT_THAT(AreEqual(SIZE_T(STRUCT_OFFSET(FPair, X)), Type->Members[3].PropertyOffset));
	}

	TEST_METHOD(NamespacesEnumsAndConstantsAreRecorded)
	{
		FAngelscriptTypeBindInfoStore Records;
		FAngelscriptBinds Binds(Records);
		int32 Value = 7;
		{
			FAngelscriptBinds::FNamespace Namespace(Binds, "Fixture");
			auto Mode = Binds.EnumForTarget("Mode");
			Mode["Off"] = 0;
			Mode["On"] = 1;
			ASSERT_THAT(IsNull(Mode.GetTypeInfo()));
			Binds.BindGlobalVariableForTarget("const int Value", &Value).PureConstant(7);
			Binds.BindGlobalFunctionForTarget("int Add(int A, int B)", &Add);
		}
		Binds.BindGlobalFunctionForTarget("int Root()", &Zero);
		const auto* Mode = Records.GetType(Records.FindType(TEXT("Mode"), TEXT("Fixture")));
		ASSERT_THAT(IsNotNull(Mode));
		ASSERT_THAT(IsTrue(Mode->Declaration.Kind == EAngelscriptTypeBindKind::Enum));
		ASSERT_THAT(AreEqual(2, Mode->Members.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("On")), Mode->Members[1].Declaration));
		ASSERT_THAT(AreEqual(1, Mode->Members[1].EnumValue));
		const auto* Constant = FindMember(Records, TEXT(""), TEXT("const int Value"), TEXT("Fixture"));
		ASSERT_THAT(IsNotNull(Constant));
		ASSERT_THAT(IsTrue(Constant->GlobalAddress == &Value));
		ASSERT_THAT(IsTrue(Constant->bPureConstant));
		ASSERT_THAT(AreEqual(uint64(7), Constant->ConstantValue));
		ASSERT_THAT(IsNotNull(FindMember(Records, TEXT(""), TEXT("int Root()"))));
		ASSERT_THAT(IsNull(FindMember(Records, TEXT(""), TEXT("int Root()"), TEXT("Fixture"))));
	}

	TEST_METHOD(CallerVariantsAndPropertyExposureSurviveRecording)
	{
		FAngelscriptTypeBindInfoStore Records;
		FAngelscriptBinds Binds(Records);
		auto Pair = Binds.ValueClassForTarget<FPair>("Pair", {});
		int32 Auxiliary = 9;
		Binds.BindGlobalFunctionForTarget("int Add(int A, int B)", &Add, &Auxiliary);
		Binds.BindGlobalGenericFunctionForTarget("void Generic()", &Generic, &Auxiliary);
		Pair.Method("int External() const", &ExternalSum, &Auxiliary);
		Pair.GenericMethod("void GenericMember()", &Generic, &Auxiliary);
		Binds.BindMethodDirectForTarget("Pair", "int Direct() const", asMETHOD(FPair, Sum),
			asCALL_THISCALL, ASAutoCaller::MakeFunctionCaller(&FPair::Sum), &Auxiliary);
		Binds.BindGlobalFunctionDirectForTarget("int DirectGlobal(int A, int B)", asFUNCTION(Add),
			asCALL_CDECL, ASAutoCaller::MakeFunctionCaller(&Add), &Auxiliary);
		FAngelscriptType::FBindParams Access; Access.bCanWrite = false; Access.bCanEdit = false; Access.bProtected = true;
		Pair.Property("int Y", STRUCT_OFFSET(FPair, Y), Access);
		Pair.Method("int Protected() const", &ExternalSum, Access, &Auxiliary);
		const auto* Global = FindMember(Records, TEXT(""), TEXT("int Add(int A, int B)"));
		ASSERT_THAT(IsNotNull(Global));
		ASSERT_THAT(IsTrue(Global->Native.Auxiliary == &Auxiliary));
		ASSERT_THAT(IsTrue(Global->Native.Function.ptr.f.func == asFUNCTION(Add).ptr.f.func));
		ASSERT_THAT(IsTrue(Global->Native.Caller.IsBound()));
		const auto* GenericCall = FindMember(Records, TEXT(""), TEXT("void Generic()"));
		ASSERT_THAT(IsNotNull(GenericCall));
		ASSERT_THAT(IsTrue(GenericCall->Native.CallConv == asCALL_GENERIC));
		const auto* External = FindMember(Records, TEXT("Pair"), TEXT("int External() const"));
		ASSERT_THAT(IsNotNull(External));
		ASSERT_THAT(IsTrue(External->Native.CallConv == asCALL_CDECL_OBJFIRST));
		const auto* Direct = FindMember(Records, TEXT("Pair"), TEXT("int Direct() const"));
		ASSERT_THAT(IsNotNull(Direct));
		ASSERT_THAT(IsTrue(Direct->Native.CallConv == asCALL_THISCALL));
		const auto* Property = FindMember(Records, TEXT("Pair"), TEXT("int Y"));
		ASSERT_THAT(IsNotNull(Property));
		ASSERT_THAT(IsTrue(Property->bCanRead && !Property->bCanWrite && !Property->bCanEdit && Property->bProtected));
		const auto* Protected = FindMember(Records, TEXT("Pair"), TEXT("int Protected() const"));
		ASSERT_THAT(IsNotNull(Protected));
		ASSERT_THAT(IsTrue(Protected->bProtected && !Protected->bCanWrite));
	}

	TEST_METHOD(FunctionTraitsAndCompilePoliciesAreDetached)
	{
		FAngelscriptTypeBindInfoStore Records;
		FAngelscriptBinds Binds(Records);
		auto Marked = Binds.BindGlobalFunctionForTarget("int Marked()", &Zero);
		Marked.EditorOnly().Deprecated("Use Next").PropertyAccessor().GeneratedAccessor().NoDiscard()
			.WorldContext().Callable(false).ImplicitConstructor().ForceConstArgumentExpressions()
			.DeterminesOutputType(1).PassScriptFunctionAsFirstParam().Documentation(TEXT("A native value"), TEXT("Fixture"));
		const auto* Member = FindMember(Records, TEXT(""), TEXT("int Marked()"));
		ASSERT_THAT(IsNotNull(Member));
		const auto Expected = EAngelscriptBindFunctionTrait::EditorOnly | EAngelscriptBindFunctionTrait::Deprecated
			| EAngelscriptBindFunctionTrait::PropertyAccessor | EAngelscriptBindFunctionTrait::GeneratedAccessor
			| EAngelscriptBindFunctionTrait::NoDiscard | EAngelscriptBindFunctionTrait::WorldContext
			| EAngelscriptBindFunctionTrait::NotCallable | EAngelscriptBindFunctionTrait::ImplicitConstructor
			| EAngelscriptBindFunctionTrait::ForceConstArguments;
		ASSERT_THAT(IsTrue(EnumHasAllFlags(Member->Traits, Expected)));
		ASSERT_THAT(IsTrue(Member->FirstParameter == EAngelscriptBindFirstParameter::ScriptFunction));
		ASSERT_THAT(AreEqual(1, Member->DeterminesOutputTypeArgument));
		ASSERT_THAT(AreEqual(FString(TEXT("Use Next")), Member->DeprecationMessage));
		ASSERT_THAT(AreEqual(FString(TEXT("A native value")), Member->Documentation));
		Marked.NoDiscard(false).Callable().PassScriptObjectTypeAsFirstParam();
		Member = FindMember(Records, TEXT(""), TEXT("int Marked()"));
		ASSERT_THAT(IsFalse(EnumHasAnyFlags(Member->Traits, EAngelscriptBindFunctionTrait::NoDiscard | EAngelscriptBindFunctionTrait::NotCallable)));
		ASSERT_THAT(IsTrue(Member->FirstParameter == EAngelscriptBindFirstParameter::ScriptObjectType));
		using FModifier = FAngelscriptBoundFunction& (FAngelscriptBoundFunction::*)();
		struct FCase { const char* Declaration; FModifier Modifier; EAngelscriptBindCompileOut Policy; };
		const FCase Cases[] = {
			{"int Entirely()", &FAngelscriptBoundFunction::CompileOutEntirely, EAngelscriptBindCompileOut::Entirely},
			{"int Chain()", &FAngelscriptBoundFunction::CompileOutAsMethodChain, EAngelscriptBindCompileOut::MethodChain},
			{"int InTest()", &FAngelscriptBoundFunction::CompileOutInTest, EAngelscriptBindCompileOut::InTest},
			{"int NoLog()", &FAngelscriptBoundFunction::CompileOutIfNoLog, EAngelscriptBindCompileOut::IfNoLog},
			{"int Ensure()", &FAngelscriptBoundFunction::CompileOutAsEnsure, EAngelscriptBindCompileOut::Ensure},
			{"int Check()", &FAngelscriptBoundFunction::CompileOutAsCheck, EAngelscriptBindCompileOut::Check},
			{"int Replace()", &FAngelscriptBoundFunction::ReplaceWithFirstArgInTest, EAngelscriptBindCompileOut::ReplaceFirstArgumentInTest},
		};
		for (const auto& Case : Cases)
		{
			auto Function = Binds.BindGlobalFunctionForTarget(Case.Declaration, &Zero);
			(Function.*Case.Modifier)();
			const auto* Recorded = FindMember(Records, TEXT(""), ANSI_TO_TCHAR(Case.Declaration));
			ASSERT_THAT(IsNotNull(Recorded));
			ASSERT_THAT(IsTrue(Recorded->CompileOut == Case.Policy));
		}
	}

	TEST_METHOD(NativeRecipesOwnTheirInputsAndPreserveOrder)
	{
		FAngelscriptTypeBindInfoStore Records;
		FAngelscriptBinds Binds(Records);
		auto Function = Binds.BindGlobalFunctionForTarget("int Native()", &Zero);
		Function.NativeConstructor("Pair", true, "new($) Pair").NativeDestructor("Pair", true)
			.NativeAssignment("Pair", true).NativeUObjectCast(TEXT("UObject"), true)
			.NativeMethod("Sum", true).NativeFunction("Zero", true).NativeFunctionHeader("Zero", "Fixture.h", true)
			.NativeUFunction(nullptr, TEXT("NativeZero"), true).NativeTArrayIndex().NativeTArrayIteratorCreate()
			.NativeTArrayIteratorProceed().NativeTemplateInstantiatedCall("Invoke", true, true, true)
			.NativeDelegateExecute().NativeMulticastExecute().NativeEventFunctionExecute().NativePushArgument().NativePushArgumentRef();
#if AS_CAN_GENERATE_JIT
		{
			FAngelscriptStaticJITNativeCallDescriptor Descriptor;
			Descriptor.Symbol = TEXT("Fixture::Zero");
			Function.ExternalNativeCall(Descriptor);
			Descriptor.Symbol = TEXT("Changed");
		}
#endif
		const auto* Member = FindMember(Records, TEXT(""), TEXT("int Native()"));
		ASSERT_THAT(IsNotNull(Member));
		ASSERT_THAT(IsTrue(Member->NativeRecipes.Num() >= 17));
		ASSERT_THAT(IsTrue(Member->NativeRecipes[0].Kind == EAngelscriptNativeBindRecipeKind::Constructor));
		ASSERT_THAT(AreEqual(FString(TEXT("new($) Pair")), Member->NativeRecipes[0].CustomForm));
		ASSERT_THAT(IsTrue(Member->NativeRecipes[3].bGuaranteed));
		ASSERT_THAT(AreEqual(FString(TEXT("Fixture.h")), Member->NativeRecipes[6].Header));
		ASSERT_THAT(IsTrue(Member->NativeRecipes[11].bNeedsCompare && Member->NativeRecipes[11].bNeedsCopy));
		ASSERT_THAT(IsTrue(Member->NativeRecipes[16].Kind == EAngelscriptNativeBindRecipeKind::PushArgumentRef));
#if AS_CAN_GENERATE_JIT
		ASSERT_THAT(IsTrue(Member->NativeRecipes.Last().ExternalCall.IsValid()));
		ASSERT_THAT(AreEqual(FString(TEXT("Fixture::Zero")), Member->NativeRecipes.Last().ExternalCall->Symbol));
#endif
	}

	TEST_METHOD(HandlesSurviveGrowthAndRejectSealedEdits)
	{
		FAngelscriptTypeBindInfoStore Records;
		FAngelscriptBinds Binds(Records);
		auto Pair = Binds.ValueClassForTarget<FPair>("Pair", {});
		auto Method = Pair.Method("int Sum() const", &FPair::Sum);
		for (int32 Index = 0; Index != 64; ++Index)
		{
			Binds.ValueClassForTarget<FPair>(FString::Printf(TEXT("Pair%d"), Index), {});
			Pair.Method(FString::Printf(TEXT("int Extra%d() const"), Index), &FPair::Sum);
		}
		Method.NoDiscard();
		ASSERT_THAT(IsTrue(Method.IsValid()));
		FString Diagnostic;
		ASSERT_THAT(IsTrue(Records.Seal(Diagnostic)));
		Method.NoDiscard(false);
		ASSERT_THAT(IsFalse(Method.IsValid()));
		const auto* Member = FindMember(Records, TEXT("Pair"), TEXT("int Sum() const"));
		ASSERT_THAT(IsNotNull(Member));
		ASSERT_THAT(IsTrue(EnumHasAnyFlags(Member->Traits, EAngelscriptBindFunctionTrait::NoDiscard)));
		ASSERT_THAT(IsTrue(Records.IsSealed()));
	}

	TEST_METHOD(NamespaceAndCaseSensitiveTypesDoNotCollide)
	{
		FAngelscriptTypeBindInfoStore Records;
		FAngelscriptBinds Binds(Records);
		Binds.ValueClassForTarget<FPair>("Pair", {});
		Binds.ValueClassForTarget<FPair>("pair", {});
		{
			FAngelscriptBinds::FNamespace Namespace(Binds, "Other");
			Binds.ValueClassForTarget<FPair>("Pair", {});
		}
		ASSERT_THAT(AreEqual(3, Records.GetTypes().Num()));
		ASSERT_THAT(IsFalse(Records.FindType(TEXT("Pair")) == Records.FindType(TEXT("pair"))));
		ASSERT_THAT(IsFalse(Records.FindType(TEXT("Pair")) == Records.FindType(TEXT("Pair"), TEXT("Other"))));
		ASSERT_THAT(IsNull(Records.GetType(Records.FindType(TEXT("PAIR")))));
	}

	TEST_METHOD(ReferenceAndTemplateDeclarationsRetainUEFacts)
	{
		FAngelscriptTypeBindInfoStore Records;
		FAngelscriptBinds Binds(Records);
		auto Object = Binds.ReferenceClassForTarget("UObject", UObject::StaticClass());
		FBindFlags Flags; Flags.bTemplate = true; Flags.TemplateType = "<T>";
		auto Box = Binds.ValueClassForTarget<FPair>("Box<class T>", Flags);
		Box.TemplateCallback("bool f(int&in Type, bool&out NoGC)", &TemplateCallback);
		const auto* ObjectType = Records.GetType(Records.FindType(TEXT("UObject")));
		const auto* TemplateType = Records.GetType(Records.FindType(TEXT("Box")));
		ASSERT_THAT(IsNotNull(ObjectType));
		ASSERT_THAT(IsNotNull(TemplateType));
		ASSERT_THAT(IsTrue(ObjectType->Declaration.Kind == EAngelscriptTypeBindKind::ObjectHandle));
		ASSERT_THAT(IsTrue(ObjectType->Declaration.UnrealType == UObject::StaticClass()));
		ASSERT_THAT(IsTrue(TemplateType->Declaration.Kind == EAngelscriptTypeBindKind::Template));
		ASSERT_THAT(AreEqual(FString(TEXT("<T>")), TemplateType->Declaration.TemplateType));
		ASSERT_THAT(AreEqual(FString(TEXT("Box<class T>")), TemplateType->Declaration.TemplateDeclaration));
		ASSERT_THAT(AreEqual(1, TemplateType->Members.Num()));
		ASSERT_THAT(IsTrue(TemplateType->Members[0].Native.Behaviour == asBEHAVE_TEMPLATE_CALLBACK));
		ASSERT_THAT(IsTrue(TemplateType->Members[0].Native.CallConv == asCALL_CDECL));
	}
};

#endif
