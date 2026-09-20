int EntryLangOpIncrementTypeRejectionLocalPreIncrement()
{
	bool Input = false;
	bool Value = Input;
	++Value; // INCREMENT_CAUSE
	return 0;
}

int EntryLangOpIncrementTypeRejectionFieldPostDecrement()
{
	bool Input = false;
	FRejectedBoolIncrementField Owner;
	Owner.Value = Input;
	Owner.Value--; // INCREMENT_CAUSE
	return 0;
}

int EntryLangOpIncrementTypeRejectionPropertyPreDecrement()
{
	FIncrementProperty_bool Owner = MakeIncrementProperty_bool();
	--Owner.Value; // INCREMENT_CAUSE
	return 0;
}

int EntryLangOpIncrementTypeRejectionAliasPostIncrement()
{
	bool Value = false;
	Value++; // INCREMENT_CAUSE
	return 0;
}
