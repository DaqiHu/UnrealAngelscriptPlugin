int EntryLangConvNonfinitePreconversionFloat32Int8AssignmentPositiveInfinity()
{
	float Unit = 1.0;
	float Zero = 0.0;
	float SourceValue = Unit / Zero;
	int8 Converted;
	Converted = SourceValue;
	return 0;
}

int EntryLangConvNonfinitePreconversionFloat32Int8ArgumentNegativeInfinity()
{
	float Unit = 1.0;
	float Zero = 0.0;
	float SourceValue = -Unit / Zero;
	int8 Converted = PassNumericInt8(SourceValue);
	return 0;
}

int EntryLangConvNonfinitePreconversionFloat32Int8ReturnNan()
{
	float Unit = 1.0;
	float Zero = 0.0;
	float SourceValue = Zero / Zero;
	int8 Converted = ReturnNumericInt8(SourceValue);
	return 0;
}

int EntryLangConvNonfinitePreconversionFloat64Float32ExplicitCastPositiveInfinity()
{
	double Unit = 1.0;
	double Zero = 0.0;
	double SourceValue = Unit / Zero;
	float Converted = float(SourceValue);
	return 0;
}

int EntryLangConvNonfinitePreconversionFloat32Uint64AssignmentNan()
{
	float Unit = 1.0;
	float Zero = 0.0;
	float SourceValue = Zero / Zero;
	uint64 Converted;
	Converted = SourceValue;
	return 0;
}
