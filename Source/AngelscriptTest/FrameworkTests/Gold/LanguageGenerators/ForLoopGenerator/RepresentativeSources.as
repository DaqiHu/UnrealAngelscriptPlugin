int InitializeIndex(int&inout InitCount)
{
	++InitCount;
	return 0;
}

bool CountCondition(int Index, int Limit, int&inout ConditionCount)
{
	++ConditionCount;
	return Index < Limit;
}

int CountIncrement(int&inout Index, int&inout IncrementCount)
{
	++IncrementCount;
	++Index;
	return Index;
}

int EntryLangCfForClausesPresentPresentPresentOne()
{
	int Limit = 1;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (Index = InitializeIndex(InitCount); CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))
	{
		++BodyCount;
	}
	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfForClausesPresentPresentOmittedOne()
{
	int Limit = 1;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (Index = InitializeIndex(InitCount); CountCondition(Index, Limit, ConditionCount); )
	{
		++BodyCount;
		++Index;
	}
	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfForClausesPresentOmittedPresentOne()
{
	int Limit = 1;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (Index = InitializeIndex(InitCount); ; CountIncrement(Index, IncrementCount))
	{
		++BodyCount;
		if (Index >= Limit - 1)
		{
			break;
		}
	}
	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfForClausesOmittedOmittedOmittedZero()
{
	int Limit = 0;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (; ; )
	{
		++BodyCount;
		if (Index >= Limit - 1)
		{
			break;
		}
		++Index;
	}
	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}
