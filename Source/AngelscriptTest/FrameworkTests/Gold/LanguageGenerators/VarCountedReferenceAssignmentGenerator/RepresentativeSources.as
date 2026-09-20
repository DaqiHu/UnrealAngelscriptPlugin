class FNativeCaseReference
{
	int Value;

	FNativeCaseReference()
	{
		Value = 0;
	}

	FNativeCaseReference(int InValue)
	{
		Value = InValue;
	}
}

FNativeCaseReference CreateNativeCaseReference(int Value)
{
	return FNativeCaseReference(Value);
}

FNativeCaseReference PassCountedReference(FNativeCaseReference Input)
{
	return Input;
}

int RaiseCountedReferenceException()
{
	int Zero = 0;
	return 1 / Zero;
}

int EntryLangVarCountedReferenceAssignmentFactoryLocal()
{
	FNativeCaseReference Source = CreateNativeCaseReference(410);
	FNativeCaseReference Value;
	Value = Source;
	return Value.Value;
}

int EntryLangVarCountedReferenceAssignmentOverwrite()
{
	FNativeCaseReference Value = CreateNativeCaseReference(420);
	Value = CreateNativeCaseReference(421);
	return Value.Value;
}

int EntryLangVarCountedReferenceAssignmentNullAssignment()
{
	FNativeCaseReference Value = CreateNativeCaseReference(430);
	Value = nullptr;
	if (Value == nullptr)
	{
		return 1;
	}
	return 0;
}

int EntryLangVarCountedReferenceAssignmentParameterReturn()
{
	FNativeCaseReference Value = CreateNativeCaseReference(440);
	FNativeCaseReference Result = PassCountedReference(Value);
	return Result.Value;
}

int EntryLangVarCountedReferenceAssignmentExceptionFrame()
{
	FNativeCaseReference Value = CreateNativeCaseReference(450);
	return RaiseCountedReferenceException() + Value.Value;
}

int EntryLangVarCountedReferenceAssignmentSaveLoad()
{
	FNativeCaseReference Source = CreateNativeCaseReference(410);
	FNativeCaseReference Value;
	Value = Source;
	return Value.Value;
}

int EntryLangVarCountedReferenceAssignmentParameterReturnSaveLoad()
{
	FNativeCaseReference Value = CreateNativeCaseReference(440);
	FNativeCaseReference Result = PassCountedReference(Value);
	return Result.Value;
}
