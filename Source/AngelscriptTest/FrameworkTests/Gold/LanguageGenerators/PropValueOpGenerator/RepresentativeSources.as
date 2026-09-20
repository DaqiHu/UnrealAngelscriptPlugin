int EntryLangPropValueOpDefaultReadMutableInt8()
{
	FStoredPropertyBase_EntryLangPropValueOpDefaultReadMutableInt8 Receiver = FStoredPropertyBase_EntryLangPropValueOpDefaultReadMutableInt8();
	int CoreResult = ObserveStoredProperty_EntryLangPropValueOpDefaultReadMutableInt8(Receiver.Stored);
	return CoreResult * 100 + Receiver.ReceiverMarker;
}

int EntryLangPropValueOpWriteMutableInt8()
{
	FStoredPropertyBase_EntryLangPropValueOpWriteMutableInt8 Receiver = FStoredPropertyBase_EntryLangPropValueOpWriteMutableInt8();
	Receiver.Stored = int8(29);
	int CoreResult = ObserveStoredProperty_EntryLangPropValueOpWriteMutableInt8(Receiver.Stored);
	return CoreResult * 100 + Receiver.ReceiverMarker;
}

int EntryLangPropValueOpCompoundWriteMutableInt8()
{
	FStoredPropertyBase_EntryLangPropValueOpCompoundWriteMutableInt8 Receiver = FStoredPropertyBase_EntryLangPropValueOpCompoundWriteMutableInt8();
	Receiver.Stored += int8(29);
	int CoreResult = ObserveStoredProperty_EntryLangPropValueOpCompoundWriteMutableInt8(Receiver.Stored);
	return CoreResult * 100 + Receiver.ReceiverMarker;
}

int EntryLangPropValueOpCopyMutableInt8()
{
	FStoredPropertyBase_EntryLangPropValueOpCopyMutableInt8 Receiver = FStoredPropertyBase_EntryLangPropValueOpCopyMutableInt8();
	int8 CopyValue = Receiver.Stored;
	CopyValue = int8(37);
	int CoreResult = ObserveStoredProperty_EntryLangPropValueOpCopyMutableInt8(Receiver.Stored) * 100 + ObserveStoredProperty_EntryLangPropValueOpCopyMutableInt8(CopyValue);
	return CoreResult * 100 + Receiver.ReceiverMarker;
}

int EntryLangPropValueOpReferenceMutationMutableInt8()
{
	FStoredPropertyBase_EntryLangPropValueOpReferenceMutationMutableInt8 Receiver = FStoredPropertyBase_EntryLangPropValueOpReferenceMutationMutableInt8();
	MutateStoredProperty_EntryLangPropValueOpReferenceMutationMutableInt8(Receiver.Stored);
	int CoreResult = ObserveStoredProperty_EntryLangPropValueOpReferenceMutationMutableInt8(Receiver.Stored);
	return CoreResult * 100 + Receiver.ReceiverMarker;
}

int EntryLangPropValueOpDefaultReadConstInt8()
{
	const FStoredPropertyBase_EntryLangPropValueOpDefaultReadConstInt8 Receiver = FStoredPropertyBase_EntryLangPropValueOpDefaultReadConstInt8();
	int CoreResult = ObserveStoredProperty_EntryLangPropValueOpDefaultReadConstInt8(Receiver.Stored);
	return CoreResult * 100 + Receiver.ReceiverMarker;
}

int EntryLangPropValueOpDefaultReadBaseViewInt8()
{
	FStoredPropertyDerived_EntryLangPropValueOpDefaultReadBaseViewInt8 DerivedReceiver = FStoredPropertyDerived_EntryLangPropValueOpDefaultReadBaseViewInt8();
	FStoredPropertyBase_EntryLangPropValueOpDefaultReadBaseViewInt8 Receiver = DerivedReceiver;
	int CoreResult = ObserveStoredProperty_EntryLangPropValueOpDefaultReadBaseViewInt8(Receiver.Stored);
	return CoreResult * 100 + Receiver.ReceiverMarker;
}

int EntryLangPropValueOpDefaultReadDerivedViewInt8()
{
	FStoredPropertyDerived_EntryLangPropValueOpDefaultReadDerivedViewInt8 Receiver = FStoredPropertyDerived_EntryLangPropValueOpDefaultReadDerivedViewInt8();
	int CoreResult = ObserveStoredProperty_EntryLangPropValueOpDefaultReadDerivedViewInt8(Receiver.Stored);
	return CoreResult * 100 + Receiver.ReceiverMarker;
}
