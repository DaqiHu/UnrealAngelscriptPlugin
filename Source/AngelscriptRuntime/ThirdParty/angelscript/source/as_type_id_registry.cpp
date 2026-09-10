#include "as_type_id_registry.h"

#include <limits>

BEGIN_AS_NAMESPACE

namespace
{
	bool WouldExceedInt(int Next, asUINT Count, int MaxInclusive)
	{
		if (Count == 0) return false;
		if (Next > MaxInclusive) return true;
		const int64 Remaining = static_cast<int64>(MaxInclusive) - static_cast<int64>(Next) + 1;
		return static_cast<int64>(Count) > Remaining;
	}

	bool WouldExceedQword(asQWORD Next, asUINT Count, asQWORD MaxInclusive)
	{
		if (Count == 0) return false;
		if (Next > MaxInclusive) return true;
		const asQWORD Remaining = MaxInclusive - Next + 1;
		return static_cast<asQWORD>(Count) > Remaining;
	}
}

asCTypeIdAllocator::asCTypeIdAllocator()
	: NextTypeSequence(asTYPEID_LAST_PRIMITIVE + 1)
	, MaxTypeSequence(asTYPEID_MASK_SEQNBR)
	, NextFunctionId(1)
	, MaxFunctionId(MAX_int32 - 1)
	, NextGeneration(1)
	, MaxGeneration(std::numeric_limits<asQWORD>::max())
{
}

asCTypeIdAllocator::asCTypeIdAllocator(
	int MaxTypeSequenceInclusive,
	int MaxFunctionIdInclusive,
	asQWORD MaxGenerationInclusive)
	: NextTypeSequence(asTYPEID_LAST_PRIMITIVE + 1)
	, MaxTypeSequence(MaxTypeSequenceInclusive)
	, NextFunctionId(1)
	, MaxFunctionId(MaxFunctionIdInclusive)
	, NextGeneration(1)
	, MaxGeneration(MaxGenerationInclusive)
{
}

asETypeIdReservationResult asCTypeIdAllocator::Reserve(
	const asSTypeIdReservationRequest& Request,
	asSTypeIdReservation& Out)
{
	std::lock_guard<std::mutex> Lock(Mutex);
	if (Request.TypeSequenceCount == 0 && Request.FunctionIdCount == 0 && Request.GenerationCount == 0)
	{
		return asTYPE_ID_RESERVE_INVALID_ARGUMENT;
	}
	if (WouldExceedInt(NextTypeSequence, Request.TypeSequenceCount, MaxTypeSequence)
		|| WouldExceedInt(NextFunctionId, Request.FunctionIdCount, MaxFunctionId)
		|| WouldExceedQword(NextGeneration, Request.GenerationCount, MaxGeneration))
	{
		return asTYPE_ID_RESERVE_EXHAUSTED;
	}

	Out.FirstTypeSequence = NextTypeSequence;
	Out.TypeSequenceCount = Request.TypeSequenceCount;
	Out.FirstFunctionId = NextFunctionId;
	Out.FunctionIdCount = Request.FunctionIdCount;
	Out.FirstGeneration = NextGeneration;
	Out.GenerationCount = Request.GenerationCount;
	NextTypeSequence += static_cast<int>(Request.TypeSequenceCount);
	NextFunctionId += static_cast<int>(Request.FunctionIdCount);
	NextGeneration += Request.GenerationCount;
	return asTYPE_ID_RESERVE_SUCCEEDED;
}

int asCTypeIdAllocator::PeekNextTypeSequence() const
{
	std::lock_guard<std::mutex> Lock(Mutex);
	return NextTypeSequence;
}

int asCTypeIdAllocator::PeekNextFunctionId() const
{
	std::lock_guard<std::mutex> Lock(Mutex);
	return NextFunctionId;
}

asQWORD asCTypeIdAllocator::PeekNextGeneration() const
{
	std::lock_guard<std::mutex> Lock(Mutex);
	return NextGeneration;
}

int asCTypeIdAllocator::EncodeObjectTypeId(int Sequence, asQWORD ObjectFlags)
{
	int Id = Sequence & asTYPEID_MASK_SEQNBR;
	if (ObjectFlags & asOBJ_SCRIPT_OBJECT) Id |= asTYPEID_SCRIPTOBJECT;
	else if (ObjectFlags & asOBJ_TEMPLATE) Id |= asTYPEID_TEMPLATE;
	else if (!(ObjectFlags & asOBJ_ENUM)) Id |= asTYPEID_APPOBJECT;
	return Id;
}

int asCTypeIdAllocator::TypeSequenceFromTypeId(int TypeId)
{
	return TypeId & asTYPEID_MASK_SEQNBR;
}

int asCTypeIdAllocator::ApplyTypeIdUseBits(int Sequence, asDWORD UseBits)
{
	return (Sequence & asTYPEID_MASK_SEQNBR) | (UseBits & ~asTYPEID_MASK_SEQNBR);
}

bool asCTypeIdAllocator::IsFixedPrimitiveTypeId(int TypeId)
{
	const int Sequence = TypeId & asTYPEID_MASK_SEQNBR;
	return Sequence >= asTYPEID_VOID
		&& Sequence <= asTYPEID_LAST_PRIMITIVE
		&& (TypeId & asTYPEID_MASK_OBJECT) == 0;
}

asCTypeIdRegistry& asCTypeIdRegistry::Get()
{
	static asCTypeIdRegistry Instance;
	return Instance;
}

asCTypeIdAllocator& asCTypeIdRegistry::Allocator()
{
	return ProcessAllocator;
}

const asCTypeIdAllocator& asCTypeIdRegistry::Allocator() const
{
	return ProcessAllocator;
}

asETypeIdReservationResult asCTypeIdRegistry::Reserve(
	const asSTypeIdReservationRequest& Request,
	asSTypeIdReservation& Out)
{
	return ProcessAllocator.Reserve(Request, Out);
}

END_AS_NAMESPACE
