int EntryLangPropVisibilityOwnerMethodReadDefault()
{
	FVisibilityOwner_EntryLangPropVisibilityOwnerMethodReadDefault Receiver = FVisibilityOwner_EntryLangPropVisibilityOwnerMethodReadDefault();
	return Receiver.ProbeOwnerMethod();
}

int EntryLangPropVisibilityOwnerMethodWriteDefault()
{
	FVisibilityOwner_EntryLangPropVisibilityOwnerMethodWriteDefault Receiver = FVisibilityOwner_EntryLangPropVisibilityOwnerMethodWriteDefault();
	return Receiver.ProbeOwnerMethod();
}

int EntryLangPropVisibilityOwnerConstructorReadDefault()
{
	FVisibilityOwner_EntryLangPropVisibilityOwnerConstructorReadDefault Receiver = FVisibilityOwner_EntryLangPropVisibilityOwnerConstructorReadDefault();
	return Receiver.ConstructorObservation;
}

int EntryLangPropVisibilityOwnerDestructorReadDefault()
{
	{
		FVisibilityOwner_EntryLangPropVisibilityOwnerDestructorReadDefault Receiver = FVisibilityOwner_EntryLangPropVisibilityOwnerDestructorReadDefault();
	}
	return ReadPropertyVisibilityDestructor();
}

int EntryLangPropVisibilityDirectDerivedReadDefault()
{
	FVisibilityDerived_EntryLangPropVisibilityDirectDerivedReadDefault Receiver = FVisibilityDerived_EntryLangPropVisibilityDirectDerivedReadDefault();
	return Receiver.ProbeDirectDerived();
}

int EntryLangPropVisibilityGlobalSameModuleReadDefault()
{
	FVisibilityOwner_EntryLangPropVisibilityGlobalSameModuleReadDefault Receiver = FVisibilityOwner_EntryLangPropVisibilityGlobalSameModuleReadDefault();
	int Observed = Receiver.VisibleValue;
	return Observed;
}

int EntryLangPropVisibilityAccessorBodyWriteDefault()
{
	FVisibilityOwner_EntryLangPropVisibilityAccessorBodyWriteDefault Receiver = FVisibilityOwner_EntryLangPropVisibilityAccessorBodyWriteDefault();
	Receiver.ExplicitWriteAccessor();
	return Receiver.ReadVisibleValue();
}
