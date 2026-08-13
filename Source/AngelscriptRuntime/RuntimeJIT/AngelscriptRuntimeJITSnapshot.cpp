#include "RuntimeJIT/AngelscriptRuntimeJITSnapshot.h"

#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"
#include "Core/AngelscriptEngine.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_datatype.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptRuntimeJITSnapshot_Private
{
	static FAngelscriptRuntimeJITContractValidationResult Invalid(
		const EAngelscriptRuntimeJITContractValidationCode Code,
		const int32 ElementIndex = INDEX_NONE)
	{
		return {Code, ElementIndex};
	}

	static bool IsPowerOfTwo(const uint32 Value)
	{
		return Value != 0 && (Value & (Value - 1)) == 0;
	}

	static bool IsConditionalBranch(const asEBCInstr Opcode)
	{
		return Opcode == asBC_JZ || Opcode == asBC_JNZ
			|| Opcode == asBC_JLowZ || Opcode == asBC_JLowNZ
			|| Opcode == asBC_JS || Opcode == asBC_JNS
			|| Opcode == asBC_JP || Opcode == asBC_JNP;
	}

	static bool HasBackwardControlFlowEdge(
		const TConstArrayView<FAngelscriptRuntimeJITControlFlowEdge> Edges)
	{
		return Edges.ContainsByPredicate(
			[](const FAngelscriptRuntimeJITControlFlowEdge& Edge)
			{
				return Edge.ToInstruction <= Edge.FromInstruction;
			});
	}

	static bool IsReferenceBearingOpcode(const asEBCInstr Opcode)
	{
		switch (Opcode)
		{
		case asBC_PshGPtr:
		case asBC_PshG4:
		case asBC_LdGRdR4:
		case asBC_CpyVtoG4:
		case asBC_CpyGtoV4:
		case asBC_LDG:
		case asBC_PGA:
		case asBC_SetG4:
		case asBC_CALL:
		case asBC_CALLBND:
		case asBC_CALLINTF:
		case asBC_CopyScript:
		case asBC_FinConstruct:
		case asBC_FREE:
		case asBC_REFCPY:
		case asBC_RefCpyV:
		case asBC_DestructScript:
		case asBC_OBJTYPE:
		case asBC_COPY:
		case asBC_STR:
		case asBC_CALLSYS:
		case asBC_FuncPtr:
		case asBC_Thiscall1:
		case asBC_ALLOC:
		case asBC_TYPEID:
		case asBC_Cast:
		case asBC_ADDSi:
		case asBC_LoadThisR:
		case asBC_LoadRObjR:
		case asBC_LoadVObjR:
		case asBC_SetListType:
			return true;
		default:
			return false;
		}
	}

	static bool IsFunctionReferenceOpcode(const asEBCInstr Opcode)
	{
		switch (Opcode)
		{
		case asBC_CALL:
		case asBC_CALLINTF:
		case asBC_CALLBND:
		case asBC_CALLSYS:
		case asBC_Thiscall1:
		case asBC_FuncPtr:
			return true;
		default:
			return false;
		}
	}

	static bool UsesFunctionIdOperand(const asEBCInstr Opcode)
	{
		return Opcode == asBC_CALL || Opcode == asBC_CALLINTF
			|| Opcode == asBC_CALLBND;
	}

	static bool IsValidTokenKindForOpcode(
		const asEBCInstr Opcode,
		const EAngelscriptRuntimeJITHelperTokenKind Kind)
	{
		switch (Opcode)
		{
		case asBC_CALL:
		case asBC_CALLINTF:
			return Kind == EAngelscriptRuntimeJITHelperTokenKind::ScriptFunction;
		case asBC_CALLSYS:
		case asBC_Thiscall1:
			return Kind == EAngelscriptRuntimeJITHelperTokenKind::SystemFunction;
		case asBC_CALLBND:
		case asBC_FuncPtr:
			return Kind == EAngelscriptRuntimeJITHelperTokenKind::ScriptFunction
				|| Kind == EAngelscriptRuntimeJITHelperTokenKind::SystemFunction;
		default:
			return false;
		}
	}

	static EAngelscriptRuntimeJITScalarKind ScalarKind(
		const asCDataType& Type)
	{
		if (!Type.IsPrimitive() || Type.IsReference())
		{
			return EAngelscriptRuntimeJITScalarKind::OpaquePointer;
		}
		switch (Type.GetTokenType())
		{
		case ttBool: return EAngelscriptRuntimeJITScalarKind::Bool;
		case ttInt8: return EAngelscriptRuntimeJITScalarKind::Int8;
		case ttUInt8: return EAngelscriptRuntimeJITScalarKind::UInt8;
		case ttInt16: return EAngelscriptRuntimeJITScalarKind::Int16;
		case ttUInt16: return EAngelscriptRuntimeJITScalarKind::UInt16;
		case ttInt: return EAngelscriptRuntimeJITScalarKind::Int32;
		case ttUInt: return EAngelscriptRuntimeJITScalarKind::UInt32;
		case ttInt64: return EAngelscriptRuntimeJITScalarKind::Int64;
		case ttUInt64: return EAngelscriptRuntimeJITScalarKind::UInt64;
		case ttFloat32: return EAngelscriptRuntimeJITScalarKind::Float32;
		case ttFloat:
		case ttFloat64:
		case ttDouble: return EAngelscriptRuntimeJITScalarKind::Float64;
		default: return EAngelscriptRuntimeJITScalarKind::Invalid;
		}
	}

	static uint32 ValueByteSize(const asCDataType& Type)
	{
		if (!Type.IsPrimitive() || Type.IsReference())
		{
			return static_cast<uint32>(FMath::Max(
				Type.GetSizeOnStackDWords() * static_cast<int32>(sizeof(asDWORD)),
				static_cast<int32>(sizeof(void*))));
		}
		return static_cast<uint32>(FMath::Max(Type.GetSizeInMemoryBytes(), 1));
	}

	static uint32 ValueAlignment(const asCDataType& Type)
	{
		const int32 Alignment = Type.GetAlignment();
		return static_cast<uint32>(FMath::Max(Alignment, 1));
	}

	static void AddFrameValue(
		TArray<FAngelscriptRuntimeJITFrameValue>& Values,
		const asCDataType& Type,
		const EAngelscriptRuntimeJITFrameValueFlags Flags,
		const uint32 ByteOffset,
		const uint32 SourceIndex,
		bool& bHasManagedValues)
	{
		FAngelscriptRuntimeJITFrameValue& Value = Values.Emplace_GetRef();
		Value.Kind = ScalarKind(Type);
		Value.Flags = Flags;
		Value.ByteOffset = ByteOffset;
		Value.ByteSize = ValueByteSize(Type);
		Value.Alignment = ValueAlignment(Type);
		Value.SourceIndex = SourceIndex;
		if (Value.Kind == EAngelscriptRuntimeJITScalarKind::Invalid
			|| Value.Kind == EAngelscriptRuntimeJITScalarKind::OpaquePointer)
		{
			bHasManagedValues = true;
		}
	}

	static bool AddCanonicalLocalFrameValue(
		TArray<FAngelscriptRuntimeJITFrameValue>& Values,
		const asCDataType& Type,
		const uint32 ByteOffset,
		const uint32 SourceIndex,
		bool& bHasManagedValues)
	{
		FAngelscriptRuntimeJITFrameValue Candidate;
		Candidate.Kind = ScalarKind(Type);
		Candidate.Flags = EAngelscriptRuntimeJITFrameValueFlags::Local;
		Candidate.ByteOffset = ByteOffset;
		Candidate.ByteSize = ValueByteSize(Type);
		Candidate.Alignment = ValueAlignment(Type);
		Candidate.SourceIndex = SourceIndex;
		if (Candidate.Kind == EAngelscriptRuntimeJITScalarKind::Invalid
			|| Candidate.Kind == EAngelscriptRuntimeJITScalarKind::OpaquePointer)
		{
			bHasManagedValues = true;
		}

		const FAngelscriptRuntimeJITFrameValue* Existing = Values.FindByPredicate(
			[ByteOffset](const FAngelscriptRuntimeJITFrameValue& Value)
			{
				return EnumHasAnyFlags(Value.Flags,
					EAngelscriptRuntimeJITFrameValueFlags::Local)
					&& Value.ByteOffset == ByteOffset;
			});
		if (Existing == nullptr)
		{
			Values.Add(Candidate);
			return true;
		}
		return Existing->Kind == Candidate.Kind
			&& Existing->ByteSize == Candidate.ByteSize
			&& Existing->Alignment == Candidate.Alignment;
	}

	static FAngelscriptHash256 BuildScriptFunctionAbi(
		const asCScriptFunction& Function,
		const FAngelscriptStableFunctionKey& FunctionKey)
	{
		if (FunctionKey.Hash.IsZero())
		{
			return {};
		}
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("runtime-jit-script-function-abi-v1"));
		Writer.WriteHash(FunctionKey.Hash);
		Writer.WriteString(UTF8_TO_TCHAR(
			Function.GetDeclaration(false, false, false)));
		auto WriteType = [&Writer](const asCDataType& Type)
		{
			const asCString Formatted = Type.Format(nullptr, true, false);
			Writer.WriteString(UTF8_TO_TCHAR(Formatted.AddressOf()));
		};
		WriteType(Function.returnType);
		Writer.WriteUInt32(Function.parameterTypes.GetLength());
		for (asUINT Index = 0; Index < Function.parameterTypes.GetLength(); ++Index)
		{
			WriteType(Function.parameterTypes[Index]);
			Writer.WriteUInt32(Index < Function.inOutFlags.GetLength()
				? static_cast<uint32>(Function.inOutFlags[Index]) : 0u);
		}
		return Writer.FinalizeHash();
	}

	static bool TryBuildFunctionToken(
		FAngelscriptEngine& RuntimeEngine,
		const FAngelscriptStableModuleKey& CallerModuleKey,
		const asCModule* CallerModule,
		asCScriptFunction& Target,
		const uint32 SlotIndex,
		FAngelscriptRuntimeJITHelperToken& OutToken)
	{
		OutToken = {};
		OutToken.SlotIndex = SlotIndex;
		if (Target.funcType == asFUNC_SYSTEM && Target.module == nullptr)
		{
			FAngelscriptArtifactReference Reference;
			if (!FAngelscriptCacheEnvironmentIdentity::TryBuildFunctionReference(
					Target, Reference))
			{
				return false;
			}
			OutToken.Kind = EAngelscriptRuntimeJITHelperTokenKind::SystemFunction;
			OutToken.StableKey = Reference.StableKey;
			OutToken.ExpectedAbi = Reference.ExpectedAbi;
			return true;
		}

		FAngelscriptStableFunctionKey FunctionKey;
		RuntimeEngine.TryResolveStableFunctionKey(&Target, FunctionKey);
		if (FunctionKey.Hash.IsZero()
			&& Target.module == CallerModule
			&& !FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				CallerModuleKey, Target, FunctionKey))
		{
			return false;
		}
		OutToken.Kind = EAngelscriptRuntimeJITHelperTokenKind::ScriptFunction;
		OutToken.StableKey = FunctionKey.Hash;
		OutToken.ExpectedAbi = BuildScriptFunctionAbi(Target, FunctionKey);
		return !OutToken.StableKey.IsZero() && !OutToken.ExpectedAbi.IsZero();
	}

	static asCScriptFunction* ResolveFunctionOperand(
		asCScriptEngine& Engine,
		const asEBCInstr Opcode,
		const asDWORD* Instruction)
	{
		if (!UsesFunctionIdOperand(Opcode))
		{
			return reinterpret_cast<asCScriptFunction*>(
				asBC_PTRARG(Instruction));
		}
		int32 FunctionId = asBC_INTARG(Instruction);
		if (Opcode == asBC_CALLBND)
		{
			if ((FunctionId & FUNC_IMPORTED) == 0)
			{
				return nullptr;
			}
			const int32 ImportIndex = FunctionId & ~FUNC_IMPORTED;
			if (ImportIndex < 0
				|| ImportIndex >= static_cast<int32>(Engine.importedFunctions.GetLength())
				|| Engine.importedFunctions[ImportIndex] == nullptr)
			{
				return nullptr;
			}
			FunctionId = Engine.importedFunctions[ImportIndex]->boundFunctionId;
		}
		return FunctionId >= 0 ? Engine.GetScriptFunction(FunctionId) : nullptr;
	}

	static void WriteFunctionTokenSlot(
		asDWORD* Instruction,
		const asEBCInstr Opcode,
		const uint32 SlotIndex)
	{
		if (UsesFunctionIdOperand(Opcode))
		{
			asBC_INTARG(Instruction) = static_cast<int32>(SlotIndex);
		}
		else
		{
			asBC_PTRARG(Instruction) = static_cast<asPWORD>(SlotIndex);
		}
	}

	static uint32 ReadFunctionTokenSlot(
		const asDWORD* Instruction,
		const asEBCInstr Opcode)
	{
		return UsesFunctionIdOperand(Opcode)
			? static_cast<uint32>(asBC_INTARG(Instruction))
			: static_cast<uint32>(asBC_PTRARG(Instruction));
	}

	static FAngelscriptHash256 BuildEntryAbiHash(
		const FAngelscriptRuntimeJITCompileSnapshotHeader& Header,
		const FStringView Declaration,
		const TConstArrayView<FAngelscriptRuntimeJITFrameValue> Values)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("runtime-jit-vm-entry-abi-v1"));
		Writer.WriteUInt32(Header.EntryAbiRevision);
		Writer.WriteString(Declaration);
		Writer.WriteUInt8(Header.PointerWidth);
		Writer.WriteUInt8(static_cast<uint8>(Header.Endianness));
		Writer.WriteUInt8(static_cast<uint8>(Header.CallingConvention));
		Writer.WriteUInt8(static_cast<uint8>(Header.Invocation));
		Writer.WriteUInt8(static_cast<uint8>(Header.Receiver));
		Writer.WriteUInt32(static_cast<uint32>(Header.ReceiverParameterIndex));
		Writer.WriteUInt32(Header.ParameterFrameByteSize);
		Writer.WriteUInt32(Header.LocalFrameByteSize);
		Writer.WriteUInt32(Header.ResultByteSize);
		Writer.WriteUInt32(static_cast<uint32>(Values.Num()));
		for (const FAngelscriptRuntimeJITFrameValue& Value : Values)
		{
			Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
			Writer.WriteUInt8(static_cast<uint8>(Value.Flags));
			Writer.WriteUInt32(Value.ByteOffset);
			Writer.WriteUInt32(Value.ByteSize);
			Writer.WriteUInt32(Value.Alignment);
			Writer.WriteUInt32(Value.SourceIndex);
		}
		return Writer.FinalizeHash();
	}

	static FAngelscriptHash256 BuildFunctionRevision(
		const FAngelscriptRuntimeJITCompileSnapshot& Snapshot,
		const FAngelscriptHash256& ExecutionHash)
	{
		const FAngelscriptRuntimeJITCompileSnapshotView View = Snapshot.GetView();
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("runtime-jit-function-revision-v1"));
		Writer.WriteHash(View.Header.FunctionKey.Hash);
		Writer.WriteHash(ExecutionHash);
		Writer.WriteHash(View.Header.EntryAbiHash);
		Writer.WriteUInt8(static_cast<uint8>(View.Header.FunctionProfile));
		Writer.WriteBytes(MakeArrayView(
			reinterpret_cast<const uint8*>(View.Bytecode),
			static_cast<int32>(View.BytecodeWordCount * sizeof(asDWORD))));
		Writer.WriteUInt32(View.FrameValueCount);
		for (uint32 Index = 0; Index < View.FrameValueCount; ++Index)
		{
			const FAngelscriptRuntimeJITFrameValue& Value = View.FrameValues[Index];
			Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
			Writer.WriteUInt8(static_cast<uint8>(Value.Flags));
			Writer.WriteUInt32(Value.ByteOffset);
			Writer.WriteUInt32(Value.ByteSize);
			Writer.WriteUInt32(Value.Alignment);
			Writer.WriteUInt32(Value.SourceIndex);
		}
		Writer.WriteUInt32(View.InstructionCount);
		for (uint32 Index = 0; Index < View.InstructionCount; ++Index)
		{
			Writer.WriteUInt32(View.InstructionWordOffsets[Index]);
		}
		Writer.WriteUInt32(View.ControlFlowEdgeCount);
		for (uint32 Index = 0; Index < View.ControlFlowEdgeCount; ++Index)
		{
			const FAngelscriptRuntimeJITControlFlowEdge& Edge =
				View.ControlFlowEdges[Index];
			Writer.WriteUInt32(Edge.FromInstruction);
			Writer.WriteUInt32(Edge.ToInstruction);
			Writer.WriteUInt8(static_cast<uint8>(Edge.Kind));
		}
		Writer.WriteUInt32(View.HelperTokenCount);
		for (uint32 Index = 0; Index < View.HelperTokenCount; ++Index)
		{
			const FAngelscriptRuntimeJITHelperToken& Token = View.HelperTokens[Index];
			Writer.WriteUInt8(static_cast<uint8>(Token.Kind));
			Writer.WriteUInt32(Token.SlotIndex);
			Writer.WriteHash(Token.StableKey);
			Writer.WriteHash(Token.ExpectedAbi);
		}
		return Writer.FinalizeHash();
	}

	static bool DecodeControlFlow(
		const TArray<asDWORD>& Bytecode,
		TArray<uint32>& OutInstructionOffsets,
		TArray<FAngelscriptRuntimeJITControlFlowEdge>& OutEdges,
		uint32& OutFailureOffset,
		bool& bOutHasReferenceOperand,
		bool& bOutHasSuspend)
	{
		OutFailureOffset = MAX_uint32;
		bOutHasReferenceOperand = false;
		bOutHasSuspend = false;
		uint32 Offset = 0;
		while (Offset < static_cast<uint32>(Bytecode.Num()))
		{
			const uint8 RawOpcode = *reinterpret_cast<const uint8*>(&Bytecode[Offset]);
			const asEBCInstr Opcode = static_cast<asEBCInstr>(RawOpcode);
			if (Opcode >= asBC_MAXBYTECODE || asBCInfo[RawOpcode].bc != Opcode)
			{
				OutFailureOffset = Offset;
				return false;
			}
			const int32 Size = asBCTypeSize[asBCInfo[RawOpcode].type];
			if (Size <= 0 || Offset + static_cast<uint32>(Size)
				> static_cast<uint32>(Bytecode.Num()))
			{
				OutFailureOffset = Offset;
				return false;
			}
			OutInstructionOffsets.Add(Offset);
			if (IsReferenceBearingOpcode(Opcode)
				&& !IsFunctionReferenceOpcode(Opcode))
			{
				bOutHasReferenceOperand = true;
				if (OutFailureOffset == MAX_uint32)
				{
					OutFailureOffset = Offset;
				}
			}
			bOutHasSuspend |= Opcode == asBC_SUSPEND;
			Offset += static_cast<uint32>(Size);
		}

		TMap<uint32, uint32> InstructionByOffset;
		for (int32 Index = 0; Index < OutInstructionOffsets.Num(); ++Index)
		{
			InstructionByOffset.Add(OutInstructionOffsets[Index], Index);
		}
		for (int32 Index = 0; Index < OutInstructionOffsets.Num(); ++Index)
		{
			const uint32 InstructionOffset = OutInstructionOffsets[Index];
			const asEBCInstr Opcode = static_cast<asEBCInstr>(
				*reinterpret_cast<const uint8*>(&Bytecode[InstructionOffset]));
			const uint32 NextOffset = InstructionOffset + static_cast<uint32>(
				asBCTypeSize[asBCInfo[Opcode].type]);
			auto AddEdge = [&](const uint32 ToOffset,
				const EAngelscriptRuntimeJITControlFlowEdgeKind Kind)
			{
				const uint32* ToIndex = InstructionByOffset.Find(ToOffset);
				if (ToIndex == nullptr)
				{
					OutFailureOffset = InstructionOffset;
					return false;
				}
				FAngelscriptRuntimeJITControlFlowEdge& Edge = OutEdges.Emplace_GetRef();
				Edge.FromInstruction = static_cast<uint32>(Index);
				Edge.ToInstruction = *ToIndex;
				Edge.Kind = Kind;
				return true;
			};

			if (Opcode == asBC_JMPP)
			{
				OutFailureOffset = InstructionOffset;
				return false;
			}
			if (Opcode == asBC_JMP || IsConditionalBranch(Opcode))
			{
				const int64 Target = static_cast<int64>(InstructionOffset) + 2
					+ static_cast<int64>(asBC_INTARG(&Bytecode[InstructionOffset]));
				if (Target < 0 || Target >= Bytecode.Num()
					|| !AddEdge(static_cast<uint32>(Target),
						Opcode == asBC_JMP
							? EAngelscriptRuntimeJITControlFlowEdgeKind::Branch
							: EAngelscriptRuntimeJITControlFlowEdgeKind::ConditionalBranch))
				{
					OutFailureOffset = InstructionOffset;
					return false;
				}
				if (Opcode == asBC_JMP)
				{
					continue;
				}
			}
			if (Opcode != asBC_RET && NextOffset < static_cast<uint32>(Bytecode.Num())
				&& !AddEdge(NextOffset,
					EAngelscriptRuntimeJITControlFlowEdgeKind::Fallthrough))
			{
				return false;
			}
		}
		return true;
	}
}

