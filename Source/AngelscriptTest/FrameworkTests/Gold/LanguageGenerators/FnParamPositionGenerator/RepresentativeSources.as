int EntryLangFnParamPositionValueFirstInt8()
{
	int8 Target = 1;
	bool Result = ProbeLangFnParamPositionValueFirstInt8(Target, 202, 303);
	return Result && (Target == 1) ? 1 : 0;
}

int EntryLangFnParamPositionValueMiddleInt8()
{
	int8 Target = 1;
	bool Result = ProbeLangFnParamPositionValueMiddleInt8(101, Target, 303);
	return Result && (Target == 1) ? 1 : 0;
}

int EntryLangFnParamPositionValueLastInt8()
{
	int8 Target = 1;
	bool Result = ProbeLangFnParamPositionValueLastInt8(101, 202, Target);
	return Result && (Target == 1) ? 1 : 0;
}

int EntryLangFnParamPositionInFirstInt8()
{
	int8 Target = 1;
	bool Result = ProbeLangFnParamPositionInFirstInt8(Target, 202, 303);
	return Result && (Target == 1) ? 1 : 0;
}

int EntryLangFnParamPositionOutFirstInt8()
{
	int8 Target = 0;
	bool Result = ProbeLangFnParamPositionOutFirstInt8(Target, 202, 303);
	return Result && (Target == 126) ? 1 : 0;
}

int EntryLangFnParamPositionInoutFirstInt8()
{
	int8 Target = 1;
	bool Result = ProbeLangFnParamPositionInoutFirstInt8(Target, 202, 303);
	return Result && (Target == 126) ? 1 : 0;
}

int EntryLangFnParamPositionValueFirstTypedef()
{
	NativeCaseAlias Target = NativeCaseAlias(1);
	bool Result = ProbeLangFnParamPositionValueFirstTypedef(Target, 202, 303);
	return Result && (Target == NativeCaseAlias(1)) ? 1 : 0;
}

int EntryLangFnParamPositionValueFirstScriptValue()
{
	FScriptCaseValue Target(1);
	bool Result = ProbeLangFnParamPositionValueFirstScriptValue(Target, 202, 303);
	return Result && (Target.Value == 1) ? 1 : 0;
}

int EntryLangFnParamPositionValueFirstNativeValue()
{
	FNativeCaseValue Target(1);
	bool Result = ProbeLangFnParamPositionValueFirstNativeValue(Target, 202, 303);
	return Result && (Target.Value == 1) ? 1 : 0;
}

int EntryLangFnParamPositionInoutLastInt8()
{
	int8 Target = 1;
	bool Result = ProbeLangFnParamPositionInoutLastInt8(101, 202, Target);
	return Result && (Target == 126) ? 1 : 0;
}

int EntryLangFnParamPositionInoutFirstBool()
{
	bool Target = true;
	bool Result = ProbeLangFnParamPositionInoutFirstBool(Target, 202, 303);
	return Result && (Target == false) ? 1 : 0;
}

int EntryLangFnParamPositionOutFirstScriptValue()
{
	FScriptCaseValue Target(1);
	bool Result = ProbeLangFnParamPositionOutFirstScriptValue(Target, 202, 303);
	return Result && (Target.Value == 1) ? 1 : 0;
}
