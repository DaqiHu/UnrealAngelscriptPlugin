#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "../Compiler/NativeSourceExecutionTestSupport.h"
#include "frontend/as_ast_codec.h"
#include "frontend/as_type_context.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(Lambda, "Angelscript.UnitTest.NativeEngine.LanguageSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	static bool HasAnonymousDiagnostic(const FSessionRun& Run)
	{
		asCCollectingDiagnosticConsumer Consumer;
		Run.Diagnostics.EmitTo(Consumer);
		for (const auto& Record : Consumer.GetRecords())
			if (Record.DiagnosticID == 3008 && Record.Severity == asEDiagnosticSeverity::Error
				&& Record.Arguments.Num() == 1 && Record.Arguments[0].TextValue == TEXT("AnonymousFunction")
				&& Record.PrimaryRange.IsOrdered()) return true;
		return false;
	}

	template<class T>
	static constexpr bool HasOriginFactory = requires(T& Image, asCScriptFunction& Function)
	{
		Image.SetLambdaOrigin(Function, Function, asUINT(0));
	};

	template<class T>
	static void SupplyRetiredOrigin(T& Identity, const asSStableKey& Parent)
	{
		// Allows this negative test to run before and after deleting the old fields.
		if constexpr (requires { Identity.EnclosingFunction; Identity.LambdaOrdinal; })
		{
			Identity.EnclosingFunction = Parent;
			Identity.LambdaOrdinal = 0;
		}
	}

