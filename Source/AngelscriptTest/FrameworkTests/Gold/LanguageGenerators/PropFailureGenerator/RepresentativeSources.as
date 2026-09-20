int EntryLangPropFailureNullReceiverDirectFreshModule()
{
	FPropertyFailure Receiver = nullptr;
	return Receiver.Value;
}

int EntryLangPropFailureThrowingGetterDirectFreshModule()
{
	FPropertyFailure Receiver = CreatePropertyFailure(31);
	int Observed = Receiver.Value;
	int Zero = 0;
	return 1 / Zero;
}

int EntryLangPropFailureThrowingSetterAlternatePathSameModuleOrContext()
{
	FPropertyFailure Receiver = CreatePropertyFailure(31);
	return InvokePropertyFailure(Receiver);
}

int EntryLangPropFailureRecursiveGetterDirectFreshModule()
{
	FPropertyFailure Receiver = CreatePropertyFailure(31);
	int Observed = Receiver.Value;
	int Zero = 0;
	return 1 / Zero;
}
