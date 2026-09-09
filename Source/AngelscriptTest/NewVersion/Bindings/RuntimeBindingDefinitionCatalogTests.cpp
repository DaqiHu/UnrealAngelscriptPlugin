#include "CQTest.h"
#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "Async/Async.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(DefinitionCatalog, "Angelscript.UnitTest.RuntimeBindings.Values",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExplicitValue { int32 Value = 7; };
	static const FAngelscriptTypeBindInfo* Find(const FAngelscriptTypeBindInfoStore& Store, const TCHAR* Name)
	{
		return Store.GetType(Store.FindType(Name));
	}
	static UE::Angelscript::Private::FAngelscriptBindCollection CollectionWith(
		TFunctionRef<void(UE::Angelscript::Private::FAngelscriptBindCollection&)> Fill)
	{
		UE::Angelscript::Private::FAngelscriptBindCollection Collection; Fill(Collection); FString Diagnostic;
		check(Collection.Finalize(Diagnostic)); return Collection;
	}
public:
	TEST_METHOD(PrimitiveDefinitionsPreserveWidthsAliasesAndFloatPolicy)
	{
		auto Collection = CollectionWith([](auto&) {}); FAngelscriptTypeBindInfoStore Store; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoCatalog::RecordRuntimeTypeDeclarations(Collection, Store, Diagnostic)));
		auto Primitive = [&](const TCHAR* Name) { return Store.GetPrimitiveDefinitions().FindByPredicate([Name](const auto& D) { return D.Name == Name; }); };
		ASSERT_THAT(AreEqual(uint32(sizeof(bool)), Primitive(TEXT("bool"))->NativeSize));
		ASSERT_THAT(AreEqual(uint32(sizeof(int8)), Primitive(TEXT("int8"))->NativeSize));
		ASSERT_THAT(AreEqual(uint32(sizeof(int32)), Primitive(TEXT("int"))->NativeSize));
		ASSERT_THAT(IsTrue(Primitive(TEXT("int"))->Aliases.Contains(TEXT("int32"))));
		ASSERT_THAT(AreEqual(uint32(sizeof(uint64)), Primitive(TEXT("uint64"))->NativeSize));
		ASSERT_THAT(AreEqual(uint32(sizeof(float)), Primitive(TEXT("float32"))->NativeSize));
		ASSERT_THAT(AreEqual(uint32(sizeof(double)), Primitive(TEXT("float"))->NativeSize));
	}

	TEST_METHOD(ActualTypeDeclarationProvidersProduceRealCatalogWithoutEngine)
	{
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine())); FAngelscriptTypeBindInfoStore Store; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)));
		const bool bRecorded = FAngelscriptTypeBindInfoCatalog::RecordRuntimeTypeDeclarations(FAngelscriptBind::GetRegisteredCollection(), Store, Diagnostic);
		ASSERT_THAT(AreEqual(FString(), Diagnostic)); ASSERT_THAT(IsTrue(bRecorded));
		ASSERT_THAT(IsNull(FAngelscriptEngine::TryGetCurrentEngine()));
		ASSERT_THAT(IsNotNull(Find(Store, TEXT("FString")))); ASSERT_THAT(IsNotNull(Find(Store, TEXT("FText"))));
		ASSERT_THAT(IsNotNull(Find(Store, TEXT("FVector")))); ASSERT_THAT(IsNotNull(Find(Store, TEXT("TArray"))));
		ASSERT_THAT(IsNotNull(Find(Store, TEXT("FAngelscriptGameThreadScopeWorldContext"))));
		ASSERT_THAT(IsNotNull(Find(Store, TEXT("FScopedMovementUpdate"))));
		for (const auto& Type : Store.GetTypes())
		{
			if (Type.Declaration.UnrealType != nullptr || Type.Declaration.Kind == EAngelscriptTypeBindKind::Enum
				|| Type.Declaration.Kind == EAngelscriptTypeBindKind::Namespace) continue;
			ASSERT_THAT(IsTrue(Type.Declaration.NativeSize > 0)); ASSERT_THAT(IsTrue(Type.Declaration.NativeAlignment > 0));
			ASSERT_THAT(IsFalse(Type.Declaration.NativeOwner.IsEmpty()));
			if (Type.Declaration.Kind != EAngelscriptTypeBindKind::Template
				&& (Type.Declaration.ObjectFlags & asOBJ_POD) == 0)
			{
				ASSERT_THAT(IsTrue(Type.Declaration.DefaultConstruct != nullptr
					|| Type.Declaration.CopyConstruct != nullptr || Type.Declaration.Destroy != nullptr));
			}
		}
	}

	TEST_METHOD(DeclarationOnlyPrepassRecordsNoMethodSurface)
	{
		auto Collection = CollectionWith([](auto& Value)
		{
			FAngelscriptBindRecord Record; Record.OwnerModule = TEXT("Test"); Record.BindName = TEXT("Catalog.DeclarationOnly");
			Record.Phase = EAngelscriptBindPhase::TypeDeclarations; Record.SourceFile = __FILE__; Record.SourceLine = __LINE__;
			Record.Callback = [](FAngelscriptBinds& Binds) { auto Type = Binds.ValueClassForTarget<FString>("FString", {}); Type.Method("int Len() const", &FString::Len); };
			FString Diagnostic; check(Value.Append(Record, Diagnostic));
		});
		FAngelscriptTypeBindInfoStore Store; FString Diagnostic;
		ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoCatalog::RecordRuntimeTypeDeclarations(Collection, Store, Diagnostic)));
		ASSERT_THAT(IsNotNull(Find(Store, TEXT("FString")))); ASSERT_THAT(IsTrue(Find(Store, TEXT("FString"))->Members.IsEmpty()));
	}

	TEST_METHOD(StringAndTextLifecycleRecipesCopyAndDestroyRealValues)
	{
		auto Collection = CollectionWith([](auto& Value)
		{
			FAngelscriptBindRecord Record; Record.OwnerModule = TEXT("Test"); Record.BindName = TEXT("Catalog.StringText");
			Record.Phase = EAngelscriptBindPhase::TypeDeclarations; Record.SourceFile = __FILE__; Record.SourceLine = __LINE__;
			Record.Callback = [](FAngelscriptBinds& Binds) { Binds.ValueClassForTarget<FString>("FString", {}); Binds.ValueClassForTarget<FText>("FText", {}); };
			FString Diagnostic; check(Value.Append(Record, Diagnostic));
		});
		FAngelscriptTypeBindInfoStore Store; FString Diagnostic; ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoCatalog::RecordRuntimeTypeDeclarations(Collection, Store, Diagnostic)));
		auto Exercise = [&](const FAngelscriptTypeBindDeclaration& D, auto Initial, auto Changed)
		{
			void* A = FMemory::Malloc(D.NativeSize, D.NativeAlignment); void* B = FMemory::Malloc(D.NativeSize, D.NativeAlignment);
			D.DefaultConstruct(A); *static_cast<decltype(Initial)*>(A) = Initial; D.CopyConstruct(B, A); *static_cast<decltype(Initial)*>(A) = Changed;
			const auto Copy = *static_cast<decltype(Initial)*>(B); D.Destroy(A); D.Destroy(B); FMemory::Free(A); FMemory::Free(B); return Copy;
		};
		const auto& String = Find(Store, TEXT("FString"))->Declaration; const auto& Text = Find(Store, TEXT("FText"))->Declaration;
		ASSERT_THAT(AreEqual(FString(TEXT("initial")), Exercise(String, FString(TEXT("initial")), FString(TEXT("changed")))));
		ASSERT_THAT(IsTrue(Exercise(Text, FText::FromString(TEXT("initial")), FText::FromString(TEXT("changed"))).EqualTo(FText::FromString(TEXT("initial")))));
	}

	TEST_METHOD(IncompatibleDuplicateNativeLayoutFailsAtItsProvider)
	{
		auto Collection = CollectionWith([](auto& Value)
		{
			for (int32 Index = 0; Index != 2; ++Index) { FAngelscriptBindRecord Record; Record.OwnerModule = TEXT("Test"); Record.BindName = Index ? TEXT("Catalog.B") : TEXT("Catalog.A");
				Record.Phase = EAngelscriptBindPhase::TypeDeclarations; Record.SourceFile = __FILE__; Record.SourceLine = __LINE__ + Index;
				Record.Callback = Index ? FAngelscriptBindCallback(+[](FAngelscriptBinds& B) { B.ValueClassForTarget("Conflict", 16, {}); })
					: FAngelscriptBindCallback(+[](FAngelscriptBinds& B) { B.ValueClassForTarget("Conflict", 8, {}); }); FString D; check(Value.Append(Record, D)); }
		});
		FAngelscriptTypeBindInfoStore Store; FString Diagnostic;
		ASSERT_THAT(IsFalse(FAngelscriptTypeBindInfoCatalog::RecordRuntimeTypeDeclarations(Collection, Store, Diagnostic)));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Conflict")))); ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("Catalog"))));
	}

	TEST_METHOD(ExplicitBindingProviderCanDeclareWithoutRecordingItsMembers)
	{
		auto Collection = CollectionWith([](auto& Value)
		{
			FAngelscriptBindRecord Record; Record.OwnerModule = TEXT("Test"); Record.BindName = TEXT("Catalog.Explicit");
			Record.Phase = EAngelscriptBindPhase::ExplicitBindings; Record.bContainsTypeDeclarations = true;
			Record.SourceFile = __FILE__; Record.SourceLine = __LINE__; Record.Callback = [](FAngelscriptBinds& Binds)
			{ auto Type = Binds.ValueClassForTarget<FExplicitValue>("FExplicitValue", {}); Type.Property("int Value", &FExplicitValue::Value); };
			FString Diagnostic; check(Value.Append(Record, Diagnostic));
		});
		FAngelscriptTypeBindInfoStore Store; FString Diagnostic; ASSERT_THAT(IsTrue(FAngelscriptTypeBindInfoCatalog::RecordRuntimeTypeDeclarations(Collection, Store, Diagnostic)));
		const auto* Type = Find(Store, TEXT("FExplicitValue")); ASSERT_THAT(IsNotNull(Type)); ASSERT_THAT(IsTrue(Type->Members.IsEmpty()));
		ASSERT_THAT(AreEqual(FName(TEXT("Catalog.Explicit")), Type->Declaration.Source.Provider));
	}

	TEST_METHOD(SealedStoreAndWorkerThreadAreRejectedBeforeProviderExecution)
	{
		auto Collection = CollectionWith([](auto&) {}); FAngelscriptTypeBindInfoStore Store; FString Diagnostic; ASSERT_THAT(IsTrue(Store.Seal(Diagnostic)));
		ASSERT_THAT(IsFalse(FAngelscriptTypeBindInfoCatalog::RecordRuntimeTypeDeclarations(Collection, Store, Diagnostic)));
		ASSERT_THAT(IsTrue(Diagnostic.Contains(TEXT("writable"))));
		FAngelscriptTypeBindInfoStore WorkerStore;
		const bool bWorkerRejected = Async(EAsyncExecution::ThreadPool, [&]
		{
			FString WorkerDiagnostic;
			return !FAngelscriptTypeBindInfoCatalog::RecordRuntimeTypeDeclarations(Collection, WorkerStore, WorkerDiagnostic)
				&& WorkerDiagnostic.Contains(TEXT("GameThread"));
		}).Get();
		ASSERT_THAT(IsTrue(bWorkerRejected)); ASSERT_THAT(IsTrue(WorkerStore.GetTypes().IsEmpty()));
	}
};
#endif
