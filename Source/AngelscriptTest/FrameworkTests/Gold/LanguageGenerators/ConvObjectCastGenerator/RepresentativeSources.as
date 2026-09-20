int EntryLangConvObjectCastBaseBaseBaseAssignment()
{
	FObjectCastBase RuntimeValue = MakeObjectCastBase();
	FObjectCastBase SourceValue = RuntimeValue;
	FObjectCastBase TargetValue;
	TargetValue = SourceValue;
	if (TargetValue == nullptr)
	{
		return -1;
	}
	return TargetValue.GetIdentity() == SourceValue.GetIdentity() ? TargetValue.GetField() : -2;
}

int EntryLangConvObjectCastDerivedBaseDerivedInitializer()
{
	FObjectCastBase RuntimeValue = MakeObjectCastDerived();
	FObjectCastBase SourceValue = RuntimeValue;
	FObjectCastDerived TargetValue = SourceValue;
	if (TargetValue == nullptr)
	{
		return -1;
	}
	return TargetValue.GetIdentity() == SourceValue.GetIdentity() ? TargetValue.GetField() : -2;
}

int EntryLangConvObjectCastNullDerivedBaseArgument()
{
	FObjectCastBase RuntimeValue = MakeObjectCastNull();
	FObjectCastDerived SourceValue = cast<FObjectCastDerived>(RuntimeValue);
	return ObserveObjectCastTarget(SourceValue);
}

int EntryLangConvObjectCastUnrelatedUnrelatedBaseReturn()
{
	FObjectCastBase RuntimeValue = MakeObjectCastUnrelated();
	FObjectCastUnrelated SourceValue = cast<FObjectCastUnrelated>(RuntimeValue);
	FObjectCastBase TargetValue = ReturnObjectCastBase(SourceValue);
	if (TargetValue == nullptr)
	{
		return -1;
	}
	return TargetValue.GetIdentity() == SourceValue.GetIdentity() ? TargetValue.GetField() : -2;
}

int EntryLangConvObjectCastBaseBaseDerivedExplicitCast()
{
	FObjectCastBase RuntimeValue = MakeObjectCastBase();
	FObjectCastBase SourceValue = RuntimeValue;
	FObjectCastDerived TargetValue = cast<FObjectCastDerived>(SourceValue);
	if (TargetValue == nullptr)
	{
		return -1;
	}
	return TargetValue.GetIdentity() == SourceValue.GetIdentity() ? TargetValue.GetField() : -2;
}

int EntryLangConvObjectCastBaseBaseDerivedAssignment()
{
	FObjectCastBase RuntimeValue = MakeObjectCastBase();
	FObjectCastBase SourceValue = RuntimeValue;
	FObjectCastDerived TargetValue;
	TargetValue = SourceValue;
	if (TargetValue == nullptr)
	{
		return -1;
	}
	return TargetValue.GetIdentity() == SourceValue.GetIdentity() ? TargetValue.GetField() : -2;
}
