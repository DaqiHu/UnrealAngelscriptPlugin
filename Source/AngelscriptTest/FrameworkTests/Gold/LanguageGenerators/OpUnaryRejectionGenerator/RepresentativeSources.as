int EntryLangOpUnaryRejectionMutableLvaluePositiveBool()
{
	bool Input = false;
	bool Value = Input;
	return int(+Value); // UNARY_CAUSE
}

int EntryLangOpUnaryRejectionTemporaryLogicalNotSigned()
{
	int Input = 0;
	return int(!MakeInvalidUnaryTemporary(Input)); // UNARY_CAUSE
}

int EntryLangOpUnaryRejectionFieldBitNotFloat32()
{
	float32 Input = 0.0;
	FInvalidUnaryOwner Owner;
	Owner.Value = Input;
	return int(~Owner.Value); // UNARY_CAUSE
}

int EntryLangOpUnaryRejectionAliasNegativeBool()
{
	bool Input = false;
	return int(-Input); // UNARY_CAUSE
}
