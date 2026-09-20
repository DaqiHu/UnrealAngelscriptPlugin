int EntryLangRefLifetimeIdentityOwnerLiveNone()
{
	FRefRoot Primary = nullptr;
	return Primary == nullptr
		? 0
		: Primary.GetIdentity();
}

int EntryLangRefLifetimeIdentityOwnerLiveOneAlias()
{
	FRefRoot Primary = MakeRefRoot(41);
	return Primary == nullptr
		? 0
		: Primary.GetIdentity();
}

int EntryLangRefLifetimeIdentityOwnerLiveMultipleAliases()
{
	FRefRoot Primary = MakeRefRoot(41);
	FRefRoot AliasA = Primary;
	FRefRoot AliasB = Primary;
	return Primary == nullptr
		? 0
		: Primary.GetIdentity();
}

int EntryLangRefLifetimeIdentityOwnerLiveCycle()
{
	FRefRoot Primary = MakeRefRoot(41);
	Primary.Link(Primary);
	Primary.ClearPeer();
	return Primary == nullptr
		? 0
		: Primary.GetIdentity();
}

int EntryLangRefLifetimeIdentityReturnedAliasNone()
{
	FRefRoot Primary = nullptr;
	FRefRoot Returned = Primary;
	return Returned == nullptr
		? 0
		: Returned.GetIdentity();
}

int EntryLangRefLifetimeIdentityGcCycleNone()
{
	return 1;
}

int EntryLangRefLifetimeIdentityModuleRetainedNone()
{
	return GMutableObservation;
}
