#include "CQTest.h"

#include "NativeEngine/NativeEngineTestSupport.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_decl.h"
#include "frontend/Compile/as_compilation_session.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace DelegateDeclarationsTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	struct FMatrixRow
	{
		const TCHAR* Spelling = nullptr;
		const TCHAR* TypeName = nullptr;
		const TCHAR* Source = nullptr;
		AS_NAMESPACE_QUALIFIER asECallableFlavor Flavor = AS_NAMESPACE_QUALIFIER asECallableFlavor::Ordinary;
		bool bMulticast = false;
		bool bRetVal = false;
		int32 Arity = 0;
		bool bNamedPairs = false;
	};

	const TCHAR* AritySuffix(int32 Arity)
	{
		switch (Arity)
		{
		case 1: return TEXT("_OneParam");
		case 2: return TEXT("_TwoParams");
		case 3: return TEXT("_ThreeParams");
		case 4: return TEXT("_FourParams");
		case 5: return TEXT("_FiveParams");
		case 6: return TEXT("_SixParams");
		case 7: return TEXT("_SevenParams");
		case 8: return TEXT("_EightParams");
		case 9: return TEXT("_NineParams");
		default: return TEXT("");
		}
	}

	FString IntList(int32 Count, bool bNamed)
	{
		FString Out;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Index) Out += TEXT(", ");
			Out += TEXT("int");
			if (bNamed) Out += FString::Printf(TEXT(", A%d"), Index);
		}
		return Out;
	}

	FString MakeSource(const TCHAR* Family, bool bRetVal, bool bNamed, int32 Arity, const TCHAR* TypeName)
	{
		FString Spelling = Family;
		if (bRetVal) Spelling += TEXT("_RetVal");
		Spelling += AritySuffix(Arity);
		FString Args;
		if (bRetVal)
		{
			Args = TEXT("int, ");
			Args += TypeName;
			if (Arity > 0)
			{
				Args += TEXT(", ");
				Args += IntList(Arity, bNamed);
			}
		}
		else
		{
			Args = TypeName;
			if (Arity > 0)
			{
				Args += TEXT(", ");
				Args += IntList(Arity, bNamed);
			}
		}
		return FString::Printf(TEXT("%s(%s);"), *Spelling, *Args);
	}

	const AS_NAMESPACE_QUALIFIER asCCallableTypeDecl* FindCallable(const FSessionRun& Run, const TCHAR* Name)
	{
		const TConstArrayView<AS_NAMESPACE_QUALIFIER asCDecl*> Found = Run.Session->Lookup(Name);
		return Found.IsEmpty() ? nullptr : AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCallableTypeDecl>(Found[0]);
	}

	bool MatchesRow(const AS_NAMESPACE_QUALIFIER asCCallableTypeDecl* Callable, const FMatrixRow& Row, FString& OutError)
	{
		if (!Callable)
		{
			OutError = FString::Printf(TEXT("missing callable %s for %s"), Row.TypeName, Row.Spelling);
			return false;
		}
		if (Callable->GetFlavor() != Row.Flavor)
		{
			OutError = FString::Printf(TEXT("%s flavor"), Row.Spelling);
			return false;
		}
		if (Callable->IsMulticast() != Row.bMulticast)
		{
			OutError = FString::Printf(TEXT("%s cast"), Row.Spelling);
			return false;
		}
		const FString ReturnSpelling = Callable->GetReturnTypeSyntax().GetSpelling();
		const bool bIntReturn = ReturnSpelling == TEXT("int") || ReturnSpelling == TEXT("int32");
		if (Row.bRetVal != bIntReturn || (!Row.bRetVal && ReturnSpelling != TEXT("void")))
		{
			OutError = FString::Printf(TEXT("%s return %s"), Row.Spelling, *ReturnSpelling);
			return false;
		}
		if (Callable->GetParameters().Num() != Row.Arity)
		{
			OutError = FString::Printf(TEXT("%s arity %d"), Row.Spelling, Callable->GetParameters().Num());
			return false;
		}
		if (Row.Arity > 0)
		{
			const FString FirstName = Callable->GetParameters()[0]->GetName();
			if (Row.bNamedPairs)
			{
				if (FirstName.IsEmpty())
				{
					OutError = FString::Printf(TEXT("%s expected named pair"), Row.Spelling);
					return false;
				}
			}
			else if (!FirstName.IsEmpty())
			{
				OutError = FString::Printf(TEXT("%s expected unnamed type, got %s"), Row.Spelling, *FirstName);
				return false;
			}
		}
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(DelegateDeclarations, "Angelscript.UnitTest.NativeEngine.Sema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SixtyRowMatrix)
	{
		using namespace DelegateDeclarationsTests;
		TArray<FMatrixRow> Rows;
		const struct FFamily
		{
			const TCHAR* Prefix;
			AS_NAMESPACE_QUALIFIER asECallableFlavor Flavor;
			bool bMulticast;
			bool bRetVal;
			bool bNamed;
			int32 MaxArity;
		} Families[] = {
			{TEXT("DECLARE_DELEGATE"), AS_NAMESPACE_QUALIFIER asECallableFlavor::Ordinary, false, false, false, 9},
			{TEXT("DECLARE_DELEGATE"), AS_NAMESPACE_QUALIFIER asECallableFlavor::Ordinary, false, true, false, 9},
			{TEXT("DECLARE_MULTICAST_DELEGATE"), AS_NAMESPACE_QUALIFIER asECallableFlavor::Ordinary, true, false, false, 9},
			{TEXT("DECLARE_DYNAMIC_DELEGATE"), AS_NAMESPACE_QUALIFIER asECallableFlavor::Dynamic, false, false, true, 9},
			{TEXT("DECLARE_DYNAMIC_DELEGATE"), AS_NAMESPACE_QUALIFIER asECallableFlavor::Dynamic, false, true, true, 9},
			{TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE"), AS_NAMESPACE_QUALIFIER asECallableFlavor::Dynamic, true, false, true, 9},
		};
		int32 TypeIndex = 0;
		for (const FFamily& Family : Families)
		{
			for (int32 Arity = 0; Arity <= Family.MaxArity; ++Arity)
			{
				FMatrixRow Row;
				FString Spelling = Family.Prefix;
				if (Family.bRetVal) Spelling += TEXT("_RetVal");
				Spelling += AritySuffix(Arity);
				Row.Spelling = nullptr;
				const FString TypeName = FString::Printf(TEXT("FType%d"), TypeIndex++);
				const FString Source = MakeSource(Family.Prefix, Family.bRetVal, Family.bNamed, Arity, *TypeName);
				FSessionRun Run({{TEXT("Matrix.as"), Source}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations(), *Source));
				const auto* Callable = FindCallable(Run, *TypeName);
				FString Error;
				Row.TypeName = *TypeName;
				Row.Flavor = Family.Flavor;
				Row.bMulticast = Family.bMulticast;
				Row.bRetVal = Family.bRetVal;
				Row.Arity = Arity;
				Row.bNamedPairs = Family.bNamed;
				ASSERT_THAT(IsTrue(MatchesRow(Callable, Row, Error), *FString::Printf(TEXT("%s: %s"), *Spelling, *Error)));
			}
		}
	}

	TEST_METHOD(OrdinaryZeroAndOne)
	{
		DelegateDeclarationsTests::FSessionRun Run({{TEXT("Ordinary.as"),
			TEXT("DECLARE_DELEGATE(FVoid); DECLARE_DELEGATE_OneParam(FOne, int);")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		const auto* Zero = DelegateDeclarationsTests::FindCallable(Run, TEXT("FVoid"));
		const auto* One = DelegateDeclarationsTests::FindCallable(Run, TEXT("FOne"));
		ASSERT_THAT(IsNotNull(Zero));
		ASSERT_THAT(IsNotNull(One));
		ASSERT_THAT(IsTrue(Zero->GetFlavor() == AS_NAMESPACE_QUALIFIER asECallableFlavor::Ordinary));
		ASSERT_THAT(IsTrue(One->GetFlavor() == AS_NAMESPACE_QUALIFIER asECallableFlavor::Ordinary));
		ASSERT_THAT(IsFalse(Zero->IsMulticast()));
		ASSERT_THAT(IsFalse(One->IsMulticast()));
		ASSERT_THAT(AreEqual(0, Zero->GetParameters().Num()));
		ASSERT_THAT(AreEqual(1, One->GetParameters().Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("void")), Zero->GetReturnTypeSyntax().GetSpelling()));
	}

	TEST_METHOD(OrdinaryRetValNine)
	{
		DelegateDeclarationsTests::FSessionRun Run({{TEXT("Nine.as"),
			TEXT("DECLARE_DELEGATE_RetVal_NineParams(int, FNine, int, int, int, int, int, int, int, int, int);")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		const auto* Nine = DelegateDeclarationsTests::FindCallable(Run, TEXT("FNine"));
		ASSERT_THAT(IsNotNull(Nine));
		ASSERT_THAT(IsTrue(Nine->GetFlavor() == AS_NAMESPACE_QUALIFIER asECallableFlavor::Ordinary));
		ASSERT_THAT(IsFalse(Nine->IsMulticast()));
		ASSERT_THAT(AreEqual(FString(TEXT("int")), Nine->GetReturnTypeSyntax().GetSpelling()));
		ASSERT_THAT(AreEqual(9, Nine->GetParameters().Num()));
	}

	TEST_METHOD(DynamicZeroAndOne)
	{
		DelegateDeclarationsTests::FSessionRun Run({{TEXT("Dynamic.as"),
			TEXT("DECLARE_DYNAMIC_DELEGATE(FDyn); DECLARE_DYNAMIC_DELEGATE_OneParam(FDynOne, int, Value);")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		const auto* Zero = DelegateDeclarationsTests::FindCallable(Run, TEXT("FDyn"));
		const auto* One = DelegateDeclarationsTests::FindCallable(Run, TEXT("FDynOne"));
		ASSERT_THAT(IsNotNull(Zero));
		ASSERT_THAT(IsNotNull(One));
		ASSERT_THAT(IsTrue(Zero->GetFlavor() == AS_NAMESPACE_QUALIFIER asECallableFlavor::Dynamic));
		ASSERT_THAT(IsTrue(One->GetFlavor() == AS_NAMESPACE_QUALIFIER asECallableFlavor::Dynamic));
		ASSERT_THAT(AreEqual(1, One->GetParameters().Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("Value")), One->GetParameters()[0]->GetName()));
	}

	TEST_METHOD(MulticastVoidOnlyAdmit)
	{
		DelegateDeclarationsTests::FSessionRun Run({{TEXT("Multi.as"),
			TEXT("DECLARE_MULTICAST_DELEGATE_OneParam(FOn, int);")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		const auto* On = DelegateDeclarationsTests::FindCallable(Run, TEXT("FOn"));
		ASSERT_THAT(IsNotNull(On));
		ASSERT_THAT(IsTrue(On->GetFlavor() == AS_NAMESPACE_QUALIFIER asECallableFlavor::Ordinary));
		ASSERT_THAT(IsTrue(On->IsMulticast()));
		ASSERT_THAT(AreEqual(FString(TEXT("void")), On->GetReturnTypeSyntax().GetSpelling()));
	}

	TEST_METHOD(ExistingVoidParameterControl)
	{
		DelegateDeclarationsTests::FSessionRun Run({{TEXT("VoidParameters.as"), TEXT("void Run(void);")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		const auto* Function = Run.Function(TEXT("Run"));
		ASSERT_THAT(IsNotNull(Function));
		ASSERT_THAT(AreEqual(0, Function->GetParameters().Num()));
	}
};

#endif
