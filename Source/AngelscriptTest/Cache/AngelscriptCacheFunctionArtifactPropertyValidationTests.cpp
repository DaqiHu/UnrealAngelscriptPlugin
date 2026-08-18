#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#include "Misc/ScopeExit.h"

#include "as_bytecode.h"
#include "as_module.h"
#include "as_restore.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheFunctionArtifactPropertyValidationTests_Private
{
	class FWriteStream final : public asIBinaryStream
	{
	public:
		virtual int Read(void*, asUINT) override
		{
			return asNOT_SUPPORTED;
		}

		virtual int Write(const void*, asUINT) override
		{
			return asSUCCESS;
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheFunctionArtifactPropertyValidationTests,
	"Angelscript.TestModule.Cache.FunctionArtifact.PropertyValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(UnresolvedObjectPropertyOffsetRejectsArtifactWithoutCrashing)
	{
		using namespace
			AngelscriptCacheFunctionArtifactPropertyValidationTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		asIScriptModule* Module = Fixture.BuildModule(
			"FunctionArtifactPropertyValidation",
			TEXT(R"AS(
				class FPropertyProbe
				{
					int Value = 40;
				}

				int ReadProperty()
				{
					FPropertyProbe Probe;
					return Probe.Value + 2;
				}
			)AS"));
		ASSERT_THAT(IsNotNull(Module));
		asCScriptFunction* Function = static_cast<asCScriptFunction*>(
			Module->GetFunctionByDecl("int ReadProperty()"));
		ASSERT_THAT(IsNotNull(Function));

		asUINT BytecodeLength = 0;
		asDWORD* const Bytecode = Function->GetByteCode(&BytecodeLength);
		ASSERT_THAT(IsNotNull(Bytecode));
		short* PropertyOffset = nullptr;
		for (asUINT Offset = 0; Offset < BytecodeLength;)
		{
			const asBYTE Opcode = *reinterpret_cast<asBYTE*>(Bytecode + Offset);
			if (Opcode == asBC_ADDSi || Opcode == asBC_LoadThisR)
			{
				PropertyOffset = reinterpret_cast<short*>(Bytecode + Offset) + 1;
				break;
			}
			if (Opcode == asBC_LoadRObjR || Opcode == asBC_LoadVObjR)
			{
				PropertyOffset = reinterpret_cast<short*>(Bytecode + Offset) + 2;
				break;
			}
			Offset += asBCTypeSize[asBCInfo[Opcode].type];
		}
		ASSERT_THAT(IsNotNull(PropertyOffset));
		const short OriginalOffset = *PropertyOffset;
		*PropertyOffset = MAX_int16;
		ON_SCOPE_EXIT
		{
			*PropertyOffset = OriginalOffset;
		};

		FWriteStream Stream;
		asCWriter Writer(
			static_cast<asCModule*>(Module),
			&Stream,
			Function->engine,
			true);
		ASSERT_THAT(IsTrue(
			Writer.WriteFunctionArtifact(Function) < asSUCCESS));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
