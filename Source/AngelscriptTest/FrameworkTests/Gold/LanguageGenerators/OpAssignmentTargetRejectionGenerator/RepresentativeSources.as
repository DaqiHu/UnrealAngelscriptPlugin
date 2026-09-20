int EntryLangOpAssignmentTargetRejectionConstInvalidAssignInt()
{
	int Input = 4;
	int Source = 2;
	const int Value = Input;
	Value = Source; // ASSIGNMENT_CAUSE
	return 0;
}

int EntryLangOpAssignmentTargetRejectionTemporaryInvalidAddAssignFloat32()
{
	float32 Input = 4.0f;
	float32 Source = 2.0f;
	MakeRejectedAssignmentTemporary(Input) += Source; // ASSIGNMENT_CAUSE
	return 0;
}

int EntryLangOpAssignmentTargetRejectionConstInvalidAssignBool()
{
	bool Input = false;
	bool Source = true;
	const bool Value = Input;
	Value = Source; // ASSIGNMENT_CAUSE
	return 0;
}
