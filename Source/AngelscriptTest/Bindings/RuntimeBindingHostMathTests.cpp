#include "CQTest.h"
#include "Core/AngelscriptBindsInternal.h"
#include "as_callfunc.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	void HostMathRegisterMissing(FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionDirectForTarget("void Missing()", asSFuncPtr(), asCALL_CDECL, ASAutoCaller::FunctionCaller::Make());
	}

	FAngelscriptBindRecord HostMathMakeRecord(FName BindName, EAngelscriptBindPhase Phase, FAngelscriptBindCallback Callback, int32 Line)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("AngelscriptTest");
		Record.BindName = BindName;
		Record.Phase = Phase;
		Record.SourceFile = "RuntimeBindingHostMathTests.cpp";
		Record.SourceLine = Line;
		Record.Callback = Callback;
		return Record;
	}

	struct FHostMathEngineOwner
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FHostMathEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
	};

	struct FHostMathSite
	{
		const TCHAR* Source;
		const TCHAR* Symbol;
		const TCHAR* Evidence;
	};

	const FHostMathSite GHostMathSites[] = {
#include "RuntimeBindingHostMathSites.inl"
	};

	bool HostMathAppendNamed(UE::Angelscript::Private::FAngelscriptBindCollection& Collection, FName BindName, FString& Diagnostic)
	{
		for (const FAngelscriptBindRecord& Record : FAngelscriptBind::GetRegisteredCollection().GetRecords())
		{
			if (Record.BindName == BindName)
			{
				return Collection.Append(Record, Diagnostic);
			}
		}
		Diagnostic = FString::Printf(TEXT("missing production bind %s"), *BindName.ToString());
		return false;
	}

	bool HostMathAppendProduction(UE::Angelscript::Private::FAngelscriptBindCollection& Collection, FString& Diagnostic)
	{
		return HostMathAppendNamed(Collection, TEXT("FVector.TypeDeclarations"), Diagnostic)
			&& HostMathAppendNamed(Collection, TEXT("FVector.TypeInfrastructure"), Diagnostic)
			&& HostMathAppendNamed(Collection, TEXT("FVector"), Diagnostic)
			&& HostMathAppendNamed(Collection, TEXT("FVector.ToStringContribution"), Diagnostic);
	}

	asCScriptFunction* HostMathFindNamed(asCObjectType* Type, const char* Name, int32 ParamCount)
	{
		if (!Type || !Name) return nullptr;
		for (asUINT Index = 0; Index < Type->GetMethodCount(); ++Index)
		{
			asIScriptFunction* Method = Type->GetMethodByIndex(Index);
			if (Method && Method->GetName() && FCStringAnsi::Strcmp(Method->GetName(), Name) == 0
				&& static_cast<int32>(Method->GetParamCount()) == ParamCount)
			{
				return static_cast<asCScriptFunction*>(Method);
			}
		}
		return nullptr;
	}

	void HostMathCallVoid(asCScriptFunction& Function, void** Args)
	{
		const auto Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->caller.IsBound()) return;
		if (Interface->caller.type == 2) Interface->caller.MethodCaller(Interface->method, Args, nullptr);
		else Interface->caller.FunctionCaller(Interface->func, Args, nullptr);
	}

	FVector HostMathCallAdd(asCScriptFunction& Function, FVector& Left, const FVector& Right)
	{
		FVector Result(ForceInitToZero);
		const auto Interface = Function.GetProcessInterface();
		if (!Interface || !Interface->caller.IsBound()) return Result;
		void* Args[2] = { &Left, const_cast<FVector*>(&Right) };
		if (Interface->caller.type == 2) Interface->caller.MethodCaller(Interface->method, Args, &Result);
		else Interface->caller.FunctionCaller(Interface->func, Args, &Result);
		return Result;
	}
}

