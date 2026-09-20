void RecordConstructorVisibilitySelection(int Marker)
{
}

int EntryLangCtorVisibilityExactOwnerDefault()
{
	FVisibilityTargetEXACTOWNERDEFAULT Owner = FVisibilityTargetEXACTOWNERDEFAULT();
	return Owner.ExerciseVisibility();
}

int EntryLangCtorVisibilityPromotionDerivedDefault()
{
	FVisibilityDerivedPROMOTIONDERIVEDDEFAULT Selected = FVisibilityDerivedPROMOTIONDERIVEDDEFAULT();
	return Selected.Value;
}

int EntryLangCtorVisibilityExplicitCastUnrelatedDefault()
{
	FVisibilityUnrelatedEXPLICITCASTUNRELATEDDEFAULT Caller = FVisibilityUnrelatedEXPLICITCASTUNRELATEDDEFAULT();
	return Caller.ExerciseVisibility();
}

int EntryLangCtorVisibilityExactGlobalDefault()
{
	FVisibilityTargetEXACTGLOBALDEFAULT Selected = FVisibilityTargetEXACTGLOBALDEFAULT(int(7));
	return Selected.Value;
}

int EntryLangCtorVisibilityExactOwnerProtected()
{
	FVisibilityTargetEXACTOWNERPROTECTED Owner = FVisibilityTargetEXACTOWNERPROTECTED();
	return Owner.ExerciseVisibility();
}

int EntryLangCtorVisibilityExactOwnerPrivate()
{
	FVisibilityTargetEXACTOWNERPRIVATE Owner = FVisibilityTargetEXACTOWNERPRIVATE();
	return Owner.ExerciseVisibility();
}

int EntryLangCtorVisibilityExactUnrelatedPrivate()
{
	FVisibilityUnrelatedEXACTUNRELATEDPRIVATE Caller = FVisibilityUnrelatedEXACTUNRELATEDPRIVATE();
	return Caller.ExerciseVisibility();
}

int EntryLangCtorVisibilityImplicitRejectedOwnerDefault()
{
	FVisibilityTargetIMPLICITREJECTEDOWNERDEFAULT Owner = FVisibilityTargetIMPLICITREJECTEDOWNERDEFAULT();
	return Owner.ExerciseVisibility();
}

int EntryLangCtorVisibilityAmbiguousOwnerDefault()
{
	FVisibilityTargetAMBIGUOUSOWNERDEFAULT Owner = FVisibilityTargetAMBIGUOUSOWNERDEFAULT();
	return Owner.ExerciseVisibility();
}

int EntryLangCtorVisibilityMissingGlobalDefault()
{
	FVisibilityTargetMISSINGGLOBALDEFAULT Selected = FVisibilityTargetMISSINGGLOBALDEFAULT(int(7));
	return Selected.Value;
}