FAngelscriptRuntimeJITCompileSnapshot
	FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
		FAngelscriptRuntimeJITCompileSnapshotHeader InHeader,
		TArray<asDWORD> InBytecode,
		TArray<FAngelscriptRuntimeJITFrameValue> InFrameValues,
		TArray<uint32> InInstructionWordOffsets,
		TArray<FAngelscriptRuntimeJITControlFlowEdge> InControlFlowEdges,
		TArray<FAngelscriptRuntimeJITHelperToken> InHelperTokens,
		FString InDiagnosticDeclaration,
		FString InDiagnosticSourceIdentity)
{
	FAngelscriptRuntimeJITCompileSnapshot Snapshot;
	Snapshot.Header = InHeader;
	Snapshot.Bytecode = MoveTemp(InBytecode);
	Snapshot.FrameValues = MoveTemp(InFrameValues);
	Snapshot.InstructionWordOffsets = MoveTemp(InInstructionWordOffsets);
	Snapshot.ControlFlowEdges = MoveTemp(InControlFlowEdges);
	Snapshot.HelperTokens = MoveTemp(InHelperTokens);
	Snapshot.DiagnosticDeclaration = MoveTemp(InDiagnosticDeclaration);
	Snapshot.DiagnosticSourceIdentity = MoveTemp(InDiagnosticSourceIdentity);
	return Snapshot;
}

