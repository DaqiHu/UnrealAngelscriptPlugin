int EntryLangDtorPartialIndependentBeforeRootNormal()
{
	RecordDestructorPartialStage(0);
	if (ReachDestructorPartialBoundary(0))
	{
		return 0;
	}
	return -1;
}

int EntryLangDtorPartialIndependentRootStartedNormal()
{
	int RootStarted = 1;
	RecordDestructorPartialStage(1);
	if (ReachDestructorPartialBoundary(1))
	{
		return 1;
	}
	return -1;
}

int EntryLangDtorPartialIndependentFirstOwnedNormal()
{
	int RootStarted = 1;
	RecordDestructorPartialStage(1);
	FNativeCaseReference First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	if (ReachDestructorPartialBoundary(2))
	{
		return First.Value;
	}
	return -1;
}

int EntryLangDtorPartialIndependentCompleteNormal()
{
	int RootStarted = 1;
	RecordDestructorPartialStage(1);
	FNativeCaseReference First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	FNativeCaseReference Middle = CreateNativeCaseReference(202);
	RecordDestructorPartialStage(3);
	FNativeCaseReference Last = CreateNativeCaseReference(303);
	RecordDestructorPartialStage(4);
	int Completed = 1;
	RecordDestructorPartialStage(5);
	if (ReachDestructorPartialBoundary(5))
	{
		return First.Value + Middle.Value + Last.Value;
	}
	return -1;
}

int EntryLangDtorPartialNestedMemberBeforeRootNormal()
{
	RecordDestructorPartialStage(0);
	if (ReachDestructorPartialBoundary(0))
	{
		return 0;
	}
	return -1;
}

int EntryLangDtorPartialNestedMemberRootStartedNormal()
{
	FPartialNestedOwner Root = FPartialNestedOwner();
	RecordDestructorPartialStage(1);
	if (ReachDestructorPartialBoundary(1))
	{
		return 1;
	}
	return -1;
}

int EntryLangDtorPartialNestedMemberFirstOwnedNormal()
{
	FPartialNestedOwner Root = FPartialNestedOwner();
	RecordDestructorPartialStage(1);
	Root.First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	if (ReachDestructorPartialBoundary(2))
	{
		return Root.First.Value;
	}
	return -1;
}

int EntryLangDtorPartialNestedMemberCompleteNormal()
{
	FPartialNestedOwner Root = FPartialNestedOwner();
	RecordDestructorPartialStage(1);
	Root.First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	Root.Middle = CreateNativeCaseReference(202);
	RecordDestructorPartialStage(3);
	Root.Last = CreateNativeCaseReference(303);
	RecordDestructorPartialStage(4);
	int Completed = 1;
	RecordDestructorPartialStage(5);
	if (ReachDestructorPartialBoundary(5))
	{
		return Root.First.Value + Root.Middle.Value + Root.Last.Value;
	}
	return -1;
}

int EntryLangDtorPartialBaseDerivedBeforeRootNormal()
{
	RecordDestructorPartialStage(0);
	if (ReachDestructorPartialBoundary(0))
	{
		return 0;
	}
	return -1;
}

int EntryLangDtorPartialBaseDerivedRootStartedNormal()
{
	FPartialDerivedOwner Root = FPartialDerivedOwner();
	RecordDestructorPartialStage(1);
	if (ReachDestructorPartialBoundary(1))
	{
		return 1;
	}
	return -1;
}

int EntryLangDtorPartialBaseDerivedFirstOwnedNormal()
{
	FPartialDerivedOwner Root = FPartialDerivedOwner();
	RecordDestructorPartialStage(1);
	Root.First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	if (ReachDestructorPartialBoundary(2))
	{
		return Root.First.Value;
	}
	return -1;
}

int EntryLangDtorPartialBaseDerivedCompleteNormal()
{
	FPartialDerivedOwner Root = FPartialDerivedOwner();
	RecordDestructorPartialStage(1);
	Root.First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	Root.Middle = CreateNativeCaseReference(202);
	RecordDestructorPartialStage(3);
	Root.Last = CreateNativeCaseReference(303);
	RecordDestructorPartialStage(4);
	int Completed = 1;
	RecordDestructorPartialStage(5);
	if (ReachDestructorPartialBoundary(5))
	{
		return Root.First.Value + Root.Middle.Value + Root.Last.Value;
	}
	return -1;
}

