int EntryLangRefSourceOpInitializeMutableNewLocal()
{
	return ExerciseEntryLangRefSourceOpInitializeMutableNewLocal(MakeRefRoot(11));
}

int EntryLangRefSourceOpAssignMutableField()
{
	FReferenceFieldOwner Owner;
	Owner.Field = MakeRefRoot(12);
	return ExerciseEntryLangRefSourceOpAssignMutableField(Owner.Field);
}

int EntryLangRefSourceOpPassConstInputParameter()
{
	FRefRoot ParameterSource = MakeRefRoot(13);
	return ExerciseEntryLangRefSourceOpPassConstInputParameter(ParameterSource);
}

int EntryLangRefSourceOpCastMutableDerivedView()
{
	return ExerciseEntryLangRefSourceOpCastMutableDerivedView(MakeRefDerived(16));
}

int EntryLangRefSourceOpMemberAccessMutableNull()
{
	return ExerciseEntryLangRefSourceOpMemberAccessMutableNull(nullptr);
}

int EntryLangRefSourceOpInitializeConstRemovalInvalidNewLocal()
{
	return ExerciseEntryLangRefSourceOpInitializeConstRemovalInvalidNewLocal(MakeRefRoot(11));
}
