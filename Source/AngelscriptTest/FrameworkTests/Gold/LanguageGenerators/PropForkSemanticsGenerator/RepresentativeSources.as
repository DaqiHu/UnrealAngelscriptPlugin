int EntryLangPropForkSemanticsDirectMethodReadWriteIndexed()
{
	FPropertyForkCarrier Receiver = CreatePropertyForkCarrier(31);
	Receiver.SetValue(41);
	Receiver.SetIndexedValue(2, 43);
	return Receiver.GetValue() + Receiver.GetIndexedValue(2);
}
