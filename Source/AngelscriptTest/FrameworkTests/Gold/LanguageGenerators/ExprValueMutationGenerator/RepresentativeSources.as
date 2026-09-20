int EntryLangExprValueMutationMutableLvalueAssignLocal()
{
	int LocalTarget = 10;
	LocalTarget = NextMutationOperand(20);
	int Result = LocalTarget;
	int After = LocalTarget;
	int AliasAfter = After;
	return Result * 10000 + After * 100 + AliasAfter;
}

int EntryLangExprValueMutationMutableLvalueCompoundAssignMember()
{
	FMutationOwner RootOwner = MakeMutationOwner();
	RootOwner.DirectTarget += NextMutationOperand(5);
	int Result = RootOwner.DirectTarget;
	int After = RootOwner.DirectTarget;
	int AliasAfter = After;
	return Result * 10000 + After * 100 + AliasAfter;
}

int EntryLangExprValueMutationReferenceAliasAssignLocal()
{
	int LocalTarget = 10;
	int& AliasTarget = BindMutationAlias(LocalTarget);
	AliasTarget = NextMutationOperand(20);
	int Result = AliasTarget;
	int After = AliasTarget;
	int AliasAfter = LocalTarget;
	return Result * 10000 + After * 100 + AliasAfter;
}

int EntryLangExprValueMutationFieldAssignLocal()
{
	FMutationOwner LocalOwner = MakeMutationOwner();
	LocalOwner.FieldTarget = NextMutationOperand(20);
	int Result = LocalOwner.FieldTarget;
	int After = LocalOwner.FieldTarget;
	int AliasAfter = After;
	return Result * 10000 + After * 100 + AliasAfter;
}

int EntryLangExprValueMutationConstLvalueAssignLocal()
{
	const int ConstLocalTarget = 10;
	ConstLocalTarget = NextMutationOperand(20);
	int Result = ConstLocalTarget;
	return Result;
}

int EntryLangExprValueMutationMutableLvaluePrefixIncrementLocal()
{
	int LocalTarget = 10;
	++LocalTarget;
	int Result = LocalTarget;
	int After = LocalTarget;
	int AliasAfter = After;
	return Result * 10000 + After * 100 + AliasAfter;
}

int EntryLangExprValueMutationMutableLvaluePostfixIncrementLocal()
{
	int LocalTarget = 10;
	int Result = LocalTarget++;
	int After = LocalTarget;
	int AliasAfter = After;
	return Result * 10000 + After * 100 + AliasAfter;
}

int EntryLangExprValueMutationMutableLvalueOutArgumentLocal()
{
	int LocalTarget = 10;
	WriteMutationOut(LocalTarget);
	int Result = 31;
	int After = LocalTarget;
	int AliasAfter = After;
	return Result * 10000 + After * 100 + AliasAfter;
}

int EntryLangExprValueMutationTemporaryAssignLocal()
{
	int LocalTarget = 10;
	MakeMutationTemporary(LocalTarget) = NextMutationOperand(20);
	int Result = MakeMutationTemporary(LocalTarget);
	return Result;
}

int EntryLangExprValueMutationPropertyAssignLocal()
{
	FMutationOwner LocalOwner = MakeMutationOwner();
	LocalOwner.GetPropertyTarget() = NextMutationOperand(20);
	int Result = LocalOwner.GetPropertyTarget();
	return Result;
}

int EntryLangExprValueMutationInvalidNonLvalueAssignLocal()
{
	int LocalTarget = 10;
	(ReadMutationNonLValue(LocalTarget) + 0) = NextMutationOperand(20);
	int Result = (ReadMutationNonLValue(LocalTarget) + 0);
	return Result;
}

int EntryLangExprValueMutationMutableLvalueAssignIndexed()
{
	FMutationOwner RootOwner = MakeMutationOwner();
	int& IndexedTarget = RootOwner.IndexedAlias(0);
	IndexedTarget = NextMutationOperand(20);
	int Result = IndexedTarget;
	int After = IndexedTarget;
	int AliasAfter = After;
	return Result * 10000 + After * 100 + AliasAfter;
}

int EntryLangExprValueMutationMutableLvalueAssignGlobal()
{
	GlobalTarget_EntryLangExprValueMutationMutableLvalueAssignGlobal = NextMutationOperand(20);
	int Result = GlobalTarget_EntryLangExprValueMutationMutableLvalueAssignGlobal;
	int After = GlobalTarget_EntryLangExprValueMutationMutableLvalueAssignGlobal;
	int AliasAfter = After;
	return Result * 10000 + After * 100 + AliasAfter;
}

int EntryLangExprValueMutationReferenceAliasAssignIndexed()
{
	FMutationOwner RootOwner = MakeMutationOwner();
	int& AliasTarget = BindMutationAlias(RootOwner.IndexedAlias(0));
	AliasTarget = NextMutationOperand(20);
	int Result = AliasTarget;
	int After = AliasTarget;
	int AliasAfter = RootOwner.GetItem(0);
	return Result * 10000 + After * 100 + AliasAfter;
}
