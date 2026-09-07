#include "CQTest.h"
#include "as_bytecode_image_builder.h"
#include "as_bytecode_image_codec.h"
#include "as_bytecode_verifier.h"
#include "frontend/as_stable_key.h"
#include "Hash/Blake3.h"
#include "HAL/UnrealMemory.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMWireFormat, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asSStableKey WireKey(uint8 Marker)
	{
		uint8 Bytes[32] = {};
		Bytes[0] = Marker;
		asSStableKey Result;
		asSStableKey::TryCreate(MakeArrayView(Bytes, 32), Result);
		return Result;
	}

	static asSByteCodeOperand Imm16(int16 Value)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::ImmediateS16;
		Operand.Immediate = Value;
		return Operand;
	}

	static asSFingerprintDigest DigestOf(TConstArrayView<uint8> Bytes)
	{
		const FBlake3Hash Hash = FBlake3::HashBuffer(Bytes.GetData(), static_cast<uint64>(Bytes.Num()));
		asSFingerprintDigest Digest;
		asSFingerprintDigest::TryCreate(MakeArrayView(Hash.GetBytes(), asSFingerprintDigest::ByteCount), Digest);
		return Digest;
	}

	static TArray<uint8> EmptyGoldenV2()
	{
		return TArray<uint8>({
			'A', 'S', 'B', 'C',
			2, 0,
			static_cast<uint8>(asBC_MAXBYTECODE), 0,
			8,
			1,
			1,
			0,
			0,
			0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0
		});
	}