int EntryLangDtorPartialCopyTransferBeforeRootNormal()
{
	RecordDestructorPartialStage(0);
	if (ReachDestructorPartialBoundary(0))
	{
		return 0;
	}
	return -1;
}

int EntryLangDtorPartialCopyTransferRootStartedNormal()
{
	FPartialBundle Source = FPartialBundle();
	RecordDestructorPartialStage(1);
	if (ReachDestructorPartialBoundary(1))
	{
		return 1;
	}
	return -1;
}

int EntryLangDtorPartialCopyTransferFirstOwnedNormal()
{
	FPartialBundle Source = FPartialBundle();
	RecordDestructorPartialStage(1);
	Source.First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	if (ReachDestructorPartialBoundary(2))
	{
		return Source.First.Value;
	}
	return -1;
}

int EntryLangDtorPartialCopyTransferCompleteNormal()
{
	FPartialBundle Source = FPartialBundle();
	RecordDestructorPartialStage(1);
	Source.First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	Source.Middle = CreateNativeCaseReference(202);
	RecordDestructorPartialStage(3);
	Source.Last = CreateNativeCaseReference(303);
	RecordDestructorPartialStage(4);
	RecordDestructorPartialTransferStart();
	FPartialBundle Target = Source;
	RecordDestructorPartialRelation(false, Source.First is Target.First, Source.Middle is Target.Middle, Source.Last is Target.Last);
	RecordDestructorPartialStage(5);
	if (ReachDestructorPartialBoundary(5))
	{
		return Source.First.Value + Source.Middle.Value + Source.Last.Value + Target.First.Value + Target.Middle.Value + Target.Last.Value;
	}
	return -1;
}

int EntryLangDtorPartialAssignmentTransferBeforeRootNormal()
{
	RecordDestructorPartialStage(0);
	if (ReachDestructorPartialBoundary(0))
	{
		return 0;
	}
	return -1;
}

int EntryLangDtorPartialAssignmentTransferRootStartedNormal()
{
	FPartialBundle Source = FPartialBundle();
	RecordDestructorPartialStage(1);
	if (ReachDestructorPartialBoundary(1))
	{
		return 1;
	}
	return -1;
}

int EntryLangDtorPartialAssignmentTransferFirstOwnedNormal()
{
	FPartialBundle Source = FPartialBundle();
	RecordDestructorPartialStage(1);
	Source.First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	if (ReachDestructorPartialBoundary(2))
	{
		return Source.First.Value;
	}
	return -1;
}

int EntryLangDtorPartialAssignmentTransferCompleteNormal()
{
	FPartialBundle Source = FPartialBundle();
	RecordDestructorPartialStage(1);
	Source.First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	Source.Middle = CreateNativeCaseReference(202);
	RecordDestructorPartialStage(3);
	Source.Last = CreateNativeCaseReference(303);
	RecordDestructorPartialStage(4);
	RecordDestructorPartialTransferStart();
	FPartialBundle Target;
	Target = Source;
	RecordDestructorPartialRelation(false, Source.First is Target.First, Source.Middle is Target.Middle, Source.Last is Target.Last);
	RecordDestructorPartialStage(5);
	if (ReachDestructorPartialBoundary(5))
	{
		return Source.First.Value + Source.Middle.Value + Source.Last.Value + Target.First.Value + Target.Middle.Value + Target.Last.Value;
	}
	return -1;
}

int EntryLangDtorPartialSelfAssignmentBeforeRootNormal()
{
	RecordDestructorPartialStage(0);
	if (ReachDestructorPartialBoundary(0))
	{
		return 0;
	}
	return -1;
}

int EntryLangDtorPartialSelfAssignmentRootStartedNormal()
{
	FPartialBundle Source = FPartialBundle();
	RecordDestructorPartialStage(1);
	if (ReachDestructorPartialBoundary(1))
	{
		return 1;
	}
	return -1;
}

int EntryLangDtorPartialSelfAssignmentFirstOwnedNormal()
{
	FPartialBundle Source = FPartialBundle();
	RecordDestructorPartialStage(1);
	Source.First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	if (ReachDestructorPartialBoundary(2))
	{
		return Source.First.Value;
	}
	return -1;
}

