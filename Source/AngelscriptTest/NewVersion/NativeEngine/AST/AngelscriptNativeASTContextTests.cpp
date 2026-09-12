#include "CQTest.h"

#include "frontend/AST/as_ast_context.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_ast_verifier.h"

#include "Async/Async.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace TypedASTContextTests
{
	static TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot()
	{
		auto Result = MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
		const uint8 Bytes[] = {'c', 'l', 'a', 's', 's', ' ', 'A'};
		Result->AddFile(TEXT("A.as"), MakeArrayView(Bytes));
		Result->Freeze();
		return Result;
	}

	static AS_NAMESPACE_QUALIFIER asCSourceRange Range(const AS_NAMESPACE_QUALIFIER asCSourceSnapshot& Snapshot, asUINT Begin, asUINT End)
	{
		return {{Snapshot.GetSnapshotID(), AS_NAMESPACE_QUALIFIER asCSourceFileID(1), Begin}, {Snapshot.GetSnapshotID(), AS_NAMESPACE_QUALIFIER asCSourceFileID(1), End}};
	}

	TEST_CLASS_WITH_FLAGS(
		ASTContext,
		"Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(ContextOwnsStableNodeAddresses)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* First = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(7, Range(*Source, 0, 1));
			for (int32 Index = 0; Index < 128; ++Index)
				Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Index, Range(*Source, 1, 2));
			ASSERT_THAT(IsTrue(Context.Owns(First)));
			ASSERT_THAT(AreEqual(static_cast<int64>(7), First->GetValue()));
			ASSERT_THAT(AreEqual(129, Context.GetNodeCount()));
		}

		TEST_METHOD(CanonicalBuiltinTypesAreUniquedPerContext)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* First = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			auto* Second = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			auto* Other = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Bool);
			ASSERT_THAT(IsTrue(First == Second));
			ASSERT_THAT(IsFalse(First == Other));
		}

		TEST_METHOD(CanonicalNominalGenericAndCallableTypesAreUniquedPerContext)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto& Registry = Context.GetTypeContext()->GetIdentityRegistry();
			auto* Int = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			AS_NAMESPACE_QUALIFIER asCQualType IntUse;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(
				Int, AS_NAMESPACE_QUALIFIER asETypeQualifiers::None, IntUse)));

			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor NominalDescriptor;
			NominalDescriptor.StableScope = TEXT("ast-context");
			NominalDescriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
			NominalDescriptor.Name = TEXT("Box");
			NominalDescriptor.GenericArity = 1;
			AS_NAMESPACE_QUALIFIER asSStableKey DeclarationKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(NominalDescriptor, DeclarationKey)));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor TemplateDescriptor;
			TemplateDescriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Nominal;
			TemplateDescriptor.Declaration = DeclarationKey;
			AS_NAMESPACE_QUALIFIER asSStableKey TemplateKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(TemplateDescriptor, TemplateKey)));
			auto* TemplateA = Context.GetNominalType(DeclarationKey, TemplateKey);
			auto* TemplateB = Context.GetNominalType(DeclarationKey, TemplateKey);
			ASSERT_THAT(IsTrue(TemplateA == TemplateB));

			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor GenericDescriptor = TemplateDescriptor;
			GenericDescriptor.Arguments = {Int->GetStableIdentity()};
			AS_NAMESPACE_QUALIFIER asSStableKey GenericKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(GenericDescriptor, GenericKey)));
			auto* GenericA = Context.GetGenericType(
				TemplateA, TArray<AS_NAMESPACE_QUALIFIER asCQualType>{IntUse}, GenericKey);
			auto* GenericB = Context.GetGenericType(
				TemplateA, TArray<AS_NAMESPACE_QUALIFIER asCQualType>{IntUse}, GenericKey);
			ASSERT_THAT(IsTrue(GenericA == GenericB));

			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor FunctionDescriptor;
			FunctionDescriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Callable;
			FunctionDescriptor.ReturnType = Int->GetStableIdentity();
			FunctionDescriptor.Parameters = {{Int->GetStableIdentity(), AS_NAMESPACE_QUALIFIER asEParameterPassingMode::In}};
			AS_NAMESPACE_QUALIFIER asSStableKey FunctionKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(FunctionDescriptor, FunctionKey)));
			auto* FunctionA = Context.GetFunctionType(
				IntUse, TArray<AS_NAMESPACE_QUALIFIER asCQualType>{IntUse}, FunctionKey,
				TArray<AS_NAMESPACE_QUALIFIER asEParameterPassingMode>{AS_NAMESPACE_QUALIFIER asEParameterPassingMode::In});
			auto* FunctionB = Context.GetFunctionType(
				IntUse, TArray<AS_NAMESPACE_QUALIFIER asCQualType>{IntUse}, FunctionKey,
				TArray<AS_NAMESPACE_QUALIFIER asEParameterPassingMode>{AS_NAMESPACE_QUALIFIER asEParameterPassingMode::In});
			ASSERT_THAT(IsTrue(FunctionA == FunctionB));

			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor OtherDescriptor = NominalDescriptor;
			OtherDescriptor.Name = TEXT("Other");
			AS_NAMESPACE_QUALIFIER asSStableKey OtherDeclaration;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(OtherDescriptor, OtherDeclaration)));
			ASSERT_THAT(IsTrue(Context.GetNominalType(OtherDeclaration, TemplateKey) == nullptr));
			ASSERT_THAT(IsTrue(Context.GetGenericType(
				TemplateA, TArray<AS_NAMESPACE_QUALIFIER asCQualType>{}, GenericKey) == nullptr));
			ASSERT_THAT(IsTrue(Context.GetFunctionType(
				IntUse, TArray<AS_NAMESPACE_QUALIFIER asCQualType>{IntUse}, FunctionKey) == nullptr));
			ASSERT_THAT(AreEqual(4, Context.GetNodeCount()));
		}

		TEST_METHOD(HandlesAreOwnerChecked)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext First(Source);
			AS_NAMESPACE_QUALIFIER asCASTContext Second(Source);
			auto* FirstNode = First.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(1, Range(*Source, 0, 1));
			auto* SecondNode = Second.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(2, Range(*Source, 0, 1));
			const AS_NAMESPACE_QUALIFIER asCASTNodeHandle FirstHandle = First.GetHandle(FirstNode);
			const AS_NAMESPACE_QUALIFIER asCASTNodeHandle SecondHandle = Second.GetHandle(SecondNode);
			ASSERT_THAT(AreEqual(FirstHandle.LocalID, SecondHandle.LocalID));
			ASSERT_THAT(IsFalse(FirstHandle.OwnerID == SecondHandle.OwnerID));
			ASSERT_THAT(IsTrue(First.Resolve(FirstHandle) == FirstNode));
			ASSERT_THAT(IsTrue(Second.Resolve(FirstHandle) == nullptr));
		}

		TEST_METHOD(CastsAreNullAwareAndFamilyChecked)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Literal = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(1, Range(*Source, 0, 1));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCExpr>(Literal) == Literal));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDecl>(Literal) == nullptr));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCExpr>(static_cast<AS_NAMESPACE_QUALIFIER asCStmt*>(nullptr)) == nullptr));
		}

		TEST_METHOD(SealIsOneWayAndRejectsMutation)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Root = Context.Create<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Range(*Source, 0, 7));
			auto* Before = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(1, Range(*Source, 1, 2));
			auto* After = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(2, Range(*Source, 3, 4));
			ASSERT_THAT(IsTrue(Before->SetExpressionSemantics(
				Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32),
				AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsTrue(After->SetExpressionSemantics(
				Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32),
				AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsTrue(Root->AddStatement(Before)));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verification;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Root, Verification)));
			ASSERT_THAT(IsTrue(Context.IsSealed()));
			ASSERT_THAT(IsFalse(Root->AddStatement(After)));
			ASSERT_THAT(IsTrue(Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(2, Range(*Source, 0, 1)) == nullptr));
			ASSERT_THAT(AreEqual(1, Root->GetStatements().Num()));
		}

		TEST_METHOD(IndependentWorkersShareSynchronizedAllocationAndBuiltinUniquing)
		{
			struct FWorkerResult
			{
				AS_NAMESPACE_QUALIFIER asCCompoundStmt* Fragment = nullptr;
				AS_NAMESPACE_QUALIFIER asCBuiltinType* SharedType = nullptr;
			};

			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Root = Context.Create<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Range(*Source, 0, 7));
			TArray<TFuture<FWorkerResult>> Workers;
			for (int32 WorkerIndex = 0; WorkerIndex < 8; ++WorkerIndex)
			{
				Workers.Add(Async(EAsyncExecution::ThreadPool, [&Context, Source, WorkerIndex]()
				{
					FWorkerResult Result;
					Result.SharedType = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
					Result.Fragment = Context.Create<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Range(*Source, 0, 7));
					auto* Literal = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(WorkerIndex, Range(*Source, 1, 2));
					Literal->SetExpressionSemantics(Result.SharedType, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue);
					Result.Fragment->AddStatement(Literal);
					return Result;
				}));
			}

			AS_NAMESPACE_QUALIFIER asCBuiltinType* CanonicalType = nullptr;
			for (TFuture<FWorkerResult>& Worker : Workers)
			{
				const FWorkerResult Result = Worker.Get();
				if (!CanonicalType) CanonicalType = Result.SharedType;
				ASSERT_THAT(IsTrue(Result.SharedType == CanonicalType));
				ASSERT_THAT(IsTrue(Root->AddStatement(Result.Fragment)));
			}
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verification;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Root, Verification)));
			ASSERT_THAT(AreEqual(18, Context.GetNodeCount()));
		}

		TEST_METHOD(ContextOwnsTheSourceSnapshotLease)
		{
			TSharedPtr<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Source = Snapshot();
			TWeakPtr<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Weak = Source;
			{
				AS_NAMESPACE_QUALIFIER asCASTContext Context(Source.ToSharedRef());
				Source.Reset();
				ASSERT_THAT(IsTrue(Weak.IsValid()));
			}
			ASSERT_THAT(IsFalse(Weak.IsValid()));
		}

		TEST_METHOD(ContextRetainsItsExplicitCanonicalOwner)
		{
			TSharedPtr<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Owner = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
			TWeakPtr<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Weak = Owner;
			{
				AS_NAMESPACE_QUALIFIER asCASTContext Context(Snapshot(), Owner.ToSharedRef());
				Owner.Reset();
				ASSERT_THAT(IsTrue(Weak.IsValid()));
				auto* Type = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt64);
				ASSERT_THAT(IsNotNull(Type));
				AS_NAMESPACE_QUALIFIER asEStableIdentityRole Role;
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
					Context.GetTypeContext()->GetIdentityRegistry().ResolveIdentityRole(Type->GetStableIdentity(), Role)));
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEStableIdentityRole::TypeUse, Role));
			}
			ASSERT_THAT(IsFalse(Weak.IsValid()));
		}

		TEST_METHOD(SharedTypeContextSharesIdentityButNotASTNodeAddresses)
		{
			auto Owner = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
			AS_NAMESPACE_QUALIFIER asCASTContext First(Snapshot(), Owner);
			AS_NAMESPACE_QUALIFIER asCASTContext Second(Snapshot(), Owner);
			auto* A = First.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			auto* B = Second.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			ASSERT_THAT(IsTrue(A != B));
			ASSERT_THAT(IsTrue(A->GetStableIdentity() == B->GetStableIdentity()));
			ASSERT_THAT(IsTrue(First.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32) == A));
			ASSERT_THAT(IsFalse(Second.Owns(A)));
			ASSERT_THAT(AreEqual(asUINT(1), Owner->GetIdentityRegistry().GetIdentityCount()));
		}

		TEST_METHOD(KnownKeyCannotDisguiseAnotherBuiltinPrimitive)
		{
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Snapshot());
			auto* Actual = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt64);
			auto* Mismatch = Context.Create<AS_NAMESPACE_QUALIFIER asCBuiltinType>(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, Actual->GetStableIdentity());
			ASSERT_THAT(IsNotNull(Mismatch));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verification;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Mismatch, Verification)));
			ASSERT_THAT(IsFalse(Context.IsSealed()));
			ASSERT_THAT(IsTrue(Mismatch->IsInvalid()));
		}

		TEST_METHOD(NominalUseCannotNameAnotherDeclarationKey)
		{
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Snapshot());
			auto& Registry = Context.GetTypeContext()->GetIdentityRegistry();
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
			Descriptor.StableScope = TEXT("ast"); Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class; Descriptor.Name = TEXT("A");
			AS_NAMESPACE_QUALIFIER asSStableKey A, B, UseB;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.InternNominalType(Descriptor, A)));
			Descriptor.Name = TEXT("B");
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.InternNominalType(Descriptor, B)));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Use;
			Use.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Nominal; Use.Declaration = B;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.InternTypeUse(Use, UseB)));
			auto* Mismatch = Context.Create<AS_NAMESPACE_QUALIFIER asCNominalType>(A, UseB);
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verification;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Mismatch, Verification)));
			ASSERT_THAT(IsFalse(Context.IsSealed()));
		}

		TEST_METHOD(CallableSignatureKeyCannotDisguiseDifferentParameterTypes)
		{
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Snapshot());
			auto* Int = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			auto* Bool = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Bool);
			AS_NAMESPACE_QUALIFIER asCQualType IntUse;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(Int, AS_NAMESPACE_QUALIFIER asETypeQualifiers::None, IntUse)));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Signature;
			Signature.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Callable;
			Signature.ReturnType = Int->GetStableIdentity();
			Signature.Parameters = {{Bool->GetStableIdentity(), AS_NAMESPACE_QUALIFIER asEParameterPassingMode::Value}};
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.GetTypeContext()->GetIdentityRegistry().InternTypeUse(Signature, Key)));
			auto* Mismatch = Context.Create<AS_NAMESPACE_QUALIFIER asCFunctionType>(IntUse, TArray<AS_NAMESPACE_QUALIFIER asCQualType>{IntUse}, Key);
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verification;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Mismatch, Verification)));
			ASSERT_THAT(IsFalse(Context.IsSealed()));
		}

		TEST_METHOD(CallableSignatureKeyCannotDisguiseParameterPassingMode)
		{
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Snapshot());
			auto& Registry = Context.GetTypeContext()->GetIdentityRegistry();
			auto* Int = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			AS_NAMESPACE_QUALIFIER asCQualType Value, Reference;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(Int, AS_NAMESPACE_QUALIFIER asETypeQualifiers::None, Value)));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(Int, AS_NAMESPACE_QUALIFIER asETypeQualifiers::Reference, Reference)));
			AS_NAMESPACE_QUALIFIER asSStableKey ReferenceKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Reference.GetStableIdentity(Registry, ReferenceKey)));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Signature;
			Signature.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Callable;
			Signature.ReturnType = Int->GetStableIdentity();
			Signature.Parameters = {{ReferenceKey, AS_NAMESPACE_QUALIFIER asEParameterPassingMode::In}};
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.InternTypeUse(Signature, Key)));
			// The three-argument constructor describes Value parameters, not In.
			auto* Mismatch = Context.Create<AS_NAMESPACE_QUALIFIER asCFunctionType>(Value, TArray<AS_NAMESPACE_QUALIFIER asCQualType>{Reference}, Key);
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verification;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Mismatch, Verification)));
			ASSERT_THAT(IsTrue(Mismatch->IsInvalid()));
		}

		TEST_METHOD(SharedRegistryDoesNotAuthorizeBorrowingForeignASTTypeNodes)
		{
			auto Owner = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
			AS_NAMESPACE_QUALIFIER asCASTContext First(Snapshot(), Owner);
			AS_NAMESPACE_QUALIFIER asCASTContext Second(Snapshot(), Owner);
			auto* Foreign = First.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			AS_NAMESPACE_QUALIFIER asCQualType Return;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(Foreign, AS_NAMESPACE_QUALIFIER asETypeQualifiers::None, Return)));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Signature;
			Signature.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Callable;
			Signature.ReturnType = Foreign->GetStableIdentity();
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Owner->GetIdentityRegistry().InternTypeUse(Signature, Key)));
			auto* DanglingRisk = Second.Create<AS_NAMESPACE_QUALIFIER asCFunctionType>(Return, TArray<AS_NAMESPACE_QUALIFIER asCQualType>{}, Key);
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verification;
			ASSERT_THAT(IsFalse(Second.VerifyAndSeal(DanglingRisk, Verification)));
			ASSERT_THAT(IsFalse(Second.IsSealed()));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
