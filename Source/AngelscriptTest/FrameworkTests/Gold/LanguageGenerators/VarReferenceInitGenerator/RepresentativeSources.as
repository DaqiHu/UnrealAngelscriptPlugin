int EntryLangVarReferenceInitExplicitTypeConstructedLocalScriptReferenceIdentity()
{
	FScriptCaseReference Original = FScriptCaseReference(11);
	FScriptCaseReference Variable = Original;
	return Variable == Original ? 1 : -1;
}

int EntryLangVarReferenceInitAutoParameterNativeReferenceMutation()
{
	FNativeCaseReference Original = CreateNativeCaseReference(11);
	return ExerciseEntryLangVarReferenceInitAutoParameterNativeReferenceMutation(Original);
}

int EntryLangVarReferenceInitConstViewFunctionReturnScriptReferenceArgument()
{
	FScriptCaseReference Original = FScriptCaseReference(11);
	const FScriptCaseReference Variable = PassVariableReference(Original);
	return ObserveVariableReference(Variable, Original);
}

int EntryLangVarReferenceInitExplicitTypeFieldNativeReferenceReturn()
{
	FVariableReferenceHolderNative Holder = FVariableReferenceHolderNative();
	FNativeCaseReference Original = Holder.Stored;
	FNativeCaseReference Variable = Holder.Stored;
	FNativeCaseReference Returned = PassVariableReference(Variable);
	return Returned == Original ? 1 : -1;
}

int EntryLangVarReferenceInitExplicitTypeNullScriptReferenceNullCompare()
{
	FScriptCaseReference Original = nullptr;
	FScriptCaseReference Variable = nullptr;
	return (Variable == nullptr) == true ? 1 : -1;
}

int EntryLangVarReferenceInitExplicitTypeNullScriptReferenceMutation()
{
	FScriptCaseReference Original = nullptr;
	FScriptCaseReference Variable = nullptr;
	Variable.Value += 5;
	return Original.Value == 16 && Variable.Value == 16 ? 1 : -1;
}

int EntryLangVarReferenceInitAutoNullScriptReferenceIdentity()
{
	FScriptCaseReference Original = nullptr;
	auto Variable = nullptr;
	return Variable == Original ? 1 : -1;
}

int EntryLangVarReferenceInitConstViewConstructedLocalScriptReferenceMutation()
{
	FScriptCaseReference Original = FScriptCaseReference(11);
	const FScriptCaseReference Variable = Original;
	Variable.Value += 5;
	return Original.Value == 16 && Variable.Value == 16 ? 1 : -1;
}
