// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as
 * CanonicalModuleName : ASStaticJITTypedControlFlowFixture
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : e3b08a3c509f1eb5fad8ca63479dcc6c2638c01694af2a1048e06e1de36d5c99
 * ProviderId          : 7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44
 * ArtifactProfile     : b58efc4c9f40879f0b98bcb54704d1b342ae56948a00e9ddbaa709787226e0c8
 * NativeEnvironment   : 76d620ec7d0db38aa2028996e595f75523f201e7e560c05d8c9000d60d04cd73
 * FunctionCount       : 4
 */

#if WITH_EDITOR && UE_BUILD_DEVELOPMENT
#include "StaticJIT/StaticJITConfig.h"
#ifndef AS_SKIP_JITTED_CODE
#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallables.h"
#include "StaticJIT/StaticJITHeader.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"
constexpr SIZE_T POFFSET_UStaticJITAotFunctionCarrier_StoredValue = Align(sizeof(UObject) + 0, 4);
constexpr SIZE_T PALIGN_UStaticJITAotFunctionCarrier_StoredValue = AlignmentMax(alignof(UObject), 4);
constexpr SIZE_T TALIGN_UStaticJITAotFunctionCarrier = PALIGN_UStaticJITAotFunctionCarrier_StoredValue;
constexpr SIZE_T TSIZE_UStaticJITAotFunctionCarrier = Align(POFFSET_UStaticJITAotFunctionCarrier_StoredValue + 4, TALIGN_UStaticJITAotFunctionCarrier);
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticExhaustiveEnum(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
 * StableFunctionKey  : 0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4
 * ExecutionHash      : 52de55a9a6328e299cf7f8704913c970b8ca29f9de21e7623850392073ca7ecd
 * DebugHash          : 68708e5118144807270fa93f1ddeb85a92b89ed2a131c0fed467dc787a1fbf1b
 * EntryAbiHash       : ac5e9740aa5289dba47d6e5ba3f40c6f1f11e575d2ab2615b198bbbc11568b3d
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_52de55a9a6328e299cf7f8704913c970b8ca29f9de21e7623850392073ca7ecd_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0);

static int32 ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_52de55a9a6328e299cf7f8704913c970b8ca29f9de21e7623850392073ca7ecd_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0)
{
	TypedFrame.SetSourcePosition(103, 23);
	const int8 as_sem_s1 = AngelscriptTypedASTJIT::WrapNarrow<int8>(as_sem_s0);
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	const int32 as_sem_switch_selector_10 = static_cast<int32>(as_sem_s1);
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	switch (as_sem_switch_selector_10)
	{
		case int32(0):
			TypedFrame.SetSourcePosition(106, 2);
			{
				TypedFrame.SetSourcePosition(107, 3);
				return int32(10);
			}
			break;
		case int32(1):
			TypedFrame.SetSourcePosition(108, 2);
			{
				TypedFrame.SetSourcePosition(109, 3);
				return int32(20);
			}
			break;
		case int32(2):
			TypedFrame.SetSourcePosition(110, 2);
			{
				TypedFrame.SetSourcePosition(111, 3);
				return int32(30);
			}
			break;
		default:
			TypedFrame.SetSourcePosition(104, 2);
			FStaticJITFunction::SetSwitchValueInvalidException(Execution);
			return int32{};
	}
}

// AS Function : int SemanticExhaustiveEnum(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
// JIT Entry   : Raw
int32 ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_52de55a9a6328e299cf7f8704913c970b8ca29f9de21e7623850392073ca7ecd_Raw(FScriptExecution& Execution, asDWORD p_0)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as",
		"int SemanticExhaustiveEnum(const int)",
		101,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_52de55a9a6328e299cf7f8704913c970b8ca29f9de21e7623850392073ca7ecd_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int SemanticExhaustiveEnum(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
// JIT Entry   : VM
void ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_52de55a9a6328e299cf7f8704913c970b8ca29f9de21e7623850392073ca7ecd_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_52de55a9a6328e299cf7f8704913c970b8ca29f9de21e7623850392073ca7ecd_Raw(Execution,
		*(asDWORD*)(l_fp + 0));
}

// AS Function : int SemanticExhaustiveEnum(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
// JIT Entry   : Parms
void ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_52de55a9a6328e299cf7f8704913c970b8ca29f9de21e7623850392073ca7ecd_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_52de55a9a6328e299cf7f8704913c970b8ca29f9de21e7623850392073ca7ecd_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticCloneableState(const int, const int, const bool, const bool)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:1
 * StableFunctionKey  : 169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855
 * ExecutionHash      : 7e06d65381801c5779762d86540527e4762fac83929ecf664601b8c302cdb58d
 * DebugHash          : 0d781fb6790454ef631cc4fe361f310dc379e15f5b83eaa513bcc7a04adeff43
 * EntryAbiHash       : 3d05f1b7e35a8891df3c73e1ce6d43c897b0603db3baca8a2f98651f018854ce
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 32
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_7e06d65381801c5779762d86540527e4762fac83929ecf664601b8c302cdb58d_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1,
	const bool as_sem_s2,
	const bool as_sem_s3);

