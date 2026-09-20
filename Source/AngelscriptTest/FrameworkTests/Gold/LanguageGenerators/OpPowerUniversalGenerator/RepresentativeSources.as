int EntryLangOpPowerUniversalIntIntConstantZeroExponent()
{
	const int Result = int(2) ** int(0); // POWER_CAUSE
	return int(Result);
}

int EntryLangOpPowerUniversalIntIntConstantOneExponent()
{
	const int Result = int(2) ** int(1); // POWER_CAUSE
	return int(Result);
}

int EntryLangOpPowerUniversalIntFloat32ConstantZeroExponent()
{
	const float32 Result = int(2) ** float32(0); // POWER_CAUSE
	return int(Result);
}

int EntryLangOpPowerUniversalInt8Uint8ConstLvalueOverflow()
{
	const int8 Base = int8(2);
	const uint8 Exponent = uint8(31);
	const int Result = Base ** Exponent; // POWER_CAUSE
	return int(Result);
}

int EntryLangOpPowerUniversalFloat64IntMutableLvalueOverflow()
{
	float64 Base = float64(1.7976931348623157e+308);
	int Exponent = int(2);
	const float64 Result = Base ** Exponent; // POWER_CAUSE
	return int(Result);
}

int EntryLangOpPowerUniversalIntIntConstantOverflow()
{
	const int Result = int(2) ** int(31); // POWER_CAUSE
	return int(Result);
}
