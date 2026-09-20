int EntryLangOpOverloadIntegerConsumerUnaryMemberAssignment()
{
	FOverloadedIntegerUnaryMember Left(5);
	FOverloadedIntegerUnaryMember Right(6);
	int Result = -Left;
	return Result;
}

int EntryLangOpOverloadIntegerConsumerBinaryInt64PromotionReturn()
{
	FOverloadedIntegerBinaryInt64Promotion Left(5);
	FOverloadedIntegerBinaryInt64Promotion Right(6);
	return ReturnIntMarker(Left + int8(6));
}

int EntryLangOpOverloadIntegerConsumerIndexIntParameterSwitchIndex()
{
	FOverloadedIntegerIndexIntParameter Left(5);
	FOverloadedIntegerIndexIntParameter Right(6);
	int Index = Left[6];
	switch (Index)
	{
	case 109:
		return 109;
	default:
		return -1;
	}
}

int EntryLangOpOverloadIntegerConsumerConversionIntTargetCondition()
{
	FOverloadedIntegerConversionIntTarget Left(5);
	FOverloadedIntegerConversionIntTarget Right(6);
	if (int(Left) == 113)
	{
		return 113;
	}
	return -1;
}

int EntryLangOpOverloadIntegerConsumerBinaryAmbiguousInt8Assignment()
{
	FOverloadedIntegerBinaryAmbiguousInt8 Left(5);
	FOverloadedIntegerBinaryAmbiguousInt8 Right(6);
	int Result = Left + int8(6); // OP_OVERLOAD_OPERATION_CAUSE
	return Result;
}

int EntryLangOpOverloadIntegerConsumerConversionMissingTargetOverloadArgument()
{
	FOverloadedIntegerConversionMissingTarget Left(5);
	FOverloadedIntegerConversionMissingTarget Right(6);
	return ConsumeIntMarker(ConsumeMissingConversion(Left)); // OP_OVERLOAD_OPERATION_CAUSE
}
