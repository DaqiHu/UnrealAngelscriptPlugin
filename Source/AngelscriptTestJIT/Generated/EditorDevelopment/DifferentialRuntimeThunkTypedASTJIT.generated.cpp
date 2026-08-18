// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : TypedASTJIT
 * Declaration        : int TypedASTRuntimeThunkShowcase(const FString&in)
 * StableFunctionKey  : 59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e
 * OriginalSymbolSet  : ASJIT_59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e_903fda0724edd107a42dab536e0562a329f5aa488514a2cf71fc6942fb4a9cb6
 * TestSymbolSet      : AS_Test_Differential_TypedAST_TypedASTRuntimeThunkShowcase
 * CleanupPlan       : VerifiedEmpty (0 actions)
 * Publication        : test-only entry accessors; no Provider registration
 */

#if WITH_EDITOR && UE_BUILD_DEVELOPMENT
#include "StaticJIT/StaticJITConfig.h"
#ifndef AS_SKIP_JITTED_CODE
#include "AngelscriptTestJITProbes.h"
#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallables.h"
#include "StaticJIT/StaticJITHeader.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"

/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTRuntimeThunkShowcase(const FString&in)
 * Source             : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:8:1
 * StableFunctionKey  : 59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e
 * ExecutionHash      : 903fda0724edd107a42dab536e0562a329f5aa488514a2cf71fc6942fb4a9cb6
 * DebugHash          : 84c4c455833d298a19e72e37e3dd5fd456053781b60624be02a142f198bb4733
 * EntryAbiHash       : eafaed37b176f7dc1025f3c86200feb10fb066ef6f083b8ce31d94120dc42f21
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 AS_Test_Differential_TypedAST_TypedASTRuntimeThunkShowcase_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const FString& as_sem_s0);

/*
 * AngelScript Native Call Site 0
 * AS Declaration      : int TypedASTRuntimeThunkProbe(const FString&in)
 * Emitted C++ Callee  : AngelscriptStaticJITNativeCallables::RuntimeThunkProbe
 * Route               : RuntimeThunk
 * Effective Target    : AngelscriptStaticJITNativeCallables::RuntimeThunkProbe
 * Runtime DLL Core    : <none>
 * Metadata Row        : <none>
 * Reference Slot      : -1
 */
static int32 AS_Test_Differential_TypedAST_TypedASTRuntimeThunkShowcase_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const FString& as_sem_s0)
{
	TypedFrame.SetSourcePosition(10, 2);
	return ([&]() -> int32
	{
		TypedFrame.SetSourcePosition(10, 9);
		const FString& as_sem_e2_arg0 = as_sem_s0;
		return AngelscriptStaticJITNativeCallables::RuntimeThunkProbe(as_sem_e2_arg0);
	}());
}

// AS Function : int TypedASTRuntimeThunkShowcase(const FString&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:8:1
// JIT Entry   : Raw
int32 AS_Test_Differential_TypedAST_TypedASTRuntimeThunkShowcase_Raw(FScriptExecution& Execution, FString* p_0)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as",
		"int TypedASTRuntimeThunkShowcase(const FString&in)",
		8,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return AS_Test_Differential_TypedAST_TypedASTRuntimeThunkShowcase_TypedBody(TypedFrame, *p_0);
}

// AS Function : int TypedASTRuntimeThunkShowcase(const FString&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:8:1
// JIT Entry   : VM
void AS_Test_Differential_TypedAST_TypedASTRuntimeThunkShowcase_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_TypedAST_TypedASTRuntimeThunkShowcase_Raw(Execution,
		*(FString**)(l_fp + 0));
}

// AS Function : int TypedASTRuntimeThunkShowcase(const FString&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:8:1
// JIT Entry   : Parms
void AS_Test_Differential_TypedAST_TypedASTRuntimeThunkShowcase_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(FString);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_Differential_TypedAST_TypedASTRuntimeThunkShowcase_Raw(Execution,
		(FString*)(((SIZE_T)Parms) + ParmOffset_0));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FTypedASTJITDifferentialEntryRegistrationReferenceSlots[2] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("11b5263801f1c0eff5669352b69d7e14aefdbe45bf94da7108751575993f81d4")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("fe6ff001ec000eaef3a24f1ace9e45af7036e1e072a9b26ad0c923488701363b")))}}},
		{sizeof(FAngelscriptJITReferenceSlot), 1u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("fce382e1f207809532a575043a4e727bc6fb503d99b6b8700fd77d71906a1080")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("207b6b88fbdf7c084cb14ddada1e386a939d1754325d527f699e94d41cd5de56")))}}},
	};

	struct FTypedASTJITDifferentialEntryRegistration
	{
		FTypedASTJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::TypedASTJIT,
				&AS_Test_Differential_TypedAST_TypedASTRuntimeThunkShowcase_VM,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_TypedAST_TypedASTRuntimeThunkShowcase_Raw),
				&AS_Test_Differential_TypedAST_TypedASTRuntimeThunkShowcase_Parms,
				TEXT("AS_Test_Differential_TypedAST_TypedASTRuntimeThunkShowcase"),
				TEXT("59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e"),
				TEXT("int TypedASTRuntimeThunkShowcase(const FString&in)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as"),
				8u,
				1u,
				AngelscriptTestJITProbes::EIsolatedDifferentialCleanupPlanState::VerifiedEmpty,
				0u,
				FTypedASTJITDifferentialEntryRegistrationReferenceSlots,
				2u);
		}
	};

	FTypedASTJITDifferentialEntryRegistration GTypedASTJITDifferentialEntryRegistration;
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
