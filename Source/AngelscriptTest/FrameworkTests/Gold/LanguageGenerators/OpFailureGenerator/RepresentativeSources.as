int EntryLangOpFailureInvalidShiftCountDiagnosticOrExceptionFreshModule()
{
	FNativeCaseValue ScopeValue(77);
	uint Value = 1;
	int Count = -1;
	return int(Value << Count); // OP_CAUSE
}

int EntryLangOpFailureDivideZeroDiagnosticOrExceptionFreshModule()
{
	FNativeCaseValue ScopeValue(77);
	int Numerator = RecordOperatorStage(1);
	int Divisor = RecordOperatorStage(0);
	return (Numerator / Divisor) + RecordOperatorStage(9); // OP_CAUSE
}

int EntryLangOpFailureUnsupportedOperandDiagnosticOrExceptionFreshModule()
{
	FNativeCaseValue ScopeValue(77);
	bool Left = true;
	bool Right = false;
	return Left + Right; // OP_CAUSE
}

int EntryLangOpFailureAssignmentExceptionDiagnosticOrExceptionFreshModule()
{
	FNativeCaseValue ScopeValue(77);
	FThrowingAssignment Left;
	FThrowingAssignment Right;
	Left = Right;
	return RecordOperatorStage(4);
}
