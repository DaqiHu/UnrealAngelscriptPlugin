bool CountCondition(int Index, int Limit, int&inout Counter)
{
	++Counter;
	return Index < Limit;
}

int CountIncrement(int&inout Index, int&inout Counter)
{
	++Counter;
	++Index;
	return Index;
}

int EntryLangCfLoopDepthWhileOneNone()
{
	int Limit = 1;
	int Index = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	while (CountCondition(Index, Limit, ConditionCount))
	{
		++BodyCount;
		CountIncrement(Index, IncrementCount);
	}
	return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfLoopDepthWhileTwoBreak()
{
	int Limit = 2;
	int Index = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	while (CountCondition(Index, Limit, ConditionCount))
	{
		++BodyCount;
		break;
	}
	return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfLoopDepthWhileTwoContinue()
{
	int Limit = 2;
	int Index = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	while (CountCondition(Index, Limit, ConditionCount))
	{
		++BodyCount;
		CountIncrement(Index, IncrementCount);
		continue;
	}
	return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfLoopDepthWhileOneReturn()
{
	int Limit = 1;
	int Index = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	while (CountCondition(Index, Limit, ConditionCount))
	{
		++BodyCount;
		return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
	}
	return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfLoopDepthWhileManyNone()
{
	int Limit = 4;
	int Index = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	while (CountCondition(Index, Limit, ConditionCount))
	{
		++BodyCount;
		CountIncrement(Index, IncrementCount);
	}
	return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfLoopDepthDoWhileOneNone()
{
	int Limit = 1;
	int Index = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	do
	{
		if (Index >= Limit)
		{
			break;
		}
		++BodyCount;
		CountIncrement(Index, IncrementCount);
	}
	while (CountCondition(Index, Limit, ConditionCount));
	return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfLoopDepthDoWhileZeroNone()
{
	int Limit = 0;
	int Index = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	do
	{
		if (Index >= Limit)
		{
			break;
		}
		++BodyCount;
		CountIncrement(Index, IncrementCount);
	}
	while (CountCondition(Index, Limit, ConditionCount));
	return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfLoopDepthForTwoNone()
{
	int Limit = 2;
	int Index = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (Index = 0; CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))
	{
		++BodyCount;
	}
	return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfLoopDepthForTwoContinue()
{
	int Limit = 2;
	int Index = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (Index = 0; CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))
	{
		++BodyCount;
		continue;
	}
	return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfLoopDepthForOneBreak()
{
	int Limit = 1;
	int Index = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (Index = 0; CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))
	{
		++BodyCount;
		break;
	}
	return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfLoopDepthForOneReturn()
{
	int Limit = 1;
	int Index = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (Index = 0; CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))
	{
		++BodyCount;
		return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
	}
	return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}
