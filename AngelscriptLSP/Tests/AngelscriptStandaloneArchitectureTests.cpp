#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	namespace fs = std::filesystem;

	bool Require(bool bCondition, const std::string& Message)
	{
		if (!bCondition)
		{
			std::cerr << Message << '\n';
		}
		return bCondition;
	}

	std::string ReadText(const fs::path& Path)
	{
		std::ifstream Input(Path, std::ios::binary);
		return std::string(
			std::istreambuf_iterator<char>(Input),
			std::istreambuf_iterator<char>());
	}

	std::string NormalizeLineEndings(std::string Text)
	{
		std::string Result;
		Result.reserve(Text.size());
		for (const char Character : Text)
		{
			if (Character != '\r')
			{
				Result.push_back(Character);
			}
		}
		return Result;
	}

	bool HasForbiddenCoreDependency(std::string_view Text)
	{
		static constexpr std::string_view Patterns[] = {
			"#include \"CoreMinimal.h\"",
			"#include \"CoreTypes.h\"",
			"#include \"AngelscriptEngine.h\"",
			"#include \"AngelscriptSettings.h\"",
			"#include \"ClassGenerator/ASClass.h\"",
			"#include \"Containers/",
			"#include \"HAL/",
			"FMemory::",
			"FPlatformAtomics::",
			"FAngelscriptEngine::",
			"UASClass::",
			"TArray<",
			"TMap<",
			"TMultiMap<",
		};

		for (std::string_view Pattern : Patterns)
		{
			if (Text.find(Pattern) != std::string_view::npos)
			{
				return true;
			}
		}
		return false;
	}

	bool HasForkPortabilityContamination(std::string_view Text)
	{
		static constexpr std::string_view Patterns[] = {
			"as_portable_containers.h",
			"as_memoryarena.h",
			"as_host_services.h",
			"asCStdArray<",
			"asCHashMap<",
			"asCPair<",
			"asCInlineAllocator<",
			"asCScopedValue<",
			"asCMemoryArena",
			"std::atomic_ref",
			"std::shared_mutex",
			"std::vector<",
		};

		for (const std::string_view Pattern : Patterns)
		{
			if (Text.find(Pattern) != std::string_view::npos)
			{
				return true;
			}
		}
		return false;
	}

	bool HasForbiddenStandaloneHostDependency(std::string_view Text)
	{
		static constexpr std::string_view Patterns[] = {
			"#include \"CoreMinimal.h\"",
			"#include \"CoreTypes.h\"",
			"#include \"UObject/",
			"#include \"Engine/",
			"#include \"Modules/",
			"#include \"HAL/",
			"#include \"Misc/Paths.h\"",
			"#include \"AngelscriptEngine.h\"",
			".generated.h\"",
			"FMemory::",
			"FPlatformProcess::",
			"FAngelscriptEngine::",
			"UObject*",
		};
		for (const std::string_view Pattern : Patterns)
		{
			if (Text.find(Pattern) != std::string_view::npos)
			{
				return true;
			}
		}
		return false;
	}

	bool HasStandaloneBindingContamination(std::string_view Text)
	{
		static constexpr std::string_view Patterns[] = {
			"ANGELSCRIPT_LANGUAGE_STANDALONE",
			"ANGELSCRIPT_STANDALONE",
			"WITH_ANGELSCRIPT_STANDALONE",
			"AS_STANDALONE",
			"ExportStandalone",
			"OfflineSymbolExporter",
			"OfflineBundleWriter",
			"OfflineExportService",
		};
		for (const std::string_view Pattern : Patterns)
		{
			if (Text.find(Pattern) != std::string_view::npos)
			{
				return true;
			}
		}
		return false;
	}

	bool HasTypedSnapshotBackendContamination(std::string_view Text)
	{
		static constexpr std::string_view Patterns[] = {
			"StaticJIT",
			"TypedASTJIT",
			"BytecodeJIT",
			"Provider",
			"UObject",
			"FAngelscriptEngine",
			"asCreateScriptEngine",
			"GetModule(",
			"AddScriptSection",
			"CreateContext(",
			"FFileHelper",
			"IFileManager",
		};
		for (const std::string_view Pattern : Patterns)
		{
			if (Text.find(Pattern) != std::string_view::npos)
			{
				return true;
			}
		}
		return false;
	}

	std::string RemoveCommentOnlyLines(std::string_view Text)
	{
		std::string Result;
		std::size_t Offset = 0;
		while (Offset < Text.size())
		{
			const std::size_t End = Text.find('\n', Offset);
			const std::string_view Line = Text.substr(
				Offset,
				End == std::string_view::npos ? Text.size() - Offset : End - Offset);
			const std::size_t FirstContent = Line.find_first_not_of(" \t");
			if (FirstContent == std::string_view::npos || !Line.substr(FirstContent).starts_with("//"))
			{
				Result.append(Line);
				Result.push_back('\n');
			}
			if (End == std::string_view::npos)
			{
				break;
			}
			Offset = End + 1;
		}
		return Result;
	}
}

