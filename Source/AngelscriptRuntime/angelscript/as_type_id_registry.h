#ifndef AS_TYPE_ID_REGISTRY_H
#define AS_TYPE_ID_REGISTRY_H

#include "angelscript.h"
#include "as_runtime_type_binding.h"

#include <mutex>

BEGIN_AS_NAMESPACE

enum asETypeIdReservationResult : asBYTE
{
	asTYPE_ID_RESERVE_SUCCEEDED = 0,
	asTYPE_ID_RESERVE_INVALID_ARGUMENT,
	asTYPE_ID_RESERVE_EXHAUSTED
};

enum asETypeQueryResult : asBYTE
{
	asTYPE_QUERY_SUCCEEDED = 0,
	asTYPE_QUERY_NOT_FOUND,
	asTYPE_QUERY_INVALID_ARGUMENT
};

struct asSTypeIdReservationRequest
{
	asUINT TypeSequenceCount = 0;
	asUINT FunctionIdCount = 0;
	asUINT GenerationCount = 0;
};

struct asSTypeIdReservation
{
	int FirstTypeSequence = 0;
	asUINT TypeSequenceCount = 0;
	int FirstFunctionId = 0;
	asUINT FunctionIdCount = 0;
	asQWORD FirstGeneration = 0;
	asUINT GenerationCount = 0;
};

class ANGELSCRIPTRUNTIME_API asCTypeIdAllocator
{
public:
	asCTypeIdAllocator();
	asCTypeIdAllocator(int MaxTypeSequenceInclusive, int MaxFunctionIdInclusive, asQWORD MaxGenerationInclusive);

	asETypeIdReservationResult Reserve(const asSTypeIdReservationRequest& Request, asSTypeIdReservation& Out);

	int PeekNextTypeSequence() const;
	int PeekNextFunctionId() const;
	asQWORD PeekNextGeneration() const;

	static int EncodeObjectTypeId(int Sequence, asQWORD ObjectFlags);
	static int TypeSequenceFromTypeId(int TypeId);
	static int ApplyTypeIdUseBits(int Sequence, asDWORD UseBits);
	static bool IsFixedPrimitiveTypeId(int TypeId);

private:
	mutable std::mutex Mutex;
	int NextTypeSequence = 0;
	int MaxTypeSequence = 0;
	int NextFunctionId = 0;
	int MaxFunctionId = 0;
	asQWORD NextGeneration = 0;
	asQWORD MaxGeneration = 0;
};

class ANGELSCRIPTRUNTIME_API asCTypeIdRegistry
{
public:
	static asCTypeIdRegistry& Get();
	asCTypeIdAllocator& Allocator();
	const asCTypeIdAllocator& Allocator() const;

	asETypeIdReservationResult Reserve(const asSTypeIdReservationRequest& Request, asSTypeIdReservation& Out);

private:
	asCTypeIdRegistry() = default;
	asCTypeIdAllocator ProcessAllocator;
};

inline asSRuntimeTypeGeneration asReservedRuntimeTypeGeneration(asQWORD Value)
{
	asSRuntimeTypeGeneration Generation;
	Generation.Value = Value;
	return Generation;
}

inline asSRuntimeTypeId asReservedRuntimeTypeId(int TypeId, asQWORD GenerationValue)
{
	asSRuntimeTypeId Id;
	Id.Value = TypeId;
	Id.Generation = asReservedRuntimeTypeGeneration(GenerationValue);
	return Id;
}

END_AS_NAMESPACE

#endif
