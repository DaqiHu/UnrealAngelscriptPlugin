int EntryLangRefFailureExplicitHandleFreshModule()
{
	FRefRoot@ Explicit = MakeRefRoot(1);
	return Explicit.GetIdentity();
}

int EntryLangRefFailureConstRemovalFreshModule()
{
	const FRefRoot ReadOnly = MakeRefRoot(2);
	FRefRoot Mutable = ReadOnly;
	Mutable.SetValue(3);
	return Mutable.GetValue();
}

int EntryLangRefFailureTemporaryOutFreshModule()
{
	WriteTemporaryOut(MakeRefRoot(4));
	return 0;
}

int EntryLangRefFailureExpiredLocalReturnFreshModule()
{
	int& Alias = ReturnExpiredLocal();
	return Alias;
}

int EntryLangRefFailureUnrelatedAssignmentFreshModule()
{
	FRefRoot Root = MakeRefRoot(6);
	FRefUnrelated Other = MakeRefUnrelated(7);
	Root = Other;
	return Root.GetValue();
}

int EntryLangRefFailureUnrelatedCastFreshModule()
{
	FRefUnrelated Other = MakeRefUnrelated(8);
	FRefDerived Result = cast<FRefDerived>(Other);
	return Result.GetValue();
}

int EntryLangRefFailureNullMemberAccessFreshModule()
{
	FRefRoot Missing = nullptr;
	return Missing.GetValue();
}

int EntryLangRefFailureStaleModuleObjectFreshModule()
{
	FModuleOwnedReference Value;
	return Value.Value;
}

int EntryLangRefFailureAmbiguousOverloadFreshModule()
{
	return SelectAmbiguous(nullptr);
}

int EntryLangRefFailureIncompatibleInoutFreshModule()
{
	FRefRoot Root = MakeRefRoot(9);
	MutateDerived(Root);
	return Root.GetValue();
}

int EntryLangRefFailureNullMemberAccessSameModuleOrContext()
{
	FRefRoot Missing = nullptr;
	return Missing.GetValue();
}