static int32 ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_7e06d65381801c5779762d86540527e4762fac83929ecf664601b8c302cdb58d_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1,
	const bool as_sem_s2,
	const bool as_sem_s3)
{
	TypedFrame.SetSourcePosition(128, 6);
	int32 as_sem_s4 = as_sem_s0;
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	TypedFrame.SetSourcePosition(129, 2);
	const int32 as_sem_switch_selector_181 = static_cast<int32>(as_sem_s1);
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	switch (as_sem_switch_selector_181)
	{
		case int32(0):
			TypedFrame.SetSourcePosition(131, 2);
			{
				TypedFrame.SetSourcePosition(133, 7);
				const int32 as_sem_s5 = ([&]() -> int32
				{
					const int32 as_sem_e29_left = ([&]() -> int32
				{
					const int32 as_sem_e26_left = ([&]() -> int32
				{
					const int32 as_sem_e21_left = ([&]() -> int32
				{
					int32& as_sem_e19_target = as_sem_s4;
					as_sem_e19_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e19_target, int32(1));
					return as_sem_e19_target;
				}());
					const int32 as_sem_e21_right = int32(100);
					return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e21_left, as_sem_e21_right);
				}());
					const int32 as_sem_e26_right = ([&]() -> int32
				{
					const int32 as_sem_e25_left = ([&]() -> int32
				{
					int32& as_sem_e23_target = as_sem_s4;
					as_sem_e23_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e23_target, int32(1));
					return as_sem_e23_target;
				}());
					const int32 as_sem_e25_right = int32(10);
					return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e25_left, as_sem_e25_right);
				}());
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e26_left, as_sem_e26_right);
				}());
					const int32 as_sem_e29_right = ([&]() -> int32
				{
					int32& as_sem_e28_target = as_sem_s4;
					as_sem_e28_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e28_target, int32(1));
					return as_sem_e28_target;
				}());
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e29_left, as_sem_e29_right);
				}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				TypedFrame.SetSourcePosition(137, 3);
				return ([&]() -> int32
				{
					const int32 as_sem_e34_left = ([&]() -> int32
				{
					const int32 as_sem_e32_left = as_sem_s4;
					const int32 as_sem_e32_right = int32(1000);
					return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e32_left, as_sem_e32_right);
				}());
					const int32 as_sem_e34_right = as_sem_s5;
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e34_left, as_sem_e34_right);
				}());
			}
			break;
		case int32(1):
			TypedFrame.SetSourcePosition(139, 2);
			{
				TypedFrame.SetSourcePosition(141, 8);
				const bool as_sem_s6 = ((as_sem_s2 && ([&]() -> bool
				{
					const int32 as_sem_e39_left = ([&]() -> int32
				{
					int32& as_sem_e37_target = as_sem_s4;
					as_sem_e37_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e37_target, int32(1));
					return as_sem_e37_target;
				}());
					const int32 as_sem_e39_right = as_sem_s0;
					return (as_sem_e39_left > as_sem_e39_right);
				}())) && as_sem_s3);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				TypedFrame.SetSourcePosition(143, 3);
				const bool as_sem_if_condition_8 = as_sem_s6;
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				if (as_sem_if_condition_8)
				{
					TypedFrame.SetSourcePosition(145, 4);
					return ([&]() -> int32
					{
						const int32 as_sem_e48_left = ([&]() -> int32
					{
						const int32 as_sem_e46_left = as_sem_s4;
						const int32 as_sem_e46_right = int32(10);
						return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e46_left, as_sem_e46_right);
					}());
						const int32 as_sem_e48_right = int32(1);
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e48_left, as_sem_e48_right);
					}());
				}
				TypedFrame.SetSourcePosition(147, 3);
				return ([&]() -> int32
				{
					const int32 as_sem_e51_left = as_sem_s4;
					const int32 as_sem_e51_right = int32(10);
					return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e51_left, as_sem_e51_right);
				}());
			}
			break;
		case int32(2):
			TypedFrame.SetSourcePosition(149, 2);
			{
				TypedFrame.SetSourcePosition(151, 3);
				([&]() -> int32
				{
					const int32 as_sem_e54_value = int32(3);
					int32& as_sem_e54_target = as_sem_s4;
					as_sem_e54_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e54_target, as_sem_e54_value);
					return as_sem_e54_target;
				}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				TypedFrame.SetSourcePosition(152, 7);
				const int32 as_sem_s7 = ([&]() -> int32
				{
					int32& as_sem_e56_target = as_sem_s4;
					as_sem_e56_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e56_target, int32(1));
					return as_sem_e56_target;
				}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				TypedFrame.SetSourcePosition(153, 7);
				const int32 as_sem_s8 = ([&]() -> int32
				{
					int32& as_sem_e58_target = as_sem_s4;
					const int32 as_sem_e58_old = as_sem_e58_target;
					as_sem_e58_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e58_old, int32(1));
					return as_sem_e58_old;
				}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				TypedFrame.SetSourcePosition(154, 3);
				return ([&]() -> int32
				{
					const int32 as_sem_e67_left = ([&]() -> int32
				{
					const int32 as_sem_e65_left = ([&]() -> int32
				{
					const int32 as_sem_e61_left = as_sem_s4;
					const int32 as_sem_e61_right = int32(100);
					return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e61_left, as_sem_e61_right);
				}());
					const int32 as_sem_e65_right = ([&]() -> int32
				{
					const int32 as_sem_e64_left = as_sem_s7;
					const int32 as_sem_e64_right = int32(10);
					return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e64_left, as_sem_e64_right);
				}());
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e65_left, as_sem_e65_right);
				}());
					const int32 as_sem_e67_right = as_sem_s8;
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e67_left, as_sem_e67_right);
				}());
			}
			break;
		case int32(3):
			TypedFrame.SetSourcePosition(156, 2);
			{
				TypedFrame.SetSourcePosition(158, 7);
				int32 as_sem_s9 = int32(0);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				while ([&]() -> bool
					{
						TypedFrame.SetSourcePosition(159, 3);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return false;
						}
						const bool as_sem_loop_condition = ([&]() -> bool
						{
							const int32 as_sem_e71_left = as_sem_s9;
							const int32 as_sem_e71_right = int32(3);
							return (as_sem_e71_left < as_sem_e71_right);
						}());
						return !UNLIKELY(Execution.bExceptionThrown) && as_sem_loop_condition;
					}())
				{
					TypedFrame.SetSourcePosition(160, 3);
					TypedFrame.SetSourcePosition(161, 4);
					([&]() -> int32
					{
						int32& as_sem_e73_target = as_sem_s9;
						as_sem_e73_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e73_target, int32(1));
						return as_sem_e73_target;
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					TypedFrame.SetSourcePosition(162, 4);
					const bool as_sem_if_condition_22 = ([&]() -> bool
					{
						const int32 as_sem_e76_left = as_sem_s9;
						const int32 as_sem_e76_right = int32(1);
						return (as_sem_e76_left == as_sem_e76_right);
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					if (as_sem_if_condition_22)
					{
						TypedFrame.SetSourcePosition(164, 5);
						continue;
					}
					TypedFrame.SetSourcePosition(166, 4);
					([&]() -> int32
					{
						const int32 as_sem_e79_value = as_sem_s9;
						int32& as_sem_e79_target = as_sem_s4;
						as_sem_e79_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e79_target, as_sem_e79_value);
						return as_sem_e79_target;
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
				}
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				{
					TypedFrame.SetSourcePosition(169, 12);
					int32 as_sem_s10 = int32(0);
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					for (; [&]() -> bool
						{
							TypedFrame.SetSourcePosition(169, 3);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return false;
							}
							const bool as_sem_loop_condition = ([&]() -> bool
							{
								const int32 as_sem_e83_left = as_sem_s10;
								const int32 as_sem_e83_right = int32(4);
								return (as_sem_e83_left < as_sem_e83_right);
							}());
							return !UNLIKELY(Execution.bExceptionThrown) && as_sem_loop_condition;
						}(); [&]()
						{
							TypedFrame.SetSourcePosition(169, 34);
							([&]() -> int32
							{
								int32& as_sem_e85_target = as_sem_s10;
								as_sem_e85_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e85_target, int32(1));
								return as_sem_e85_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return;
							}
						}())
					{
						TypedFrame.SetSourcePosition(170, 3);
						TypedFrame.SetSourcePosition(171, 4);
						const int32 as_sem_switch_selector_41 = static_cast<int32>(as_sem_s10);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						switch (as_sem_switch_selector_41)
						{
							case int32(1):
								TypedFrame.SetSourcePosition(173, 4);
								{
									TypedFrame.SetSourcePosition(174, 5);
									([&]() -> int32
									{
										const int32 as_sem_e91_value = int32(10);
										int32& as_sem_e91_target = as_sem_s4;
										as_sem_e91_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e91_target, as_sem_e91_value);
										return as_sem_e91_target;
									}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									TypedFrame.SetSourcePosition(175, 5);
									continue;
								}
								break;
							case int32(2):
								TypedFrame.SetSourcePosition(176, 4);
								{
									TypedFrame.SetSourcePosition(177, 5);
									([&]() -> int32
									{
										const int32 as_sem_e94_value = int32(20);
										int32& as_sem_e94_target = as_sem_s4;
										as_sem_e94_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e94_target, as_sem_e94_value);
										return as_sem_e94_target;
									}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									TypedFrame.SetSourcePosition(178, 5);
									break;
								}
								break;
							default:
								TypedFrame.SetSourcePosition(179, 4);
								{
									TypedFrame.SetSourcePosition(180, 5);
									([&]() -> int32
									{
										const int32 as_sem_e97_value = as_sem_s10;
										int32& as_sem_e97_target = as_sem_s4;
										as_sem_e97_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e97_target, as_sem_e97_value);
										return as_sem_e97_target;
									}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									TypedFrame.SetSourcePosition(181, 5);
									break;
								}
								break;
						}
						TypedFrame.SetSourcePosition(184, 4);
						const bool as_sem_if_condition_44 = ([&]() -> bool
						{
							const int32 as_sem_e100_left = as_sem_s10;
							const int32 as_sem_e100_right = int32(2);
							return (as_sem_e100_left == as_sem_e100_right);
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						if (as_sem_if_condition_44)
						{
							TypedFrame.SetSourcePosition(186, 5);
							break;
						}
					}
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
				}
				TypedFrame.SetSourcePosition(189, 3);
				return as_sem_s4;
			}
			break;
		case int32(4):
			TypedFrame.SetSourcePosition(191, 2);
			{
				TypedFrame.SetSourcePosition(193, 7);
				int32 as_sem_s11 = int32(0);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				while ([&]() -> bool
					{
						TypedFrame.SetSourcePosition(194, 3);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return false;
						}
						const bool as_sem_loop_condition = ([&]() -> bool
						{
							const int32 as_sem_e108_left = ([&]() -> int32
						{
							const int32 as_sem_e105_left = int32(10);
							const int32 as_sem_e105_right = as_sem_s0;
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e105_left, as_sem_e105_right);
						}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_sem_e108_right = ([&]() -> int32
						{
							int32& as_sem_e107_target = as_sem_s11;
							as_sem_e107_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e107_target, int32(1));
							return as_sem_e107_target;
						}());
							return (as_sem_e108_left > as_sem_e108_right);
						}());
						return !UNLIKELY(Execution.bExceptionThrown) && as_sem_loop_condition;
					}())
				{
					TypedFrame.SetSourcePosition(195, 3);
					TypedFrame.SetSourcePosition(196, 4);
					([&]() -> int32
					{
						const int32 as_sem_e111_value = int32(1);
						int32& as_sem_e111_target = as_sem_s4;
						as_sem_e111_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e111_target, as_sem_e111_value);
						return as_sem_e111_target;
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					TypedFrame.SetSourcePosition(197, 4);
					break;
				}
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				TypedFrame.SetSourcePosition(199, 3);
				return ([&]() -> int32
				{
					const int32 as_sem_e114_left = as_sem_s4;
					const int32 as_sem_e114_right = as_sem_s11;
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e114_left, as_sem_e114_right);
				}());
			}
			break;
		case int32(5):
			TypedFrame.SetSourcePosition(201, 2);
			{
				{
					TypedFrame.SetSourcePosition(203, 12);
					int32 as_sem_s12 = int32(0);
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					for (; [&]() -> bool
						{
							TypedFrame.SetSourcePosition(203, 3);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return false;
							}
							const bool as_sem_loop_condition = ([&]() -> bool
							{
								const int32 as_sem_e118_left = as_sem_s12;
								const int32 as_sem_e118_right = int32(1);
								return (as_sem_e118_left < as_sem_e118_right);
							}());
							return !UNLIKELY(Execution.bExceptionThrown) && as_sem_loop_condition;
						}(); [&]()
						{
							TypedFrame.SetSourcePosition(203, 34);
							([&]() -> int32
							{
								int32& as_sem_e120_target = as_sem_s12;
								as_sem_e120_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e120_target, int32(1));
								return as_sem_e120_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return;
							}
						}())
					{
						TypedFrame.SetSourcePosition(204, 3);
						TypedFrame.SetSourcePosition(205, 4);
						([&]() -> int32
						{
							const int32 as_sem_e125_value = ([&]() -> int32
							{
								const int32 as_sem_e123_left = int32(10);
								const int32 as_sem_e123_right = as_sem_s0;
								return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e123_left, as_sem_e123_right);
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							int32& as_sem_e125_target = as_sem_s4;
							as_sem_e125_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e125_target, as_sem_e125_value);
							return as_sem_e125_target;
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						TypedFrame.SetSourcePosition(206, 4);
						([&]() -> int32
						{
							const int32 as_sem_e128_value = int32(100);
							int32& as_sem_e128_target = as_sem_s4;
							as_sem_e128_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e128_target, as_sem_e128_value);
							return as_sem_e128_target;
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
					}
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
				}
				TypedFrame.SetSourcePosition(208, 3);
				return as_sem_s4;
			}
			break;
		case int32(6):
			TypedFrame.SetSourcePosition(210, 2);
			{
				TypedFrame.SetSourcePosition(212, 7);
				int32 as_sem_s13 = int32(0);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				{
					TypedFrame.SetSourcePosition(213, 12);
					int32 as_sem_s14 = int32(0);
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					for (; [&]() -> bool
						{
							TypedFrame.SetSourcePosition(213, 3);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return false;
							}
							const bool as_sem_loop_condition = ([&]() -> bool
							{
								const int32 as_sem_e134_left = as_sem_s14;
								const int32 as_sem_e134_right = int32(1);
								return (as_sem_e134_left < as_sem_e134_right);
							}());
							return !UNLIKELY(Execution.bExceptionThrown) && as_sem_loop_condition;
						}(); [&]()
						{
							TypedFrame.SetSourcePosition(214, 4);
							([&]() -> int32
							{
								const int32 as_sem_e139_value = ([&]() -> int32
								{
									const int32 as_sem_e137_left = int32(10);
									const int32 as_sem_e137_right = as_sem_s0;
									return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e137_left, as_sem_e137_right);
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return int32{};
								}
								int32& as_sem_e139_target = as_sem_s14;
								as_sem_e139_target = as_sem_e139_value;
								return as_sem_e139_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return;
							}
							TypedFrame.SetSourcePosition(214, 23);
							([&]() -> int32
							{
								int32& as_sem_e141_target = as_sem_s13;
								as_sem_e141_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e141_target, int32(1));
								return as_sem_e141_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return;
							}
						}())
					{
						TypedFrame.SetSourcePosition(215, 3);
						TypedFrame.SetSourcePosition(216, 4);
						([&]() -> int32
						{
							int32& as_sem_e143_target = as_sem_s14;
							as_sem_e143_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e143_target, int32(1));
							return as_sem_e143_target;
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						TypedFrame.SetSourcePosition(217, 4);
						continue;
					}
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
				}
				TypedFrame.SetSourcePosition(219, 3);
				return ([&]() -> int32
				{
					const int32 as_sem_e146_left = as_sem_s4;
					const int32 as_sem_e146_right = as_sem_s13;
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e146_left, as_sem_e146_right);
				}());
			}
			break;
		case int32(7):
			TypedFrame.SetSourcePosition(221, 2);
			{
				TypedFrame.SetSourcePosition(223, 3);
				const int32 as_sem_switch_selector_88 = static_cast<int32>(([&]() -> int32
				{
					const int32 as_sem_e149_left = int32(10);
					const int32 as_sem_e149_right = as_sem_s0;
					return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e149_left, as_sem_e149_right);
				}()));
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				switch (as_sem_switch_selector_88)
				{
					case int32(0):
						TypedFrame.SetSourcePosition(225, 3);
						{
							TypedFrame.SetSourcePosition(226, 4);
							([&]() -> int32
							{
								const int32 as_sem_e153_value = int32(1);
								int32& as_sem_e153_target = as_sem_s4;
								as_sem_e153_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e153_target, as_sem_e153_value);
								return as_sem_e153_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							TypedFrame.SetSourcePosition(227, 4);
							break;
						}
						break;
					default:
						TypedFrame.SetSourcePosition(228, 3);
						{
							TypedFrame.SetSourcePosition(229, 4);
							([&]() -> int32
							{
								const int32 as_sem_e156_value = int32(2);
								int32& as_sem_e156_target = as_sem_s4;
								as_sem_e156_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e156_target, as_sem_e156_value);
								return as_sem_e156_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							TypedFrame.SetSourcePosition(230, 4);
							break;
						}
						break;
				}
				TypedFrame.SetSourcePosition(232, 3);
				return as_sem_s4;
			}
			break;
		case int32(8):
			TypedFrame.SetSourcePosition(234, 2);
			{
				TypedFrame.SetSourcePosition(236, 7);
				int32 as_sem_s15 = int32(0);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				do
				{
					TypedFrame.SetSourcePosition(238, 3);
					TypedFrame.SetSourcePosition(239, 4);
					([&]() -> int32
					{
						int32& as_sem_e160_target = as_sem_s15;
						as_sem_e160_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e160_target, int32(1));
						return as_sem_e160_target;
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					TypedFrame.SetSourcePosition(240, 4);
					continue;
				}
				while ([&]() -> bool
					{
						TypedFrame.SetSourcePosition(237, 3);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return false;
						}
						const bool as_sem_loop_condition = ([&]() -> bool
						{
							const int32 as_sem_e165_left = ([&]() -> int32
						{
							const int32 as_sem_e163_left = int32(10);
							const int32 as_sem_e163_right = as_sem_s0;
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e163_left, as_sem_e163_right);
						}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_sem_e165_right = as_sem_s15;
							return (as_sem_e165_left > as_sem_e165_right);
						}());
						return !UNLIKELY(Execution.bExceptionThrown) && as_sem_loop_condition;
					}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				TypedFrame.SetSourcePosition(243, 3);
				return ([&]() -> int32
				{
					const int32 as_sem_e168_left = as_sem_s4;
					const int32 as_sem_e168_right = as_sem_s15;
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e168_left, as_sem_e168_right);
				}());
			}
			break;
		case int32(10):
			TypedFrame.SetSourcePosition(245, 2);
			{
				do
				{
					TypedFrame.SetSourcePosition(248, 3);
					TypedFrame.SetSourcePosition(249, 4);
					([&]() -> int32
					{
						const int32 as_sem_e173_value = ([&]() -> int32
						{
							const int32 as_sem_e171_left = int32(10);
							const int32 as_sem_e171_right = as_sem_s0;
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e171_left, as_sem_e171_right);
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						int32& as_sem_e173_target = as_sem_s4;
						as_sem_e173_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e173_target, as_sem_e173_value);
						return as_sem_e173_target;
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					TypedFrame.SetSourcePosition(250, 4);
					([&]() -> int32
					{
						const int32 as_sem_e176_value = int32(100);
						int32& as_sem_e176_target = as_sem_s4;
						as_sem_e176_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e176_target, as_sem_e176_value);
						return as_sem_e176_target;
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
				}
				while ([&]() -> bool
					{
						TypedFrame.SetSourcePosition(247, 3);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return false;
						}
						const bool as_sem_loop_condition = AngelscriptTypedASTJIT::FromCanonicalBits<bool>(UINT64_C(0));
						return !UNLIKELY(Execution.bExceptionThrown) && as_sem_loop_condition;
					}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				TypedFrame.SetSourcePosition(253, 3);
				return as_sem_s4;
			}
			break;
		case int32(11):
			TypedFrame.SetSourcePosition(255, 2);
			{
				TypedFrame.SetSourcePosition(257, 7);
				int32 as_sem_s16 = int32(0);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				while ([&]() -> bool
					{
						TypedFrame.SetSourcePosition(258, 3);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return false;
						}
						const bool as_sem_loop_condition = (([&]() -> bool
						{
							const int32 as_sem_e182_left = as_sem_s16;
							const int32 as_sem_e182_right = int32(0);
							return (as_sem_e182_left == as_sem_e182_right);
						}()) || ([&]() -> bool
						{
							const int32 as_sem_e187_left = ([&]() -> int32
						{
							const int32 as_sem_e185_left = int32(10);
							const int32 as_sem_e185_right = as_sem_s0;
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e185_left, as_sem_e185_right);
						}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_sem_e187_right = as_sem_s16;
							return (as_sem_e187_left > as_sem_e187_right);
						}()));
						return !UNLIKELY(Execution.bExceptionThrown) && as_sem_loop_condition;
					}())
				{
					TypedFrame.SetSourcePosition(259, 3);
					TypedFrame.SetSourcePosition(260, 4);
					([&]() -> int32
					{
						int32& as_sem_e190_target = as_sem_s16;
						as_sem_e190_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e190_target, int32(1));
						return as_sem_e190_target;
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					TypedFrame.SetSourcePosition(261, 4);
					continue;
				}
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				TypedFrame.SetSourcePosition(263, 3);
				return ([&]() -> int32
				{
					const int32 as_sem_e193_left = as_sem_s4;
					const int32 as_sem_e193_right = as_sem_s16;
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e193_left, as_sem_e193_right);
				}());
			}
			break;
		case int32(12):
			TypedFrame.SetSourcePosition(265, 2);
			{
				{
					TypedFrame.SetSourcePosition(267, 12);
					int32 as_sem_s17 = int32(0);
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					for (; [&]() -> bool
						{
							TypedFrame.SetSourcePosition(267, 3);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return false;
							}
							const bool as_sem_loop_condition = ([&]() -> bool
							{
								const int32 as_sem_e199_left = ([&]() -> int32
							{
								const int32 as_sem_e197_left = int32(10);
								const int32 as_sem_e197_right = as_sem_s0;
								return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e197_left, as_sem_e197_right);
							}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return bool{};
								}
								const int32 as_sem_e199_right = as_sem_s17;
								return (as_sem_e199_left > as_sem_e199_right);
							}());
							return !UNLIKELY(Execution.bExceptionThrown) && as_sem_loop_condition;
						}(); [&]()
						{
							TypedFrame.SetSourcePosition(267, 44);
							([&]() -> int32
							{
								int32& as_sem_e201_target = as_sem_s17;
								as_sem_e201_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e201_target, int32(1));
								return as_sem_e201_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return;
							}
						}())
					{
						TypedFrame.SetSourcePosition(268, 3);
						TypedFrame.SetSourcePosition(269, 4);
						break;
					}
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
				}
				TypedFrame.SetSourcePosition(271, 3);
				return as_sem_s4;
			}
			break;
		case int32(13):
			TypedFrame.SetSourcePosition(273, 2);
			{
				while ([&]() -> bool
					{
						TypedFrame.SetSourcePosition(275, 3);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return false;
						}
						const bool as_sem_loop_condition = ([&]() -> bool
						{
							const int32 as_sem_e205_left = as_sem_s4;
							const int32 as_sem_e205_right = as_sem_s0;
							return (as_sem_e205_left == as_sem_e205_right);
						}());
						return !UNLIKELY(Execution.bExceptionThrown) && as_sem_loop_condition;
					}())
				{
					TypedFrame.SetSourcePosition(276, 3);
					TypedFrame.SetSourcePosition(277, 4);
					([&]() -> int32
					{
						const int32 as_sem_e210_value = ([&]() -> int32
						{
							const int32 as_sem_e208_left = int32(10);
							const int32 as_sem_e208_right = as_sem_s0;
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e208_left, as_sem_e208_right);
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						int32& as_sem_e210_target = as_sem_s4;
						as_sem_e210_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e210_target, as_sem_e210_value);
						return as_sem_e210_target;
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					TypedFrame.SetSourcePosition(278, 4);
					([&]() -> int32
					{
						const int32 as_sem_e213_value = int32(100);
						int32& as_sem_e213_target = as_sem_s4;
						as_sem_e213_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e213_target, as_sem_e213_value);
						return as_sem_e213_target;
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					TypedFrame.SetSourcePosition(279, 4);
					break;
				}
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				TypedFrame.SetSourcePosition(281, 3);
				return as_sem_s4;
			}
			break;
		case int32(14):
			TypedFrame.SetSourcePosition(283, 2);
			{
				TypedFrame.SetSourcePosition(285, 7);
				int32 as_sem_s18 = int32(0);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				do
				{
					TypedFrame.SetSourcePosition(287, 3);
					TypedFrame.SetSourcePosition(288, 4);
					([&]() -> int32
					{
						int32& as_sem_e217_target = as_sem_s18;
						as_sem_e217_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e217_target, int32(1));
						return as_sem_e217_target;
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					TypedFrame.SetSourcePosition(289, 4);
					const bool as_sem_if_condition_136 = ([&]() -> bool
					{
						const int32 as_sem_e220_left = as_sem_s18;
						const int32 as_sem_e220_right = int32(1);
						return (as_sem_e220_left == as_sem_e220_right);
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					if (as_sem_if_condition_136)
					{
						TypedFrame.SetSourcePosition(291, 5);
						continue;
					}
					TypedFrame.SetSourcePosition(293, 4);
					([&]() -> int32
					{
						const int32 as_sem_e225_value = ([&]() -> int32
						{
							const int32 as_sem_e223_left = as_sem_s18;
							const int32 as_sem_e223_right = int32(10);
							return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e223_left, as_sem_e223_right);
						}());
						int32& as_sem_e225_target = as_sem_s4;
						as_sem_e225_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e225_target, as_sem_e225_value);
						return as_sem_e225_target;
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
				}
				while ([&]() -> bool
					{
						TypedFrame.SetSourcePosition(286, 3);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return false;
						}
						const bool as_sem_loop_condition = ([&]() -> bool
						{
							const int32 as_sem_e228_left = as_sem_s18;
							const int32 as_sem_e228_right = int32(3);
							return (as_sem_e228_left < as_sem_e228_right);
						}());
						return !UNLIKELY(Execution.bExceptionThrown) && as_sem_loop_condition;
					}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				TypedFrame.SetSourcePosition(296, 3);
				return ([&]() -> int32
				{
					const int32 as_sem_e233_left = ([&]() -> int32
				{
					const int32 as_sem_e231_left = as_sem_s4;
					const int32 as_sem_e231_right = int32(10);
					return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e231_left, as_sem_e231_right);
				}());
					const int32 as_sem_e233_right = as_sem_s18;
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e233_left, as_sem_e233_right);
				}());
			}
			break;
		case int32(15):
			TypedFrame.SetSourcePosition(298, 2);
			{
				TypedFrame.SetSourcePosition(300, 3);
				const bool as_sem_if_condition_145 = ([&]() -> bool
				{
					const int32 as_sem_e236_left = as_sem_s0;
					const int32 as_sem_e236_right = int32(0);
					return (as_sem_e236_left < as_sem_e236_right);
				}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				if (as_sem_if_condition_145)
				{
					TypedFrame.SetSourcePosition(302, 4);
					return AngelscriptTypedASTJIT::WrapNegate<int32>(int32(10));
				}
				TypedFrame.SetSourcePosition(304, 3);
				const bool as_sem_if_condition_148 = ([&]() -> bool
				{
					const int32 as_sem_e241_left = as_sem_s0;
					const int32 as_sem_e241_right = int32(0);
					return (as_sem_e241_left == as_sem_e241_right);
				}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				if (as_sem_if_condition_148)
				{
					TypedFrame.SetSourcePosition(306, 4);
					return int32(0);
				}
				TypedFrame.SetSourcePosition(308, 3);
				const bool as_sem_if_condition_157 = as_sem_s2;
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				if (as_sem_if_condition_157)
				{
					TypedFrame.SetSourcePosition(310, 4);
					const bool as_sem_if_condition_151 = as_sem_s3;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					if (as_sem_if_condition_151)
					{
						TypedFrame.SetSourcePosition(312, 5);
						return int32(11);
					}
					TypedFrame.SetSourcePosition(314, 4);
					return int32(12);
				}
				else
				TypedFrame.SetSourcePosition(316, 8);
				const bool as_sem_if_condition_156 = as_sem_s3;
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				if (as_sem_if_condition_156)
				{
					TypedFrame.SetSourcePosition(318, 4);
					return int32(13);
				}
				TypedFrame.SetSourcePosition(320, 3);
				return int32(14);
			}
			break;
		case int32(16):
			TypedFrame.SetSourcePosition(322, 2);
			{
				TypedFrame.SetSourcePosition(324, 3);
				const int32 as_sem_switch_selector_174 = static_cast<int32>(as_sem_s0);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				switch (as_sem_switch_selector_174)
				{
					case int32(0):
						TypedFrame.SetSourcePosition(326, 3);
						{
							TypedFrame.SetSourcePosition(327, 4);
							([&]() -> int32
							{
								const int32 as_sem_e255_value = int32(1);
								int32& as_sem_e255_target = as_sem_s4;
								as_sem_e255_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e255_target, as_sem_e255_value);
								return as_sem_e255_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
						}
					case int32(1):
						TypedFrame.SetSourcePosition(329, 3);
						{
							TypedFrame.SetSourcePosition(331, 8);
							const int32 as_sem_s19 = int32(20);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							TypedFrame.SetSourcePosition(332, 4);
							([&]() -> int32
							{
								const int32 as_sem_e259_value = as_sem_s19;
								int32& as_sem_e259_target = as_sem_s4;
								as_sem_e259_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e259_target, as_sem_e259_value);
								return as_sem_e259_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							TypedFrame.SetSourcePosition(333, 4);
							break;
						}
						break;
					default:
						TypedFrame.SetSourcePosition(335, 3);
						{
							TypedFrame.SetSourcePosition(337, 8);
							const int32 as_sem_s20 = int32(30);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							TypedFrame.SetSourcePosition(338, 4);
							([&]() -> int32
							{
								const int32 as_sem_e263_value = as_sem_s20;
								int32& as_sem_e263_target = as_sem_s4;
								as_sem_e263_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e263_target, as_sem_e263_value);
								return as_sem_e263_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							TypedFrame.SetSourcePosition(339, 4);
							break;
						}
						break;
				}
				TypedFrame.SetSourcePosition(342, 3);
				return as_sem_s4;
			}
			break;
		default:
			TypedFrame.SetSourcePosition(344, 2);
			{
				TypedFrame.SetSourcePosition(345, 3);
				return AngelscriptTypedASTJIT::WrapNegate<int32>(int32(1));
			}
			break;
	}
}

// AS Function : int SemanticCloneableState(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:1
// JIT Entry   : Raw
int32 ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_7e06d65381801c5779762d86540527e4762fac83929ecf664601b8c302cdb58d_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1, asBYTE p_2, asBYTE p_3)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as",
		"int SemanticCloneableState(const int, const int, const bool, const bool)",
		122,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_7e06d65381801c5779762d86540527e4762fac83929ecf664601b8c302cdb58d_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1), static_cast<bool>(p_2), static_cast<bool>(p_3));
}

// AS Function : int SemanticCloneableState(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:1
// JIT Entry   : VM
void ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_7e06d65381801c5779762d86540527e4762fac83929ecf664601b8c302cdb58d_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_7e06d65381801c5779762d86540527e4762fac83929ecf664601b8c302cdb58d_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1),
		*(asBYTE*)(l_fp + 2),
		*(asBYTE*)(l_fp + 3));
}

// AS Function : int SemanticCloneableState(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:1
// JIT Entry   : Parms
void ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_7e06d65381801c5779762d86540527e4762fac83929ecf664601b8c302cdb58d_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(bool));
	const SIZE_T ParmOffset_2 = ParmsOffset;
	ParmsOffset += sizeof(bool);
	ParmsOffset = Align(ParmsOffset, alignof(bool));
	const SIZE_T ParmOffset_3 = ParmsOffset;
	ParmsOffset += sizeof(bool);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_7e06d65381801c5779762d86540527e4762fac83929ecf664601b8c302cdb58d_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)),
		static_cast<asBYTE>(*(bool*)(((SIZE_T)Parms) + ParmOffset_2)),
		static_cast<asBYTE>(*(bool*)(((SIZE_T)Parms) + ParmOffset_3)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticStructuredControlFlow(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:9:1
 * StableFunctionKey  : 5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43
 * ExecutionHash      : 8ee164ff884cc04a0c650eaaa01566f915ed8a2216dc0bb0eaccdd309e6c084f
 * DebugHash          : 8d35a65a64a001041fb0da6768d44ae2a42b7a3af618d20ffed20a8e49b4e5b8
 * EntryAbiHash       : 2fe16db0121b593e09cfa3e040d4124228d3d043449cc2ba5580b68918773fef
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 32
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_8ee164ff884cc04a0c650eaaa01566f915ed8a2216dc0bb0eaccdd309e6c084f_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1);

static int32 ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_8ee164ff884cc04a0c650eaaa01566f915ed8a2216dc0bb0eaccdd309e6c084f_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1)
{
	TypedFrame.SetSourcePosition(11, 6);
	int32 as_sem_s2 = int32(0);
	{
		TypedFrame.SetSourcePosition(12, 11);
		int32 as_sem_s3 = int32(0);
		for (; [&]() -> bool
			{
				TypedFrame.SetSourcePosition(12, 2);
				const bool as_sem_loop_condition = ([&]() -> bool
				{
					const int32 as_sem_e4_left = as_sem_s3;
					const int32 as_sem_e4_right = as_sem_s0;
					return (as_sem_e4_left < as_sem_e4_right);
				}());
				return as_sem_loop_condition;
			}(); [&]()
			{
				TypedFrame.SetSourcePosition(12, 37);
				([&]() -> int32
				{
					int32& as_sem_e6_target = as_sem_s3;
					as_sem_e6_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e6_target, int32(1));
					return as_sem_e6_target;
				}());
			}())
		{
			TypedFrame.SetSourcePosition(13, 2);
			TypedFrame.SetSourcePosition(14, 3);
			if (([&]() -> bool
			{
				const int32 as_sem_e9_left = as_sem_s3;
				const int32 as_sem_e9_right = int32(1);
				return (as_sem_e9_left == as_sem_e9_right);
			}()))
			{
				TypedFrame.SetSourcePosition(16, 4);
				continue;
			}
			TypedFrame.SetSourcePosition(19, 3);
			const int32 as_sem_switch_selector_19 = static_cast<int32>(as_sem_s3);
			switch (as_sem_switch_selector_19)
			{
				case int32(2):
					TypedFrame.SetSourcePosition(21, 3);
					{
						TypedFrame.SetSourcePosition(22, 4);
						([&]() -> int32
						{
							const int32 as_sem_e15_value = int32(20);
							int32& as_sem_e15_target = as_sem_s2;
							as_sem_e15_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e15_target, as_sem_e15_value);
							return as_sem_e15_target;
						}());
						TypedFrame.SetSourcePosition(23, 4);
						break;
					}
					break;
				case int32(3):
					TypedFrame.SetSourcePosition(24, 3);
					{
						TypedFrame.SetSourcePosition(25, 4);
						([&]() -> int32
						{
							const int32 as_sem_e18_value = int32(30);
							int32& as_sem_e18_target = as_sem_s2;
							as_sem_e18_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e18_target, as_sem_e18_value);
							return as_sem_e18_target;
						}());
						TypedFrame.SetSourcePosition(26, 4);
						continue;
					}
					break;
				default:
					TypedFrame.SetSourcePosition(27, 3);
					{
						TypedFrame.SetSourcePosition(28, 4);
						([&]() -> int32
						{
							const int32 as_sem_e21_value = as_sem_s3;
							int32& as_sem_e21_target = as_sem_s2;
							as_sem_e21_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e21_target, as_sem_e21_value);
							return as_sem_e21_target;
						}());
						TypedFrame.SetSourcePosition(29, 4);
						break;
					}
					break;
			}
			TypedFrame.SetSourcePosition(32, 3);
			if (([&]() -> bool
			{
				const int32 as_sem_e24_left = as_sem_s2;
				const int32 as_sem_e24_right = int32(45);
				return (as_sem_e24_left > as_sem_e24_right);
			}()))
			{
				TypedFrame.SetSourcePosition(34, 4);
				break;
			}
		}
	}
	TypedFrame.SetSourcePosition(38, 6);
	int32 as_sem_s4 = int32(0);
	while ([&]() -> bool
		{
			TypedFrame.SetSourcePosition(39, 2);
			const bool as_sem_loop_condition = ([&]() -> bool
			{
				const int32 as_sem_e28_left = as_sem_s4;
				const int32 as_sem_e28_right = as_sem_s0;
				return (as_sem_e28_left < as_sem_e28_right);
			}());
			return as_sem_loop_condition;
		}())
	{
		TypedFrame.SetSourcePosition(40, 2);
		TypedFrame.SetSourcePosition(41, 3);
		([&]() -> int32
		{
			int32& as_sem_e30_target = as_sem_s4;
			as_sem_e30_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e30_target, int32(1));
			return as_sem_e30_target;
		}());
		TypedFrame.SetSourcePosition(42, 3);
		if (([&]() -> bool
		{
			const int32 as_sem_e33_left = as_sem_s4;
			const int32 as_sem_e33_right = int32(2);
			return (as_sem_e33_left == as_sem_e33_right);
		}()))
		{
			TypedFrame.SetSourcePosition(44, 4);
			continue;
		}
		TypedFrame.SetSourcePosition(46, 3);
		([&]() -> int32
		{
			const int32 as_sem_e36_value = int32(3);
			int32& as_sem_e36_target = as_sem_s2;
			as_sem_e36_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e36_target, as_sem_e36_value);
			return as_sem_e36_target;
		}());
		TypedFrame.SetSourcePosition(47, 3);
		if (([&]() -> bool
		{
			const int32 as_sem_e39_left = as_sem_s4;
			const int32 as_sem_e39_right = int32(4);
			return (as_sem_e39_left >= as_sem_e39_right);
		}()))
		{
			TypedFrame.SetSourcePosition(49, 4);
			break;
		}
	}
	TypedFrame.SetSourcePosition(53, 6);
	int32 as_sem_s5 = int32(0);
	do
	{
		TypedFrame.SetSourcePosition(55, 2);
		TypedFrame.SetSourcePosition(56, 3);
		([&]() -> int32
		{
			int32& as_sem_e42_target = as_sem_s5;
			as_sem_e42_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e42_target, int32(1));
			return as_sem_e42_target;
		}());
		TypedFrame.SetSourcePosition(57, 3);
		if (([&]() -> bool
		{
			const int32 as_sem_e45_left = as_sem_s5;
			const int32 as_sem_e45_right = int32(1);
			return (as_sem_e45_left == as_sem_e45_right);
		}()))
		{
			TypedFrame.SetSourcePosition(59, 4);
			continue;
		}
		TypedFrame.SetSourcePosition(61, 3);
		([&]() -> int32
		{
			const int32 as_sem_e48_value = int32(5);
			int32& as_sem_e48_target = as_sem_s2;
			as_sem_e48_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e48_target, as_sem_e48_value);
			return as_sem_e48_target;
		}());
	}
	while ([&]() -> bool
		{
			TypedFrame.SetSourcePosition(54, 2);
			const bool as_sem_loop_condition = ([&]() -> bool
			{
				const int32 as_sem_e51_left = as_sem_s5;
				const int32 as_sem_e51_right = int32(2);
				return (as_sem_e51_left < as_sem_e51_right);
			}());
			return as_sem_loop_condition;
		}());
	TypedFrame.SetSourcePosition(65, 2);
	const int32 as_sem_switch_selector_72 = static_cast<int32>(as_sem_s1);
	switch (as_sem_switch_selector_72)
	{
		case int32(0):
			TypedFrame.SetSourcePosition(67, 2);
			{
				TypedFrame.SetSourcePosition(68, 3);
				([&]() -> int32
				{
					const int32 as_sem_e59_value = int32(100);
					int32& as_sem_e59_target = as_sem_s2;
					as_sem_e59_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e59_target, as_sem_e59_value);
					return as_sem_e59_target;
				}());
				TypedFrame.SetSourcePosition(69, 3);
				break;
			}
			break;
		case int32(1):
			TypedFrame.SetSourcePosition(70, 2);
			{
				TypedFrame.SetSourcePosition(71, 3);
				([&]() -> int32
				{
					const int32 as_sem_e62_value = int32(200);
					int32& as_sem_e62_target = as_sem_s2;
					as_sem_e62_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e62_target, as_sem_e62_value);
					return as_sem_e62_target;
				}());
			}
		case int32(2):
			TypedFrame.SetSourcePosition(72, 2);
			{
				TypedFrame.SetSourcePosition(74, 7);
				const int32 as_sem_s6 = int32(20);
				TypedFrame.SetSourcePosition(75, 3);
				([&]() -> int32
				{
					const int32 as_sem_e66_value = as_sem_s6;
					int32& as_sem_e66_target = as_sem_s2;
					as_sem_e66_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e66_target, as_sem_e66_value);
					return as_sem_e66_target;
				}());
				TypedFrame.SetSourcePosition(76, 3);
				break;
			}
			break;
		case int32(3):
			TypedFrame.SetSourcePosition(78, 2);
			{
				TypedFrame.SetSourcePosition(80, 7);
				int32 as_sem_s7 = int32(0);
				while ([&]() -> bool
					{
						TypedFrame.SetSourcePosition(81, 3);
						const bool as_sem_loop_condition = ([&]() -> bool
						{
							const int32 as_sem_e70_left = as_sem_s7;
							const int32 as_sem_e70_right = int32(2);
							return (as_sem_e70_left < as_sem_e70_right);
						}());
						return as_sem_loop_condition;
					}())
				{
					TypedFrame.SetSourcePosition(82, 3);
					TypedFrame.SetSourcePosition(83, 4);
					([&]() -> int32
					{
						const int32 as_sem_e73_value = int32(7);
						int32& as_sem_e73_target = as_sem_s2;
						as_sem_e73_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e73_target, as_sem_e73_value);
						return as_sem_e73_target;
					}());
					TypedFrame.SetSourcePosition(84, 4);
					([&]() -> int32
					{
						int32& as_sem_e75_target = as_sem_s7;
						as_sem_e75_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e75_target, int32(1));
						return as_sem_e75_target;
					}());
					TypedFrame.SetSourcePosition(85, 4);
					if (([&]() -> bool
					{
						const int32 as_sem_e78_left = as_sem_s7;
						const int32 as_sem_e78_right = int32(1);
						return (as_sem_e78_left == as_sem_e78_right);
					}()))
					{
						TypedFrame.SetSourcePosition(87, 5);
						continue;
					}
					TypedFrame.SetSourcePosition(89, 4);
					break;
				}
				TypedFrame.SetSourcePosition(91, 3);
				break;
			}
			break;
		default:
			TypedFrame.SetSourcePosition(93, 2);
			{
				TypedFrame.SetSourcePosition(94, 3);
				([&]() -> int32
				{
					const int32 as_sem_e81_value = int32(9);
					int32& as_sem_e81_target = as_sem_s2;
					as_sem_e81_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e81_target, as_sem_e81_value);
					return as_sem_e81_target;
				}());
				TypedFrame.SetSourcePosition(95, 3);
				break;
			}
			break;
	}
	TypedFrame.SetSourcePosition(97, 2);
	return as_sem_s2;
}

