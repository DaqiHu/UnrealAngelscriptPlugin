int EntryLangOpComparisonFloatFloat32LessNegativeZeroLeftRight()
{
	float32 Left = -0.0f;
	float32 Right = 0.0f;
	const bool Result = TraceFloat32Operand(1, Left) < TraceFloat32Operand(2, Right);
	return Result ? 1 : 0;
}

int EntryLangOpComparisonFloatFloat32GreaterNanLeftRight()
{
	float32 Left = 0.0f / 0.0f;
	float32 Right = 1.0f;
	const bool Result = TraceFloat32Operand(1, Left) > TraceFloat32Operand(2, Right);
	return Result ? 1 : 0;
}

int EntryLangOpComparisonFloatFloat64EqualEqualPairRightLeft()
{
	float64 Left = 13.25;
	float64 Right = 13.25;
	const bool Result = TraceFloat64Operand(2, Right) == TraceFloat64Operand(1, Left);
	return Result ? 1 : 0;
}

int EntryLangOpComparisonFloatFloat32NotEqualPositiveInfinityLeftRight()
{
	float32 Left = 1.0f / 0.0f;
	float32 Right = 3.402823466e+38f;
	const bool Result = TraceFloat32Operand(1, Left) != TraceFloat32Operand(2, Right);
	return Result ? 1 : 0;
}
