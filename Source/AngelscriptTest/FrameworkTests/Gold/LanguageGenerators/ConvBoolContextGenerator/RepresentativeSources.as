int EntryLangConvBoolContextBoolIfZero()
{
	bool SourceValue = false;
	if (SourceValue)
	{
		return 1;
	}
	return 0;
}

int EntryLangConvBoolContextBoolWhileOne()
{
	bool SourceValue = true;
	while (SourceValue)
	{
		return 1;
	}
	return 0;
}

int EntryLangConvBoolContextBoolTernaryConditionZero()
{
	bool SourceValue = false;
	return SourceValue ? 1 : 0;
}

int EntryLangConvBoolContextBoolLogicalAndOne()
{
	bool SourceValue = true;
	return SourceValue && true ? 1 : 0;
}

int EntryLangConvBoolContextBoolLogicalOrZero()
{
	bool SourceValue = false;
	return SourceValue || false ? 1 : 0;
}

int EntryLangConvBoolContextBoolLogicalXorNegative()
{
	bool SourceValue = true;
	return SourceValue ^^ false ? 1 : 0;
}

int EntryLangConvBoolContextEnumIfZero()
{
	EBoolConversion SourceValue = EBoolConversion::Zero;
	if (SourceValue)
	{
		return 1;
	}
	return 0;
}
