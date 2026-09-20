int EntryLangOpAssignmentLocalAssignInt()
{
	int Input = 4;
	int Source = 2;
	int Value = Input;
	Value = Source;
	int Result = Value;
	ObserveAssignment_int(Result, Value);
	return int(Result);
}

int EntryLangOpAssignmentLocalAddAssignInt()
{
	int Input = 4;
	int Source = 2;
	int Value = Input;
	Value += Source;
	int Result = Value;
	ObserveAssignment_int(Result, Value);
	return int(Result);
}

int EntryLangOpAssignmentLocalModuloAssignInt()
{
	int Input = 4;
	int Source = 2;
	int Value = Input;
	Value %= Source;
	int Result = Value;
	ObserveAssignment_int(Result, Value);
	return int(Result);
}

int EntryLangOpAssignmentLocalAssignBool()
{
	bool Input = false;
	bool Source = true;
	bool Value = Input;
	Value = Source;
	bool Result = Value;
	ObserveAssignment_bool(Result, Value);
	return Result ? 1 : 0;
}

int EntryLangOpAssignmentLocalPowerAssignFloat32()
{
	float32 Input = 4.0f;
	float32 Source = 2.0f;
	float32 Value = Input;
	Value **= Source;
	float32 Result = Value;
	ObserveAssignment_float32(Result, Value);
	return int(Result);
}

int EntryLangOpAssignmentFieldAssignInt()
{
	int Input = 4;
	int Source = 2;
	FAssignmentFieldOwner_int Owner;
	Owner.Value = Input;
	Owner.Value = Source;
	int Result = Owner.Value;
	ObserveAssignment_int(Result, Owner.Value);
	return int(Result);
}

int EntryLangOpAssignmentPropertyAssignInt()
{
	int Input = 4;
	int Source = 2;
	FAssignmentProperty_int Owner = MakeAssignmentProperty_int();
	int Result = Source;
	Owner.set_Value(Result);
	ObserveAssignment_int(Result, Owner.get_Value());
	return int(Result);
}

int EntryLangOpAssignmentPropertyAddAssignInt()
{
	int Input = 4;
	int Source = 2;
	FAssignmentProperty_int Owner = MakeAssignmentProperty_int();
	int Current = Owner.get_Value();
	Current += Source;
	int Result = Current;
	Owner.set_Value(Result);
	ObserveAssignment_int(Result, Owner.get_Value());
	return int(Result);
}

int EntryLangOpAssignmentAliasAssignInt()
{
	int Input = 4;
	int Source = 2;
	int Value = Input;
	int Result = ApplyAssignAlias_int(Value, Source);
	return int(Result);
}

int EntryLangOpAssignmentLocalShiftRightArithmeticAssignInt64()
{
	int64 Input = 4;
	int64 Source = 2;
	int64 Value = Input;
	Value >>>= Source;
	int64 Result = Value;
	ObserveAssignment_int64(Result, Value);
	return int(Result);
}
