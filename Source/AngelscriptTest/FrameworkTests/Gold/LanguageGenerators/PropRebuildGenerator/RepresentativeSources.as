int EntryLangPropRebuildMetadataRebuildStoredSameSource()
{
	FStoredRebuildDerived_EntryLangPropRebuildMetadataRebuildStoredSameSource Receiver = FStoredRebuildDerived_EntryLangPropRebuildMetadataRebuildStoredSameSource();
	return int(Receiver.Stored);
}

int EntryLangPropRebuildRuntimeRebuildRegisteredSetterPresence()
{
	FRebuildRegisteredB@ Receiver = CreateFRebuildRegisteredB(73);
	Receiver.Value = 73;
	return ObserveFRebuildRegisteredB(Receiver);
}

int EntryLangPropRebuildRuntimeRebuildRegisteredIndexedValue()
{
	FRebuildIndexedA@ Receiver = CreateFRebuildIndexedA(29);
	int Index = int(1);
	return Receiver.Value[Index];
}

int EntryLangPropRebuildOldHandleCleanupSaveLoadStoredValue()
{
	FStoredRebuildDerived_EntryLangPropRebuildOldHandleCleanupSaveLoadStoredValue Receiver = FStoredRebuildDerived_EntryLangPropRebuildOldHandleCleanupSaveLoadStoredValue();
	return int(Receiver.Stored);
}

int EntryLangPropRebuildRuntimeRebuildRegisteredConstness()
{
	const FRebuildRegisteredB@ Receiver = CreateFRebuildRegisteredB(11);
	return Receiver.Value;
}