int EntryLangDtorPartialSelfAssignmentCompleteNormal()
{
	FPartialBundle Source = FPartialBundle();
	RecordDestructorPartialStage(1);
	Source.First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	Source.Middle = CreateNativeCaseReference(202);
	RecordDestructorPartialStage(3);
	Source.Last = CreateNativeCaseReference(303);
	RecordDestructorPartialStage(4);
	RecordDestructorPartialTransferStart();
	Source = Source;
	RecordDestructorPartialRelation(true, Source.First is Source.First, Source.Middle is Source.Middle, Source.Last is Source.Last);
	RecordDestructorPartialStage(5);
	if (ReachDestructorPartialBoundary(5))
	{
		return Source.First.Value + Source.Middle.Value + Source.Last.Value;
	}
	return -1;
}

int EntryLangDtorPartialReferenceAliasBeforeRootNormal()
{
	RecordDestructorPartialStage(0);
	if (ReachDestructorPartialBoundary(0))
	{
		return 0;
	}
	return -1;
}

int EntryLangDtorPartialReferenceAliasRootStartedNormal()
{
	FPartialReferenceOwner Source = FPartialReferenceOwner();
	RecordDestructorPartialStage(1);
	if (ReachDestructorPartialBoundary(1))
	{
		return 1;
	}
	return -1;
}

int EntryLangDtorPartialReferenceAliasFirstOwnedNormal()
{
	FPartialReferenceOwner Source = FPartialReferenceOwner();
	RecordDestructorPartialStage(1);
	Source.First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	if (ReachDestructorPartialBoundary(2))
	{
		return Source.First.Value;
	}
	return -1;
}

int EntryLangDtorPartialReferenceAliasCompleteNormal()
{
	FPartialReferenceOwner Source = FPartialReferenceOwner();
	RecordDestructorPartialStage(1);
	Source.First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	Source.Middle = CreateNativeCaseReference(202);
	RecordDestructorPartialStage(3);
	Source.Last = CreateNativeCaseReference(303);
	RecordDestructorPartialStage(4);
	RecordDestructorPartialTransferStart();
	FPartialReferenceOwner Alias = Source;
	RecordDestructorPartialRelation(Source is Alias, Source.First is Alias.First, Source.Middle is Alias.Middle, Source.Last is Alias.Last);
	RecordDestructorPartialStage(5);
	if (ReachDestructorPartialBoundary(5))
	{
		return Source.First.Value + Source.Middle.Value + Source.Last.Value + Alias.First.Value + Alias.Middle.Value + Alias.Last.Value;
	}
	return -1;
}

int EntryLangDtorPartialIndependentMiddleOwnedNormal()
{
	int RootStarted = 1;
	RecordDestructorPartialStage(1);
	FNativeCaseReference First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	FNativeCaseReference Middle = CreateNativeCaseReference(202);
	RecordDestructorPartialStage(3);
	if (ReachDestructorPartialBoundary(3))
	{
		return First.Value + Middle.Value;
	}
	return -1;
}

int EntryLangDtorPartialIndependentAllOwnedNormal()
{
	int RootStarted = 1;
	RecordDestructorPartialStage(1);
	FNativeCaseReference First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	FNativeCaseReference Middle = CreateNativeCaseReference(202);
	RecordDestructorPartialStage(3);
	FNativeCaseReference Last = CreateNativeCaseReference(303);
	RecordDestructorPartialStage(4);
	if (ReachDestructorPartialBoundary(4))
	{
		return First.Value + Middle.Value + Last.Value;
	}
	return -1;
}

int EntryLangDtorPartialIndependentBeforeRootException()
{
	RecordDestructorPartialStage(0);
	if (ReachDestructorPartialBoundary(0))
	{
		int Zero = 0;
		return 1 / Zero;
	}
	return -1;
}

int EntryLangDtorPartialCopyTransferCompleteException()
{
	FPartialBundle Source = FPartialBundle();
	RecordDestructorPartialStage(1);
	Source.First = CreateNativeCaseReference(101);
	RecordDestructorPartialStage(2);
	Source.Middle = CreateNativeCaseReference(202);
	RecordDestructorPartialStage(3);
	Source.Last = CreateNativeCaseReference(303);
	RecordDestructorPartialStage(4);
	RecordDestructorPartialTransferStart();
	FPartialBundle Target = Source;
	RecordDestructorPartialRelation(false, Source.First is Target.First, Source.Middle is Target.Middle, Source.Last is Target.Last);
	RecordDestructorPartialStage(5);
	if (ReachDestructorPartialBoundary(5))
	{
		int Zero = 0;
		return 1 / Zero;
	}
	return -1;
}
