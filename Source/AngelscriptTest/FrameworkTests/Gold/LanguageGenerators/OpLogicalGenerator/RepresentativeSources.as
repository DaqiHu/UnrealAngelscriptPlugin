int EntryLangOpLogicalAssignmentAndBoolLiteralFalseFalse()
{
	bool Result = TraceLogicalOperand(1, false) && TraceLogicalOperand(2, false);
	return Result ? 1 : 0;
}

int EntryLangOpLogicalReturnOrBoolLvalueTrueFalse()
{
	bool LeftValue = true;
	bool RightValue = false;
	return (TraceLogicalOperand(1, LeftValue) || TraceLogicalOperand(2, RightValue)) ? 1 : 0;
}

int EntryLangOpLogicalConditionXorComparisonTrueTrue()
{
	int LeftNumber = 1;
	int RightNumber = 1;
	if (TraceLogicalOperand(1, LeftNumber == 1) ^^ TraceLogicalOperand(2, RightNumber == 1))
	{
		return 1;
	}
	return 0;
}

int EntryLangOpLogicalArgumentAndConversionOperatorFalseTrue()
{
	FLogicalConversionValue LeftValue(false, 1);
	FLogicalConversionValue RightValue(true, 2);
	return ConsumeLogical(LeftValue.opImplConv() && RightValue.opImplConv()) ? 1 : 0;
}
