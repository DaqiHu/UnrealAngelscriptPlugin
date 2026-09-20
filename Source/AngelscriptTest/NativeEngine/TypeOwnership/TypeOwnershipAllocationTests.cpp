#include "CQTest.h"
#include "TypeOwnershipTestSupport.h"
#include "as_type_id_registry.h"
#include "Misc/ScopeExit.h"

#include <atomic>
#include <thread>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace TypeOwnershipAllocationPrivate
{
	using namespace TypeOwnershipTest;

	bool RangesOverlap(int FirstA, asUINT CountA, int FirstB, asUINT CountB)
	{
		if (CountA == 0 || CountB == 0) return false;
		const int EndA = FirstA + static_cast<int>(CountA);
		const int EndB = FirstB + static_cast<int>(CountB);
		return FirstA < EndB && FirstB < EndA;
	}

	bool QwordRangesOverlap(asQWORD FirstA, asUINT CountA, asQWORD FirstB, asUINT CountB)
	{
		if (CountA == 0 || CountB == 0) return false;
		const asQWORD EndA = FirstA + CountA;
		const asQWORD EndB = FirstB + CountB;
		return FirstA < EndB && FirstB < EndA;
	}

	struct FWorkerBundles
	{
		TArray<asSTypeIdReservation> Ok;
		std::atomic<int32> Failures{0};
	};
}

