int EntryLangFnArgSourceValueLiteral()
{
	return ProbeEntryLangFnArgSourceValueLiteral(7);
}

int EntryLangFnArgSourceInLocalLvalue()
{
	int Value = 7;
	return ProbeEntryLangFnArgSourceInLocalLvalue(Value);
}

int EntryLangFnArgSourceValueConstLocal()
{
	const int Value = 7;
	return ProbeEntryLangFnArgSourceValueConstLocal(Value);
}

int EntryLangFnArgSourceValueField()
{
	FHolder Holder;
	return ProbeEntryLangFnArgSourceValueField(Holder.Value);
}

int EntryLangFnArgSourceValueArithmeticExpression()
{
	return ProbeEntryLangFnArgSourceValueArithmeticExpression(3 + 4);
}

int EntryLangFnArgSourceValueNull()
{
	FNativeCaseReference NullValue = nullptr;
	return ProbeEntryLangFnArgSourceValueNull(NullValue);
}

int EntryLangFnArgSourceValueBaseView()
{
	FBase Value;
	Value.Value = 7;
	return ProbeEntryLangFnArgSourceValueBaseView(Value);
}
