struct FTransferValue
{
	int Value = 0;
}

int EntryLangCtorTransferScriptValueLocalCopyDeclarationLocalInitialIdentityValue()
{
	FTransferValue Source = FTransferValue(7);
	FTransferValue Target = Source;
	FTransferValue Copied = Target;
	Target.Value = 23;
	RecordConstructorTransferState(0, Target.Value, Target.Value, Target.Value, 0, 101);
	return 70107;
}

int EntryLangCtorTransferScriptValueTemporaryFieldConstructorTransferInitialIdentityValue()
{
	FTransferValue Target = FTransferValue(7);
	FTransferValue Field = Target;
	Target.Value = 23;
	RecordConstructorTransferState(0, Target.Value, Target.Value, Target.Value, 2, 101);
	return 70107;
}

int EntryLangCtorTransferNativeValueLocalAssignmentLocalInitialIdentityValue()
{
	FNativeCaseValue Source = FNativeCaseValue(7);
	FNativeCaseValue Target = Source;
	FNativeCaseValue Assigned;
	Assigned = Target;
	Target.Value = 23;
	RecordConstructorTransferState(0, Target.Value, Target.Value, Target.Value, 0, 201);
	return 7;
}

int EntryLangCtorTransferScriptReferenceLocalFieldAssignmentAfterDefaultInitialIdentityValue()
{
	FTransferReference Source = FTransferReference(7);
	FTransferReference Target = Source;
	FTransferReference Field;
	Field = Target;
	Target.Value = 23;
	RecordConstructorTransferState(0, Target.Value, Target.Value, Target.Value, 1, 301);
	return 70107;
}

int EntryLangCtorTransferNativeReferenceLocalArgumentTransferInitialIdentityValue()
{
	FNativeCaseReference Source = FNativeCaseReference(7);
	FNativeCaseReference Target = Source;
	int Auxiliary = Target.Value;
	Target.Value = 23;
	RecordConstructorTransferState(0, Target.Value, Target.Value, Target.Value, 1, 401);
	return 7;
}

int EntryLangCtorTransferScriptValueReturnReturnTransferInitialIdentityValue()
{
	FTransferValue Source = FTransferValue(7);
	FTransferValue Target = Source;
	Target.Value = 23;
	RecordConstructorTransferState(0, Target.Value, Target.Value, Target.Value, 0, 101);
	return 70107;
}

int EntryLangCtorTransferDerivedReferenceExactSelfAssignmentInitialIdentityValue()
{
	FTransferDerived Source = FTransferDerived(7);
	FTransferDerived Target = Source;
	Target = Target;
	Target.Value = 23;
	RecordConstructorTransferState(0, Target.Value, Target.Value, Target.Value, 1, 502);
	return 70107;
}

int EntryLangCtorTransferDerivedReferenceBaseViewChainedAssignmentInitialIdentityValue()
{
	FTransferDerived Source = FTransferDerived(7);
	FTransferDerived Target = Source;
	FTransferDerived Mid;
	FTransferDerived Last;
	Last = Mid = Target;
	Target.Value = 31;
	RecordConstructorTransferState(0, Target.Value, Target.Value, Target.Value, 1, 502);
	return 70107;
}

int EntryLangCtorTransferScriptValueLocalCopyDeclarationLocalTargetMutationRelation()
{
	FTransferValue Source = FTransferValue(7);
	FTransferValue Target = Source;
	FTransferValue Copied = Target;
	Target.Value = 23;
	RecordConstructorTransferState(2, Target.Value, Target.Value, Target.Value, 0, 101);
	return 0;
}

int EntryLangCtorTransferScriptValueTemporaryCopyDeclarationLocalSourceOrTemporaryState()
{
	FTransferValue Target = FTransferValue(7);
	FTransferValue Copied = Target;
	Target.Value = 23;
	RecordConstructorTransferState(1, Target.Value, Target.Value, Target.Value, 2, 101);
	return -1;
}
