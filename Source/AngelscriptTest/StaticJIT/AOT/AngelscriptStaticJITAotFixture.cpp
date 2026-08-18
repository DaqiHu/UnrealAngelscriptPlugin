#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"

#include "AngelscriptEngine.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "ClassGenerator/ASFunction.h"
#include "Core/FunctionCallers.h"
#include "Core/angelscript.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallDescriptor.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallables.h"
#include "StaticJIT/StaticJITHeader.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace AngelscriptStaticJITAotFixture
{
	namespace
	{
		struct FObjectLastNativeProbe
		{
			int32 Value = 0;
		};

		struct FObjectLastNativeObservation
		{
			int32 CallCount = 0;
			int32 Left = INDEX_NONE;
			int32 Right = INDEX_NONE;
			int32 ObjectValue = INDEX_NONE;
		};

		FObjectLastNativeObservation ObjectLastNativeObservation;
		int32 TypedASTPrivateBridgeCallCount = 0;
		int32 TypedASTPrivateBridgeReplacementCallCount = 0;
		int32 TypedASTPrivateBridgeExceptionCallCount = 0;
		int32 TypedASTRuntimeThunkRegisteredCallCount = 0;
		int32 TypedASTPublicUASFunctionInvokeCallCount = 0;
		TArray<FTypedASTFrameObservation> TypedASTFrameObservations;

		void ConstructObjectLastNativeProbe(
			const int32 Left,
			const int32 Right,
			FObjectLastNativeProbe* Address)
		{
			new (Address) FObjectLastNativeProbe();
			Address->Value = Left * 1000 + Right;
			ObjectLastNativeObservation.CallCount++;
			ObjectLastNativeObservation.Left = Left;
			ObjectLastNativeObservation.Right = Right;
			ObjectLastNativeObservation.ObjectValue = Address->Value;
		}

		int32 TypedASTProviderPrivateAdd(
			const int32 Left,
			const int32 Right)
		{
			++TypedASTPrivateBridgeCallCount;
			return Left + Right + 100;
		}

		int32 TypedASTProviderPrivateAddReplacement(
			const int32 Left,
			const int32 Right)
		{
			++TypedASTPrivateBridgeReplacementCallCount;
			return Left + Right + 200;
		}

		int32 TypedASTObserveFrame(const int32 Marker)
		{
			FTypedASTFrameObservation& Observation =
				TypedASTFrameObservations.AddDefaulted_GetRef();
			Observation.Marker = Marker;
			FAngelscriptEngine::GetAngelscriptExecutionFileAndLine(
				Observation.PublicFilename,
				Observation.PublicLine);

			asCThreadLocalData* const ThreadLocalData =
				asCThreadManager::GetLocalData();
			FScriptExecution* const Execution = ThreadLocalData != nullptr
				? ThreadLocalData->activeExecution : nullptr;
#if AS_JIT_DEBUG_CALLSTACKS
			FScopeJITDebugCallstack* Frame = Execution != nullptr
				? static_cast<FScopeJITDebugCallstack*>(
					Execution->debugCallStack)
				: nullptr;
			if (Frame != nullptr)
			{
				Observation.TopFilename = ANSI_TO_TCHAR(Frame->Filename);
				Observation.TopFunction = ANSI_TO_TCHAR(Frame->FunctionName);
				Observation.TopLine = Frame->LineNumber;
				if (Frame->PrevFrame != nullptr)
				{
					Observation.ParentFilename =
						ANSI_TO_TCHAR(Frame->PrevFrame->Filename);
					Observation.ParentFunction =
						ANSI_TO_TCHAR(Frame->PrevFrame->FunctionName);
					Observation.ParentLine = Frame->PrevFrame->LineNumber;
				}
				while (Frame != nullptr)
				{
					++Observation.FrameDepth;
					Frame = Frame->PrevFrame;
				}
			}
#else
			static_cast<void>(Execution);
#endif
			return Marker * 10;
		}

		int32 TypedASTRuntimeThunkRegistered(const FString& Text)
		{
			++TypedASTRuntimeThunkRegisteredCallCount;
			return Text.Len() * 3 + 5;
		}

		void TypedASTProviderPrivateFail(asIScriptGeneric* Generic)
		{
			++TypedASTPrivateBridgeExceptionCallCount;
			if (asIScriptContext* const Context = asGetActiveContext())
			{
				Context->SetException(
					"TypedASTJIT provider-private bridge failure");
			}
			Generic->SetReturnDWord(999u);
		}

		void TypedASTInvokePublicUASFunctionException(asIScriptGeneric* Generic)
		{
			++TypedASTPublicUASFunctionInvokeCallCount;
			asIScriptContext* const ActiveContext = asGetActiveContext();
			FAngelscriptEngine* const Engine =
				FAngelscriptEngine::TryGetByScriptEngine(Generic->GetEngine());
			UClass* const StaticsClass = Engine != nullptr
				? FindGeneratedClass(
					Engine, GetTypedPrivateBridgeStaticsClassName())
				: nullptr;
			UASFunction* const Function = StaticsClass != nullptr
				? Cast<UASFunction>(FindGeneratedFunction(
					StaticsClass, TEXT("TypedASTPrivateBridgeException")))
				: nullptr;
			FIntProperty* const ValueProperty = Function != nullptr
				? FindFProperty<FIntProperty>(Function, TEXT("Value"))
				: nullptr;
			if (Engine == nullptr || StaticsClass == nullptr || Function == nullptr
				|| ValueProperty == nullptr)
			{
				if (ActiveContext != nullptr)
				{
					ActiveContext->SetException(
						"TypedASTJIT public UASFunction fixture route unavailable");
				}
				return;
			}

			FStructOnScope Parms(Function);
			ValueProperty->SetPropertyValue_InContainer(
				Parms.GetStructMemory(),
				static_cast<int32>(Generic->GetArgDWord(0)));
			Function->RuntimeCallEvent(
				StaticsClass->GetDefaultObject(), Parms.GetStructMemory());
		}

	}

	const FName& GetModuleName()
	{
		static const FName ModuleName(TEXT("ASStaticJITAotFixture"));
		return ModuleName;
	}

	const FString& GetSourceFilename()
	{
		static const FString SourceFilename(TEXT("ASStaticJITAotFixture.as"));
		return SourceFilename;
	}

	const FName& GetImportProviderModuleName()
	{
		static const FName ModuleName(TEXT("ASStaticJITAotImportProvider"));
		return ModuleName;
	}

	const FString& GetImportProviderSourceFilename()
	{
		static const FString SourceFilename(TEXT("ASStaticJITAotImportProvider.as"));
		return SourceFilename;
	}

	const FString& GetImportProviderScriptSource()
	{
		static const FString ScriptSource = ASTEST_AS(R"AS(
			int ImportedValueForAOT(int Value)
			{
				return Value + 77;
			}

			int IndependentModuleEntryForAOT()
			{
				return 87;
			}
			)AS");
		return ScriptSource;
	}

	const FName& GetTypedPrintModuleName()
	{
		static const FName ModuleName(TEXT("ASStaticJITTypedPrintFixture"));
		return ModuleName;
	}

	const FString& GetTypedPrintSourceFilename()
	{
		static const FString SourceFilename(
			TEXT("ASStaticJITTypedPrintFixture.as"));
		return SourceFilename;
	}

	const FString& GetTypedPrintScriptSource()
	{
		static const FString ScriptSource = ASTEST_AS(R"AS(
			UFUNCTION()
			int TypedASTPrintShowcase(
				const FString&in Text,
				float Duration,
				const FLinearColor&in Color)
			{
				Print(Text, Duration, Color);
				return 314;
			}
			)AS");
		return ScriptSource;
	}

	const FName& GetTypedPrivateBridgeModuleName()
	{
		static const FName ModuleName(
			TEXT("ASStaticJITTypedPrivateBridgeFixture"));
		return ModuleName;
	}

	const FName& GetTypedPrivateBridgeStaticsClassName()
	{
		static const FName ClassName(
			TEXT("UModule_ASStaticJITTypedPrivateBridgeFixtureStatics"));
		return ClassName;
	}

	const FString& GetTypedPrivateBridgeSourceFilename()
	{
		static const FString SourceFilename(
			TEXT("ASStaticJITTypedPrivateBridgeFixture.as"));
		return SourceFilename;
	}

	const FString& GetTypedPrivateBridgeScriptSource()
	{
		static const FString ScriptSource = ASTEST_AS(R"AS(
			UFUNCTION()
			int TypedASTPrivateBridgeShowcase(int A, int B)
			{
				return TypedASTProviderPrivateAdd(A, B);
			}

			UFUNCTION()
			int TypedASTRuntimeThunkShowcase(const FString&in Text)
			{
				return TypedASTRuntimeThunkProbe(Text);
			}

			UFUNCTION(meta = (NotBlueprintThreadSafe))
			int TypedASTPrivateBridgeException(int Value)
			{
				int Result = TypedASTProviderPrivateFail(Value);
				TypedASTProviderPrivateAdd(1, 2);
				return Result;
			}

			void TypedASTPublicUASFunctionExceptionEntry(int Value)
			{
				TypedASTInvokePublicUASFunctionException(Value);
				TypedASTProviderPrivateAdd(3, 4);
			}

			UFUNCTION()
			float32 TypedASTOrdinaryBridgeSqrt(float32 Value)
			{
				return Math::Sqrt(Value);
			}
			)AS");
		return ScriptSource;
	}

	const FName& GetTypedRecursionModuleName()
	{
		static const FName ModuleName(
			TEXT("ASStaticJITTypedRecursionFixture"));
		return ModuleName;
	}

	const FString& GetTypedRecursionSourceFilename()
	{
		static const FString SourceFilename(
			TEXT("ASStaticJITTypedRecursionFixture.as"));
		return SourceFilename;
	}

	const FString& GetTypedRecursionScriptSource()
	{
		static const FString ScriptSource = ASTEST_AS(R"AS(
			UFUNCTION()
			int TypedASTSelfRecursion(int Value)
			{
				if (Value > 0)
				{
					return Value + TypedASTSelfRecursion(Value - 1);
				}
				return 1;
			}

			int TypedASTMutualOdd(int Value)
			{
				if (Value > 0)
				{
					return TypedASTMutualEven(Value - 1) + 1;
				}
				return 100;
			}

			UFUNCTION()
			int TypedASTMutualEven(int Value)
			{
				if (Value > 0)
				{
					return TypedASTMutualOdd(Value - 1) + 1;
				}
				return 100;
			}

			int TypedASTFrameHelper(int Value)
			{
				return TypedASTObserveFrame(2) + Value;
			}

			UFUNCTION()
			int TypedASTFrameEntry(int Value)
			{
				int RootBefore = TypedASTObserveFrame(1);
				int Nested = TypedASTFrameHelper(Value);
				int RootAfter = TypedASTObserveFrame(3);
				return RootBefore + Nested + RootAfter;
			}

			int TypedASTExceptionHelper(int Numerator, int Denominator)
			{
				return Numerator / Denominator;
			}

			UFUNCTION()
			int TypedASTExceptionEntry(int Numerator, int Denominator)
			{
				int Result = TypedASTExceptionHelper(Numerator, Denominator);
				TypedASTProviderPrivateAdd(1, 2);
				return Result + 99;
			}
			)AS");
		return ScriptSource;
	}

	const FName& GetTypedControlFlowModuleName()
	{
		static const FName ModuleName(
			TEXT("ASStaticJITTypedControlFlowFixture"));
		return ModuleName;
	}

	const FName& GetTypedControlFlowStaticsClassName()
	{
		static const FName ClassName(
			TEXT("UModule_ASStaticJITTypedControlFlowFixtureStatics"));
		return ClassName;
	}

	const FName& GetTypedFinalScalarModuleName()
	{
		static const FName ModuleName(
			TEXT("ASStaticJITTypedFinalScalarFixture"));
		return ModuleName;
	}

	const FName& GetTypedFinalScalarReceiverClassName()
	{
		static const FName ClassName(TEXT("UTypedASTFinalScalarReceiver"));
		return ClassName;
	}

	const FString& GetTypedControlFlowSourceFilename()
	{
		static const FString SourceFilename(
			TEXT("ASStaticJITTypedControlFlowFixture.as"));
		return SourceFilename;
	}

	const FString& GetTypedControlFlowScriptSource()
	{
		static const FString ScriptSource = ASTEST_AS(R"AS(
			enum ETypedASTControlMode
			{
				Zero,
				One,
				Two,
			}

			UFUNCTION()
			int SemanticStructuredControlFlow(int Limit, int Selector)
			{
				int Trace = 0;
				for (int Index = 0; Index < Limit; ++Index)
				{
					if (Index == 1)
					{
						continue;
					}

					switch (Index)
					{
					case 2:
						Trace += 20;
						break;
					case 3:
						Trace += 30;
						continue;
					default:
						Trace += Index;
						break;
					}

					if (Trace > 45)
					{
						break;
					}
				}

				int WhileIndex = 0;
				while (WhileIndex < Limit)
				{
					++WhileIndex;
					if (WhileIndex == 2)
					{
						continue;
					}
					Trace += 3;
					if (WhileIndex >= 4)
					{
						break;
					}
				}

				int DoIndex = 0;
				do
				{
					++DoIndex;
					if (DoIndex == 1)
					{
						continue;
					}
					Trace += 5;
				}
				while (DoIndex < 2);

				switch (Selector)
				{
				case 0:
					Trace += 100;
					break;
				case 1:
					Trace += 200;
				case 2:
				{
					int CaseLocal = 20;
					Trace += CaseLocal;
					break;
				}
				case 3:
				{
					int NestedIndex = 0;
					while (NestedIndex < 2)
					{
						Trace += 7;
						++NestedIndex;
						if (NestedIndex == 1)
						{
							continue;
						}
						break;
					}
					break;
				}
				default:
					Trace += 9;
					break;
				}
				return Trace;
			}

			UFUNCTION()
			int SemanticExhaustiveEnum(int RawMode)
			{
				ETypedASTControlMode Mode = ETypedASTControlMode(RawMode);
				switch (Mode)
				{
				case ETypedASTControlMode::Zero:
					return 10;
				case ETypedASTControlMode::One:
					return 20;
				case ETypedASTControlMode::Two:
					return 30;
				}
			}

			UFUNCTION()
			int SemanticCheckedDivide(int Numerator, int Denominator)
			{
				return Numerator / Denominator;
			}

			UFUNCTION()
			int SemanticCloneableState(
				int Seed,
				int Mode,
				bool bLeft,
				bool bRight)
			{
				int Trace = Seed;
				switch (Mode)
				{
				case 0:
				{
					int Eager =
						(++Trace) * 100
						+ (++Trace) * 10
						+ (++Trace);
					return Trace * 1000 + Eager;
				}
				case 1:
				{
					bool bGate =
						bLeft && (++Trace > Seed) && bRight;
					if (bGate)
					{
						return Trace * 10 + 1;
					}
					return Trace * 10;
				}
				case 2:
				{
					Trace += 3;
					int Prefix = ++Trace;
					int Postfix = Trace++;
					return Trace * 100 + Prefix * 10 + Postfix;
				}
				case 3:
				{
					int WhileIndex = 0;
					while (WhileIndex < 3)
					{
						++WhileIndex;
						if (WhileIndex == 1)
						{
							continue;
						}
						Trace += WhileIndex;
					}

					for (int Outer = 0; Outer < 4; ++Outer)
					{
						switch (Outer)
						{
						case 1:
							Trace += 10;
							continue;
						case 2:
							Trace += 20;
							break;
						default:
							Trace += Outer;
							break;
						}

						if (Outer == 2)
						{
							break;
						}
					}
					return Trace;
				}
				case 4:
				{
					int ConditionEffect = 0;
					while ((10 / Seed) > ++ConditionEffect)
					{
						Trace += 1;
						break;
					}
					return Trace + ConditionEffect;
				}
				case 5:
				{
					for (int Index = 0; Index < 1; ++Index)
					{
						Trace += 10 / Seed;
						Trace += 100;
					}
					return Trace;
				}
				case 6:
				{
					int LaterIncrement = 0;
					for (int Index = 0; Index < 1;
						Index = 10 / Seed, ++LaterIncrement)
					{
						++Index;
						continue;
					}
					return Trace + LaterIncrement;
				}
				case 7:
				{
					switch (10 / Seed)
					{
					case 0:
						Trace += 1;
						break;
					default:
						Trace += 2;
						break;
					}
					return Trace;
				}
				case 8:
				{
					int Index = 0;
					do
					{
						++Index;
						continue;
					}
					while ((10 / Seed) > Index);
					return Trace + Index;
				}
				case 10:
				{
					do
					{
						Trace += 10 / Seed;
						Trace += 100;
					}
					while (false);
					return Trace;
				}
				case 11:
				{
					int Index = 0;
					while (Index == 0 || (10 / Seed) > Index)
					{
						++Index;
						continue;
					}
					return Trace + Index;
				}
				case 12:
				{
					for (int Index = 0; (10 / Seed) > Index; ++Index)
					{
						break;
					}
					return Trace;
				}
				case 13:
				{
					while (Trace == Seed)
					{
						Trace += 10 / Seed;
						Trace += 100;
						break;
					}
					return Trace;
				}
				case 14:
				{
					int Index = 0;
					do
					{
						++Index;
						if (Index == 1)
						{
							continue;
						}
						Trace += Index * 10;
					}
					while (Index < 3);
					return Trace * 10 + Index;
				}
				case 15:
				{
					if (Seed < 0)
					{
						return -10;
					}
					if (Seed == 0)
					{
						return 0;
					}
					if (bLeft)
					{
						if (bRight)
						{
							return 11;
						}
						return 12;
					}
					else if (bRight)
					{
						return 13;
					}
					return 14;
				}
				case 16:
				{
					switch (Seed)
					{
					case 0:
						Trace += 1;
						fallthrough;
					case 1:
					{
						int CaseLocal = 20;
						Trace += CaseLocal;
						break;
					}
					default:
					{
						int CaseLocal = 30;
						Trace += CaseLocal;
						break;
					}
					}
					return Trace;
				}
				default:
					return -1;
				}
			}
			)AS");
		return ScriptSource;
	}

	const FName& GetIneligibleReceiverModuleName()
	{
		static const FName ModuleName(
			TEXT("ASStaticJITAotIneligibleReceiverFixture"));
		return ModuleName;
	}

	const FString& GetIneligibleReceiverSourceFilename()
	{
		static const FString SourceFilename(
			TEXT("ASStaticJITAotIneligibleReceiverFixture.as"));
		return SourceFilename;
	}

	const FString& GetIneligibleReceiverScriptSource()
	{
		static const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UAotIneligibleMixinHost : UObject
			{
				UFUNCTION()
				int MixinReceiverRoot(int Amount) final
				{
					return this.MixinAddAmount(Amount);
				}
			}

			mixin int MixinAddAmount(UAotIneligibleMixinHost Self, int Amount)
			{
				return Amount + 10;
			}

			UFUNCTION()
			int ExternalReceiverRoot(UAotIneligibleMixinHost Self, int Add)
				external_implicit_this
			{
				return Add + 7;
			}

			UCLASS()
			class UAotGeneratedLifecycleHost : UObject
			{
				UPROPERTY()
				int StoredValue = 17;

				UFUNCTION()
				int GeneratedLifecycleRoot() final
				{
					return StoredValue;
				}
			}
			)AS");
		return ScriptSource;
	}

	const FString& GetMixinReceiverRootDeclaration()
	{
		static const FString Declaration(
			TEXT("int MixinReceiverRoot(int)"));
		return Declaration;
	}

	const FString& GetExternalReceiverRootDeclaration()
	{
		static const FString Declaration(
			TEXT("int ExternalReceiverRoot(FAotExternalValue&inout, int)"));
		return Declaration;
	}

	const FString& GetGeneratedLifecycleRootDeclaration()
	{
		static const FString Declaration(
			TEXT("int GeneratedLifecycleRoot()"));
		return Declaration;
	}

	int32 GetExpectedMixinReceiverRootResult()
	{
		return 15;
	}

	int32 GetExpectedExternalReceiverRootResult()
	{
		return 12;
	}

	int32 GetExpectedGeneratedLifecycleRootResult()
	{
		return 17;
	}

	const FString& GetTypedFinalScalarSourceFilename()
	{
		static const FString SourceFilename(
			TEXT("ASStaticJITTypedFinalScalarFixture.as"));
		return SourceFilename;
	}

	const FString& GetTypedFinalScalarScriptSource()
	{
		static const FString ScriptSource = ASTEST_AS(R"AS(
			import int ImportedValueForAOT(int Value) from "ASStaticJITAotImportProvider";

			const int TypedASTDifferentialFoldedGlobal = 7001;

			UCLASS()
			class UTypedASTFinalScalarReceiver : UObject
			{
				UFUNCTION()
				int TypedFinalRawValue() final
				{
					return 84;
				}

				UFUNCTION()
				int DifferentialScalarValue(int A, int B) final
				{
					return A * 3 + B - 7;
				}

				UFUNCTION()
				int SemanticImportedBinding(int Value) final
				{
					return ImportedValueForAOT(Value);
				}

				UFUNCTION()
				int SemanticNumericBoundary(
					int Left,
					int Right,
					int ShiftCount,
					int Mode) final
				{
					switch (Mode)
					{
					case 0:
						return Left + Right;
					case 1:
						return Left - Right;
					case 2:
						return Left * Right;
					case 3:
						return Left / Right;
					case 4:
						return Left % Right;
					case 5:
						return Left << ShiftCount;
					case 6:
						return Left >> ShiftCount;
					case 7:
						return Left >>> ShiftCount;
					case 8:
					{
						int64 Wide = int64(Left);
						if (Wide == int64(Left))
						{
							return Left;
						}
						return 0;
					}
					case 9:
						if (Left != 0)
						{
							return 1;
						}
						return 0;
					case 10:
						return TypedASTDifferentialFoldedGlobal + Left;
					default:
						return -1;
					}
				}

				UFUNCTION()
				float64 SemanticPowerBoundary(
					float64 Base,
					float64 Exponent) final
				{
					return Base ** Exponent;
				}
			}
			)AS");
		return ScriptSource;
	}

	const FString& GetScriptSource()
	{
		static const FString ScriptSource = ASTEST_AS(R"AS(
			import int ImportedValueForAOT(int Value) from "ASStaticJITAotImportProvider";

			int AddForAOT(int Value)
			{
				return Value + 7;
			}

			int SemanticScalarBranch(int A, int B)
			{
				int Sum = A + B;
				if (Sum > 10)
				{
					return Sum * 2;
				}
				return Sum - 1;
			}

			int SemanticMutationParity(int Input, int Source)
			{
				int Value = Input;
				Value = Source;
				Value += 3;
				int Prefix = ++Value;
				int Postfix = Value++;
				return Value * 1000 + Prefix * 100 + Postfix;
			}

			float32 SemanticPowerFloat(float32 Base, float32 Exponent)
			{
				return Base ** Exponent;
			}

			float64 SemanticPowerDouble(float64 Base, float64 Exponent)
			{
				return Base ** Exponent;
			}

			float64 SemanticPowerDoubleInt(float64 Base, int Exponent)
			{
				return Base ** Exponent;
			}

			UFUNCTION()
			int TypedASTExportedSymbolShowcase(int Year, int Month)
			{
				return FDateTime::DaysInMonth(Year, Month);
			}

			UFUNCTION()
			int TypedASTCapabilityShowcase(
				int A,
				int B,
				bool bEnabled,
				bool bExtraCheck)
			{
				int Value = A + B;
				Value = Value - 1;
				Value += 3;
				Value -= 1;
				Value *= 2;

				int PrefixResult = ++Value;
				int PostfixResult = Value++;
				int Negated = -PrefixResult;
				int Inverted = ~PostfixResult;
				int ShiftedLeft = Value << 1;
				int ShiftedRight = Value >> 1;

				bool bMutationGate = bEnabled && (++Value > B);
				bool bNativeGate =
					bMutationGate && bExtraCheck && IsRunningCommandlet();
				if (bNativeGate)
				{
					return ShiftedLeft + Inverted + Value;
				}
				if (ShiftedRight > 10)
				{
					return ShiftedRight * 2 + Value;
				}
				return Negated - 1 + Value;
			}

			int Entry()
			{
				return AddForAOT(35);
			}

			int64 DoubleToInt64ForAOT(double Value)
			{
				return int64(Value);
			}

			uint64 DoubleToUint64ForAOT(double Value)
			{
				return uint64(Value);
			}

			int ObjectLastNativeForAOT()
			{
				FAotObjectLastProbe Value(39, 97);
				return Value.Value;
			}

			void FailForAOT()
			{
				throw("StaticJITAotNestedFailure");
			}

			int ExceptionEntryForAOT()
			{
				FailForAOT();
				return 99;
			}

			int BumpReferenceForAOT(int& Value)
			{
				Value += 5;
				return Value * 2;
			}

			int ReferenceEntryForAOT()
			{
				int Value = 7;
				int Returned = BumpReferenceForAOT(Value);
				return Value * 100 + Returned;
			}

			int ObjectLifetimeEntryForAOT()
			{
				FString Original = "route";
				FString Decorated = Original + "!";
				return (Original == "route" ? 100 : 0) + Decorated.Len();
			}

			int RecurseForAOT(int Value)
			{
				if (Value == 0)
				{
					return 0;
				}
				return Value + RecurseForAOT(Value - 1) * 10;
			}

			int RecursionEntryForAOT()
			{
				return RecurseForAOT(4);
			}

			int ImportEntryForAOT()
			{
				return ImportedValueForAOT(10);
			}

			UCLASS()
			class UStaticJITAotFunctionCarrier : UObject
			{
				UPROPERTY()
					int StoredValue;

				int AddForAOT(int Value)
				{
					return Value + 7;
				}

				UFUNCTION()
					int Entry()
				{
					return AddForAOT(35);
				}

				int64 DoubleToInt64ForAOT(double Value)
				{
					return int64(Value);
				}

				uint64 DoubleToUint64ForAOT(double Value)
				{
					return uint64(Value);
				}

				int ObjectLastNativeForAOT()
				{
					FAotObjectLastProbe Value(39, 97);
					return Value.Value;
				}

				void FailForAOT()
				{
					throw("StaticJITAotNestedFailure");
				}

				int ExceptionEntryForAOT()
				{
					FailForAOT();
					return 99;
				}

				int BumpReferenceForAOT(int& Value)
				{
					Value += 5;
					return Value * 2;
				}

				int ReferenceEntryForAOT()
				{
					int Value = 7;
					int Returned = BumpReferenceForAOT(Value);
					return Value * 100 + Returned;
				}

				UFUNCTION()
				int ObjectLifetimeEntryForAOT()
				{
					FString Original = "route";
					FString Decorated = Original + "!";
					return (Original == "route" ? 100 : 0) + Decorated.Len();
				}

				int RecurseForAOT(int Value)
				{
					if (Value == 0)
					{
						return 0;
					}
					return Value + RecurseForAOT(Value - 1) * 10;
				}

				int RecursionEntryForAOT()
				{
					return RecurseForAOT(4);
				}

				UFUNCTION()
				void ResetStoredValue()
				{
					StoredValue = 71;
				}

				UFUNCTION()
				void StorePrimitiveArg(int Value)
				{
					StoredValue = Value + 3;
				}

				UFUNCTION()
				int ReturnPrimitive()
				{
					return 61;
				}

				UFUNCTION()
				void IncrementReference(int& Value)
				{
					Value += 9;
					StoredValue = Value;
				}

				UFUNCTION()
				int BumpReference(int& Value)
				{
					Value += 5;
					StoredValue = Value;
					return Value;
				}

				UFUNCTION()
				UStaticJITAotFunctionCarrier ReturnSelfObject()
				{
					return this;
				}

				UFUNCTION()
				int AddTwo(int A, int B)
				{
					return A + B;
				}

				UFUNCTION(meta = (BlueprintThreadSafe))
				int ThreadSafeReturn()
				{
					return 73;
				}

				UFUNCTION(Client)
				void ClientStoreRpcValue(int Value)
				{
					StoredValue = Value;
				}

			}

			UCLASS()
			class UStaticJITAotVirtualParent : UObject
			{
				UFUNCTION(BlueprintEvent)
					int GetVirtualValue()
				{
					return 21;
				}

				UFUNCTION()
				int CallVirtualValue()
				{
					return GetVirtualValue();
				}
			}

			UCLASS()
			class UStaticJITAotVirtualChild : UStaticJITAotVirtualParent
			{
				UFUNCTION(BlueprintOverride)
					int GetVirtualValue()
				{
					return 217;
				}
			}

			UFUNCTION()
			int BytecodeFallbackGeneratedWorldContext(int Value)
			{
				FString Marker = "vm";
				return Value + Marker.Len();
			}

			UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
			int StaticWorldContextCheck(UObject WorldContextObject, int Value)
			{
				if (__WorldContext() != WorldContextObject)
				{
					return -1;
				}
				return Value + 2;
			}

			)AS");
		return ScriptSource;
	}

	const FName& GetGeneratedClassName()
	{
		static const FName ClassName(TEXT("UStaticJITAotFunctionCarrier"));
		return ClassName;
	}

	const FName& GetVirtualParentClassName()
	{
		static const FName ClassName(TEXT("UStaticJITAotVirtualParent"));
		return ClassName;
	}

	const FName& GetVirtualChildClassName()
	{
		static const FName ClassName(TEXT("UStaticJITAotVirtualChild"));
		return ClassName;
	}

	const FString& GetEntryDeclaration()
	{
		static const FString EntryDeclaration(TEXT("int Entry()"));
		return EntryDeclaration;
	}

	const FString& GetSemanticScalarBranchDeclaration()
	{
		static const FString Declaration(
			TEXT("int SemanticScalarBranch(int, int)"));
		return Declaration;
	}

	const FString& GetSemanticMutationParityDeclaration()
	{
		static const FString Declaration(
			TEXT("int SemanticMutationParity(int, int)"));
		return Declaration;
	}

	const FString& GetSemanticStructuredControlFlowDeclaration()
	{
		static const FString Declaration(
			TEXT("int SemanticStructuredControlFlow(int, int)"));
		return Declaration;
	}

	const FString& GetSemanticExhaustiveEnumDeclaration()
	{
		static const FString Declaration(
			TEXT("int SemanticExhaustiveEnum(int)"));
		return Declaration;
	}

	const FString& GetSemanticCheckedDivideDeclaration()
	{
		static const FString Declaration(
			TEXT("int SemanticCheckedDivide(int, int)"));
		return Declaration;
	}

	const FString& GetSemanticNumericBoundaryDeclaration()
	{
		static const FString Declaration(
			TEXT("int SemanticNumericBoundary(int, int, int, int)"));
		return Declaration;
	}

	const FString& GetSemanticPowerBoundaryDeclaration()
	{
		static const FString Declaration(
			TEXT("float64 SemanticPowerBoundary(float64, float64)"));
		return Declaration;
	}

	const FString& GetSemanticImportedBindingDeclaration()
	{
		static const FString Declaration(
			TEXT("int SemanticImportedBinding(int)"));
		return Declaration;
	}

	const FString& GetSemanticCloneableStateDeclaration()
	{
		static const FString Declaration(
			TEXT("int SemanticCloneableState(int, int, bool, bool)"));
		return Declaration;
	}

	const FString& GetTypedFinalScalarRawDeclaration()
	{
		static const FString Declaration(
			TEXT("int TypedFinalRawValue()"));
		return Declaration;
	}

	const FString& GetTypedDifferentialScalarDeclaration()
	{
		static const FString Declaration(
			TEXT("int DifferentialScalarValue(const int, const int)"));
		return Declaration;
	}

	const FString& GetSemanticPowerFloatDeclaration()
	{
		static const FString Declaration(
			TEXT("float32 SemanticPowerFloat(float32, float32)"));
		return Declaration;
	}

	const FString& GetSemanticPowerDoubleDeclaration()
	{
		static const FString Declaration(
			TEXT("float64 SemanticPowerDouble(float64, float64)"));
		return Declaration;
	}

	const FString& GetSemanticPowerDoubleIntDeclaration()
	{
		static const FString Declaration(
			TEXT("float64 SemanticPowerDoubleInt(float64, int)"));
		return Declaration;
	}

	const FString& GetTypedASTCapabilityShowcaseDeclaration()
	{
		static const FString Declaration(
			TEXT("int TypedASTCapabilityShowcase(int, int, bool, bool)"));
		return Declaration;
	}

	const FString& GetTypedASTExportedSymbolShowcaseDeclaration()
	{
		static const FString Declaration(
			TEXT("int TypedASTExportedSymbolShowcase(int, int)"));
		return Declaration;
	}

	const FString& GetTypedASTPrivateBridgeShowcaseDeclaration()
	{
		static const FString Declaration(
			TEXT("int TypedASTPrivateBridgeShowcase(int, int)"));
		return Declaration;
	}

	const FString& GetTypedASTRuntimeThunkShowcaseDeclaration()
	{
		static const FString Declaration(
			TEXT("int TypedASTRuntimeThunkShowcase(const FString&in)"));
		return Declaration;
	}

	const FString& GetTypedASTPrivateBridgeExceptionDeclaration()
	{
		static const FString Declaration(
			TEXT("int TypedASTPrivateBridgeException(int)"));
		return Declaration;
	}

	const FString& GetTypedASTPublicUASFunctionExceptionEntryDeclaration()
	{
		static const FString Declaration(
			TEXT("void TypedASTPublicUASFunctionExceptionEntry(int)"));
		return Declaration;
	}

	const FString& GetTypedASTOrdinaryBridgeSqrtDeclaration()
	{
		static const FString Declaration(
			TEXT("float32 TypedASTOrdinaryBridgeSqrt(float32)"));
		return Declaration;
	}

	const FString& GetTypedASTSelfRecursionDeclaration()
	{
		static const FString Declaration(
			TEXT("int TypedASTSelfRecursion(int)"));
		return Declaration;
	}

	const FString& GetTypedASTMutualRecursionDeclaration()
	{
		static const FString Declaration(
			TEXT("int TypedASTMutualEven(int)"));
		return Declaration;
	}

	const FString& GetTypedASTFrameEntryDeclaration()
	{
		static const FString Declaration(
			TEXT("int TypedASTFrameEntry(int)"));
		return Declaration;
	}

	const FString& GetTypedASTExceptionHelperDeclaration()
	{
		static const FString Declaration(
			TEXT("int TypedASTExceptionHelper(int, int)"));
		return Declaration;
	}

	const FString& GetTypedASTExceptionEntryDeclaration()
	{
		static const FString Declaration(
			TEXT("int TypedASTExceptionEntry(int, int)"));
		return Declaration;
	}

	const FString& GetTypedASTPrintShowcaseDeclaration()
	{
		static const FString Declaration(
			TEXT("int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)"));
		return Declaration;
	}

	const FString& GetDoubleToInt64Declaration()
	{
		static const FString Declaration(TEXT("int64 DoubleToInt64ForAOT(double)"));
		return Declaration;
	}

	const FString& GetDoubleToUint64Declaration()
	{
		static const FString Declaration(TEXT("uint64 DoubleToUint64ForAOT(double)"));
		return Declaration;
	}

	const FString& GetObjectLastNativeEntryDeclaration()
	{
		static const FString Declaration(TEXT("int ObjectLastNativeForAOT()"));
		return Declaration;
	}

	const FString& GetExceptionEntryDeclaration()
	{
		static const FString Declaration(TEXT("int ExceptionEntryForAOT()"));
		return Declaration;
	}

	const FString& GetExceptionThrowDeclaration()
	{
		static const FString Declaration(TEXT("void FailForAOT()"));
		return Declaration;
	}

	const FString& GetReferenceEntryDeclaration()
	{
		static const FString Declaration(TEXT("int ReferenceEntryForAOT()"));
		return Declaration;
	}

	const FString& GetObjectLifetimeEntryDeclaration()
	{
		static const FString Declaration(TEXT("int ObjectLifetimeEntryForAOT()"));
		return Declaration;
	}

	const FString& GetRecursionEntryDeclaration()
	{
		static const FString Declaration(TEXT("int RecursionEntryForAOT()"));
		return Declaration;
	}

	const FString& GetImportEntryDeclaration()
	{
		static const FString Declaration(TEXT("int ImportEntryForAOT()"));
		return Declaration;
	}

	const FString& GetVirtualCallDeclaration()
	{
		static const FString Declaration(TEXT("int CallVirtualValue()"));
		return Declaration;
	}

	const FString& GetVirtualOverrideDeclaration()
	{
		static const FString Declaration(TEXT("int GetVirtualValue()"));
		return Declaration;
	}

	const FString& GetVirtualOverrideImplementationDeclaration()
	{
		static const FString Declaration(TEXT("int GetVirtualValue_Implementation()"));
		return Declaration;
	}

	const FString& GetMethodNoParamsDeclaration()
	{
		static const FString Declaration(TEXT("void ResetStoredValue()"));
		return Declaration;
	}

	const FString& GetMethodPrimitiveArgDeclaration()
	{
		static const FString Declaration(TEXT("void StorePrimitiveArg(int)"));
		return Declaration;
	}

	const FString& GetMethodPrimitiveReturnDeclaration()
	{
		static const FString Declaration(TEXT("int ReturnPrimitive()"));
		return Declaration;
	}

	const FString& GetMethodReferenceArgDeclaration()
	{
		static const FString Declaration(TEXT("void IncrementReference(int&inout)"));
		return Declaration;
	}

	const FString& GetMethodReferenceDeclaration()
	{
		static const FString Declaration(TEXT("int BumpReference(int&inout)"));
		return Declaration;
	}

	const FString& GetMethodObjectReturnDeclaration()
	{
		static const FString Declaration(
			TEXT("UStaticJITAotFunctionCarrier@ ReturnSelfObject()"));
		return Declaration;
	}

	const FString& GetMethodMultiArgDeclaration()
	{
		static const FString Declaration(TEXT("int AddTwo(int, int)"));
		return Declaration;
	}

	const FString& GetMethodThreadSafeDeclaration()
	{
		static const FString Declaration(TEXT("int ThreadSafeReturn()"));
		return Declaration;
	}

	const FString& GetMethodRpcImplementationDeclaration()
	{
		static const FString Declaration(
			TEXT("void ClientStoreRpcValue_Implementation(int)"));
		return Declaration;
	}

	const FString& GetStaticWorldContextDeclaration()
	{
		static const FString Declaration(TEXT("int StaticWorldContextCheck(UObject, int)"));
		return Declaration;
	}

	const FString& GetBytecodeFallbackGeneratedWorldContextDeclaration()
	{
		static const FString Declaration(
			TEXT("int BytecodeFallbackGeneratedWorldContext(int)"));
		return Declaration;
	}

	int32 GetExpectedEntryResult()
	{
		return 42;
	}

	int32 GetExpectedNoParamsStoredValue()
	{
		return 71;
	}

	int32 GetExpectedPrimitiveArgStoredValue()
	{
		return 44;
	}

	int32 GetExpectedPrimitiveReturnValue()
	{
		return 61;
	}

	int32 GetExpectedReferenceArgValue()
	{
		return 22;
	}

	int32 GetExpectedReferenceReturnValue()
	{
		return 18;
	}

	int32 GetExpectedMultiArgResult()
	{
		return 42;
	}

	int32 GetExpectedThreadSafeResult()
	{
		return 73;
	}

	int32 GetExpectedRpcStoredValue()
	{
		return 91;
	}

	int32 GetExpectedStaticWorldContextResult()
	{
		return 33;
	}

	int32 GetExpectedBytecodeFallbackGeneratedWorldContextResult()
	{
		return 33;
	}

	int32 GetExpectedObjectLastNativeResult()
	{
		return 39097;
	}

	const FString& GetExpectedExceptionText()
	{
		static const FString ExceptionText(TEXT("StaticJITAotNestedFailure"));
		return ExceptionText;
	}

	int32 GetExpectedReferenceEntryResult()
	{
		return 1224;
	}

	int32 GetExpectedObjectLifetimeResult()
	{
		return 106;
	}

	int32 GetExpectedRecursionResult()
	{
		return 1234;
	}

	int32 GetExpectedImportResult()
	{
		return 87;
	}

	int32 GetExpectedNestedThrowLine()
	{
		const FString& Source = GetScriptSource();
		const int32 ThrowOffset = Source.Find(
			TEXT("throw(\"StaticJITAotNestedFailure\")"),
			ESearchCase::CaseSensitive);
		check(ThrowOffset != INDEX_NONE);

		int32 Line = 1;
		for (int32 Index = 0; Index < ThrowOffset; ++Index)
		{
			if (Source[Index] == TEXT('\n'))
			{
				++Line;
			}
		}
		return Line;
	}

	int32 GetExpectedVirtualOverrideResult()
	{
		return 217;
	}

	bool RegisterObjectLastNativeSurface(asIScriptEngine& ScriptEngine)
	{
		if (ScriptEngine.GetTypeInfoByDecl("FAotObjectLastProbe") != nullptr)
		{
			return true;
		}

		const ASAutoCaller::FunctionCaller ConstructorCaller =
			ASAutoCaller::MakeFunctionCaller(ConstructObjectLastNativeProbe);
		return ScriptEngine.RegisterObjectType(
				"FAotObjectLastProbe",
				sizeof(FObjectLastNativeProbe),
				asOBJ_VALUE
					| asOBJ_POD
					| asGetTypeTraits<FObjectLastNativeProbe>()
					| asOBJ_APP_CLASS_ALLINTS) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FAotObjectLastProbe",
				asBEHAVE_CONSTRUCT,
				"void f(int Left, int Right)",
				asFUNCTION(ConstructObjectLastNativeProbe),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ConstructorCaller) >= 0
			&& ScriptEngine.RegisterObjectProperty(
				"FAotObjectLastProbe",
				"int Value",
				asOFFSET(FObjectLastNativeProbe, Value)) >= 0;
	}

	bool RegisterTypedASTPrivateBridgeSurface(
		FAngelscriptEngine& Engine,
		const ETypedASTPrivateBridgeRegistration Registration)
	{
		asIScriptEngine* const ScriptEngine = Engine.GetScriptEngine();
		if (ScriptEngine == nullptr)
		{
			return false;
		}

		auto AttachDescriptor = [&Engine](
			asIScriptFunction* TargetFunction,
			const TCHAR* Symbol,
			const TCHAR* CppCallableSignature,
			const EAngelscriptStaticJITNativeCallABIDomain ABIDomain,
			const EAngelscriptStaticJITNativeCallExceptionPolicy ExceptionPolicy)
		{
			if (TargetFunction == nullptr)
			{
				return false;
			}
			FAngelscriptStaticJITNativeABIIdentity NativeABI;
			FAngelscriptStaticJITScalarABIIdentity ScalarABI;
			if (!TryBuildAngelscriptStaticJITNativeABIIdentity(
					*TargetFunction, NativeABI)
				|| (ABIDomain
						== EAngelscriptStaticJITNativeCallABIDomain::Scalar
					&& !TryBuildAngelscriptStaticJITScalarABIIdentity(
						*TargetFunction, ScalarABI)))
			{
				return false;
			}

			FAngelscriptStaticJITNativeCallDescriptor Descriptor;
			Descriptor.Linkage =
				EAngelscriptStaticJITNativeCallLinkage::ProviderPrivate;
			Descriptor.Visibility =
				EAngelscriptStaticJITNativeCallVisibility::ProviderPrivate;
			Descriptor.Symbol = Symbol;
			Descriptor.OwningModule = TEXT("AngelscriptTest");
			Descriptor.CppCallableSignature = CppCallableSignature;
			Descriptor.NativeABI = MoveTemp(NativeABI);
			Descriptor.ABIDomain = ABIDomain;
			Descriptor.ExceptionPolicy = ExceptionPolicy;
			Descriptor.ScalarABI = MoveTemp(ScalarABI);
			Descriptor.RouteFlags = ABIDomain
				== EAngelscriptStaticJITNativeCallABIDomain::Scalar
					? EAngelscriptStaticJITNativeCallRouteFlags::
						CurrentNativeCall
					: EAngelscriptStaticJITNativeCallRouteFlags::ScalarBridge;
			Descriptor.LifetimeFlags =
				EAngelscriptStaticJITNativeCallLifetimeFlags::ByValueScalarOnly
				| EAngelscriptStaticJITNativeCallLifetimeFlags::NoRetainedReferences
				| EAngelscriptStaticJITNativeCallLifetimeFlags::NoEngineLocalCapture;
			return Engine.GetPurpose()
					!= EAngelscriptEnginePurpose::StaticJITGeneration
				|| FAngelscriptStaticJITNativeCallRegistry::Attach(
					Engine, TargetFunction, Descriptor);
		};

		asIScriptFunction* AddFunction = ScriptEngine->GetGlobalFunctionByDecl(
			"int TypedASTProviderPrivateAdd(int Left, int Right)");
		if (AddFunction == nullptr)
		{
			int32 FunctionId = asERROR;
			if (Registration
				== ETypedASTPrivateBridgeRegistration::Replacement)
			{
				const ASAutoCaller::FunctionCaller Caller =
					ASAutoCaller::MakeFunctionCaller(
						TypedASTProviderPrivateAddReplacement);
				FunctionId = ScriptEngine->RegisterGlobalFunction(
					"int TypedASTProviderPrivateAdd(int Left, int Right)",
					asFUNCTION(TypedASTProviderPrivateAddReplacement),
					asCALL_CDECL,
					*(asFunctionCaller*)&Caller);
			}
			else
			{
				const ASAutoCaller::FunctionCaller Caller =
					ASAutoCaller::MakeFunctionCaller(
						TypedASTProviderPrivateAdd);
				FunctionId = ScriptEngine->RegisterGlobalFunction(
					"int TypedASTProviderPrivateAdd(int Left, int Right)",
					asFUNCTION(TypedASTProviderPrivateAdd),
					asCALL_CDECL,
					*(asFunctionCaller*)&Caller);
			}
			AddFunction = FunctionId >= 0
				? ScriptEngine->GetFunctionById(FunctionId) : nullptr;
		}
		if (!AttachDescriptor(
			AddFunction,
			TEXT("AngelscriptStaticJITAotFixture::TypedASTProviderPrivateAdd"),
			TEXT("int32(int32, int32)"),
			EAngelscriptStaticJITNativeCallABIDomain::Scalar,
			EAngelscriptStaticJITNativeCallExceptionPolicy::
				DoesNotSetScriptException))
		{
			return false;
		}

		asIScriptFunction* FrameObservationFunction =
			ScriptEngine->GetGlobalFunctionByDecl(
				"int TypedASTObserveFrame(int Marker)");
		if (FrameObservationFunction == nullptr)
		{
			const ASAutoCaller::FunctionCaller Caller =
				ASAutoCaller::MakeFunctionCaller(TypedASTObserveFrame);
			const int32 FunctionId = ScriptEngine->RegisterGlobalFunction(
				"int TypedASTObserveFrame(int Marker)",
				asFUNCTION(TypedASTObserveFrame),
				asCALL_CDECL,
				*(asFunctionCaller*)&Caller);
			FrameObservationFunction = FunctionId >= 0
				? ScriptEngine->GetFunctionById(FunctionId) : nullptr;
		}
		if (!AttachDescriptor(
			FrameObservationFunction,
			TEXT("AngelscriptStaticJITAotFixture::TypedASTObserveFrame"),
			TEXT("int32(int32)"),
			EAngelscriptStaticJITNativeCallABIDomain::Scalar,
			EAngelscriptStaticJITNativeCallExceptionPolicy::
				DoesNotSetScriptException))
		{
			return false;
		}

		asIScriptFunction* RuntimeThunkFunction =
			ScriptEngine->GetGlobalFunctionByDecl(
				"int TypedASTRuntimeThunkProbe(const FString&in Text)");
		if (RuntimeThunkFunction == nullptr)
		{
			const ASAutoCaller::FunctionCaller Caller =
				ASAutoCaller::MakeFunctionCaller(
					TypedASTRuntimeThunkRegistered);
			const int32 FunctionId = ScriptEngine->RegisterGlobalFunction(
				"int TypedASTRuntimeThunkProbe(const FString&in Text)",
				asFUNCTION(TypedASTRuntimeThunkRegistered),
				asCALL_CDECL,
				*(asFunctionCaller*)&Caller);
			RuntimeThunkFunction = FunctionId >= 0
				? ScriptEngine->GetFunctionById(FunctionId) : nullptr;
		}
		if (RuntimeThunkFunction == nullptr)
		{
			return false;
		}
		FAngelscriptStaticJITNativeABIIdentity RuntimeThunkNativeABI;
		if (!TryBuildAngelscriptStaticJITNativeABIIdentity(
				*RuntimeThunkFunction, RuntimeThunkNativeABI))
		{
			return false;
		}
		FAngelscriptStaticJITNativeCallDescriptor RuntimeThunkDescriptor;
		RuntimeThunkDescriptor.Linkage =
			EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeThunk;
		RuntimeThunkDescriptor.Visibility =
			EAngelscriptStaticJITNativeCallVisibility::External;
		RuntimeThunkDescriptor.Symbol = TEXT(
			"AngelscriptStaticJITNativeCallables::RuntimeThunkProbe");
		RuntimeThunkDescriptor.Include = TEXT(
			"StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallables.h");
		RuntimeThunkDescriptor.OwningModule = TEXT("AngelscriptRuntime");
		RuntimeThunkDescriptor.ApiMacro = TEXT("ANGELSCRIPTRUNTIME_API");
		RuntimeThunkDescriptor.CppCallableSignature =
			TEXT("int32(const FString&)");
		RuntimeThunkDescriptor.NativeABI = MoveTemp(RuntimeThunkNativeABI);
		RuntimeThunkDescriptor.ABIDomain =
			EAngelscriptStaticJITNativeCallABIDomain::BorrowedManagedPOD;
		RuntimeThunkDescriptor.ExceptionPolicy =
			EAngelscriptStaticJITNativeCallExceptionPolicy::
				DoesNotSetScriptException;
		RuntimeThunkDescriptor.RouteFlags =
			EAngelscriptStaticJITNativeCallRouteFlags::DirectCall;
		RuntimeThunkDescriptor.LifetimeFlags =
			EAngelscriptStaticJITNativeCallLifetimeFlags::NoRetainedReferences
			| EAngelscriptStaticJITNativeCallLifetimeFlags::NoEngineLocalCapture
			| EAngelscriptStaticJITNativeCallLifetimeFlags::BorrowedInputReferences
			| EAngelscriptStaticJITNativeCallLifetimeFlags::ByValueObjectArguments;
		if (Engine.GetPurpose() == EAngelscriptEnginePurpose::StaticJITGeneration
			&& !FAngelscriptStaticJITNativeCallRegistry::Attach(
				Engine, RuntimeThunkFunction, RuntimeThunkDescriptor))
		{
			return false;
		}

		asIScriptFunction* ExceptionFunction =
			ScriptEngine->GetGlobalFunctionByDecl(
				"int TypedASTProviderPrivateFail(int Value)");
		if (ExceptionFunction == nullptr)
		{
			const int32 FunctionId = ScriptEngine->RegisterGlobalFunction(
				"int TypedASTProviderPrivateFail(int Value)",
				asFUNCTION(TypedASTProviderPrivateFail),
				asCALL_GENERIC);
			ExceptionFunction = FunctionId >= 0
				? ScriptEngine->GetFunctionById(FunctionId) : nullptr;
		}
		asIScriptFunction* PublicInvokeFunction =
			ScriptEngine->GetGlobalFunctionByDecl(
				"void TypedASTInvokePublicUASFunctionException(int Value)");
		if (PublicInvokeFunction == nullptr)
		{
			const int32 FunctionId = ScriptEngine->RegisterGlobalFunction(
				"void TypedASTInvokePublicUASFunctionException(int Value)",
				asFUNCTION(TypedASTInvokePublicUASFunctionException),
				asCALL_GENERIC);
			PublicInvokeFunction = FunctionId >= 0
				? ScriptEngine->GetFunctionById(FunctionId) : nullptr;
		}
		// The descriptor is generation-only inventory. Runtime Engines rebuild
		// the current function reference from their registration surface and do
		// not retain compatibility/native-form metadata.
		return PublicInvokeFunction != nullptr && AttachDescriptor(
			ExceptionFunction,
			TEXT("AngelscriptStaticJITAotFixture::TypedASTProviderPrivateFail"),
			TEXT("void(asIScriptGeneric*)"),
			EAngelscriptStaticJITNativeCallABIDomain::VMScalarBridge,
			EAngelscriptStaticJITNativeCallExceptionPolicy::MaySetScriptException);
	}

	void ResetTypedASTPrivateBridgeObservation()
	{
		TypedASTPrivateBridgeCallCount = 0;
		TypedASTPrivateBridgeReplacementCallCount = 0;
		TypedASTPrivateBridgeExceptionCallCount = 0;
		TypedASTPublicUASFunctionInvokeCallCount = 0;
	}

	void ResetTypedASTRuntimeThunkObservation()
	{
		TypedASTRuntimeThunkRegisteredCallCount = 0;
		AngelscriptStaticJITNativeCallables::
			ResetRuntimeThunkProbeCountForTesting();
	}

	int32 GetTypedASTRuntimeThunkRegisteredCallCount()
	{
		return TypedASTRuntimeThunkRegisteredCallCount;
	}

	int32 GetTypedASTPrivateBridgeCallCount()
	{
		return TypedASTPrivateBridgeCallCount;
	}

	int32 GetTypedASTPrivateBridgeReplacementCallCount()
	{
		return TypedASTPrivateBridgeReplacementCallCount;
	}

	int32 GetTypedASTPrivateBridgeExceptionCallCount()
	{
		return TypedASTPrivateBridgeExceptionCallCount;
	}

	int32 GetTypedASTPublicUASFunctionInvokeCallCount()
	{
		return TypedASTPublicUASFunctionInvokeCallCount;
	}

	int32 GetExpectedTypedASTPrivateBridgeResult()
	{
		return 142;
	}

	int32 GetExpectedTypedASTPrivateBridgeReplacementResult()
	{
		return 242;
	}

	const FString& GetExpectedTypedASTPrivateBridgeExceptionText()
	{
		static const FString Message(
			TEXT("TypedASTJIT provider-private bridge failure"));
		return Message;
	}

	int32 GetExpectedTypedASTSelfRecursionResult()
	{
		return 16;
	}

	int32 GetExpectedTypedASTMutualRecursionResult()
	{
		return 106;
	}

	int32 GetExpectedTypedASTFrameResult(const int32 Value)
	{
		return 60 + Value;
	}

	int32 GetExpectedTypedASTFrameObservationLine(const int32 Marker)
	{
		const FString& Source = GetTypedRecursionScriptSource();
		const FString Needle = FString::Printf(
			TEXT("TypedASTObserveFrame(%d)"), Marker);
		const int32 ObservationOffset = Source.Find(
			Needle, ESearchCase::CaseSensitive);
		check(ObservationOffset != INDEX_NONE);

		int32 Line = 1;
		for (int32 Index = 0; Index < ObservationOffset; ++Index)
		{
			if (Source[Index] == TEXT('\n'))
			{
				++Line;
			}
		}
		return Line;
	}

	int32 GetExpectedTypedASTFrameHelperCallLine()
	{
		const FString& Source = GetTypedRecursionScriptSource();
		const int32 CallOffset = Source.Find(
			TEXT("TypedASTFrameHelper(Value)"),
			ESearchCase::CaseSensitive);
		check(CallOffset != INDEX_NONE);

		int32 Line = 1;
		for (int32 Index = 0; Index < CallOffset; ++Index)
		{
			if (Source[Index] == TEXT('\n'))
			{
				++Line;
			}
		}
		return Line;
	}

	void ResetTypedASTFrameObservations()
	{
		TypedASTFrameObservations.Reset();
	}

	int32 GetTypedASTFrameObservationCount()
	{
		return TypedASTFrameObservations.Num();
	}

	bool TryGetTypedASTFrameObservation(
		const int32 Index,
		FTypedASTFrameObservation& OutObservation)
	{
		if (!TypedASTFrameObservations.IsValidIndex(Index))
		{
			OutObservation = {};
			return false;
		}
		OutObservation = TypedASTFrameObservations[Index];
		return true;
	}

	int32 GetExpectedTypedASTExceptionLine()
	{
		const FString& Source = GetTypedRecursionScriptSource();
		const int32 DivisionOffset = Source.Find(
			TEXT("return Numerator / Denominator;"),
			ESearchCase::CaseSensitive);
		check(DivisionOffset != INDEX_NONE);

		int32 Line = 1;
		for (int32 Index = 0; Index < DivisionOffset; ++Index)
		{
			if (Source[Index] == TEXT('\n'))
			{
				++Line;
			}
		}
		return Line;
	}

	void ResetObjectLastNativeObservation()
	{
		ObjectLastNativeObservation = FObjectLastNativeObservation();
	}

	int32 GetObjectLastNativeCallCount()
	{
		return ObjectLastNativeObservation.CallCount;
	}

	int32 GetObjectLastNativeLeftSentinel()
	{
		return ObjectLastNativeObservation.Left;
	}

	int32 GetObjectLastNativeRightSentinel()
	{
		return ObjectLastNativeObservation.Right;
	}

	int32 GetObjectLastNativeObjectValue()
	{
		return ObjectLastNativeObservation.ObjectValue;
	}

}