TEST_CLASS_WITH_FLAGS(HostMath, "Angelscript.UnitTest.Bindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(MathBoundBehavior)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostMathAppendProduction(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		const bool bHostReady = Collection.ExecuteToHost(Diagnostic);
		ASSERT_THAT(IsTrue(bHostReady, Diagnostic.IsEmpty() ? TEXT("<empty host diagnostic>") : *Diagnostic));
		asCObjectType* VectorType = static_cast<asCObjectType*>(Collection.GetHostDefinitions()->FindType("FVector"));
		ASSERT_THAT(IsNotNull(VectorType));
		asCScriptFunction* CtorZero = HostMathFindNamed(VectorType, "f", 0);
		asCScriptFunction* CtorXyz = HostMathFindNamed(VectorType, "f", 3);
		asCScriptFunction* Add = HostMathFindNamed(VectorType, "opAdd", 1);
		ASSERT_THAT(IsNotNull(CtorZero));
		ASSERT_THAT(IsNotNull(CtorXyz));
		ASSERT_THAT(IsNotNull(Add));
		FVector Left(ForceInitToZero);
		FVector Right(ForceInitToZero);
		FVector Zero(1.0, 1.0, 1.0);
		double X = 1.0, Y = 2.0, Z = 3.0;
		void* ZeroArgs[1] = { &Zero };
		HostMathCallVoid(*CtorZero, ZeroArgs);
		ASSERT_THAT(IsTrue(Zero.Equals(FVector::ZeroVector)));
		void* LeftArgs[4] = { &Left, &X, &Y, &Z };
		HostMathCallVoid(*CtorXyz, LeftArgs);
		double X2 = 4.0, Y2 = 5.0, Z2 = 6.0;
		void* RightArgs[4] = { &Right, &X2, &Y2, &Z2 };
		HostMathCallVoid(*CtorXyz, RightArgs);
		const FVector Sum = HostMathCallAdd(*Add, Left, Right);
		ASSERT_THAT(IsTrue(Sum.Equals(FVector(5.0, 7.0, 9.0))));
		FHostMathEngineOwner A;
		FHostMathEngineOwner B;
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(AreEqual(1, Collection.GetCallbackInvocationCount(TEXT("FVector"))));
	}

	TEST_METHOD(DoubleVectorAdditionReturnsFiveSevenNine)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostMathAppendProduction(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), *Diagnostic));
		asCObjectType* VectorType = static_cast<asCObjectType*>(Collection.GetHostDefinitions()->FindType("FVector"));
		asCScriptFunction* Add = HostMathFindNamed(VectorType, "opAdd", 1);
		ASSERT_THAT(IsNotNull(Add));
		FVector Left(1.0, 2.0, 3.0);
		FVector Right(4.0, 5.0, 6.0);
		const FVector Sum = HostMathCallAdd(*Add, Left, Right);
		ASSERT_THAT(IsTrue(Sum.Equals(FVector(5.0, 7.0, 9.0))));
	}

	TEST_METHOD(CompleteFamilyAccounting)
	{
		ASSERT_THAT(AreEqual(118, static_cast<int32>(UE_ARRAY_COUNT(GHostMathSites))));
		int32 Executable = 0, MetadataOnly = 0, Companion = 0;
		for (const FHostMathSite& Site : GHostMathSites)
		{
			if (FCString::Strcmp(Site.Evidence, TEXT("executable")) == 0) ++Executable;
			else if (FCString::Strcmp(Site.Evidence, TEXT("metadata_only")) == 0) ++MetadataOnly;
			else ++Companion;
		}
		ASSERT_THAT(AreEqual(2, Executable));
		ASSERT_THAT(AreEqual(80, MetadataOnly));
		ASSERT_THAT(AreEqual(36, Companion));
		UE::Angelscript::Private::FAngelscriptBindCollection Collection;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostMathAppendProduction(Collection, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Collection.ExecuteToHost(Diagnostic), *Diagnostic));
		const int32 Before = Collection.GetCallbackInvocationCount(TEXT("FVector"));
		FHostMathEngineOwner A;
		FHostMathEngineOwner B;
		asCDefinitionGraphLease Lease = Collection.RetainHostGraph();
		ASSERT_THAT(IsTrue(A.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(B.Ptr->InjectDefinitions(*Lease) == asERegistrationResult::Succeeded));
		ASSERT_THAT(AreEqual(Before, Collection.GetCallbackInvocationCount(TEXT("FVector"))));
	}

	TEST_METHOD(OwnerAndFailureBoundaries)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Healthy;
		FString Diagnostic;
		ASSERT_THAT(IsTrue(HostMathAppendProduction(Healthy, Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.Finalize(Diagnostic), *Diagnostic));
		ASSERT_THAT(IsTrue(Healthy.ExecuteToHost(Diagnostic), *Diagnostic));
		asCScriptFunction* Add = HostMathFindNamed(static_cast<asCObjectType*>(Healthy.GetHostDefinitions()->FindType("FVector")), "opAdd", 1);
		FVector Left(1.0, 2.0, 3.0);
		const FVector Right(4.0, 5.0, 6.0);
		ASSERT_THAT(IsTrue(HostMathCallAdd(*Add, Left, Right).Equals(FVector(5.0, 7.0, 9.0))));
		UE::Angelscript::Private::FAngelscriptBindCollection Failed;
		ASSERT_THAT(IsTrue(Failed.Append(HostMathMakeRecord(TEXT("HostMath.Missing"), EAngelscriptBindPhase::ExplicitBindings, &HostMathRegisterMissing, 20), Diagnostic)));
		ASSERT_THAT(IsTrue(Failed.Finalize(Diagnostic)));
		ASSERT_THAT(IsFalse(Failed.ExecuteToHost(Diagnostic)));
		ASSERT_THAT(IsNull(Failed.GetHostDefinitions()));
		ASSERT_THAT(IsTrue(HostMathCallAdd(*Add, Left, Right).Equals(FVector(5.0, 7.0, 9.0))));
	}
};

#endif
