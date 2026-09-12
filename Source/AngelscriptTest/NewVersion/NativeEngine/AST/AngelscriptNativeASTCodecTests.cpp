#include "CQTest.h"
#include "../NativeEngineTestSupport.h"

#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_ast_codec.h"
#include "frontend/AST/as_ast_context.h"
#include "frontend/AST/as_ast_projection.h"
#include "frontend/AST/as_ast_verifier.h"
#include "frontend/AST/as_ast_visitor.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace TypedASTCodecTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	struct FRootWireOffsets
	{
		int32 FirstCrossKind = INDEX_NONE;
		int32 FirstCrossLocalIndex = INDEX_NONE;
		int32 PayloadTag = INDEX_NONE;
	};

	static bool ReadWireU32(TConstArrayView<uint8> Bytes, int32& Position, uint32& Out)
	{
		if (Position < 0 || Position + 4 > Bytes.Num()) return false;
		Out = static_cast<uint32>(Bytes[Position])
			| (static_cast<uint32>(Bytes[Position + 1]) << 8)
			| (static_cast<uint32>(Bytes[Position + 2]) << 16)
			| (static_cast<uint32>(Bytes[Position + 3]) << 24);
		Position += 4;
		return true;
	}

	static bool SkipWireString(TConstArrayView<uint8> Bytes, int32& Position)
	{
		uint32 Length = 0;
		if (!ReadWireU32(Bytes, Position, Length)
			|| Length > static_cast<uint32>(Bytes.Num() - Position)) return false;
		Position += static_cast<int32>(Length);
		return true;
	}

	static bool SkipWireAnchor(TConstArrayView<uint8> Bytes, int32& Position)
	{
		if (!SkipWireString(Bytes, Position) || Position + 16 > Bytes.Num()) return false;
		Position += 16;
		return true;
	}

	static bool LocateRootWireOffsets(
		TConstArrayView<uint8> Bytes,
		FRootWireOffsets& Out)
	{
		Out = {};
		int32 Position = 10;
		if (Position + 3 > Bytes.Num()) return false;
		Position += 3;
		if (!SkipWireAnchor(Bytes, Position)) return false;
		uint32 Count = 0;
		if (!ReadWireU32(Bytes, Position, Count)
			|| Count > static_cast<uint32>((Bytes.Num() - Position) / 4)) return false;
		Position += static_cast<int32>(Count * 4);
		if (!ReadWireU32(Bytes, Position, Count)) return false;
		if (Count > 0)
		{
			Out.FirstCrossKind = Position;
			Out.FirstCrossLocalIndex = Position + 1;
		}
		if (Count > static_cast<uint32>((Bytes.Num() - Position) / 37)) return false;
		Position += static_cast<int32>(Count * 37);
		if (!ReadWireU32(Bytes, Position, Count)
			|| Count > static_cast<uint32>((Bytes.Num() - Position) / 38)) return false;
		Position += static_cast<int32>(Count * 38);
		if (Position >= Bytes.Num()) return false;
		const bool bDeclaration = Bytes[Position++] != 0;
		if (bDeclaration)
		{
			if (Position + 2 > Bytes.Num()) return false;
			++Position;
			const bool bAuthoredAccess = Bytes[Position++] != 0;
			if (bAuthoredAccess)
			{
				if (!SkipWireString(Bytes, Position)) return false;
				for (int32 Anchor = 0; Anchor < 3; ++Anchor)
					if (!SkipWireAnchor(Bytes, Position)) return false;
			}
		}
		if (Position >= Bytes.Num()) return false;
		const bool bExpression = Bytes[Position++] != 0;
		if (bExpression)
		{
			if (Position + static_cast<int32>(AS_NAMESPACE_QUALIFIER asSStableKey::ByteCount) + 2
				> Bytes.Num()) return false;
			Position += static_cast<int32>(AS_NAMESPACE_QUALIFIER asSStableKey::ByteCount) + 2;
		}
		if (Position >= Bytes.Num()) return false;
		Out.PayloadTag = Position;
		return true;
	}

	static void WriteWireU32(TArray<uint8>& Bytes, int32 Position, uint32 Value)
	{
		check(Position >= 0 && Position + 4 <= Bytes.Num());
		for (int32 Byte = 0; Byte < 4; ++Byte)
			Bytes[Position + Byte] = static_cast<uint8>(Value >> (Byte * 8));
	}

	static TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot()
	{
		auto Result = MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
		const uint8 Bytes[] = {'r','e','t','u','r','n',' ','1',';'};
		Result->AddFile(TEXT("Codec.as"), MakeArrayView(Bytes));
		Result->Freeze();
		return Result;
	}

	static AS_NAMESPACE_QUALIFIER asCSourceRange Range(const AS_NAMESPACE_QUALIFIER asCSourceSnapshot& Source, asUINT Begin, asUINT End)
	{
		return {{Source.GetSnapshotID(), AS_NAMESPACE_QUALIFIER asCSourceFileID(1), Begin}, {Source.GetSnapshotID(), AS_NAMESPACE_QUALIFIER asCSourceFileID(1), End}};
	}

	static bool MakeProjection(AS_NAMESPACE_QUALIFIER asCASTProjection& Out)
	{
		auto Source = Snapshot();
		AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
		auto* Compound = Context.Create<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Range(*Source, 0, 9));
		auto* Literal = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(1, Range(*Source, 7, 8));
		if (!Literal->SetExpressionSemantics(
			Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32),
			AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)) return false;
		Compound->AddStatement(Literal);
		AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verify;
		if (!Context.VerifyAndSeal(Compound, Verify)) return false;
		FString Error;
		return AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, Compound, Out, Error);
	}

	static TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> ControlSnapshot()
	{
		auto Result = MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
		const uint8 Bytes[] =
		{
			's','w','i','t','c','h','(','x',')','{','c','a','s','e',' ','1',':',
			'f','a','l','l','t','h','r','o','u','g','h',';','d','e','f','a','u','l','t',
			':','b','r','e','a','k',';','}'
		};
		Result->AddFile(TEXT("CodecControl.as"), MakeArrayView(Bytes));
		Result->Freeze();
		return Result;
	}

	static AS_NAMESPACE_QUALIFIER asCIntegerLiteral* TypedInteger(
		AS_NAMESPACE_QUALIFIER asCASTContext& Context,
		const AS_NAMESPACE_QUALIFIER asCSourceSnapshot& Source,
		int64 Value,
		asUINT Begin,
		asUINT End)
	{
		auto* Literal = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(
			Value, Range(Source, Begin, End));
		if (!Literal->SetExpressionSemantics(
			Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32),
			AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)) return nullptr;
		return Literal;
	}

	TEST_CLASS_WITH_FLAGS(
		ASTCodec,
		"Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(ProjectionAndCodecRoundTripDeterministically)
		{
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			ASSERT_THAT(IsTrue(MakeProjection(Projection)));
			TArray<uint8> First;
			TArray<uint8> Second;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, First)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Second)));
			ASSERT_THAT(IsTrue(First == Second));
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(First, Decoded)));
			ASSERT_THAT(IsTrue(Projection.Equals(Decoded)));
		}

		TEST_METHOD(ProjectionPreservesSourceAnchorsAndOwningOrder)
		{
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			ASSERT_THAT(IsTrue(MakeProjection(Projection)));
			ASSERT_THAT(AreEqual(2, Projection.GetRecords().Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("Codec.as")), Projection.GetRecords()[0].Source.LogicalSourceKey));
			ASSERT_THAT(AreEqual(1, Projection.GetRecords()[0].Children.Num()));
			ASSERT_THAT(AreEqual(static_cast<uint32>(1), Projection.GetRecords()[0].Children[0]));
		}

		TEST_METHOD(TypeProjectionCarriesTypedPayloadAndFixedIdentity)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Type = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verify;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Type, Verify)));
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, Type, Projection, Error)));
			ASSERT_THAT(IsTrue(Projection.GetRecords()[0].StableIdentity == Type->GetStableIdentity()));
			ASSERT_THAT(IsFalse(Projection.GetRecords()[0].StableIdentity.IsZero()));
			const auto* Payload = Projection.GetRecords()[0].TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTBuiltinTypePayload>();
			ASSERT_THAT(IsNotNull(Payload));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, Payload->Primitive));
			TArray<uint8> Bytes;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			ASSERT_THAT(IsTrue(Bytes.Num() > static_cast<int32>(AS_NAMESPACE_QUALIFIER asSStableKey::ByteCount)));
		}

		TEST_METHOD(TypedBuiltinPayloadMustAuthenticateAgainstItsRetainedIdentity)
		{
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Snapshot());
			auto* Type = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verify;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Type, Verify)));
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, Type, Projection, Error)));
			TArray<uint8> Bytes;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));

			// A one-record v6 type projection has declaration/expression presence
			// bytes before its payload tag at byte 47 and primitive at byte 48.
			// Retain the valid Int32 key in the final 32
			// bytes while changing only the structurally-valid typed payload.
			ASSERT_THAT(IsTrue(Bytes.Num() >= 81));
			Bytes[48] = static_cast<uint8>(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Bool);
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(IsFalse(Decoded.ValidateIdentities(
				Context.GetTypeContext()->GetIdentityRegistry(), Error)));
			ASSERT_THAT(IsFalse(Error.IsEmpty()));
		}

		TEST_METHOD(ExpressionPayloadSemanticTypeAndOperatorRangeRoundTrip)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Type = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			auto* Left = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(41, Range(*Source, 0, 1));
			auto* Right = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(1, Range(*Source, 2, 3));
			auto* Add = Context.Create<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(
				AS_NAMESPACE_QUALIFIER asEBinaryOperator::Add, Left, Right,
				Range(*Source, 1, 2), Range(*Source, 0, 3));
			auto* Root = Context.Create<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Range(*Source, 0, 9));
			ASSERT_THAT(IsTrue(Left->SetExpressionSemantics(Type, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsTrue(Right->SetExpressionSemantics(Type, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsTrue(Add->SetExpressionSemantics(Type, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsTrue(Left->SetReadOnly(true)));
			ASSERT_THAT(IsTrue(Root->AddStatement(Add)));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verify;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Root, Verify)));

			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, Root, Projection, Error)));
			ASSERT_THAT(AreEqual(4, Projection.GetRecords().Num()));
			const auto& AddRecord = Projection.GetRecords()[1];
			const auto* AddPayload = AddRecord.TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTBinaryOperatorPayload>();
			ASSERT_THAT(IsNotNull(AddPayload));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBinaryOperator::Add, AddPayload->Operator));
			ASSERT_THAT(AreEqual(static_cast<asUINT>(1), AddPayload->OperatorSource.BeginByteOffset));
			ASSERT_THAT(IsTrue(AddRecord.Expression.bPresent));
			ASSERT_THAT(IsTrue(AddRecord.Expression.TypeIdentity == Type->GetStableIdentity()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue, AddRecord.Expression.ValueCategory));
			const auto* LeftPayload = Projection.GetRecords()[2].TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTIntegerLiteralPayload>();
			ASSERT_THAT(IsNotNull(LeftPayload));
			ASSERT_THAT(AreEqual(static_cast<int64>(41), LeftPayload->Value));
			ASSERT_THAT(IsTrue(Projection.GetRecords()[2].Expression.bReadOnly));
			AS_NAMESPACE_QUALIFIER asSStableKey ReadOnlyTypeIdentity;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.GetTypeContext()->GetIdentityRegistry().InternQualifiedTypeUse(
					Type->GetStableIdentity(), AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const,
					ReadOnlyTypeIdentity)));
			ASSERT_THAT(IsTrue(
				Projection.GetRecords()[2].Expression.TypeIdentity == ReadOnlyTypeIdentity));

			TArray<uint8> Bytes;
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(IsTrue(Projection.Equals(Decoded)));
			ASSERT_THAT(IsTrue(Projection.DumpText().Contains(TEXT("BinaryOperator"))));
			ASSERT_THAT(IsTrue(Projection.DumpText().Contains(TEXT("value=41"))));
			const FString Json = Projection.DumpJson();
			ASSERT_THAT(IsTrue(Json.Contains(TEXT("\"format\":\"typed-ast-v7\""))));
			ASSERT_THAT(IsTrue(Json.Contains(TEXT("\"expression\":{"))));
			ASSERT_THAT(IsTrue(Json.Contains(TEXT("\"semanticReferences\":"))));
			ASSERT_THAT(IsTrue(Json.Contains(TEXT("\"crossReferences\":"))));
			ASSERT_THAT(IsTrue(Json.Contains(TEXT("\"type\":\"integer-literal\",\"bits\":\"0000000000000029\""))));
		}

		TEST_METHOD(AuthoredTypeSyntaxAndNestedAttributePayloadRoundTrip)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Variable = Context.Create<AS_NAMESPACE_QUALIFIER asCVarDecl>(
				AS_NAMESPACE_QUALIFIER asEDeclKind::Var, TEXT("Score"), Range(*Source, 0, 9));
			AS_NAMESPACE_QUALIFIER asSTypeSyntax Syntax;
			Syntax.NameParts = {TEXT("Score")};
			Syntax.ComponentRanges = {Range(*Source, 0, 5)};
			Syntax.Range = Range(*Source, 0, 9);
			AS_NAMESPACE_QUALIFIER asSTypeSyntax GenericArgument;
			GenericArgument.NameParts = {TEXT("int")};
			GenericArgument.ComponentRanges = {Range(*Source, 6, 9)};
			GenericArgument.Range = Range(*Source, 6, 9);
			Syntax.GenericArguments.Add(MoveTemp(GenericArgument));
			Syntax.ArrayDimensions.Add(Range(*Source, 7, 9));
			ASSERT_THAT(IsTrue(Variable->SetTypeSyntax(MoveTemp(Syntax))));
			ASSERT_THAT(IsTrue(Variable->SetResolvedType(
				Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32))));

			AS_NAMESPACE_QUALIFIER asSAnnotationArgument Nested;
			Nested.Name = TEXT("Count");
			Nested.Kind = AS_NAMESPACE_QUALIFIER asEAnnotationValueKind::Scalar;
			Nested.Value = TEXT("2");
			Nested.Range = Range(*Source, 4, 8);
			Nested.ValueRange = Range(*Source, 7, 8);
			AS_NAMESPACE_QUALIFIER asSAnnotationArgument Group;
			Group.Name = TEXT("Meta");
			Group.Kind = AS_NAMESPACE_QUALIFIER asEAnnotationValueKind::Group;
			Group.Range = Range(*Source, 0, 9);
			Group.ValueRange = Range(*Source, 4, 9);
			Group.Arguments.Add(MoveTemp(Nested));
			auto* Attribute = Context.Create<AS_NAMESPACE_QUALIFIER asCAnnotationAttr>(
				AS_NAMESPACE_QUALIFIER asEAnnotationKind::UProperty,
				TArray<AS_NAMESPACE_QUALIFIER asSAnnotationArgument>{MoveTemp(Group)},
				Range(*Source, 0, 9), AS_NAMESPACE_QUALIFIER asEAttrTarget::Variable);
			ASSERT_THAT(IsTrue(Variable->AddAttr(Attribute)));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verify;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Variable, Verify)));

			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, Variable, Projection, Error)));
			ASSERT_THAT(AreEqual(2, Projection.GetRecords().Num()));
			const auto* VarPayload = Projection.GetRecords()[0].TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTVarDeclPayload>();
			ASSERT_THAT(IsNotNull(VarPayload));
			ASSERT_THAT(AreEqual(FString(TEXT("Score")), VarPayload->Name));
			ASSERT_THAT(IsTrue(VarPayload->TypeSyntax.bPresent));
			ASSERT_THAT(AreEqual(FString(TEXT("Score<int>[]")), VarPayload->TypeSyntax.Spelling));
			ASSERT_THAT(AreEqual(1, VarPayload->TypeSyntax.ComponentRanges.Num()));
			ASSERT_THAT(AreEqual(1, VarPayload->TypeSyntax.GenericArguments.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("int")),
				VarPayload->TypeSyntax.GenericArguments[0].Spelling));
			ASSERT_THAT(AreEqual(1, VarPayload->TypeSyntax.ArrayDimensions.Num()));
			ASSERT_THAT(IsTrue(Projection.GetRecords()[0].Declaration.bPresent));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEAccessSpecifier::Public,
				Projection.GetRecords()[0].Declaration.Access));
			ASSERT_THAT(IsFalse(Projection.GetRecords()[1].Declaration.bPresent));
			const auto* AttrPayload = Projection.GetRecords()[1].TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTAnnotationAttrPayload>();
			ASSERT_THAT(IsNotNull(AttrPayload));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEAnnotationKind::UProperty, AttrPayload->AnnotationKind));
			ASSERT_THAT(AreEqual(2, AttrPayload->Arguments.Num()));
			ASSERT_THAT(AreEqual(MAX_uint32, AttrPayload->Arguments[0].ParentIndex));
			ASSERT_THAT(AreEqual(static_cast<uint32>(0), AttrPayload->Arguments[1].ParentIndex));

			TArray<uint8> Bytes;
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(IsTrue(Projection.Equals(Decoded)));
			ASSERT_THAT(IsTrue(Decoded.GetRecords()[0].Declaration.bPresent));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEAccessSpecifier::Public,
				Decoded.GetRecords()[0].Declaration.Access));

			AS_NAMESPACE_QUALIFIER asSASTCodecBudgets ShallowBudget;
			ShallowBudget.MaxTypeSyntaxDepth = 1;
			Decoded = Projection;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::BudgetExceeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded, ShallowBudget)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));

			int32 SpellingOffset = INDEX_NONE;
			for (int32 Index = 0; Index + 5 < Bytes.Num(); ++Index)
			{
				if (Bytes[Index] == 'S' && Bytes[Index + 1] == 'c'
					&& Bytes[Index + 2] == 'o' && Bytes[Index + 3] == 'r'
					&& Bytes[Index + 4] == 'e' && Bytes[Index + 5] == '<')
				{
					SpellingOffset = Index;
					break;
				}
			}
			ASSERT_THAT(AreNotEqual(INDEX_NONE, SpellingOffset));
			if (SpellingOffset == INDEX_NONE) return;
			Bytes[SpellingOffset] = 'X';
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::InvalidPayload,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
		}

		TEST_METHOD(StringLiteralAndCallSourceMappingRoundTrip)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Type = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			AS_NAMESPACE_QUALIFIER asCQualType QualifiedType;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(
				Type, AS_NAMESPACE_QUALIFIER asETypeQualifiers::None, QualifiedType)));
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor StringDeclarationDescriptor;
			StringDeclarationDescriptor.StableScope = TEXT("codec");
			StringDeclarationDescriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct;
			StringDeclarationDescriptor.Name = TEXT("FString");
			AS_NAMESPACE_QUALIFIER asSStableKey StringDeclarationIdentity;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.GetTypeContext()->GetIdentityRegistry().InternNominalType(
					StringDeclarationDescriptor, StringDeclarationIdentity)));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor StringUseDescriptor;
			StringUseDescriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Nominal;
			StringUseDescriptor.Declaration = StringDeclarationIdentity;
			AS_NAMESPACE_QUALIFIER asSStableKey StringUseIdentity;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.GetTypeContext()->GetIdentityRegistry().InternTypeUse(
					StringUseDescriptor, StringUseIdentity)));
			auto* StringType = Context.Create<AS_NAMESPACE_QUALIFIER asCNominalType>(
				StringDeclarationIdentity, StringUseIdentity);
			AS_NAMESPACE_QUALIFIER asCQualType QualifiedStringType;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(
				StringType, AS_NAMESPACE_QUALIFIER asETypeQualifiers::None, QualifiedStringType)));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor SignatureDescriptor;
			SignatureDescriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Callable;
			SignatureDescriptor.ReturnType = Type->GetStableIdentity();
			SignatureDescriptor.Parameters.Add({
				StringUseIdentity, AS_NAMESPACE_QUALIFIER asEParameterPassingMode::Value});
			AS_NAMESPACE_QUALIFIER asSStableKey SignatureIdentity;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.GetTypeContext()->GetIdentityRegistry().InternTypeUse(
					SignatureDescriptor, SignatureIdentity)));
			auto* Signature = Context.Create<AS_NAMESPACE_QUALIFIER asCFunctionType>(
				QualifiedType, TArray<AS_NAMESPACE_QUALIFIER asCQualType>{QualifiedStringType}, SignatureIdentity);

			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor FunctionDescriptor;
			FunctionDescriptor.StableScope = TEXT("codec");
			FunctionDescriptor.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global;
			FunctionDescriptor.Name = TEXT("Invoke");
			FunctionDescriptor.Parameters = SignatureDescriptor.Parameters;
			FunctionDescriptor.ReturnType = Type->GetStableIdentity();
			AS_NAMESPACE_QUALIFIER asSStableKey FunctionIdentity;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.GetTypeContext()->GetIdentityRegistry().InternFunction(
					FunctionDescriptor, FunctionIdentity)));
			auto* Function = Context.Create<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(
				TEXT("Invoke"), Range(*Source, 0, 1));
			ASSERT_THAT(IsTrue(Function->SetStableIdentity(FunctionIdentity)));
			ASSERT_THAT(IsTrue(Function->SetResolvedSignature(Signature)));
			ASSERT_THAT(IsTrue(Function->SetResolvedReturnType(Type)));
			auto* Callee = Context.Create<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Function, Range(*Source, 0, 1));
			auto* String = Context.Create<AS_NAMESPACE_QUALIFIER asCStringLiteral>(TEXT("hello"), Range(*Source, 2, 7));
			auto* Call = Context.Create<AS_NAMESPACE_QUALIFIER asCCallExpr>(Callee, Range(*Source, 0, 9));
			ASSERT_THAT(IsTrue(Call->AddArgument(String)));
			ASSERT_THAT(IsTrue(Call->SetArgumentSourceOrdinals(TArray<int32>{0})));
			ASSERT_THAT(IsTrue(Callee->SetExpressionSemantics(Signature, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsTrue(String->SetExpressionSemantics(StringType, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			ASSERT_THAT(IsTrue(Call->SetExpressionSemantics(Type, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verify;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Call, Verify)));

			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, Call, Projection, Error)));
			const auto* CallPayload = Projection.GetRecords()[0].TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTCallExprPayload>();
			ASSERT_THAT(IsNotNull(CallPayload));
			ASSERT_THAT(AreEqual(1, CallPayload->ArgumentSourceOrdinals.Num()));
			ASSERT_THAT(AreEqual(0, CallPayload->ArgumentSourceOrdinals[0]));
			const auto* StringPayload = Projection.GetRecords()[2].TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTStringLiteralPayload>();
			ASSERT_THAT(IsNotNull(StringPayload));
			ASSERT_THAT(AreEqual(FString(TEXT("hello")), StringPayload->Value));

			TArray<uint8> Bytes;
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(IsTrue(Projection.Equals(Decoded)));
		}

		TEST_METHOD(SwitchRoundTripPreservesClauseOrderAndTypedTransferTargets)
		{
			auto Source = ControlSnapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Selector = TypedInteger(Context, *Source, 0, 7, 8);
			auto* Switch = Context.Create<AS_NAMESPACE_QUALIFIER asCSwitchStmt>(
				Selector, Range(*Source, 0, 9), Range(*Source, 0, 44));
			auto* Label = TypedInteger(Context, *Source, 1, 15, 16);
			auto* Case = Context.Create<AS_NAMESPACE_QUALIFIER asCCaseStmt>(
				Switch, Label, Range(*Source, 10, 14), Range(*Source, 16, 17),
				Range(*Source, 10, 29));
			ASSERT_THAT(IsTrue(Case->SetResolvedValueBits(1)));
			auto* Default = Context.Create<AS_NAMESPACE_QUALIFIER asCDefaultStmt>(
				Switch, Range(*Source, 29, 36), Range(*Source, 36, 37),
				Range(*Source, 29, 43));
			auto* Fallthrough = Context.Create<AS_NAMESPACE_QUALIFIER asCFallthroughStmt>(
				Default, Range(*Source, 17, 29));
			ASSERT_THAT(IsTrue(Case->AddStatement(Fallthrough)));
			ASSERT_THAT(IsTrue(Default->AddStatement(Context.Create<AS_NAMESPACE_QUALIFIER asCBreakStmt>(
				Switch, Range(*Source, 37, 43)))));
			ASSERT_THAT(IsTrue(Switch->AddClause(Case)));
			ASSERT_THAT(IsTrue(Switch->AddClause(Default)));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verify;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Switch, Verify)));

			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, Switch, Projection, Error)));
			const auto* SwitchPayload = Projection.GetRecords()[0].TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTSwitchStmtPayload>();
			ASSERT_THAT(IsNotNull(SwitchPayload));
			ASSERT_THAT(AreEqual(static_cast<uint32>(2), SwitchPayload->ClauseCount));
			ASSERT_THAT(AreEqual(static_cast<uint32>(1), SwitchPayload->DefaultOrdinal));
			ASSERT_THAT(AreEqual(static_cast<uint32>(0), SwitchPayload->HeaderSource.BeginByteOffset));
			ASSERT_THAT(AreEqual(static_cast<uint32>(9), SwitchPayload->HeaderSource.EndByteOffset));
			const auto* CasePayload = Projection.GetRecords()[2].TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTCaseStmtPayload>();
			ASSERT_THAT(IsNotNull(CasePayload));
			ASSERT_THAT(AreEqual(static_cast<uint32>(1), CasePayload->StatementCount));
			ASSERT_THAT(AreEqual(static_cast<uint64>(1), CasePayload->ResolvedValueBits));
			TArray<uint8> Bytes;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(IsTrue(Projection.Equals(Decoded)));
			ASSERT_THAT(AreEqual(7, Decoded.GetRecords().Num()));
			ASSERT_THAT(AreEqual(static_cast<uint16>(AS_NAMESPACE_QUALIFIER asEStmtKind::Switch),
				Decoded.GetRecords()[0].Kind));
			ASSERT_THAT(AreEqual(static_cast<uint32>(2), Decoded.GetRecords()[0].Children[1]));
			ASSERT_THAT(AreEqual(static_cast<uint32>(5), Decoded.GetRecords()[0].Children[2]));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCrossReferenceKind::SwitchParent,
				Decoded.GetRecords()[2].CrossReferences[0].Kind));
			ASSERT_THAT(AreEqual(static_cast<uint32>(1),
				Decoded.GetRecords()[2].CrossReferences[0].LocalIndex));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCrossReferenceKind::FallthroughTarget,
				Decoded.GetRecords()[4].CrossReferences[0].Kind));
			ASSERT_THAT(AreEqual(static_cast<uint32>(6),
				Decoded.GetRecords()[4].CrossReferences[0].LocalIndex));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCrossReferenceKind::BreakTarget,
				Decoded.GetRecords()[6].CrossReferences[0].Kind));
			ASSERT_THAT(AreEqual(static_cast<uint32>(1),
				Decoded.GetRecords()[6].CrossReferences[0].LocalIndex));

			// Root record's stable prefix is unchanged in v6. With the fifteen-byte
			// logical source name and three owning children, the Switch payload tag
			// is byte 74 and ClauseCount begins at byte 75.
			ASSERT_THAT(IsTrue(Bytes.Num() > 79));
			TArray<uint8> WrongClauseCount = Bytes;
			WrongClauseCount[75] = 1;
			Decoded = Projection;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::InvalidPayload,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(WrongClauseCount, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));

			// Case begins at byte 277 in this fixed fixture. Its first cross-role is
			// byte 331; changing SwitchParent to the valid ContinueTarget enum must
			// still fail because continue may not target a switch.
			ASSERT_THAT(IsTrue(Bytes.Num() > 331));
			TArray<uint8> WrongCrossRole = Bytes;
			WrongCrossRole[331] = static_cast<uint8>(AS_NAMESPACE_QUALIFIER asEASTCrossReferenceKind::ContinueTarget);
			Decoded = Projection;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::InvalidPayload,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(WrongCrossRole, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
		}

		TEST_METHOD(MultiForRoundTripPreservesExactPhaseBoundaries)
		{
			auto Source = ControlSnapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* InitA = Context.Create<AS_NAMESPACE_QUALIFIER asCNullStmt>(Range(*Source, 0, 1));
			auto* InitB = Context.Create<AS_NAMESPACE_QUALIFIER asCNullStmt>(Range(*Source, 1, 2));
			auto* Condition = Context.Create<AS_NAMESPACE_QUALIFIER asCBooleanLiteral>(true, Range(*Source, 2, 3));
			ASSERT_THAT(IsTrue(Condition->SetExpressionSemantics(
				Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Bool),
				AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			auto* IncrementA = TypedInteger(Context, *Source, 1, 3, 4);
			auto* IncrementB = TypedInteger(Context, *Source, 2, 4, 5);
			auto* Body = Context.Create<AS_NAMESPACE_QUALIFIER asCNullStmt>(Range(*Source, 5, 6));
			auto* For = Context.Create<AS_NAMESPACE_QUALIFIER asCForStmt>(
				TArray<AS_NAMESPACE_QUALIFIER asCStmt*>{InitA, InitB}, Condition,
				TArray<AS_NAMESPACE_QUALIFIER asCExpr*>{IncrementA, IncrementB}, Body,
				Range(*Source, 0, 5), Range(*Source, 0, 6));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verify;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(For, Verify)));

			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, For, Projection, Error)));
			const auto* Payload = Projection.GetRecords()[0].TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTForStmtPayload>();
			ASSERT_THAT(IsNotNull(Payload));
			ASSERT_THAT(AreEqual(static_cast<uint32>(2), Payload->InitializerCount));
			ASSERT_THAT(IsTrue(Payload->bHasCondition));
			ASSERT_THAT(AreEqual(static_cast<uint32>(2), Payload->IncrementCount));
			ASSERT_THAT(AreEqual(6, Projection.GetRecords()[0].Children.Num()));

			TArray<uint8> Bytes;
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(IsTrue(Projection.Equals(Decoded)));
		}

		TEST_METHOD(InitializerListAndValueInitializationRoundTripTypedBinding)
		{
			auto Source = ControlSnapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Type = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Descriptor;
			Descriptor.StableScope = TEXT("codec");
			Descriptor.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global;
			Descriptor.Name = TEXT("InitializePair");
			Descriptor.ReturnType = Type->GetStableIdentity();
			AS_NAMESPACE_QUALIFIER asSStableKey InitializerIdentity;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.GetTypeContext()->GetIdentityRegistry().InternFunction(
					Descriptor, InitializerIdentity)));
			auto* ValueInit = Context.Create<AS_NAMESPACE_QUALIFIER asCValueInitExpr>(Range(*Source, 1, 2));
			auto* Explicit = TypedInteger(Context, *Source, 7, 3, 4);
			ASSERT_THAT(IsTrue(ValueInit->SetExpressionSemantics(
				Type, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			auto* Initializer = Context.Create<AS_NAMESPACE_QUALIFIER asCInitListExpr>(
				TArray<AS_NAMESPACE_QUALIFIER asCExpr*>{ValueInit, Explicit}, Range(*Source, 0, 5));
			ASSERT_THAT(IsTrue(Initializer->SetInitializerBinding(
				InitializerIdentity, TArray<int32>{1, 1})));
			ASSERT_THAT(IsTrue(Initializer->SetExpressionSemantics(
				Type, AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue)));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verify;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Initializer, Verify)));

			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, Initializer, Projection, Error)));
			const auto* Payload = Projection.GetRecords()[0].TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTInitListExprPayload>();
			ASSERT_THAT(IsNotNull(Payload));
			ASSERT_THAT(IsTrue(Payload->InitializerIdentity == InitializerIdentity));
			ASSERT_THAT(IsTrue(Payload->ElementPatternIndices == TArray<int32>{1, 1}));
			ASSERT_THAT(IsNotNull(Projection.GetRecords()[1].TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTValueInitExprPayload>()));

			TArray<uint8> Bytes;
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(IsTrue(Projection.Equals(Decoded)));
			ASSERT_THAT(IsTrue(Decoded.ValidateIdentities(
				Context.GetTypeContext()->GetIdentityRegistry(), Error)));

			const uint8 MappingWire[] = {2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
			int32 MappingOffset = INDEX_NONE;
			for (int32 Offset = 0; Offset + UE_ARRAY_COUNT(MappingWire) <= Bytes.Num(); ++Offset)
			{
				if (FMemory::Memcmp(Bytes.GetData() + Offset, MappingWire, UE_ARRAY_COUNT(MappingWire)) != 0) continue;
				ASSERT_THAT(AreEqual(INDEX_NONE, MappingOffset));
				MappingOffset = Offset;
			}
			ASSERT_THAT(IsTrue(MappingOffset != INDEX_NONE));
			TArray<uint8> NegativeMapping = Bytes;
			for (int32 Byte = 0; Byte < 4; ++Byte)
				NegativeMapping[MappingOffset + 8 + Byte] = 0xff;
			Decoded = Projection;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::InvalidPayload,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(NegativeMapping, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));

			AS_NAMESPACE_QUALIFIER asSASTCodecBudgets PayloadBudget;
			PayloadBudget.MaxEdges = 3;
			Decoded = Projection;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::BudgetExceeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded, PayloadBudget)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
		}

		TEST_METHOD(EnumInitializerStateAndSourceRoundTrip)
		{
			auto Source = Snapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Initializer = TypedInteger(Context, *Source, 7, 7, 8);
			auto* Constant = Context.Create<AS_NAMESPACE_QUALIFIER asCEnumConstantDecl>(
				TEXT("Entry"), 7, true, Range(*Source, 0, 9));
			ASSERT_THAT(IsTrue(Constant->SetDeferredInitializerRange(Range(*Source, 7, 8))));
			ASSERT_THAT(IsTrue(Constant->SetInitializer(Initializer)));
			ASSERT_THAT(IsTrue(Constant->SetResolvedValue(7)));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verify;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Constant, Verify)));

			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, Constant, Projection, Error)));
			const auto* Payload = Projection.GetRecords()[0].TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTEnumConstantDeclPayload>();
			ASSERT_THAT(IsNotNull(Payload));
			ASSERT_THAT(IsTrue(Payload->bExplicitValue));
			ASSERT_THAT(IsTrue(Payload->bResolvedValue));
			ASSERT_THAT(IsTrue(Payload->bHasDeferredInitializer));
			ASSERT_THAT(AreEqual(static_cast<uint32>(7),
				Payload->DeferredInitializerSource.BeginByteOffset));

			TArray<uint8> Bytes;
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(IsTrue(Projection.Equals(Decoded)));
		}

		TEST_METHOD(OldAndUnknownVersionsFailWithoutPartialOutput)
		{
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			ASSERT_THAT(IsTrue(MakeProjection(Projection)));
			TArray<uint8> Bytes;
			AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes);
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			TArray<uint8> Old = Bytes;
			Old[4] = 1; Old[5] = 0;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::IncompatibleVersion, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Old, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
			Old[4] = 2;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::IncompatibleVersion, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Old, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
			Old[4] = 3;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::IncompatibleVersion, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Old, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
			Old[4] = 4;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::IncompatibleVersion, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Old, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
			Old[4] = 5;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::IncompatibleVersion, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Old, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
			TArray<uint8> Unknown = Bytes;
			Unknown[4] = 99; Unknown[5] = 0;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::UnsupportedVersion, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Unknown, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
		}

		TEST_METHOD(CorruptIndicesAndTruncationFailClosed)
		{
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			ASSERT_THAT(IsTrue(MakeProjection(Projection)));
			TArray<uint8> Bytes;
			AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes);
			// This fixture has an eight-byte logical source name; its first child
			// starts at byte 45 in the documented v6 projection, after edge count.
			ASSERT_THAT(IsTrue(Bytes.Num() > 49));
			Bytes[45] = 0x0f; Bytes[46] = 0x27; Bytes[47] = 0; Bytes[48] = 0;
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::InvalidIndex, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			Bytes.SetNum(7);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Malformed, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
		}

		TEST_METHOD(DecodeBudgetsAreEnforcedBeforePublication)
		{
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			ASSERT_THAT(IsTrue(MakeProjection(Projection)));
			TArray<uint8> Bytes;
			AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes);
			AS_NAMESPACE_QUALIFIER asSASTCodecBudgets Budgets;
			Budgets.MaxNodes = 1;
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::BudgetExceeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded, Budgets)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
		}

		TEST_METHOD(FixedIdentitySurvivesSourceAndContextDestruction)
		{
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			AS_NAMESPACE_QUALIFIER asSStableKey Expected;
			{
				auto Source = Snapshot();
				AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
				auto* Type = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt64);
				Expected = Type->GetStableIdentity();
				AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verify;
				ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Type, Verify)));
				FString Error;
				ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, Type, Projection, Error)));
			}
			TArray<uint8> Bytes;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(IsTrue(Decoded.GetRecords()[0].StableIdentity == Expected));
			ASSERT_THAT(IsTrue(Projection.Equals(Decoded)));
		}

		TEST_METHOD(EveryTruncatedFixedKeyRecordClearsPreviousOutput)
		{
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			ASSERT_THAT(IsTrue(MakeProjection(Projection)));
			TArray<uint8> Bytes;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			for (int32 Size = 0; Size < Bytes.Num(); ++Size)
			{
				AS_NAMESPACE_QUALIFIER asCASTProjection Decoded = Projection;
				ASSERT_THAT(AreNotEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
					AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(MakeArrayView(Bytes.GetData(), Size), Decoded)));
				ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
			}
		}

		TEST_METHOD(ByteAndEdgeBudgetsRejectBeforePublishingAnyRecord)
		{
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			ASSERT_THAT(IsTrue(MakeProjection(Projection)));
			TArray<uint8> Bytes;
			AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes);
			AS_NAMESPACE_QUALIFIER asSASTCodecBudgets Budgets;
			Budgets.MaxBytes = static_cast<uint32>(Bytes.Num() - 1);
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded = Projection;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::BudgetExceeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded, Budgets)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
			Budgets.MaxBytes = static_cast<uint32>(Bytes.Num());
			Budgets.MaxEdges = 0;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::BudgetExceeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded, Budgets)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
		}

		TEST_METHOD(UnknownConcreteKindAndTrailingDataFailClosed)
		{
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			ASSERT_THAT(IsTrue(MakeProjection(Projection)));
			TArray<uint8> Bytes;
			AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes);
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			TArray<uint8> Unknown = Bytes;
			Unknown[11] = 0xff; Unknown[12] = 0xff;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Malformed, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Unknown, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
			Bytes.Add(0);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Malformed, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
		}

		TEST_METHOD(WrongPayloadAlternativeAndInvalidExpressionEnumFailClosed)
		{
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			ASSERT_THAT(IsTrue(MakeProjection(Projection)));
			TArray<uint8> Bytes;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));

			// The root Compound payload tag is byte 59 after its one child,
			// empty cross/semantic lists, and declaration/expression flags.
			TArray<uint8> WrongPayload = Bytes;
			WrongPayload[59] = 0xff;
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded = Projection;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::InvalidPayload,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(WrongPayload, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));

			// The following IntegerLiteral begins at byte 92; its expression
			// value category is byte 169 in this fixed fixture.
			TArray<uint8> InvalidEnum = Bytes;
			InvalidEnum[169] = 0xff;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::InvalidPayload,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(InvalidEnum, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
		}

		TEST_METHOD(DecodedIdentityRequiresAnExplicitMatchingRegistry)
		{
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Snapshot());
			auto* Type = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verify;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Type, Verify)));
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, Type, Projection, Error)));
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			ASSERT_THAT(IsFalse(Projection.ValidateIdentities(Registry, Error)));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Descriptor;
			Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
			Descriptor.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32;
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.InternTypeUse(Descriptor, Key)));
			ASSERT_THAT(IsTrue(Projection.ValidateIdentities(Registry, Error)));
			TArray<uint8> Bytes;
			AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes);
			for (int32 Index = Bytes.Num() - 32; Index < Bytes.Num(); ++Index) Bytes[Index] = 0xff;
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(IsFalse(Decoded.ValidateIdentities(Registry, Error)));
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Nominal;
			Nominal.StableScope = TEXT("codec"); Nominal.Name = TEXT("KnownButWrongRole"); Nominal.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.InternNominalType(Nominal, Key)));
			FMemory::Memcpy(Bytes.GetData() + Bytes.Num() - 32, Key.GetBytes().GetData(), 32);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(IsFalse(Decoded.ValidateIdentities(Registry, Error)));
		}

		TEST_METHOD(ProjectionRejectsUnownedIdentityBeforePublishingRecords)
		{
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Snapshot());
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry ForeignRegistry;
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Descriptor;
			Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
			Descriptor.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt16;
			AS_NAMESPACE_QUALIFIER asSStableKey ForeignKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, ForeignRegistry.InternTypeUse(Descriptor, ForeignKey)));
			auto* Type = Context.Create<AS_NAMESPACE_QUALIFIER asCBuiltinType>(AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt16, ForeignKey);
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verify;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Type, Verify)));
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, Type, Projection, Error)));
			ASSERT_THAT(AreEqual(0, Projection.GetRecords().Num()));
			ASSERT_THAT(IsFalse(Error.IsEmpty()));
		}

		TEST_METHOD(ReversedSourceRangeDoesNotSurviveDecode)
		{
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			ASSERT_THAT(IsTrue(MakeProjection(Projection)));
			TArray<uint8> Bytes;
			AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes);
			// Root source begin is byte 33 and end is byte 37; authored end is 9.
			Bytes[33] = 10;
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Malformed, AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
		}

		TEST_METHOD(V7AccessAndUserConversionRoundTripPreservesAuthoredSemantics)
		{
			FSessionRun Run({{TEXT("AccessConversionCodecV7.as"), TEXT(
				"class Source { "
				"access GateV7Unique = private, Reader(readonly), *(editdefaults, inherited); "
				"access:GateV7Unique int Value; "
				"int opImplConv() const; "
				"} "
				"int Convert(Source Input) { return Input; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const bool bVerified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(bVerified, Run.Session->GetVerificationError()));

			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(
				Run.Session->GetASTContext(), Run.Session->GetTranslationUnit(), Projection, Error),
				Error));

			int32 AccessIndex = INDEX_NONE;
			int32 MemberIndex = INDEX_NONE;
			int32 ConversionIndex = INDEX_NONE;
			for (int32 Index = 0; Index < Projection.GetRecords().Num(); ++Index)
			{
				const AS_NAMESPACE_QUALIFIER asSASTProjectionRecord& Record = Projection.GetRecords()[Index];
				if (Record.Family == AS_NAMESPACE_QUALIFIER asEASTNodeFamily::Decl
					&& Record.Kind == static_cast<uint16>(AS_NAMESPACE_QUALIFIER asEDeclKind::AccessSpecifier))
					AccessIndex = Index;
				if (Record.Declaration.AuthoredAccess.bPresent) MemberIndex = Index;
				if (Record.Family == AS_NAMESPACE_QUALIFIER asEASTNodeFamily::Stmt
					&& Record.Kind == static_cast<uint16>(AS_NAMESPACE_QUALIFIER asEStmtKind::UserConversionExpr))
					ConversionIndex = Index;
			}
			ASSERT_THAT(IsTrue(AccessIndex != INDEX_NONE));
			ASSERT_THAT(IsTrue(MemberIndex != INDEX_NONE));
			ASSERT_THAT(IsTrue(ConversionIndex != INDEX_NONE));

			const auto& AccessRecord = Projection.GetRecords()[AccessIndex];
			const auto* Access = AccessRecord.TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTAccessSpecifierDeclPayload>();
			ASSERT_THAT(IsNotNull(Access));
			ASSERT_THAT(AreEqual(FString(TEXT("GateV7Unique")), Access->Name));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEAccessSpecifier::Private, Access->BaseAccess));
			ASSERT_THAT(AreEqual(2, Access->Permissions.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEAccessPermissionSubject::Named,
				Access->Permissions[0].Subject));
			ASSERT_THAT(AreEqual(FString(TEXT("Reader")), Access->Permissions[0].Name));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEAccessPermissionModifier::ReadOnly,
				Access->Permissions[0].Modifiers[0].Kind));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEAccessPermissionSubject::Wildcard,
				Access->Permissions[1].Subject));
			ASSERT_THAT(IsTrue(Access->Permissions[1].Name.IsEmpty()));
			ASSERT_THAT(AreEqual(2, Access->Permissions[1].Modifiers.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEAccessPermissionModifier::Inherited,
				Access->Permissions[1].Modifiers[1].Kind));

			const auto& Member = Projection.GetRecords()[MemberIndex];
			ASSERT_THAT(AreEqual(FString(TEXT("GateV7Unique")),
				Member.Declaration.AuthoredAccess.Name));
			ASSERT_THAT(AreEqual(1, Member.CrossReferences.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCrossReferenceKind::CustomAccessSpecifier,
				Member.CrossReferences[0].Kind));
			ASSERT_THAT(AreEqual(static_cast<uint32>(AccessIndex + 1),
				Member.CrossReferences[0].LocalIndex));

			const auto& ConversionRecord = Projection.GetRecords()[ConversionIndex];
			const auto* Conversion =
				ConversionRecord.TryGetPayload<AS_NAMESPACE_QUALIFIER asSASTUserConversionPayload>();
			ASSERT_THAT(IsNotNull(Conversion));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEUserConversionMode::Implicit, Conversion->Mode));
			ASSERT_THAT(AreEqual(1, ConversionRecord.Children.Num()));
			ASSERT_THAT(AreEqual(1, ConversionRecord.CrossReferences.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCrossReferenceKind::ConversionFunction,
				ConversionRecord.CrossReferences[0].Kind));
			ASSERT_THAT(IsTrue(ConversionRecord.CrossReferences[0].LocalIndex != 0));

			TArray<uint8> First;
			TArray<uint8> Second;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, First)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Second)));
			ASSERT_THAT(IsTrue(First == Second));
			ASSERT_THAT(AreEqual(static_cast<uint8>(AS_NAMESPACE_QUALIFIER asCASTCodec::CurrentVersion), First[4]));
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(First, Decoded)));
			ASSERT_THAT(IsTrue(Projection.Equals(Decoded)));
		}

		TEST_METHOD(V7AccessPayloadRejectsTagEnumRangeAndNestedBudgetCorruption)
		{
			FSessionRun Run({{TEXT("AccessCodecV7.as"), TEXT(
				"class Source { "
				"access GateV7Unique = protected, Reader(readonly), *(editdefaults, inherited); "
				"}")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const bool bVerified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(bVerified, Run.Session->GetVerificationError()));
			auto* Record = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(
				Run.Session->Lookup(TEXT("Source"))[0]);
			const TConstArrayView<AS_NAMESPACE_QUALIFIER asCDecl*> Found = Record->Lookup(TEXT("GateV7Unique"));
			ASSERT_THAT(AreEqual(1, Found.Num()));
			auto* Access = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCAccessSpecifierDecl>(Found[0]);
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(
				Run.Session->GetASTContext(), Access, Projection, Error), Error));
			TArray<uint8> Bytes;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			FRootWireOffsets Offsets;
			ASSERT_THAT(IsTrue(LocateRootWireOffsets(Bytes, Offsets)));
			// Tags 0..36 are the frozen v6 payload tags. Access is appended as 37.
			ASSERT_THAT(AreEqual(static_cast<uint8>(37), Bytes[Offsets.PayloadTag]));

			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded = Projection;
			TArray<uint8> WrongTag = Bytes;
			WrongTag[Offsets.PayloadTag] = 0;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::InvalidPayload,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(WrongTag, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));

			int32 NameLengthOffset = Offsets.PayloadTag + 1;
			uint32 NameLength = 0;
			ASSERT_THAT(IsTrue(ReadWireU32(Bytes, NameLengthOffset, NameLength)));
			const int32 BaseAccessOffset = NameLengthOffset + static_cast<int32>(NameLength);
			TArray<uint8> PublicBase = Bytes;
			PublicBase[BaseAccessOffset] = static_cast<uint8>(AS_NAMESPACE_QUALIFIER asEAccessSpecifier::Public);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::InvalidPayload,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(PublicBase, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));

			int32 KeywordAnchor = BaseAccessOffset + 1;
			uint32 LogicalNameLength = 0;
			ASSERT_THAT(IsTrue(ReadWireU32(Bytes, KeywordAnchor, LogicalNameLength)));
			const int32 KeywordBegin = KeywordAnchor
				+ static_cast<int32>(LogicalNameLength) + 8;
			TArray<uint8> InvalidRange = Bytes;
			WriteWireU32(InvalidRange, KeywordBegin, MAX_uint32);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::InvalidPayload,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(InvalidRange, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));

			AS_NAMESPACE_QUALIFIER asSASTCodecBudgets Budget;
			Budget.MaxEdges = 4;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::BudgetExceeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(Bytes, Decoded, Budget)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
		}

		TEST_METHOD(V7UserConversionRejectsModeCrossRoleAndIndexCorruption)
		{
			FSessionRun Run({{TEXT("ConversionCodecV7.as"), TEXT(
				"class Source { int opImplConv() const; } "
				"int Convert(Source Input) { return Input; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const bool bVerified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(bVerified, Run.Session->GetVerificationError()));
			const AS_NAMESPACE_QUALIFIER asCUserConversionExpr* Conversion = nullptr;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVisitStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTVisitor::Walk(Run.Session->GetTranslationUnit(),
					[&Conversion](const AS_NAMESPACE_QUALIFIER asCASTNode& Node)
					{
						if (!Conversion)
							Conversion = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCUserConversionExpr>(&Node);
					})));
			ASSERT_THAT(IsNotNull(Conversion));
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(
				Run.Session->GetASTContext(), Conversion, Projection, Error), Error));
			TArray<uint8> Bytes;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			FRootWireOffsets Offsets;
			ASSERT_THAT(IsTrue(LocateRootWireOffsets(Bytes, Offsets)));
			ASSERT_THAT(IsTrue(Offsets.FirstCrossKind != INDEX_NONE));
			// UserConversion is appended after Access as v7 payload tag 38.
			ASSERT_THAT(AreEqual(static_cast<uint8>(38), Bytes[Offsets.PayloadTag]));

			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded = Projection;
			TArray<uint8> InvalidMode = Bytes;
			InvalidMode[Offsets.PayloadTag + 1] = 0xff;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::InvalidPayload,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(InvalidMode, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));

			TArray<uint8> WrongRole = Bytes;
			WrongRole[Offsets.FirstCrossKind] =
				static_cast<uint8>(AS_NAMESPACE_QUALIFIER asEASTCrossReferenceKind::CustomAccessSpecifier);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::InvalidPayload,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(WrongRole, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));

			TArray<uint8> InvalidRole = Bytes;
			InvalidRole[Offsets.FirstCrossKind] = 0xff;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::InvalidPayload,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(InvalidRole, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));

			TArray<uint8> InvalidIndex = Bytes;
			WriteWireU32(InvalidIndex, Offsets.FirstCrossLocalIndex,
				static_cast<uint32>(Projection.GetRecords().Num() + 1));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::InvalidIndex,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(InvalidIndex, Decoded)));
			ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
		}

		TEST_METHOD(V7UserConversionRejectsAdmittedTypeUseKeyInFunctionSlot)
		{
			FSessionRun Run({{TEXT("ConversionCodecWrongRole.as"), TEXT(
				"class Source { int opImplConv() const; } "
				"int Convert(Source Input) { return Input; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST(), Run.Session->GetVerificationError()));
			const AS_NAMESPACE_QUALIFIER asCUserConversionExpr* Conversion = nullptr;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVisitStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTVisitor::Walk(Run.Session->GetTranslationUnit(),
					[&Conversion](const AS_NAMESPACE_QUALIFIER asCASTNode& Node)
					{
						if (!Conversion)
							Conversion = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCUserConversionExpr>(&Node);
					})));
			ASSERT_THAT(IsNotNull(Conversion));
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(
				Run.Session->GetASTContext(), Conversion, Projection, Error), Error));
			TArray<uint8> Bytes;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Encode(Projection, Bytes)));
			FRootWireOffsets Offsets;
			ASSERT_THAT(IsTrue(LocateRootWireOffsets(Bytes, Offsets)));
			ASSERT_THAT(IsTrue(Offsets.FirstCrossKind != INDEX_NONE));
			const TConstArrayView<uint8> TypeUse =
				Projection.GetRecords()[0].Expression.TypeIdentity.GetBytes();
			ASSERT_THAT(AreEqual(static_cast<int32>(AS_NAMESPACE_QUALIFIER asSStableKey::ByteCount), TypeUse.Num()));
			TArray<uint8> WrongKey = Bytes;
			FMemory::Memcpy(&WrongKey[Offsets.FirstCrossKind + 5], TypeUse.GetData(), TypeUse.Num());
			AS_NAMESPACE_QUALIFIER asCASTProjection Decoded = Projection;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCodecStatus::Succeeded,
				AS_NAMESPACE_QUALIFIER asCASTCodec::Decode(WrongKey, Decoded)));
			FString IdentityError;
			ASSERT_THAT(IsFalse(Decoded.ValidateIdentities(
				Run.Session->GetASTContext().GetTypeContext()->GetIdentityRegistry(), IdentityError)));
		}

		TEST_METHOD(VerificationFailureCannotSealOrPublishMismatchedTypeIdentity)
		{
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Snapshot());
			auto* Int = Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			auto* Forged = Context.Create<AS_NAMESPACE_QUALIFIER asCBuiltinType>(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Bool, Int->GetStableIdentity());
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verify;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Forged, Verify)));
			ASSERT_THAT(IsFalse(Context.IsSealed()));
			AS_NAMESPACE_QUALIFIER asCASTProjection Projection;
			FString Error;
			// The key belongs to this registry and has the TypeUse role, but the
			// actual type node does not describe that key. Failed verification never
			// establishes a publishable sealed state.
			ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER asCASTProjection::Build(Context, Forged, Projection, Error)));
			ASSERT_THAT(AreEqual(0, Projection.GetRecords().Num()));
			ASSERT_THAT(IsFalse(Error.IsEmpty()));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
