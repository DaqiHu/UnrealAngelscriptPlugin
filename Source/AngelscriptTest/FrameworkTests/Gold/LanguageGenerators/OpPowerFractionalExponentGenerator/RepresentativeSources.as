int EntryLangOpPowerFractionalExponentIntFloat32ConstantFractionalExponent()
{
	return ObserveNumericType(int(4) ** float32(0.5));
}

int EntryLangOpPowerFractionalExponentInt8Float64MutableLvalueFractionalExponent()
{
	int8 Base = int8(4);
	float64 Exponent = float64(0.5);
	return ObserveNumericType(Base ** Exponent);
}

int EntryLangOpPowerFractionalExponentUintFloat32ConstLvalueFractionalExponent()
{
	const uint Base = uint(4);
	const float32 Exponent = float32(0.5);
	return ObserveNumericType(Base ** Exponent);
}

int EntryLangOpPowerFractionalExponentFloat64Float32FunctionReturnFractionalExponent()
{
	return ObserveNumericType(PowerBaseValueFLOAT64FLOAT32() ** PowerExponentValueFLOAT64FLOAT32());
}
