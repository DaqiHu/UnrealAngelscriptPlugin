int EntryLangFnArityTargetZeroGlobal()
{
	return ProbeEntryLangFnArityTargetZeroGlobal();
}

int EntryLangFnArityTargetOneNamespaceGlobal()
{
	return NEntryLangFnArityTargetOneNamespaceGlobal::ProbeEntryLangFnArityTargetOneNamespaceGlobal(1);
}

int EntryLangFnArityTargetTwoInstanceMethod()
{
	FOwnerEntryLangFnArityTargetTwoInstanceMethod Owner;
	return Owner.ProbeEntryLangFnArityTargetTwoInstanceMethod(1, 2);
}