FAngelscriptRuntimeJITCompileSnapshotView
	FAngelscriptRuntimeJITCompileSnapshot::GetView() const
{
	FAngelscriptRuntimeJITCompileSnapshotView View;
	View.Header = Header;
	View.Bytecode = Bytecode.GetData();
	View.BytecodeWordCount = static_cast<uint32>(Bytecode.Num());
	View.FrameValues = FrameValues.GetData();
	View.FrameValueCount = static_cast<uint32>(FrameValues.Num());
	View.InstructionWordOffsets = InstructionWordOffsets.GetData();
	View.InstructionCount = static_cast<uint32>(InstructionWordOffsets.Num());
	View.ControlFlowEdges = ControlFlowEdges.GetData();
	View.ControlFlowEdgeCount = static_cast<uint32>(ControlFlowEdges.Num());
	View.HelperTokens = HelperTokens.GetData();
	View.HelperTokenCount = static_cast<uint32>(HelperTokens.Num());
	return View;
}

FAngelscriptRuntimeJITContractValidationResult
	FAngelscriptRuntimeJITCompileSnapshot::Validate() const
{
	return FAngelscriptRuntimeJITContract::ValidateSnapshot(GetView());
}

FAngelscriptRuntimeJITSnapshotCaptureResult
	FAngelscriptRuntimeJITCompileSnapshot::Capture(
		const FAngelscriptRuntimeJITSnapshotCaptureInput& Input,
		FAngelscriptRuntimeJITCompileSnapshot& OutSnapshot)
{
	using namespace AngelscriptRuntimeJITSnapshot_Private;
	auto Failure = [](const EAngelscriptRuntimeJITSnapshotCaptureCode Code,
		const EAngelscriptRuntimeJITDiagnosticReason Reason,
		const uint32 Offset = MAX_uint32,
		const FAngelscriptRuntimeJITContractValidationResult Validation = {})
	{
		return FAngelscriptRuntimeJITSnapshotCaptureResult{
			Code, Reason, Offset, Validation};
	};
	if (Input.Function == nullptr || Input.Route == nullptr
		|| Input.Function != Input.Route->Function
		|| Input.Function->GetModule() == nullptr
		|| Input.Route->NumericFunctionId != Input.Function->GetId())
	{
		return Failure(EAngelscriptRuntimeJITSnapshotCaptureCode::InvalidInput,
			EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot);
	}
	if (!Input.Route->bHasVerifiedArtifactIdentity
		|| Input.Route->ModuleKey.Hash.IsZero()
		|| Input.Route->Identity.FunctionKey.Hash.IsZero()
		|| Input.Route->Identity.Content.Execution.IsZero())
	{
		return Failure(
			EAngelscriptRuntimeJITSnapshotCaptureCode::InvalidRouteIdentity,
			EAngelscriptRuntimeJITDiagnosticReason::StaleRevision);
	}

	asCScriptFunction* Function = static_cast<asCScriptFunction*>(Input.Function);
	if (Function->scriptData == nullptr
		|| Function->scriptData->byteCode.GetLength() == 0)
	{
		return Failure(EAngelscriptRuntimeJITSnapshotCaptureCode::InvalidInput,
			EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot);
	}

	FAngelscriptRuntimeJITCompileSnapshotHeader Header;
	Header.BackendId = Input.BackendId;
	Header.Platform = Input.Platform;
	Header.Configuration = Input.Configuration;
	Header.PointerWidth = static_cast<uint8>(sizeof(void*));
#if PLATFORM_LITTLE_ENDIAN
	Header.Endianness = EAngelscriptRuntimeJITEndianness::Little;
#else
	Header.Endianness = EAngelscriptRuntimeJITEndianness::Big;
#endif
	Header.CallingConvention =
		EAngelscriptRuntimeJITCallingConvention::AngelscriptVMEntry;
	Header.EntryAbiRevision = FAngelscriptRuntimeJITBackendAbi::EntryAbiRevision;
	Header.EngineNamespace = Input.EngineNamespace;
	Header.PublicationOrdinal = Input.PublicationOrdinal;
	Header.CancellationGeneration = Input.CancellationGeneration;
	Header.ModuleKey = Input.Route->ModuleKey;
	Header.FunctionKey = Input.Route->Identity.FunctionKey;
	Header.ParameterFrameByteSize = static_cast<uint32>(
		FMath::Max(Function->totalSpaceBeforeFunction, 0)
		* static_cast<int32>(sizeof(asDWORD)));
	Header.LocalFrameByteSize = Function->scriptData->variableSpace
		* static_cast<uint32>(sizeof(asDWORD));
	Header.ResultByteSize = static_cast<uint32>(FMath::Max(
		Function->returnType.GetSizeOnStackDWords(), 0)
		* static_cast<int32>(sizeof(asDWORD)));

	if (Function->objectType != nullptr)
	{
		Header.Invocation = EAngelscriptRuntimeJITInvocationKind::ScriptMethod;
		Header.Receiver = EAngelscriptRuntimeJITReceiverKind::NativeObjectSlot;
	}
	else if (Function->traits.GetTrait(asTRAIT_EXTERNAL_IMPLICIT_THIS))
	{
		Header.Invocation = EAngelscriptRuntimeJITInvocationKind::Global;
		Header.Receiver = EAngelscriptRuntimeJITReceiverKind::DeclaredParameterAlias;
		Header.ReceiverParameterIndex = 0;
	}
	else if (Function->IsMixin())
	{
		Header.Invocation = EAngelscriptRuntimeJITInvocationKind::Mixin;
		Header.Receiver = EAngelscriptRuntimeJITReceiverKind::DeclaredParameterAlias;
		Header.ReceiverParameterIndex = 0;
	}
	else
	{
		Header.Invocation = EAngelscriptRuntimeJITInvocationKind::Global;
		Header.Receiver = EAngelscriptRuntimeJITReceiverKind::None;
	}

	TArray<FAngelscriptRuntimeJITFrameValue> Values;
	bool bHasManagedValues = false;
	if (Function->returnType.GetTokenType() != ttVoid)
	{
		AddFrameValue(Values, Function->returnType,
			EAngelscriptRuntimeJITFrameValueFlags::Result,
			0, 0, bHasManagedValues);
	}
	const int32 ParameterCount = static_cast<int32>(
		Function->parameterTypes.GetLength());
	for (int32 Index = 0; Index < ParameterCount; ++Index)
	{
		EAngelscriptRuntimeJITFrameValueFlags Flags =
			EAngelscriptRuntimeJITFrameValueFlags::Parameter;
		if (Header.Receiver ==
				EAngelscriptRuntimeJITReceiverKind::DeclaredParameterAlias
			&& Header.ReceiverParameterIndex == Index)
		{
			Flags |= EAngelscriptRuntimeJITFrameValueFlags::ReceiverAlias;
		}
		AddFrameValue(Values, Function->parameterTypes[Index], Flags,
			static_cast<uint32>(FMath::Max(Function->parameterOffsets[Index], 0)
				* static_cast<int32>(sizeof(asDWORD))),
			static_cast<uint32>(Index), bHasManagedValues);
	}
	const int32 VariableCount = static_cast<int32>(
		Function->scriptData->variables.GetLength());
	for (int32 Index = 0; Index < VariableCount; ++Index)
	{
		const asSScriptVariable* Variable = Function->scriptData->variables[Index];
		if (Variable == nullptr)
		{
			return Failure(
				EAngelscriptRuntimeJITSnapshotCaptureCode::InvalidFrameLayout,
				EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot);
		}
		if (Variable->stackOffset <= 0)
		{
			continue;
		}
		if (Variable->stackOffset
				> static_cast<int32>(Function->scriptData->variableSpace))
		{
			return Failure(
				EAngelscriptRuntimeJITSnapshotCaptureCode::InvalidFrameLayout,
				EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot);
		}
		const uint32 ByteOffset =
			(static_cast<uint32>(Function->scriptData->variableSpace)
				- static_cast<uint32>(Variable->stackOffset))
			* static_cast<uint32>(sizeof(asDWORD));
		if (!AddCanonicalLocalFrameValue(Values, Variable->type, ByteOffset,
			static_cast<uint32>(Index), bHasManagedValues))
		{
			return Failure(
				EAngelscriptRuntimeJITSnapshotCaptureCode::InvalidFrameLayout,
				EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot);
		}
		bHasManagedValues |= Variable->onHeap;
	}
	const int32 TemporaryVariableCount = static_cast<int32>(
		Function->scriptData->temporaryVariables.GetLength());
	for (int32 Index = 0; Index < TemporaryVariableCount; ++Index)
	{
		const auto& Variable = Function->scriptData->temporaryVariables[Index];
		if (Variable.Offset <= 0 || Variable.Offset
			> static_cast<int32>(Function->scriptData->variableSpace))
		{
			continue;
		}
		const uint32 ByteOffset =
			(static_cast<uint32>(Function->scriptData->variableSpace)
				- static_cast<uint32>(Variable.Offset))
			* static_cast<uint32>(sizeof(asDWORD));
		const asCDataType Type = asCDataType::CreatePrimitive(
			Variable.Token, false);
		if (!AddCanonicalLocalFrameValue(Values, Type, ByteOffset,
			static_cast<uint32>(
				Function->scriptData->variables.GetLength() + Index),
			bHasManagedValues))
		{
			return Failure(
				EAngelscriptRuntimeJITSnapshotCaptureCode::InvalidFrameLayout,
				EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot);
		}
	}

	TArray<asDWORD> Bytecode;
	Bytecode.Append(Function->scriptData->byteCode.AddressOf(),
		Function->scriptData->byteCode.GetLength());
	TArray<uint32> InstructionOffsets;
	TArray<FAngelscriptRuntimeJITControlFlowEdge> Edges;
	uint32 FailureOffset = MAX_uint32;
	bool bHasReferenceOperand = false;
	bool bHasSuspend = false;
	if (!DecodeControlFlow(Bytecode, InstructionOffsets, Edges, FailureOffset,
		bHasReferenceOperand, bHasSuspend))
	{
		return Failure(
			EAngelscriptRuntimeJITSnapshotCaptureCode::MalformedBytecode,
			EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot,
			FailureOffset);
	}
	if (bHasReferenceOperand)
	{
		return Failure(
			EAngelscriptRuntimeJITSnapshotCaptureCode::UnsupportedReferenceOperand,
			EAngelscriptRuntimeJITDiagnosticReason::UnsupportedOpcode,
			FailureOffset);
	}
	FAngelscriptEngine* const RuntimeEngine =
		FAngelscriptEngine::TryGetByScriptEngine(Function->engine);
	if (RuntimeEngine == nullptr)
	{
		return Failure(EAngelscriptRuntimeJITSnapshotCaptureCode::InvalidInput,
			EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot);
	}
	TArray<FAngelscriptRuntimeJITHelperToken> HelperTokens;
	for (const uint32 InstructionOffset : InstructionOffsets)
	{
		asDWORD* const Instruction = &Bytecode[InstructionOffset];
		const asEBCInstr Opcode = static_cast<asEBCInstr>(
			*reinterpret_cast<const uint8*>(Instruction));
		if (!IsFunctionReferenceOpcode(Opcode))
		{
			continue;
		}
		asCScriptFunction* const Target = ResolveFunctionOperand(
			*Function->engine, Opcode, Instruction);
		FAngelscriptRuntimeJITHelperToken Token;
		if (Target == nullptr
			|| !TryBuildFunctionToken(*RuntimeEngine, Input.Route->ModuleKey,
				Function->module, *Target,
				static_cast<uint32>(HelperTokens.Num()), Token)
			|| !IsValidTokenKindForOpcode(Opcode, Token.Kind))
		{
			return Failure(
				EAngelscriptRuntimeJITSnapshotCaptureCode::UnsupportedReferenceOperand,
				EAngelscriptRuntimeJITDiagnosticReason::UnsupportedOpcode,
				InstructionOffset);
		}
		WriteFunctionTokenSlot(Instruction, Opcode, Token.SlotIndex);
		HelperTokens.Add(Token);
	}
	for (const uint32 InstructionOffset : InstructionOffsets)
	{
		const asEBCInstr Opcode = static_cast<asEBCInstr>(
			*reinterpret_cast<const uint8*>(&Bytecode[InstructionOffset]));
		if (Opcode == asBC_JitEntry)
		{
			// JitEntry is a VM resume marker. Runtime publication lives in the
			// function's separate JIT binding, so its legacy pointer payload is
			// never worker input and must be canonicalized out of the snapshot.
			asBC_PTRARG(&Bytecode[InstructionOffset]) = 0;
		}
	}
	Header.FunctionProfile = bHasSuspend
			&& HasBackwardControlFlowEdge(Edges)
		? EAngelscriptRuntimeJITFunctionProfile::SuspendOrLatent
		: Function->scriptData->tryCatchInfo.GetLength() != 0
			? EAngelscriptRuntimeJITFunctionProfile::ExceptionCleanup
			: bHasManagedValues
				? EAngelscriptRuntimeJITFunctionProfile::ManagedValues
				: EAngelscriptRuntimeJITFunctionProfile::ScalarWholeFunction;

	const FString Declaration = Input.Route->CanonicalDeclaration.IsEmpty()
		? UTF8_TO_TCHAR(Function->GetDeclaration(false, false, false))
		: Input.Route->CanonicalDeclaration;
	Header.EntryAbiHash = BuildEntryAbiHash(Header, Declaration, Values);
	FAngelscriptRuntimeJITCompileSnapshot Candidate = FromOwnedData(
		Header, MoveTemp(Bytecode), MoveTemp(Values),
		MoveTemp(InstructionOffsets), MoveTemp(Edges), MoveTemp(HelperTokens), Declaration,
		UTF8_TO_TCHAR(Function->GetModuleName()));
	Candidate.Header.FunctionRevision = BuildFunctionRevision(
		Candidate, Input.Route->Identity.Content.Execution);
	const FAngelscriptRuntimeJITContractValidationResult Validation =
		Candidate.Validate();
	if (!Validation.IsValid())
	{
		return Failure(
			EAngelscriptRuntimeJITSnapshotCaptureCode::InvalidFrameLayout,
			Validation.Code
				== EAngelscriptRuntimeJITContractValidationCode::InvalidEntryAbi
					? EAngelscriptRuntimeJITDiagnosticReason::EntryAbiMismatch
					: EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot,
			MAX_uint32, Validation);
	}
	OutSnapshot = MoveTemp(Candidate);
	return {EAngelscriptRuntimeJITSnapshotCaptureCode::Captured,
		EAngelscriptRuntimeJITDiagnosticReason::None, MAX_uint32, {}};
}