public:
	TEST_METHOD(ImmediateAnonymousFunctionCannotPublish)
	{
		FSessionRun Run({{TEXT("Immediate.as"), TEXT("int F(){return function(int V){return V+1;}(41);}")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		const bool Analyzed = Run.Session->AnalyzeBodies(1);
		ASSERT_THAT(IsTrue(HasAnonymousDiagnostic(Run)));
		ASSERT_THAT(IsFalse(Analyzed));
		ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
	}

	TEST_METHOD(StoredAnonymousFunctionCannotPublish)
	{
		FSessionRun Run({{TEXT("Stored.as"), TEXT("delegate int Read(); int F(){Read Op=function(){return 42;}; return Op();}")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		const bool Analyzed = Run.Session->AnalyzeBodies(1);
		ASSERT_THAT(IsTrue(HasAnonymousDiagnostic(Run)));
		ASSERT_THAT(IsFalse(Analyzed));
		ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
	}

	TEST_METHOD(EmptyValueAndReferenceCaptureListsHaveExplicitRejection)
	{
		for (const TCHAR* Expression : {TEXT("[](){return 42;}()"), TEXT("[Value](){return Value;}()"),
			TEXT("[&Value](){return Value;}()"), TEXT("function[Value](){return Value;}()")})
		{
			FSessionRun Run({{TEXT("Captures.as"), FString::Printf(TEXT("int F(){int Value=42; return %s;}"), Expression)}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			const bool Analyzed = Run.Session->AnalyzeBodies(1);
			ASSERT_THAT(IsTrue(HasAnonymousDiagnostic(Run), Expression));
			ASSERT_THAT(IsFalse(Analyzed));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
		}
	}

	TEST_METHOD(DefaultAndNestedAnonymousFunctionsCannotEscapeTheBoundary)
	{
		for (const TCHAR* Source : {
			TEXT("delegate int Read(); int F(Read Value=function(){return 42;}){return Value();}"),
			TEXT("int F(){return function(){return function(){return 42;}();}();}")})
		{
			FSessionRun Run({{TEXT("Nested.as"), Source}});
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			if (Run.Session->ResolveDeclarations(1)) Run.Session->AnalyzeBodies(1);
			ASSERT_THAT(IsTrue(HasAnonymousDiagnostic(Run), Source));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
		}
	}

	TEST_METHOD(PreviousASTFormatIsRejectedEvenForANamedFunction)
	{
		FSessionRun Run({{TEXT("Named.as"), TEXT("int AddOne(int Value){return Value+1;}")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		asCASTProjection Projection;
		FString Error;
		ASSERT_THAT(IsTrue(asCASTProjection::Build(Run.Session->GetASTContext(), Run.Session->GetTranslationUnit(), Projection, Error), Error));
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEASTCodecStatus::Succeeded, asCASTCodec::Encode(Projection, Bytes)));
		ASSERT_THAT(IsTrue(Bytes.Num() > 6));
		Bytes[4] = 8; Bytes[5] = 0; // Last format capable of representing anonymous-function products.
		asCASTProjection Decoded;
		ASSERT_THAT(AreEqual(asEASTCodecStatus::IncompatibleVersion, asCASTCodec::Decode(Bytes, Decoded)));
		ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
	}

	TEST_METHOD(RetiredStatementOrdinalCannotDecodeAsASupportedNode)
	{
		FSessionRun Run({{TEXT("Named.as"), TEXT("int F(){return 42;}")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		asCASTProjection Projection;
		FString Error;
		ASSERT_THAT(IsTrue(asCASTProjection::Build(Run.Session->GetASTContext(), Run.Session->GetTranslationUnit(), Projection, Error), Error));
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEASTCodecStatus::Succeeded, asCASTCodec::Encode(Projection, Bytes)));
		ASSERT_THAT(IsTrue(Bytes.Num() > 13));
		Bytes[10] = 1; Bytes[11] = 119; Bytes[12] = 0; // Retired LambdaExpr wire ordinal.
		asCASTProjection Decoded;
		ASSERT_THAT(AreNotEqual(asEASTCodecStatus::Succeeded, asCASTCodec::Decode(Bytes, Decoded)));
		ASSERT_THAT(AreEqual(0, Decoded.GetRecords().Num()));
	}

	TEST_METHOD(RetiredFunctionIdentityCannotBeInterned)
	{
		asCTypeContext Types;
		asSStableKey Int, Parent, Rejected;
		ASSERT_THAT(AreEqual(asETypeIdentityStatus::Succeeded, Types.InternPrimitiveTypeUse(asEPrimitiveType::Int32, Int)));
		asSFunctionIdentityDescriptor Identity;
		Identity.StableScope = TEXT("removed-origin");
		Identity.Kind = asEFunctionIdentityKind::Global;
		Identity.Name = TEXT("Named");
		Identity.ReturnType = Int;
		ASSERT_THAT(AreEqual(asETypeIdentityStatus::Succeeded, Types.GetIdentityRegistry().InternFunction(Identity, Parent)));
		Identity.Kind = static_cast<asEFunctionIdentityKind>(7); // Reserved old anonymous origin.
		Identity.Name.Reset();
		SupplyRetiredOrigin(Identity, Parent);
		ASSERT_THAT(AreEqual(asETypeIdentityStatus::InvalidDescriptor, Types.GetIdentityRegistry().InternFunction(Identity, Rejected)));
		ASSERT_THAT(IsTrue(Rejected.IsZero()));
	}

	TEST_METHOD(DefinitionSetExposesNoAnonymousOriginFactory)
	{
		ASSERT_THAT(IsFalse(HasOriginFactory<asCModuleDefinitionSet>));
	}

	TEST_METHOD(NamedFunctionRoundtripAndExecutionPreserveFortyTwo)
	{
		FSourceExecutionInput Input(TEXT("int AddOne(int V){return V+1;} int Good(){return AddOne(41);}"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		auto* Session = Builder.GetCompilationSession();
		asCASTProjection Projection, Decoded;
		FString Error;
		ASSERT_THAT(IsTrue(asCASTProjection::Build(Session->GetASTContext(), Session->GetTranslationUnit(), Projection, Error), Error));
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEASTCodecStatus::Succeeded, asCASTCodec::Encode(Projection, Bytes)));
		ASSERT_THAT(AreEqual(asEASTCodecStatus::Succeeded, asCASTCodec::Decode(Bytes, Decoded)));
		ASSERT_THAT(IsTrue(Projection.Equals(Decoded)));
		auto Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		ON_SCOPE_EXIT { Snapshot->Release(); };
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(0, Context->Prepare(FindSourceFunction(Builder, "Good"))));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
	}
};

#endif
