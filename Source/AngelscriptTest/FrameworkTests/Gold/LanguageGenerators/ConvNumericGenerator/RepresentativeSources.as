int EntryLangConvNumericInt8Int8AssignmentZero()
{
	int8 SourceValue = 0;
	int8 Converted;
	Converted = SourceValue;
	int8 ExpectedValue = int8(0);
	return Converted == ExpectedValue ? 1 : 0;
}

int EntryLangConvNumericInt8IntInitializerOne()
{
	int8 SourceValue = 1;
	int Converted = SourceValue;
	int ExpectedValue = int(1);
	return Converted == ExpectedValue ? 1 : 0;
}

int EntryLangConvNumericIntUint8ArgumentNegative()
{
	int SourceValue = -1;
	uint8 Converted = PassNumericUint8(SourceValue);
	uint8 ExpectedValue = uint8(255);
	return Converted == ExpectedValue ? 1 : 0;
}

int EntryLangConvNumericFloat32Float64ReturnFractional()
{
	float SourceValue = 1.5f;
	double Converted = ReturnNumericDouble(SourceValue);
	double ExpectedValue = double(1.5);
	return Converted == ExpectedValue ? 1 : 0;
}

int EntryLangConvNumericInt8Int8PromotionOne()
{
	int8 SourceValue = 1;
	int8 Converted = SourceValue + int8(0);
	int8 ExpectedValue = int8(1);
	return Converted == ExpectedValue ? 1 : 0;
}

int EntryLangConvNumericInt16Int8ExplicitCastOne()
{
	int16 SourceValue = 1;
	int8 Converted = int8(SourceValue);
	int8 ExpectedValue = int8(1);
	return Converted == ExpectedValue ? 1 : 0;
}

int EntryLangConvNumericFloat32Int8AssignmentMin()
{
	float SourceValue = -3.402823466e+38f;
	int8 Converted;
	Converted = SourceValue;
	return 1;
}
