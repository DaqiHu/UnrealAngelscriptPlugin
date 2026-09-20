int EntryLangFnReturnDirectVoid()
{
	ProbeLangFnReturnDirectVoid();
	return 0;
}

int EntryLangFnReturnDirectInt()
{
	return int(ProbeLangFnReturnDirectInt());
}

int EntryLangFnReturnDirectFloat32()
{
	float32 Observed = ProbeLangFnReturnDirectFloat32();
	return Observed == Observed ? 0 : 1;
}

int EntryLangFnReturnDirectBool()
{
	return ProbeLangFnReturnDirectBool() ? 1 : 0;
}

int EntryLangFnReturnDirectEnum()
{
	return int(ProbeLangFnReturnDirectEnum());
}

int EntryLangFnReturnDirectTypedef()
{
	return int(ProbeLangFnReturnDirectTypedef());
}

int EntryLangFnReturnDirectScriptValue()
{
	return ProbeLangFnReturnDirectScriptValue().Value;
}

int EntryLangFnReturnDirectScriptReference()
{
	FScriptCaseReference@ Result = ProbeLangFnReturnDirectScriptReference();
	if (Result is null)
	{
		return 0;
	}
	return Result.Value;
}

int EntryLangFnReturnDirectNullReference()
{
	FScriptCaseReference@ Result = ProbeLangFnReturnDirectNullReference();
	if (Result is null)
	{
		return 0;
	}
	return Result.Value;
}

int EntryLangFnReturnIfElseInt()
{
	return int(ProbeLangFnReturnIfElseInt());
}

int EntryLangFnReturnSwitchInt()
{
	return int(ProbeLangFnReturnSwitchInt());
}

int EntryLangFnReturnEarlyInt()
{
	return int(ProbeLangFnReturnEarlyInt());
}

int EntryLangFnReturnRecursiveBaseInt()
{
	return int(ProbeLangFnReturnRecursiveBaseInt());
}

int EntryLangFnReturnExceptionInt()
{
	return int(ProbeLangFnReturnExceptionInt());
}

int EntryLangFnReturnMissingInvalidVoid()
{
	ProbeLangFnReturnMissingInvalidVoid();
	return 0;
}

int EntryLangFnReturnMissingInvalidInt()
{
	return int(ProbeLangFnReturnMissingInvalidInt());
}

int EntryLangFnReturnIncompatibleInvalidInt()
{
	return int(ProbeLangFnReturnIncompatibleInvalidInt());
}

int EntryLangFnReturnDirectInt8()
{
	return int(ProbeLangFnReturnDirectInt8());
}

int EntryLangFnReturnDirectFloat64()
{
	double Observed = ProbeLangFnReturnDirectFloat64();
	return Observed == Observed ? 0 : 1;
}

int EntryLangFnReturnIfElseVoid()
{
	ProbeLangFnReturnIfElseVoid();
	return 0;
}

int EntryLangFnReturnExceptionVoid()
{
	ProbeLangFnReturnExceptionVoid();
	return 0;
}
