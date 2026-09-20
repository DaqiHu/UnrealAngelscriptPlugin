void RecordConstructorFailureBegin(int Stage)
{
}

int EntryLangCtorOrderFailureNoneFlatMembersValues()
{
	FCtorFailGraphNONEFLATMEMBERS Graph = FCtorFailGraphNONEFLATMEMBERS();
	return Graph.Total();
}

int EntryLangCtorOrderFailureNoneNestedMembersValues()
{
	FCtorFailGraphNONENESTEDMEMBERS Graph = FCtorFailGraphNONENESTEDMEMBERS();
	return Graph.Total();
}

int EntryLangCtorOrderFailureNoneDeepNestedMembersValues()
{
	FCtorFailGraphNONEDEEPNESTEDMEMBERS Graph = FCtorFailGraphNONEDEEPNESTEDMEMBERS();
	return Graph.Total();
}

int EntryLangCtorOrderFailureNoneBaseAndDerivedMembersValues()
{
	FCtorFailGraphNONEBASEANDDERIVEDMEMBERS Graph = FCtorFailGraphNONEBASEANDDERIVEDMEMBERS();
	return Graph.Total();
}

int EntryLangCtorOrderFailureBaseFlatMembersValues()
{
	FCtorFailGraphBASEFLATMEMBERS Graph = FCtorFailGraphBASEFLATMEMBERS();
	return Graph.Total();
}

int EntryLangCtorOrderFailureMemberFirstFlatMembersValues()
{
	FCtorFailGraphMEMBERFIRSTFLATMEMBERS Graph = FCtorFailGraphMEMBERFIRSTFLATMEMBERS();
	return Graph.Total();
}

int EntryLangCtorOrderFailureCopyDeepNestedMembersValues()
{
	FCtorFailGraphCOPYDEEPNESTEDMEMBERS Graph = FCtorFailGraphCOPYDEEPNESTEDMEMBERS();
	return Graph.Total();
}

int EntryLangCtorOrderFailureConversionBaseAndDerivedMembersValues()
{
	FCtorFailGraphCONVERSIONBASEANDDERIVEDMEMBERS Graph = FCtorFailGraphCONVERSIONBASEANDDERIVEDMEMBERS();
	return Graph.Total();
}
