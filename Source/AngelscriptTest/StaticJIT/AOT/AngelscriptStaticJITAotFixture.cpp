#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"

#include "AngelscriptTestMacros.h"
#include "Core/FunctionCallers.h"
#include "Core/angelscript.h"

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

	const FString& GetScriptSource()
	{
		static const FString ScriptSource = ASTEST_AS(R"AS(
			import int ImportedValueForAOT(int Value) from "ASStaticJITAotImportProvider";

			int AddForAOT(int Value)
			{
				return Value + 7;
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

	const FString& GetStaticWorldContextDeclaration()
	{
		static const FString Declaration(TEXT("int StaticWorldContextCheck(UObject, int)"));
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

	int32 GetExpectedStaticWorldContextResult()
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
