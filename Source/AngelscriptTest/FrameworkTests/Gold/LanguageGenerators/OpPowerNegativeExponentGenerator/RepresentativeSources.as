int EntryLangOpPowerNegativeExponentIntIntConstantNegativeExponent()
{
	const int Result = int(2) ** int(-2); // POWER_CAUSE
	return int(Result);
}

int EntryLangOpPowerNegativeExponentIntIntMutableLvalueNegativeExponent()
{
	int Base = int(2);
	int Exponent = int(-2);
	const int Result = Base ** Exponent; // POWER_CAUSE
	return int(Result);
}

int EntryLangOpPowerNegativeExponentIntIntConstLvalueNegativeExponent()
{
	const int Base = int(2);
	const int Exponent = int(-2);
	const int Result = Base ** Exponent; // POWER_CAUSE
	return int(Result);
}

int EntryLangOpPowerNegativeExponentIntIntFunctionReturnNegativeExponent()
{
	const int Result = PowerBase_EntryLangOpPowerNegativeExponentIntIntFunctionReturnNegativeExponent() ** PowerExponent_EntryLangOpPowerNegativeExponentIntIntFunctionReturnNegativeExponent(); // POWER_CAUSE
	return int(Result);
}

int EntryLangOpPowerNegativeExponentUintIntConstantNegativeExponent()
{
	const uint Result = uint(2) ** int(-2); // POWER_CAUSE
	return int(Result);
}

int EntryLangOpPowerNegativeExponentIntFloat32ConstantNegativeExponent()
{
	const float32 Result = int(2) ** float32(-2); // POWER_CAUSE
	return int(Result);
}
