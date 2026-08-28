#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheASTBodySidecar.h"

#include "CQTest.h"
#include <cstring>

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_lifetime.h"
#include "source/as_ast_sidecar.h"
#include "source/as_sema.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheASTBodySidecarTests,
	"Angelscript.TestModule.Cache.ASTBodySidecar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SidecarAdmissionRequiresBuildingInputAndFrozenPublishableOutput)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		Sema.ActOnTranslationUnit("SidecarLifecycle");
		asSASTSidecarHeader Header;
		Header.functionKey = "SidecarLifecycle";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;

		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("Sidecar encode must reject Building")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_BUILDING, asAST_CONTEXT_SEMA_FINALIZED),
			TEXT("fixture must reach SemaFinalized")));
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("Sidecar encode must reject SemaFinalized")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_SEMA_FINALIZED, asAST_CONTEXT_LIFETIME_PLANNED),
			TEXT("fixture must reach LifetimePlanned")));
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("Sidecar encode must reject LifetimePlanned")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_LIFETIME_PLANNED, asAST_CONTEXT_FROZEN_PUBLISHABLE),
			TEXT("fixture must reach Frozen/Publishable")));
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("Sidecar encode must accept only Frozen/Publishable")));

		asCASTContext StaleDecodeTarget;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK,
			StaleDecodeTarget.AdvanceLifecycle(
				asAST_CONTEXT_BUILDING, asAST_CONTEXT_SEMA_FINALIZED),
			TEXT("decode rejection fixture must leave Building")));
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTDecodeSidecar(
				StaleDecodeTarget, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("Sidecar decode must reject every non-Building destination")));
		ASSERT_THAT(IsFalse(StaleDecodeTarget.IsPublishable(),
			TEXT("rejected decode must not publish an intermediate Context")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("Sidecar decode must accept a fresh Building destination")));
		ASSERT_THAT(IsTrue(Restored.IsPublishable(),
			TEXT("successful decode must complete the same Frozen/Publishable lifecycle")));
	}

	TEST_METHOD(RecordKindVersionRoundTripAndAbsenceRules)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		Sema.ActOnTranslationUnit("Mod");
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("sample AST should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "Mod.F";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode sealed AST sidecar")));
		ASSERT_THAT(IsTrue(Bytes.GetLength() > 0, TEXT("sidecar payload is not absence")));

		asCASTContext Restored;
		asSASTSidecarHeader Expected = Header;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Expected, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("retain restore performs zero re-Sema")));
		ASSERT_THAT(IsTrue(Restored.IsPublishable(), TEXT("retain restore publishes a Frozen/Publishable AST")));

		asSASTSidecarHeader WrongKey = Header;
		WrongKey.functionKey = "Other.F";
		asCASTContext Mismatch;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_FUNCTION_KEY_MISMATCH,
			asCASTDecodeSidecar(Mismatch, WrongKey, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("FunctionKey mismatch fails closed")));

		asSASTSidecarHeader WrongProfile = Header;
		WrongProfile.profile = "Android";
		asCASTContext ProfileMismatch;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_PROFILE_MISMATCH,
			asCASTDecodeSidecar(ProfileMismatch, WrongProfile, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("profile mismatch fails closed")));

		asCASTContext Empty;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_EMPTY_NOT_ABSENCE,
			asCASTDecodeSidecar(Empty, Header, Bytes.AddressOf(), 0),
			TEXT("empty payload is not canonical absence")));

		const asBYTE Unknown[] = { 'C', 'A', 'S', 'T', 99, 0, 0, 0 };
		asCASTContext UnknownKind;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNKNOWN_KIND,
			asCASTDecodeSidecar(UnknownKind, Header, Unknown, 8),
			TEXT("unknown node/type/schema kind fails closed")));
	}

	TEST_METHOD(SidecarRoundTripPreservesCanonicalLifetimeProtocol)
	{
		asCASTContext Context;
		const asASTDeclId TranslationUnit =
			Context.CreateTranslationUnit("LifetimeSidecar");
		const asCQualType VoidType = Context.InternPrimitive(ttVoid);
		const asCQualType ElementType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FElement", 0);
		const asCQualType ArrayType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FElementArray", 0);

		const asASTDeclId ElementClass = Context.CreateDecl(
			asAST_DECL_CLASS, TranslationUnit, asCSourceRange(), "FElement");
		ASSERT_THAT(AreEqual(0,
			Context.SetStableKey(ElementClass, "FElement")));
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(ElementClass, ElementType)));
		const asASTDeclId ElementDestructor = Context.CreateDecl(
			asAST_DECL_DESTRUCTOR, ElementClass,
			asCSourceRange(), "FElement");
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(ElementDestructor, VoidType)));

		const asASTDeclId ArrayClass = Context.CreateDecl(
			asAST_DECL_CLASS, TranslationUnit,
			asCSourceRange(), "FElementArray");
		ASSERT_THAT(AreEqual(0,
			Context.SetStableKey(ArrayClass, "FElementArray")));
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(ArrayClass, ArrayType)));
		const asASTDeclId ListFactory = Context.CreateDecl(
			asAST_DECL_CONSTRUCTOR, ArrayClass,
			asCSourceRange(), "FElementArray");
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(ListFactory, VoidType)));
		ASSERT_THAT(AreEqual(0,
			Context.AddDeclTrait(ListFactory, asAST_TRAIT_LIST_FACTORY)));

		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, TranslationUnit,
			asCSourceRange(), "BuildArray");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Function, VoidType)));
		const asASTStmtId Body = Context.CreateStmt(
			asAST_STMT_BLOCK, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetBody(Function, Body)));

		const asASTExprId Aggregate = Context.CreateExpr(
			asAST_EXPR_CONSTRUCT, ArrayType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.SetLiteral(Aggregate, "list-pattern")));
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(Aggregate, ListFactory)));
		const asASTExprId Element = Context.CreateExpr(
			asAST_EXPR_CONSTRUCT, ElementType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.SetLiteral(Element, "FElement::FElement()")));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Aggregate, Element)));
		const asASTStmtId AggregateStatement = Context.CreateStmt(
			asAST_STMT_EXPR, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.SetStmtExpr(AggregateStatement, Aggregate)));
		ASSERT_THAT(AreEqual(0,
			Context.AddStmtChild(Body, AggregateStatement)));

		asSASTLifetimeRecord Record;
		Record.subjectKind = asAST_LIFETIME_SUBJECT_AGGREGATE_ELEMENT;
		Record.subject = asSASTLifetimeNodeId::FromExpr(Aggregate);
		Record.actionKind = asAST_LIFETIME_ACTION_DESTROY_ELEMENT;
		Record.actionTarget =
			asSASTLifetimeNodeId::FromDecl(ElementDestructor);
		Record.activationPoint = asSASTLifetimeNodeId::FromExpr(Aggregate);
		Record.semanticRegion = asSASTLifetimeNodeId::FromDecl(Function);
		Record.phase = asAST_LIFETIME_PHASE_AGGREGATE_ELEMENT;
		Record.supportedExitMask = asAST_LIFETIME_EXIT_EXCEPTION;
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Record)));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("the Sema-authored lifetime fixture must publish")));

		asSASTSidecarHeader Header;
		Header.functionKey = "LifetimeSidecar.BuildArray";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes)));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(
				Restored, Header, Bytes.AddressOf(), Bytes.GetLength())));
		ASSERT_THAT(AreEqual(
			Context.GetLifetimeProtocolRevision(),
			Restored.GetLifetimeProtocolRevision(),
			TEXT("the durable lifetime protocol revision must round-trip")));
		ASSERT_THAT(AreEqual(
			Context.GetLifetimeRecordCount(),
			Restored.GetLifetimeRecordCount(),
			TEXT("Sidecar restore must not erase Sema-authored lifetime facts")));
		ASSERT_THAT(AreEqual(
			Context.GetLifetimeProtocolHash(),
			Restored.GetLifetimeProtocolHash(),
			TEXT("the typed lifetime protocol identity must round-trip exactly")));
		ASSERT_THAT(IsNotNull(Restored.GetLifetimeRecord(0)));
		if (Restored.GetLifetimeRecord(0) != nullptr)
		{
			ASSERT_THAT(IsTrue(
				*Restored.GetLifetimeRecord(0) == Record,
				TEXT("every pointer-free lifetime field must round-trip")));
		}

		asCArray<asBYTE> CorruptBytes = Bytes;
		CorruptBytes[CorruptBytes.GetLength() - 1] = 0xff;
		asCASTContext CorruptRestore;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTDecodeSidecar(
				CorruptRestore,
				Header,
				CorruptBytes.AddressOf(),
				CorruptBytes.GetLength()),
			TEXT("a forged lifetime commit flag must fail closed")));
		ASSERT_THAT(IsFalse(CorruptRestore.IsPublishable(),
			TEXT("malformed lifetime bytes must not publish a partial snapshot")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			CorruptRestore.GetLifetimeRecordCount(),
			TEXT("failed decode must abandon all candidate lifetime records")));
	}

	TEST_METHOD(SidecarRoundTripRestoresSourceFunctionBodyAndExpression)
	{
		const char Source[] = "int F() { return 7; }";
		asCASTContext Context;
		const asASTFileID File = Context.AddSourceSection(
			"CacheBody.as",
			asAST_SOURCE_AUTHORED,
			Source,
			sizeof(Source) - 1,
			3);
		ASSERT_THAT(IsTrue(File != 0, TEXT("source section should be owned by the sidecar snapshot")));
		const asCSourceManager& Sources = Context.GetSourceManager();
		const asCSourceRange FunctionRange = Sources.MakeRange(File, 0, sizeof(Source) - 1);
		const asCSourceRange ReturnRange = Sources.MakeRange(File, 10, 19);
		const asCSourceRange LiteralRange = Sources.MakeRange(File, 17, 18);

		const asASTDeclId Tu = Context.CreateTranslationUnit("CacheBody");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Function = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, FunctionRange, "F");
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(Function, "F()"), TEXT("function identity should include its signature")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Function, IntType), TEXT("function return type")));
		const asASTStmtId Return = Context.CreateStmt(asAST_STMT_RETURN, Function, ReturnRange);
		ASSERT_THAT(AreEqual(0, Context.SetBody(Function, Return), TEXT("function body")));
		const asASTExprId Literal = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, LiteralRange);
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Literal, "7"), TEXT("return literal")));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Return, Literal), TEXT("return expression")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("complete sidecar input should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "F()";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode complete body sidecar")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode must rebuild a sealed AST without re-running Sema")));
		ASSERT_THAT(AreEqual((asUINT)1, Restored.GetSourceManager().GetSectionCount(),
			TEXT("source model survives sidecar round-trip")));
		ASSERT_THAT(IsTrue(asCString(Restored.GetSourceManager().GetLogicalKey(1)).Equals("CacheBody.as"),
			TEXT("restored source keeps its logical identity")));
		ASSERT_THAT(AreEqual(3, Restored.GetSourceManager().GetLineOffset(1),
			TEXT("restored source keeps its line offset")));
		ASSERT_THAT(AreEqual((asUINT)2, Restored.GetDeclCount(), TEXT("translation unit and function restore")));
		ASSERT_THAT(AreEqual((asUINT)1, Restored.GetStmtCount(), TEXT("function body statement restores")));
		ASSERT_THAT(AreEqual((asUINT)1, Restored.GetExprCount(), TEXT("function body expression restores")));

		const asCDecl* RestoredFunction = Restored.GetDecl(asASTDeclId(2));
		ASSERT_THAT(IsNotNull(RestoredFunction, TEXT("function declaration restores")));
		ASSERT_THAT(IsTrue(RestoredFunction->stableKey.Equals("F()"), TEXT("function stable key restores")));
		ASSERT_THAT(IsTrue(RestoredFunction->body.IsValid(), TEXT("function retains its body link")));
		ASSERT_THAT(IsTrue(Restored.GetSourceManager().IsValidRange(RestoredFunction->range),
			TEXT("function source range remaps into the restored source model")));

		const asCStmt* RestoredReturn = Restored.GetStmt(RestoredFunction->body);
		ASSERT_THAT(IsNotNull(RestoredReturn, TEXT("return statement restores")));
		ASSERT_THAT(AreEqual((int)asAST_STMT_RETURN, (int)RestoredReturn->kind, TEXT("return statement kind restores")));
		const asCExpr* RestoredLiteral = Restored.GetExpr(RestoredReturn->expr);
		ASSERT_THAT(IsNotNull(RestoredLiteral, TEXT("return literal restores")));
		ASSERT_THAT(AreEqual((int)asAST_EXPR_INTEGER_LITERAL, (int)RestoredLiteral->kind, TEXT("literal expression kind restores")));
		ASSERT_THAT(IsTrue(RestoredLiteral->literal.Equals("7"), TEXT("literal payload restores")));
	}

	TEST_METHOD(SidecarRoundTripPreservesCompleteSourceModel)
	{
		struct FSourceFixture
		{
			const char* LogicalKey;
			asEASTSourceOrigin Origin;
			const char* Bytes;
			int LineOffset;
		};

		const FSourceFixture Fixtures[] = {
			{ "/Angelscript/Game/Script/Authored.as", asAST_SOURCE_AUTHORED, "int Authored;\n", 2 },
			{ "/Angelscript/Game/Script/Processed.as", asAST_SOURCE_PROCESSED, "int Processed;\n", 5 },
			{ "<generated:default:SourceModel>", asAST_SOURCE_GENERATED, "void Generated() {}\n", 0 },
		};

		asCASTContext Context;
		for (const FSourceFixture& Fixture : Fixtures)
		{
			ASSERT_THAT(IsTrue(Context.AddSourceSection(
				Fixture.LogicalKey,
				Fixture.Origin,
				Fixture.Bytes,
				std::strlen(Fixture.Bytes),
				Fixture.LineOffset) != 0,
				TEXT("every source provenance class must be owned by the sidecar input")));
		}
		Context.CreateTranslationUnit("SourceModel");
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("source-model sidecar input should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "SourceModel";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode complete source model")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode complete source model without re-running Sema")));
		const asCSourceManager& RestoredSources = Restored.GetSourceManager();
		ASSERT_THAT(AreEqual((asUINT)UE_ARRAY_COUNT(Fixtures), RestoredSources.GetSectionCount(),
			TEXT("all source records must survive sidecar round-trip")));

		for (asUINT Index = 0; Index < UE_ARRAY_COUNT(Fixtures); ++Index)
		{
			const FSourceFixture& Fixture = Fixtures[Index];
			const asASTFileID File = RestoredSources.GetFileIDByIndex(Index);
			ASSERT_THAT(IsTrue(FCStringAnsi::Strcmp(
				RestoredSources.GetLogicalKey(File), Fixture.LogicalKey) == 0,
				TEXT("restored source must preserve its logical key")));
			ASSERT_THAT(AreEqual((int)Fixture.Origin, (int)RestoredSources.GetOrigin(File),
				TEXT("restored source must preserve its provenance class")));
			ASSERT_THAT(AreEqual(Fixture.LineOffset, RestoredSources.GetLineOffset(File),
				TEXT("restored source must preserve its line offset")));
			ASSERT_THAT(AreEqual((asUINT)std::strlen(Fixture.Bytes), RestoredSources.GetBufferLength(File),
				TEXT("restored source must preserve its byte length")));
			ASSERT_THAT(IsTrue(std::memcmp(
				RestoredSources.GetBuffer(File),
				Fixture.Bytes,
				std::strlen(Fixture.Bytes)) == 0,
				TEXT("restored source must preserve its exact bytes")));
		}
	}

	TEST_METHOD(SidecarRoundTripPreservesNeutralSourceProvenance)
	{
		const char* const Processed = "int __GeneratedAsset = 1;";
		asSSourceProvenanceRange Provenance;
		Provenance.processedOffset = 4;
		Provenance.processedLength = 16;
		Provenance.hasAuthoredOrigin = true;
		Provenance.authoredOrigin.section =
			"/Angelscript/Game/AuthoredAsset.as";
		Provenance.authoredOrigin.sourceOffset = 31;
		Provenance.authoredOrigin.sourceLength = 12;
		Provenance.authoredOrigin.row = 7;
		Provenance.authoredOrigin.column = 4;
		Provenance.hasGeneratedOrigin = true;
		Provenance.generatedOrigin.kind = asEGeneratedSourceKind::LiteralAsset;
		Provenance.generatedOrigin.generatorName = "LiteralAsset:GeneratedAsset";
		Provenance.generatedOrigin.hasAuthoredAnchor = true;
		Provenance.generatedOrigin.authoredAnchor = Provenance.authoredOrigin;

		asCASTContext Context;
		const asASTFileID File = Context.AddSourceSection(
			"/Angelscript/Game/ProcessedAsset.as",
			asAST_SOURCE_PROCESSED,
			Processed,
			std::strlen(Processed),
			0,
			&Provenance,
			1);
		ASSERT_THAT(IsTrue(File != 0));
		Context.CreateTranslationUnit("SourceProvenanceSidecar");
		ASSERT_THAT(AreEqual(0, Context.Seal()));

		asSASTSidecarHeader Header;
		Header.functionKey = "SourceProvenanceSidecar";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode snapshot-owned source provenance")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(
				Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode source provenance without re-running preprocessing or Sema")));
		asSSourceSpan RestoredSpan;
		ASSERT_THAT(IsTrue(Restored.GetSourceManager().ResolveSourceSpan(
			Restored.GetSourceManager().MakeRange(1, 4, 20),
			RestoredSpan)));
		ASSERT_THAT(IsTrue(RestoredSpan.hasAuthoredOrigin,
			TEXT("Cache restore must preserve authored diagnostic provenance")));
		ASSERT_THAT(IsTrue(RestoredSpan.authoredOrigin.section
			== "/Angelscript/Game/AuthoredAsset.as"));
		ASSERT_THAT(IsTrue(RestoredSpan.hasGeneratedOrigin,
			TEXT("Cache restore must preserve generated diagnostic provenance")));
		ASSERT_THAT(IsTrue(RestoredSpan.generatedOrigin.kind
			== asEGeneratedSourceKind::LiteralAsset));
		ASSERT_THAT(IsTrue(RestoredSpan.generatedOrigin.generatorName
			== "LiteralAsset:GeneratedAsset"));
		ASSERT_THAT(IsTrue(RestoredSpan.generatedOrigin.hasAuthoredAnchor));
	}

	TEST_METHOD(SourceProvenanceIsDiagnosticOnlyForFunctionRecordIdentity)
	{
		auto BuildContext = [](
			asCASTContext& Context,
			const char* AuthoredSection) -> int
		{
			const char* const Source = "int F() { return 7; }";
			asSSourceProvenanceRange Provenance;
			Provenance.processedOffset = 0;
			Provenance.processedLength = (asUINT)std::strlen(Source);
			Provenance.hasAuthoredOrigin = true;
			Provenance.authoredOrigin.section = AuthoredSection;
			Provenance.authoredOrigin.sourceOffset = 11;
			Provenance.authoredOrigin.sourceLength = 8;
			Provenance.authoredOrigin.row = 3;
			Provenance.authoredOrigin.column = 2;

			const asASTFileID File = Context.AddSourceSection(
				"/Angelscript/Game/ProcessedIdentity.as",
				asAST_SOURCE_PROCESSED,
				Source,
				std::strlen(Source),
				0,
				&Provenance,
				1);
			if (File == 0)
			{
				return -1;
			}
			const asCSourceRange Range = Context.GetSourceManager().MakeRange(
				File, 0, (asUINT)std::strlen(Source));
			const asASTDeclId Tu = Context.CreateTranslationUnit("IdentityModule");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTDeclId Function = Context.CreateDecl(
				asAST_DECL_FUNCTION, Tu, Range, "F");
			Context.SetStableKey(Function, "F()");
			Context.SetDeclType(Function, IntType);
			const asASTStmtId Return = Context.CreateStmt(
				asAST_STMT_RETURN, Function, Range);
			Context.SetBody(Function, Return);
			const asASTExprId Value = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL,
				IntType,
				asAST_VALUE_PRVALUE,
				Range);
			Context.SetLiteral(Value, "7");
			Context.SetStmtExpr(Return, Value);
			return Context.Seal();
		};

		asCASTContext First;
		asCASTContext Second;
		ASSERT_THAT(AreEqual(0,
			BuildContext(First, "/Angelscript/Game/FirstAuthored.as")));
		ASSERT_THAT(AreEqual(0,
			BuildContext(Second, "/Angelscript/Game/SecondAuthored.as")));

		asCArray<asSASTSidecarRecordId> FirstRecords;
		asCArray<asSASTSidecarRecordId> SecondRecords;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTCollectFunctionRecords(First, "Win64", FirstRecords)));
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTCollectFunctionRecords(Second, "Win64", SecondRecords)));
		ASSERT_THAT(AreEqual((asUINT)1, FirstRecords.GetLength()));
		ASSERT_THAT(AreEqual((asUINT)1, SecondRecords.GetLength()));
		ASSERT_THAT(IsTrue(FirstRecords[0].contentHash.Equals(
			SecondRecords[0].contentHash),
			TEXT("authored/generated diagnostic provenance must not alter semantic function identity")));
	}

	TEST_METHOD(MalformedSourceProvenanceFailsClosed)
	{
		const char* const Source = "int Generated = 1;";
		const char* const LogicalKey = "/Angelscript/Game/MalformedProvenance.as";
		asSSourceProvenanceRange Provenance;
		Provenance.processedOffset = 0;
		Provenance.processedLength = (asUINT)std::strlen(Source);
		Provenance.hasAuthoredOrigin = true;
		Provenance.authoredOrigin.section = "/Angelscript/Game/Authored.as";

		asCASTContext Context;
		ASSERT_THAT(IsTrue(Context.AddSourceSection(
			LogicalKey,
			asAST_SOURCE_PROCESSED,
			Source,
			std::strlen(Source),
			0,
			&Provenance,
			1) != 0));
		Context.CreateTranslationUnit("MalformedProvenance");
		ASSERT_THAT(AreEqual(0, Context.Seal()));

		asSASTSidecarHeader Header;
		Header.functionKey = "MalformedProvenance";
		Header.profile = "Win64";
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes)));

		const asUINT ProvenanceOffset = 8
			+ 4 + Header.functionKey.GetLength()
			+ 4 + Header.profile.GetLength()
			+ 4
			+ 4 + (asUINT)std::strlen(LogicalKey)
			+ 4
			+ 4
			+ 4 + (asUINT)std::strlen(Source)
			+ 4;
		const asUINT HasAuthoredOriginOffset = ProvenanceOffset + 8;
		ASSERT_THAT(IsTrue(HasAuthoredOriginOffset < Bytes.GetLength()));
		Bytes[HasAuthoredOriginOffset] = 2;

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTDecodeSidecar(
				Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("invalid provenance presence flag must fail closed")));
		ASSERT_THAT(IsFalse(Restored.IsPublishable()));
		ASSERT_THAT(AreEqual((asUINT)0,
			Restored.GetSourceManager().GetSectionCount(),
			TEXT("failed provenance decode must not publish a partial source model")));
	}

	TEST_METHOD(SidecarRoundTripPreservesSealedCallDispatch)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("CallDispatchCache");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCQualType OwnerType = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT, "DispatchOwner", 0);
		const asASTDeclId Owner = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "DispatchOwner");
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(Owner, "DispatchOwner")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Owner, OwnerType)));

		const asASTDeclId VirtualTarget = Context.CreateDecl(
			asAST_DECL_METHOD, Owner, asCSourceRange(), "VirtualTarget");
		const asASTDeclId DirectTarget = Context.CreateDecl(
			asAST_DECL_METHOD, Owner, asCSourceRange(), "DirectTarget");
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(
			VirtualTarget, "DispatchOwner::VirtualTarget()")));
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(
			DirectTarget, "DispatchOwner::DirectTarget()")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(VirtualTarget, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(DirectTarget, IntType)));

		const asASTDeclId Caller = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "Caller");
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(Caller, "Caller()")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Caller, IntType)));
		const asASTStmtId Body = Context.CreateStmt(
			asAST_STMT_BLOCK, Caller, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetBody(Caller, Body)));

		const asASTExprId VirtualCall = Context.CreateExpr(
			asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(VirtualCall, VirtualTarget)));
		ASSERT_THAT(AreEqual(0, Context.SetExprSafePointRole(
			VirtualCall, asAST_SAFEPOINT_CALL)));
		ASSERT_THAT(AreEqual(0, Context.SetExprCallDispatch(
			VirtualCall, asAST_CALL_DISPATCH_VIRTUAL)));
		const asASTStmtId VirtualStatement = Context.CreateStmt(
			asAST_STMT_EXPR, Caller, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(VirtualStatement, VirtualCall)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Body, VirtualStatement)));

		const asASTExprId DirectCall = Context.CreateExpr(
			asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(DirectCall, DirectTarget)));
		ASSERT_THAT(AreEqual(0, Context.SetExprSafePointRole(
			DirectCall, asAST_SAFEPOINT_CALL)));
		ASSERT_THAT(AreEqual(0, Context.SetExprCallDispatch(
			DirectCall, asAST_CALL_DISPATCH_DIRECT)));
		const asASTStmtId DirectStatement = Context.CreateStmt(
			asAST_STMT_EXPR, Caller, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(DirectStatement, DirectCall)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Body, DirectStatement)));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("call-dispatch sidecar input must be a sealed semantic graph")));

		asSASTSidecarHeader Header;
		Header.functionKey = "Caller()";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode sealed call-dispatch facts")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(
				Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode call-dispatch facts without re-running Sema")));
		const asCExpr* const RestoredVirtual = Restored.GetExpr(VirtualCall);
		const asCExpr* const RestoredDirect = Restored.GetExpr(DirectCall);
		ASSERT_THAT(IsTrue(
			RestoredVirtual != nullptr
				&& RestoredVirtual->callDispatch == asAST_CALL_DISPATCH_VIRTUAL,
			TEXT("sidecar must preserve ordinary virtual-eligible member dispatch")));
		ASSERT_THAT(IsTrue(
			RestoredDirect != nullptr
				&& RestoredDirect->callDispatch == asAST_CALL_DISPATCH_DIRECT,
			TEXT("sidecar must preserve qualified base direct dispatch")));
	}

	TEST_METHOD(SidecarRoundTripPreservesStringLiteralBytesAfterEmbeddedNull)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("StringLiteralCache");
		const asCQualType VoidType = Context.InternPrimitive(ttVoid, 0);
		const asCQualType StringType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FString", asAST_QUAL_CONST);
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "Entry");
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(Function, "Entry()"),
			TEXT("string sidecar function identity")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Function, VoidType),
			TEXT("string sidecar function return type")));
		const asASTStmtId Statement = Context.CreateStmt(
			asAST_STMT_EXPR, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetBody(Function, Statement),
			TEXT("string sidecar function body")));

		static const char ExpectedBytes[] = {
			'l', 'i', 'n', 'e', '\n', 'm', 'i', 'd', '\0', 't', 'a', 'i', 'l'
		};
		const asASTExprId Literal = Context.CreateExpr(
			asAST_EXPR_STRING_LITERAL, StringType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBytes(
			Literal, ExpectedBytes, static_cast<asUINT>(sizeof(ExpectedBytes))),
			TEXT("string sidecar semantic byte payload")));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(
			Literal, static_cast<asQWORD>(sizeof(ExpectedBytes))),
			TEXT("string sidecar exact byte length")));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Statement, Literal),
			TEXT("string sidecar expression statement")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("string sidecar input must seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "Entry()";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode exact string byte payload")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode exact string byte payload")));
		const asCExpr* const RestoredLiteral = Restored.GetExpr(asASTExprId(1));
		ASSERT_THAT(IsNotNull(RestoredLiteral, TEXT("restored string literal")));
		if (RestoredLiteral == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<size_t>(sizeof(ExpectedBytes)),
			RestoredLiteral->literal.GetLength(),
			TEXT("sidecar must not truncate the semantic payload at embedded NUL")));
		ASSERT_THAT(AreEqual(static_cast<asQWORD>(sizeof(ExpectedBytes)),
			RestoredLiteral->literalBits,
			TEXT("sidecar must preserve the exact StringLiteral byte length")));
		bool BytesMatch = RestoredLiteral->literal.GetLength() == sizeof(ExpectedBytes);
		for (size_t ByteIndex = 0; BytesMatch && ByteIndex < sizeof(ExpectedBytes); ++ByteIndex)
		{
			BytesMatch = RestoredLiteral->literal[ByteIndex] == ExpectedBytes[ByteIndex];
		}
		ASSERT_THAT(IsTrue(BytesMatch,
			TEXT("sidecar must preserve bytes after the embedded NUL")));
	}

	TEST_METHOD(SidecarRoundTripPreservesResolvedFloatWidthsAndConstantBits)
	{
		// A Cache V2 sidecar carries the already-resolved canonical AST.  It must
		// preserve both width tokens and raw constant storage, never recover them
		// later from the source spelling `float`.
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("FloatCache");
		const asCQualType Float32 = Context.InternPrimitive(ttFloat32, asAST_QUAL_CONST);
		const asCQualType Float64 = Context.InternPrimitive(ttFloat64, asAST_QUAL_CONST);
		const asASTDeclId Narrow = Context.CreateDecl(asAST_DECL_VAR, Tu, asCSourceRange(), "Narrow");
		const asASTDeclId Wide = Context.CreateDecl(asAST_DECL_VAR, Tu, asCSourceRange(), "Wide");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Narrow, Float32), TEXT("float32 declaration type")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Wide, Float64), TEXT("float64 declaration type")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclConstantValue(Narrow, 0x40500000ull),
			TEXT("float32 constant bits")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclConstantValue(Wide, 0x4012000000000000ull),
			TEXT("float64 constant bits")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("resolved float-width sidecar input should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "FloatCache.F";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode float-width Cache V2 sidecar")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode float-width Cache V2 sidecar without re-Sema")));
		const asCDecl* const RestoredNarrow = Restored.GetDecl(Narrow);
		const asCDecl* const RestoredWide = Restored.GetDecl(Wide);
		ASSERT_THAT(IsNotNull(RestoredNarrow, TEXT("float32 declaration restores")));
		ASSERT_THAT(IsNotNull(RestoredWide, TEXT("float64 declaration restores")));
		if (RestoredNarrow != nullptr)
		{
			const asCType* const NarrowType = Restored.GetType(RestoredNarrow->type.type);
			ASSERT_THAT(IsTrue(NarrowType != nullptr && NarrowType->primitiveToken == ttFloat32,
				TEXT("Cache V2 must preserve resolved float32 rather than a source spelling")));
			ASSERT_THAT(IsTrue(RestoredNarrow->hasConstantValue && RestoredNarrow->constantValue == 0x40500000ull,
				TEXT("Cache V2 must preserve float32 raw storage bits")));
		}
		if (RestoredWide != nullptr)
		{
			const asCType* const WideType = Restored.GetType(RestoredWide->type.type);
			ASSERT_THAT(IsTrue(WideType != nullptr && WideType->primitiveToken == ttFloat64,
				TEXT("Cache V2 must preserve resolved float64 rather than a source spelling")));
			ASSERT_THAT(IsTrue(RestoredWide->hasConstantValue && RestoredWide->constantValue == 0x4012000000000000ull,
				TEXT("Cache V2 must preserve float64 raw storage bits")));
		}
	}

	TEST_METHOD(SidecarRoundTripPreservesMixedWidthComparisonNormalization)
	{
		// Cache V2 persists the *sealed* semantic graph.  This graph is the
		// post-Sema form of `float64Value > 0.5f`: the literal remains float32
		// beneath a widening conversion, both comparison operands are float64,
		// and the operation itself is bool.
		asCASTContext Context;
		Context.CreateTranslationUnit("FloatComparisonCache");
		const asCQualType Float32 = Context.InternPrimitive(ttFloat32, 0);
		const asCQualType Float64 = Context.InternPrimitive(ttFloat64, 0);
		const asCQualType Bool = Context.InternPrimitive(ttBool, 0);
		const asASTExprId Left = Context.CreateExpr(asAST_EXPR_FLOAT_LITERAL, Float64, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId OriginalRight = Context.CreateExpr(asAST_EXPR_FLOAT_LITERAL, Float32, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId WidenedRight = Context.CreateExpr(asAST_EXPR_CONVERSION, Float64, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId Comparison = Context.CreateExpr(asAST_EXPR_BINARY, Bool, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Left, "2.0"), TEXT("float64 comparison left literal")));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(OriginalRight, "0.5f"), TEXT("float32 comparison source literal")));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(WidenedRight, "conv"), TEXT("widening conversion marker")));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Comparison, ">"), TEXT("comparison operator")));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(WidenedRight, OriginalRight), TEXT("conversion owns float32 literal")));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Comparison, Left), TEXT("comparison owns float64 left operand")));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Comparison, WidenedRight), TEXT("comparison owns widened right operand")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("mixed-width comparison sidecar input should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "FloatComparisonCache.F";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode normalized mixed-width comparison sidecar")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode normalized comparison without recomputing Sema")));
		const asCExpr* const RestoredComparison = Restored.GetExpr(Comparison);
		ASSERT_THAT(IsNotNull(RestoredComparison, TEXT("normalized comparison restores")));
		if (RestoredComparison == nullptr || RestoredComparison->children.GetLength() != 2)
		{
			return;
		}
		const asCExpr* const RestoredLeft = Restored.GetExpr(RestoredComparison->children[0]);
		const asCExpr* const RestoredWidenedRight = Restored.GetExpr(RestoredComparison->children[1]);
		const asCType* const RestoredResultType = Restored.GetType(RestoredComparison->type.type);
		const asCType* const RestoredLeftType = RestoredLeft != nullptr ? Restored.GetType(RestoredLeft->type.type) : nullptr;
		const asCType* const RestoredRightType = RestoredWidenedRight != nullptr ? Restored.GetType(RestoredWidenedRight->type.type) : nullptr;
		ASSERT_THAT(IsTrue(RestoredResultType != nullptr && RestoredResultType->primitiveToken == ttBool,
			TEXT("Cache V2 preserves bool comparison result")));
		ASSERT_THAT(IsTrue(RestoredLeftType != nullptr && RestoredLeftType->primitiveToken == ttFloat64,
			TEXT("Cache V2 preserves normalized float64 left operand")));
		ASSERT_THAT(IsTrue(RestoredWidenedRight != nullptr && RestoredWidenedRight->kind == asAST_EXPR_CONVERSION,
			TEXT("Cache V2 preserves explicit widening conversion")));
		ASSERT_THAT(IsTrue(RestoredRightType != nullptr && RestoredRightType->primitiveToken == ttFloat64,
			TEXT("Cache V2 preserves normalized float64 right operand")));
		if (RestoredWidenedRight != nullptr && RestoredWidenedRight->children.GetLength() == 1)
		{
			const asCExpr* const RestoredOriginalRight = Restored.GetExpr(RestoredWidenedRight->children[0]);
			const asCType* const RestoredOriginalRightType = RestoredOriginalRight != nullptr
				? Restored.GetType(RestoredOriginalRight->type.type)
				: nullptr;
			ASSERT_THAT(IsTrue(RestoredOriginalRight != nullptr && RestoredOriginalRight->kind == asAST_EXPR_FLOAT_LITERAL,
				TEXT("Cache V2 keeps the original literal beneath conversion")));
			ASSERT_THAT(IsTrue(RestoredOriginalRightType != nullptr && RestoredOriginalRightType->primitiveToken == ttFloat32,
				TEXT("Cache V2 keeps the float32 source-literal type")));
		}
	}

	TEST_METHOD(UnsupportedSidecarVersionIsRejectedWithoutHirBytes)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		Sema.ActOnTranslationUnit("Ver");
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("version sample should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "Ver.F";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("current sidecar version should encode")));
		ASSERT_THAT(IsTrue(Bytes.GetLength() >= 8, TEXT("sidecar has magic+version")));
		const asUINT PreviousSchema = asAST_SIDECAR_SCHEMA_VERSION - 1;
		Bytes[4] = static_cast<asBYTE>(PreviousSchema & 0xff);
		Bytes[5] = static_cast<asBYTE>((PreviousSchema >> 8) & 0xff);
		Bytes[6] = static_cast<asBYTE>((PreviousSchema >> 16) & 0xff);
		Bytes[7] = static_cast<asBYTE>((PreviousSchema >> 24) & 0xff);

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNKNOWN_KIND,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("the previous V5 sidecar schema must fail closed after the V6 bump")));
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNKNOWN_KIND,
			asCASTRejectNonSidecarInput(Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("reject helper must not reinterpret the previous schema as current AST input")));

		const asBYTE HirDump[] = { '.', 'h', 'i', 'r', ' ', 'x' };
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNSUPPORTED_INPUT,
			asCASTRejectNonSidecarInput(HirDump, 6),
			TEXT("HIR dump bytes are not a Cache V2 VM/AST payload")));
	}

	TEST_METHOD(SidecarRoundTripPreservesAccessSpecifierPermissionAndMemberEdge)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("AccessSidecar");
		const asASTDeclId Record = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "Vault");
		const asASTDeclId Specifier = Context.CreateDecl(
			asAST_DECL_ACCESS_SPECIFIER, Record, asCSourceRange(), "Internal");
		const asASTDeclId Permission = Context.CreateDecl(
			asAST_DECL_ACCESS_PERMISSION, Specifier, asCSourceRange(), "*");
		const asASTDeclId Field = Context.CreateDecl(
			asAST_DECL_VAR, Record, asCSourceRange(), "Value");
		ASSERT_THAT(AreEqual(0, Context.SetDeclTraits(
			Specifier, asAST_TRAIT_PRIVATE), TEXT("access base trait")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclTraits(
			Permission,
			asAST_TRAIT_ACCESS_WILDCARD
				| asAST_TRAIT_ACCESS_READONLY
				| asAST_TRAIT_ACCESS_EDITDEFAULTS),
			TEXT("access permission traits")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclAccessSpecifier(Field, Specifier),
			TEXT("member-to-access edge")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("access metadata sidecar graph should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "AccessSidecar.Vault";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode access metadata sidecar")));

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode access metadata without re-Sema")));
		const asCDecl* const RestoredSpecifier = Restored.GetDecl(Specifier);
		const asCDecl* const RestoredPermission = Restored.GetDecl(Permission);
		const asCDecl* const RestoredField = Restored.GetDecl(Field);
		ASSERT_THAT(IsTrue(RestoredSpecifier != nullptr
			&& RestoredSpecifier->kind == asAST_DECL_ACCESS_SPECIFIER
			&& (RestoredSpecifier->traits & asAST_TRAIT_PRIVATE) != 0,
			TEXT("sidecar preserves access specifier declaration and base")));
		ASSERT_THAT(IsTrue(RestoredPermission != nullptr
			&& RestoredPermission->kind == asAST_DECL_ACCESS_PERMISSION
			&& (RestoredPermission->traits & asAST_TRAIT_ACCESS_WILDCARD) != 0
			&& (RestoredPermission->traits & asAST_TRAIT_ACCESS_READONLY) != 0
			&& (RestoredPermission->traits & asAST_TRAIT_ACCESS_EDITDEFAULTS) != 0,
			TEXT("sidecar preserves permission flags")));
		ASSERT_THAT(IsTrue(RestoredField != nullptr
			&& RestoredField->accessSpecifier.value == Specifier.value,
			TEXT("sidecar preserves the exact member-to-access DeclId edge")));
	}

	TEST_METHOD(SidecarHeaderInspectionExposesOwnerAndProfileWithoutWildcardRestore)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		Sema.ActOnTranslationUnit("Inspect");
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("inspection sample should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "module:abc123";
		Header.profile = "profile:def456";
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode inspectable V2 sidecar")));

		asSASTSidecarHeader Inspected;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTReadSidecarHeader(Bytes.AddressOf(), Bytes.GetLength(), Inspected),
			TEXT("inspect V2 header without reconstructing a graph")));
		ASSERT_THAT(IsTrue(Inspected.functionKey.Equals("module:abc123"),
			TEXT("inspection exposes the durable owner key")));
		ASSERT_THAT(IsTrue(Inspected.profile.Equals("profile:def456"),
			TEXT("inspection exposes the exact target profile")));
		ASSERT_THAT(AreEqual(asAST_SIDECAR_SCHEMA_VERSION, Inspected.version,
			TEXT("inspection reports the accepted schema version")));

		const asBYTE Truncated[] = {
			'C', 'A', 'S', 'T',
			static_cast<asBYTE>(asAST_SIDECAR_SCHEMA_VERSION & 0xff),
			static_cast<asBYTE>((asAST_SIDECAR_SCHEMA_VERSION >> 8) & 0xff),
			static_cast<asBYTE>((asAST_SIDECAR_SCHEMA_VERSION >> 16) & 0xff),
			static_cast<asBYTE>((asAST_SIDECAR_SCHEMA_VERSION >> 24) & 0xff),
		};
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTReadSidecarHeader(Truncated, 8, Inspected),
			TEXT("truncated owner/profile data is a safe miss")));
	}

	TEST_METHOD(EnvelopeUsesASTBodySidecarKindWithoutFunctionBodyBytes)
	{
		TArray<uint8> Payload = {1, 2, 3, 4};
		TArray<uint8> Envelope;
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
				EAngelscriptCacheRecordKind::ASTBodySidecar,
				Payload,
				Envelope);
		ASSERT_THAT(IsTrue(Result.IsSuccess(), TEXT("ASTBodySidecar envelope should serialize")));
		ASSERT_THAT(IsTrue(Envelope.Num() > Payload.Num(), TEXT("envelope wraps payload")));
		ASSERT_THAT(AreEqual(static_cast<uint8>(EAngelscriptCacheRecordKind::ASTBodySidecar), Envelope[12],
			TEXT("record kind byte is ASTBodySidecar")));
	}

	TEST_METHOD(RuntimeWrapperRejectsInvalidPayloadAndPreservesVerifiedCurrentDto)
	{
		FAngelscriptCacheASTBodySidecar Sidecar;
		Sidecar.FunctionKey = TEXT("Mod.F");
		TArray<uint8> Bytes;
		const FAngelscriptCacheASTBodySidecarResult Empty =
			AngelscriptEncodeASTBodySidecar(Sidecar, Bytes);
		ASSERT_THAT(IsTrue(Empty.Error == EAngelscriptCacheASTBodySidecarError::EmptyPayloadNotAbsence,
			TEXT("discard/empty sidecar cannot be treated as a body")));

		Sidecar.CanonicalAstBytes = {1};
		const FAngelscriptCacheASTBodySidecarResult Invalid =
			AngelscriptEncodeASTBodySidecar(Sidecar, Bytes);
		ASSERT_THAT(IsTrue(Invalid.Error == EAngelscriptCacheASTBodySidecarError::Malformed,
			TEXT("the runtime wrapper must reject an arbitrary non-empty placeholder payload")));
		ASSERT_THAT(AreEqual(0, Bytes.Num(), TEXT("failed validation must not publish replacement bytes")));

		asCASTContext PayloadContext;
		asCSema PayloadSema(nullptr, PayloadContext);
		PayloadSema.ActOnTranslationUnit("Mod");
		ASSERT_THAT(AreEqual(0, PayloadContext.Seal(), TEXT("wrapper test payload must seal")));
		asSASTSidecarHeader Header;
		Header.functionKey = "Mod.F";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> ForkBytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(PayloadContext, Header, ForkBytes),
			TEXT("construct an independently verified current-schema sidecar")));
		Sidecar.CanonicalAstBytes.Reset();
		for (asUINT Index = 0; Index < ForkBytes.GetLength(); ++Index)
		{
			Sidecar.CanonicalAstBytes.Add(ForkBytes[Index]);
		}

		const FAngelscriptCacheASTBodySidecarResult EmptyProfileEncode =
			AngelscriptEncodeASTBodySidecar(Sidecar, Bytes);
		ASSERT_THAT(IsTrue(EmptyProfileEncode.Error == EAngelscriptCacheASTBodySidecarError::ProfileMismatch,
			TEXT("runtime encode must not treat an empty Sidecar.Profile as a wildcard")));
		ASSERT_THAT(AreEqual(0, Bytes.Num(), TEXT("empty-profile encode must not publish replacement bytes")));

		Sidecar.Profile = TEXT("Win64");
		const FAngelscriptCacheASTBodySidecarResult Encoded =
			AngelscriptEncodeASTBodySidecar(Sidecar, Bytes);
		ASSERT_THAT(IsTrue(Encoded.IsSuccess(), TEXT("verified current-schema sidecar encodes")));
		ASSERT_THAT(IsTrue(Bytes == Sidecar.CanonicalAstBytes,
			TEXT("the runtime wrapper must preserve, rather than replace, the verified DTO")));

		FAngelscriptCacheASTBodySidecar WildcardDecoded;
		const FAngelscriptCacheASTBodySidecarResult WildcardRoundTrip =
			AngelscriptDecodeASTBodySidecar(Bytes, TEXT("Mod.F"), TEXT(""), WildcardDecoded);
		ASSERT_THAT(IsTrue(WildcardRoundTrip.Error == EAngelscriptCacheASTBodySidecarError::ProfileMismatch,
			TEXT("runtime decode must not treat an empty expected profile as a wildcard")));

		FAngelscriptCacheASTBodySidecar Decoded;
		const FAngelscriptCacheASTBodySidecarResult RoundTrip =
			AngelscriptDecodeASTBodySidecar(Bytes, TEXT("Mod.F"), TEXT("Win64"), Decoded);
		ASSERT_THAT(IsTrue(RoundTrip.IsSuccess(), TEXT("round-trip decode")));
		ASSERT_THAT(IsTrue(Decoded.FunctionKey == TEXT("Mod.F"), TEXT("function key remaps without Engine pointers")));
		ASSERT_THAT(IsTrue(Decoded.Profile == TEXT("Win64"), TEXT("exact profile remaps without Engine pointers")));

		TArray<uint8> Truncated = Bytes;
		Truncated.Pop(EAllowShrinking::No);
		FAngelscriptCacheASTBodySidecar Inspected;
		const FAngelscriptCacheASTBodySidecarResult HeaderOnly =
			AngelscriptInspectASTBodySidecar(Truncated, Inspected);
		ASSERT_THAT(IsTrue(HeaderOnly.IsSuccess(),
			TEXT("discard-policy admission should inspect the bounded envelope/header without rebuilding the optional AST graph")));
		ASSERT_THAT(IsTrue(Inspected.FunctionKey == TEXT("Mod.F"),
			TEXT("header-only admission keeps the durable owner for graph validation")));
		ASSERT_THAT(IsTrue(Inspected.CanonicalAstBytes.IsEmpty(),
			TEXT("header-only admission must not retain a duplicate AST payload")));
		FAngelscriptCacheASTBodySidecar RejectedRetain;
		const FAngelscriptCacheASTBodySidecarResult FullDecode =
			AngelscriptDecodeASTBodySidecar(
				Truncated, TEXT("Mod.F"), TEXT("Win64"), RejectedRetain);
		ASSERT_THAT(IsFalse(FullDecode.IsSuccess(),
			TEXT("a retained snapshot must still strictly reject a body-truncated sidecar")));
	}

	TEST_METHOD(IncrementalRebuildsChangedFunctionReusesUnchangedAndStaysAtomic)
	{
		asCASTContext Previous;
		asCSema PrevSema(nullptr, Previous);
		const asASTDeclId PrevTu = PrevSema.ActOnTranslationUnit("IncMod");
		const asCQualType IntType = Previous.InternPrimitive(ttInt, 0);
		const asASTDeclId PrevF = PrevSema.ActOnFunctionDecl(PrevTu, "F", IntType, asCSourceRange());
		const asASTDeclId PrevG = PrevSema.ActOnFunctionDecl(PrevTu, "G", IntType, asCSourceRange());
		PrevSema.RecordDependency(PrevF, "Other");
		ASSERT_THAT(AreEqual(0, Previous.Seal(), TEXT("previous graph should seal")));

		asCArray<asSASTSidecarRecordId> PrevRecords;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTCollectFunctionRecords(Previous, "Win64", PrevRecords),
			TEXT("collect previous function records")));

		asCASTContext Current;
		asCSema CurrSema(nullptr, Current);
		const asASTDeclId CurrTu = CurrSema.ActOnTranslationUnit("IncMod");
		const asCQualType CurrInt = Current.InternPrimitive(ttInt, 0);
		const asASTDeclId CurrF = CurrSema.ActOnFunctionDecl(CurrTu, "F", CurrInt, asCSourceRange());
		const asASTDeclId CurrG = CurrSema.ActOnFunctionDecl(CurrTu, "G", Current.InternPrimitive(ttInt, asAST_QUAL_CONST), asCSourceRange());
		CurrSema.RecordDependency(CurrF, "Other");
		ASSERT_THAT(AreEqual(0, Current.Seal(), TEXT("current graph should seal")));

		asCArray<asSASTSidecarRecordId> CurrRecords;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTCollectFunctionRecords(Current, "Win64", CurrRecords),
			TEXT("collect current function records")));

		asCArray<asCString> Reused;
		asCArray<asCString> Rebuilt;
		asCArray<asCString> Retired;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTPlanIncrementalSidecars(PrevRecords, CurrRecords, Reused, Rebuilt, Retired),
			TEXT("incremental plan")));
		bool bReusedF = false;
		bool bRebuiltG = false;
		for (asUINT i = 0; i < Reused.GetLength(); ++i)
		{
			if (Reused[i].Equals("F()"))
			{
				bReusedF = true;
			}
		}
		for (asUINT i = 0; i < Rebuilt.GetLength(); ++i)
		{
			if (Rebuilt[i].Equals("G()"))
			{
				bRebuiltG = true;
			}
		}
		ASSERT_THAT(IsTrue(bReusedF, TEXT("unchanged F must reuse its record id")));
		ASSERT_THAT(IsTrue(bRebuiltG, TEXT("changed G must rebuild its FunctionBody/ASTBodySidecar")));
		ASSERT_THAT(AreEqual((asUINT)0, Retired.GetLength(), TEXT("no retirement on in-place body edit")));

		asCArray<asSASTSidecarRecordId> Live;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTCommitSidecarSet(Live, PrevRecords),
			TEXT("first activation")));
		asCArray<asSASTSidecarRecordId> Invalid = CurrRecords;
		Invalid[0].contentHash = "";
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED, asCASTCommitSidecarSet(Live, Invalid),
			TEXT("failed activation must not mutate the live set")));
		ASSERT_THAT(AreEqual(PrevRecords.GetLength(), Live.GetLength(),
			TEXT("atomic activation keeps the previous sidecar set")));
		(void)PrevG;
		(void)CurrG;
	}

	TEST_METHOD(DependencyChangeInvalidatesOnlyDependentClosure)
	{
		asCASTContext Previous;
		asCSema PrevSema(nullptr, Previous);
		const asASTDeclId PrevTu = PrevSema.ActOnTranslationUnit("DepMod");
		const asCQualType IntType = Previous.InternPrimitive(ttInt, 0);
		const asASTDeclId PrevF = PrevSema.ActOnFunctionDecl(PrevTu, "F", IntType, asCSourceRange());
		PrevSema.ActOnFunctionDecl(PrevTu, "H", IntType, asCSourceRange());
		PrevSema.RecordDependency(PrevF, "Other");
		ASSERT_THAT(AreEqual(0, Previous.Seal(), TEXT("previous dep graph should seal")));

		asCASTContext Current;
		asCSema CurrSema(nullptr, Current);
		const asASTDeclId CurrTu = CurrSema.ActOnTranslationUnit("DepMod");
		const asCQualType CurrInt = Current.InternPrimitive(ttInt, 0);
		const asASTDeclId CurrF = CurrSema.ActOnFunctionDecl(CurrTu, "F", CurrInt, asCSourceRange());
		CurrSema.ActOnFunctionDecl(CurrTu, "H", CurrInt, asCSourceRange());
		CurrSema.RecordDependency(CurrF, "OtherChanged");
		ASSERT_THAT(AreEqual(0, Current.Seal(), TEXT("current dep graph should seal")));

		asCArray<asSASTSidecarRecordId> PrevRecords;
		asCArray<asSASTSidecarRecordId> CurrRecords;
		asCASTCollectFunctionRecords(Previous, "Win64", PrevRecords);
		asCASTCollectFunctionRecords(Current, "Win64", CurrRecords);
		asCArray<asCString> Reused;
		asCArray<asCString> Rebuilt;
		asCArray<asCString> Retired;
		asCASTPlanIncrementalSidecars(PrevRecords, CurrRecords, Reused, Rebuilt, Retired);
		bool bReusedH = false;
		bool bRebuiltF = false;
		for (asUINT i = 0; i < Reused.GetLength(); ++i)
		{
			if (Reused[i].Equals("H()"))
			{
				bReusedH = true;
			}
		}
		for (asUINT i = 0; i < Rebuilt.GetLength(); ++i)
		{
			if (Rebuilt[i].Equals("F()"))
			{
				bRebuiltF = true;
			}
		}
		ASSERT_THAT(IsTrue(bReusedH, TEXT("H has no import/type dependency change")));
		ASSERT_THAT(IsTrue(bRebuiltF, TEXT("F's import/type dependency change invalidates its sidecar")));
	}

	TEST_METHOD(FunctionBodyAndProfileChangesInvalidateFunctionSidecarRecord)
	{
		auto BuildReturnLiteral = [](asCASTContext& Context, const char* Literal) -> int
		{
			const asASTDeclId Tu = Context.CreateTranslationUnit("BodyIdentity");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTDeclId Function = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
			Context.SetDeclType(Function, IntType);
			const asASTStmtId Return = Context.CreateStmt(asAST_STMT_RETURN, Function, asCSourceRange());
			Context.SetBody(Function, Return);
			const asASTExprId Value = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
			Context.SetLiteral(Value, Literal);
			Context.SetStmtExpr(Return, Value);
			return Context.Seal();
		};

		asCASTContext Previous;
		asCASTContext BodyChanged;
		ASSERT_THAT(AreEqual(0, BuildReturnLiteral(Previous, "7"), TEXT("previous body should seal")));
		ASSERT_THAT(AreEqual(0, BuildReturnLiteral(BodyChanged, "8"), TEXT("changed body should seal")));

		asCArray<asSASTSidecarRecordId> PreviousRecords;
		asCArray<asSASTSidecarRecordId> BodyChangedRecords;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTCollectFunctionRecords(Previous, "Win64", PreviousRecords),
			TEXT("collect previous body record")));
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTCollectFunctionRecords(BodyChanged, "Win64", BodyChangedRecords),
			TEXT("collect changed body record")));
		ASSERT_THAT(AreEqual((asUINT)1, PreviousRecords.GetLength(), TEXT("one previous function record")));
		ASSERT_THAT(AreEqual((asUINT)1, BodyChangedRecords.GetLength(), TEXT("one changed function record")));
		ASSERT_THAT(IsFalse(PreviousRecords[0].contentHash.Equals(BodyChangedRecords[0].contentHash),
			TEXT("a return-expression change must invalidate the same function's sidecar identity")));

		asCArray<asCString> Reused;
		asCArray<asCString> Rebuilt;
		asCArray<asCString> Retired;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTPlanIncrementalSidecars(PreviousRecords, BodyChangedRecords, Reused, Rebuilt, Retired),
			TEXT("plan body-change increment")));
		ASSERT_THAT(AreEqual((asUINT)0, Reused.GetLength(), TEXT("changed function body must never be reused")));
		ASSERT_THAT(AreEqual((asUINT)1, Rebuilt.GetLength(), TEXT("changed function body must rebuild exactly once")));

		asCArray<asSASTSidecarRecordId> OtherProfileRecords;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTCollectFunctionRecords(Previous, "Android", OtherProfileRecords),
			TEXT("collect profile-specific record")));
		ASSERT_THAT(AreEqual((asUINT)1, OtherProfileRecords.GetLength(), TEXT("one profile-specific function record")));
		ASSERT_THAT(IsFalse(PreviousRecords[0].contentHash.Equals(OtherProfileRecords[0].contentHash),
			TEXT("the target profile must participate in FunctionBody/ASTBodySidecar identity")));
	}

	TEST_METHOD(OldSchemaDumpAndHirInputsAreSafeMisses)
	{
		const asBYTE Dump[] = { 'A', 'S', 'T', '\n', 'T', 'U' };
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNSUPPORTED_INPUT,
			asCASTRejectNonSidecarInput(Dump, 6),
			TEXT("AST dump is not a Cache input path")));
		const asBYTE Hir[] = { 'x', '.', 'h', 'i', 'r', '.', 't', 'x', 't' };
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNSUPPORTED_INPUT,
			asCASTRejectNonSidecarInput(Hir, 9),
			TEXT(".hir.txt/.hir.json are not Cache inputs")));
		const asBYTE OldSchema[] = { 'C', 'A', 'S', 'T', 0, 0, 0, 0 };
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNKNOWN_KIND,
			asCASTRejectNonSidecarInput(OldSchema, 8),
			TEXT("old sidecar schema is a safe miss")));
	}

	TEST_METHOD(AdversarialSidecarLengthFieldsFailClosedWithoutHugeAlloc)
	{
		const asBYTE Schema = static_cast<asBYTE>(asAST_SIDECAR_SCHEMA_VERSION & 0xff);
		const asBYTE UintMaxOwnerLen[] = {
			'C', 'A', 'S', 'T',
			Schema, 0, 0, 0,
			0xff, 0xff, 0xff, 0xff,
		};
		asSASTSidecarHeader Inspected;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTReadSidecarHeader(UintMaxOwnerLen, 12, Inspected),
			TEXT("UINT_MAX owner length must not wrap offset+n")));
		asCASTContext MaxDecode;
		asSASTSidecarHeader Expected;
		Expected.functionKey = "Mod.F";
		Expected.profile = "Win64";
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTDecodeSidecar(MaxDecode, Expected, UintMaxOwnerLen, 12),
			TEXT("UINT_MAX owner length must fail closed on full decode")));
		ASSERT_THAT(IsFalse(MaxDecode.IsPublishable(),
			TEXT("malicious length must not publish a graph")));

		const asBYTE UintMaxMinusOne[] = {
			'C', 'A', 'S', 'T',
			Schema, 0, 0, 0,
			0xfe, 0xff, 0xff, 0xff,
		};
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTReadSidecarHeader(UintMaxMinusOne, 12, Inspected),
			TEXT("UINT_MAX-1 owner length must fail closed")));

		const asBYTE ExactRemaining[] = {
			'C', 'A', 'S', 'T',
			Schema, 0, 0, 0,
			4, 0, 0, 0,
			'M', 'o', 'd', '.',
		};
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTReadSidecarHeader(ExactRemaining, 16, Inspected),
			TEXT("exact remaining owner bytes without a profile are still truncated")));

		const asBYTE TruncatedCount[] = {
			'C', 'A', 'S', 'T',
			Schema, 0, 0, 0,
			2, 0, 0, 0,
			'A',
		};
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTReadSidecarHeader(TruncatedCount, 13, Inspected),
			TEXT("truncated owner payload is a safe miss")));

		const asBYTE NestedUintMaxCount[] = {
			'C', 'A', 'S', 'T',
			Schema, 0, 0, 0,
			5, 0, 0, 0, 'M', 'o', 'd', '.', 'F',
			5, 0, 0, 0, 'W', 'i', 'n', '6', '4',
			0xff, 0xff, 0xff, 0xff,
		};
		asCASTContext NestedCount;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_BUDGET,
			asCASTDecodeSidecar(NestedCount, Expected, NestedUintMaxCount, sizeof(NestedUintMaxCount)),
			TEXT("UINT_MAX nested source count must fail closed before allocation")));
		ASSERT_THAT(IsFalse(NestedCount.IsPublishable(),
			TEXT("nested UINT_MAX count must not publish a graph")));

		const asBYTE NestedUintMaxString[] = {
			'C', 'A', 'S', 'T',
			Schema, 0, 0, 0,
			5, 0, 0, 0, 'M', 'o', 'd', '.', 'F',
			5, 0, 0, 0, 'W', 'i', 'n', '6', '4',
			1, 0, 0, 0,
			0xff, 0xff, 0xff, 0xff,
		};
		asCASTContext NestedString;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_MALFORMED,
			asCASTDecodeSidecar(NestedString, Expected, NestedUintMaxString, sizeof(NestedUintMaxString)),
			TEXT("UINT_MAX nested string length must fail closed without wrapping offset+n")));
		ASSERT_THAT(IsFalse(NestedString.IsPublishable(),
			TEXT("nested UINT_MAX string must not publish a graph")));
	}

	TEST_METHOD(FullDecodeRejectsEmptyExpectedProfileWhenPayloadProfileIsPresent)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		Sema.ActOnTranslationUnit("Mod");
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("profile-exact sample should seal")));

		asSASTSidecarHeader Header;
		Header.functionKey = "Mod.F";
		Header.profile = "Win64";
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode a sidecar with a non-empty profile")));

		asSASTSidecarHeader EmptyExpected = Header;
		EmptyExpected.profile = "";
		asCASTContext Wildcard;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_PROFILE_MISMATCH,
			asCASTDecodeSidecar(Wildcard, EmptyExpected, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("full decode must not treat empty expected profile as wildcard")));
		ASSERT_THAT(IsFalse(Wildcard.IsPublishable(),
			TEXT("wildcard restore must not publish a graph")));

		asSASTSidecarHeader Inspected;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTReadSidecarHeader(Bytes.AddressOf(), Bytes.GetLength(), Inspected),
			TEXT("header inspection remains available without an expected profile")));
		ASSERT_THAT(IsTrue(Inspected.profile.Equals("Win64"),
			TEXT("inspection still reports the encoded profile")));
	}
};

#endif
