int EntryLangPropCopyIndependenceSourceAfterTransferCopyConstructInt8Exact()
{
	FPropertyCopyBase_EntryLangPropCopyIndependenceSourceAfterTransferCopyConstructInt8Exact Receiver = FPropertyCopyBase_EntryLangPropCopyIndependenceSourceAfterTransferCopyConstructInt8Exact();
	int8 TransferTarget = Receiver.Source;
	Receiver.Source = int8(37);
	return 37 * 1000 + 11;
}

int EntryLangPropCopyIndependenceTargetAfterTransferAssignInt8Exact()
{
	FPropertyCopyBase_EntryLangPropCopyIndependenceTargetAfterTransferAssignInt8Exact Receiver = FPropertyCopyBase_EntryLangPropCopyIndependenceTargetAfterTransferAssignInt8Exact();
	Receiver.Target = Receiver.Source;
	Receiver.Target = int8(37);
	return 11 * 1000 + 37;
}

int EntryLangPropCopyIndependenceNestedMemberSelfAssignInt8Exact()
{
	FPropertyCopyBase_EntryLangPropCopyIndependenceNestedMemberSelfAssignInt8Exact Receiver = FPropertyCopyBase_EntryLangPropCopyIndependenceNestedMemberSelfAssignInt8Exact();
	Receiver.NestedTarget = Receiver.NestedTarget;
	Receiver.NestedSource.Stored = int8(37);
	return 11 * 1000 + 37;
}

int EntryLangPropCopyIndependenceSourceAfterTransferSelfAssignBoolExact()
{
	FPropertyCopyBase_EntryLangPropCopyIndependenceSourceAfterTransferSelfAssignBoolExact Receiver = FPropertyCopyBase_EntryLangPropCopyIndependenceSourceAfterTransferSelfAssignBoolExact();
	Receiver.Source = Receiver.Source;
	Receiver.Source = false;
	return 0 * 1000 + 0;
}

int EntryLangPropCopyIndependenceSourceAfterTransferCopyConstructInt8Derived()
{
	FPropertyCopyDerived_EntryLangPropCopyIndependenceSourceAfterTransferCopyConstructInt8Derived Receiver = FPropertyCopyDerived_EntryLangPropCopyIndependenceSourceAfterTransferCopyConstructInt8Derived();
	int8 TransferTarget = Receiver.Source;
	Receiver.Source = int8(37);
	return 37 * 1000 + 11;
}

int EntryLangPropCopyIndependenceSourceAfterTransferCopyConstructScriptReferenceExact()
{
	FPropertyCopyBase_EntryLangPropCopyIndependenceSourceAfterTransferCopyConstructScriptReferenceExact Receiver = FPropertyCopyBase_EntryLangPropCopyIndependenceSourceAfterTransferCopyConstructScriptReferenceExact();
	FScriptCaseReference TransferTarget = Receiver.Source;
	Receiver.Source = FScriptCaseReference(37);
	return 37 * 1000 + 37;
}
