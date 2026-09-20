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

int EntryLangCfForClausesPresentPresentPresentZero()
{
	int Limit = 0;
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

int EntryLangCfForClausesPresentPresentPresentMany()
{
	int Limit = 4;
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

int EntryLangCfForClausesPresentPresentOmittedZero()
{
	int Limit = 0;
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

int EntryLangCfForClausesPresentPresentOmittedMany()
{
	int Limit = 4;
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

int EntryLangCfForClausesPresentOmittedPresentZero()
{
	int Limit = 0;
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

int EntryLangCfForClausesPresentOmittedPresentMany()
{
	int Limit = 4;
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

int EntryLangCfForClausesPresentOmittedOmittedZero()
{
	int Limit = 0;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (Index = InitializeIndex(InitCount); ; )
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

int EntryLangCfForClausesPresentOmittedOmittedOne()
{
	int Limit = 1;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (Index = InitializeIndex(InitCount); ; )
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

int EntryLangCfForClausesPresentOmittedOmittedMany()
{
	int Limit = 4;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (Index = InitializeIndex(InitCount); ; )
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

int EntryLangCfForClausesOmittedPresentPresentZero()
{
	int Limit = 0;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (; CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))
	{
		++BodyCount;
	}
	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfForClausesOmittedPresentPresentOne()
{
	int Limit = 1;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (; CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))
	{
		++BodyCount;
	}
	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfForClausesOmittedPresentPresentMany()
{
	int Limit = 4;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (; CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))
	{
		++BodyCount;
	}
	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfForClausesOmittedPresentOmittedZero()
{
	int Limit = 0;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (; CountCondition(Index, Limit, ConditionCount); )
	{
		++BodyCount;
		++Index;
	}
	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfForClausesOmittedPresentOmittedOne()
{
	int Limit = 1;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (; CountCondition(Index, Limit, ConditionCount); )
	{
		++BodyCount;
		++Index;
	}
	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfForClausesOmittedPresentOmittedMany()
{
	int Limit = 4;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (; CountCondition(Index, Limit, ConditionCount); )
	{
		++BodyCount;
		++Index;
	}
	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfForClausesOmittedOmittedPresentZero()
{
	int Limit = 0;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (; ; CountIncrement(Index, IncrementCount))
	{
		++BodyCount;
		if (Index >= Limit - 1)
		{
			break;
		}
	}
	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfForClausesOmittedOmittedPresentOne()
{
	int Limit = 1;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (; ; CountIncrement(Index, IncrementCount))
	{
		++BodyCount;
		if (Index >= Limit - 1)
		{
			break;
		}
	}
	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

int EntryLangCfForClausesOmittedOmittedPresentMany()
{
	int Limit = 4;
	int Index = 0;
	int InitCount = 0;
	int BodyCount = 0;
	int ConditionCount = 0;
	int IncrementCount = 0;
	for (; ; CountIncrement(Index, IncrementCount))
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

int EntryLangCfForClausesOmittedOmittedOmittedOne()
{
	int Limit = 1;
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

int EntryLangCfForClausesOmittedOmittedOmittedMany()
{
	int Limit = 4;
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
