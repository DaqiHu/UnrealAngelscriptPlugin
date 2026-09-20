struct FConditionValue
{
	bool Value;

	FConditionValue(bool InValue)
	{
		Value = InValue;
	}

	bool opImplConv() const
	{
		return Value;
	}
}

class FInvalidCondition
{
}

bool EvaluateCondition(bool InValue, int&inout EvaluationCount)
{
	++EvaluationCount;
	return InValue;
}

int EntryLangCfConditionIfBoolLiteralTrue()
{
	bool ConditionValue = true;
	int Trace = 0;
	int Guard = 0;
	int EvaluationCount = 0;
	if (true)
	{
		Trace = 1;
	}
	return Trace * 10 + EvaluationCount;
}

int EntryLangCfConditionIfBoolLiteralFalse()
{
	bool ConditionValue = false;
	int Trace = 0;
	int Guard = 0;
	int EvaluationCount = 0;
	if (false)
	{
		Trace = 1;
	}
	return Trace * 10 + EvaluationCount;
}

int EntryLangCfConditionIfVariableTrue()
{
	bool ConditionValue = true;
	int Trace = 0;
	int Guard = 0;
	int EvaluationCount = 0;
	if (ConditionValue)
	{
		Trace = 1;
	}
	return Trace * 10 + EvaluationCount;
}

int EntryLangCfConditionIfComparisonFalse()
{
	bool ConditionValue = false;
	int Trace = 0;
	int Guard = 0;
	int EvaluationCount = 0;
	if (2 < 1)
	{
		Trace = 1;
	}
	return Trace * 10 + EvaluationCount;
}

int EntryLangCfConditionIfLogicalTrue()
{
	bool ConditionValue = true;
	int Trace = 0;
	int Guard = 0;
	int EvaluationCount = 0;
	if (ConditionValue && true)
	{
		Trace = 1;
	}
	return Trace * 10 + EvaluationCount;
}

int EntryLangCfConditionIfSideEffectCallTrue()
{
	bool ConditionValue = true;
	int Trace = 0;
	int Guard = 0;
	int EvaluationCount = 0;
	if (EvaluateCondition(ConditionValue, EvaluationCount))
	{
		Trace = 1;
	}
	return Trace * 10 + EvaluationCount;
}

int EntryLangCfConditionIfOverloadedConversionTrue()
{
	bool ConditionValue = true;
	int Trace = 0;
	int Guard = 0;
	int EvaluationCount = 0;
	if (FConditionValue(ConditionValue))
	{
		Trace = 1;
	}
	return Trace * 10 + EvaluationCount;
}

int EntryLangCfConditionWhileSideEffectCallTrue()
{
	bool ConditionValue = true;
	int Trace = 0;
	int Guard = 0;
	int EvaluationCount = 0;
	while ((EvaluateCondition(ConditionValue, EvaluationCount)) && Guard < 1)
	{
		++Trace;
		++Guard;
	}
	return Trace * 10 + EvaluationCount;
}

int EntryLangCfConditionDoWhileBoolLiteralFalse()
{
	bool ConditionValue = false;
	int Trace = 0;
	int Guard = 0;
	int EvaluationCount = 0;
	do
	{
		++Trace;
		++Guard;
	}
	while ((false) && Guard < 1);
	return Trace * 10 + EvaluationCount;
}

int EntryLangCfConditionForBoolLiteralTrue()
{
	bool ConditionValue = true;
	int Trace = 0;
	int Guard = 0;
	int EvaluationCount = 0;
	for (; (true) && Guard < 1; ++Guard)
	{
		++Trace;
	}
	return Trace * 10 + EvaluationCount;
}

int EntryLangCfConditionIfInvalidTypeFalse()
{
	bool ConditionValue = false;
	int Trace = 0;
	int Guard = 0;
	int EvaluationCount = 0;
	if (FInvalidCondition())
	{
		Trace = 1;
	}
	return Trace * 10 + EvaluationCount;
}
