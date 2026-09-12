#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"
#include "../../Support/AngelscriptNativeCompilerBytecodeTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "as_builder.h"
#include "source/as_bytecode_codegen.h"
#include "as_module.h"
#include "as_objecttype.h"
#include "frontend/Parser/as_parser.h"
#include "source/as_scriptcode.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "frontend/Sema/as_sema.h"
#include "EndAngelscriptHeaders.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	template<typename T>
	asUINT CountNonNullPointers(const asCArray<T*>& Values)
	{
		asUINT Count = 0;
		for (asUINT Index = 0; Index < Values.GetLength(); ++Index)
		{
			if (Values[Index] != nullptr)
			{
				++Count;
			}
		}
		return Count;
	}

	template<typename TSymbolMap>
	asUINT CountSymbols(const TSymbolMap& Values)
	{
		asUINT Count = 0;
		Values.IterateAll([&Count](auto*)
		{
			++Count;
		});
		return Count;
	}

	struct FInterfacePublicationSnapshot
	{
		asUINT ModuleFunctions = 0;
		asUINT ModuleScriptFunctions = 0;
		asUINT ModuleClassTypes = 0;
		asUINT ModuleLocalTypes = 0;
		asUINT EngineFunctionSlots = 0;
		asUINT EngineOccupiedFunctions = 0;
		asUINT EngineFreeFunctionIds = 0;
		asUINT EngineGlobalPropertySlots = 0;
		asUINT EngineOccupiedGlobals = 0;
		asUINT EngineFreeGlobalIds = 0;
		asUINT EngineDeclaredTypes = 0;
		asUINT EngineTypeIdMap = 0;
		int EngineTypeIdSequence = 0;
		asEBytecodePublisher Publisher = asBYTECODE_PUBLISHER_NONE;
		bool HasDigest = false;
		asQWORD Digest = 0;
	};

	FInterfacePublicationSnapshot CaptureInterfacePublication(
		asCScriptEngine* Engine,
		asCModule* Module)
	{
		FInterfacePublicationSnapshot Snapshot;
		if (Module != nullptr)
		{
			Snapshot.ModuleFunctions = Module->GetFunctionCount();
			Snapshot.ModuleScriptFunctions = Module->scriptFunctions.GetLength();
			Snapshot.ModuleClassTypes = Module->classTypes.GetLength();
			Snapshot.ModuleLocalTypes = CountSymbols(Module->allLocalTypes);
			Snapshot.Publisher = Module->GetLastBytecodePublisher();
			Snapshot.HasDigest =
				Module->GetLastCanonicalCodeGenAstDigest(Snapshot.Digest);
		}
		if (Engine != nullptr)
		{
			Snapshot.EngineFunctionSlots = Engine->scriptFunctions.GetLength();
			Snapshot.EngineOccupiedFunctions =
				CountNonNullPointers(Engine->scriptFunctions);
			Snapshot.EngineFreeFunctionIds =
				Engine->freeScriptFunctionIds.GetLength();
			Snapshot.EngineGlobalPropertySlots =
				Engine->globalProperties.GetLength();
			Snapshot.EngineOccupiedGlobals =
				CountNonNullPointers(Engine->globalProperties);
			Snapshot.EngineFreeGlobalIds =
				Engine->freeGlobalPropertyIds.GetLength();
			Snapshot.EngineDeclaredTypes =
				CountSymbols(Engine->allScriptDeclaredTypes);
			Snapshot.EngineTypeIdMap =
				static_cast<asUINT>(Engine->mapTypeIdToTypeInfo.Num());
			Snapshot.EngineTypeIdSequence = Engine->typeIdSeqNbr;
		}
		return Snapshot;
	}

	bool InterfacePublicationEquals(
		const FInterfacePublicationSnapshot& A,
		const FInterfacePublicationSnapshot& B)
	{
		return A.ModuleFunctions == B.ModuleFunctions
			&& A.ModuleScriptFunctions == B.ModuleScriptFunctions
			&& A.ModuleClassTypes == B.ModuleClassTypes
			&& A.ModuleLocalTypes == B.ModuleLocalTypes
			&& A.EngineFunctionSlots == B.EngineFunctionSlots
			&& A.EngineOccupiedFunctions == B.EngineOccupiedFunctions
			&& A.EngineFreeFunctionIds == B.EngineFreeFunctionIds
			&& A.EngineGlobalPropertySlots == B.EngineGlobalPropertySlots
			&& A.EngineOccupiedGlobals == B.EngineOccupiedGlobals
			&& A.EngineFreeGlobalIds == B.EngineFreeGlobalIds
			&& A.EngineDeclaredTypes == B.EngineDeclaredTypes
			&& A.EngineTypeIdMap == B.EngineTypeIdMap
			&& A.EngineTypeIdSequence == B.EngineTypeIdSequence
			&& A.Publisher == B.Publisher
			&& A.HasDigest == B.HasDigest
			&& (!A.HasDigest || A.Digest == B.Digest);
	}
}

