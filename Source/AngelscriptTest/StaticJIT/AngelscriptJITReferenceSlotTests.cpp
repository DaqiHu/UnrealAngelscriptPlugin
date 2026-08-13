#include "StaticJIT/AngelscriptJITReferenceResolver.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITReferenceSlotTests_Private
{
	static FAngelscriptHash256 MakeHash(const TCHAR* Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("jit-reference-slot-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static FAngelscriptJITReferenceSlot MakeSlot(
		const uint32 SlotIndex,
		const FAngelscriptArtifactReference& Reference)
	{
		FAngelscriptJITReferenceSlot Slot;
		Slot.SlotIndex = SlotIndex;
		Slot.Reference = Reference;
		return Slot;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITReferenceSlotTests,
	"Angelscript.TestModule.StaticJIT.ReferenceSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(StableDescriptorKindsResolveIntoOrderedImmutableSlots)
	{
		using namespace AngelscriptJITReferenceSlotTests_Private;
		const int EngineIdentity = 1;
		int Values[6] = {};
		const FAngelscriptHash256 Abi = MakeHash(TEXT("abi"));
		TArray<FAngelscriptArtifactReference> References;
		References.Add(FAngelscriptJITReferenceDescriptorBuilder::ForFunction(
			FAngelscriptStableFunctionKey{MakeHash(TEXT("function"))}, Abi));
		References.Add(FAngelscriptJITReferenceDescriptorBuilder::ForType(
			FAngelscriptStableTypeKey{MakeHash(TEXT("type"))}, Abi));
		References.Add(FAngelscriptJITReferenceDescriptorBuilder::ForGlobal(
			FAngelscriptStableGlobalKey{MakeHash(TEXT("global"))}, Abi));
		References.Add(FAngelscriptJITReferenceDescriptorBuilder::ForImport(
			MakeHash(TEXT("import")), Abi));
		References.Add(
			FAngelscriptJITReferenceDescriptorBuilder::ForStringLiteral(
				TEXT("Hello, provider"), Abi));
		References.Add(
			FAngelscriptJITReferenceDescriptorBuilder::ForRuntimeHelper(
				TEXT("FScriptExecution::Call"), Abi));

		FAngelscriptJITReferenceIndexBuilder Index(&EngineIdentity);
		TArray<FAngelscriptJITReferenceSlot> Slots;
		for (int32 IndexValue = 0; IndexValue < References.Num(); ++IndexValue)
		{
			ASSERT_THAT(IsTrue(Index.Add(References[IndexValue], &Values[IndexValue])));
			Slots.Add(MakeSlot(IndexValue, References[IndexValue]));
		}
		const TSharedRef<const FAngelscriptJITReferenceResolver,
			ESPMode::ThreadSafe> Resolver = Index.Build();
		const FAngelscriptJITReferenceTableBuildResult Result =
			FAngelscriptJITResolvedReferenceTableBuilder::Build(Slots, *Resolver);

		ASSERT_THAT(IsTrue(Result.IsExact()));
		ASSERT_THAT(AreEqual(6, Result.Table->Slots.Num()));
		for (int32 SlotIndex = 0; SlotIndex < Result.Table->Slots.Num(); ++SlotIndex)
		{
			ASSERT_THAT(AreEqual(SlotIndex,
				static_cast<int32>(Result.Table->Slots[SlotIndex].SlotIndex)));
			ASSERT_THAT(IsTrue(Result.Table->Slots[SlotIndex].Value
				== &Values[SlotIndex]));
		}
		ASSERT_THAT(IsTrue(References.Last().Kind
			== EAngelscriptArtifactReferenceKind::RuntimeHelper));
	}

	TEST_METHOD(MissingAmbiguousWrongKindAndAbiMismatchAreDistinct)
	{
		using namespace AngelscriptJITReferenceSlotTests_Private;
		const int EngineIdentity = 1;
		int FirstValue = 1;
		int SecondValue = 2;
		const FAngelscriptHash256 StableKey = MakeHash(TEXT("shared-key"));
		const FAngelscriptHash256 Abi = MakeHash(TEXT("abi"));
		const FAngelscriptArtifactReference Function =
			FAngelscriptJITReferenceDescriptorBuilder::ForFunction(
				FAngelscriptStableFunctionKey{StableKey}, Abi);

		FAngelscriptJITReferenceIndexBuilder EmptyBuilder(&EngineIdentity);
		TSharedRef<const FAngelscriptJITReferenceResolver,
			ESPMode::ThreadSafe> Resolver = EmptyBuilder.Build();
		ASSERT_THAT(IsTrue(Resolver->Resolve(Function).Code
			== EAngelscriptJITReferenceResolutionCode::Missing));

		FAngelscriptJITReferenceIndexBuilder WrongKindBuilder(&EngineIdentity);
		ASSERT_THAT(IsTrue(WrongKindBuilder.Add(
			FAngelscriptJITReferenceDescriptorBuilder::ForType(
				FAngelscriptStableTypeKey{StableKey}, Abi), &FirstValue)));
		Resolver = WrongKindBuilder.Build();
		ASSERT_THAT(IsTrue(Resolver->Resolve(Function).Code
			== EAngelscriptJITReferenceResolutionCode::WrongKind));

		FAngelscriptJITReferenceIndexBuilder AbiBuilder(&EngineIdentity);
		FAngelscriptArtifactReference WrongAbi = Function;
		WrongAbi.ExpectedAbi = MakeHash(TEXT("other-abi"));
		ASSERT_THAT(IsTrue(AbiBuilder.Add(WrongAbi, &FirstValue)));
		Resolver = AbiBuilder.Build();
		ASSERT_THAT(IsTrue(Resolver->Resolve(Function).Code
			== EAngelscriptJITReferenceResolutionCode::AbiMismatch));

		FAngelscriptJITReferenceIndexBuilder AmbiguousBuilder(&EngineIdentity);
		ASSERT_THAT(IsTrue(AmbiguousBuilder.Add(Function, &FirstValue)));
		ASSERT_THAT(IsTrue(AmbiguousBuilder.Add(Function, &SecondValue)));
		Resolver = AmbiguousBuilder.Build();
		ASSERT_THAT(IsTrue(Resolver->Resolve(Function).Code
			== EAngelscriptJITReferenceResolutionCode::Ambiguous));
	}

	TEST_METHOD(ProviderRefreshAndModuleDiscardDoNotMutateOldTables)
	{
		using namespace AngelscriptJITReferenceSlotTests_Private;
		const int EngineIdentity = 1;
		int OldFunction = 1;
		int NewFunction = 2;
		const FAngelscriptArtifactReference Function =
			FAngelscriptJITReferenceDescriptorBuilder::ForFunction(
				FAngelscriptStableFunctionKey{MakeHash(TEXT("function"))},
				MakeHash(TEXT("abi")));
		const TArray<FAngelscriptJITReferenceSlot> Slots = {
			MakeSlot(0, Function)};

		FAngelscriptJITReferenceIndexBuilder OldBuilder(&EngineIdentity);
		ASSERT_THAT(IsTrue(OldBuilder.Add(Function, &OldFunction)));
		const TSharedRef<const FAngelscriptJITReferenceResolver,
			ESPMode::ThreadSafe> OldResolver = OldBuilder.Build();
		const FAngelscriptJITReferenceTableBuildResult OldResult =
			FAngelscriptJITResolvedReferenceTableBuilder::Build(
				Slots, *OldResolver);
		ASSERT_THAT(IsTrue(OldResult.IsExact()));

		FAngelscriptJITReferenceIndexBuilder DiscardedBuilder(&EngineIdentity);
		const TSharedRef<const FAngelscriptJITReferenceResolver,
			ESPMode::ThreadSafe> DiscardedResolver = DiscardedBuilder.Build();
		ASSERT_THAT(IsTrue(
			FAngelscriptJITResolvedReferenceTableBuilder::Build(
				Slots, *DiscardedResolver).Result
				== EAngelscriptArtifactMatchResult::MissingReference));

		FAngelscriptJITReferenceIndexBuilder NewBuilder(&EngineIdentity);
		ASSERT_THAT(IsTrue(NewBuilder.Add(Function, &NewFunction)));
		const TSharedRef<const FAngelscriptJITReferenceResolver,
			ESPMode::ThreadSafe> NewResolver = NewBuilder.Build();
		const FAngelscriptJITReferenceTableBuildResult NewResult =
			FAngelscriptJITResolvedReferenceTableBuilder::Build(
				Slots, *NewResolver);
		ASSERT_THAT(IsTrue(NewResult.IsExact()));
		ASSERT_THAT(IsTrue(OldResult.Table->Slots[0].Value == &OldFunction));
		ASSERT_THAT(IsTrue(NewResult.Table->Slots[0].Value == &NewFunction));
	}

	TEST_METHOD(TwoEnginesResolveSameDescriptorsToSeparateLocalValues)
	{
		using namespace AngelscriptJITReferenceSlotTests_Private;
		const int EngineAIdentity = 1;
		const int EngineBIdentity = 2;
		int EngineAValues[2] = {};
		int EngineBValues[2] = {};
		const FAngelscriptHash256 Abi = MakeHash(TEXT("abi"));
		const FAngelscriptArtifactReference Function =
			FAngelscriptJITReferenceDescriptorBuilder::ForFunction(
				FAngelscriptStableFunctionKey{MakeHash(TEXT("function"))}, Abi);
		const FAngelscriptArtifactReference Type =
			FAngelscriptJITReferenceDescriptorBuilder::ForType(
				FAngelscriptStableTypeKey{MakeHash(TEXT("type"))}, Abi);
		const TArray<FAngelscriptJITReferenceSlot> Slots = {
			MakeSlot(0, Function), MakeSlot(1, Type)};

		FAngelscriptJITReferenceIndexBuilder BuilderA(&EngineAIdentity);
		ASSERT_THAT(IsTrue(BuilderA.Add(Function, &EngineAValues[0])));
		ASSERT_THAT(IsTrue(BuilderA.Add(Type, &EngineAValues[1])));
		FAngelscriptJITReferenceIndexBuilder BuilderB(&EngineBIdentity);
		ASSERT_THAT(IsTrue(BuilderB.Add(Type, &EngineBValues[1])));
		ASSERT_THAT(IsTrue(BuilderB.Add(Function, &EngineBValues[0])));

		const TSharedRef<const FAngelscriptJITReferenceResolver,
			ESPMode::ThreadSafe> ResolverA = BuilderA.Build();
		const TSharedRef<const FAngelscriptJITReferenceResolver,
			ESPMode::ThreadSafe> ResolverB = BuilderB.Build();
		const FAngelscriptJITReferenceTableBuildResult ResultA =
			FAngelscriptJITResolvedReferenceTableBuilder::Build(Slots, *ResolverA);
		const FAngelscriptJITReferenceTableBuildResult ResultB =
			FAngelscriptJITResolvedReferenceTableBuilder::Build(Slots, *ResolverB);

		ASSERT_THAT(IsTrue(ResultA.IsExact()));
		ASSERT_THAT(IsTrue(ResultB.IsExact()));
		ASSERT_THAT(IsTrue(ResolverA->GetEngineIdentity() !=
			ResolverB->GetEngineIdentity()));
		ASSERT_THAT(IsTrue(ResultA.Table->Slots[0].Value
			== &EngineAValues[0]));
		ASSERT_THAT(IsTrue(ResultB.Table->Slots[0].Value
			== &EngineBValues[0]));
		ASSERT_THAT(IsTrue(ResultA.Table->Slots[0].Value
			!= ResultB.Table->Slots[0].Value));
	}

	TEST_METHOD(ScriptFunctionSlotRetainsCurrentEngineObjectAcrossModuleDiscard)
	{
		using namespace AngelscriptJITReferenceSlotTests_Private;
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FAngelscriptArtifactReference FunctionReference =
			FAngelscriptJITReferenceDescriptorBuilder::ForFunction(
				FAngelscriptStableFunctionKey{MakeHash(TEXT("retained-function"))},
				MakeHash(TEXT("retained-function-abi")));
		TSharedPtr<const FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe> RetainedTable;
		asIScriptFunction* Function = nullptr;
		{
			const std::string Source = ASTEST_AS_ANSI(R"AS(
				int Retained()
				{
					return 91;
				}
				)AS");
			FScopedNativeModule Module(
				*TestRunner, Engine, "JitRetainedReference", Source);
			ASSERT_THAT(IsTrue(Module.IsValid()));
			Function = GetNativeFunctionByDecl(Module.Get(), "int Retained()");
			ASSERT_THAT(IsNotNull(Function));
			if (Function == nullptr)
			{
				Engine.Destroy();
				return;
			}

			FAngelscriptJITReferenceIndexBuilder Builder(ScriptEngine);
			ASSERT_THAT(IsTrue(Builder.AddScriptFunction(
				FunctionReference, Function)));
			const TArray<FAngelscriptJITReferenceSlot> Slots = {
				MakeSlot(0, FunctionReference)};
			const FAngelscriptJITReferenceTableBuildResult Result =
				FAngelscriptJITResolvedReferenceTableBuilder::Build(
					Slots, *Builder.Build());
			ASSERT_THAT(IsTrue(Result.IsExact()));
			RetainedTable = Result.Table;
		}

		ASSERT_THAT(IsTrue(RetainedTable.IsValid()));
		ASSERT_THAT(IsTrue(RetainedTable->Slots[0].Value == Function));
		ASSERT_THAT(AreEqual(
			FString(TEXT("int Retained()")),
			FString(UTF8_TO_TCHAR(Function->GetDeclaration()))));
		RetainedTable.Reset();
		Engine.Destroy();
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
