int EntryLangPropAccessorGetterReadMutableSingleLine()
{
	FRegisteredProperty Receiver = CreateRegisteredProperty(31);
	return Receiver.Value;
}

int EntryLangPropAccessorGetterReadMutableMultiline()
{
	FRegisteredProperty Receiver = CreateRegisteredProperty(31);
	int Observed =
		Receiver.Value;
	return Observed;
}

int EntryLangPropAccessorGetterReadMutableParenthesized()
{
	FRegisteredProperty Receiver = CreateRegisteredProperty(31);
	return (((Receiver.Value)));
}

int EntryLangPropAccessorGetterReadMutableHelperCall()
{
	FRegisteredProperty Receiver = CreateRegisteredProperty(31);
	return InvokeRegisteredProperty_EntryLangPropAccessorGetterReadMutableHelperCall(Receiver);
}

int EntryLangPropAccessorSetterWriteMutableSingleLine()
{
	FRegisteredProperty Receiver = CreateRegisteredProperty(31);
	Receiver.Value = 73;
	return ObserveRegisteredProperty(Receiver);
}

int EntryLangPropAccessorBothCompoundMutableSingleLine()
{
	FRegisteredProperty Receiver = CreateRegisteredProperty(31);
	Receiver.Value += 5;
	return ObserveRegisteredProperty(Receiver);
}

int EntryLangPropAccessorThrowingGetterSingleLine()
{
	FRegisteredProperty Receiver = CreateRegisteredProperty(31);
	int Observed = Receiver.Value;
	int Zero = 0;
	return 1 / Zero;
}

int EntryLangPropAccessorGetterReadConstSingleLine()
{
	const FRegisteredProperty Receiver = CreateRegisteredProperty(31);
	return Receiver.Value;
}