FAngelscriptRuntimeJITContractValidationResult
	FAngelscriptRuntimeJITCompileSnapshot::ValidateStructure(
		const FAngelscriptRuntimeJITCompileSnapshotView& Snapshot)
{
	using namespace AngelscriptRuntimeJITSnapshot_Private;
	if (Snapshot.Header.EntryAbiRevision
		!= FAngelscriptRuntimeJITBackendAbi::EntryAbiRevision)
	{
		return Invalid(EAngelscriptRuntimeJITContractValidationCode::InvalidEntryAbi);
	}
	const bool bHasReceiverAlias = Snapshot.Header.Receiver
		== EAngelscriptRuntimeJITReceiverKind::DeclaredParameterAlias;
	if (bHasReceiverAlias != (Snapshot.Header.ReceiverParameterIndex >= 0)
		|| (!bHasReceiverAlias
			&& Snapshot.Header.ReceiverParameterIndex != INDEX_NONE))
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidReceiverParameter);
	}
	if ((Snapshot.Header.Invocation == EAngelscriptRuntimeJITInvocationKind::Global
			&& Snapshot.Header.Receiver != EAngelscriptRuntimeJITReceiverKind::None
			&& Snapshot.Header.Receiver
				!= EAngelscriptRuntimeJITReceiverKind::DeclaredParameterAlias)
		|| (Snapshot.Header.Invocation
				== EAngelscriptRuntimeJITInvocationKind::ScriptMethod
			&& Snapshot.Header.Receiver
				!= EAngelscriptRuntimeJITReceiverKind::NativeObjectSlot)
		|| (Snapshot.Header.Invocation == EAngelscriptRuntimeJITInvocationKind::Mixin
			&& Snapshot.Header.Receiver
				!= EAngelscriptRuntimeJITReceiverKind::DeclaredParameterAlias)
		|| Snapshot.Header.Invocation
			== EAngelscriptRuntimeJITInvocationKind::ExternalImplicitThis)
	{
		return Invalid(EAngelscriptRuntimeJITContractValidationCode::InvalidReceiver);
	}

	TArray<TPair<uint32, uint32>> ParameterRanges;
	TArray<TPair<uint32, uint32>> LocalRanges;
	int32 ResultCount = 0;
	int32 AliasCount = 0;
	for (uint32 Index = 0; Index < Snapshot.FrameValueCount; ++Index)
	{
		const FAngelscriptRuntimeJITFrameValue& Value = Snapshot.FrameValues[Index];
		const bool bParameter = EnumHasAnyFlags(Value.Flags,
			EAngelscriptRuntimeJITFrameValueFlags::Parameter);
		const bool bResult = EnumHasAnyFlags(Value.Flags,
			EAngelscriptRuntimeJITFrameValueFlags::Result);
		const bool bLocal = EnumHasAnyFlags(Value.Flags,
			EAngelscriptRuntimeJITFrameValueFlags::Local);
		const bool bAlias = EnumHasAnyFlags(Value.Flags,
			EAngelscriptRuntimeJITFrameValueFlags::ReceiverAlias);
		if (Value.Kind == EAngelscriptRuntimeJITScalarKind::Invalid
			|| Value.Kind > EAngelscriptRuntimeJITScalarKind::OpaquePointer
			|| static_cast<int32>(bParameter) + static_cast<int32>(bResult)
				+ static_cast<int32>(bLocal) != 1
			|| Value.ByteSize == 0 || !IsPowerOfTwo(Value.Alignment)
			|| Value.ByteOffset % Value.Alignment != 0 || (bAlias && !bParameter))
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidFrameValue,
				static_cast<int32>(Index));
		}
		if (bResult)
		{
			++ResultCount;
			if (Value.ByteOffset != 0
				|| Value.ByteSize > Snapshot.Header.ResultByteSize)
			{
				return Invalid(
					EAngelscriptRuntimeJITContractValidationCode::InvalidFrameLayout,
					static_cast<int32>(Index));
			}
		}
		else
		{
			const uint32 FrameSize = bParameter
				? Snapshot.Header.ParameterFrameByteSize
				: Snapshot.Header.LocalFrameByteSize;
			if (Value.ByteOffset > FrameSize
				|| Value.ByteSize > FrameSize - Value.ByteOffset)
			{
				return Invalid(
					EAngelscriptRuntimeJITContractValidationCode::InvalidFrameLayout,
					static_cast<int32>(Index));
			}
			TArray<TPair<uint32, uint32>>& Ranges = bParameter
				? ParameterRanges : LocalRanges;
			for (const TPair<uint32, uint32>& Range : Ranges)
			{
				if (Value.ByteOffset < Range.Value
					&& Range.Key < Value.ByteOffset + Value.ByteSize)
				{
					return Invalid(
						EAngelscriptRuntimeJITContractValidationCode::InvalidFrameLayout,
						static_cast<int32>(Index));
				}
			}
			Ranges.Emplace(Value.ByteOffset, Value.ByteOffset + Value.ByteSize);
		}
		if (bAlias)
		{
			++AliasCount;
			if (static_cast<int32>(Value.SourceIndex)
				!= Snapshot.Header.ReceiverParameterIndex)
			{
				return Invalid(
					EAngelscriptRuntimeJITContractValidationCode::InvalidReceiverParameter,
					static_cast<int32>(Index));
			}
		}
	}
	if (ResultCount > 1 || (Snapshot.Header.ResultByteSize == 0) != (ResultCount == 0)
		|| (bHasReceiverAlias && AliasCount != 1) || (!bHasReceiverAlias && AliasCount != 0))
	{
		return Invalid(EAngelscriptRuntimeJITContractValidationCode::InvalidFrameLayout);
	}
	if (Snapshot.Header.FunctionProfile
		== EAngelscriptRuntimeJITFunctionProfile::ScalarWholeFunction)
	{
		for (uint32 Index = 0; Index < Snapshot.FrameValueCount; ++Index)
		{
			if (Snapshot.FrameValues[Index].Kind
				== EAngelscriptRuntimeJITScalarKind::OpaquePointer)
			{
				return Invalid(
					EAngelscriptRuntimeJITContractValidationCode::InvalidFrameLayout,
					static_cast<int32>(Index));
			}
		}
	}

	if (Snapshot.InstructionCount == 0
		|| Snapshot.InstructionWordOffsets[0] != 0)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidInstructionBoundary);
	}
	uint32 ExpectedOffset = 0;
	for (uint32 Index = 0; Index < Snapshot.InstructionCount; ++Index)
	{
		const uint32 Offset = Snapshot.InstructionWordOffsets[Index];
		if (Offset != ExpectedOffset || Offset >= Snapshot.BytecodeWordCount)
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidInstructionBoundary,
				static_cast<int32>(Index));
		}
		const uint8 Opcode = *reinterpret_cast<const uint8*>(&Snapshot.Bytecode[Offset]);
		if (Opcode >= asBC_MAXBYTECODE || asBCInfo[Opcode].bc != Opcode)
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidInstructionBoundary,
				static_cast<int32>(Index));
		}
		const int32 Size = asBCTypeSize[asBCInfo[Opcode].type];
		if (Size <= 0 || ExpectedOffset + static_cast<uint32>(Size)
			> Snapshot.BytecodeWordCount)
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidInstructionBoundary,
				static_cast<int32>(Index));
		}
		if (Opcode == asBC_JitEntry
			&& asBC_PTRARG(&Snapshot.Bytecode[Offset]) != 0)
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidHelperToken,
				static_cast<int32>(Index));
		}
		ExpectedOffset += static_cast<uint32>(Size);
	}
	if (ExpectedOffset != Snapshot.BytecodeWordCount)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidInstructionBoundary);
	}
	TArray<asDWORD> DecodedBytecode;
	DecodedBytecode.Append(Snapshot.Bytecode, Snapshot.BytecodeWordCount);
	TArray<uint32> DecodedOffsets;
	TArray<FAngelscriptRuntimeJITControlFlowEdge> DecodedEdges;
	uint32 DecodeFailureOffset = MAX_uint32;
	bool bHasReferenceOperand = false;
	bool bHasSuspend = false;
	if (!DecodeControlFlow(DecodedBytecode, DecodedOffsets, DecodedEdges,
		DecodeFailureOffset, bHasReferenceOperand, bHasSuspend))
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidControlFlowEdge,
			DecodeFailureOffset == MAX_uint32
				? INDEX_NONE : static_cast<int32>(DecodeFailureOffset));
	}
	if (bHasReferenceOperand)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidHelperToken);
	}
	if (bHasSuspend && HasBackwardControlFlowEdge(DecodedEdges)
		&& Snapshot.Header.FunctionProfile
		!= EAngelscriptRuntimeJITFunctionProfile::SuspendOrLatent)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidFunctionProfile);
	}
	if (DecodedOffsets.Num() != static_cast<int32>(Snapshot.InstructionCount)
		|| DecodedEdges.Num() != static_cast<int32>(Snapshot.ControlFlowEdgeCount))
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidControlFlowEdge);
	}
	for (int32 Index = 0; Index < DecodedOffsets.Num(); ++Index)
	{
		if (DecodedOffsets[Index] != Snapshot.InstructionWordOffsets[Index])
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidInstructionBoundary,
				Index);
		}
	}
	for (int32 Index = 0; Index < DecodedEdges.Num(); ++Index)
	{
		const FAngelscriptRuntimeJITControlFlowEdge& Expected = DecodedEdges[Index];
		const FAngelscriptRuntimeJITControlFlowEdge& Actual =
			Snapshot.ControlFlowEdges[Index];
		if (Expected.FromInstruction != Actual.FromInstruction
			|| Expected.ToInstruction != Actual.ToInstruction
			|| Expected.Kind != Actual.Kind)
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidControlFlowEdge,
				Index);
		}
	}
	for (uint32 Index = 0; Index < Snapshot.ControlFlowEdgeCount; ++Index)
	{
		const FAngelscriptRuntimeJITControlFlowEdge& Edge =
			Snapshot.ControlFlowEdges[Index];
		if (Edge.FromInstruction >= Snapshot.InstructionCount
			|| Edge.ToInstruction >= Snapshot.InstructionCount
			|| Edge.Kind > EAngelscriptRuntimeJITControlFlowEdgeKind::Switch)
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidControlFlowEdge,
				static_cast<int32>(Index));
		}
	}
	uint32 ExpectedTokenSlot = 0;
	for (uint32 InstructionIndex = 0;
		InstructionIndex < Snapshot.InstructionCount; ++InstructionIndex)
	{
		const asDWORD* const Instruction = &Snapshot.Bytecode[
			Snapshot.InstructionWordOffsets[InstructionIndex]];
		const asEBCInstr Opcode = static_cast<asEBCInstr>(
			*reinterpret_cast<const uint8*>(Instruction));
		if (!IsFunctionReferenceOpcode(Opcode))
		{
			continue;
		}
		if (ExpectedTokenSlot >= Snapshot.HelperTokenCount)
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidHelperToken,
				static_cast<int32>(InstructionIndex));
		}
		const FAngelscriptRuntimeJITHelperToken& Token =
			Snapshot.HelperTokens[ExpectedTokenSlot];
		if (ReadFunctionTokenSlot(Instruction, Opcode) != ExpectedTokenSlot
			|| Token.SlotIndex != ExpectedTokenSlot
			|| !IsValidTokenKindForOpcode(Opcode, Token.Kind))
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidHelperToken,
				static_cast<int32>(InstructionIndex));
		}
		++ExpectedTokenSlot;
	}
	if (ExpectedTokenSlot != Snapshot.HelperTokenCount)
	{
		return Invalid(
			EAngelscriptRuntimeJITContractValidationCode::InvalidHelperToken,
			static_cast<int32>(ExpectedTokenSlot));
	}
	for (uint32 Index = 0; Index < Snapshot.HelperTokenCount; ++Index)
	{
		const FAngelscriptRuntimeJITHelperToken& Token = Snapshot.HelperTokens[Index];
		if (Token.Kind == EAngelscriptRuntimeJITHelperTokenKind::Invalid
			|| Token.Kind > EAngelscriptRuntimeJITHelperTokenKind::StringLiteral
			|| Token.SlotIndex != Index || Token.StableKey.IsZero()
			|| Token.ExpectedAbi.IsZero())
		{
			return Invalid(
				EAngelscriptRuntimeJITContractValidationCode::InvalidHelperToken,
				static_cast<int32>(Index));
		}
	}
	return {};
}