// Production half of the interface-dispatch gate. SemaAuthority owns exact
// method-relation selection; these tests only allow CodeGen to project those
// already-sealed DeclIds into generation-local Runtime shells and dispatch
// tables.
TEST_CLASS_WITH_FLAGS(FCanonicalASTProductionInterfaceDispatchTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.ProductionCodeGen.InterfaceDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 FindInterfaceOrdinal(
		const asCObjectType* ObjectType,
		const asCObjectType* InterfaceType)
	{
		if (ObjectType == nullptr || InterfaceType == nullptr)
		{
			return INDEX_NONE;
		}
		for (asUINT Index = 0; Index < ObjectType->interfaces.GetLength(); ++Index)
		{
			if (ObjectType->interfaces[Index] == InterfaceType)
			{
				return static_cast<int32>(Index);
			}
		}
		return INDEX_NONE;
	}

	static asCScriptFunction* ResolveRuntimeMethod(
		asCScriptEngine* Engine,
		const asCObjectType* ObjectType,
		asUINT Ordinal)
	{
		if (Engine == nullptr || ObjectType == nullptr
			|| Ordinal >= ObjectType->methods.GetLength())
		{
			return nullptr;
		}
		const int FunctionId = ObjectType->methods[Ordinal];
		return FunctionId >= 0
			&& static_cast<asUINT>(FunctionId) < Engine->scriptFunctions.GetLength()
			? Engine->scriptFunctions[static_cast<asUINT>(FunctionId)]
			: nullptr;
	}

public:
	TEST_METHOD(CanonicalBuildPublishesDeclarationOnlyInterfaceMethods)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			interface ICanonicalProbe
			{
				int Probe(int Value);
			}

			class CanonicalProbe : ICanonicalProbe
			{
				int Probe(int Value)
				{
					return Value + 1;
				}
			}
			)AS");
		const int BuildResult = CompileNativeModule(
			Engine.Get(),
			"CanonicalProductionInterfaceShell",
			ScriptSource.c_str(),
			Module);
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			BuildResult,
			TEXT("a sealed interface declaration must be accepted by detached Canonical CodeGen")));
		ASSERT_THAT(IsNotNull(Module));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}

		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		asCObjectType* const InterfaceType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("ICanonicalProbe"));
		ASSERT_THAT(IsNotNull(InterfaceType));
		if (InterfaceType == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			InterfaceType->isInterfaceDeclaration,
			TEXT("the Runtime shell must preserve explicit interface identity")));
		ASSERT_THAT(AreEqual(0, InterfaceType->size));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			InterfaceType->methods.GetLength(),
			TEXT("the interface shell must publish its exact declaration method")));

		asCScriptFunction* const InterfaceMethod =
			InterfaceType->methods.GetLength() == 1
				? ScriptEngine->scriptFunctions[
					static_cast<asUINT>(InterfaceType->methods[0])]
				: nullptr;
		ASSERT_THAT(IsNotNull(InterfaceMethod));
		if (InterfaceMethod != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asFUNC_INTERFACE),
				static_cast<int32>(InterfaceMethod->funcType),
				TEXT("declaration-only interface methods must not masquerade as script bodies")));
			ASSERT_THAT(IsTrue(
				InterfaceMethod->scriptData == nullptr,
				TEXT("an interface requirement must not own ScriptFunctionData")));
			ASSERT_THAT(IsTrue(
				InterfaceMethod->objectType == InterfaceType,
				TEXT("the requirement method must belong to the exact Runtime interface shell")));
			ASSERT_THAT(AreEqual(
				0,
				InterfaceMethod->vfTableIdx,
				TEXT("the first authored interface requirement owns chunk-local ordinal zero")));
		}

		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("interface publication must be owned by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			InternalModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("the interface shell gate must not execute asCCompiler")));
	}

	TEST_METHOD(InterfaceInheritanceExecutesThroughExactDispatchChunks)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			interface IBaseProbe
			{
				int ProbeBase(int Value);
			}

			interface IDerivedProbe : IBaseProbe
			{
				int ProbeDerived(bool Value);
			}

			class BaseProbe
			{
				int ProbeBase(int Value)
				{
					return Value + 1;
				}
			}

			class DerivedProbe : BaseProbe, IDerivedProbe
			{
				int ProbeDerived(bool Value)
				{
					return Value ? 40 : 0;
				}
			}

			int RunInterfaceDispatch()
			{
				DerivedProbe Object;
				IDerivedProbe InterfaceView = Object;
				return InterfaceView.ProbeBase(1)
					+ InterfaceView.ProbeDerived(true);
			}
			)AS");
		const int BuildResult = CompileNativeModule(
			Engine.Get(),
			"CanonicalProductionInterfaceDispatch",
			ScriptSource.c_str(),
			Module);
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			BuildResult,
			TEXT("interface inheritance must compile from the exact sealed relation graph")));
		ASSERT_THAT(IsNotNull(Module));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}

		asCObjectType* const DerivedType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("DerivedProbe"));
		asCObjectType* const DerivedInterface = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("IDerivedProbe"));
		asCObjectType* const BaseInterface = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("IBaseProbe"));
		ASSERT_THAT(IsNotNull(DerivedType));
		ASSERT_THAT(IsNotNull(DerivedInterface));
		ASSERT_THAT(IsNotNull(BaseInterface));
		if (DerivedType == nullptr || DerivedInterface == nullptr
			|| BaseInterface == nullptr)
		{
			return;
		}

		const int32 DerivedInterfaceOrdinal = FindInterfaceOrdinal(
			DerivedType, DerivedInterface);
		const int32 BaseInterfaceOrdinal = FindInterfaceOrdinal(
			DerivedType, BaseInterface);
		ASSERT_THAT(IsTrue(
			DerivedInterfaceOrdinal != INDEX_NONE,
			TEXT("the class must publish the exact directly implemented interface")));
		ASSERT_THAT(IsTrue(
			BaseInterfaceOrdinal != INDEX_NONE,
			TEXT("the class must publish the transitive base-interface closure")));
		ASSERT_THAT(AreEqual(
			DerivedType->interfaces.GetLength(),
			DerivedType->interfaceVFTOffsets.GetLength(),
			TEXT("each exact interface identity needs one dispatch-chunk offset")));
		if (DerivedInterfaceOrdinal == INDEX_NONE
			|| BaseInterfaceOrdinal == INDEX_NONE
			|| DerivedType->interfaces.GetLength()
				!= DerivedType->interfaceVFTOffsets.GetLength())
		{
			return;
		}

		asCScriptFunction* const DerivedRequirement =
			static_cast<asCScriptFunction*>(
				DerivedInterface->GetMethodByDecl("int ProbeDerived(bool)"));
		asCScriptFunction* const BaseRequirement =
			static_cast<asCScriptFunction*>(
				BaseInterface->GetMethodByDecl("int ProbeBase(int)"));
		ASSERT_THAT(IsNotNull(DerivedRequirement));
		ASSERT_THAT(IsNotNull(BaseRequirement));
		if (DerivedRequirement == nullptr || BaseRequirement == nullptr)
		{
			return;
		}

		const asUINT DerivedDispatchIndex =
			DerivedType->interfaceVFTOffsets[
				static_cast<asUINT>(DerivedInterfaceOrdinal)]
			+ static_cast<asUINT>(DerivedRequirement->vfTableIdx);
		const asUINT BaseDispatchIndex =
			DerivedType->interfaceVFTOffsets[
				static_cast<asUINT>(BaseInterfaceOrdinal)]
			+ static_cast<asUINT>(BaseRequirement->vfTableIdx);
		ASSERT_THAT(IsTrue(
			DerivedRequirement->vfTableIdx >= 0
				&& BaseRequirement->vfTableIdx >= 0
				&& DerivedDispatchIndex < DerivedType->virtualFunctionTable.GetLength()
				&& BaseDispatchIndex < DerivedType->virtualFunctionTable.GetLength(),
			TEXT("both exact requirements must address an installed class dispatch entry")));
		if (DerivedRequirement->vfTableIdx < 0
			|| BaseRequirement->vfTableIdx < 0
			|| DerivedDispatchIndex >= DerivedType->virtualFunctionTable.GetLength()
			|| BaseDispatchIndex >= DerivedType->virtualFunctionTable.GetLength())
		{
			return;
		}

		asCScriptFunction* const DerivedImplementation =
			DerivedType->virtualFunctionTable[DerivedDispatchIndex];
		asCScriptFunction* const BaseImplementation =
			DerivedType->virtualFunctionTable[BaseDispatchIndex];
		ASSERT_THAT(IsTrue(
			DerivedImplementation != nullptr
				&& DerivedImplementation->canonicalASTStableDeclKey.Equals(
					"DerivedProbe::ProbeDerived(bool)"),
			TEXT("the bool chunk entry must come from the exact implementation DeclId")));
		ASSERT_THAT(IsTrue(
			BaseImplementation != nullptr
				&& BaseImplementation->canonicalASTStableDeclKey.Equals(
					"BaseProbe::ProbeBase(int)"),
			TEXT("the inherited int chunk entry must come from the exact implementation DeclId")));

		asIScriptFunction* const Entry = Module->GetFunctionByDecl(
			"int RunInterfaceDispatch()");
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(
				Entry, asBC_CALLINTF),
			TEXT("an interface-typed source call must lower to VM interface dispatch")));
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher()));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			InternalModule->GetLastLegacyCompilerInvocationCount()));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(
				*TestRunner,
				Engine.Get(),
				Module,
				"int RunInterfaceDispatch()",
				Value)
				&& Value == 42,
			*FString::Printf(
				TEXT("VM interface dispatch must execute inherited int + derived bool implementations; got=%d"),
				Value)));
	}

	TEST_METHOD(InheritedClassImplementationSatisfiesDerivedInterfaceAtRuntime)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			interface IInheritedProbe
			{
				int Probe(int Value);
			}

			class ProbeBase
			{
				int Probe(int Value)
				{
					return Value + 41;
				}
			}

			class ProbeDerived : ProbeBase, IInheritedProbe
			{
			}

			int RunInheritedInterfaceDispatch()
			{
				ProbeDerived Object;
				IInheritedProbe InterfaceView = Object;
				return InterfaceView.Probe(1);
			}
			)AS");
		ASSERT_THAT(AreEqual(asSUCCESS, CompileNativeModule(
			Engine.Get(),
			"CanonicalInheritedInterfaceImplementation",
			ScriptSource.c_str(),
			Module)));
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		asCObjectType* const Derived = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("ProbeDerived"));
		asCObjectType* const Interface = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("IInheritedProbe"));
		ASSERT_THAT(IsNotNull(Derived));
		ASSERT_THAT(IsNotNull(Interface));
		if (Derived == nullptr || Interface == nullptr)
		{
			return;
		}
		const int32 InterfaceOrdinal = FindInterfaceOrdinal(Derived, Interface);
		ASSERT_THAT(IsTrue(InterfaceOrdinal != INDEX_NONE));
		if (InterfaceOrdinal == INDEX_NONE)
		{
			return;
		}
		asCScriptFunction* const Requirement = static_cast<asCScriptFunction*>(
			Interface->GetMethodByDecl("int Probe(int)"));
		ASSERT_THAT(IsNotNull(Requirement));
		if (Requirement == nullptr)
		{
			return;
		}
		const asUINT DispatchIndex =
			Derived->interfaceVFTOffsets[static_cast<asUINT>(InterfaceOrdinal)]
			+ static_cast<asUINT>(Requirement->vfTableIdx);
		ASSERT_THAT(IsTrue(DispatchIndex < Derived->virtualFunctionTable.GetLength()));
		if (DispatchIndex >= Derived->virtualFunctionTable.GetLength())
		{
			return;
		}
		asCScriptFunction* const Implementation =
			Derived->virtualFunctionTable[DispatchIndex];
		ASSERT_THAT(IsTrue(
			Implementation != nullptr
				&& Implementation->canonicalASTStableDeclKey.Equals(
					"ProbeBase::Probe(int)"),
			TEXT("the derived interface chunk must retain the inherited implementation DeclId")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(
				*TestRunner,
				Engine.Get(),
				Module,
				"int RunInheritedInterfaceDispatch()",
				Value)
				&& Value == 42,
			TEXT("an inherited class implementation must execute through the derived interface chunk")));
	}

	TEST_METHOD(OverloadedInterfaceCallUsesSealedRequirementEdge)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			interface IOverloadedProbe
			{
				int Probe(int Value);
				int Probe(bool Value);
			}

			class OverloadedProbe : IOverloadedProbe
			{
				int Probe(int Value)
				{
					return Value == 1 ? 101 : 0;
				}

				int Probe(bool Value)
				{
					return Value ? 202 : 0;
				}
			}

			int RunOverloadedInterfaceDispatch()
			{
				OverloadedProbe Object;
				IOverloadedProbe InterfaceView = Object;
				return InterfaceView.Probe(1) + InterfaceView.Probe(true);
			}
			)AS");
		ASSERT_THAT(AreEqual(asSUCCESS, CompileNativeModule(
			Engine.Get(),
			"CanonicalOverloadedInterfaceDispatch",
			ScriptSource.c_str(),
			Module)));
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}
		asCObjectType* const Interface = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("IOverloadedProbe"));
		ASSERT_THAT(IsNotNull(Interface));
		if (Interface == nullptr)
		{
			return;
		}
		asCScriptFunction* const IntegerRequirement =
			static_cast<asCScriptFunction*>(
				Interface->GetMethodByDecl("int Probe(int)"));
		asCScriptFunction* const BooleanRequirement =
			static_cast<asCScriptFunction*>(
				Interface->GetMethodByDecl("int Probe(bool)"));
		ASSERT_THAT(IsNotNull(IntegerRequirement));
		ASSERT_THAT(IsNotNull(BooleanRequirement));
		if (IntegerRequirement == nullptr || BooleanRequirement == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, IntegerRequirement->vfTableIdx,
			TEXT("the first authored interface requirement owns chunk ordinal zero")));
		ASSERT_THAT(AreEqual(1, BooleanRequirement->vfTableIdx,
			TEXT("the second authored interface requirement owns chunk ordinal one")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(
				*TestRunner,
				Engine.Get(),
				Module,
				"int RunOverloadedInterfaceDispatch()",
				Value)
				&& Value == 303,
			*FString::Printf(
				TEXT("the exact int/bool requirement edges must select 101 and 202; got=%d"),
				Value)));
	}

	TEST_METHOD(SiblingIdenticalRequirementsUseLegacySignatureDeduplication)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			interface ILeftRequirement
			{
				int Shared(int Value);
			}

			interface IRightRequirement
			{
				int Shared(int Value);
			}

			interface IDiamondRequirement : ILeftRequirement, IRightRequirement
			{
			}

			class DiamondRequirementImpl : IDiamondRequirement
			{
				int Shared(int Value)
				{
					return Value + 20;
				}
			}

			int RunDiamondRequirementDispatch()
			{
				DiamondRequirementImpl Object;
				ILeftRequirement Left = Object;
				IRightRequirement Right = Object;
				return Left.Shared(1) + Right.Shared(1);
			}
			)AS");
		ASSERT_THAT(AreEqual(asSUCCESS, CompileNativeModule(
			Engine.Get(),
			"CanonicalDiamondInterfaceRequirement",
			ScriptSource.c_str(),
			Module)));
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		asCObjectType* const DiamondInterface = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("IDiamondRequirement"));
		asCObjectType* const LeftInterface = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("ILeftRequirement"));
		asCObjectType* const RightInterface = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("IRightRequirement"));
		asCObjectType* const ImplementationType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("DiamondRequirementImpl"));
		ASSERT_THAT(IsNotNull(DiamondInterface));
		ASSERT_THAT(IsNotNull(LeftInterface));
		ASSERT_THAT(IsNotNull(RightInterface));
		ASSERT_THAT(IsNotNull(ImplementationType));
		if (DiamondInterface == nullptr || LeftInterface == nullptr
			|| RightInterface == nullptr || ImplementationType == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(1),
			DiamondInterface->methods.GetLength(),
			TEXT("legacy interface inventory de-duplicates sibling requirements by complete signature")));
		asCScriptFunction* const InheritedRequirement = ResolveRuntimeMethod(
			ScriptEngine, DiamondInterface, 0);
		ASSERT_THAT(IsTrue(
			InheritedRequirement != nullptr
				&& InheritedRequirement->objectType == LeftInterface,
			TEXT("the first direct base owns the retained inherited requirement")));
		asCScriptFunction* const Implementation = static_cast<asCScriptFunction*>(
			ImplementationType->GetMethodByDecl("int Shared(int)"));
		ASSERT_THAT(IsNotNull(Implementation));
		ASSERT_THAT(IsTrue(
			DiamondInterface->interfaces.GetLength() == 2
				&& DiamondInterface->interfaces[0] == LeftInterface
				&& DiamondInterface->interfaces[1] == RightInterface,
			TEXT("the derived interface must preserve both exact defining interfaces")));
		ASSERT_THAT(IsTrue(
			Implementation != nullptr && Implementation->vfTableIdx == 0
				&& ImplementationType->interfaces.GetLength() == 3
				&& ImplementationType->interfaces[0] == DiamondInterface
				&& ImplementationType->interfaces[1] == LeftInterface
				&& ImplementationType->interfaces[2] == RightInterface
				&& ImplementationType->interfaceVFTOffsets.GetLength() == 3
				&& ImplementationType->interfaceVFTOffsets[0] == 1
				&& ImplementationType->interfaceVFTOffsets[1] == 1
				&& ImplementationType->interfaceVFTOffsets[2] == 2
				&& ImplementationType->virtualFunctionTable.GetLength() == 3
				&& ImplementationType->virtualFunctionTable[0] == Implementation
				&& ImplementationType->virtualFunctionTable[1] == Implementation
				&& ImplementationType->virtualFunctionTable[2] == Implementation,
			TEXT("an empty derived-interface chunk and two sibling chunks must retain exact legacy offsets")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(
				*TestRunner,
				Engine.Get(),
				Module,
				"int RunDiamondRequirementDispatch()",
				Value)
				&& Value == 42,
			TEXT("both sibling interface chunks must still dispatch after inventory de-duplication")));
	}

	TEST_METHOD(ClassInterfaceClosurePreservesLegacyDepthFirstOrder)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			interface IClosureRoot
			{
			}

			interface IClosureLeaf : IClosureRoot
			{
			}

			interface IClosureSibling
			{
			}

			interface IBaseTail
			{
			}

			class ClosureBase : IBaseTail
			{
			}

			class ClosureOrdered : ClosureBase, IClosureLeaf, IClosureSibling
			{
			}
			)AS");
		ASSERT_THAT(AreEqual(asSUCCESS, CompileNativeModule(
			Engine.Get(),
			"CanonicalInterfaceClosureOrder",
			ScriptSource.c_str(),
			Module)));
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		asCObjectType* const ClassType = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("ClosureOrdered"));
		asCObjectType* const Leaf = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("IClosureLeaf"));
		asCObjectType* const Root = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("IClosureRoot"));
		asCObjectType* const Sibling = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("IClosureSibling"));
		asCObjectType* const BaseTail = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("IBaseTail"));
		ASSERT_THAT(IsNotNull(ClassType));
		ASSERT_THAT(IsNotNull(Leaf));
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Sibling));
		ASSERT_THAT(IsNotNull(BaseTail));
		if (ClassType == nullptr || Leaf == nullptr || Root == nullptr
			|| Sibling == nullptr || BaseTail == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<asUINT>(4),
			ClassType->interfaces.GetLength()));
		if (ClassType->interfaces.GetLength() == 4)
		{
			ASSERT_THAT(IsTrue(
				ClassType->interfaces[0] == Leaf
					&& ClassType->interfaces[1] == Root
					&& ClassType->interfaces[2] == Sibling
					&& ClassType->interfaces[3] == BaseTail,
				TEXT("class interfaces must preserve explicit DFS/preorder before inherited class interfaces")));
		}
		ASSERT_THAT(AreEqual(
			ClassType->interfaces.GetLength(),
			ClassType->interfaceVFTOffsets.GetLength()));
		ASSERT_THAT(IsTrue(
			ClassType->interfaceVFTOffsets.GetLength() == 4
				&& ClassType->interfaceVFTOffsets[0] == 0
				&& ClassType->interfaceVFTOffsets[1] == 0
				&& ClassType->interfaceVFTOffsets[2] == 0
				&& ClassType->interfaceVFTOffsets[3] == 0
				&& ClassType->virtualFunctionTable.GetLength() == 0,
			TEXT("empty explicit and inherited interface chunks may share the exact terminal offset")));
	}

	TEST_METHOD(DerivedOrdinarySlotFollowsCopiedBaseInterfaceChunk)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* Module = nullptr;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			interface IBaseChunk
			{
				int BaseProbe();
			}

			class LayoutBase : IBaseChunk
			{
				int BaseProbe()
				{
					return 10;
				}
			}

			class LayoutDerived : LayoutBase
			{
				int DerivedProbe()
				{
					return 32;
				}
			}

			int RunInheritedChunkLayout()
			{
				LayoutDerived Object;
				IBaseChunk InterfaceView = Object;
				return Object.DerivedProbe() + InterfaceView.BaseProbe();
			}
			)AS");
		ASSERT_THAT(AreEqual(asSUCCESS, CompileNativeModule(
			Engine.Get(),
			"CanonicalInheritedInterfaceChunkLayout",
			ScriptSource.c_str(),
			Module)));
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		asCObjectType* const Base = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("LayoutBase"));
		asCObjectType* const Derived = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("LayoutDerived"));
		asCObjectType* const Interface = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("IBaseChunk"));
		ASSERT_THAT(IsNotNull(Base));
		ASSERT_THAT(IsNotNull(Derived));
		ASSERT_THAT(IsNotNull(Interface));
		if (Base == nullptr || Derived == nullptr || Interface == nullptr)
		{
			return;
		}
		asCScriptFunction* const BaseMethod = static_cast<asCScriptFunction*>(
			Base->GetMethodByDecl("int BaseProbe()"));
		asCScriptFunction* const DerivedMethod = static_cast<asCScriptFunction*>(
			Derived->GetMethodByDecl("int DerivedProbe()"));
		ASSERT_THAT(IsNotNull(BaseMethod));
		ASSERT_THAT(IsNotNull(DerivedMethod));
		ASSERT_THAT(IsTrue(
			BaseMethod != nullptr && BaseMethod->vfTableIdx == 0
				&& Base->interfaceVFTOffsets.GetLength() == 1
				&& Base->interfaceVFTOffsets[0] == 1
				&& Base->virtualFunctionTable.GetLength() == 2
				&& Base->virtualFunctionTable[0] == BaseMethod
				&& Base->virtualFunctionTable[1] == BaseMethod,
			TEXT("the base table must establish its ordinary slot and first interface chunk before derivation")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2),
			Base->virtualFunctionTable.GetLength(),
			TEXT("base layout is one ordinary slot followed by its interface chunk")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(4),
			Derived->virtualFunctionTable.GetLength(),
			TEXT("derived layout copies the complete base table, adds its method, then rebuilds the interface chunk")));
		ASSERT_THAT(AreEqual(2, DerivedMethod != nullptr
			? DerivedMethod->vfTableIdx : -1));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			Derived->interfaceVFTOffsets.GetLength()));
		if (BaseMethod != nullptr && DerivedMethod != nullptr
			&& Derived->virtualFunctionTable.GetLength() == 4
			&& Derived->interfaceVFTOffsets.GetLength() == 1)
		{
			ASSERT_THAT(IsTrue(
				Derived->virtualFunctionTable[0] == BaseMethod
					&& Derived->virtualFunctionTable[1] == BaseMethod
					&& Derived->virtualFunctionTable[2] == DerivedMethod
					&& Derived->interfaceVFTOffsets[0] == 3
					&& Derived->virtualFunctionTable[3] == BaseMethod,
				TEXT("derived vtable must retain copied base chunks before new ordinary slots and fresh chunks")));
		}

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(
				*TestRunner,
				Engine.Get(),
				Module,
				"int RunInheritedChunkLayout()",
				Value)
				&& Value == 42));
	}

	TEST_METHOD(DispatchPreparationFailurePublishesNoEngineOrModuleEntries)
	{
		using namespace AngelscriptBuilderTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine, "CanonicalInterfaceDispatchPreparationFailure");
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			interface IPreparedFailureProbe
			{
				int Probe(int Value);
			}

			class PreparedFailureProbe : IPreparedFailureProbe
			{
				int Probe(int Value)
				{
					return Value + 1;
				}
			}
			)AS");
		Code.SetCode(
			"CanonicalInterfaceDispatchPreparationFailure.as",
			ScriptSource.c_str(),
			true);
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(asSUCCESS, Parser.ParseScript(&Code)));
		Sema.LayoutScriptClassFields();
		ASSERT_THAT(AreEqual(asSUCCESS, Context.Seal()));

		const FInterfacePublicationSnapshot Before =
			CaptureInterfacePublication(ScriptEngine, Module);
		asCBytecodeCodeGen CodeGen;
		CodeGen.SetTestFailureAfterInterfaceDispatchPreparation(
			asCONTEXT_NOT_FINISHED);
		ASSERT_THAT(AreEqual(
			asCONTEXT_NOT_FINISHED,
			CodeGen.Generate(Context, Module),
			TEXT("the explicit post-dispatch-preparation boundary must reject before Commit")));
		ASSERT_THAT(IsTrue(
			CodeGen.WasTestInterfaceDispatchPreparationFailureInjected(),
			TEXT("the rollback gate must prove it reached the exact dispatch boundary")));
		ASSERT_THAT(IsTrue(
			InterfacePublicationEquals(
				Before,
				CaptureInterfacePublication(ScriptEngine, Module)),
			TEXT("dispatch-plan Abandon must preserve every Engine/module slot, free list, TypeId map, publisher and digest")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetTypeInfoByName("IPreparedFailureProbe")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetTypeInfoByName("PreparedFailureProbe")));
		Module->Discard();
	}

	TEST_METHOD(FailedInterfaceReplacementKeepsLastGoodGenerationExecutable)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL)));
		asIScriptModule* const Module = ScriptEngine->GetModule(
			"CanonicalInterfaceLastGood", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		if (Module == nullptr)
		{
			return;
		}

		const std::string GenerationA = ASTEST_AS_ANSI(R"AS(
			interface IStableProbe
			{
				int Probe(int Value);
			}

			class StableProbe : IStableProbe
			{
				int Probe(int Value)
				{
					return Value + 41;
				}
			}

			int StableEntry()
			{
				StableProbe Object;
				IStableProbe InterfaceView = Object;
				return InterfaceView.Probe(1);
			}
			)AS");
		ASSERT_THAT(AreEqual(asSUCCESS, Module->AddScriptSection(
			"CanonicalInterfaceLastGood.as",
			GenerationA.c_str(),
			static_cast<asUINT>(GenerationA.size()))));
		ASSERT_THAT(AreEqual(asSUCCESS, Module->Build()));
		asCObjectType* const InterfaceA = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("IStableProbe"));
		asCObjectType* const ClassA = static_cast<asCObjectType*>(
			Module->GetTypeInfoByName("StableProbe"));
		ASSERT_THAT(IsNotNull(InterfaceA));
		ASSERT_THAT(IsNotNull(ClassA));
		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		asQWORD DigestA = 0;
		ASSERT_THAT(IsTrue(
			InternalModule->GetLastCanonicalCodeGenAstDigest(DigestA)));
		int32 InitialValue = 0;
		ASSERT_THAT(IsTrue(CanonicalExecuteInt(
			*TestRunner,
			Engine.Get(),
			Module,
			"int StableEntry()",
			InitialValue)
			&& InitialValue == 42));

		const std::string InvalidGenerationB = ASTEST_AS_ANSI(R"AS(
			interface IStableProbe
			{
				int Probe(int Value);
			}

			class StableProbe : IStableProbe
			{
			}

			int ReplacementEntry()
			{
				return 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(asSUCCESS, Module->AddScriptSection(
			"CanonicalInterfaceLastGood.as",
			InvalidGenerationB.c_str(),
			static_cast<asUINT>(InvalidGenerationB.size()))));
		ASSERT_THAT(IsTrue(Module->Build() < 0,
			TEXT("a class missing its exact interface implementation must reject generation B")));

		asQWORD DigestAfter = 0;
		ASSERT_THAT(IsTrue(
			InternalModule->GetLastCanonicalCodeGenAstDigest(DigestAfter)
				&& DigestAfter == DigestA,
			TEXT("failed interface replacement must preserve the last-good AST/codegen digest")));
		ASSERT_THAT(IsTrue(
			Module->GetTypeInfoByName("IStableProbe") == InterfaceA
				&& Module->GetTypeInfoByName("StableProbe") == ClassA,
			TEXT("failed interface replacement must preserve exact generation-A Runtime types")));
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int ReplacementEntry()")));
		int32 RetainedValue = 0;
		ASSERT_THAT(IsTrue(
			CanonicalExecuteInt(
				*TestRunner,
				Engine.Get(),
				Module,
				"int StableEntry()",
				RetainedValue)
				&& RetainedValue == 42,
			TEXT("failed interface replacement must keep generation A executable")));
	}
};

#endif
