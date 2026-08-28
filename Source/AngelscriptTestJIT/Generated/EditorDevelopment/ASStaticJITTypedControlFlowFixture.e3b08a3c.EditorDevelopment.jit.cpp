// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as
 * CanonicalModuleName : ASStaticJITTypedControlFlowFixture
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : e3b08a3c509f1eb5fad8ca63479dcc6c2638c01694af2a1048e06e1de36d5c99
 * ProviderId          : 7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44
 * ArtifactProfile     : 018cc71ab6612eca7608aa13abe1f7a1a7d81964bb8cc5821f1b1e047fcf7a11
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
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:5
 * StableFunctionKey  : 0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4
 * ExecutionHash      : 9f66be3bac5f691ee91f3728c339fdd3b53089aa0b40a8ff647198f26b3c29df
 * DebugHash          : eb35fee034448a8ea8818e4e236e9542c76c9ddb2744a6c4f421f31127dbd279
 * EntryAbiHash       : ac5e9740aa5289dba47d6e5ba3f40c6f1f11e575d2ab2615b198bbbc11568b3d
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_9f66be3bac5f691ee91f3728c339fdd3b53089aa0b40a8ff647198f26b3c29df_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d16);

static int32 ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_9f66be3bac5f691ee91f3728c339fdd3b53089aa0b40a8ff647198f26b3c29df_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d16)
{
	int8 as_ast_d17;
	{
		TypedFrame.SetSourcePosition(103, 23);
		([&]() -> int8
		{
			const int8 as_ast_e151_value = AngelscriptTypedASTJIT::WrapNarrow<int8>(as_ast_d16);
			int8& as_ast_e151_target = as_ast_d17;
			as_ast_e151_target = as_ast_e151_value;
			return as_ast_e151_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(104, 2);
		const auto as_ast_stmt_84_condition = as_ast_d17;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
		switch (as_ast_stmt_84_condition)
		{
			case int8(0):
				{
					TypedFrame.SetSourcePosition(107, 3);
					return int32(10);
				}
			case int8(1):
				{
					TypedFrame.SetSourcePosition(109, 3);
					return int32(20);
				}
			case int8(2):
				{
					TypedFrame.SetSourcePosition(111, 3);
					return int32(30);
				}
			default:
				TypedFrame.SetSourcePosition(104, 2);
				FStaticJITFunction::SetSwitchValueInvalidException(Execution);
				return {};
		}
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int SemanticExhaustiveEnum(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:5
// JIT Entry   : Raw
int32 ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_9f66be3bac5f691ee91f3728c339fdd3b53089aa0b40a8ff647198f26b3c29df_Raw(FScriptExecution& Execution, asDWORD p_0)
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
	return ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_9f66be3bac5f691ee91f3728c339fdd3b53089aa0b40a8ff647198f26b3c29df_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int SemanticExhaustiveEnum(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:5
// JIT Entry   : VM
void ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_9f66be3bac5f691ee91f3728c339fdd3b53089aa0b40a8ff647198f26b3c29df_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_9f66be3bac5f691ee91f3728c339fdd3b53089aa0b40a8ff647198f26b3c29df_Raw(Execution,
		*(asDWORD*)(l_fp + 0));
}

// AS Function : int SemanticExhaustiveEnum(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:5
// JIT Entry   : Parms
void ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_9f66be3bac5f691ee91f3728c339fdd3b53089aa0b40a8ff647198f26b3c29df_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_9f66be3bac5f691ee91f3728c339fdd3b53089aa0b40a8ff647198f26b3c29df_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticCloneableState(const int, const int, const bool, const bool)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:5
 * StableFunctionKey  : 169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855
 * ExecutionHash      : 519b1553e7481d1d437988c873b5b540ceab439acecb19969d7cd4b85bad1170
 * DebugHash          : 7857027b75d3e59edc57f45d70117a833f97365c247cefc12f32243c700520c5
 * EntryAbiHash       : 3d05f1b7e35a8891df3c73e1ce6d43c897b0603db3baca8a2f98651f018854ce
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 32
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_519b1553e7481d1d437988c873b5b540ceab439acecb19969d7cd4b85bad1170_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d22,
	const int32 as_ast_d23,
	const bool as_ast_d24,
	const bool as_ast_d25);

static int32 ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_519b1553e7481d1d437988c873b5b540ceab439acecb19969d7cd4b85bad1170_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d22,
	const int32 as_ast_d23,
	const bool as_ast_d24,
	const bool as_ast_d25)
{
	int32 as_ast_d26;
	{
		TypedFrame.SetSourcePosition(128, 6);
		([&]() -> int32
		{
			const int32 as_ast_e164_value = as_ast_d22;
			int32& as_ast_e164_target = as_ast_d26;
			as_ast_e164_target = as_ast_e164_value;
			return as_ast_e164_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(129, 2);
		switch (as_ast_d23)
		{
			case int32(0):
				{
					int32 as_ast_d27;
					{
						TypedFrame.SetSourcePosition(133, 7);
						([&]() -> int32
						{
							const int32 as_ast_e181_value = ([&]() -> int32
						{
							const int32 as_ast_e177_left = ([&]() -> int32
						{
							const int32 as_ast_e176_left = ([&]() -> int32
						{
							const int32 as_ast_e173_left = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d26);
							const int32 as_ast_e173_right = int32(100);
							return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e173_left, as_ast_e173_right);
						}());
							const int32 as_ast_e176_right = ([&]() -> int32
						{
							const int32 as_ast_e175_left = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d26);
							const int32 as_ast_e175_right = int32(10);
							return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e175_left, as_ast_e175_right);
						}());
							return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e176_left, as_ast_e176_right);
						}());
							const int32 as_ast_e177_right = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d26);
							return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e177_left, as_ast_e177_right);
						}());
							int32& as_ast_e181_target = as_ast_d27;
							as_ast_e181_target = as_ast_e181_value;
							return as_ast_e181_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(137, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e186_left = ([&]() -> int32
					{
						const int32 as_ast_e185_left = as_ast_d26;
						const int32 as_ast_e185_right = int32(1000);
						return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e185_left, as_ast_e185_right);
					}());
						const int32 as_ast_e186_right = as_ast_d27;
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e186_left, as_ast_e186_right);
					}());
					}
				}
			case int32(1):
				{
					bool as_ast_d28;
					{
						TypedFrame.SetSourcePosition(141, 8);
						([&]() -> bool
						{
							const bool as_ast_e198_value = ((as_ast_d24 && ([&]() -> bool
						{
							const int32 as_ast_e193_left = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d26);
							const int32 as_ast_e193_right = as_ast_d22;
							return (as_ast_e193_left > as_ast_e193_right);
						}())) && as_ast_d25);
							bool& as_ast_e198_target = as_ast_d28;
							as_ast_e198_target = as_ast_e198_value;
							return as_ast_e198_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(143, 3);
						if (as_ast_d28)
						{
							{
								TypedFrame.SetSourcePosition(145, 4);
								return ([&]() -> int32
							{
								const int32 as_ast_e204_left = ([&]() -> int32
							{
								const int32 as_ast_e202_left = as_ast_d26;
								const int32 as_ast_e202_right = int32(10);
								return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e202_left, as_ast_e202_right);
							}());
								const int32 as_ast_e204_right = int32(1);
								return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e204_left, as_ast_e204_right);
							}());
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(147, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e209_left = as_ast_d26;
						const int32 as_ast_e209_right = int32(10);
						return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e209_left, as_ast_e209_right);
					}());
					}
				}
			case int32(2):
				{
					{
						TypedFrame.SetSourcePosition(151, 3);
						([&]() -> int32
						{
							const int32 as_ast_e216_value = int32(3);
							int32& as_ast_e216_target = as_ast_d26;
							as_ast_e216_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e216_target, as_ast_e216_value);
							return as_ast_e216_target;
						}());
					}
					int32 as_ast_d29;
					{
						TypedFrame.SetSourcePosition(152, 7);
						([&]() -> int32
						{
							const int32 as_ast_e222_value = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d26);
							int32& as_ast_e222_target = as_ast_d29;
							as_ast_e222_target = as_ast_e222_value;
							return as_ast_e222_target;
						}());
					}
					int32 as_ast_d30;
					{
						TypedFrame.SetSourcePosition(153, 7);
						([&]() -> int32
						{
							const int32 as_ast_e227_value = (as_ast_d26, AngelscriptTypedASTJIT::WrapPostIncrement<int32>(as_ast_d26));
							int32& as_ast_e227_target = as_ast_d30;
							as_ast_e227_target = as_ast_e227_value;
							return as_ast_e227_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(154, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e236_left = ([&]() -> int32
					{
						const int32 as_ast_e235_left = ([&]() -> int32
					{
						const int32 as_ast_e232_left = as_ast_d26;
						const int32 as_ast_e232_right = int32(100);
						return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e232_left, as_ast_e232_right);
					}());
						const int32 as_ast_e235_right = ([&]() -> int32
					{
						const int32 as_ast_e234_left = as_ast_d29;
						const int32 as_ast_e234_right = int32(10);
						return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e234_left, as_ast_e234_right);
					}());
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e235_left, as_ast_e235_right);
					}());
						const int32 as_ast_e236_right = as_ast_d30;
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e236_left, as_ast_e236_right);
					}());
					}
				}
			case int32(3):
				{
					int32 as_ast_d31;
					{
						TypedFrame.SetSourcePosition(158, 7);
						([&]() -> int32
						{
							const int32 as_ast_e242_value = int32(0);
							int32& as_ast_e242_target = as_ast_d31;
							as_ast_e242_target = as_ast_e242_value;
							return as_ast_e242_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(159, 3);
						while (([&]() -> bool
						{
							const int32 as_ast_e245_left = as_ast_d31;
							const int32 as_ast_e245_right = int32(3);
							return (as_ast_e245_left < as_ast_e245_right);
						}()))
						{
							{
								TypedFrame.SetSourcePosition(161, 4);
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d31);
							}
							{
								TypedFrame.SetSourcePosition(162, 4);
								if (([&]() -> bool
								{
									const int32 as_ast_e250_left = as_ast_d31;
									const int32 as_ast_e250_right = int32(1);
									return (as_ast_e250_left == as_ast_e250_right);
								}()))
								{
									{
										TypedFrame.SetSourcePosition(164, 5);
										continue;
									}
								}
							}
							{
								TypedFrame.SetSourcePosition(166, 4);
								([&]() -> int32
								{
									const int32 as_ast_e256_value = as_ast_d31;
									int32& as_ast_e256_target = as_ast_d26;
									as_ast_e256_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e256_target, as_ast_e256_value);
									return as_ast_e256_target;
								}());
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(169, 3);
						int32 as_ast_d32;
						{
							TypedFrame.SetSourcePosition(169, 12);
							([&]() -> int32
							{
								const int32 as_ast_e262_value = int32(0);
								int32& as_ast_e262_target = as_ast_d32;
								as_ast_e262_target = as_ast_e262_value;
								return as_ast_e262_target;
							}());
						}
						for ( ; ([&]() -> bool
					{
						const int32 as_ast_e265_left = as_ast_d32;
						const int32 as_ast_e265_right = int32(4);
						return (as_ast_e265_left < as_ast_e265_right);
					}()) ; AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d32) )
						{
							{
								TypedFrame.SetSourcePosition(171, 4);
								switch (as_ast_d32)
								{
									case int32(1):
										{
											TypedFrame.SetSourcePosition(174, 5);
											([&]() -> int32
											{
												const int32 as_ast_e273_value = int32(10);
												int32& as_ast_e273_target = as_ast_d26;
												as_ast_e273_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e273_target, as_ast_e273_value);
												return as_ast_e273_target;
											}());
										}
										{
											TypedFrame.SetSourcePosition(175, 5);
											continue;
										}
									case int32(2):
										{
											TypedFrame.SetSourcePosition(177, 5);
											([&]() -> int32
											{
												const int32 as_ast_e281_value = int32(20);
												int32& as_ast_e281_target = as_ast_d26;
												as_ast_e281_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e281_target, as_ast_e281_value);
												return as_ast_e281_target;
											}());
										}
										{
											TypedFrame.SetSourcePosition(178, 5);
											break;
										}
									default:
										{
											TypedFrame.SetSourcePosition(180, 5);
											([&]() -> int32
											{
												const int32 as_ast_e288_value = as_ast_d32;
												int32& as_ast_e288_target = as_ast_d26;
												as_ast_e288_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e288_target, as_ast_e288_value);
												return as_ast_e288_target;
											}());
										}
										{
											TypedFrame.SetSourcePosition(181, 5);
											break;
										}
								}
							}
							{
								TypedFrame.SetSourcePosition(184, 4);
								if (([&]() -> bool
								{
									const int32 as_ast_e292_left = as_ast_d32;
									const int32 as_ast_e292_right = int32(2);
									return (as_ast_e292_left == as_ast_e292_right);
								}()))
								{
									{
										TypedFrame.SetSourcePosition(186, 5);
										break;
									}
								}
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(189, 3);
						return as_ast_d26;
					}
				}
			case int32(4):
				{
					int32 as_ast_d33;
					{
						TypedFrame.SetSourcePosition(193, 7);
						([&]() -> int32
						{
							const int32 as_ast_e300_value = int32(0);
							int32& as_ast_e300_target = as_ast_d33;
							as_ast_e300_target = as_ast_e300_value;
							return as_ast_e300_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(194, 3);
						while (([&]() -> bool
						{
							const int32 as_ast_e307_left = ([&]() -> int32
						{
							TypedFrame.SetSourcePosition(194, 3);
							const int32 as_ast_e303_left = int32(10);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							const int32 as_ast_e303_right = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e303_left, as_ast_e303_right);
						}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e307_right = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d33);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e307_left > as_ast_e307_right);
						}()))
						{
							{
								TypedFrame.SetSourcePosition(196, 4);
								([&]() -> int32
								{
									const int32 as_ast_e312_value = int32(1);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e312_target = as_ast_d26;
									as_ast_e312_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e312_target, as_ast_e312_value);
									return as_ast_e312_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(197, 4);
								break;
							}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(199, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e319_left = as_ast_d26;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						const int32 as_ast_e319_right = as_ast_d33;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e319_left, as_ast_e319_right);
					}());
					}
				}
			case int32(5):
				{
					{
						TypedFrame.SetSourcePosition(203, 3);
						int32 as_ast_d34;
						{
							TypedFrame.SetSourcePosition(203, 12);
							([&]() -> int32
							{
								const int32 as_ast_e323_value = int32(0);
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return int32{};
								}
								int32& as_ast_e323_target = as_ast_d34;
								as_ast_e323_target = as_ast_e323_value;
								return as_ast_e323_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return {};
							}
						}
						for ( ; ([&]() -> bool
					{
						const int32 as_ast_e326_left = as_ast_d34;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						const int32 as_ast_e326_right = int32(1);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						return (as_ast_e326_left < as_ast_e326_right);
					}()) ; AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d34) )
						{
							{
								TypedFrame.SetSourcePosition(205, 4);
								([&]() -> int32
								{
									const int32 as_ast_e336_value = ([&]() -> int32
								{
									TypedFrame.SetSourcePosition(205, 4);
									const int32 as_ast_e332_left = int32(10);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									const int32 as_ast_e332_right = as_ast_d22;
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e332_left, as_ast_e332_right);
								}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e336_target = as_ast_d26;
									as_ast_e336_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e336_target, as_ast_e336_value);
									return as_ast_e336_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(206, 4);
								([&]() -> int32
								{
									const int32 as_ast_e343_value = int32(100);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e343_target = as_ast_d26;
									as_ast_e343_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e343_target, as_ast_e343_value);
									return as_ast_e343_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(208, 3);
						return as_ast_d26;
					}
				}
			case int32(6):
				{
					int32 as_ast_d35;
					{
						TypedFrame.SetSourcePosition(212, 7);
						([&]() -> int32
						{
							const int32 as_ast_e351_value = int32(0);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							int32& as_ast_e351_target = as_ast_d35;
							as_ast_e351_target = as_ast_e351_value;
							return as_ast_e351_target;
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(213, 3);
						int32 as_ast_d36;
						{
							TypedFrame.SetSourcePosition(213, 12);
							([&]() -> int32
							{
								const int32 as_ast_e354_value = int32(0);
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return int32{};
								}
								int32& as_ast_e354_target = as_ast_d36;
								as_ast_e354_target = as_ast_e354_value;
								return as_ast_e354_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return {};
							}
						}
						for ( ; ([&]() -> bool
					{
						const int32 as_ast_e357_left = as_ast_d36;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						const int32 as_ast_e357_right = int32(1);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						return (as_ast_e357_left < as_ast_e357_right);
					}()) ; (([&]() -> int32
						{
							const int32 as_ast_e363_value = ([&]() -> int32
						{
							TypedFrame.SetSourcePosition(213, 3);
							const int32 as_ast_e361_left = int32(10);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							const int32 as_ast_e361_right = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e361_left, as_ast_e361_right);
						}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							int32& as_ast_e363_target = as_ast_d36;
							as_ast_e363_target = as_ast_e363_value;
							return as_ast_e363_target;
						}()), AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d35)) )
						{
							{
								TypedFrame.SetSourcePosition(216, 4);
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d36);
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(217, 4);
								continue;
							}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(219, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e373_left = as_ast_d26;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						const int32 as_ast_e373_right = as_ast_d35;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e373_left, as_ast_e373_right);
					}());
					}
				}
			case int32(7):
				{
					{
						TypedFrame.SetSourcePosition(223, 3);
						const auto as_ast_stmt_190_condition = ([&]() -> int32
						{
							TypedFrame.SetSourcePosition(223, 3);
							const int32 as_ast_e377_left = int32(10);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							const int32 as_ast_e377_right = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e377_left, as_ast_e377_right);
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
						switch (as_ast_stmt_190_condition)
						{
							case int32(0):
								{
									TypedFrame.SetSourcePosition(226, 4);
									([&]() -> int32
									{
										const int32 as_ast_e382_value = int32(1);
										if (UNLIKELY(Execution.bExceptionThrown))
										{
											return int32{};
										}
										int32& as_ast_e382_target = as_ast_d26;
										as_ast_e382_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e382_target, as_ast_e382_value);
										return as_ast_e382_target;
									}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return {};
									}
								}
								{
									TypedFrame.SetSourcePosition(227, 4);
									break;
								}
							default:
								{
									TypedFrame.SetSourcePosition(229, 4);
									([&]() -> int32
									{
										const int32 as_ast_e390_value = int32(2);
										if (UNLIKELY(Execution.bExceptionThrown))
										{
											return int32{};
										}
										int32& as_ast_e390_target = as_ast_d26;
										as_ast_e390_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e390_target, as_ast_e390_value);
										return as_ast_e390_target;
									}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return {};
									}
								}
								{
									TypedFrame.SetSourcePosition(230, 4);
									break;
								}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(232, 3);
						return as_ast_d26;
					}
				}
			case int32(8):
				{
					int32 as_ast_d37;
					{
						TypedFrame.SetSourcePosition(236, 7);
						([&]() -> int32
						{
							const int32 as_ast_e399_value = int32(0);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							int32& as_ast_e399_target = as_ast_d37;
							as_ast_e399_target = as_ast_e399_value;
							return as_ast_e399_target;
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(237, 3);
						do
						{
							{
								TypedFrame.SetSourcePosition(239, 4);
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d37);
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(240, 4);
								continue;
							}
						}
						while (([&]() -> bool
						{
							const int32 as_ast_e407_left = ([&]() -> int32
						{
							TypedFrame.SetSourcePosition(237, 3);
							const int32 as_ast_e404_left = int32(10);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							const int32 as_ast_e404_right = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e404_left, as_ast_e404_right);
						}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e407_right = as_ast_d37;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e407_left > as_ast_e407_right);
						}()));
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(243, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e412_left = as_ast_d26;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						const int32 as_ast_e412_right = as_ast_d37;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e412_left, as_ast_e412_right);
					}());
					}
				}
			case int32(10):
				{
					{
						TypedFrame.SetSourcePosition(247, 3);
						do
						{
							{
								TypedFrame.SetSourcePosition(249, 4);
								([&]() -> int32
								{
									const int32 as_ast_e421_value = ([&]() -> int32
								{
									TypedFrame.SetSourcePosition(249, 4);
									const int32 as_ast_e417_left = int32(10);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									const int32 as_ast_e417_right = as_ast_d22;
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e417_left, as_ast_e417_right);
								}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e421_target = as_ast_d26;
									as_ast_e421_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e421_target, as_ast_e421_value);
									return as_ast_e421_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(250, 4);
								([&]() -> int32
								{
									const int32 as_ast_e428_value = int32(100);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e428_target = as_ast_d26;
									as_ast_e428_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e428_target, as_ast_e428_value);
									return as_ast_e428_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
						}
						while (AngelscriptTypedASTJIT::FromCanonicalBits<bool>(UINT64_C(0)));
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(253, 3);
						return as_ast_d26;
					}
				}
			case int32(11):
				{
					int32 as_ast_d38;
					{
						TypedFrame.SetSourcePosition(257, 7);
						([&]() -> int32
						{
							const int32 as_ast_e437_value = int32(0);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							int32& as_ast_e437_target = as_ast_d38;
							as_ast_e437_target = as_ast_e437_value;
							return as_ast_e437_target;
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(258, 3);
						while ((([&]() -> bool
						{
							const int32 as_ast_e444_left = as_ast_d38;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e444_right = int32(0);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e444_left == as_ast_e444_right);
						}()) || ([&]() -> bool
						{
							const int32 as_ast_e446_left = ([&]() -> int32
						{
							TypedFrame.SetSourcePosition(258, 3);
							const int32 as_ast_e441_left = int32(10);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							const int32 as_ast_e441_right = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e441_left, as_ast_e441_right);
						}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e446_right = as_ast_d38;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e446_left > as_ast_e446_right);
						}())))
						{
							{
								TypedFrame.SetSourcePosition(260, 4);
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d38);
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(261, 4);
								continue;
							}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(263, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e456_left = as_ast_d26;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						const int32 as_ast_e456_right = as_ast_d38;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e456_left, as_ast_e456_right);
					}());
					}
				}
			case int32(12):
				{
					{
						TypedFrame.SetSourcePosition(267, 3);
						int32 as_ast_d39;
						{
							TypedFrame.SetSourcePosition(267, 12);
							([&]() -> int32
							{
								const int32 as_ast_e460_value = int32(0);
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return int32{};
								}
								int32& as_ast_e460_target = as_ast_d39;
								as_ast_e460_target = as_ast_e460_value;
								return as_ast_e460_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return {};
							}
						}
						for ( ; ([&]() -> bool
					{
						const int32 as_ast_e466_left = ([&]() -> int32
					{
						TypedFrame.SetSourcePosition(267, 3);
						const int32 as_ast_e463_left = int32(10);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						const int32 as_ast_e463_right = as_ast_d22;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e463_left, as_ast_e463_right);
					}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						const int32 as_ast_e466_right = as_ast_d39;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						return (as_ast_e466_left > as_ast_e466_right);
					}()) ; AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d39) )
						{
							{
								TypedFrame.SetSourcePosition(269, 4);
								break;
							}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(271, 3);
						return as_ast_d26;
					}
				}
			case int32(13):
				{
					{
						TypedFrame.SetSourcePosition(275, 3);
						while (([&]() -> bool
						{
							const int32 as_ast_e474_left = as_ast_d26;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e474_right = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e474_left == as_ast_e474_right);
						}()))
						{
							{
								TypedFrame.SetSourcePosition(277, 4);
								([&]() -> int32
								{
									const int32 as_ast_e482_value = ([&]() -> int32
								{
									TypedFrame.SetSourcePosition(277, 4);
									const int32 as_ast_e478_left = int32(10);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									const int32 as_ast_e478_right = as_ast_d22;
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e478_left, as_ast_e478_right);
								}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e482_target = as_ast_d26;
									as_ast_e482_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e482_target, as_ast_e482_value);
									return as_ast_e482_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(278, 4);
								([&]() -> int32
								{
									const int32 as_ast_e489_value = int32(100);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e489_target = as_ast_d26;
									as_ast_e489_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e489_target, as_ast_e489_value);
									return as_ast_e489_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(279, 4);
								break;
							}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(281, 3);
						return as_ast_d26;
					}
				}
			case int32(14):
				{
					int32 as_ast_d40;
					{
						TypedFrame.SetSourcePosition(285, 7);
						([&]() -> int32
						{
							const int32 as_ast_e496_value = int32(0);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							int32& as_ast_e496_target = as_ast_d40;
							as_ast_e496_target = as_ast_e496_value;
							return as_ast_e496_target;
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(286, 3);
						do
						{
							{
								TypedFrame.SetSourcePosition(288, 4);
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d40);
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(289, 4);
								const bool as_ast_stmt_252_condition = ([&]() -> bool
								{
									const int32 as_ast_e501_left = as_ast_d40;
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return bool{};
									}
									const int32 as_ast_e501_right = int32(1);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return bool{};
									}
									return (as_ast_e501_left == as_ast_e501_right);
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
								if (as_ast_stmt_252_condition)
								{
									{
										TypedFrame.SetSourcePosition(291, 5);
										continue;
									}
								}
							}
							{
								TypedFrame.SetSourcePosition(293, 4);
								([&]() -> int32
								{
									const int32 as_ast_e511_value = ([&]() -> int32
								{
									const int32 as_ast_e507_left = as_ast_d40;
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									const int32 as_ast_e507_right = int32(10);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e507_left, as_ast_e507_right);
								}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e511_target = as_ast_d26;
									as_ast_e511_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e511_target, as_ast_e511_value);
									return as_ast_e511_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
						}
						while (([&]() -> bool
						{
							const int32 as_ast_e516_left = as_ast_d40;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e516_right = int32(3);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e516_left < as_ast_e516_right);
						}()));
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(296, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e523_left = ([&]() -> int32
					{
						const int32 as_ast_e522_left = as_ast_d26;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						const int32 as_ast_e522_right = int32(10);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e522_left, as_ast_e522_right);
					}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						const int32 as_ast_e523_right = as_ast_d40;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e523_left, as_ast_e523_right);
					}());
					}
				}
			case int32(15):
				{
					{
						TypedFrame.SetSourcePosition(300, 3);
						const bool as_ast_stmt_260_condition = ([&]() -> bool
						{
							const int32 as_ast_e528_left = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e528_right = int32(0);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e528_left < as_ast_e528_right);
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
						if (as_ast_stmt_260_condition)
						{
							{
								TypedFrame.SetSourcePosition(302, 4);
								return AngelscriptTypedASTJIT::WrapNegate<int32>(int32(10));
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(304, 3);
						const bool as_ast_stmt_263_condition = ([&]() -> bool
						{
							const int32 as_ast_e535_left = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e535_right = int32(0);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e535_left == as_ast_e535_right);
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
						if (as_ast_stmt_263_condition)
						{
							{
								TypedFrame.SetSourcePosition(306, 4);
								return int32(0);
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(308, 3);
						const bool as_ast_stmt_272_condition = as_ast_d24;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
						if (as_ast_stmt_272_condition)
						{
							{
								TypedFrame.SetSourcePosition(310, 4);
								const bool as_ast_stmt_266_condition = as_ast_d25;
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
								if (as_ast_stmt_266_condition)
								{
									{
										TypedFrame.SetSourcePosition(312, 5);
										return int32(11);
									}
								}
							}
							{
								TypedFrame.SetSourcePosition(314, 4);
								return int32(12);
							}
						}
						else
						{
							TypedFrame.SetSourcePosition(316, 8);
							const bool as_ast_stmt_271_condition = as_ast_d25;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return {};
							}
							if (as_ast_stmt_271_condition)
							{
								{
									TypedFrame.SetSourcePosition(318, 4);
									return int32(13);
								}
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(320, 3);
						return int32(14);
					}
				}
			case int32(16):
				{
					{
						TypedFrame.SetSourcePosition(324, 3);
						const auto as_ast_stmt_276_condition = as_ast_d22;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
						switch (as_ast_stmt_276_condition)
						{
							case int32(0):
								{
									TypedFrame.SetSourcePosition(327, 4);
									([&]() -> int32
									{
										const int32 as_ast_e552_value = int32(1);
										if (UNLIKELY(Execution.bExceptionThrown))
										{
											return int32{};
										}
										int32& as_ast_e552_target = as_ast_d26;
										as_ast_e552_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e552_target, as_ast_e552_value);
										return as_ast_e552_target;
									}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return {};
									}
								}
							case int32(1):
								{
									int32 as_ast_d41;
									{
										TypedFrame.SetSourcePosition(331, 8);
										([&]() -> int32
										{
											const int32 as_ast_e558_value = int32(20);
											if (UNLIKELY(Execution.bExceptionThrown))
											{
												return int32{};
											}
											int32& as_ast_e558_target = as_ast_d41;
											as_ast_e558_target = as_ast_e558_value;
											return as_ast_e558_target;
										}());
										if (UNLIKELY(Execution.bExceptionThrown))
										{
											return {};
										}
									}
									{
										TypedFrame.SetSourcePosition(332, 4);
										([&]() -> int32
										{
											const int32 as_ast_e562_value = as_ast_d41;
											if (UNLIKELY(Execution.bExceptionThrown))
											{
												return int32{};
											}
											int32& as_ast_e562_target = as_ast_d26;
											as_ast_e562_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e562_target, as_ast_e562_value);
											return as_ast_e562_target;
										}());
										if (UNLIKELY(Execution.bExceptionThrown))
										{
											return {};
										}
									}
									{
										TypedFrame.SetSourcePosition(333, 4);
										break;
									}
								}
							default:
								{
									int32 as_ast_d42;
									{
										TypedFrame.SetSourcePosition(337, 8);
										([&]() -> int32
										{
											const int32 as_ast_e567_value = int32(30);
											if (UNLIKELY(Execution.bExceptionThrown))
											{
												return int32{};
											}
											int32& as_ast_e567_target = as_ast_d42;
											as_ast_e567_target = as_ast_e567_value;
											return as_ast_e567_target;
										}());
										if (UNLIKELY(Execution.bExceptionThrown))
										{
											return {};
										}
									}
									{
										TypedFrame.SetSourcePosition(338, 4);
										([&]() -> int32
										{
											const int32 as_ast_e571_value = as_ast_d42;
											if (UNLIKELY(Execution.bExceptionThrown))
											{
												return int32{};
											}
											int32& as_ast_e571_target = as_ast_d26;
											as_ast_e571_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e571_target, as_ast_e571_value);
											return as_ast_e571_target;
										}());
										if (UNLIKELY(Execution.bExceptionThrown))
										{
											return {};
										}
									}
									{
										TypedFrame.SetSourcePosition(339, 4);
										break;
									}
								}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(342, 3);
						return as_ast_d26;
					}
				}
			default:
				{
					TypedFrame.SetSourcePosition(345, 3);
					return AngelscriptTypedASTJIT::WrapNegate<int32>(int32(1));
				}
		}
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int SemanticCloneableState(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:5
// JIT Entry   : Raw
int32 ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_519b1553e7481d1d437988c873b5b540ceab439acecb19969d7cd4b85bad1170_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1, asBYTE p_2, asBYTE p_3)
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
	return ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_519b1553e7481d1d437988c873b5b540ceab439acecb19969d7cd4b85bad1170_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1), static_cast<bool>(p_2), static_cast<bool>(p_3));
}

// AS Function : int SemanticCloneableState(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:5
// JIT Entry   : VM
void ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_519b1553e7481d1d437988c873b5b540ceab439acecb19969d7cd4b85bad1170_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_519b1553e7481d1d437988c873b5b540ceab439acecb19969d7cd4b85bad1170_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1),
		*(asBYTE*)(l_fp + 2),
		*(asBYTE*)(l_fp + 3));
}

// AS Function : int SemanticCloneableState(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:5
// JIT Entry   : Parms
void ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_519b1553e7481d1d437988c873b5b540ceab439acecb19969d7cd4b85bad1170_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_519b1553e7481d1d437988c873b5b540ceab439acecb19969d7cd4b85bad1170_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)),
		static_cast<asBYTE>(*(bool*)(((SIZE_T)Parms) + ParmOffset_2)),
		static_cast<asBYTE>(*(bool*)(((SIZE_T)Parms) + ParmOffset_3)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticStructuredControlFlow(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:9:5
 * StableFunctionKey  : 5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43
 * ExecutionHash      : 2a72a7acf6d954fbbd348d78caa3485a18b29c0797286c1c9b38f7153214d7f4
 * DebugHash          : b354187a84dc67df025e2b1368647f0c2329bdec6385021a680cc8377f60c022
 * EntryAbiHash       : 2fe16db0121b593e09cfa3e040d4124228d3d043449cc2ba5580b68918773fef
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 32
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_2a72a7acf6d954fbbd348d78caa3485a18b29c0797286c1c9b38f7153214d7f4_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d7,
	const int32 as_ast_d8);

static int32 ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_2a72a7acf6d954fbbd348d78caa3485a18b29c0797286c1c9b38f7153214d7f4_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d7,
	const int32 as_ast_d8)
{
	int32 as_ast_d9;
	{
		TypedFrame.SetSourcePosition(11, 6);
		([&]() -> int32
		{
			const int32 as_ast_e3_value = int32(0);
			int32& as_ast_e3_target = as_ast_d9;
			as_ast_e3_target = as_ast_e3_value;
			return as_ast_e3_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(12, 2);
		int32 as_ast_d10;
		{
			TypedFrame.SetSourcePosition(12, 11);
			([&]() -> int32
			{
				const int32 as_ast_e6_value = int32(0);
				int32& as_ast_e6_target = as_ast_d10;
				as_ast_e6_target = as_ast_e6_value;
				return as_ast_e6_target;
			}());
		}
		for ( ; ([&]() -> bool
	{
		const int32 as_ast_e9_left = as_ast_d10;
		const int32 as_ast_e9_right = as_ast_d7;
		return (as_ast_e9_left < as_ast_e9_right);
	}()) ; AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d10) )
		{
			{
				TypedFrame.SetSourcePosition(14, 3);
				if (([&]() -> bool
				{
					const int32 as_ast_e14_left = as_ast_d10;
					const int32 as_ast_e14_right = int32(1);
					return (as_ast_e14_left == as_ast_e14_right);
				}()))
				{
					{
						TypedFrame.SetSourcePosition(16, 4);
						continue;
					}
				}
			}
			{
				TypedFrame.SetSourcePosition(19, 3);
				switch (as_ast_d10)
				{
					case int32(2):
						{
							TypedFrame.SetSourcePosition(22, 4);
							([&]() -> int32
							{
								const int32 as_ast_e22_value = int32(20);
								int32& as_ast_e22_target = as_ast_d9;
								as_ast_e22_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e22_target, as_ast_e22_value);
								return as_ast_e22_target;
							}());
						}
						{
							TypedFrame.SetSourcePosition(23, 4);
							break;
						}
					case int32(3):
						{
							TypedFrame.SetSourcePosition(25, 4);
							([&]() -> int32
							{
								const int32 as_ast_e30_value = int32(30);
								int32& as_ast_e30_target = as_ast_d9;
								as_ast_e30_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e30_target, as_ast_e30_value);
								return as_ast_e30_target;
							}());
						}
						{
							TypedFrame.SetSourcePosition(26, 4);
							continue;
						}
					default:
						{
							TypedFrame.SetSourcePosition(28, 4);
							([&]() -> int32
							{
								const int32 as_ast_e37_value = as_ast_d10;
								int32& as_ast_e37_target = as_ast_d9;
								as_ast_e37_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e37_target, as_ast_e37_value);
								return as_ast_e37_target;
							}());
						}
						{
							TypedFrame.SetSourcePosition(29, 4);
							break;
						}
				}
			}
			{
				TypedFrame.SetSourcePosition(32, 3);
				if (([&]() -> bool
				{
					const int32 as_ast_e41_left = as_ast_d9;
					const int32 as_ast_e41_right = int32(45);
					return (as_ast_e41_left > as_ast_e41_right);
				}()))
				{
					{
						TypedFrame.SetSourcePosition(34, 4);
						break;
					}
				}
			}
		}
	}
	int32 as_ast_d11;
	{
		TypedFrame.SetSourcePosition(38, 6);
		([&]() -> int32
		{
			const int32 as_ast_e46_value = int32(0);
			int32& as_ast_e46_target = as_ast_d11;
			as_ast_e46_target = as_ast_e46_value;
			return as_ast_e46_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(39, 2);
		while (([&]() -> bool
		{
			const int32 as_ast_e49_left = as_ast_d11;
			const int32 as_ast_e49_right = as_ast_d7;
			return (as_ast_e49_left < as_ast_e49_right);
		}()))
		{
			{
				TypedFrame.SetSourcePosition(41, 3);
				AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d11);
			}
			{
				TypedFrame.SetSourcePosition(42, 3);
				if (([&]() -> bool
				{
					const int32 as_ast_e54_left = as_ast_d11;
					const int32 as_ast_e54_right = int32(2);
					return (as_ast_e54_left == as_ast_e54_right);
				}()))
				{
					{
						TypedFrame.SetSourcePosition(44, 4);
						continue;
					}
				}
			}
			{
				TypedFrame.SetSourcePosition(46, 3);
				([&]() -> int32
				{
					const int32 as_ast_e61_value = int32(3);
					int32& as_ast_e61_target = as_ast_d9;
					as_ast_e61_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e61_target, as_ast_e61_value);
					return as_ast_e61_target;
				}());
			}
			{
				TypedFrame.SetSourcePosition(47, 3);
				if (([&]() -> bool
				{
					const int32 as_ast_e66_left = as_ast_d11;
					const int32 as_ast_e66_right = int32(4);
					return (as_ast_e66_left >= as_ast_e66_right);
				}()))
				{
					{
						TypedFrame.SetSourcePosition(49, 4);
						break;
					}
				}
			}
		}
	}
	int32 as_ast_d12;
	{
		TypedFrame.SetSourcePosition(53, 6);
		([&]() -> int32
		{
			const int32 as_ast_e71_value = int32(0);
			int32& as_ast_e71_target = as_ast_d12;
			as_ast_e71_target = as_ast_e71_value;
			return as_ast_e71_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(54, 2);
		do
		{
			{
				TypedFrame.SetSourcePosition(56, 3);
				AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d12);
			}
			{
				TypedFrame.SetSourcePosition(57, 3);
				if (([&]() -> bool
				{
					const int32 as_ast_e76_left = as_ast_d12;
					const int32 as_ast_e76_right = int32(1);
					return (as_ast_e76_left == as_ast_e76_right);
				}()))
				{
					{
						TypedFrame.SetSourcePosition(59, 4);
						continue;
					}
				}
			}
			{
				TypedFrame.SetSourcePosition(61, 3);
				([&]() -> int32
				{
					const int32 as_ast_e83_value = int32(5);
					int32& as_ast_e83_target = as_ast_d9;
					as_ast_e83_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e83_target, as_ast_e83_value);
					return as_ast_e83_target;
				}());
			}
		}
		while (([&]() -> bool
		{
			const int32 as_ast_e88_left = as_ast_d12;
			const int32 as_ast_e88_right = int32(2);
			return (as_ast_e88_left < as_ast_e88_right);
		}()));
	}
	{
		TypedFrame.SetSourcePosition(65, 2);
		switch (as_ast_d8)
		{
			case int32(0):
				{
					TypedFrame.SetSourcePosition(68, 3);
					([&]() -> int32
					{
						const int32 as_ast_e96_value = int32(100);
						int32& as_ast_e96_target = as_ast_d9;
						as_ast_e96_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e96_target, as_ast_e96_value);
						return as_ast_e96_target;
					}());
				}
				{
					TypedFrame.SetSourcePosition(69, 3);
					break;
				}
			case int32(1):
				{
					TypedFrame.SetSourcePosition(71, 3);
					([&]() -> int32
					{
						const int32 as_ast_e104_value = int32(200);
						int32& as_ast_e104_target = as_ast_d9;
						as_ast_e104_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e104_target, as_ast_e104_value);
						return as_ast_e104_target;
					}());
				}
			case int32(2):
				{
					int32 as_ast_d13;
					{
						TypedFrame.SetSourcePosition(74, 7);
						([&]() -> int32
						{
							const int32 as_ast_e110_value = int32(20);
							int32& as_ast_e110_target = as_ast_d13;
							as_ast_e110_target = as_ast_e110_value;
							return as_ast_e110_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(75, 3);
						([&]() -> int32
						{
							const int32 as_ast_e114_value = as_ast_d13;
							int32& as_ast_e114_target = as_ast_d9;
							as_ast_e114_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e114_target, as_ast_e114_value);
							return as_ast_e114_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(76, 3);
						break;
					}
				}
			case int32(3):
				{
					int32 as_ast_d14;
					{
						TypedFrame.SetSourcePosition(80, 7);
						([&]() -> int32
						{
							const int32 as_ast_e119_value = int32(0);
							int32& as_ast_e119_target = as_ast_d14;
							as_ast_e119_target = as_ast_e119_value;
							return as_ast_e119_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(81, 3);
						while (([&]() -> bool
						{
							const int32 as_ast_e122_left = as_ast_d14;
							const int32 as_ast_e122_right = int32(2);
							return (as_ast_e122_left < as_ast_e122_right);
						}()))
						{
							{
								TypedFrame.SetSourcePosition(83, 4);
								([&]() -> int32
								{
									const int32 as_ast_e127_value = int32(7);
									int32& as_ast_e127_target = as_ast_d9;
									as_ast_e127_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e127_target, as_ast_e127_value);
									return as_ast_e127_target;
								}());
							}
							{
								TypedFrame.SetSourcePosition(84, 4);
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d14);
							}
							{
								TypedFrame.SetSourcePosition(85, 4);
								if (([&]() -> bool
								{
									const int32 as_ast_e134_left = as_ast_d14;
									const int32 as_ast_e134_right = int32(1);
									return (as_ast_e134_left == as_ast_e134_right);
								}()))
								{
									{
										TypedFrame.SetSourcePosition(87, 5);
										continue;
									}
								}
							}
							{
								TypedFrame.SetSourcePosition(89, 4);
								break;
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(91, 3);
						break;
					}
				}
			default:
				{
					TypedFrame.SetSourcePosition(94, 3);
					([&]() -> int32
					{
						const int32 as_ast_e144_value = int32(9);
						int32& as_ast_e144_target = as_ast_d9;
						as_ast_e144_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e144_target, as_ast_e144_value);
						return as_ast_e144_target;
					}());
				}
				{
					TypedFrame.SetSourcePosition(95, 3);
					break;
				}
		}
	}
	{
		TypedFrame.SetSourcePosition(97, 2);
		return as_ast_d9;
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int SemanticStructuredControlFlow(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:9:5
// JIT Entry   : Raw
int32 ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_2a72a7acf6d954fbbd348d78caa3485a18b29c0797286c1c9b38f7153214d7f4_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
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
	return ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_2a72a7acf6d954fbbd348d78caa3485a18b29c0797286c1c9b38f7153214d7f4_TypedBody(TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int SemanticStructuredControlFlow(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:9:5
// JIT Entry   : VM
void ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_2a72a7acf6d954fbbd348d78caa3485a18b29c0797286c1c9b38f7153214d7f4_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_2a72a7acf6d954fbbd348d78caa3485a18b29c0797286c1c9b38f7153214d7f4_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int SemanticStructuredControlFlow(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:9:5
// JIT Entry   : Parms
void ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_2a72a7acf6d954fbbd348d78caa3485a18b29c0797286c1c9b38f7153214d7f4_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_2a72a7acf6d954fbbd348d78caa3485a18b29c0797286c1c9b38f7153214d7f4_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticCheckedDivide(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:5
 * StableFunctionKey  : a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5
 * ExecutionHash      : 5821bbe194a5e0baea8666a1f2ffdd243992a522b594aea35a9cf34698f5a18e
 * DebugHash          : 89a3d6ad2685fce9c5148240b40ea3ef2c7c0164e92a49f023034e651886331a
 * EntryAbiHash       : 34574e76e092ff59a9ab137303c6c8a1a00f50e50bf9ce931ed07453463297ff
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_5821bbe194a5e0baea8666a1f2ffdd243992a522b594aea35a9cf34698f5a18e_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d19,
	const int32 as_ast_d20);

static int32 ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_5821bbe194a5e0baea8666a1f2ffdd243992a522b594aea35a9cf34698f5a18e_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d19,
	const int32 as_ast_d20)
{
	{
		TypedFrame.SetSourcePosition(118, 2);
		return ([&]() -> int32
	{
		TypedFrame.SetSourcePosition(118, 2);
		const int32 as_ast_e161_left = as_ast_d19;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		const int32 as_ast_e161_right = as_ast_d20;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e161_left, as_ast_e161_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int SemanticCheckedDivide(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:5
// JIT Entry   : Raw
int32 ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_5821bbe194a5e0baea8666a1f2ffdd243992a522b594aea35a9cf34698f5a18e_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
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
	return ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_5821bbe194a5e0baea8666a1f2ffdd243992a522b594aea35a9cf34698f5a18e_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int SemanticCheckedDivide(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:5
// JIT Entry   : VM
void ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_5821bbe194a5e0baea8666a1f2ffdd243992a522b594aea35a9cf34698f5a18e_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_5821bbe194a5e0baea8666a1f2ffdd243992a522b594aea35a9cf34698f5a18e_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int SemanticCheckedDivide(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:5
// JIT Entry   : Parms
void ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_5821bbe194a5e0baea8666a1f2ffdd243992a522b594aea35a9cf34698f5a18e_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_5821bbe194a5e0baea8666a1f2ffdd243992a522b594aea35a9cf34698f5a18e_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
