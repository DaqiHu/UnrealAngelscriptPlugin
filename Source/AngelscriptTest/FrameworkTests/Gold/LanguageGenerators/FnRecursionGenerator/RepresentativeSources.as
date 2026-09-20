int EntryLangFnRecursionZeroReturnPrimitive()
{
	int Value = 1;
	return RecurseALangFnRecursionZeroReturnPrimitive(Value, 0);
}

int EntryLangFnRecursionZeroReturnValueObject()
{
	FNativeCaseValue Value(1);
	return RecurseALangFnRecursionZeroReturnValueObject(Value, 0);
}

int EntryLangFnRecursionZeroReturnReferenceObject()
{
	return RecurseALangFnRecursionZeroReturnReferenceObject(nullptr, 0, 1);
}

int EntryLangFnRecursionZeroExceptionPrimitive()
{
	int Value = 1;
	return RecurseALangFnRecursionZeroExceptionPrimitive(Value, 0);
}

int EntryLangFnRecursionOneReturnPrimitive()
{
	int Value = 1;
	return RecurseALangFnRecursionOneReturnPrimitive(Value, 1);
}

int EntryLangFnRecursionEightReturnPrimitive()
{
	int Value = 1;
	return RecurseALangFnRecursionEightReturnPrimitive(Value, 8);
}

int EntryLangFnRecursionEightExceptionPrimitive()
{
	int Value = 1;
	return RecurseALangFnRecursionEightExceptionPrimitive(Value, 8);
}

int EntryLangFnRecursionOneExceptionPrimitive()
{
	int Value = 1;
	return RecurseALangFnRecursionOneExceptionPrimitive(Value, 1);
}

int EntryLangFnRecursionConfiguredLimitReturnPrimitive()
{
	int Value = 1;
	return RecurseALangFnRecursionConfiguredLimitReturnPrimitive(Value, 11);
}

int EntryLangFnRecursionConfiguredLimitExceptionPrimitive()
{
	int Value = 1;
	return RecurseALangFnRecursionConfiguredLimitExceptionPrimitive(Value, 13);
}
