int EntryLangOpIncrementLocalBeforePreIncrementInt()
{
	int Input = 12;
	int Value = Input;
	int Before = Value;
	++Value;
	return int(Before);
}

int EntryLangOpIncrementFieldExpressionResultPostIncrementInt8()
{
	int8 Input = 12;
	FIncrementFieldOwner_int8 Owner;
	Owner.Value = Input;
	int8 Result = Owner.Value++;
	return int(Result);
}

int EntryLangOpIncrementPropertyAfterPreDecrementFloat32()
{
	float32 Input = 12.5;
	FIncrementProperty_float32 Owner = MakeIncrementProperty_float32();
	float32 Before = Owner.get_Value();
	float32 Updated = Before;
	--Updated;
	Owner.set_Value(Updated);
	return int(Owner.get_Value());
}

int EntryLangOpIncrementAliasBeforePostDecrementUint()
{
	uint Input = 12;
	uint Value = Input;
	uint Before = Value;
	ApplyPostDecrementAlias_uint(Value);
	return int(Before);
}