TEST_CLASS_WITH_FLAGS(Allocation, "Angelscript.UnitTest.NativeEngine.TypeOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(Parallel)
	{
		using namespace TypeOwnershipAllocationPrivate;
		asCTypeIdAllocator Allocator;
		constexpr int32 WorkerCount = 8;
		constexpr int32 BundlesPerWorker = 128;
		FTestBarrier Barrier(WorkerCount);
		TArray<FWorkerBundles> Workers;
		Workers.SetNum(WorkerCount);
		TArray<TUniquePtr<std::thread>> Threads;
		ON_SCOPE_EXIT
		{
			for (TUniquePtr<std::thread>& Thread : Threads)
			{
				if (Thread && Thread->joinable()) Thread->join();
			}
		};
		for (int32 Index = 0; Index < WorkerCount; ++Index)
		{
			Threads.Add(MakeUnique<std::thread>([&, Index]()
			{
				Barrier.ArriveAndWait();
				for (int32 Bundle = 0; Bundle < BundlesPerWorker; ++Bundle)
				{
					asSTypeIdReservationRequest Request;
					Request.TypeSequenceCount = 2;
					Request.FunctionIdCount = 3;
					Request.GenerationCount = 1;
					asSTypeIdReservation Reservation;
					if (Allocator.Reserve(Request, Reservation) != asTYPE_ID_RESERVE_SUCCEEDED)
					{
						Workers[Index].Failures.fetch_add(1);
						return;
					}
					Workers[Index].Ok.Add(Reservation);
				}
			}));
		}
		for (TUniquePtr<std::thread>& Thread : Threads)
		{
			Thread->join();
		}
		Threads.Reset();
		TSet<int> Types;
		TSet<int> Functions;
		TSet<asQWORD> Generations;
		TArray<asSTypeIdReservation> All;
		for (int32 Index = 0; Index < WorkerCount; ++Index)
		{
			ASSERT_THAT(AreEqual(0, Workers[Index].Failures.load()));
			ASSERT_THAT(AreEqual(BundlesPerWorker, Workers[Index].Ok.Num()));
			All.Append(Workers[Index].Ok);
		}
		for (int32 Index = 0; Index < All.Num(); ++Index)
		{
			const asSTypeIdReservation& Item = All[Index];
			ASSERT_THAT(AreEqual(2, static_cast<int32>(Item.TypeSequenceCount)));
			ASSERT_THAT(AreEqual(3, static_cast<int32>(Item.FunctionIdCount)));
			ASSERT_THAT(AreEqual(1, static_cast<int32>(Item.GenerationCount)));
			for (asUINT Offset = 0; Offset < Item.TypeSequenceCount; ++Offset)
			{
				const int Value = Item.FirstTypeSequence + static_cast<int>(Offset);
				ASSERT_THAT(IsFalse(Types.Contains(Value)));
				Types.Add(Value);
			}
			for (asUINT Offset = 0; Offset < Item.FunctionIdCount; ++Offset)
			{
				const int Value = Item.FirstFunctionId + static_cast<int>(Offset);
				ASSERT_THAT(IsFalse(Functions.Contains(Value)));
				Functions.Add(Value);
			}
			for (asUINT Offset = 0; Offset < Item.GenerationCount; ++Offset)
			{
				const asQWORD Value = Item.FirstGeneration + Offset;
				ASSERT_THAT(IsFalse(Generations.Contains(Value)));
				Generations.Add(Value);
			}
			for (int32 Other = Index + 1; Other < All.Num(); ++Other)
			{
				ASSERT_THAT(IsFalse(RangesOverlap(
					Item.FirstTypeSequence, Item.TypeSequenceCount,
					All[Other].FirstTypeSequence, All[Other].TypeSequenceCount)));
				ASSERT_THAT(IsFalse(RangesOverlap(
					Item.FirstFunctionId, Item.FunctionIdCount,
					All[Other].FirstFunctionId, All[Other].FunctionIdCount)));
				ASSERT_THAT(IsFalse(QwordRangesOverlap(
					Item.FirstGeneration, Item.GenerationCount,
					All[Other].FirstGeneration, All[Other].GenerationCount)));
			}
		}
		ASSERT_THAT(AreEqual(2048, Types.Num()));
		ASSERT_THAT(AreEqual(3072, Functions.Num()));
		ASSERT_THAT(AreEqual(1024, Generations.Num()));
	}

	TEST_METHOD(LastBundle)
	{
		using namespace TypeOwnershipAllocationPrivate;
		struct FLimitCase
		{
			int MaxType = 0;
			int MaxFunction = 0;
			asQWORD MaxGeneration = 0;
		};
		const FLimitCase Cases[] = {
			{asTYPEID_LAST_PRIMITIVE + 2, MAX_int32 - 1, 1000},
			{asTYPEID_MASK_SEQNBR, 3, 1000},
			{asTYPEID_MASK_SEQNBR, MAX_int32 - 1, 1},
		};
		for (const FLimitCase& Case : Cases)
		{
			asCTypeIdAllocator Allocator(Case.MaxType, Case.MaxFunction, Case.MaxGeneration);
			std::atomic<int32> Successes{0};
			std::atomic<int32> Failures{0};
			FTestBarrier Barrier(2);
			TArray<TUniquePtr<std::thread>> Threads;
			ON_SCOPE_EXIT
			{
				for (TUniquePtr<std::thread>& Thread : Threads)
				{
					if (Thread && Thread->joinable()) Thread->join();
				}
			};
			for (int32 Index = 0; Index < 2; ++Index)
			{
				Threads.Add(MakeUnique<std::thread>([&]()
				{
					Barrier.ArriveAndWait();
					asSTypeIdReservationRequest Request;
					Request.TypeSequenceCount = 2;
					Request.FunctionIdCount = 3;
					Request.GenerationCount = 1;
					asSTypeIdReservation Reservation;
					if (Allocator.Reserve(Request, Reservation) == asTYPE_ID_RESERVE_SUCCEEDED)
					{
						Successes.fetch_add(1);
					}
					else
					{
						Failures.fetch_add(1);
					}
				}));
			}
			for (TUniquePtr<std::thread>& Thread : Threads)
			{
				Thread->join();
			}
			Threads.Reset();
			ASSERT_THAT(AreEqual(1, Successes.load()));
			ASSERT_THAT(AreEqual(1, Failures.load()));
			asSTypeIdReservationRequest Later;
			Later.TypeSequenceCount = 2;
			Later.FunctionIdCount = 3;
			Later.GenerationCount = 1;
			asSTypeIdReservation Unused;
			ASSERT_THAT(AreEqual(asTYPE_ID_RESERVE_EXHAUSTED, Allocator.Reserve(Later, Unused)));
			ASSERT_THAT(IsTrue(Allocator.PeekNextTypeSequence() <= asTYPEID_MASK_SEQNBR + 1));
			ASSERT_THAT(IsTrue(Allocator.PeekNextFunctionId() <= MAX_int32));
			ASSERT_THAT(IsTrue(Allocator.PeekNextGeneration() >= 1));
		}
	}

	TEST_METHOD(Encoding)
	{
		for (int Primitive = asTYPEID_VOID; Primitive <= asTYPEID_LAST_PRIMITIVE; ++Primitive)
		{
			ASSERT_THAT(IsTrue(asCTypeIdAllocator::IsFixedPrimitiveTypeId(Primitive)));
		}
		const int Sequence = asTYPEID_LAST_PRIMITIVE + 9;
		const int Script = asCTypeIdAllocator::EncodeObjectTypeId(Sequence, asOBJ_SCRIPT_OBJECT);
		const int App = asCTypeIdAllocator::EncodeObjectTypeId(Sequence, asOBJ_APP_CLASS);
		ASSERT_THAT(AreEqual(Sequence, asCTypeIdAllocator::TypeSequenceFromTypeId(Script)));
		ASSERT_THAT(AreEqual(Sequence, asCTypeIdAllocator::TypeSequenceFromTypeId(App)));
		ASSERT_THAT(IsTrue(Script != App));
		ASSERT_THAT(IsTrue((Script & asTYPEID_SCRIPTOBJECT) != 0));
		ASSERT_THAT(IsTrue((App & asTYPEID_APPOBJECT) != 0));
		const int Handle = asCTypeIdAllocator::ApplyTypeIdUseBits(Sequence, asTYPEID_OBJHANDLE);
		ASSERT_THAT(AreEqual(Sequence, asCTypeIdAllocator::TypeSequenceFromTypeId(Handle)));
		ASSERT_THAT(IsTrue((Handle & asTYPEID_OBJHANDLE) != 0));
		ASSERT_THAT(IsFalse(asCTypeIdAllocator::IsFixedPrimitiveTypeId(Script)));

		asCTypeIdAllocator Allocator;
		const int TypeBefore = Allocator.PeekNextTypeSequence();
		const int FunctionBefore = Allocator.PeekNextFunctionId();
		const asQWORD GenerationBefore = Allocator.PeekNextGeneration();
		asSTypeIdReservation EmptyOut;
		asSTypeIdReservationRequest Empty;
		ASSERT_THAT(AreEqual(asTYPE_ID_RESERVE_INVALID_ARGUMENT, Allocator.Reserve(Empty, EmptyOut)));
		ASSERT_THAT(AreEqual(TypeBefore, Allocator.PeekNextTypeSequence()));
		ASSERT_THAT(AreEqual(FunctionBefore, Allocator.PeekNextFunctionId()));
		ASSERT_THAT(IsTrue(Allocator.PeekNextGeneration() == GenerationBefore));

		asSTypeIdReservationRequest UnusedType;
		UnusedType.FunctionIdCount = 1;
		asSTypeIdReservation FunctionOnly;
		ASSERT_THAT(AreEqual(asTYPE_ID_RESERVE_SUCCEEDED, Allocator.Reserve(UnusedType, FunctionOnly)));
		ASSERT_THAT(AreEqual(TypeBefore, Allocator.PeekNextTypeSequence()));
		ASSERT_THAT(AreEqual(FunctionBefore, FunctionOnly.FirstFunctionId));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(FunctionOnly.FunctionIdCount)));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(FunctionOnly.TypeSequenceCount)));

		asSTypeIdReservationRequest Overlarge;
		Overlarge.TypeSequenceCount = asTYPEID_MASK_SEQNBR;
		asSTypeIdReservation OverlargeOut;
		const int TypeAfterFunction = Allocator.PeekNextTypeSequence();
		ASSERT_THAT(AreEqual(asTYPE_ID_RESERVE_EXHAUSTED, Allocator.Reserve(Overlarge, OverlargeOut)));
		ASSERT_THAT(AreEqual(TypeAfterFunction, Allocator.PeekNextTypeSequence()));

		asSTypeIdReservationRequest Overflow;
		Overflow.FunctionIdCount = static_cast<asUINT>(MAX_int32);
		asSTypeIdReservation OverflowOut;
		const int FunctionAfter = Allocator.PeekNextFunctionId();
		ASSERT_THAT(AreEqual(asTYPE_ID_RESERVE_EXHAUSTED, Allocator.Reserve(Overflow, OverflowOut)));
		ASSERT_THAT(AreEqual(FunctionAfter, Allocator.PeekNextFunctionId()));

		asSRuntimeTypeGeneration Generation = asReservedRuntimeTypeGeneration(7);
		ASSERT_THAT(IsTrue(Generation.IsValid()));
		asSRuntimeTypeId Typed = asReservedRuntimeTypeId(Script, 7);
		ASSERT_THAT(IsTrue(Typed.IsValid()));
		ASSERT_THAT(AreEqual(Script, Typed.Value));
	}

	TEST_METHOD(Service)
	{
		asCTypeIdRegistry* First = &asCTypeIdRegistry::Get();
		asCTypeIdRegistry* Second = &asCTypeIdRegistry::Get();
		ASSERT_THAT(IsTrue(First == Second));
		asSTypeIdReservationRequest TypeRequest;
		TypeRequest.TypeSequenceCount = 1;
		asSTypeIdReservation FirstType;
		ASSERT_THAT(AreEqual(asTYPE_ID_RESERVE_SUCCEEDED, First->Reserve(TypeRequest, FirstType)));
		{
			FNativeEngineOwner Engine;
			Engine.Reset();
		}
		asSTypeIdReservation SecondType;
		ASSERT_THAT(AreEqual(asTYPE_ID_RESERVE_SUCCEEDED, First->Reserve(TypeRequest, SecondType)));
		ASSERT_THAT(IsTrue(SecondType.FirstTypeSequence > FirstType.FirstTypeSequence));
		const int TypeNext = First->Allocator().PeekNextTypeSequence();
		asSTypeIdReservationRequest FunctionRequest;
		FunctionRequest.FunctionIdCount = 2;
		asSTypeIdReservation Functions;
		ASSERT_THAT(AreEqual(asTYPE_ID_RESERVE_SUCCEEDED, First->Reserve(FunctionRequest, Functions)));
		ASSERT_THAT(AreEqual(TypeNext, First->Allocator().PeekNextTypeSequence()));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Functions.FunctionIdCount)));
		ASSERT_THAT(IsTrue(Functions.FirstFunctionId > 0));
		ASSERT_THAT(IsTrue(Functions.FirstFunctionId != SecondType.FirstTypeSequence));
	}
};

#endif
