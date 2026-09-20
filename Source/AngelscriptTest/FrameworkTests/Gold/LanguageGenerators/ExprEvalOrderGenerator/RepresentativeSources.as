int EntryLangExprEvalOrderBinaryTwoCompleteSingleLine()
{
	return RecordEagerStage(1, 1, false) + RecordEagerStage(2, 2, false);
}

int EntryLangExprEvalOrderAssignmentTwoCompleteSingleLine()
{
	int Target = 0;

	Target = RecordEagerStage(1, 1, false) + RecordEagerStage(2, 2, false);
	return Target;
}

int EntryLangExprEvalOrderCompoundAssignmentTwoCompleteSingleLine()
{
	int Target = 10;

	Target += RecordEagerStage(1, 1, false) + RecordEagerStage(2, 2, false);
	return Target;
}

int EntryLangExprEvalOrderCallArgumentsTwoCompleteSingleLine()
{
	return CollectEager(RecordEagerStage(1, 1, false), RecordEagerStage(2, 2, false));
}

int EntryLangExprEvalOrderConstructorArgumentsTwoCompleteSingleLine()
{
	return FEagerConstructed2(RecordEagerStage(1, 1, false), RecordEagerStage(2, 2, false)).Value;
}

int EntryLangExprEvalOrderIndexArgumentsTwoCompleteSingleLine()
{
	FEagerIndexer2 Indexer;

	return Indexer[RecordEagerStage(1, 1, false), RecordEagerStage(2, 2, false)];
}

int EntryLangExprEvalOrderCallChainTwoCompleteSingleLine()
{
	return FEagerChain(RecordEagerStage(1, 1, false)).Step(RecordEagerStage(2, 2, false)).Read();
}

int EntryLangExprEvalOrderMemberIndexChainTwoCompleteSingleLine()
{
	return FEagerChain(RecordEagerStage(1, 1, false)).Step(RecordEagerStage(2, 2, false))[0].Value;
}

int EntryLangExprEvalOrderNestedCastTwoCompleteSingleLine()
{
	return int(float(double(RecordEagerStage(1, 1, false) + RecordEagerStage(2, 2, false))));
}

int EntryLangExprEvalOrderBinaryTwoExceptionFirstSingleLine()
{
	return RecordEagerStage(1, 1, true) + RecordEagerStage(2, 2, false);
}

int EntryLangExprEvalOrderBinaryTwoExceptionMiddleSingleLine()
{
	return CompleteEagerBoundary(RecordEagerStage(1, 1, false) + RecordEagerStage(2, 2, false), true);
}

int EntryLangExprEvalOrderBinaryTwoExceptionLastSingleLine()
{
	return RecordEagerStage(1, 1, false) + RecordEagerStage(2, 2, true);
}

int EntryLangExprEvalOrderBinaryThreeCompleteSingleLine()
{
	return RecordEagerStage(1, 1, false) + RecordEagerStage(2, 2, false) + RecordEagerStage(3, 3, false);
}

int EntryLangExprEvalOrderBinaryTwoCompleteComments()
{
	return /* stage_1 */ RecordEagerStage(1, 1, false) /* operator boundary */ + /* next operand */ /* stage_2 */ RecordEagerStage(2, 2, false);
}

int EntryLangExprEvalOrderBinaryTwoCompleteWhitespace()
{
	return RecordEagerStage(1, 1, false)  +	 RecordEagerStage(2, 2, false);
}

int EntryLangExprEvalOrderBinaryTwoCompleteMultiline()
{
	return RecordEagerStage(1, 1, false) +
		RecordEagerStage(2, 2, false);
}

int EntryLangExprEvalOrderBinaryTwoCompleteNestedParentheses()
{
	return (((((((((RecordEagerStage(1, 1, false)))) + (((RecordEagerStage(2, 2, false))))))))));
}
