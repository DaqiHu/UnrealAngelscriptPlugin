int EntryLangExprSourceBoundaryFirstInitializerParenthesesOne()
{
	int64 Result = (int64(41));
	return int(Result);
}

int EntryLangExprSourceBoundaryFirstInitializerChainOne()
{
	int64 Result = MakeBoundaryChain(41).Step().Value;
	return int(Result);
}

int EntryLangExprSourceBoundaryFirstInitializerArgumentsZero()
{
	int64 Result = BoundaryArgumentsZero();
	return int(Result);
}

int EntryLangExprSourceBoundaryFirstInitializerNumericMinimum()
{
	int64 Result = int64(-2147483647 - 1);
	return int(Result);
}

int EntryLangExprSourceBoundaryChangedRebuildInitializerParenthesesOne()
{
	int64 Result = (int64(58));
	return int(Result);
}

int EntryLangExprSourceBoundaryFirstConditionParenthesesOne()
{
	if ((int64(41)) == 41)
	{
		return 41;
	}
	return -1;
}

int EntryLangExprSourceBoundaryFirstReturnParenthesesOne()
{
	return int(ProduceBoundary_EntryLangExprSourceBoundaryFirstReturnParenthesesOne());
}

int EntryLangExprSourceBoundaryFirstArgumentParenthesesOne()
{
	return int(ObserveBoundary((int64(41))));
}

int EntryLangExprSourceBoundaryFirstIndexParenthesesOne()
{
	FBoundaryIndex Probe;
	return int(Probe[(int64(41))]);
}

int EntryLangExprSourceBoundaryFirstInitializerParenthesesEight()
{
	int64 Result = ((((((((int64(41)))))))));
	return int(Result);
}

int EntryLangExprSourceBoundaryFirstInitializerChainEight()
{
	int64 Result = MakeBoundaryChain(41).Step().Step().Step().Step().Step().Step().Step().Step().Value;
	return int(Result);
}

int EntryLangExprSourceBoundaryFirstInitializerArgumentsOne()
{
	int64 Result = BoundaryArgumentsOne(41);
	return int(Result);
}

int EntryLangExprSourceBoundaryFirstInitializerArgumentsMany()
{
	int64 Result = BoundaryArgumentsMany(41, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
	return int(Result);
}

int EntryLangExprSourceBoundaryFirstInitializerNumericMaximum()
{
	int64 Result = int64(2147483647);
	return int(Result);
}

int EntryLangExprSourceBoundaryFirstInitializerWhitespace()
{
	int64 Result = int64(  41	+	3  );
	return int(Result);
}

int EntryLangExprSourceBoundaryFirstInitializerComments()
{
	int64 Result = int64(41 /* seed */ + /* delta */ 3);
	return int(Result);
}

int EntryLangExprSourceBoundaryFirstInitializerMultiline()
{
	int64 Result = int64(
		41
		+
		3
	);
	return int(Result);
}

int EntryLangExprSourceBoundarySameRebuildInitializerParenthesesOne()
{
	int64 Result = (int64(41));
	return int(Result);
}
