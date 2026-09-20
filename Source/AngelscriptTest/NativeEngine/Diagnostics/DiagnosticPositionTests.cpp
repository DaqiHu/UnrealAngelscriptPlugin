#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"

#include "frontend/Basic/as_source_position_codec.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	DiagnosticsPosition,
	"Angelscript.UnitTest.NativeEngine.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(UnicodeAndCRLF)
	{
		const uint8 Bytes[] = {
			0x41,
			0xE4, 0xB8, 0xAD,
			0xF0, 0x9F, 0x98, 0x80,
			0x0D, 0x0A,
			0x5A};
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager =
			AngelscriptNativeEngineTest::MakeSourceManagerFromBytes(
				TEXT("A.as"),
				MakeArrayView(Bytes),
				&FileID);
		const AS_NAMESPACE_QUALIFIER asCSourcePositionCodec Codec(Manager);

		auto ExpectUtf16 = [&](asUINT ByteOffset, int32 ExpectedLine, int32 ExpectedColumn)
		{
			int32 Line = 77;
			int32 Column = 88;
			ASSERT_THAT(IsTrue(Codec.ByteToPosition(
				Manager.MakeLocation(FileID, ByteOffset),
				true,
				Line,
				Column)));
			ASSERT_THAT(AreEqual(ExpectedLine, Line));
			ASSERT_THAT(AreEqual(ExpectedColumn, Column));

			AS_NAMESPACE_QUALIFIER asCSourceLocation Out;
			Out.ByteOffset = 123;
			ASSERT_THAT(IsTrue(Codec.PositionToByte(FileID, ExpectedLine, ExpectedColumn, true, Out)));
			ASSERT_THAT(AreEqual(ByteOffset, Out.ByteOffset));
			ASSERT_THAT(AreEqual(Manager.GetSnapshotID(), Out.SnapshotID));
			ASSERT_THAT(IsTrue(Out.FileID == FileID));
		};

		ExpectUtf16(8, 0, 4);
		ExpectUtf16(10, 1, 0);
		ExpectUtf16(11, 1, 1);

		int32 Line = 77;
		int32 Column = 88;
		ASSERT_THAT(IsFalse(Codec.ByteToPosition(Manager.MakeLocation(FileID, 9), true, Line, Column)));
		ASSERT_THAT(AreEqual(77, Line));
		ASSERT_THAT(AreEqual(88, Column));
		ASSERT_THAT(IsFalse(Codec.ByteToPosition(Manager.MakeLocation(FileID, 2), true, Line, Column)));
		ASSERT_THAT(AreEqual(77, Line));
		ASSERT_THAT(AreEqual(88, Column));

		AS_NAMESPACE_QUALIFIER asCSourceLocation Rejected;
		Rejected.ByteOffset = 123;
		Rejected.SnapshotID = 7;
		ASSERT_THAT(IsFalse(Codec.PositionToByte(FileID, 0, 3, true, Rejected)));
		ASSERT_THAT(AreEqual(123u, Rejected.ByteOffset));
		ASSERT_THAT(AreEqual(uint64(7), Rejected.SnapshotID));
	}

	TEST_METHOD(MalformedNativeSource)
	{
		const uint8 Bytes[] = {0xC0, 0xAF};
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager =
			AngelscriptNativeEngineTest::MakeSourceManagerFromBytes(
				TEXT("Bad.as"),
				MakeArrayView(Bytes),
				&FileID);
		TConstArrayView<uint8> Native;
		ASSERT_THAT(IsTrue(Manager.TryGetBytes(FileID, Native)));
		ASSERT_THAT(AreEqual(2, Native.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0xC0), Native[0]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0xAF), Native[1]));

		const AS_NAMESPACE_QUALIFIER asCSourcePositionCodec Codec(Manager);
		for (asUINT Offset = 0; Offset <= 2; ++Offset)
		{
			int32 Line = 11;
			int32 Column = 22;
			ASSERT_THAT(IsFalse(Codec.ByteToPosition(
				Manager.MakeLocation(FileID, Offset),
				true,
				Line,
				Column)));
			ASSERT_THAT(AreEqual(11, Line));
			ASSERT_THAT(AreEqual(22, Column));
			ASSERT_THAT(IsFalse(Codec.ByteToPosition(
				Manager.MakeLocation(FileID, Offset),
				false,
				Line,
				Column)));
			ASSERT_THAT(AreEqual(11, Line));
			ASSERT_THAT(AreEqual(22, Column));
		}

		AS_NAMESPACE_QUALIFIER asCSourceLocation Out;
		Out.ByteOffset = 55;
		ASSERT_THAT(IsFalse(Codec.PositionToByte(FileID, 0, 0, true, Out)));
		ASSERT_THAT(AreEqual(55u, Out.ByteOffset));
		ASSERT_THAT(IsFalse(Codec.PositionToByte(FileID, 0, 0, false, Out)));
		ASSERT_THAT(AreEqual(55u, Out.ByteOffset));
	}
};

#endif
