// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript TypedASTJIT Provider Entry Probe
 * Declaration : int SemanticScalarBranch(int, int)
 * Source      : /Angelscript/Game/ASStaticJITAotFixture.as
 * Entry ABI   : 108ad1ef8d5103b1a80d679116c872a1dc9d3ee674beaa1115ec65b288287e69
 * Scope       : test-only compiled raw/VM/Parms carrier
 */
#include "AngelscriptTestJITProbes.h"
#include "CoreMinimal.h"
#include "StaticJIT/StaticJITHeader.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"

namespace
{
static int32 AS_Test_TypedASTProvider_SemanticScalarBranch_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1)
{
	TypedFrame.SetSourcePosition(10, 6);
	const int32 as_sem_s2 = ([&]() -> int32
	{
		const int32 as_sem_e2_left = as_sem_s0;
		const int32 as_sem_e2_right = as_sem_s1;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e2_left, as_sem_e2_right);
	}());
	TypedFrame.SetSourcePosition(11, 2);
	if (([&]() -> bool
	{
		const int32 as_sem_e5_left = as_sem_s2;
		const int32 as_sem_e5_right = int32(10);
		return (as_sem_e5_left > as_sem_e5_right);
	}()))
	{
		TypedFrame.SetSourcePosition(13, 3);
		return ([&]() -> int32
		{
			const int32 as_sem_e8_left = as_sem_s2;
			const int32 as_sem_e8_right = int32(2);
			return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e8_left, as_sem_e8_right);
		}());
	}
	TypedFrame.SetSourcePosition(15, 2);
	return ([&]() -> int32
	{
		const int32 as_sem_e11_left = as_sem_s2;
		const int32 as_sem_e11_right = int32(1);
		return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_sem_e11_left, as_sem_e11_right);
	}());
}

// AS Function : int SemanticScalarBranch(const int, const int)
// JIT Entry   : Raw
int32 AS_Test_TypedASTProvider_SemanticScalarBranch_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITAotFixture.as",
		"int SemanticScalarBranch(const int, const int)",
		8,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return AS_Test_TypedASTProvider_SemanticScalarBranch_TypedBody(TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int SemanticScalarBranch(const int, const int)
// JIT Entry   : VM
void AS_Test_TypedASTProvider_SemanticScalarBranch_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_TypedASTProvider_SemanticScalarBranch_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int SemanticScalarBranch(const int, const int)
// JIT Entry   : Parms
void AS_Test_TypedASTProvider_SemanticScalarBranch_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_TypedASTProvider_SemanticScalarBranch_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}

	struct FTypedASTScalarProviderEntryRegistration
	{
		FTypedASTScalarProviderEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterTypedASTScalarProviderEntries(
				&AS_Test_TypedASTProvider_SemanticScalarBranch_VM,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_TypedASTProvider_SemanticScalarBranch_Raw),
				&AS_Test_TypedASTProvider_SemanticScalarBranch_Parms);
		}
	};

	FTypedASTScalarProviderEntryRegistration GTypedASTScalarProviderEntryRegistration;
}

/*
 * AngelScript TypedASTJIT Production Capability Carrier
 * Declaration : int TypedASTCapabilityShowcase(int, int, bool, bool)
 * Entry ABI   : 315429bd98bc57e8264be8ad0e7bc5007a550d671431aa6b3c1f76f34b2aa667
 * Body owner  : ASStaticJITAotFixture.c8f22911.EditorDevelopment.jit.cpp
 * Native call : IsRunningCommandlet (production call closure)
 */
extern int32 ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1, asBYTE p_2, asBYTE p_3);
extern void ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872_VM(FScriptExecution&, asDWORD*, asQWORD*);
extern void ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872_Parms(FScriptExecution&, void*, void*);

namespace
{
	struct FTypedASTCapabilityProviderEntryRegistration
	{
		FTypedASTCapabilityProviderEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterTypedASTCapabilityProviderEntries(
				&ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872_VM,
				reinterpret_cast<asJITFunction_Raw>(&ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872_Raw),
				&ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872_Parms);
		}
	};

	FTypedASTCapabilityProviderEntryRegistration GTypedASTCapabilityProviderEntryRegistration;
}
