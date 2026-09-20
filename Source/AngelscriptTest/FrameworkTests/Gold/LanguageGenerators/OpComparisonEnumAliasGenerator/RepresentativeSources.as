int EntryLangOpComparisonEnumAliasEnumLessEqualZeroLeftRight()
{
	EComparisonEnum Left = EComparisonEnum::Zero;
	EComparisonEnum Right = EComparisonEnum::Zero;
	const bool Result = TraceEnumOperand(1, Left) < TraceEnumOperand(2, Right);
	return Result ? 1 : 0;
}

int EntryLangOpComparisonEnumAliasEnumGreaterMaximumBoundaryRightLeft()
{
	EComparisonEnum Left = EComparisonEnum::Maximum;
	EComparisonEnum Right = EComparisonEnum::One;
	const bool Result = TraceEnumOperand(2, Right) > TraceEnumOperand(1, Left);
	return Result ? 1 : 0;
}

int EntryLangOpComparisonEnumAliasAliasNotEqualEqualNamedLeftRight()
{
	ComparisonAlias Left = ComparisonAlias(1);
	ComparisonAlias Right = ComparisonAlias(1);
	const bool Result = TraceAliasOperand(1, Left) != TraceAliasOperand(2, Right);
	return Result ? 1 : 0;
}
