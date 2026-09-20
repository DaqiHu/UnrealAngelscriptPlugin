int EntryLangOpResultContextUnaryAssignmentExact()
{
	int EvaluationCount = 0;
	int Result = -TraceInt(EvaluationCount, 5);
	return VerifyExact(Result, EvaluationCount, -5, 1);
}

int EntryLangOpResultContextArithmeticReturnConverted()
{
	int EvaluationCount = 0;
	return VerifyConverted(ReturnContextProbe_EntryLangOpResultContextArithmeticReturnConverted(EvaluationCount), EvaluationCount, 5, 0);
}

int EntryLangOpResultContextPowerConditionExact()
{
	int EvaluationCount = 0;
	if (VerifyExact(int(TraceFloat(EvaluationCount, 2.0f) ** TraceFloat(EvaluationCount, 3.0f)), EvaluationCount, 8, 0) == 1)
	{
		return 1;
	}
	return -1;
}

int EntryLangOpResultContextUnaryAssignmentAmbiguous()
{
	int EvaluationCount = 0;
	int Result = SelectAmbiguous(MakeAmbiguousOperand(-TraceInt(EvaluationCount, 5))); // OP_CONTEXT_CAUSE
	return Result;
}

int EntryLangOpResultContextUnaryAssignmentRejected()
{
	int EvaluationCount = 0;
	FContextNoConversion Result = -TraceInt(EvaluationCount, 5); // OP_CONTEXT_CAUSE
	return Result.Value ? 1 : 0;
}
