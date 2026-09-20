int EntryLangCfLoopCondTransferDepthWhileVariableZeroNone()
{
	int Limit = 0;
	int BodyCalls = 0;
	int Index = 0;
	bool KeepGoing = Index < Limit;
	while (KeepGoing)
	{
		++BodyCalls;
		++Index;
		KeepGoing = Index < Limit;
	}

	return BodyCalls * 100 + GetConditionCalls();
}

int EntryLangCfLoopCondTransferDepthWhileComparisonOneNone()
{
	int Limit = 1;
	int BodyCalls = 0;
	int Index = 0;
	while (Index < Limit)
	{
		++BodyCalls;
		++Index;
	}

	return BodyCalls * 100 + GetConditionCalls();
}

int EntryLangCfLoopCondTransferDepthWhileLogicalTwoBreak()
{
	int Limit = 2;
	int BodyCalls = 0;
	int Index = 0;
	while ((Index < Limit) && (Limit >= 0))
	{
		++BodyCalls;
		if (Index == 0)
		{
			break;
		}
		++Index;
	}

	return BodyCalls * 100 + GetConditionCalls();
}

int EntryLangCfLoopCondTransferDepthWhileNegatedOneContinue()
{
	int Limit = 1;
	int BodyCalls = 0;
	int Index = 0;
	while (!(Index >= Limit))
	{
		++BodyCalls;
		if (Index == 0)
		{
			++Index;
			continue;
		}
		++Index;
	}

	return BodyCalls * 100 + GetConditionCalls();
}

int EntryLangCfLoopCondTransferDepthWhileSideEffectOneReturn()
{
	int Limit = 1;
	int BodyCalls = 0;
	int Index = 0;
	while (CheckCondition(Index, Limit))
	{
		++BodyCalls;
		if (Index == 0)
		{
			return 1000 + BodyCalls * 100 + GetConditionCalls();
		}
		++Index;
	}

	return BodyCalls * 100 + GetConditionCalls();
}

int EntryLangCfLoopCondTransferDepthDoWhileVariableOneNone()
{
	int Limit = 1;
	int BodyCalls = 0;
	int Index = 0;
	bool KeepGoing = Index < Limit;
	if (Limit == 0)
	{
		return 0;
	}

	do
	{
		++BodyCalls;
		++Index;
		KeepGoing = Index < Limit;
	}
	while (KeepGoing);

	return BodyCalls * 100 + GetConditionCalls();
}

int EntryLangCfLoopCondTransferDepthForVariableOneNone()
{
	int Limit = 1;
	int BodyCalls = 0;
	int Index = 0;
	bool KeepGoing = Index < Limit;
	for (; KeepGoing; ++Index)
	{
		++BodyCalls;
		KeepGoing = Index + 1 < Limit;
	}

	return BodyCalls * 100 + GetConditionCalls();
}

int EntryLangCfLoopCondTransferDepthForSideEffectTwoContinue()
{
	int Limit = 2;
	int BodyCalls = 0;
	int Index = 0;
	for (; CheckCondition(Index, Limit); ++Index)
	{
		++BodyCalls;
		if (Index == 0)
		{
			continue;
		}
	}

	return BodyCalls * 100 + GetConditionCalls();
}
