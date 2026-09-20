int EntryLangCfBranchConditionDepthIfVariableFirstLf()
{
	bool FirstCondition = true;
	bool SecondCondition = false;
	int BranchMarker = 0;
	if (FirstCondition)
	{
		BranchMarker = 10;
	}
	return BranchMarker * 100 + ReadBranchConditionCalls();
}

int EntryLangCfBranchConditionDepthIfComparisonSecondLf()
{
	bool FirstCondition = false;
	bool SecondCondition = true;
	int BranchMarker = 0;
	if (FirstCondition == true)
	{
		BranchMarker = 10;
	}
	return BranchMarker * 100 + ReadBranchConditionCalls();
}

int EntryLangCfBranchConditionDepthIfLogicalNoneLf()
{
	bool FirstCondition = false;
	bool SecondCondition = false;
	int BranchMarker = 0;
	if (FirstCondition && true)
	{
		BranchMarker = 10;
	}
	return BranchMarker * 100 + ReadBranchConditionCalls();
}

int EntryLangCfBranchConditionDepthIfNegatedFirstLf()
{
	bool FirstCondition = true;
	bool SecondCondition = false;
	int BranchMarker = 0;
	if (!(FirstCondition == false))
	{
		BranchMarker = 10;
	}
	return BranchMarker * 100 + ReadBranchConditionCalls();
}

int EntryLangCfBranchConditionDepthIfSideEffectFirstLf()
{
	bool FirstCondition = true;
	bool SecondCondition = false;
	int BranchMarker = 0;
	if (EvaluateBranchCondition(FirstCondition))
	{
		BranchMarker = 10;
	}
	return BranchMarker * 100 + ReadBranchConditionCalls();
}

int EntryLangCfBranchConditionDepthIfElseVariableSecondLf()
{
	bool FirstCondition = false;
	bool SecondCondition = true;
	int BranchMarker = 0;
	if (FirstCondition)
	{
		BranchMarker = 10;
	}
	else
	{
		BranchMarker = 20;
	}
	return BranchMarker * 100 + ReadBranchConditionCalls();
}

int EntryLangCfBranchConditionDepthElseIfChainVariableNoneLf()
{
	bool FirstCondition = false;
	bool SecondCondition = false;
	int BranchMarker = 0;
	if (FirstCondition)
	{
		BranchMarker = 10;
	}
	else if (SecondCondition)
	{
		BranchMarker = 20;
	}
	else
	{
		BranchMarker = 30;
	}
	return BranchMarker * 100 + ReadBranchConditionCalls();
}

int EntryLangCfBranchConditionDepthElseIfChainSideEffectSecondLf()
{
	bool FirstCondition = false;
	bool SecondCondition = true;
	int BranchMarker = 0;
	if (EvaluateBranchCondition(FirstCondition))
	{
		BranchMarker = 10;
	}
	else if (EvaluateBranchCondition(SecondCondition))
	{
		BranchMarker = 20;
	}
	else
	{
		BranchMarker = 30;
	}
	return BranchMarker * 100 + ReadBranchConditionCalls();
}
