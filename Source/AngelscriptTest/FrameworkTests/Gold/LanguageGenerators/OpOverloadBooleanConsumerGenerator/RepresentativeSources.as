int EntryLangOpOverloadBooleanConsumerComparisonIntParameterAssignment()
{
	FOverloadedBooleanComparisonIntParameter Left(5);
	FOverloadedBooleanComparisonIntParameter Right(6);
	bool Result = Left == 6;
	return Result ? 107 : -1;
}

int EntryLangOpOverloadBooleanConsumerComparisonValueParameterReturn()
{
	FOverloadedBooleanComparisonValueParameter Left(5);
	FOverloadedBooleanComparisonValueParameter Right(6);
	return ReturnBoolMarker(Left == Right) ? 108 : -1;
}

int EntryLangOpOverloadBooleanConsumerComparisonIntParameterChain()
{
	FOverloadedBooleanComparisonIntParameter Left(5);
	FOverloadedBooleanComparisonIntParameter Right(6);
	return ChainBoolMarker(Left == 6 && true) ? 107 : -1;
}

int EntryLangOpOverloadBooleanConsumerComparisonAmbiguousInt8Condition()
{
	FOverloadedBooleanComparisonAmbiguousInt8 Left(5);
	FOverloadedBooleanComparisonAmbiguousInt8 Right(6);
	if (Left == int8(6)) // OP_OVERLOAD_OPERATION_CAUSE
	{
		return 202;
	}
	return -1;
}

int EntryLangOpOverloadBooleanConsumerComparisonMissingLessOverloadArgument()
{
	FOverloadedBooleanComparisonMissingLess Left(5);
	FOverloadedBooleanComparisonMissingLess Right(6);
	return ConsumeBoolMarker(Left < Right) ? 303 : -1; // OP_OVERLOAD_OPERATION_CAUSE
}
