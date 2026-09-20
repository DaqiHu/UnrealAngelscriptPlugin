int EntryLangFnValueLifecycleNoneZeroValueArgument()
{
	return ConsumeValueArgument(ProduceArgumentValue(41, false), 1);
}

int EntryLangFnValueLifecycleNoneOneValueArgument()
{
	FNativeCaseValue Initialized0(100);

	return ConsumeValueArgument(ProduceArgumentValue(41, false), 1);
}

int EntryLangFnValueLifecycleNoneManyValueReturn()
{
	FNativeCaseValue Initialized0(100);
	FNativeCaseValue Initialized1(101);
	FNativeCaseValue Initialized2(102);
	FNativeCaseValue Initialized3(103);

	FNativeCaseValue Value = ProduceReturnValue(41, 1, false);
	return Value.Value;
}

int EntryLangFnValueLifecycleArgumentEvaluationZeroValueArgument()
{
	int Zero = 0;
	return ConsumeValueArgument(FNativeCaseValue(41), 1 / Zero);
}

int EntryLangFnValueLifecycleBodyZeroValueArgument()
{
	return ConsumeValueArgument(FNativeCaseValue(41), 0);
}

int EntryLangFnValueLifecycleReturnConstructionZeroValueReturn()
{
	FNativeCaseValue Value = ProduceReturnValue(41, 1, true);
	return Value.Value;
}

int EntryLangFnValueLifecycleNoneZeroValueReturn()
{
	FNativeCaseValue Value = ProduceReturnValue(41, 1, false);
	return Value.Value;
}

int EntryLangFnValueLifecycleArgumentEvaluationZeroValueReturn()
{
	int Zero = 0;
	FNativeCaseValue Value = ProduceReturnValue(41, 1 / Zero, false);
	return Value.Value;
}

int EntryLangFnValueLifecycleBodyZeroValueReturn()
{
	FNativeCaseValue Value = ProduceReturnValue(41, 0, false);
	return Value.Value;
}

int EntryLangFnValueLifecycleReturnConstructionZeroValueArgument()
{
	return ConsumeValueArgument(ProduceArgumentValue(41, true), 1);
}
