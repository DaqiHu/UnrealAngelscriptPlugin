int EntryLangConvFloatFiniteSpecialFloat32Int8AssignmentPositiveZero()
{
	float32 SourceValue = 0.0;
	int8 Converted;
	Converted = SourceValue;
	return int(Converted);
}

int EntryLangConvFloatFiniteSpecialFloat32Float32AssignmentNegativeZero()
{
	float32 SourceValue = -0.0;
	float32 Converted;
	Converted = SourceValue;
	return ObserveFloat32Bits(Converted);
}

int EntryLangConvFloatFiniteSpecialFloat32Int8AssignmentSubnormal()
{
	float32 SourceValue = 1.40129846e-45f;
	int8 Converted;
	Converted = SourceValue;
	return int(Converted);
}

int EntryLangConvFloatFiniteSpecialFloat64IntArgumentPositiveZero()
{
	float SourceValue = 0.0;
	int Converted = PassNumericBoundaryTargetInt(SourceValue);
	return int(Converted);
}

int EntryLangConvFloatFiniteSpecialFloat32Float64ReturnSubnormal()
{
	float32 SourceValue = 1.40129846e-45f;
	float Converted = ReturnNumericBoundaryTargetFloat32Float64(SourceValue);
	return ObserveFloat64Bits(Converted);
}

int EntryLangConvFloatFiniteSpecialFloat64Float32ExplicitCastSubnormal()
{
	float SourceValue = 4.9406564584124654e-324;
	float32 Converted = float32(SourceValue);
	return ObserveFloat32Bits(Converted);
}
