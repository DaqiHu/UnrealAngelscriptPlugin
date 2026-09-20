int EntryLangConvEnumAliasEnumEnumAssignmentZero()
{
	EConversionEnum SourceValue = EConversionEnum::Zero;
	EConversionEnum Converted;
	Converted = SourceValue;
	EConversionEnum ActualValue = Converted;
	EConversionEnum ExpectedValue = EConversionEnum::Zero;
	return ActualValue == ExpectedValue ? 1 : 0;
}

int EntryLangConvEnumAliasEnumIntInitializerOne()
{
	EConversionEnum SourceValue = EConversionEnum::One;
	int Converted = SourceValue;
	int ActualValue = Converted;
	int ExpectedValue = 1;
	return ActualValue == ExpectedValue ? 1 : 0;
}

int EntryLangConvEnumAliasAliasInt8Int64ArgumentNearMax()
{
	AliasInt8 SourceValue = 126;
	int64 Converted = PassEnumAliasTargetInt64(SourceValue);
	int64 ActualValue = Converted;
	int64 ExpectedValue = 126;
	return ActualValue == ExpectedValue ? 1 : 0;
}

int EntryLangConvEnumAliasEnumFloat64ReturnZero()
{
	EConversionEnum SourceValue = EConversionEnum::Zero;
	float Converted = ReturnEnumAliasTargetEnumFloat64(SourceValue);
	float ActualValue = Converted;
	float ExpectedValue = 0;
	return ActualValue == ExpectedValue ? 1 : 0;
}

int EntryLangConvEnumAliasAliasUintUintPromotionNegative()
{
	AliasUInt SourceValue = -1;
	uint Converted = SourceValue + uint(0);
	uint ActualValue = Converted;
	uint ExpectedValue = 4294967295;
	return ActualValue == ExpectedValue ? 1 : 0;
}

int EntryLangConvEnumAliasAliasInt8EnumExplicitCastZero()
{
	AliasInt8 SourceValue = 0;
	EConversionEnum Converted = EConversionEnum(SourceValue);
	EConversionEnum ActualValue = Converted;
	EConversionEnum ExpectedValue = EConversionEnum::Zero;
	return ActualValue == ExpectedValue ? 1 : 0;
}

int EntryLangConvEnumAliasAliasIntEnumAssignmentZero()
{
	AliasInt SourceValue = 0;
	EConversionEnum Converted;
	Converted = SourceValue;
	EConversionEnum ActualValue = Converted;
	EConversionEnum ExpectedValue = EConversionEnum::Zero;
	return ActualValue == ExpectedValue ? 1 : 0;
}

int EntryLangConvEnumAliasEnumEnumPromotionZero()
{
	EConversionEnum SourceValue = EConversionEnum::Zero;
	EConversionEnum Converted = SourceValue + EConversionEnum(0);
	EConversionEnum ActualValue = Converted;
	EConversionEnum ExpectedValue = EConversionEnum::Zero;
	return ActualValue == ExpectedValue ? 1 : 0;
}
