// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : TypedASTJIT
 * Declaration        : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
 * StableFunctionKey  : 9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4
 * OriginalSymbolSet  : ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_3d287fec96570ae323b5be5f87799c96aff6472b47dfb88cd4de99ab40b03762
 * TestSymbolSet      : AS_Test_Differential_TypedAST_TypedASTPrintShowcase
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
 * Declaration       : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
 * Source             : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:1
 * StableFunctionKey  : 9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4
 * ExecutionHash      : 3d287fec96570ae323b5be5f87799c96aff6472b47dfb88cd4de99ab40b03762
 * DebugHash          : 636443155f3600493c3c1febfd095150c987de2da9db64ac7a9fb82658c7520b
 * EntryAbiHash       : 97fbcd10bc26610e92f0705696227e7d6c0a85c0ece2feeca0b53d76f314e25a
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 AS_Test_Differential_TypedAST_TypedASTPrintShowcase_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const FString& as_sem_s0,
	const double as_sem_s1,
	const FLinearColor& as_sem_s2);

/*
 * AngelScript Native Call Site 0
 * AS Declaration      : void Print(const FString&inout, float32 = 5.f, FLinearColor = FLinearColor :: LucBlue)
 * Emitted C++ Callee  : AngelscriptStaticJITNativeCallables::Print
 * Route               : DirectExported
 * Effective Target    : AngelscriptStaticJITNativeCallables::Print
 * Runtime DLL Core    : <none>
 * Metadata Row        : <none>
 * Reference Slot      : -1
 */
static int32 AS_Test_Differential_TypedAST_TypedASTPrintShowcase_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const FString& as_sem_s0,
	const double as_sem_s1,
	const FLinearColor& as_sem_s2)
{
	TypedFrame.SetSourcePosition(7, 2);
	([&]() -> void
	{
		TypedFrame.SetSourcePosition(7, 2);
		const FLinearColor& as_sem_e6_arg2 = as_sem_s2;
		const float as_sem_e6_arg1 = static_cast<float>(as_sem_s1);
		const FString& as_sem_e6_arg0 = as_sem_s0;
		return AngelscriptStaticJITNativeCallables::Print(as_sem_e6_arg0, as_sem_e6_arg1, as_sem_e6_arg2);
	}());
	TypedFrame.SetSourcePosition(8, 2);
	return int32(314);
}

// AS Function : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:1
// JIT Entry   : Raw
int32 AS_Test_Differential_TypedAST_TypedASTPrintShowcase_Raw(FScriptExecution& Execution, FString* p_0, double p_1, FLinearColor* p_2)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedPrintFixture.as",
		"int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)",
		2,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return AS_Test_Differential_TypedAST_TypedASTPrintShowcase_TypedBody(TypedFrame, *p_0, static_cast<double>(p_1), *p_2);
}

// AS Function : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:1
// JIT Entry   : VM
void AS_Test_Differential_TypedAST_TypedASTPrintShowcase_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_TypedAST_TypedASTPrintShowcase_Raw(Execution,
		*(FString**)(l_fp + 0),
		*(double*)(l_fp + 2),
		*(FLinearColor**)(l_fp + 4));
}

// AS Function : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:1
// JIT Entry   : Parms
void AS_Test_Differential_TypedAST_TypedASTPrintShowcase_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(FString);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(FLinearColor));
	const SIZE_T ParmOffset_2 = ParmsOffset;
	ParmsOffset += sizeof(FLinearColor);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_Differential_TypedAST_TypedASTPrintShowcase_Raw(Execution,
		(FString*)(((SIZE_T)Parms) + ParmOffset_0),
		static_cast<double>(*(double*)(((SIZE_T)Parms) + ParmOffset_1)),
		(FLinearColor*)(((SIZE_T)Parms) + ParmOffset_2));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FTypedASTJITDifferentialEntryRegistrationReferenceSlots[4] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("415ec867beab32a47d558983ab77291ac493547168161b93836dc77a357f059a")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("4966ab4336cca1891b1b828360f0d1a587dad0189ac8dc042b1dde592cb032d5")))}}},
		{sizeof(FAngelscriptJITReferenceSlot), 1u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("cc621ee62270e50578779a415d2789dc2b5e1a70d1ea5a785cbd60e43233c192")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("dfcf882626eca6dff9e5a227f732555955dceaef4baf13aa55a7b1e7839c39ef")))}}},
		{sizeof(FAngelscriptJITReferenceSlot), 2u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("cffdb228484b4358e4f3d607d572055b99963e418b0765f6dd934433adcbfca3")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("86168bbdfe709e4f79413ae5fcdfb97fb18587ec710ec2cd4727e5bb6913597d")))}}},
		{sizeof(FAngelscriptJITReferenceSlot), 3u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("fce382e1f207809532a575043a4e727bc6fb503d99b6b8700fd77d71906a1080")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("207b6b88fbdf7c084cb14ddada1e386a939d1754325d527f699e94d41cd5de56")))}}},
	};

	struct FTypedASTJITDifferentialEntryRegistration
	{
		FTypedASTJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::TypedASTJIT,
				&AS_Test_Differential_TypedAST_TypedASTPrintShowcase_VM,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_TypedAST_TypedASTPrintShowcase_Raw),
				&AS_Test_Differential_TypedAST_TypedASTPrintShowcase_Parms,
				TEXT("AS_Test_Differential_TypedAST_TypedASTPrintShowcase"),
				TEXT("9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4"),
				TEXT("int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedPrintFixture.as"),
				2u,
				1u,
				AngelscriptTestJITProbes::EIsolatedDifferentialCleanupPlanState::VerifiedEmpty,
				0u,
				FTypedASTJITDifferentialEntryRegistrationReferenceSlots,
				4u);
		}
	};

	FTypedASTJITDifferentialEntryRegistration GTypedASTJITDifferentialEntryRegistration;
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
