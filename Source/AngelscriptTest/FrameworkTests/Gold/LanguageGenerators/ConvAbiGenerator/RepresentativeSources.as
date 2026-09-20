int EntryLangConvAbiFloatFloat32Argument()
{
	float Value = float(3.25);
	return CallAbiLangConvAbiFloatFloat32Argument(Value);
}

int EntryLangConvAbiFloatFloat32Return()
{
	float Result = CallAbiLangConvAbiFloatFloat32Return();
	return Result == float(3.25) ? 1 : 0;
}

int EntryLangConvAbiFloatFloat32Property()
{
	float Input = float(6.5);
	AbiValueLangConvAbiFloatFloat32Property = Input;
	float Output = AbiValueLangConvAbiFloatFloat32Property;
	return Output == Input ? 1 : 0;
}

int EntryLangConvAbiDoubleFloat64Argument()
{
	double Value = double(3.25);
	return CallAbiLangConvAbiDoubleFloat64Argument(Value);
}
