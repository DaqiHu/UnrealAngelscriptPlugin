int EntryLangOpUnaryMutableLvaluePositiveIntZero()
{
	int Input = int(0);
	int Value = Input;
	return ObserveUnaryNumericType(+Value);
}

int EntryLangOpUnaryConstLvalueNegativeInt8One()
{
	int8 Input = int8(1);
	const int8 Value = Input;
	return ObserveUnaryNumericType(-Value);
}

int EntryLangOpUnaryTemporaryBitNotUint64NearMax()
{
	uint64 Input = uint64(18446744073709551614);
	return ObserveUnaryNumericType(~MakeUnaryTemporaryUint64(Input));
}

int EntryLangOpUnaryFieldPositiveFloat64Negative()
{
	float64 Input = float64(-3.0);
	FUnaryFieldOwnerFloat64 Owner;
	Owner.Value = Input;
	return ObserveUnaryNumericType(+Owner.Value);
}

int EntryLangOpUnaryAliasNegativeUintNearMin()
{
	uint Input = uint(1);
	uint Value = Input;
	return ObserveUnaryAliasTypeUint_NEGATIVE_UINT(Value);
}
