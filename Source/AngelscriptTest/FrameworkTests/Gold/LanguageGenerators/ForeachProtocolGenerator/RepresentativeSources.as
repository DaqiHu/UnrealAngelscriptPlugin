int EntryLangFeProtocolCompleteExactSingle()
{
	FForeachProtocolRange_EntryLangFeProtocolCompleteExactSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}

int EntryLangFeProtocolCompleteExactSameIterable()
{
	FForeachProtocolRange_EntryLangFeProtocolCompleteExactSameIterable Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int OuterValue : Range)
	{
		Trace += OuterValue;
		foreach (int Value : Range)
		{
			Trace += Value;
		}
	}
	return Trace;
}

int EntryLangFeProtocolCompleteExactDistinctIterable()
{
	FForeachProtocolRange_EntryLangFeProtocolCompleteExactDistinctIterable Range;
	Range.Native.Count = 2;
	FForeachProtocolRange_EntryLangFeProtocolCompleteExactDistinctIterable OtherRange;
	OtherRange.Native.Count = 2;
	int Trace = 0;
	foreach (int OuterValue : Range)
	{
		Trace += OuterValue;
		foreach (int Value : OtherRange)
		{
			Trace += Value;
		}
	}
	return Trace;
}

int EntryLangFeProtocolCompleteExactInsideFor()
{
	FForeachProtocolRange_EntryLangFeProtocolCompleteExactInsideFor Range;
	Range.Native.Count = 2;
	int Trace = 0;
	for (int OuterIndex = 0; OuterIndex < 2; ++OuterIndex)
	{
		foreach (int Value : Range)
		{
			Trace += Value;
		}
	}
	return Trace;
}

int EntryLangFeProtocolCompleteExactContainsFor()
{
	FForeachProtocolRange_EntryLangFeProtocolCompleteExactContainsFor Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int Value : Range)
	{
		Trace += Value;
		for (int Index = 0; Index < 2; ++Index)
		{
			Trace += Value;
		}
	}
	return Trace;
}

int EntryLangFeProtocolCompleteConversionSingle()
{
	FForeachProtocolRange_EntryLangFeProtocolCompleteConversionSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int64 Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}

int EntryLangFeProtocolCompleteConstOverloadSingle()
{
	FForeachProtocolRange_EntryLangFeProtocolCompleteConstOverloadSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (const int Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}

int EntryLangFeProtocolCompleteAmbiguousSingle()
{
	FForeachProtocolRange_EntryLangFeProtocolCompleteAmbiguousSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}

int EntryLangFeProtocolOverloadedExactSingle()
{
	FForeachProtocolRange_EntryLangFeProtocolOverloadedExactSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}

int EntryLangFeProtocolThrowingExactSingle()
{
	FForeachProtocolRange_EntryLangFeProtocolThrowingExactSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	int Zero = 0;
	Trace += 1 / Zero;
	foreach (int Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}

int EntryLangFeProtocolThrowingAmbiguousSingle()
{
	FForeachProtocolRange_EntryLangFeProtocolThrowingAmbiguousSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}

int EntryLangFeProtocolMissingBeginExactSingle()
{
	FBrokenProtocol_EntryLangFeProtocolMissingBeginExactSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}

int EntryLangFeProtocolMissingNextExactSingle()
{
	FBrokenProtocol_EntryLangFeProtocolMissingNextExactSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}

int EntryLangFeProtocolMissingValueExactSingle()
{
	FBrokenProtocol_EntryLangFeProtocolMissingValueExactSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}

int EntryLangFeProtocolMissingEndExactSingle()
{
	FBrokenProtocol_EntryLangFeProtocolMissingEndExactSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}

int EntryLangFeProtocolWrongParameterExactSingle()
{
	FBrokenProtocol_EntryLangFeProtocolWrongParameterExactSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}

int EntryLangFeProtocolWrongReturnExactSingle()
{
	FBrokenProtocol_EntryLangFeProtocolWrongReturnExactSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}

int EntryLangFeProtocolInaccessibleExactSingle()
{
	FBrokenProtocol_EntryLangFeProtocolInaccessibleExactSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}

int EntryLangFeProtocolCompleteMissingSingle()
{
	FForeachProtocolRange_EntryLangFeProtocolCompleteMissingSingle Range;
	Range.Native.Count = 2;
	int Trace = 0;
	foreach (int Value : Range)
	{
		Trace += Value;
	}
	return Trace;
}
