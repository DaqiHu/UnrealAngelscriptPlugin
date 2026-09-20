int EntryLangRefForkDerivedToBaseInref()
{
	FRefDerived Source = MakeRefDerived(16);
	return RequireRootInput(Source, Source) ? 1 : 0;
}
