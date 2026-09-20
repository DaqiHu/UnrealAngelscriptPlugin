int EntryLangExprPrimaryContextInitializerIntLiteral()
{
	int LocalValue = 41;
	const int ConstValue = 41;
	FPrimaryValue ValueObject(41);
	FRefRoot Object = MakeRefRoot(41);
	FRefRoot RootObject = MakeRefRoot(41);
	FRefDerived DerivedObject = MakeRefDerived(41);
	FRefRoot BaseView = MakeRefDerivedAsRoot(41);
	int Result = 41;
	return ObservePrimary(Result);
}

int EntryLangExprPrimaryContextAssignmentLhsLocalIdentifier()
{
	int LocalValue = 41;
	const int ConstValue = 41;
	FPrimaryValue ValueObject(41);
	FRefRoot Object = MakeRefRoot(41);
	FRefRoot RootObject = MakeRefRoot(41);
	FRefDerived DerivedObject = MakeRefDerived(41);
	FRefRoot BaseView = MakeRefDerivedAsRoot(41);
	LocalValue = 51;
	return LocalValue == 51 ? 1 : 0;
}

int EntryLangExprPrimaryContextReturnIntLiteral()
{
	return ObservePrimary(ReturnPrimary_EntryLangExprPrimaryContextReturnIntLiteral());
}

int EntryLangExprPrimaryContextConditionIntLiteral()
{
	int LocalValue = 41;
	const int ConstValue = 41;
	FPrimaryValue ValueObject(41);
	FRefRoot Object = MakeRefRoot(41);
	FRefRoot RootObject = MakeRefRoot(41);
	FRefDerived DerivedObject = MakeRefDerived(41);
	FRefRoot BaseView = MakeRefDerivedAsRoot(41);
	if (ObservePrimary(41) == 1)
	{
		return 1;
	}
	return 0;
}

int EntryLangExprPrimaryContextSwitchSelectorIntLiteral()
{
	int LocalValue = 41;
	const int ConstValue = 41;
	FPrimaryValue ValueObject(41);
	FRefRoot Object = MakeRefRoot(41);
	FRefRoot RootObject = MakeRefRoot(41);
	FRefDerived DerivedObject = MakeRefDerived(41);
	FRefRoot BaseView = MakeRefDerivedAsRoot(41);
	switch (41)
	{
	case 41:
		return 1;
	default:
		return 0;
	}
}

int EntryLangExprPrimaryContextAssignmentLhsIntLiteral()
{
	int LocalValue = 41;
	const int ConstValue = 41;
	FPrimaryValue ValueObject(41);
	FRefRoot Object = MakeRefRoot(41);
	FRefRoot RootObject = MakeRefRoot(41);
	FRefDerived DerivedObject = MakeRefDerived(41);
	FRefRoot BaseView = MakeRefDerivedAsRoot(41);
	41 = 51;
	return 0;
}

int EntryLangExprPrimaryContextAssignmentRhsIntLiteral()
{
	int LocalValue = 41;
	const int ConstValue = 41;
	FPrimaryValue ValueObject(41);
	FRefRoot Object = MakeRefRoot(41);
	FRefRoot RootObject = MakeRefRoot(41);
	FRefDerived DerivedObject = MakeRefDerived(41);
	FRefRoot BaseView = MakeRefDerivedAsRoot(41);
	int Result;
	Result = 41;
	return ObservePrimary(Result);
}

int EntryLangExprPrimaryContextArgumentIntLiteral()
{
	int LocalValue = 41;
	const int ConstValue = 41;
	FPrimaryValue ValueObject(41);
	FRefRoot Object = MakeRefRoot(41);
	FRefRoot RootObject = MakeRefRoot(41);
	FRefDerived DerivedObject = MakeRefDerived(41);
	FRefRoot BaseView = MakeRefDerivedAsRoot(41);
	return ObservePrimary(41);
}

int EntryLangExprPrimaryContextLoopClauseIntLiteral()
{
	int LocalValue = 41;
	const int ConstValue = 41;
	FPrimaryValue ValueObject(41);
	FRefRoot Object = MakeRefRoot(41);
	FRefRoot RootObject = MakeRefRoot(41);
	FRefDerived DerivedObject = MakeRefDerived(41);
	FRefRoot BaseView = MakeRefDerivedAsRoot(41);
	int Count = 0;
	while (Count < 1 && (ObservePrimary(41) == 1))
	{
		++Count;
	}
	return Count == 1 ? 1 : 0;
}

int EntryLangExprPrimaryContextIndexIntLiteral()
{
	int LocalValue = 41;
	const int ConstValue = 41;
	FPrimaryValue ValueObject(41);
	FRefRoot Object = MakeRefRoot(41);
	FRefRoot RootObject = MakeRefRoot(41);
	FRefDerived DerivedObject = MakeRefDerived(41);
	FRefRoot BaseView = MakeRefDerivedAsRoot(41);
	return Object.GetItem(41) == 82 ? 1 : 0;
}

int EntryLangExprPrimaryContextPropertyAccessorIntLiteral()
{
	int LocalValue = 41;
	const int ConstValue = 41;
	FPrimaryValue ValueObject(41);
	FRefRoot Object = MakeRefRoot(41);
	FRefRoot RootObject = MakeRefRoot(41);
	FRefDerived DerivedObject = MakeRefDerived(41);
	FRefRoot BaseView = MakeRefDerivedAsRoot(41);
	Object.SetVirtualValue(41);
	return Object.GetVirtualValue() == 41 ? 1 : 0;
}
