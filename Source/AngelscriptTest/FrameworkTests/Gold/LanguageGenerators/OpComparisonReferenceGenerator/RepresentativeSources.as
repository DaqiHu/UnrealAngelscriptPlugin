FComparisonReferenceRoot TraceReferenceOperand(int Marker, FComparisonReferenceRoot Value)
{
	RecordReferenceOperand(Marker, Value == nullptr ? 0 : Value.GetIdentity());
	return Value;
}

const FComparisonReferenceRoot TraceConstReferenceOperand(int Marker, const FComparisonReferenceRoot Value)
{
	RecordReferenceOperand(Marker, Value == nullptr ? 0 : Value.GetIdentity());
	return Value;
}

int EntryLangOpComparisonReferenceEqualSameNonNullLeftRight()
{
	FComparisonReferenceRoot Left = MakeComparisonRoot();
	FComparisonReferenceRoot Right = Left;
	return (TraceReferenceOperand(1, Left) == TraceReferenceOperand(2, Right)) ? 1 : 0; // REFERENCE_COMPARE_CAUSE
}

int EntryLangOpComparisonReferenceEqualConstSameLeftRight()
{
	FComparisonReferenceRoot Mutable = MakeComparisonRoot();
	const FComparisonReferenceRoot Left = Mutable;
	const FComparisonReferenceRoot Right = Mutable;
	return (TraceConstReferenceOperand(1, Left) == TraceConstReferenceOperand(2, Right)) ? 1 : 0; // REFERENCE_COMPARE_CAUSE
}

int EntryLangOpComparisonReferenceNotEqualBothNullRightLeft()
{
	FComparisonReferenceRoot Left = nullptr;
	FComparisonReferenceRoot Right = nullptr;
	return (TraceReferenceOperand(2, Right) != TraceReferenceOperand(1, Left)) ? 1 : 0; // REFERENCE_COMPARE_CAUSE
}

int EntryLangOpComparisonReferenceLessSameNonNullLeftRight()
{
	FComparisonReferenceRoot Left = MakeComparisonRoot();
	FComparisonReferenceRoot Right = Left;
	return (TraceReferenceOperand(1, Left) < TraceReferenceOperand(2, Right)) ? 1 : 0; // REFERENCE_COMPARE_CAUSE
}