public:
	TEST_METHOD(EmptyImageMatchesIndependentGoldenAndRejectsV1)
	{
		asCByteCodeImage Empty;
		Empty.PointerByteSize = 8;
		Empty.bFloatIsFloat64 = true;
		TArray<uint8> Encoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(Empty, Encoded)));
		const TArray<uint8> Golden = EmptyGoldenV2();
		ASSERT_THAT(AreEqual(Golden.Num(), Encoded.Num()));
		ASSERT_THAT(IsTrue(FMemory::Memcmp(Golden.GetData(), Encoded.GetData(), Golden.Num()) == 0));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Golden, Decoded)));
		ASSERT_THAT(AreEqual(2u, Decoded->GetFormatVersion()));

		TArray<uint8> Version1 = Golden;
		Version1[4] = 1;
		Version1[5] = 0;
		TUniquePtr<asCByteCodeImage> Rejected;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::UnknownVersion, asCByteCodeImageCodec::Decode(Version1, Rejected)));
		ASSERT_THAT(IsNull(Rejected.Get()));
	}

	TEST_METHOD(StringsPreserveExactUtf8BoundsAndRejectTruncatedMultibyte)
	{
		asCByteCodeImage Image;
		Image.PointerByteSize = 8;
		Image.Strings.Add(FString());
		Image.Strings.Add(FString(TEXT("café")));
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(Image, Bytes)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		ASSERT_THAT(AreEqual(2, Decoded->Strings.Num()));
		ASSERT_THAT(AreEqual(0, Decoded->Strings[0].Len()));
		ASSERT_THAT(IsTrue(Decoded->Strings[1] == FString(TEXT("café"))));

		TArray<uint8> Embedded;
		Embedded.Append(EmptyGoldenV2().GetData(), 14);
		const uint8 ReqZero[4] = { 0, 0, 0, 0 };
		const uint8 OneString[4] = { 1, 0, 0, 0 };
		const uint8 ThreeBytes[4] = { 3, 0, 0, 0 };
		const uint8 FnZero[4] = { 0, 0, 0, 0 };
		Embedded.Append(ReqZero, 4);
		Embedded.Append(OneString, 4);
		Embedded.Append(ThreeBytes, 4);
		Embedded.Add(static_cast<uint8>('a'));
		Embedded.Add(0);
		Embedded.Add(static_cast<uint8>('b'));
		Embedded.Append(FnZero, 4);
		TUniquePtr<asCByteCodeImage> EmbeddedDecoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Embedded, EmbeddedDecoded)));
		ASSERT_THAT(AreEqual(1, EmbeddedDecoded->Strings.Num()));
		ASSERT_THAT(AreEqual(3, EmbeddedDecoded->Strings[0].Len()));
		ASSERT_THAT(AreEqual(static_cast<int32>('a'), static_cast<int32>(EmbeddedDecoded->Strings[0][0])));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(EmbeddedDecoded->Strings[0][1])));
		ASSERT_THAT(AreEqual(static_cast<int32>('b'), static_cast<int32>(EmbeddedDecoded->Strings[0][2])));

		TArray<uint8> TruncatedMb;
		TruncatedMb.Append(EmptyGoldenV2().GetData(), 14);
		const uint8 TwoBytes[4] = { 2, 0, 0, 0 };
		TruncatedMb.Append(ReqZero, 4);
		TruncatedMb.Append(OneString, 4);
		TruncatedMb.Append(TwoBytes, 4);
		TruncatedMb.Add(0xC3);
		TUniquePtr<asCByteCodeImage> BadUtf;
		ASSERT_THAT(AreNotEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(TruncatedMb, BadUtf)));
		ASSERT_THAT(IsNull(BadUtf.Get()));
	}

	TEST_METHOD(MalformedCountsTargetsAndTrailingBytesReject)
	{
		TArray<uint8> Truncated;
		Truncated.Append(EmptyGoldenV2().GetData(), 8);
		TUniquePtr<asCByteCodeImage> Out;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Truncated, asCByteCodeImageCodec::Decode(Truncated, Out)));

		TArray<uint8> BadPtr = EmptyGoldenV2();
		BadPtr[8] = 3;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::InvalidArgument, asCByteCodeImageCodec::Decode(BadPtr, Out)));

		TArray<uint8> Trailing = EmptyGoldenV2();
		Trailing.Add(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::InvalidArgument, asCByteCodeImageCodec::Decode(Trailing, Out)));

		TArray<uint8> Overflow = EmptyGoldenV2();
		Overflow[14] = 0xFF;
		Overflow[15] = 0xFF;
		Overflow[16] = 0xFF;
		Overflow[17] = 0x7F;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Overflow, asCByteCodeImageCodec::Decode(Overflow, Out)));
	}

	TEST_METHOD(CanonicalOrderRoundtripPreservesContractsFrameAndCleanup)
	{
		asCByteCodeImageBuilder First;
		asUINT StringA = 0;
		asUINT StringB = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, First.AddString(TEXT("beta"), StringB)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, First.AddString(TEXT("alpha"), StringA)));
		asSByteCodeRequirement Req;
		Req.Role = asEByteCodeRequirementRole::Type;
		Req.Key = WireKey(9);
		Req.Name = TEXT("T");
		const uint8 WitnessBytes[] = { 1, 2, 3, 4 };
		Req.SchemaWitness.Append(WitnessBytes, 4);
		Req.LayoutWitness.Append(WitnessBytes, 4);
		Req.Schema.Digest = DigestOf(Req.SchemaWitness);
		Req.Layout.Digest = DigestOf(Req.LayoutWitness);
		asUINT Slot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, First.AddRequirement(Req, Slot)));
		const asSStableKey Fn = WireKey(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, First.BeginFunction(Fn, Fn, 4, 0)));
		asSByteCodeFrameDescriptor Frame;
		Frame.LocalDwords = 4;
		Frame.ParameterDwords = 0;
		Frame.ResultDwords = 1;
		Frame.ResultOffset = 0;
		Frame.InitializedObjectSlots.Add(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, First.SetFrameDescriptor(Frame)));
		asSByteCodeCleanupAction Cleanup;
		Cleanup.Kind = asEByteCodeCleanupKind::ReleaseHandle;
		Cleanup.Path = asEByteCodeCleanupPath::Exception;
		Cleanup.FrameOffset = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, First.AddCleanupAction(Cleanup)));
		asSByteCodeSourceRecord Source;
		Source.InstructionIndex = 0;
		Source.Line = 7;
		Source.Column = 2;
		Source.FileStringSlot = StringA;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, First.AddSourceRecord(Source)));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, First.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, First.EndFunction()));
		TUniquePtr<asCByteCodeImage> ImageA;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, First.Finish(ImageA)));

		asCByteCodeImageBuilder Second;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Second.AddString(TEXT("alpha"), StringA)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Second.AddString(TEXT("beta"), StringB)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Second.AddRequirement(Req, Slot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Second.BeginFunction(Fn, Fn, 4, 0)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Second.SetFrameDescriptor(Frame)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Second.AddCleanupAction(Cleanup)));
		Source.FileStringSlot = StringA;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Second.AddSourceRecord(Source)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Second.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Second.EndFunction()));
		TUniquePtr<asCByteCodeImage> ImageB;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Second.Finish(ImageB)));

		TArray<uint8> BytesA;
		TArray<uint8> BytesB;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*ImageA, BytesA)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*ImageB, BytesB)));
		ASSERT_THAT(AreEqual(BytesA.Num(), BytesB.Num()));
		ASSERT_THAT(IsTrue(FMemory::Memcmp(BytesA.GetData(), BytesB.GetData(), BytesA.Num()) == 0));

		TUniquePtr<asCByteCodeImage> Round;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(BytesA, Round)));
		ASSERT_THAT(AreEqual(1, Round->Requirements.Num()));
		ASSERT_THAT(AreEqual(4, Round->Requirements[0].SchemaWitness.Num()));
		ASSERT_THAT(AreEqual(1, Round->Functions[0].Cleanup.Num()));
		ASSERT_THAT(AreEqual(7, Round->Functions[0].Source[0].Line));
		ASSERT_THAT(AreEqual(1u, Round->Functions[0].Frame.ResultDwords));
		ASSERT_THAT(AreEqual(TEXT("alpha"), Round->Strings[Round->Functions[0].Source[0].FileStringSlot]));

		TArray<uint8> Tampered = BytesA;
		for (int32 Index = 0; Index < Tampered.Num() - 4; ++Index)
		{
			if (Tampered[Index] == 1 && Tampered[Index + 1] == 2 && Tampered[Index + 2] == 3 && Tampered[Index + 3] == 4)
			{
				Tampered[Index] = 9;
				break;
			}
		}
		TUniquePtr<asCByteCodeImage> TamperedImage;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::CorruptWitness, asCByteCodeImageCodec::Decode(Tampered, TamperedImage)));
		ASSERT_THAT(IsNull(TamperedImage.Get()));

		FString Dump;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Dump(*Round, Dump)));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("role="))));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("T"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("0x"))));
	}
};

#endif