int main()
{
	const fs::path PluginRoot = fs::path(ANGELSCRIPT_STANDALONE_SOURCE_ROOT);
	const fs::path ForkRoot = PluginRoot / "Source/AngelscriptRuntime/ThirdParty/angelscript/source";
	const fs::path PublicHeader = PluginRoot / "Source/AngelscriptRuntime/Core/angelscript.h";
	const fs::path StandaloneRoot = PluginRoot / "Standalone";
	const fs::path CompatRoot = StandaloneRoot / "Compat";
	const fs::path StandaloneSourceRoot = StandaloneRoot / "Source";
	const fs::path StandaloneFrontendRoot =
		StandaloneSourceRoot / "Compiler/Frontend";
	const fs::path RuntimeLanguageRoot =
		PluginRoot / "Source/AngelscriptRuntime/Language";
	const fs::path UEPreprocessor =
		PluginRoot / "Source/AngelscriptRuntime/Preprocessor/AngelscriptPreprocessor.cpp";
	const fs::path ParserHeader = ForkRoot / "as_parser.h";
	const fs::path ParserSource = ForkRoot / "as_parser.cpp";
	const fs::path BuilderSource = ForkRoot / "as_builder.cpp";
	const fs::path ModuleHeader = ForkRoot / "as_module.h";
	const fs::path ModuleSource = ForkRoot / "as_module.cpp";
	const fs::path ASTPublicViewHeader = ForkRoot / "as_ast_public_view.h";
	const fs::path ASTPublicViewSource = ForkRoot / "as_ast_public_view.cpp";
	const fs::path ASTDiagnosticsSource =
		PluginRoot / "Source/AngelscriptRuntime/Dump/AngelscriptASTDiagnostics.cpp";
	const fs::path StaticJITGenerationSnapshotHeader =
		PluginRoot / "Source/AngelscriptRuntime/StaticJIT/AngelscriptStaticJITGenerationSnapshot.h";
	const fs::path StaticJITGenerationSnapshotSource =
		PluginRoot / "Source/AngelscriptRuntime/StaticJIT/AngelscriptStaticJITGenerationSnapshot.cpp";
	const fs::path CacheCleanCaptureSource =
		PluginRoot / "Source/AngelscriptRuntime/Cache/AngelscriptCacheCleanCapture.cpp";
	const fs::path ScriptFunctionHeader = ForkRoot / "as_scriptfunction.h";
	const fs::path ScriptFunctionSource = ForkRoot / "as_scriptfunction.cpp";
	const fs::path RestoreSource = ForkRoot / "as_restore.cpp";
	const fs::path BindingRoot =
		PluginRoot / "Source/AngelscriptRuntime/Binds";
	const fs::path ClassGeneratorRoot =
		PluginRoot / "Source/AngelscriptRuntime/ClassGenerator";
	const fs::path BuildRoot = fs::path(ANGELSCRIPT_STANDALONE_BUILD_ROOT);

	bool bPassed = true;
	const std::string PublicHeaderText = ReadText(PublicHeader);
	bPassed &= Require(
		fs::exists(ASTDiagnosticsSource),
		"canonical AST diagnostics source is missing");
	for (const fs::path& RetiredHIRDumpSurface : {
		PluginRoot / "Source/AngelscriptEditor/StaticJIT/AngelscriptHIRDumpCommand.cpp",
		PluginRoot / "Source/AngelscriptEditor/StaticJIT/AngelscriptHIRDumpCommand.h",
		PluginRoot / "Source/AngelscriptEditor/StaticJIT/AngelscriptHIRDumpCommandlet.cpp",
		PluginRoot / "Source/AngelscriptEditor/StaticJIT/AngelscriptHIRDumpCommandlet.h",
		PluginRoot / "Source/AngelscriptTest/StaticJIT/AngelscriptHIRDumpCommandletTests.cpp"})
	{
		bPassed &= Require(
			!fs::exists(RetiredHIRDumpSurface),
			"retired HIR dump surface remains: " + RetiredHIRDumpSurface.string());
	}

	// Detector self-tests are deliberately bad fixtures. UE spellings remain
	// forbidden in the private frontend and ordinary standalone host code, but are
	// expected in the maintained fork and resolved by Standalone/Compat.
	bPassed &= Require(
		HasForbiddenCoreDependency("#include \"CoreMinimal.h\"\n"),
		"language/host detector did not reject a CoreMinimal include");
	bPassed &= Require(
		HasForkPortabilityContamination("std::vector<void*> Values;\n"),
		"fork detector did not reject a mechanical std::vector migration");
	bPassed &= Require(
		HasForkPortabilityContamination("#include \"as_host_services.h\"\n"),
		"fork detector did not reject the obsolete host-service seam");
	bPassed &= Require(
		HasTypedSnapshotBackendContamination("Provider->GenerateStaticJIT();\n"),
		"typed snapshot detector did not reject backend/provider behavior");
	bPassed &= Require(
		!HasForkPortabilityContamination("TArray<int> Values;\nFMemory::Malloc(4);\n"),
		"fork detector rejected the preserved UE-spelled boundary");

	const std::vector<std::string> MaintainedSources = {
		"as_atomic.cpp",
		"as_builder.cpp",
		"as_bytecode.cpp",
		"as_callfunc.cpp",
		"as_compiler.cpp",
		"as_source_manager.cpp",
		"as_ast_type.cpp",
		"as_decl.cpp",
		"as_stmt.cpp",
		"as_expr.cpp",
		"as_ast_context.cpp",
		"as_ast_verifier.cpp",
		"as_ast_dump.cpp",
		"as_ast_sidecar.cpp",
		"as_ast_public_view.cpp",
		"as_runtime_type_bridge.cpp",
		"as_bytecode_codegen.cpp",
		"as_sema.cpp",
		"as_sema_decl.cpp",
		"as_sema_expr.cpp",
		"as_sema_stmt.cpp",
		"as_sema_lifetime.cpp",
		"as_configgroup.cpp",
		"as_context.cpp",
		"as_datatype.cpp",
		"as_gc.cpp",
		"as_generic.cpp",
		"as_globalproperty.cpp",
		"as_memory.cpp",
		"as_module.cpp",
		"as_objecttype.cpp",
		"as_outputbuffer.cpp",
		"as_parser.cpp",
		"as_restore.cpp",
		"as_scriptcode.cpp",
		"as_scriptengine.cpp",
		"as_scriptfunction.cpp",
		"as_scriptnode.cpp",
		"as_scriptobject.cpp",
		"as_string.cpp",
		"as_string_util.cpp",
		"as_thread.cpp",
		"as_tokenizer.cpp",
		"as_typeinfo.cpp",
		"as_variablescope.cpp",
	};

	bPassed &= Require(fs::exists(PublicHeader), "maintained public header is missing");
	bPassed &= Require(
		!HasForkPortabilityContamination(RemoveCommentOnlyLines(PublicHeaderText)),
		"maintained public header contains standalone portability contamination");
	bPassed &= Require(
		PublicHeaderText.find("#include \"CoreMinimal.h\"") != std::string::npos,
		"maintained public header no longer preserves its UE Core include");
	bPassed &= Require(
		PublicHeaderText.find("#include \"FunctionCallers.h\"") != std::string::npos,
		"maintained public header no longer preserves FunctionCallers inclusion");
	bPassed &= Require(
		PublicHeaderText.find("TypedSemantic") == std::string::npos
			&& PublicHeaderText.find("typedSemantic") == std::string::npos,
		"fork-private typed semantic HIR leaked into the public angelscript.h ABI");
	const std::size_t ModuleForwardDecl = PublicHeaderText.find("class asIScriptModule");
	const std::size_t ModuleBegin = PublicHeaderText.find("class asIScriptModule", ModuleForwardDecl + 1);
	const std::size_t ModuleEnd = PublicHeaderText.find("class asIScriptContext", ModuleBegin);
	const std::size_t CompileFunction = PublicHeaderText.find("virtual int         CompileFunction", ModuleBegin);
	const std::size_t SetAccessMask = PublicHeaderText.find("virtual asDWORD     SetAccessMask", ModuleBegin);
	const std::size_t ClearImports = PublicHeaderText.find("virtual void ClearImports()", ModuleBegin);
	const std::size_t SetASTRetentionPolicy = PublicHeaderText.find("virtual int         SetASTRetentionPolicy", ModuleBegin);
	const std::size_t GetASTRetentionPolicy = PublicHeaderText.find("virtual asEASTRetentionPolicy GetASTRetentionPolicy", ModuleBegin);
	const std::size_t AcquireASTSnapshot = PublicHeaderText.find("virtual asIASTSnapshot* AcquireASTSnapshot", ModuleBegin);
	bPassed &= Require(
		ModuleBegin != std::string::npos
			&& ModuleEnd != std::string::npos
			&& CompileFunction != std::string::npos
			&& SetAccessMask != std::string::npos
			&& ClearImports != std::string::npos
			&& SetASTRetentionPolicy != std::string::npos
			&& GetASTRetentionPolicy != std::string::npos
			&& AcquireASTSnapshot != std::string::npos,
		"asIScriptModule AST ABI layout markers are missing from the maintained public header");
	bPassed &= Require(
		CompileFunction < SetAccessMask
			&& SetAccessMask < ClearImports
			&& ClearImports < SetASTRetentionPolicy
			&& SetASTRetentionPolicy < GetASTRetentionPolicy
			&& GetASTRetentionPolicy < AcquireASTSnapshot
			&& AcquireASTSnapshot < ModuleEnd,
		"asIScriptModule AST methods must be trailing append-only slots after ClearImports");
	const std::string ModuleHeaderText = ReadText(ModuleHeader);
	const std::string ModuleSourceText = ReadText(ModuleSource);
	const std::string ASTPublicViewHeaderText = ReadText(ASTPublicViewHeader);
	const std::string ASTPublicViewSourceText = ReadText(ASTPublicViewSource);
	const std::string StaticJITGenerationSnapshotHeaderText =
		ReadText(StaticJITGenerationSnapshotHeader);
	const std::string StaticJITGenerationSnapshotSourceText =
		NormalizeLineEndings(ReadText(StaticJITGenerationSnapshotSource));
	const std::string CacheCleanCaptureSourceText =
		NormalizeLineEndings(ReadText(CacheCleanCaptureSource));
	auto FunctionUsesSnapshotGate = [&ModuleSourceText](const std::string_view Signature)
	{
		const std::size_t Start = ModuleSourceText.find(Signature);
		const std::size_t End = Start == std::string::npos
			? std::string::npos
			: ModuleSourceText.find("\n}\n", Start);
		return Start != std::string::npos
			&& End != std::string::npos
			&& ModuleSourceText.substr(Start, End - Start).find("FScopeLock SnapshotLock(&astSnapshotLock)")
				!= std::string::npos;
	};
	bPassed &= Require(
		ModuleHeaderText.find("mutable FCriticalSection astSnapshotLock;") != std::string::npos
			&& FunctionUsesSnapshotGate("asIASTSnapshot* asCModule::AcquireASTSnapshot")
			&& FunctionUsesSnapshotGate("void asCModule::ReleaseCanonicalASTSnapshot")
			&& FunctionUsesSnapshotGate("void asCModule::PublishCanonicalASTSnapshot"),
		"public AST snapshot acquire, retirement, and publication must share one module lifetime gate");
	bPassed &= Require(
		ASTPublicViewHeaderText.find("mutable volatile int32 currentGeneration;") != std::string::npos
			&& ASTPublicViewHeaderText.find("bool currentGeneration;") == std::string::npos
			&& ASTPublicViewSourceText.find("FPlatformAtomics::InterlockedCompareExchange") != std::string::npos
			&& ASTPublicViewSourceText.find("FPlatformAtomics::InterlockedExchange") != std::string::npos,
		"public AST snapshot generation state must use atomic reads and writes");
	const std::size_t GenerationSnapshotBuilderStart =
		StaticJITGenerationSnapshotSourceText.find(
			"bool BuildAngelscriptStaticJITGenerationSnapshot(");
	const std::size_t GenerationSnapshotBuilderEnd =
		GenerationSnapshotBuilderStart == std::string::npos
		? std::string::npos
		: StaticJITGenerationSnapshotSourceText.find(
			"\n}\n", GenerationSnapshotBuilderStart);
	const std::string_view GenerationSnapshotBuilderBody =
		GenerationSnapshotBuilderStart == std::string::npos
		|| GenerationSnapshotBuilderEnd == std::string::npos
		? std::string_view()
		: std::string_view(StaticJITGenerationSnapshotSourceText).substr(
			GenerationSnapshotBuilderStart,
			GenerationSnapshotBuilderEnd - GenerationSnapshotBuilderStart);
	bPassed &= Require(
		!GenerationSnapshotBuilderBody.empty()
			&& StaticJITGenerationSnapshotHeaderText.find("CanonicalASTLeases")
				!= std::string::npos
			&& GenerationSnapshotBuilderBody.find("GetCanonicalASTContext")
				== std::string::npos
			&& GenerationSnapshotBuilderBody.find(
				"AcquireASTSnapshot(asAST_API_VERSION_1)") != std::string::npos
			&& GenerationSnapshotBuilderBody.find(
				"FAngelscriptStaticJITGenerationASTLease") != std::string::npos
			&& StaticJITGenerationSnapshotSourceText.find("asCASTSnapshot")
				!= std::string::npos
			&& GenerationSnapshotBuilderBody.find("GetContext()")
				!= std::string::npos,
		"typed StaticJIT generation must own V1 AST leases instead of borrowing a raw module context");
	const std::size_t CacheSidecarCaptureStart = CacheCleanCaptureSourceText.find(
		"static bool TryCaptureCanonicalASTBodySidecar(");
	const std::size_t CacheSidecarCaptureEnd = CacheSidecarCaptureStart == std::string::npos
		? std::string::npos
		: CacheCleanCaptureSourceText.find("\n\ttemplate <", CacheSidecarCaptureStart);
	const std::string_view CacheSidecarCaptureBody = CacheSidecarCaptureStart == std::string::npos
		|| CacheSidecarCaptureEnd == std::string::npos
		? std::string_view()
		: std::string_view(CacheCleanCaptureSourceText).substr(
			CacheSidecarCaptureStart,
			CacheSidecarCaptureEnd - CacheSidecarCaptureStart);
	bPassed &= Require(
		!CacheSidecarCaptureBody.empty()
			&& CacheSidecarCaptureBody.find("GetCanonicalASTContext")
				== std::string::npos
			&& CacheSidecarCaptureBody.find(
				"AcquireASTSnapshot(asAST_API_VERSION_1)") != std::string::npos
			&& CacheSidecarCaptureBody.find("asCASTSnapshot") != std::string::npos
			&& CacheSidecarCaptureBody.find("GetContext()") != std::string::npos,
		"Cache V2 AST sidecar capture must encode through a held V1 AST snapshot lease");
	const std::string ParserHeaderText =
		RemoveCommentOnlyLines(ReadText(ParserHeader));
	const std::string ParserSourceText =
		RemoveCommentOnlyLines(ReadText(ParserSource));
	bPassed &= Require(
		ParserHeaderText.find("FMemStackBase MemStack;") != std::string::npos
			&& ParserSourceText.find(
				"MemStack.Alloc(sizeof(asCScriptNode), alignof(asCScriptNode))")
				!= std::string::npos,
		"asCParser no longer owns raw syntax nodes through its local FMemStackBase");

	const std::string BuilderSourceText =
		RemoveCommentOnlyLines(ReadText(BuilderSource));
	const std::size_t ReleaseParsersBegin = BuilderSourceText.find(
		"void asCBuilder::ReleaseParsers()");
	const std::size_t ReleaseParsersEnd = ReleaseParsersBegin
		== std::string::npos
		? std::string::npos
		: BuilderSourceText.find("asCBuilder::~asCBuilder()", ReleaseParsersBegin);
	const std::size_t BuilderDestructorBegin = BuilderSourceText.find(
		"asCBuilder::~asCBuilder()");
	const std::size_t BuilderDestructorEnd = BuilderDestructorBegin
		== std::string::npos
		? std::string::npos
		: BuilderSourceText.find("void asCBuilder::Reset()", BuilderDestructorBegin);
	const std::size_t BuildCompileCodeBegin = BuilderSourceText.find(
		"int asCBuilder::BuildCompileCode()");
	const std::size_t BuildCompileCodeEnd = BuildCompileCodeBegin
		== std::string::npos
		? std::string::npos
		: BuilderSourceText.find(
			"int asCBuilder::ValidateDefaultArgs", BuildCompileCodeBegin);
	bPassed &= Require(
		ReleaseParsersBegin != std::string::npos
			&& ReleaseParsersEnd != std::string::npos,
		"Builder is missing its shared Parser/FMemStack release routine");
	bPassed &= Require(
		BuilderDestructorBegin != std::string::npos
			&& BuilderDestructorEnd != std::string::npos,
		"Builder destructor source boundary is missing");
	bPassed &= Require(
		BuildCompileCodeBegin != std::string::npos
			&& BuildCompileCodeEnd != std::string::npos,
		"Builder legacy compile source boundary is missing");
	if (ReleaseParsersBegin != std::string::npos
		&& ReleaseParsersEnd != std::string::npos)
	{
		const std::string_view ReleaseParsers(
			BuilderSourceText.data() + ReleaseParsersBegin,
			ReleaseParsersEnd - ReleaseParsersBegin);
		bPassed &= Require(
			ReleaseParsers.find("asDELETE(parsers[n],asCParser)")
				!= std::string_view::npos
				&& ReleaseParsers.find("parsers.SetLength(0)")
					!= std::string_view::npos,
			"Builder shared Parser release must destroy every Parser and clear the owner array");
	}
	if (BuilderDestructorBegin != std::string::npos
		&& BuilderDestructorEnd != std::string::npos)
	{
		const std::string_view BuilderDestructor(
			BuilderSourceText.data() + BuilderDestructorBegin,
			BuilderDestructorEnd - BuilderDestructorBegin);
		bPassed &= Require(
			BuilderDestructor.find("ReleaseParsers();")
				!= std::string_view::npos,
			"CANONICAL Builder destruction must release Parser-owned syntax trees");
	}
	if (BuildCompileCodeBegin != std::string::npos
		&& BuildCompileCodeEnd != std::string::npos)
	{
		const std::string_view BuildCompileCode(
			BuilderSourceText.data() + BuildCompileCodeBegin,
			BuildCompileCodeEnd - BuildCompileCodeBegin);
		bPassed &= Require(
			BuildCompileCode.find("ReleaseParsers();")
				!= std::string_view::npos,
			"LEGACY compile must use the same idempotent Parser release routine");
	}

	const std::string ScriptFunctionHeaderText =
		RemoveCommentOnlyLines(ReadText(ScriptFunctionHeader));
	const std::size_t ScriptDataBegin =
		ScriptFunctionHeaderText.find("struct ScriptFunctionData");
	const std::size_t ScriptDataEnd = ScriptDataBegin == std::string::npos
		? std::string::npos
		: ScriptFunctionHeaderText.find(
			"ScriptFunctionData          *scriptData;",
			ScriptDataBegin);
	bPassed &= Require(
		ScriptDataBegin != std::string::npos
			&& ScriptDataEnd != std::string::npos,
		"asCScriptFunction::ScriptFunctionData source boundary is missing");
	if (ScriptDataBegin != std::string::npos
		&& ScriptDataEnd != std::string::npos)
	{
		const std::string_view ScriptDataView(
			ScriptFunctionHeaderText.data() + ScriptDataBegin,
			ScriptDataEnd - ScriptDataBegin);
		bPassed &= Require(
			ScriptDataView.find("asCScriptNode") == std::string_view::npos,
			"ScriptFunctionData retained a raw parser syntax node pointer");
	}
	const std::string ScriptFunctionSourceText =
		RemoveCommentOnlyLines(ReadText(ScriptFunctionSource));
	const std::size_t DestroyInternalBegin = ScriptFunctionSourceText.find(
		"void asCScriptFunction::DestroyInternal()");
	const std::size_t DestroyInternalEnd = DestroyInternalBegin
		== std::string::npos
		? std::string::npos
		: ScriptFunctionSourceText.find(
			"int asCScriptFunction::GetId()", DestroyInternalBegin);
	bPassed &= Require(
		DestroyInternalBegin != std::string::npos
			&& DestroyInternalEnd != std::string::npos,
		"asCScriptFunction::DestroyInternal source boundary is missing");
	if (DestroyInternalBegin != std::string::npos
		&& DestroyInternalEnd != std::string::npos)
	{
		const std::string_view DestroyInternal(
			ScriptFunctionSourceText.data() + DestroyInternalBegin,
			DestroyInternalEnd - DestroyInternalBegin);
		const std::size_t ReleaseReferences = DestroyInternal.find(
			"ReleaseReferences();");
		const std::size_t ReleaseObjectType = DestroyInternal.find(
			"objectType->ReleaseInternal();");
		bPassed &= Require(
			ReleaseReferences != std::string_view::npos
				&& ReleaseObjectType != std::string_view::npos
				&& DestroyInternal.find("TypedSemantic") == std::string_view::npos,
			"script-function destruction must not retain TypedSemantic HIR ownership");
	}

	const std::string RestoreSourceText =
		RemoveCommentOnlyLines(ReadText(RestoreSource));
	const std::size_t CommitBegin = RestoreSourceText.find(
		"int asCReader::CommitFunctionArtifactToExisting(");
	const std::size_t CommitEnd = CommitBegin == std::string::npos
		? std::string::npos
		: RestoreSourceText.find("RestoreGlobalFunctionArtifact", CommitBegin);
	bPassed &= Require(
		CommitBegin != std::string::npos && CommitEnd != std::string::npos,
		"Cache donor commit source boundary is missing");
	if (CommitBegin != std::string::npos && CommitEnd != std::string::npos)
	{
		const std::string_view Commit(
			RestoreSourceText.data() + CommitBegin,
			CommitEnd - CommitBegin);
		const std::size_t SwapScriptData = Commit.find(
			"target->scriptData = artifact->scriptData;");
		const std::size_t DestroyDonor = Commit.find(
			"artifact->DestroyHalfCreated();");
		bPassed &= Require(
			SwapScriptData != std::string_view::npos
				&& DestroyDonor != std::string_view::npos
				&& SwapScriptData < DestroyDonor
				&& Commit.find("TypedSemantic") == std::string_view::npos,
			"Cache donor commit must swap private script data without TypedSemantic HIR ownership");
	}

	for (const std::string& SourceName : MaintainedSources)
	{
		const fs::path SourcePath = ForkRoot / SourceName;
		bPassed &= Require(fs::exists(SourcePath), "maintained source is missing: " + SourcePath.string());
		if (fs::exists(SourcePath))
		{
			const std::string SourceText = RemoveCommentOnlyLines(ReadText(SourcePath));
			bPassed &= Require(
				!HasForkPortabilityContamination(SourceText),
				"maintained source contains standalone portability contamination: " + SourcePath.string());
			bPassed &= Require(
				SourceText.find("llvm::") == std::string::npos
					&& SourceText.find("clangAST") == std::string::npos,
				"maintained source must not include LLVM or Clang AST types: " + SourcePath.string());
		}
	}

	for (const fs::path& RetiredHIRFile : {
		ForkRoot / "as_typed_semantic_ir.h",
		ForkRoot / "as_typed_semantic_ir.cpp",
	})
	{
		bPassed &= Require(
			!fs::exists(RetiredHIRFile),
			"retired TypedSemantic HIR file remains in the maintained fork: "
				+ RetiredHIRFile.string());
	}

	for (const fs::path& ObsoleteForkFile : {
		ForkRoot / "as_host_services.cpp",
		ForkRoot / "as_host_services.h",
		ForkRoot / "as_memoryarena.h",
		ForkRoot / "as_portable_containers.h"})
	{
		bPassed &= Require(
			!fs::exists(ObsoleteForkFile),
			"obsolete standalone portability file remains in the fork: "
				+ ObsoleteForkFile.string());
	}

	bPassed &= Require(
		!fs::exists(RuntimeLanguageRoot),
		"Runtime-owned Language layer must be removed");
	bPassed &= Require(
		fs::exists(StandaloneFrontendRoot),
		"standalone-private frontend root is missing");
	if (fs::exists(StandaloneFrontendRoot))
	{
		for (const fs::directory_entry& Entry :
			fs::recursive_directory_iterator(StandaloneFrontendRoot))
		{
			if (!Entry.is_regular_file()
				|| (Entry.path().extension() != ".h"
					&& Entry.path().extension() != ".cpp"))
			{
				continue;
			}
			const std::string Text = RemoveCommentOnlyLines(ReadText(Entry.path()));
			bPassed &= Require(
				!HasForbiddenCoreDependency(Text),
				"standalone frontend source has an Unreal dependency: "
					+ Entry.path().string());
			bPassed &= Require(
				Text.find("ANGELSCRIPT_LANGUAGE_STANDALONE") == std::string::npos
					&& Text.find("UEAngelscript::Language") == std::string::npos,
				"standalone frontend retains the removed Language layer identity: "
					+ Entry.path().string());
		}
	}
	const std::string UEPreprocessorText = ReadText(UEPreprocessor);
	bPassed &= Require(
		UEPreprocessorText.find("Language/") == std::string::npos
			&& UEPreprocessorText.find("UEAngelscript::Language") == std::string::npos,
		"UE preprocessor must not delegate to the Standalone frontend");

	const std::string CMakeText = ReadText(StandaloneRoot / "CMakeLists.txt");
	bPassed &= Require(
		CMakeText.find("AngelscriptLanguageCore") == std::string::npos
			&& CMakeText.find("ANGELSCRIPT_LANGUAGE_STANDALONE") == std::string::npos,
		"standalone CMake retains the removed shared LanguageCore target or macro");
	bPassed &= Require(
		CMakeText.find("ANGELSCRIPT_STANDALONE_COMPAT") != std::string::npos
			&& CMakeText.find("${ANGELSCRIPT_STANDALONE_COMPAT}")
				!= std::string::npos,
		"maintained-fork CMake target does not select Standalone/Compat");
	bPassed &= Require(
		CMakeText.find("llvm-project") == std::string::npos
			&& CMakeText.find("find_package(LLVM") == std::string::npos
			&& CMakeText.find("find_package(Clang") == std::string::npos
			&& CMakeText.find("clangAST") == std::string::npos
			&& CMakeText.find("LLVM::") == std::string::npos,
		"standalone CMake must not link LLVM or Clang AST");
	bPassed &= Require(
		CMakeText.find("as_bytecode_codegen.cpp") != std::string::npos
			&& CMakeText.find("as_sema.cpp") != std::string::npos
			&& CMakeText.find("AngelscriptStandaloneCanonicalASTTests") != std::string::npos,
		"standalone CMake must compile canonical Parser/Sema/AST/Bytecode and its CTest");
	static constexpr std::string_view ForbiddenCMakePatterns[] = {
		"as_callfunc_x86.cpp",
		"as_callfunc_x64_",
		"as_callfunc_arm",
		".asm",
		"Unreal Engine",
		"Engine/Source",
		".generated.h",
	};
	for (std::string_view Pattern : ForbiddenCMakePatterns)
	{
		bPassed &= Require(
			CMakeText.find(Pattern) == std::string::npos,
			"standalone CMake target contains forbidden input: " + std::string(Pattern));
	}

	for (const fs::path& RequiredCompatHeader : {
		CompatRoot / "CoreMinimal.h",
		CompatRoot / "CoreTypes.h",
		CompatRoot / "UECompat.h",
		CompatRoot / "AngelscriptEngine.h",
		CompatRoot / "AngelscriptSettings.h",
		CompatRoot / "ClassGenerator/ASClass.h"})
	{
		bPassed &= Require(
			fs::exists(RequiredCompatHeader),
			"required standalone compatibility header is missing: "
				+ RequiredCompatHeader.string());
	}

	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(StandaloneRoot))
	{
		if (Entry.path().lexically_relative(StandaloneRoot).begin()->string() == "out")
		{
			continue;
		}
		if (Entry.is_regular_file())
		{
			const std::string FileName = Entry.path().filename().string();
			if (FileName == "CoreMinimal.h" || FileName == "CoreTypes.h")
			{
				const fs::path Relative = Entry.path().lexically_relative(CompatRoot);
				bPassed &= Require(
					!Relative.empty() && *Relative.begin() != "..",
					"UE compatibility entry header escaped Standalone/Compat: "
						+ Entry.path().string());
			}
		}
	}

	for (const fs::directory_entry& Entry
		: fs::recursive_directory_iterator(StandaloneSourceRoot))
	{
		if (!Entry.is_regular_file()
			|| (Entry.path().extension() != ".h"
				&& Entry.path().extension() != ".cpp"))
		{
			continue;
		}
		bPassed &= Require(
			!HasForbiddenStandaloneHostDependency(
				RemoveCommentOnlyLines(ReadText(Entry.path()))),
			"standalone host source has an Unreal dependency: "
				+ Entry.path().string());
	}

	for (const fs::path& ProtectedRoot
		: {BindingRoot, ClassGeneratorRoot})
	{
		for (const fs::directory_entry& Entry
			: fs::recursive_directory_iterator(ProtectedRoot))
		{
			if (!Entry.is_regular_file()
				|| (Entry.path().extension() != ".h"
					&& Entry.path().extension() != ".cpp"))
			{
				continue;
			}
			bPassed &= Require(
				!HasStandaloneBindingContamination(
					RemoveCommentOnlyLines(ReadText(Entry.path()))),
				"existing bindings/ClassGenerator gained a standalone "
				"branch or exporter hook: " + Entry.path().string());
		}
	}

	const fs::path RuntimeBuildFile =
		PluginRoot / "Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs";
	if (fs::exists(RuntimeBuildFile))
	{
		bPassed &= Require(
			ReadText(RuntimeBuildFile).find("Standalone/Compat") == std::string::npos,
			"UBT Runtime module imports the CMake-only compatibility facade");
	}
	const fs::path RuntimeModule =
		PluginRoot / "Source/AngelscriptRuntime/Core/AngelscriptRuntimeModule.cpp";
	bPassed &= Require(
		ReadText(RuntimeModule).find("AngelscriptSDKHostServices") == std::string::npos,
		"UE Runtime still installs the standalone SDK host-service table");
	bPassed &= Require(
		!fs::exists(
			PluginRoot
			/ "Source/AngelscriptRuntime/Core/AngelscriptSDKHostServices.h")
			&& !fs::exists(
				PluginRoot
				/ "Source/AngelscriptRuntime/Core/AngelscriptSDKHostServices.cpp"),
		"obsolete UE Runtime SDK host-service adapter remains present");

	const fs::path ProjectFile = BuildRoot / "AngelscriptMaintainedFork.vcxproj";
	bPassed &= Require(fs::exists(ProjectFile), "generated maintained-fork project is missing");
	if (fs::exists(ProjectFile))
	{
		const std::string ProjectText = ReadText(ProjectFile);
		bPassed &= Require(
			ProjectText.find("Unreal Engine") == std::string::npos &&
			ProjectText.find("Engine\\\\Source") == std::string::npos &&
			ProjectText.find(".generated.h") == std::string::npos,
			"generated compile/link target contains an Unreal or generated-code path");
		bPassed &= Require(
			ProjectText.find("Standalone\\Compat") != std::string::npos
				|| ProjectText.find("Standalone/Compat") != std::string::npos,
			"generated maintained-fork target does not contain the compatibility include path");
	}
	const fs::path SolutionFile = BuildRoot / "AngelscriptStandalone.sln";
	bPassed &= Require(
		fs::exists(SolutionFile),
		"generated Standalone solution is missing");
	if (fs::exists(SolutionFile))
	{
		const std::string SolutionText = ReadText(SolutionFile);
		bPassed &= Require(
			SolutionText.find("AngelscriptLanguageCore") == std::string::npos,
			"generated solution still exposes a LanguageCore target");
	}
	const fs::path HostProjectFile = BuildRoot / "AngelscriptStandaloneHost.vcxproj";
	bPassed &= Require(
		fs::exists(HostProjectFile),
		"generated Standalone Host project is missing");
	if (fs::exists(HostProjectFile))
	{
		const std::string ProjectText = ReadText(HostProjectFile);
		bPassed &= Require(
			ProjectText.find("Unreal Engine") == std::string::npos
				&& ProjectText.find("Engine\\\\Source") == std::string::npos
				&& ProjectText.find(".generated.h") == std::string::npos
				&& ProjectText.find("AngelscriptRuntime\\Language") == std::string::npos
				&& ProjectText.find("AngelscriptRuntime/Language") == std::string::npos,
			"generated Standalone Host target contains Unreal or Runtime Language paths");
	}

	return bPassed ? 0 : 1;
}
