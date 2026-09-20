int EntryLangRefResolutionMutableExactDirectMutableLvalue()
{
	FRefRoot Source = MakeRefRoot(31);
	return SelectReference(Source);
}

int EntryLangRefResolutionMutableExactHelperMutableLvalue()
{
	FRefRoot Source = MakeRefRoot(31);
	return InvokeThroughHelperEntryLangRefResolutionMutableExactHelperMutableLvalue(Source);
}

int EntryLangRefResolutionConstPairDirectConstLvalue()
{
	const FRefRoot Source = MakeRefRoot(32);
	return SelectReference(Source);
}

int EntryLangRefResolutionValueVsInDirectField()
{
	FResolutionOwner Owner;
	Owner.Field = MakeRefRoot(34);
	return SelectReference(Owner.Field);
}

int EntryLangRefResolutionReturnCovarianceDirectDerivedView()
{
	FRefDerived Source = MakeRefDerived(37);
	const FRefDerived Result = SelectReference(Source);
	return Result != nullptr && Result.GetKind() == 202
		&& SameReference(Result, Source)
		? 1
		: 0;
}

int EntryLangRefResolutionMissingCandidateDirectMutableLvalue()
{
	FRefRoot Source = MakeRefRoot(31);
	return SelectReference(Source);
}
