int ObserveLogicalNotType(bool Value)
{
	return 301;
}

int ObserveLogicalNotType(int Value)
{
	return 399;
}

bool MakeLogicalNotTemporary(bool Input)
{
	return Input;
}

struct FLogicalNotOwner
{
	bool Value;
}

bool ApplyLogicalNotAlias(bool& in Value)
{
	return !ObserveNotOperand(Value);
}

int EntryLangOpLogicalNotMutableLvalueFalse()
{
	bool Input = false;
	bool Value = Input;
	const bool Result = !ObserveNotOperand(Value);
	return Result ? 1 : 0;
}

int EntryLangOpLogicalNotMutableLvalueTrue()
{
	bool Input = true;
	bool Value = Input;
	const bool Result = !ObserveNotOperand(Value);
	return Result ? 1 : 0;
}

int EntryLangOpLogicalNotConstLvalueFalse()
{
	bool Input = false;
	const bool Value = Input;
	const bool Result = !ObserveNotOperand(Value);
	return Result ? 1 : 0;
}

int EntryLangOpLogicalNotTemporaryTrue()
{
	bool Input = true;
	const bool Result = !MakeLogicalNotTemporary(ObserveNotOperand(Input));
	return Result ? 1 : 0;
}

int EntryLangOpLogicalNotFieldFalse()
{
	bool Input = false;
	FLogicalNotOwner Owner;
	Owner.Value = Input;
	const bool Result = !ObserveNotOperand(Owner.Value);
	return Result ? 1 : 0;
}

int EntryLangOpLogicalNotAliasTrue()
{
	bool Input = true;
	bool Value = Input;
	const bool Result = ApplyLogicalNotAlias(Value);
	return Result ? 1 : 0;
}
