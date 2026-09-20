int EntryLangOpIncrementTargetRejectionConstInvalidPreIncrementInt()
{
	int Input = 12;
	const int Value = Input;
	++Value; // INCREMENT_CAUSE
	return 0;
}

int EntryLangOpIncrementTargetRejectionTemporaryInvalidPostDecrementFloat32()
{
	float32 Input = 12;
	MakeRejectedIncrementTemporary(Input)--; // INCREMENT_CAUSE
	return 0;
}
