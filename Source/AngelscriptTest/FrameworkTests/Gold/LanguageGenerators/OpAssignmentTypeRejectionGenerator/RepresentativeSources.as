int EntryLangOpAssignmentTypeRejectionLocalAddAssignBool()
{
	bool Input = false;
	bool Source = true;
	bool Value = Input;
	Value += Source; // ASSIGNMENT_CAUSE
	return 0;
}

int EntryLangOpAssignmentTypeRejectionFieldPowerAssignInt()
{
	int Input = 4;
	int Source = 2;
	FRejectedAssignmentField Owner;
	Owner.Value = Input;
	Owner.Value **= Source; // ASSIGNMENT_CAUSE
	return 0;
}

int EntryLangOpAssignmentTypeRejectionPropertyAndAssignFloat32()
{
	float32 Input = 4.0f;
	float32 Source = 2.0f;
	FAssignmentProperty_float32 Owner = MakeAssignmentProperty_float32();
	Owner.Value &= Source; // ASSIGNMENT_CAUSE
	return 0;
}

int EntryLangOpAssignmentTypeRejectionAliasPowerAssignBool()
{
	bool Input = false;
	bool Source = true;
	bool Value = Input;
	Value **= Source; // ASSIGNMENT_CAUSE
	return 0;
}
