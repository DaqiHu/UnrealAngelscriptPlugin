int EntryLangVarAssignTargetSimpleMutableLocalInt8()
{
	int8 Target = int8(11);
	Target = int8(29);
	return ObserveAssignmentValue(Target);
}

int EntryLangVarAssignTargetCopySourceMutableLocalScriptValue()
{
	FScriptCaseValue Target = FScriptCaseValue(11);
	FScriptCaseValue Source = FScriptCaseValue(29);
	Target = Source;
	Source.Value = 37;
	return ObserveAssignmentValue(Target) * 100 + ObserveAssignmentValue(Source);
}

int EntryLangVarAssignTargetSelfAssignmentMutableLocalBool()
{
	bool Target = false;
	Target = Target;
	return ObserveAssignmentValue(Target);
}

int EntryLangVarAssignTargetCompoundMutableFieldInt()
{
	FAssignmentHolderInt Holder = FAssignmentHolderInt();
	Holder.Target += int(29);
	return ObserveAssignmentValue(Holder.Target);
}

int EntryLangVarAssignTargetReferenceRebindReferenceAliasScriptReference()
{
	FScriptCaseReference Target = FScriptCaseReference(11);
	FScriptCaseReference Source = FScriptCaseReference(29);
	return ApplyAssignmentAliasReferenceRebindScriptReference(Target, Source);
}

int EntryLangVarAssignTargetSimpleConstLocalScriptReference()
{
	const FScriptCaseReference Target = FScriptCaseReference(11);
	Target = FScriptCaseReference(29);
	return ObserveAssignmentValue(Target);
}

int EntryLangVarAssignTargetSimpleTemporaryScriptValue()
{
	FScriptCaseValue(11) = FScriptCaseValue(29);
	return 0;
}

int EntryLangVarAssignTargetSimpleConstFieldInt8()
{
	const FAssignmentHolderInt8 Holder = FAssignmentHolderInt8();
	Holder.Target = int8(29);
	return 0;
}
