#include "CQTest.h"
#include "frontend/as_binding_declaration.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
BEGIN_AS_NAMESPACE
namespace BindingDeclarationTests
{
TEST_CLASS_WITH_FLAGS(Declarations, "Angelscript.UnitTest.RuntimeBindings.Types",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	asCTypeContext Types;
	asSBindingDeclarationContext Environment;
	asSBindingDeclaration Result;
	asSBindingDeclarationDiagnostic Diagnostic;
	TMap<FString, asSStableKey> Nominals;
	void Prepare()
	{
		Environment.StableScope = TEXT("binding-declaration-fixture");
		Environment.Namespace = TEXT("Fixture");
		for (const TCHAR* Name : {TEXT("FVector"), TEXT("FString"), TEXT("UObject"), TEXT("TArray")})
		{
			asSNominalTypeDescriptor Descriptor;
			Descriptor.StableScope = Environment.StableScope;
			Descriptor.NamespacePath = { TEXT("Fixture") };
			Descriptor.Name = Name;
			Descriptor.Kind = FString(Name) == TEXT("UObject") ? asETypeDeclKind::Class : asETypeDeclKind::Struct;
			Descriptor.GenericArity = FString(Name) == TEXT("TArray") ? 1 : 0;
			asSStableKey Key;
			ASSERT_THAT(IsTrue(Types.GetIdentityRegistry().InternNominalType(Descriptor, Key) == asETypeIdentityStatus::Succeeded));
			Nominals.Add(Name, Key);
		}
	}
	bool Parse(const TCHAR* Text)
	{
		return asCBindingDeclarationParser::Parse(Text, Environment, Types,
			[this](const FString& Spelling, const FString& Namespace)
			{
				FString Name = Spelling;
				Name.RemoveFromStart(TEXT("::"), ESearchCase::CaseSensitive);
				Name.RemoveFromStart(TEXT("Fixture::"), ESearchCase::CaseSensitive);
				const asSStableKey* Found = Nominals.Find(Name);
				return Found ? *Found : asSStableKey{};
			}, Result, Diagnostic);
	}
	asSStableTypeUseDescriptor Resolve(const asSStableKey& Key)
	{
		asSStableTypeUseDescriptor Descriptor;
		Types.GetIdentityRegistry().ResolveTypeUse(Key, Descriptor);
		return Descriptor;
	}
	TEST_METHOD(ConstReferenceMethodAndDefaultAreDetached)
	{
		Prepare(); Environment.Owner = Nominals[TEXT("FVector")]; Environment.FunctionKind = asEFunctionIdentityKind::Method;
		ASSERT_THAT(IsTrue(Parse(TEXT("const FVector& Get(int Index = 2) const"))));
		ASSERT_THAT(AreEqual(TEXT("Get"), Result.Name));
		ASSERT_THAT(AreEqual(1, Result.Parameters.Num()));
		ASSERT_THAT(AreEqual(TEXT("Index"), Result.Parameters[0].Name));
		ASSERT_THAT(AreEqual(TEXT("2"), Result.Parameters[0].DefaultText));
		ASSERT_THAT(IsTrue(Result.Signature.Modifiers == asEFunctionIdentityModifiers::ConstMethod));
		ASSERT_THAT(IsTrue(Resolve(Result.Type).Qualifiers == (asETypeUseQualifiers::Const | asETypeUseQualifiers::Reference)));
		ASSERT_THAT(IsTrue(Result.Source->IsValidRange(Result.Parameters[0].Range)));
	}
	TEST_METHOD(ExplicitVoidAndReferenceDirections)
	{
		Prepare();
		ASSERT_THAT(IsTrue(Parse(TEXT("void Fill(FString&out Value, const FString&in Source, int&inout Count)"))));
		ASSERT_THAT(IsTrue(Resolve(Result.Type).Primitive == asEPrimitiveType::Void));
		ASSERT_THAT(AreEqual(3, Result.Parameters.Num()));
		ASSERT_THAT(IsTrue(Result.Parameters[0].Identity.PassingMode == asEParameterPassingMode::Out));
		ASSERT_THAT(IsTrue(Result.Parameters[1].Identity.PassingMode == asEParameterPassingMode::In));
		ASSERT_THAT(IsTrue(Result.Parameters[2].Identity.PassingMode == asEParameterPassingMode::InOut));
	}
	TEST_METHOD(NestedTemplatesAndObjectHandlesResolve)
	{
		Prepare();
		ASSERT_THAT(IsTrue(Parse(TEXT("TArray<TArray<int>> Convert(UObject@ Object)"))));
		const auto Outer = Resolve(Result.Type);
		ASSERT_THAT(AreEqual(1, Outer.Arguments.Num()));
		const auto Inner = Resolve(Outer.Arguments[0]);
		ASSERT_THAT(AreEqual(1, Inner.Arguments.Num()));
		ASSERT_THAT(IsTrue(Resolve(Inner.Arguments[0]).Primitive == asEPrimitiveType::Int32));
		ASSERT_THAT(IsTrue(Resolve(Result.Parameters[0].Identity.Type).Qualifiers == (asETypeUseQualifiers::Handle | asETypeUseQualifiers::Nullable)));
	}
	TEST_METHOD(EquivalentSpellingSharesIdentityAndRefChangesOverload)
	{
		Prepare();
		ASSERT_THAT(IsTrue(Parse(TEXT("double Read(FVector Value)")))); const auto First = Result.Function;
		ASSERT_THAT(IsTrue(Parse(TEXT("float64 Read(::Fixture::FVector Renamed)"))));
		ASSERT_THAT(IsTrue(First == Result.Function));
		ASSERT_THAT(IsTrue(Parse(TEXT("float64 Read(FVector& Value)"))));
		ASSERT_THAT(IsFalse(First == Result.Function));
		Environment.Namespace = TEXT("Other");
		ASSERT_THAT(IsTrue(Parse(TEXT("float64 Read(::Fixture::FVector Value)"))));
		ASSERT_THAT(IsFalse(First == Result.Function));
	}
	TEST_METHOD(DefaultExpressionsKeepNestedCommasAndOriginalText)
	{
		Prepare();
		ASSERT_THAT(IsTrue(Parse(TEXT("void Use(int A = Make(1, 2), FString B = \"a,b\")"))));
		ASSERT_THAT(AreEqual(2, Result.Parameters.Num()));
		ASSERT_THAT(AreEqual(TEXT("Make(1, 2)"), Result.Parameters[0].DefaultText));
		ASSERT_THAT(AreEqual(TEXT("\"a,b\""), Result.Parameters[1].DefaultText));
	}
	TEST_METHOD(PropertyAndStandaloneTypeUseCanonicalTypes)
	{
		Prepare();
		ASSERT_THAT(IsTrue(Parse(TEXT("FVector Location")))); const auto PropertyType = Result.Type;
		ASSERT_THAT(AreEqual(TEXT("Location"), Result.Name));
		ASSERT_THAT(IsTrue(Result.Function.IsZero()));
		ASSERT_THAT(IsTrue(Parse(TEXT("::Fixture::FVector"))));
		ASSERT_THAT(IsTrue(PropertyType == Result.Type));
		ASSERT_THAT(IsTrue(Result.Name.IsEmpty()));
	}
	TEST_METHOD(InvalidDeclarationsReportOwnedSourceRanges)
	{
		Prepare();
		for (const TCHAR* Text : { TEXT("Missing Value"), TEXT("TArray<TArray<int> Value"), TEXT("void Fill(int out Value)"), TEXT("void Fill(const int&out Value)"), TEXT("void Fill(void Value)"), TEXT("void Fill(int A = )"), TEXT("int F() garbage"), TEXT("TArray<int, int> Value") })
		{
			ASSERT_THAT(IsFalse(Parse(Text)));
			ASSERT_THAT(IsFalse(Diagnostic.Message.IsEmpty()));
			ASSERT_THAT(IsTrue(Result.Source.IsValid()));
			ASSERT_THAT(IsTrue(Result.Source->IsValidRange(Diagnostic.Range)));
			ASSERT_THAT(IsTrue(Result.Function.IsZero()));
		}
	}
	TEST_METHOD(ReturnTypeConflictCannotPublishCallable)
	{
		Prepare();
		ASSERT_THAT(IsTrue(Parse(TEXT("int Same()"))));
		ASSERT_THAT(IsFalse(Parse(TEXT("bool Same()"))));
		ASSERT_THAT(IsTrue(Diagnostic.Message.Contains(TEXT("return"))));
		ASSERT_THAT(IsTrue(Result.Function.IsZero()));
	}
};
}
END_AS_NAMESPACE
#endif
