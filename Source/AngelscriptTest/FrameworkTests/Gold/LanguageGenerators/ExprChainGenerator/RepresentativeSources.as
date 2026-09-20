int EntryLangExprChainInitializerTwoCallMemberValid()
{
	int Result = MakeChain().StepCall().GetMember().Value;
	return Result;
}

int EntryLangExprChainArgumentThreeMemberCallValid()
{
	return ObserveChain(MakeChain().GetMember().StepCall().GetMember().Value);
}

int EntryLangExprChainReturnTwoIndexMemberValid()
{
	return ProduceChainValue_EntryLangExprChainReturnTwoIndexMemberValid();
}

int EntryLangExprChainConditionTwoCastMemberValid()
{
	if (MakeChain().opCast().GetMember().Value == 110)
	{
		return 1;
	}
	return 0;
}

int EntryLangExprChainAssignmentTwoCallIndexCastValid()
{
	int Result = 0;
	Result = MakeChain().StepCall()[2].Value;
	return Result;
}

int EntryLangExprChainInitializerEightMemberCallIndexValid()
{
	int Result = MakeChain().GetMember().StepCall()[3].GetMember().StepCall()[6].GetMember().StepCall().Value;
	return Result;
}

int EntryLangExprChainInitializerTwoCallIndexCastValid()
{
	int Result = MakeChain().StepCall()[2].Value;
	return Result;
}

int EntryLangExprChainInitializerTwoCallMemberInvalidIntermediate()
{
	int Result = MakeChain().BreakToInt().GetMember().Value;
	return Result;
}

int EntryLangExprChainInitializerTwoCallMemberMissingTerminal()
{
	int Result = MakeChain().StepCall().GetMember().MissingTerminal;
	return Result;
}
