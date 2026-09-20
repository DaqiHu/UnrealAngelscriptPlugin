int EntryLangVarInitStorageDefaultLocalInt8()
{
	int8 VariableValue;
	VariableValue = 0;
	return 0;
}

int EntryLangVarInitStorageLiteralLocalInt8()
{
	int8 VariableValue = 1;
	return 1;
}

int EntryLangVarInitStorageExpressionLocalInt()
{
	int VariableValue = 1 + 1;
	return 2;
}

int EntryLangVarInitStorageCopyConstLocalBool()
{
	bool VariableSource = true;
	const bool VariableValue = VariableSource;
	return 1;
}

int EntryLangVarInitStorageConstructorBranchLocalScriptValue()
{
	if (true)
	{
		FScriptCaseValue VariableValue = FScriptCaseValue(1);
		return 1;
	}
	return -2;
}

int EntryLangVarInitStorageFunctionReturnFieldLinkageInt8()
{
	FVariableHolderFunctionReturnInt8 Holder;
	return 1;
}

int EntryLangVarInitStorageConditionalConstGlobalInt8()
{
	int Observed = int(VariableValueConditionalInt8);
	return 1;
}

int EntryLangVarInitStorageDefaultAutoInt8()
{
	auto VariableValue;
	return 0;
}

int EntryLangVarInitStorageLiteralConstGlobalNativeValue()
{
	int Observed = VariableValueLiteralNativeValue.Value;
	return 1;
}