// AS Function : int SemanticStructuredControlFlow(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:9:1
// JIT Entry   : Raw
int32 ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_8ee164ff884cc04a0c650eaaa01566f915ed8a2216dc0bb0eaccdd309e6c084f_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as",
		"int SemanticStructuredControlFlow(const int, const int)",
		9,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_8ee164ff884cc04a0c650eaaa01566f915ed8a2216dc0bb0eaccdd309e6c084f_TypedBody(TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int SemanticStructuredControlFlow(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:9:1
// JIT Entry   : VM
void ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_8ee164ff884cc04a0c650eaaa01566f915ed8a2216dc0bb0eaccdd309e6c084f_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_8ee164ff884cc04a0c650eaaa01566f915ed8a2216dc0bb0eaccdd309e6c084f_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int SemanticStructuredControlFlow(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:9:1
// JIT Entry   : Parms
void ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_8ee164ff884cc04a0c650eaaa01566f915ed8a2216dc0bb0eaccdd309e6c084f_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_8ee164ff884cc04a0c650eaaa01566f915ed8a2216dc0bb0eaccdd309e6c084f_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticCheckedDivide(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
 * StableFunctionKey  : a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5
 * ExecutionHash      : 3c9395cb48100816600076228d1517860b80fdd10e52295aac2a31f1b9af6f3d
 * DebugHash          : 7be5fa7ce3d293b37a860e95bb2fc9f0448c71efd8c89d5f7d190ea0fd86bf06
 * EntryAbiHash       : 34574e76e092ff59a9ab137303c6c8a1a00f50e50bf9ce931ed07453463297ff
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_3c9395cb48100816600076228d1517860b80fdd10e52295aac2a31f1b9af6f3d_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1);

static int32 ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_3c9395cb48100816600076228d1517860b80fdd10e52295aac2a31f1b9af6f3d_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1)
{
	TypedFrame.SetSourcePosition(118, 2);
	return ([&]() -> int32
	{
		const int32 as_sem_e2_left = as_sem_s0;
		const int32 as_sem_e2_right = as_sem_s1;
		return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e2_left, as_sem_e2_right);
	}());
}

// AS Function : int SemanticCheckedDivide(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
// JIT Entry   : Raw
int32 ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_3c9395cb48100816600076228d1517860b80fdd10e52295aac2a31f1b9af6f3d_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as",
		"int SemanticCheckedDivide(const int, const int)",
		116,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_3c9395cb48100816600076228d1517860b80fdd10e52295aac2a31f1b9af6f3d_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int SemanticCheckedDivide(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
// JIT Entry   : VM
void ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_3c9395cb48100816600076228d1517860b80fdd10e52295aac2a31f1b9af6f3d_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_3c9395cb48100816600076228d1517860b80fdd10e52295aac2a31f1b9af6f3d_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int SemanticCheckedDivide(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
// JIT Entry   : Parms
void ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_3c9395cb48100816600076228d1517860b80fdd10e52295aac2a31f1b9af6f3d_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_3c9395cb48100816600076228d1517860b80fdd10e52295aac2a31f1b9af6f3d_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
