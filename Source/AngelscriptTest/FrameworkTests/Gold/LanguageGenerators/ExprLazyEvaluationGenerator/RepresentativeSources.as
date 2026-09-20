int EntryLangExprLazyEvaluationLogicalAndValueFalseSingleLine()
{
	return RecordExpressionBool(1, false) && true ? 1 : 0;
}

int EntryLangExprLazyEvaluationLogicalAndValueTrueComments()
{
	return RecordExpressionBool(1, true) /* selector */ && /* guarded */ true ? 1 : 0;
}

int EntryLangExprLazyEvaluationLogicalOrSideEffectFalseMultiline()
{
	return (
		RecordExpressionBool(1, false)
		||
		RecordExpressionBool(2, true)
	) ? 1 : 0;
}

int EntryLangExprLazyEvaluationConditionalValueFalseParenthesized()
{
	return (RecordExpressionBool(1, false)) ? (41) : (RecordExpressionInt(3, 23));
}

int EntryLangExprLazyEvaluationLogicalAndExceptionTrueSingleLine()
{
	return RecordExpressionBool(1, true) && RaiseGuardedBool() ? 1 : 0;
}

int EntryLangExprLazyEvaluationConditionalExceptionTrueSingleLine()
{
	return RecordExpressionBool(1, true) ? RaiseGuardedInt() : RecordExpressionInt(3, 23);
}

int EntryLangExprLazyEvaluationLogicalOrValueFalseSingleLine()
{
	return RecordExpressionBool(1, false) || true ? 1 : 0;
}

int EntryLangExprLazyEvaluationConditionalValueTrueSingleLine()
{
	return RecordExpressionBool(1, true) ? 41 : RecordExpressionInt(3, 23);
}

int EntryLangExprLazyEvaluationLogicalAndValueFalseParenthesized()
{
	return ((RecordExpressionBool(1, false)) && (true)) ? 1 : 0;
}

int EntryLangExprLazyEvaluationLogicalAndValueTrueMultiline()
{
	return (
		RecordExpressionBool(1, true)
		&&
		true
	) ? 1 : 0;
}

int EntryLangExprLazyEvaluationLogicalOrValueTrueComments()
{
	return RecordExpressionBool(1, true) /* selector */ || /* guarded */ true ? 1 : 0;
}

int EntryLangExprLazyEvaluationConditionalValueTrueComments()
{
	return RecordExpressionBool(1, true) /* selector */ ? /* guarded */ 41 : /* fallback */ RecordExpressionInt(3, 23);
}

int EntryLangExprLazyEvaluationConditionalValueFalseMultiline()
{
	return (
		RecordExpressionBool(1, false)
		?
		41
		:
		RecordExpressionInt(3, 23)
	);
}

int EntryLangExprLazyEvaluationLogicalAndSideEffectTrueSingleLine()
{
	return RecordExpressionBool(1, true) && RecordExpressionBool(2, true) ? 1 : 0;
}

int EntryLangExprLazyEvaluationConditionalSideEffectFalseSingleLine()
{
	return RecordExpressionBool(1, false) ? RecordExpressionInt(2, 41) : RecordExpressionInt(3, 23);
}

int EntryLangExprLazyEvaluationLogicalOrExceptionFalseSingleLine()
{
	return RecordExpressionBool(1, false) || RaiseGuardedBool() ? 1 : 0;
}
