#include "CQTest.h"

#include "AngelscriptTestEngine.h"
#include "Core/AngelscriptBinds.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallables.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallDescriptor.h"

#include "Misc/DateTime.h"
#include "Misc/Guid.h"

#include "as_callfunc.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITNativeCallLinkageTests,
	"Angelscript.TestModule.StaticJIT.NativeCallLinkage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNativeCallLinkageValue
	{
		int32 Value = 0;

		int32 Read() const
		{
			return Value;
		}
	};

	static int32 NativeABIAddOne(const int32 Value)
	{
		return Value + 1;
	}

	static FAngelscriptStaticJITScalarABIIdentity ScalarABI(
		const TCHAR* Canonical)
	{
		return FAngelscriptStaticJITScalarABIIdentity::FromCanonical(Canonical);
	}

	static FAngelscriptStaticJITNativeABIIdentity NativeABI(
		const TCHAR* Canonical)
	{
		return FAngelscriptStaticJITNativeABIIdentity::FromCanonical(Canonical);
	}

	static FAngelscriptStaticJITNativeCallValidationContext MakeContext(
		const FAngelscriptStaticJITScalarABIIdentity& ExpectedABI)
	{
		FAngelscriptStaticJITNativeCallValidationContext Context;
		Context.ExpectedScalarABI = ExpectedABI;
		Context.ExpectedNativeABI = NativeABI(TEXT(
			"cdecl:return=u32;params=(u32);receiver=none;hidden=none"));
		Context.KnownModules = {
			FName(TEXT("Core")),
			FName(TEXT("AngelscriptRuntime")),
			FName(TEXT("PrivateRuntime")),
		};
		Context.PublicDependencyModules = {
			FName(TEXT("Core")),
			FName(TEXT("AngelscriptRuntime")),
		};
		return Context;
	}

	static EAngelscriptStaticJITNativeCallLifetimeFlags SafeScalarLifetime()
	{
		return EAngelscriptStaticJITNativeCallLifetimeFlags::ByValueScalarOnly
			| EAngelscriptStaticJITNativeCallLifetimeFlags::NoRetainedReferences
			| EAngelscriptStaticJITNativeCallLifetimeFlags::NoEngineLocalCapture;
	}

	static FAngelscriptStaticJITNativeCallDescriptor MakeExportedSymbol(
		const FAngelscriptStaticJITScalarABIIdentity& ABI)
	{
		FAngelscriptStaticJITNativeCallDescriptor Descriptor;
		Descriptor.Linkage =
			EAngelscriptStaticJITNativeCallLinkage::ExportedSymbol;
		Descriptor.Visibility =
			EAngelscriptStaticJITNativeCallVisibility::External;
		Descriptor.Symbol = TEXT("FPlatformMath::CountBits");
		Descriptor.Include = TEXT("HAL/PlatformMath.h");
		Descriptor.OwningModule = FName(TEXT("Core"));
		Descriptor.ApiMacro = TEXT("CORE_API");
		Descriptor.NativeABI = NativeABI(TEXT(
			"cdecl:return=u32;params=(u32);receiver=none;hidden=none"));
		Descriptor.CppCallableSignature = TEXT("uint32(uint32)");
		Descriptor.ABIDomain =
			EAngelscriptStaticJITNativeCallABIDomain::Scalar;
		Descriptor.CompileOutPolicy =
			EAngelscriptStaticJITNativeCallCompileOutPolicy::None;
		Descriptor.DefaultArgumentPolicy =
			EAngelscriptStaticJITNativeCallDefaultArgumentPolicy::None;
		Descriptor.WorldContextPolicy =
			EAngelscriptStaticJITNativeCallWorldContextPolicy::None;
		Descriptor.ExceptionPolicy =
			EAngelscriptStaticJITNativeCallExceptionPolicy::DoesNotSetScriptException;
		Descriptor.ScalarABI = ABI;
		Descriptor.RouteFlags =
			EAngelscriptStaticJITNativeCallRouteFlags::DirectCall;
		Descriptor.LifetimeFlags = SafeScalarLifetime();
		return Descriptor;
	}

