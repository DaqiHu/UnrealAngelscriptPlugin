/* Maintained Unreal AngelScript fork — symbolic bytecode image builder. */
#pragma once

#include "as_bytecode_image.h"
#include "as_typeinfo.h"
#include "as_objecttype.h"
#include "as_property.h"

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Templates/UniquePtr.h"

BEGIN_AS_NAMESPACE

class asCScriptFunction;

class ANGELSCRIPTRUNTIME_API asCByteCodeImageBuilder
{
public:
	asCByteCodeImageBuilder();

	asEByteCodeImageStatus AddRequirement(const asSByteCodeRequirement& Requirement, asUINT& OutSlot);
	asEByteCodeImageStatus AddTypeRequirement(const asCTypeInfo& Type, asUINT& OutSlot);
	asEByteCodeImageStatus AddFunctionRequirement(const asCScriptFunction& Function, asUINT& OutSlot);
	asEByteCodeImageStatus AddPropertyRequirement(const asCObjectType& Owner, const asCObjectProperty& Property, asUINT& OutSlot);
	asEByteCodeImageStatus AddString(const FString& Value, asUINT& OutSlot);

	asUINT NewLabel();
	asEByteCodeImageStatus BeginFunction(const asSStableKey& FunctionKey, const asSStableKey& SignatureKey, asUINT FrameSize, asUINT ParameterSize);
	asEByteCodeImageStatus SetFrameDescriptor(const asSByteCodeFrameDescriptor& Frame);
	asEByteCodeImageStatus AddCleanupAction(const asSByteCodeCleanupAction& Action);
	asEByteCodeImageStatus AddSourceRecord(const asSByteCodeSourceRecord& Record);
	asEByteCodeImageStatus BindLabel(asUINT Label);
	asEByteCodeImageStatus Emit(asEBCInstr Opcode, TConstArrayView<asSByteCodeOperand> Operands);
	asEByteCodeImageStatus EndFunction();
	asEByteCodeImageStatus Finish(TUniquePtr<asCByteCodeImage>& OutImage);
	asUINT GetInstructionCount() const;

private:
	struct asSDraftFunction
	{
		asSByteCodeFunctionBody Body;
		TMap<asUINT, asUINT> LabelToInstruction;
		TSet<asUINT> UnboundLabels;
		bool bOpen = false;
	};

	TArray<asSByteCodeRequirement> Requirements;
	TArray<asSDraftFunction> Functions;
	TArray<FString> Strings;
	asSDraftFunction* Current = nullptr;
	asUINT NextLabel = 1;
};

END_AS_NAMESPACE
