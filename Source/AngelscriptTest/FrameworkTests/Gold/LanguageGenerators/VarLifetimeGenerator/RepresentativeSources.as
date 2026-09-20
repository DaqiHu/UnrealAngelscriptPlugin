int EntryLangVarLifetimeBlockEndOneLocalValue()
{
	int Trace = 0;
	for (int TransferLoop = 0; TransferLoop < 1; ++TransferLoop)
	{
		FNativeCaseValue local_valueOwner0(110);
		Trace += local_valueOwner0.Value;
	}
	return 71 + Trace - Trace;
}

int EntryLangVarLifetimeReturnSequentialNestedValue()
{
	int Trace = 0;
	for (int TransferLoop = 0; TransferLoop < 1; ++TransferLoop)
	{
		FNestedLifetimeValue nested_valueOwner0;
		nested_valueOwner0.Tracked.Value = 210;
		Trace += nested_valueOwner0.Tracked.Value;
		FNestedLifetimeValue nested_valueOwner1;
		nested_valueOwner1.Tracked.Value = 211;
		Trace += nested_valueOwner1.Tracked.Value;
		return 72 + Trace - Trace;
	}
	return 72 + Trace - Trace;
}

int EntryLangVarLifetimeBreakNestedScopesField()
{
	int Trace = 0;
	for (int TransferLoop = 0; TransferLoop < 1; ++TransferLoop)
	{
		FFieldLifetimeOwner fieldOwner0 = FFieldLifetimeOwner();
		fieldOwner0.Tracked.Value = 310;
		Trace += fieldOwner0.Tracked.Value;
		{
			FFieldLifetimeOwner fieldOwner1 = FFieldLifetimeOwner();
			fieldOwner1.Tracked.Value = 311;
			Trace += fieldOwner1.Tracked.Value;
			break;
		}
	}
	return 73 + Trace - Trace;
}

int EntryLangVarLifetimeContinueLoopReference()
{
	int Trace = 0;
	for (int OwnerIteration = 0; OwnerIteration < 2; ++OwnerIteration)
	{
		FNativeCaseReference referenceOwner0 = CreateNativeCaseReference(410);
		Trace += referenceOwner0.Value;
		continue;
	}
	return 74 + Trace - Trace;
}

int EntryLangVarLifetimeExceptionNestedCallLocalValue()
{
	int Trace = 0;
	FNativeCaseValue local_valueOwner0(110);
	Trace += local_valueOwner0.Value;
	int NestedResult = NestedEntryLangVarLifetimeExceptionNestedCallLocalValue();
	return NestedResult + Trace - Trace;
}