public:
	TEST_METHOD(DateTimeDaysInMonthUsesTheExactCoreExportedSymbol)
	{
		using FDaysInMonthCallable = int32(*)(int32, int32);
		const FDaysInMonthCallable ImportedCallable = &FDateTime::DaysInMonth;
		ASSERT_THAT(IsNotNull(ImportedCallable));

		FAngelscriptEngineConfig Config;
		Config.Purpose = EAngelscriptEnginePurpose::StaticJITGeneration;
		Config.bCollectStaticJITCompatibilityBinds = true;
		const FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine =
			FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsTrue(Engine.IsValid()));
		if (!Engine.IsValid())
		{
			return;
		}

		asIScriptFunction* RegisteredFunction = nullptr;
		asIScriptEngine* const ScriptEngine = Engine->GetScriptEngine();
		for (asUINT Index = 0;
			Index < ScriptEngine->GetGlobalFunctionCount();
			++Index)
		{
			asIScriptFunction* const Candidate =
				ScriptEngine->GetGlobalFunctionByIndex(Index);
			if (Candidate != nullptr
				&& FCStringAnsi::Strcmp(Candidate->GetName(), "DaysInMonth") == 0
				&& FCStringAnsi::Strcmp(
					Candidate->GetNamespace(), "FDateTime") == 0)
			{
				ASSERT_THAT(IsNull(RegisteredFunction));
				RegisteredFunction = Candidate;
			}
		}
		ASSERT_THAT(IsNotNull(RegisteredFunction));
		if (RegisteredFunction == nullptr)
		{
			return;
		}

		const asCScriptFunction* const InternalFunction =
			static_cast<const asCScriptFunction*>(RegisteredFunction);
		ASSERT_THAT(IsNotNull(InternalFunction->sysFuncIntf));
		const asSFuncPtr Expected = asFunctionPtr(ImportedCallable);
		ASSERT_THAT(IsTrue(
			InternalFunction->sysFuncIntf->func == Expected.ptr.f.func));

		const FAngelscriptStaticJITNativeCallDescriptor* const Descriptor =
			FAngelscriptStaticJITNativeCallRegistry::Find(RegisteredFunction);
		ASSERT_THAT(IsNotNull(Descriptor));
		if (Descriptor == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallLinkage::ExportedSymbol,
			Descriptor->Linkage));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallVisibility::External,
			Descriptor->Visibility));
		ASSERT_THAT(AreEqual(
			FString(TEXT("FDateTime::DaysInMonth")), Descriptor->Symbol));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Misc/DateTime.h")), Descriptor->Include));
		ASSERT_THAT(AreEqual(
			FName(TEXT("Core")), Descriptor->OwningModule));
		ASSERT_THAT(AreEqual(
			FString(TEXT("CORE_API")), Descriptor->ApiMacro));
		ASSERT_THAT(AreEqual(
			FString(TEXT("int32(int32, int32)")),
			Descriptor->CppCallableSignature));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallABIDomain::Scalar,
			Descriptor->ABIDomain));

		FAngelscriptStaticJITNativeABIIdentity ExpectedNativeABI;
		FAngelscriptStaticJITScalarABIIdentity ExpectedScalarABI;
		FString Error;
		ASSERT_THAT(IsTrue(TryBuildAngelscriptStaticJITNativeABIIdentity(
			*RegisteredFunction, ExpectedNativeABI, &Error), Error));
		ASSERT_THAT(IsTrue(TryBuildAngelscriptStaticJITScalarABIIdentity(
			*RegisteredFunction, ExpectedScalarABI, &Error), Error));
		ASSERT_THAT(IsTrue(ExpectedNativeABI == Descriptor->NativeABI));
		ASSERT_THAT(IsTrue(ExpectedScalarABI == Descriptor->ScalarABI));

		FAngelscriptStaticJITNativeCallValidationContext Context;
		Context.ExpectedNativeABI = ExpectedNativeABI;
		Context.ExpectedScalarABI = ExpectedScalarABI;
		Context.KnownModules.Add(FName(TEXT("Core")));
		Context.PublicDependencyModules.Add(FName(TEXT("Core")));
		const FAngelscriptStaticJITNativeCallValidation Validation =
			ValidateAngelscriptStaticJITNativeCallDescriptor(
				*Descriptor, Context);
		ASSERT_THAT(IsTrue(Validation.bMetadataValid));
		ASSERT_THAT(IsTrue(Validation.bDirectCallable));
		ASSERT_THAT(IsFalse(Validation.bScalarBridgeCallable));
	}

	TEST_METHOD(PrintBindUsesTheSameExportedRuntimeCallable)
	{
		using FPrintCallable = void(*)(
			const FString&, float, FLinearColor);
		const FPrintCallable ImportedCallable =
			&AngelscriptStaticJITNativeCallables::Print;
		ASSERT_THAT(IsNotNull(ImportedCallable));

		FAngelscriptEngineConfig Config;
		Config.Purpose = EAngelscriptEnginePurpose::StaticJITGeneration;
		Config.bCollectStaticJITCompatibilityBinds = true;
		const FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine =
			FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsTrue(Engine.IsValid()));
		if (!Engine.IsValid())
		{
			return;
		}

		asIScriptFunction* RegisteredPrint = nullptr;
		asIScriptEngine* const ScriptEngine = Engine->GetScriptEngine();
		for (asUINT Index = 0;
			Index < ScriptEngine->GetGlobalFunctionCount(); ++Index)
		{
			asIScriptFunction* const Candidate =
				ScriptEngine->GetGlobalFunctionByIndex(Index);
			if (Candidate != nullptr
				&& FCStringAnsi::Strcmp(Candidate->GetName(), "Print") == 0)
			{
				ASSERT_THAT(IsNull(RegisteredPrint));
				RegisteredPrint = Candidate;
			}
		}
		ASSERT_THAT(IsNotNull(RegisteredPrint));
		if (RegisteredPrint == nullptr)
		{
			return;
		}

		const asCScriptFunction* const InternalPrint =
			static_cast<const asCScriptFunction*>(RegisteredPrint);
		ASSERT_THAT(IsNotNull(InternalPrint->sysFuncIntf));
		const asSFuncPtr Expected = asFunctionPtr(ImportedCallable);
		ASSERT_THAT(IsTrue(
			InternalPrint->sysFuncIntf->func == Expected.ptr.f.func));

		const FAngelscriptStaticJITNativeCallDescriptor* const Descriptor =
			FAngelscriptStaticJITNativeCallRegistry::Find(RegisteredPrint);
		ASSERT_THAT(IsNotNull(Descriptor));
		if (Descriptor == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeCallable,
			Descriptor->Linkage));
		ASSERT_THAT(AreEqual(
			FString(TEXT("AngelscriptStaticJITNativeCallables::Print")),
			Descriptor->Symbol));
		ASSERT_THAT(AreEqual(
			FString(TEXT(
				"StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallables.h")),
			Descriptor->Include));
		ASSERT_THAT(AreEqual(FName(TEXT("AngelscriptRuntime")),
			Descriptor->OwningModule));
		ASSERT_THAT(AreEqual(FString(TEXT("ANGELSCRIPTRUNTIME_API")),
			Descriptor->ApiMacro));
		ASSERT_THAT(AreEqual(FString(TEXT(
			"void(const FString&, float, FLinearColor)")),
			Descriptor->CppCallableSignature));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallABIDomain::BorrowedManagedPOD,
			Descriptor->ABIDomain));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallCompileOutPolicy::IfNoLog,
			Descriptor->CompileOutPolicy));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallDefaultArgumentPolicy::CallerMaterializes,
			Descriptor->DefaultArgumentPolicy));
		ASSERT_THAT(AreEqual(static_cast<uint8>(2),
			Descriptor->DefaultArgumentCount));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallWorldContextPolicy::RuntimeCurrentWorld,
			Descriptor->WorldContextPolicy));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallExceptionPolicy::DoesNotSetScriptException,
			Descriptor->ExceptionPolicy));

		FAngelscriptStaticJITNativeABIIdentity ExpectedNativeABI;
		FString NativeABIError;
		ASSERT_THAT(IsTrue(TryBuildAngelscriptStaticJITNativeABIIdentity(
			*RegisteredPrint, ExpectedNativeABI, &NativeABIError), NativeABIError));
		ASSERT_THAT(IsTrue(ExpectedNativeABI == Descriptor->NativeABI));
		FAngelscriptStaticJITScalarABIIdentity UnsupportedScalarABI;
		ASSERT_THAT(IsFalse(TryBuildAngelscriptStaticJITScalarABIIdentity(
			*RegisteredPrint, UnsupportedScalarABI)));

		FAngelscriptStaticJITNativeCallValidationContext Context;
		Context.ExpectedNativeABI = ExpectedNativeABI;
		Context.KnownModules.Add(FName(TEXT("AngelscriptRuntime")));
		Context.PublicDependencyModules.Add(FName(TEXT("AngelscriptRuntime")));
		const FAngelscriptStaticJITNativeCallValidation Validation =
			ValidateAngelscriptStaticJITNativeCallDescriptor(
				*Descriptor, Context);
		ASSERT_THAT(IsTrue(Validation.bMetadataValid));
		ASSERT_THAT(IsTrue(Validation.bDirectCallable));
		ASSERT_THAT(IsFalse(Validation.bScalarBridgeCallable));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallValidationCode::Valid,
			Validation.Code));
	}

	TEST_METHOD(ScalarABIIdentityComesFromExactRegisteredSystemFunction)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine =
			FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsTrue(Engine.IsValid()));
		FAngelscriptBinds Binds(*Engine);
		const FAngelscriptBoundFunction Global =
			Binds.BindGlobalFunctionForTarget(
				"int32 NativeABIAddOne(int32 Value)",
				&NativeABIAddOne);

		FBindFlags Flags;
		Flags.bPOD = true;
		const FString TypeName = FString::Printf(
			TEXT("FNativeCallABIValue_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
		FAngelscriptBinds Type =
			Binds.ValueClassForTarget<FNativeCallLinkageValue>(
				TypeName, Flags);
		const FAngelscriptBoundFunction Method = Type.Method(
			"int32 Read() const", &FNativeCallLinkageValue::Read);
		ASSERT_THAT(IsTrue(Global.IsValid() && Method.IsValid()));

		FAngelscriptStaticJITScalarABIIdentity GlobalABI;
		FAngelscriptStaticJITScalarABIIdentity MethodABI;
		FString Error;
		ASSERT_THAT(IsTrue(TryBuildAngelscriptStaticJITScalarABIIdentity(
			*Global.GetFunction(), GlobalABI, &Error), Error));
		ASSERT_THAT(AreEqual(
			FString(TEXT("cdecl:i32(i32)")), GlobalABI.Canonical));
		ASSERT_THAT(IsTrue(TryBuildAngelscriptStaticJITScalarABIIdentity(
			*Method.GetFunction(), MethodABI, &Error), Error));
		ASSERT_THAT(AreEqual(
			FString(TEXT("thiscall:i32(ptr)")), MethodABI.Canonical));
		ASSERT_THAT(IsTrue(GlobalABI.IsValid() && MethodABI.IsValid()));
	}

	TEST_METHOD(DescriptorsClassifyDirectBridgeAndAbsentMetadata)
	{
		const FAngelscriptStaticJITScalarABIIdentity ABI =
			ScalarABI(TEXT("cdecl:u32(u32)"));
		const FAngelscriptStaticJITNativeCallValidationContext Context =
			MakeContext(ABI);

		const FAngelscriptStaticJITNativeCallValidation Exported =
			ValidateAngelscriptStaticJITNativeCallDescriptor(
				MakeExportedSymbol(ABI), Context);
		ASSERT_THAT(IsTrue(Exported.bMetadataValid));
		ASSERT_THAT(IsTrue(Exported.bDirectCallable));
		ASSERT_THAT(IsFalse(Exported.bScalarBridgeCallable));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallValidationCode::Valid,
			Exported.Code));

		FAngelscriptStaticJITNativeCallDescriptor Inline =
			MakeExportedSymbol(ABI);
		Inline.Linkage = EAngelscriptStaticJITNativeCallLinkage::HeaderInline;
		Inline.Visibility =
			EAngelscriptStaticJITNativeCallVisibility::HeaderInline;
		Inline.Symbol = TEXT("FMath::Abs");
		Inline.Include = TEXT("Math/UnrealMathUtility.h");
		Inline.ApiMacro.Reset();
		const FAngelscriptStaticJITNativeCallValidation InlineResult =
			ValidateAngelscriptStaticJITNativeCallDescriptor(Inline, Context);
		ASSERT_THAT(IsTrue(InlineResult.bMetadataValid));
		ASSERT_THAT(IsTrue(InlineResult.bDirectCallable));

		FAngelscriptStaticJITNativeCallDescriptor Thunk =
			MakeExportedSymbol(ABI);
		Thunk.Linkage =
			EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeThunk;
		Thunk.Symbol = TEXT("AngelscriptStaticJIT_CountBitsThunk");
		Thunk.Include =
			TEXT("StaticJIT/AngelscriptStaticJITNativeCallables.h");
		Thunk.OwningModule = FName(TEXT("AngelscriptRuntime"));
		Thunk.ApiMacro = TEXT("ANGELSCRIPTRUNTIME_API");
		const FAngelscriptStaticJITNativeCallValidation ThunkResult =
			ValidateAngelscriptStaticJITNativeCallDescriptor(Thunk, Context);
		ASSERT_THAT(IsTrue(ThunkResult.bMetadataValid));
		ASSERT_THAT(IsTrue(ThunkResult.bDirectCallable));

		FAngelscriptStaticJITNativeCallDescriptor Private =
			MakeExportedSymbol(ABI);
		Private.Linkage =
			EAngelscriptStaticJITNativeCallLinkage::ProviderPrivate;
		Private.Visibility =
			EAngelscriptStaticJITNativeCallVisibility::ProviderPrivate;
		Private.Symbol = TEXT("FAngelscriptPrivateBinds::CountBits");
		Private.OwningModule = FName(TEXT("AngelscriptRuntime"));
		Private.Include.Reset();
		Private.ApiMacro.Reset();
		Private.CppCallableSignature = TEXT("uint32(uint32)");
		Private.ScalarABI = ABI;
		Private.RouteFlags =
			EAngelscriptStaticJITNativeCallRouteFlags::CurrentNativeCall;
		Private.LifetimeFlags = SafeScalarLifetime();
		const FAngelscriptStaticJITNativeCallValidation PrivateResult =
			ValidateAngelscriptStaticJITNativeCallDescriptor(Private, Context);
		ASSERT_THAT(IsTrue(PrivateResult.bMetadataValid));
		ASSERT_THAT(IsFalse(PrivateResult.bDirectCallable));
		ASSERT_THAT(IsTrue(PrivateResult.bCurrentNativeCallable));
		ASSERT_THAT(IsFalse(PrivateResult.bScalarBridgeCallable));

		const FAngelscriptStaticJITNativeCallValidation Absent =
			ValidateAngelscriptStaticJITNativeCallDescriptor(
				FAngelscriptStaticJITNativeCallDescriptor{}, Context);
		ASSERT_THAT(IsTrue(Absent.bMetadataValid));
		ASSERT_THAT(IsFalse(Absent.bDirectCallable));
		ASSERT_THAT(IsFalse(Absent.bCurrentNativeCallable));
		ASSERT_THAT(IsFalse(Absent.bScalarBridgeCallable));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallValidationCode::Absent,
			Absent.Code));
	}

	TEST_METHOD(ProviderPrivateVMScalarBridgeDoesNotClaimNativeScalarABI)
	{
		const FAngelscriptStaticJITScalarABIIdentity NativeScalarABI =
			ScalarABI(TEXT("cdecl:u32(u32)"));
		const FAngelscriptStaticJITNativeCallValidationContext Context =
			MakeContext(NativeScalarABI);

		FAngelscriptStaticJITNativeCallDescriptor Descriptor =
			MakeExportedSymbol(NativeScalarABI);
		Descriptor.Linkage =
			EAngelscriptStaticJITNativeCallLinkage::ProviderPrivate;
		Descriptor.Visibility =
			EAngelscriptStaticJITNativeCallVisibility::ProviderPrivate;
		Descriptor.Symbol = TEXT("FPrivateBinds::GenericScalar");
		Descriptor.OwningModule = FName(TEXT("PrivateRuntime"));
		Descriptor.Include.Reset();
		Descriptor.ApiMacro.Reset();
		Descriptor.CppCallableSignature = TEXT("void(asIScriptGeneric*)");
		Descriptor.ABIDomain =
			EAngelscriptStaticJITNativeCallABIDomain::VMScalarBridge;
		Descriptor.ExceptionPolicy =
			EAngelscriptStaticJITNativeCallExceptionPolicy::MaySetScriptException;
		Descriptor.ScalarABI = {};
		Descriptor.RouteFlags =
			EAngelscriptStaticJITNativeCallRouteFlags::ScalarBridge;
		Descriptor.LifetimeFlags = SafeScalarLifetime();

		const FAngelscriptStaticJITNativeCallValidation Result =
			ValidateAngelscriptStaticJITNativeCallDescriptor(
				Descriptor, Context);
		ASSERT_THAT(IsTrue(Result.bMetadataValid, Result.Detail));
		ASSERT_THAT(IsFalse(Result.bDirectCallable));
		ASSERT_THAT(IsFalse(Result.bCurrentNativeCallable));
		ASSERT_THAT(IsTrue(Result.bScalarBridgeCallable));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallValidationCode::Valid,
			Result.Code));
	}

	TEST_METHOD(ProviderPrivateCurrentNativeRejectsExceptionCapableTarget)
	{
		const FAngelscriptStaticJITScalarABIIdentity ABI =
			ScalarABI(TEXT("cdecl:u32(u32)"));
		const FAngelscriptStaticJITNativeCallValidationContext Context =
			MakeContext(ABI);

		FAngelscriptStaticJITNativeCallDescriptor Descriptor =
			MakeExportedSymbol(ABI);
		Descriptor.Linkage =
			EAngelscriptStaticJITNativeCallLinkage::ProviderPrivate;
		Descriptor.Visibility =
			EAngelscriptStaticJITNativeCallVisibility::ProviderPrivate;
		Descriptor.Symbol = TEXT("FPrivateBinds::MayFail");
		Descriptor.OwningModule = FName(TEXT("PrivateRuntime"));
		Descriptor.Include.Reset();
		Descriptor.ApiMacro.Reset();
		Descriptor.RouteFlags =
			EAngelscriptStaticJITNativeCallRouteFlags::CurrentNativeCall;
		Descriptor.ExceptionPolicy =
			EAngelscriptStaticJITNativeCallExceptionPolicy::MaySetScriptException;

		const FAngelscriptStaticJITNativeCallValidation Result =
			ValidateAngelscriptStaticJITNativeCallDescriptor(
				Descriptor, Context);
		ASSERT_THAT(IsFalse(Result.bMetadataValid));
		ASSERT_THAT(IsFalse(Result.bDirectCallable));
		ASSERT_THAT(IsFalse(Result.bCurrentNativeCallable));
		ASSERT_THAT(IsFalse(Result.bScalarBridgeCallable));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallValidationCode::UnsafeRouting,
			Result.Code));
	}

	TEST_METHOD(InvalidDescriptorMatrixFailsClosed)
	{
		const FAngelscriptStaticJITScalarABIIdentity ABI =
			ScalarABI(TEXT("cdecl:u32(u32)"));
		const FAngelscriptStaticJITNativeCallValidationContext Context =
			MakeContext(ABI);

		struct FCase
		{
			const TCHAR* Name;
			FAngelscriptStaticJITNativeCallDescriptor Descriptor;
			EAngelscriptStaticJITNativeCallValidationCode Expected;
		};
		TArray<FCase> Cases;

		FAngelscriptStaticJITNativeCallDescriptor MissingSymbol =
			MakeExportedSymbol(ABI);
		MissingSymbol.Symbol.Reset();
		Cases.Add({TEXT("missing-symbol"), MissingSymbol,
			EAngelscriptStaticJITNativeCallValidationCode::MissingSymbol});

		FAngelscriptStaticJITNativeCallDescriptor Internal =
			MakeExportedSymbol(ABI);
		Internal.Visibility =
			EAngelscriptStaticJITNativeCallVisibility::Internal;
		Cases.Add({TEXT("internal-linkage"), Internal,
			EAngelscriptStaticJITNativeCallValidationCode::InternalLinkage});

		FAngelscriptStaticJITNativeCallDescriptor MissingInclude =
			MakeExportedSymbol(ABI);
		MissingInclude.Include.Reset();
		Cases.Add({TEXT("missing-include"), MissingInclude,
			EAngelscriptStaticJITNativeCallValidationCode::MissingInclude});

		FAngelscriptStaticJITNativeCallDescriptor MissingApi =
			MakeExportedSymbol(ABI);
		MissingApi.ApiMacro.Reset();
		Cases.Add({TEXT("missing-api"), MissingApi,
			EAngelscriptStaticJITNativeCallValidationCode::MissingApiMacro});

		FAngelscriptStaticJITNativeCallDescriptor PrivateDependency =
			MakeExportedSymbol(ABI);
		PrivateDependency.OwningModule = FName(TEXT("PrivateRuntime"));
		Cases.Add({TEXT("private-dependency"), PrivateDependency,
			EAngelscriptStaticJITNativeCallValidationCode::PrivateDependency});

		FAngelscriptStaticJITNativeCallDescriptor UnknownModule =
			MakeExportedSymbol(ABI);
		UnknownModule.OwningModule = FName(TEXT("UnknownModule"));
		Cases.Add({TEXT("unknown-module"), UnknownModule,
			EAngelscriptStaticJITNativeCallValidationCode::UnknownOwningModule});

		FAngelscriptStaticJITNativeCallDescriptor MismatchedABI =
			MakeExportedSymbol(ScalarABI(TEXT("cdecl:i64(i32)")));
		Cases.Add({TEXT("abi-mismatch"), MismatchedABI,
			EAngelscriptStaticJITNativeCallValidationCode::ScalarABIMismatch});

		FAngelscriptStaticJITNativeCallDescriptor UnsafeRoute =
			MakeExportedSymbol(ABI);
		UnsafeRoute.RouteFlags |=
			EAngelscriptStaticJITNativeCallRouteFlags::RequiresProcessEvent;
		Cases.Add({TEXT("unsafe-route"), UnsafeRoute,
			EAngelscriptStaticJITNativeCallValidationCode::UnsafeRouting});

		FAngelscriptStaticJITNativeCallDescriptor UnsafeLifetime =
			MakeExportedSymbol(ABI);
		UnsafeLifetime.LifetimeFlags =
			EAngelscriptStaticJITNativeCallLifetimeFlags::ByValueScalarOnly;
		Cases.Add({TEXT("unsafe-lifetime"), UnsafeLifetime,
			EAngelscriptStaticJITNativeCallValidationCode::UnsafeLifetime});

		for (const FCase& Case : Cases)
		{
			const FAngelscriptStaticJITNativeCallValidation Result =
				ValidateAngelscriptStaticJITNativeCallDescriptor(
					Case.Descriptor, Context);
			ASSERT_THAT(IsFalse(Result.bMetadataValid, Case.Name));
			ASSERT_THAT(IsFalse(Result.bDirectCallable, Case.Name));
			ASSERT_THAT(IsFalse(Result.bScalarBridgeCallable, Case.Name));
			ASSERT_THAT(AreEqual(Case.Expected, Result.Code, Case.Name));
			ASSERT_THAT(IsFalse(Result.Detail.IsEmpty(), Case.Name));
		}
	}

	TEST_METHOD(ExplicitMetadataAttachesToTheExactEngineFunction)
	{
		const int32 DescriptorCountBefore =
			FAngelscriptStaticJITNativeCallRegistry::NumDescriptors();
		{
			const FAngelscriptEngineConfig Config;
			const FAngelscriptEngineDependencies Dependencies =
				FAngelscriptEngineDependencies::CreateDefault();
			TUniquePtr<FAngelscriptEngine> EngineA =
				FAngelscriptTestEngine::Create(Config, Dependencies);
			TUniquePtr<FAngelscriptEngine> EngineB =
				FAngelscriptTestEngine::Create(Config, Dependencies);
			ASSERT_THAT(IsTrue(EngineA.IsValid() && EngineB.IsValid()));

			EngineA->bCollectStaticJITCompatibilityBinds = true;
			EngineB->bCollectStaticJITCompatibilityBinds = true;
			FAngelscriptBinds BindsA(*EngineA);
			FAngelscriptBinds BindsB(*EngineB);
			FBindFlags Flags;
			Flags.bPOD = true;
			const FString TypeName = FString::Printf(
				TEXT("FNativeCallLinkageValue_%s"),
				*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
			FAngelscriptBinds TypeA =
				BindsA.ValueClassForTarget<FNativeCallLinkageValue>(
					TypeName, Flags);
			FAngelscriptBinds TypeB =
				BindsB.ValueClassForTarget<FNativeCallLinkageValue>(
					TypeName, Flags);
			FAngelscriptBoundFunction FunctionA = TypeA.Method(
				"int32 Read() const", &FNativeCallLinkageValue::Read);
			FAngelscriptBoundFunction FunctionB = TypeB.Method(
				"int32 Read() const", &FNativeCallLinkageValue::Read);
			ASSERT_THAT(IsTrue(FunctionA.IsValid() && FunctionB.IsValid()));

			const FAngelscriptStaticJITScalarABIIdentity ABI =
				ScalarABI(TEXT("thiscall:i32(ptr)"));
			FAngelscriptStaticJITNativeCallDescriptor DescriptorA =
				MakeExportedSymbol(ABI);
			ASSERT_THAT(IsTrue(TryBuildAngelscriptStaticJITNativeABIIdentity(
				*FunctionA.GetFunction(), DescriptorA.NativeABI)));
			FunctionA
				.NativeMethod("FNativeCallLinkageValue::Read", true)
				.ExternalNativeCall(DescriptorA);
			FunctionB.NativeMethod(
				"FNativeCallLinkageValue::Read", true);

			const FAngelscriptStaticJITNativeCallDescriptor* FoundA =
				FAngelscriptStaticJITNativeCallRegistry::Find(
					FunctionA.GetFunction());
			ASSERT_THAT(IsNotNull(FoundA));
			ASSERT_THAT(AreEqual(
				DescriptorA.DumpNormalized(), FoundA->DumpNormalized()));
			ASSERT_THAT(IsNull(
				FAngelscriptStaticJITNativeCallRegistry::Find(
					FunctionB.GetFunction()),
				TEXT("Legacy NativeMethod metadata must remain non-external")));
			ASSERT_THAT(AreEqual(
				DescriptorCountBefore + 1,
				FAngelscriptStaticJITNativeCallRegistry::NumDescriptors()));

			FAngelscriptStaticJITNativeCallDescriptor DescriptorB =
				DescriptorA;
			DescriptorB.Linkage =
				EAngelscriptStaticJITNativeCallLinkage::HeaderInline;
			DescriptorB.Visibility =
				EAngelscriptStaticJITNativeCallVisibility::HeaderInline;
			DescriptorB.ApiMacro.Reset();
			FunctionB.ExternalNativeCall(DescriptorB);
			ASSERT_THAT(AreEqual(
				DescriptorCountBefore + 2,
				FAngelscriptStaticJITNativeCallRegistry::NumDescriptors()));

			EngineA.Reset();
			ASSERT_THAT(AreEqual(
				DescriptorCountBefore + 1,
				FAngelscriptStaticJITNativeCallRegistry::NumDescriptors()));
			const FAngelscriptStaticJITNativeCallDescriptor* FoundB =
				FAngelscriptStaticJITNativeCallRegistry::Find(
					FunctionB.GetFunction());
			ASSERT_THAT(IsNotNull(FoundB));
			ASSERT_THAT(AreEqual(
				DescriptorB.DumpNormalized(), FoundB->DumpNormalized()));
		}
		ASSERT_THAT(AreEqual(
			DescriptorCountBefore,
			FAngelscriptStaticJITNativeCallRegistry::NumDescriptors()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
