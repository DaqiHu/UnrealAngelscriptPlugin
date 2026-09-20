int EntryLangOpIntegralBitwiseMutableLvalueBitAndZeroInt()
{
	int Input = int(0xA55AA55A);
	int Right = 0;
	int Value = Input;
	return int(Value & Right);
}

int EntryLangOpIntegralBitwiseConstLvalueShiftLeftOneInt8()
{
	int8 Input = int8(0xA5);
	int Right = 1;
	const int8 Value = Input;
	return int(Value << Right);
}

int EntryLangOpIntegralBitwiseTemporaryBitXorSourceWidthUint()
{
	uint Input = uint(0xA55AA55A);
	int Right = 32;
	uint Value = Input;
	return int(MakeBitwiseTemporary_uint(Input) ^ Right);
}

int EntryLangOpIntegralBitwiseFieldShiftRightLogicalNegativeInt64()
{
	int64 Input = int64(0xA55AA55AA55AA55A);
	int Right = -1;
	FBitwiseFieldOwner_int64 Owner;
	Owner.Value = Input;
	return int(Owner.Value >> Right);
}

int EntryLangOpIntegralBitwiseAliasShiftRightArithmeticLargeUint8()
{
	uint8 Input = uint8(0xA5);
	int Right = 100000;
	uint8 Value = Input;
	return int(ApplyShiftRightArithmeticAlias_uint8(Value, Right));
}
