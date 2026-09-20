int EntryLangOpOverloadAssignmentConsumerAssignmentIntParameterAssignment()
{
	FOverloadedAssignmentIntParameter Left(5);
	FOverloadedAssignmentIntParameter Right(6);
	Left = 6;
	FOverloadedAssignmentIntParameter Result = Left;
	return Result.Value;
}

int EntryLangOpOverloadAssignmentConsumerAssignmentValueParameterReturn()
{
	FOverloadedAssignmentValueParameter Left(5);
	FOverloadedAssignmentValueParameter Right(6);
	Left = Right;
	return ReturnValueMarker(Left);
}

int EntryLangOpOverloadAssignmentConsumerAssignmentIntParameterCondition()
{
	FOverloadedAssignmentIntParameter Left(5);
	FOverloadedAssignmentIntParameter Right(6);
	Left = 6;
	if ((Left).Value == 114)
	{
		return 114;
	}
	return -1;
}

int EntryLangOpOverloadAssignmentConsumerAssignmentAmbiguousInt8Assignment()
{
	FOverloadedAssignmentAmbiguousInt8 Left(5);
	FOverloadedAssignmentAmbiguousInt8 Right(6);
	FOverloadedAssignmentAmbiguousInt8 Result = Left = int8(6); // OP_OVERLOAD_OPERATION_CAUSE
	return Result.Value;
}

int EntryLangOpOverloadAssignmentConsumerAssignmentMissingAddAssignSwitchIndex()
{
	FOverloadedAssignmentMissingAddAssign Left(5);
	FOverloadedAssignmentMissingAddAssign Right(6);
	switch ((Left += Right).Value) // OP_OVERLOAD_OPERATION_CAUSE
	{
	case 307:
		return 307;
	default:
		return -1;
	}
}
