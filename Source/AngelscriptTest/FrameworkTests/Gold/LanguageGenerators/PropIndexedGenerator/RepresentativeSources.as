int EntryLangPropIndexedSameTypeInt8ReadMutable()
{
	const int CandidateMarker = 101;
	FIndexedProperty Receiver = CreateIndexedProperty();
	int8 Index = 3;
	return Receiver.Value[Index];
}

int EntryLangPropIndexedSameTypeInt8WriteMutable()
{
	const int CandidateMarker = 101;
	FIndexedProperty Receiver = CreateIndexedProperty();
	int8 Index = 3;
	Receiver.Value[Index] = 73;
	return ObserveIndexedProperty(Receiver);
}

int EntryLangPropIndexedAdjacentNumericInt8WriteMutable()
{
	const int CandidateMarker = 201;
	FIndexedProperty Receiver = CreateIndexedProperty();
	int8 Index = 3;
	Receiver.Value[Index] = 73;
	return ObserveIndexedProperty(Receiver);
}

int EntryLangPropIndexedSameTypeBoolReadMutable()
{
	const int CandidateMarker = 101;
	FIndexedProperty Receiver = CreateIndexedProperty();
	bool Index = true;
	return Receiver.Value[Index];
}

int EntryLangPropIndexedCrossFamilyInt8ReadConst()
{
	const int CandidateMarker = 301;
	const FIndexedProperty Receiver = CreateIndexedProperty();
	int8 Index = 3;
	return Receiver.Value[Index];
}
