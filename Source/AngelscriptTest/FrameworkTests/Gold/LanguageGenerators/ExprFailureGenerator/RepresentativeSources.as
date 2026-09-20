int EntryLangExprFailureInitializerDivideZeroFreshModule()
{
	int Result = RecordFailureStage(1) / RecordFailureStage(0);
	return Result;
}

int EntryLangExprFailureAssignmentExceptionLeftFreshModule()
{
	int Result = 0;
	Result = ThrowFailureStage(1) + RecordFailureStage(2);
	return Result;
}

int EntryLangExprFailureArgumentNullAccessFreshModule()
{
	FRefRoot NullFailure = nullptr;
	return ObserveFailure(RecordFailureStage(1) + NullFailure.GetValue());
}

int EntryLangExprFailureConditionIndexOutOfRangeFreshModule()
{
	FFailureIndex FailureIndex;
	if (FailureIndex[RecordFailureStage(9)] != 0)
	{
		return 1;
	}
	return 0;
}

int EntryLangExprFailureInitializerInvalidLvalueFreshModule()
{
	int Result = ((1 + 2) = 3);
	return Result;
}

int EntryLangExprFailureInitializerMissingDelimiterFreshModule()
{
	int Result = RecordFailureStage(1];
	return Result;
}

int EntryLangExprFailureInitializerMalformedTernaryFreshModule()
{
	int Result = (true ? RecordFailureStage(1));
	return Result;
}

int EntryLangExprFailureInitializerMissingSymbolFreshModule()
{
	int Result = MissingFailureSymbol;
	return Result;
}
