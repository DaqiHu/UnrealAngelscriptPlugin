#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "source/as_buildartifact.h"
#include "source/as_bytecode.h"
#include "source/as_objecttype.h"
#include "source/as_property.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeTypedSemanticIRCaptureParityTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.CaptureParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 InvokeHiddenParityCall(const int32 Visible, const int32 Hidden)
	{
		return Visible * 10 + Hidden;
	}

	static void InvokeCompileOutParityCall(const int32)
	{
		++CompileOutInvocationCount;
	}

	static bool RegisterParityCalls(asIScriptEngine& ScriptEngine)
	{
		const ASAutoCaller::FunctionCaller HiddenCaller =
			ASAutoCaller::MakeFunctionCaller(InvokeHiddenParityCall);
		const ASAutoCaller::FunctionCaller CompileOutCaller =
			ASAutoCaller::MakeFunctionCaller(InvokeCompileOutParityCall);
		const int HiddenFunctionId = ScriptEngine.RegisterGlobalFunction(
			"int InvokeHiddenParityCall(int Visible, int Hidden)",
			asFUNCTION(InvokeHiddenParityCall),
			asCALL_CDECL,
			*(asFunctionCaller*)&HiddenCaller);
		const int CompileOutFunctionId = ScriptEngine.RegisterGlobalFunction(
			"void InvokeCompileOutParityCall(int Value)",
			asFUNCTION(InvokeCompileOutParityCall),
			asCALL_CDECL,
			*(asFunctionCaller*)&CompileOutCaller);
		if (HiddenFunctionId < 0 || CompileOutFunctionId < 0)
		{
			return false;
		}

		asCScriptFunction* const HiddenFunction = static_cast<asCScriptFunction*>(
			ScriptEngine.GetFunctionById(HiddenFunctionId));
		asCScriptFunction* const CompileOutFunction = static_cast<asCScriptFunction*>(
			ScriptEngine.GetFunctionById(CompileOutFunctionId));
		if (HiddenFunction == nullptr || CompileOutFunction == nullptr)
		{
			return false;
		}
		HiddenFunction->hiddenArgumentIndex = 1;
		HiddenFunction->hiddenArgumentDefault = "4";
		CompileOutFunction->compileOutType =
			asECompileOutType::CompileOutEntirely;
		return true;
	}

	static bool HasEngineLocalPointerOperand(const asBYTE Opcode)
	{
		switch (Opcode)
		{
		case asBC_ALLOC:
		case asBC_REFCPY:
		case asBC_RefCpyV:
		case asBC_OBJTYPE:
		case asBC_FinConstruct:
		case asBC_DestructScript:
		case asBC_CopyScript:
		case asBC_JitEntry:
		case asBC_CALLSYS:
		case asBC_Thiscall1:
		case asBC_FuncPtr:
		case asBC_PGA:
		case asBC_PshGPtr:
		case asBC_LDG:
		case asBC_PshG4:
		case asBC_LdGRdR4:
		case asBC_CpyGtoV4:
		case asBC_CpyVtoG4:
		case asBC_SetG4:
		case asBC_FREE:
			return true;
		default:
			return false;
		}
	}

	static TArray<asDWORD> CopyNormalizedBytecode(asIScriptFunction* Function)
	{
		TArray<asDWORD> Result =
			AngelscriptNativeTestSupport::CopyNativeBytecode(Function);
		asUINT Offset = 0;
		while (Offset < static_cast<asUINT>(Result.Num()))
		{
			const asBYTE Opcode = *reinterpret_cast<const asBYTE*>(&Result[Offset]);
			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0
				|| Offset + static_cast<asUINT>(InstructionSize)
					> static_cast<asUINT>(Result.Num()))
			{
				Result.Reset();
				return Result;
			}
			if (HasEngineLocalPointerOperand(Opcode))
			{
				if (InstructionSize < 1 + AS_PTR_SIZE)
				{
					Result.Reset();
					return Result;
				}
				for (int32 PointerWord = 1; PointerWord <= AS_PTR_SIZE; ++PointerWord)
				{
					Result[Offset + static_cast<asUINT>(PointerWord)] = 0;
				}
			}
			Offset += static_cast<asUINT>(InstructionSize);
		}
		return Result;
	}

	static FString NormalizeDependency(const asSBuildArtifactDependency& Dependency)
	{
		const FString TypeName = Dependency.type != nullptr
			? UTF8_TO_TCHAR(Dependency.type->GetName()) : TEXT("<none>");
		const FString FunctionDeclaration = Dependency.function != nullptr
			? UTF8_TO_TCHAR(Dependency.function->GetDeclaration(
				true, true, true, false)) : TEXT("<none>");
		const FString GlobalName = Dependency.globalProperty != nullptr
			? UTF8_TO_TCHAR(Dependency.globalProperty->name.AddressOf())
			: TEXT("<none>");
		const FString PropertyOwnerName = Dependency.propertyOwnerType != nullptr
			? UTF8_TO_TCHAR(Dependency.propertyOwnerType->GetName())
			: TEXT("<none>");
		const FString PropertyName = Dependency.objectProperty != nullptr
			? UTF8_TO_TCHAR(Dependency.objectProperty->GetName())
			: TEXT("<none>");
		return FString::Printf(
			TEXT("v=%u,k=%u,r=%u,t=%s,f=%s,g=%s,o=%s,p=%s"),
			Dependency.descriptorVersion,
			static_cast<uint32>(Dependency.kind),
			static_cast<uint32>(Dependency.referenceKind),
			*TypeName,
			*FunctionDeclaration,
			*GlobalName,
			*PropertyOwnerName,
			*PropertyName);
	}

	static FString NormalizeFunctionMetadata(asCScriptFunction* Function)
	{
		if (Function == nullptr || Function->scriptData == nullptr)
		{
			return TEXT("<invalid>");
		}
		FString Result = FString::Printf(
			TEXT("decl=%s|traits=%u|signature=%d|hidden=%d:%s|output=%d|compileout=%u"),
			UTF8_TO_TCHAR(Function->GetDeclaration(true, true, true, false)),
			Function->traits.traits,
			Function->signatureId,
			static_cast<int32>(Function->hiddenArgumentIndex),
			UTF8_TO_TCHAR(Function->hiddenArgumentDefault.AddressOf()),
			static_cast<int32>(Function->determinesOutputTypeArgumentIndex),
			static_cast<uint32>(Function->compileOutType));
		for (asUINT ParameterIndex = 0;
			ParameterIndex < Function->parameterTypes.GetLength();
			++ParameterIndex)
		{
			Result += FString::Printf(
				TEXT("|arg[%u]=%s:%u:%s:%s"),
				ParameterIndex,
				UTF8_TO_TCHAR(Function->parameterTypes[ParameterIndex]
					.Format(Function->nameSpace, true).AddressOf()),
				static_cast<uint32>(Function->inOutFlags[ParameterIndex]),
				UTF8_TO_TCHAR(Function->parameterNames[ParameterIndex].AddressOf()),
				Function->defaultArgs[ParameterIndex] != nullptr
					? UTF8_TO_TCHAR(Function->defaultArgs[ParameterIndex]->AddressOf())
					: TEXT("<none>"));
		}
		for (asUINT LineIndex = 0;
			LineIndex < Function->scriptData->lineNumbers.GetLength();
			++LineIndex)
		{
			Result += FString::Printf(
				TEXT("|line[%u]=%d"),
				LineIndex,
				Function->scriptData->lineNumbers[LineIndex]);
		}
		for (asUINT SectionIndex = 0;
			SectionIndex < Function->scriptData->sectionIdxs.GetLength();
			++SectionIndex)
		{
			Result += FString::Printf(
				TEXT("|section[%u]=%d"),
				SectionIndex,
				Function->scriptData->sectionIdxs[SectionIndex]);
		}
		for (asUINT DependencyIndex = 0;
			DependencyIndex < Function->scriptData->artifactDependencies.GetLength();
			++DependencyIndex)
		{
			Result += FString::Printf(
				TEXT("|dependency[%u]=%s"),
				DependencyIndex,
				*NormalizeDependency(
					Function->scriptData->artifactDependencies[DependencyIndex]));
		}
		return Result;
	}

	static bool ExecuteEntry(
		asIScriptEngine* ScriptEngine,
		asIScriptFunction* Function,
		int32& OutValue)
	{
		if (ScriptEngine == nullptr || Function == nullptr)
		{
			return false;
		}
		asIScriptContext* const Context = ScriptEngine->CreateContext();
		if (Context == nullptr)
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		if (Context->Prepare(Function) < 0
			|| Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		OutValue = static_cast<int32>(Context->GetReturnDWord());
		return true;
	}

	inline static int32 CompileOutInvocationCount = 0;

public:
	TEST_METHOD(ReceiverCallRewriteMetadataAndVmRemainCaptureInvariant)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-CAPTURE-PARITY",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FParityCounter
			{
				int Value = 0;
			}

			int ReadExternalParity(FParityCounter& Self) external_implicit_this
			{
				return this.Value;
			}

			mixin int AddMixinParity(FParityCounter& Self, int Delta = 3)
			{
				return Self.Value + Delta;
			}

			int AddDefaultParity(int Value, int Delta = 5)
			{
				return Value + Delta;
			}

			int CaptureParityProbe()
			{
				FParityCounter Counter;
				return 1;
			}

			int CaptureParityEntry()
			{
				FParityCounter Counter;
				Counter.Value = 7;
				InvokeCompileOutParityCall(999);
				return ReadExternalParity(Counter)
					+ Counter.AddMixinParity()
					+ AddDefaultParity(4)
					+ InvokeHiddenParityCall(2);
			}
			)AS");

		FNativeTestEngine CaptureOffEngine;
		FNativeTestEngine CaptureOnEngine;
		CaptureOffEngine.Create(*TestRunner);
		CaptureOnEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			CaptureOnEngine.Destroy();
			CaptureOffEngine.Destroy();
		};
		ASSERT_THAT(IsTrue(RegisterParityCalls(*CaptureOffEngine.Get())));
		ASSERT_THAT(IsTrue(RegisterParityCalls(*CaptureOnEngine.Get())));
		asCScriptEngine* const CaptureOnScriptEngine =
			static_cast<asCScriptEngine*>(CaptureOnEngine.Get());
		ASSERT_THAT(IsTrue(
			CaptureOnScriptEngine->SetTypedSemanticIRCapture(true)));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRCaptureParityOff",
			ScriptSource);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRCaptureParityOn",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid(),
			TEXT("The capture-off parity fixture must compile")));
		ASSERT_THAT(IsTrue(CaptureOnModule.IsValid(),
			TEXT("The capture-on parity fixture must compile")));
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		asIScriptModule* const CaptureOffScriptModule = CaptureOffModule.Get();
		asIScriptModule* const CaptureOnScriptModule = CaptureOnModule.Get();
		ASSERT_THAT(AreEqual(
			CaptureOffScriptModule->GetFunctionCount(),
			CaptureOnScriptModule->GetFunctionCount(),
			TEXT("Capture must not change the published global function set")));
		for (asUINT FunctionIndex = 0;
			FunctionIndex < CaptureOffScriptModule->GetFunctionCount();
			++FunctionIndex)
		{
			asCScriptFunction* const CaptureOffFunction =
				static_cast<asCScriptFunction*>(
					CaptureOffScriptModule->GetFunctionByIndex(FunctionIndex));
			asCScriptFunction* const CaptureOnFunction =
				static_cast<asCScriptFunction*>(
					CaptureOnScriptModule->GetFunctionByIndex(FunctionIndex));
			ASSERT_THAT(IsNotNull(CaptureOffFunction));
			ASSERT_THAT(IsNotNull(CaptureOnFunction));
			if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr)
			{
				continue;
			}
			ASSERT_THAT(AreEqual(
				NormalizeFunctionMetadata(CaptureOffFunction),
				NormalizeFunctionMetadata(CaptureOnFunction),
				TEXT("Capture must preserve declaration, traits, signature, line, section and dependency metadata")));
			ASSERT_THAT(AreEqual(
				CopyNormalizedBytecode(CaptureOffFunction),
				CopyNormalizedBytecode(CaptureOnFunction),
				TEXT("Capture must preserve bytecode after normalizing only Engine-local pointer operands")));
		}

		asCScriptFunction* const CaptureOffProbe = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(
				CaptureOffModule, "int CaptureParityProbe()"));
		asCScriptFunction* const CaptureOnProbe = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(
				CaptureOnModule, "int CaptureParityProbe()"));
		ASSERT_THAT(IsNotNull(CaptureOffProbe));
		ASSERT_THAT(IsNotNull(CaptureOnProbe));
		if (CaptureOffProbe == nullptr || CaptureOnProbe == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNull(CaptureOffProbe->GetTypedSemanticFunction()));
		ASSERT_THAT(IsNotNull(CaptureOnProbe->GetTypedSemanticFunction(),
			TEXT("Capture-on must publish a representable HIR probe, including implicit default construction")));

		asCScriptFunction* const CaptureOffEntry = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(
				CaptureOffModule, "int CaptureParityEntry()"));
		asCScriptFunction* const CaptureOnEntry = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(
				CaptureOnModule, "int CaptureParityEntry()"));
		ASSERT_THAT(IsNotNull(CaptureOffEntry));
		ASSERT_THAT(IsNotNull(CaptureOnEntry));
		if (CaptureOffEntry == nullptr || CaptureOnEntry == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNull(CaptureOffEntry->GetTypedSemanticFunction()));
		TestRunner->AddInfo(FString::Printf(
			TEXT("CaptureParity entry HIR: Present=%d Diagnostic=%s"),
			CaptureOnEntry->GetTypedSemanticFunction() != nullptr ? 1 : 0,
			UTF8_TO_TCHAR(
				CaptureOnEntry->GetTypedSemanticCaptureDiagnostic().AddressOf())));
		ASSERT_THAT(IsTrue(
			CaptureOnEntry->scriptData->lineNumbers.GetLength() > 0,
			TEXT("The parity fixture must exercise retained line metadata")));
		ASSERT_THAT(IsTrue(
			CaptureOnEntry->scriptData->artifactDependencies.GetLength() > 0,
			TEXT("The parity fixture must exercise authoritative dependencies")));

		int32 ExternalTraitCount = 0;
		int32 MixinTraitCount = 0;
		for (asUINT FunctionIndex = 0;
			FunctionIndex < CaptureOnScriptModule->GetFunctionCount();
			++FunctionIndex)
		{
			asCScriptFunction* const Function = static_cast<asCScriptFunction*>(
				CaptureOnScriptModule->GetFunctionByIndex(FunctionIndex));
			if (Function != nullptr
				&& Function->traits.GetTrait(asTRAIT_EXTERNAL_IMPLICIT_THIS))
			{
				++ExternalTraitCount;
			}
			if (Function != nullptr && Function->traits.GetTrait(asTRAIT_MIXIN))
			{
				++MixinTraitCount;
			}
		}
		ASSERT_THAT(AreEqual(1, ExternalTraitCount,
			TEXT("The parity fixture must contain one external-implicit-this function")));
		ASSERT_THAT(AreEqual(1, MixinTraitCount,
			TEXT("The parity fixture must contain one mixin function")));

		CompileOutInvocationCount = 0;
		int32 CaptureOffValue = 0;
		int32 CaptureOnValue = 0;
		ASSERT_THAT(IsTrue(ExecuteEntry(
			CaptureOffEngine.Get(), CaptureOffEntry, CaptureOffValue)));
		ASSERT_THAT(IsTrue(ExecuteEntry(
			CaptureOnEngine.Get(), CaptureOnEntry, CaptureOnValue)));
		ASSERT_THAT(AreEqual(50, CaptureOffValue,
			TEXT("The capture-off fixture must execute receiver, default and hidden argument behavior")));
		ASSERT_THAT(AreEqual(CaptureOffValue, CaptureOnValue,
			TEXT("Capture-on VM execution must equal capture-off execution")));
		ASSERT_THAT(AreEqual(0, CompileOutInvocationCount,
			TEXT("Compile-out calls must remain absent from both VM executions")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
