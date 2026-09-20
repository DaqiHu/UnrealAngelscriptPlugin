int EntryLangRefDirectionValueNonNullSameTwoNames()
{
	FRefRoot Source = MakeRefRoot(21);
	FRefRoot Target = Source;
	CaptureDirectionSnapshot(0, Source);
	CaptureDirectionSnapshot(1, Target);
	ApplyEntryLangRefDirectionValueNonNullSameTwoNames(Target);
	CaptureDirectionSnapshot(4, Source);
	CaptureDirectionSnapshot(5, Target);
	CaptureDirectionSnapshot(6, Source);
	CaptureDirectionSnapshot(7, Target);
	return 1;
}

int EntryLangRefDirectionInNullInputDistinct()
{
	FRefRoot Source = MakeRefRoot(22);
	FRefRoot Target = nullptr;
	CaptureDirectionSnapshot(0, Source);
	CaptureDirectionSnapshot(1, Target);
	ApplyEntryLangRefDirectionInNullInputDistinct(Target);
	CaptureDirectionSnapshot(4, Source);
	CaptureDirectionSnapshot(5, Target);
	CaptureDirectionSnapshot(6, Source);
	CaptureDirectionSnapshot(7, Target);
	return 1;
}

int EntryLangRefDirectionOutNullOutputOutReplacement()
{
	FRefRoot Source = MakeRefRoot(25);
	FRefRoot Target = Source;
	CaptureDirectionSnapshot(0, Source);
	CaptureDirectionSnapshot(1, Target);
	ApplyEntryLangRefDirectionOutNullOutputOutReplacement(Target);
	CaptureDirectionSnapshot(4, Source);
	CaptureDirectionSnapshot(5, Target);
	CaptureDirectionSnapshot(6, Source);
	CaptureDirectionSnapshot(7, Target);
	return 1;
}

int EntryLangRefDirectionInoutNullReturnInoutMutation()
{
	FRefRoot Source = MakeRefRoot(26);
	FRefRoot Target = Source;
	CaptureDirectionSnapshot(0, Source);
	CaptureDirectionSnapshot(1, Target);
	ApplyEntryLangRefDirectionInoutNullReturnInoutMutation(Target);
	CaptureDirectionSnapshot(4, Source);
	CaptureDirectionSnapshot(5, Target);
	CaptureDirectionSnapshot(6, Source);
	CaptureDirectionSnapshot(7, Target);
	return 1;
}

int EntryLangRefDirectionValueNonNullBaseDerivedViews()
{
	FRefDerived Derived = MakeRefDerived(24);
	FRefRoot Source = Derived.opImplCast();
	FRefRoot Target = Source;
	CaptureDirectionSnapshot(0, Source);
	CaptureDirectionSnapshot(1, Target);
	ApplyEntryLangRefDirectionValueNonNullBaseDerivedViews(Target);
	CaptureDirectionSnapshot(4, Source);
	CaptureDirectionSnapshot(5, Target);
	CaptureDirectionSnapshot(6, Source);
	CaptureDirectionSnapshot(7, Target);
	return 1;
}
