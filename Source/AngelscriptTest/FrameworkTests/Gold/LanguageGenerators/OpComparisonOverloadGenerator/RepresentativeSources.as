int EntryLangOpComparisonOverloadLessLessLeftRightMutable()
{
	FOverloadedComparisonValue Left(1);
	FOverloadedComparisonValue Right(2);
	const bool Result = Left < Right;
	return Result ? 1 : 0;
}

int EntryLangOpComparisonOverloadEqualEqualLeftRightConst()
{
	const FOverloadedComparisonValue Left(2);
	FOverloadedComparisonValue Right(2);
	const bool Result = Left == Right;
	return Result ? 1 : 0;
}

int EntryLangOpComparisonOverloadNotEqualUnequalRightLeftMutable()
{
	FOverloadedComparisonValue Left(-4);
	FOverloadedComparisonValue Right(5);
	const bool Result = Right != Left;
	return Result ? 1 : 0;
}
