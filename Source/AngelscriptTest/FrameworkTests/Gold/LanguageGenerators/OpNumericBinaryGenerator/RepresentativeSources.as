int ObserveNumericType(int Value)
{
	return 101;
}

int ObserveNumericType(uint Value)
{
	return 102;
}

int ObserveNumericType(int64 Value)
{
	return 103;
}

int ObserveNumericType(uint64 Value)
{
	return 104;
}

int ObserveNumericType(float32 Value)
{
	return 105;
}

int ObserveNumericType(float64 Value)
{
	return 106;
}

int ObserveNumericType(bool Value)
{
	return 107;
}

int EntryLangOpNumericBinaryIntAddIntZero()
{
	int Left = int(0);
	int Right = int(1);
	return ObserveNumericType(Left + Right);
}

int EntryLangOpNumericBinaryInt8SubtractInt8One()
{
	int8 Left = int8(1);
	int8 Right = int8(1);
	return ObserveNumericType(Left - Right);
}

int EntryLangOpNumericBinaryUintMultiplyUint8Negative()
{
	uint Left = uint(4294967293);
	uint8 Right = uint8(2);
	return ObserveNumericType(Left * Right);
}

int EntryLangOpNumericBinaryInt64DivideIntNearMax()
{
	int64 Left = int64(9223372036854775806);
	int Right = int(1);
	return ObserveNumericType(Left / Right);
}

int EntryLangOpNumericBinaryFloat32ModuloFloat32NearMin()
{
	float32 Left = float32(-1.70141173e38);
	float32 Right = float32(2.0);
	return ObserveNumericType(Left % Right);
}

int EntryLangOpNumericBinaryIntLessIntZero()
{
	int Left = int(0);
	int Right = int(0);
	return ObserveNumericType(Left < Right);
}

int EntryLangOpNumericBinaryUint64LessEqualInt16One()
{
	uint64 Left = uint64(1);
	int16 Right = int16(1);
	return ObserveNumericType(Left <= Right);
}

int EntryLangOpNumericBinaryFloat64GreaterFloat32Negative()
{
	float64 Left = float64(-3.0);
	float32 Right = float32(-3.0);
	return ObserveNumericType(Left > Right);
}

int EntryLangOpNumericBinaryIntGreaterEqualUintNearMin()
{
	int Left = int(-2147483647);
	uint Right = uint(1);
	return ObserveNumericType(Left >= Right);
}

int EntryLangOpNumericBinaryInt8EqualInt8NearMax()
{
	int8 Left = int8(126);
	int8 Right = int8(126);
	return ObserveNumericType(Left == Right);
}

int EntryLangOpNumericBinaryFloat32NotEqualInt64Zero()
{
	float32 Left = float32(0.0);
	int64 Right = int64(0);
	return ObserveNumericType(Left != Right);
}
