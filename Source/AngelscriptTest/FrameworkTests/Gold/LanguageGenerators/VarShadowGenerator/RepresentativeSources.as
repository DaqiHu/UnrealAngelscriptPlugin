int EntryLangVarShadowNoneFunctionAfterDeclaration()
{
	return ScopeOwnerEntryLangVarShadowNoneFunctionAfterDeclaration();
}

int EntryLangVarShadowInnerOuterNestedBlockInside()
{
	return ScopeOwnerEntryLangVarShadowInnerOuterNestedBlockInside();
}

int EntryLangVarShadowParameterGlobalFunctionBeforeDeclaration()
{
	return ScopeOwnerEntryLangVarShadowParameterGlobalFunctionBeforeDeclaration(11);
}

int EntryLangVarShadowParameterMemberAfterOwner()
{
	FScopeOwnerAfterOwner Owner = FScopeOwnerAfterOwner();
	return Owner.ReadAfterOwner();
}

int EntryLangVarShadowSiblingFunctionSiblingCall()
{
	return ScopeOwnerEntryLangVarShadowSiblingFunctionSiblingCall();
}

int EntryLangVarShadowNoneFunctionBeforeDeclaration()
{
	return ScopeOwnerEntryLangVarShadowNoneFunctionBeforeDeclaration();
}

int EntryLangVarShadowNoneAfterOwner()
{
	return ReadAfterOwner();
}
