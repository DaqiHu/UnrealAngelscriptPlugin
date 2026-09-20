int EntryLangFnParamDirectionValueInt8()
{
	int8 Value = 1;
	return ProbeLangFnParamDirectionValueInt8(Value) && (Value == 1) ? 1 : 0;
}

int EntryLangFnParamDirectionInInt8()
{
	int8 Value = 1;
	return ProbeLangFnParamDirectionInInt8(Value) && (Value == 1) ? 1 : 0;
}

int EntryLangFnParamDirectionOutInt8()
{
	int8 Value = 0;
	ProbeLangFnParamDirectionOutInt8(Value);
	return Value == 1 ? 1 : 0;
}

int EntryLangFnParamDirectionInoutInt8()
{
	int8 Value = 1;
	ProbeLangFnParamDirectionInoutInt8(Value);
	return Value == 1 ? 1 : 0;
}

int EntryLangFnParamDirectionValueFloat32()
{
	float32 Value = 1.0f;
	return ProbeLangFnParamDirectionValueFloat32(Value) && (Value == 1.0f) ? 1 : 0;
}

int EntryLangFnParamDirectionValueBool()
{
	bool Value = true;
	return ProbeLangFnParamDirectionValueBool(Value) && (Value == true) ? 1 : 0;
}

int EntryLangFnParamDirectionValueEnum()
{
	ENativeCaseEnum Value = ENativeCaseEnum::One;
	return ProbeLangFnParamDirectionValueEnum(Value) && (Value == ENativeCaseEnum::One) ? 1 : 0;
}

int EntryLangFnParamDirectionValueTypedef()
{
	NativeCaseAlias Value = NativeCaseAlias(1);
	return ProbeLangFnParamDirectionValueTypedef(Value) && (Value == NativeCaseAlias(1)) ? 1 : 0;
}

int EntryLangFnParamDirectionValueScriptValue()
{
	FScriptCaseValue Value(1);
	return ProbeLangFnParamDirectionValueScriptValue(Value) && (Value.Value == 1) ? 1 : 0;
}

int EntryLangFnParamDirectionValueNativeValue()
{
	FNativeCaseValue Value(1);
	return ProbeLangFnParamDirectionValueNativeValue(Value) && (Value.Value == 1) ? 1 : 0;
}

int EntryLangFnParamDirectionValueInt()
{
	int Value = 1;
	return ProbeLangFnParamDirectionValueInt(Value) && (Value == 1) ? 1 : 0;
}

int EntryLangFnParamDirectionValueFloat64()
{
	float64 Value = 1.0;
	return ProbeLangFnParamDirectionValueFloat64(Value) && (Value == 1.0) ? 1 : 0;
}

int EntryLangFnParamDirectionOutScriptValue()
{
	FScriptCaseValue Value(0);
	ProbeLangFnParamDirectionOutScriptValue(Value);
	return Value.Value == 1 ? 1 : 0;
}

int EntryLangFnParamDirectionInoutScriptValue()
{
	FScriptCaseValue Value(1);
	ProbeLangFnParamDirectionInoutScriptValue(Value);
	return Value.Value == 1 ? 1 : 0;
}
