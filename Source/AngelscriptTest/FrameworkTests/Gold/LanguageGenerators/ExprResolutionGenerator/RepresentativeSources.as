int EntryLangExprResolutionInitializerIdentifierExact()
{
	int ExactIdentifier = 101;
	FNativeCaseValue Result = FNativeCaseValue(RecordResolution(ExactIdentifier));
	return Result.Value;
}

int EntryLangExprResolutionArgumentCallExact()
{
	return ObserveResolution(ResolveExactCall());
}

int EntryLangExprResolutionReturnMemberExact()
{
	return ProduceResolution_EntryLangExprResolutionReturnMemberExact().Value;
}

int EntryLangExprResolutionInitializerIdentifierMissing()
{
	FNativeCaseValue Result = FNativeCaseValue(MissingIdentifier);
	return Result.Value;
}

int EntryLangExprResolutionAssignmentIdentifierExact()
{
	int ExactIdentifier = 101;
	FNativeCaseValue Result;
	Result = FNativeCaseValue(RecordResolution(ExactIdentifier));
	return Result.Value;
}

int EntryLangExprResolutionConditionCallExact()
{
	if ((ResolveExactCall()).Value == 102)
	{
		return 102;
	}
	return -1;
}

int EntryLangExprResolutionIndexMemberExact()
{
	FResolutionOwner ResolutionOwner;
	FResolutionIndexProbe Probe;
	return Probe[(FNativeCaseValue(RecordResolution(ResolutionOwner.ExactMember))).Value];
}

int EntryLangExprResolutionMemberReceiverScopedNameExact()
{
	return (FNativeCaseValue(RecordResolution(ResolutionExact::ExactScoped))).Value;
}

int EntryLangExprResolutionInitializerIdentifierNamespaceQualified()
{
	FNativeCaseValue Result = FNativeCaseValue(RecordResolution(ResolutionQualified::QualifiedIdentifier));
	return Result.Value;
}

int EntryLangExprResolutionInitializerIdentifierOverload()
{
	FNativeCaseValue OverloadIdentifier = ResolveIdentifierOverload(7);
	FNativeCaseValue Result = OverloadIdentifier;
	return Result.Value;
}

int EntryLangExprResolutionInitializerIdentifierConversion()
{
	FNativeCaseValue ConversionIdentifier = ResolveIdentifierConversion(int8(7));
	FNativeCaseValue Result = ConversionIdentifier;
	return Result.Value;
}

int EntryLangExprResolutionInitializerIdentifierAmbiguous()
{
	FNativeCaseValue Result = FNativeCaseValue(AmbiguousIdentifier());
	return Result.Value;
}

int EntryLangExprResolutionInitializerIdentifierInaccessible()
{
	FNativeCaseValue Result = FNativeCaseValue(HiddenIdentifier);
	return Result.Value;
}

int EntryLangExprResolutionInitializerIdentifierWrongType()
{
	FWrongResolutionValue WrongIdentifier;
	FNativeCaseValue Result = WrongIdentifier;
	return Result.Value;
}
