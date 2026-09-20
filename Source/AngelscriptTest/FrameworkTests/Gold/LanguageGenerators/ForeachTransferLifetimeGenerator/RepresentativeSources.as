int EntryLangFeTransferLifetimeCompleteSinglePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeCompleteSinglePrimitiveValue Range;
	Range.Native.Count = 3;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		Trace += Value - Value;
	}
	return Trace;
}

int EntryLangFeTransferLifetimeCompleteNestedSamePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeCompleteNestedSamePrimitiveValue Range;
	Range.Native.Count = 3;
	int Trace = 0;
	int OuterIteration = 0;
	foreach (int OuterValue : Range)
	{
		int CurrentOuterIteration = OuterIteration;
		++OuterIteration;
		Trace += 100;
		int InnerIteration = 0;
		foreach (int InnerValue : Range)
		{
			int CurrentIteration = InnerIteration;
			++InnerIteration;
			Trace += 1;
			Trace += InnerValue - InnerValue;
		}
	}
	return Trace;
}

int EntryLangFeTransferLifetimeCompleteNestedDistinctPrimitiveValue()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeCompleteNestedDistinctPrimitiveValue Range;
	Range.Native.Count = 3;
	FForeachTransferRange_EntryLangFeTransferLifetimeCompleteNestedDistinctPrimitiveValue OtherRange;
	OtherRange.Native.Count = 2;
	int Trace = 0;
	int OuterIteration = 0;
	foreach (int OuterValue : Range)
	{
		int CurrentOuterIteration = OuterIteration;
		++OuterIteration;
		Trace += 100;
		int InnerIteration = 0;
		foreach (int InnerValue : OtherRange)
		{
			int CurrentIteration = InnerIteration;
			++InnerIteration;
			Trace += 1;
			Trace += InnerValue - InnerValue;
		}
	}
	return Trace;
}

int EntryLangFeTransferLifetimeCompleteInsideForPrimitiveValue()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeCompleteInsideForPrimitiveValue Range;
	Range.Native.Count = 3;
	int Trace = 0;
	for (int OuterIndex = 0; OuterIndex < 2; ++OuterIndex)
	{
		Trace += 10;
		int Iteration = 0;
		foreach (int Value : Range)
		{
			int CurrentIteration = Iteration;
			++Iteration;
			Trace += 1;
			Trace += Value - Value;
		}
	}
	return Trace;
}

int EntryLangFeTransferLifetimeBreakFirstSinglePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeBreakFirstSinglePrimitiveValue Range;
	Range.Native.Count = 3;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		Trace += Value - Value;
		if (CurrentIteration == 0)
		{
			break;
		}
	}
	return Trace;
}

int EntryLangFeTransferLifetimeBreakMiddleSinglePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeBreakMiddleSinglePrimitiveValue Range;
	Range.Native.Count = 3;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		Trace += Value - Value;
		if (CurrentIteration == 1)
		{
			break;
		}
	}
	return Trace;
}

int EntryLangFeTransferLifetimeContinueFirstSinglePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeContinueFirstSinglePrimitiveValue Range;
	Range.Native.Count = 3;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		Trace += Value - Value;
		if (CurrentIteration == 0)
		{
			continue;
		}
	}
	return Trace;
}

int EntryLangFeTransferLifetimeContinueMiddleSinglePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeContinueMiddleSinglePrimitiveValue Range;
	Range.Native.Count = 3;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		Trace += Value - Value;
		if (CurrentIteration == 1)
		{
			continue;
		}
	}
	return Trace;
}

int EntryLangFeTransferLifetimeReturnSinglePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeReturnSinglePrimitiveValue Range;
	Range.Native.Count = 3;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		Trace += Value - Value;
		return Trace;
	}
	return Trace;
}

int EntryLangFeTransferLifetimeExceptionSinglePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeExceptionSinglePrimitiveValue Range;
	Range.Native.Count = 3;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		Trace += Value - Value;
		int Zero = 0;
		Trace += 1 / Zero;
	}
	return Trace;
}

int EntryLangFeTransferLifetimeCompleteSingleValueObjectCopy()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeCompleteSingleValueObjectCopy Range;
	Range.Native.Count = 3;
	int Trace = 0;
	int Iteration = 0;
	foreach (FNativeCaseValue Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		Trace += Value.Value - Value.Value;
	}
	return Trace;
}

int EntryLangFeTransferLifetimeCompleteSingleValueObjectConstRef()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeCompleteSingleValueObjectConstRef Range;
	Range.Native.Count = 3;
	int Trace = 0;
	int Iteration = 0;
	foreach (const FNativeCaseValue& Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		Trace += Value.Value - Value.Value;
	}
	return Trace;
}

int EntryLangFeTransferLifetimeExceptionNestedSamePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeExceptionNestedSamePrimitiveValue Range;
	Range.Native.Count = 3;
	int Trace = 0;
	int OuterIteration = 0;
	foreach (int OuterValue : Range)
	{
		int CurrentOuterIteration = OuterIteration;
		++OuterIteration;
		Trace += 100;
		int InnerIteration = 0;
		foreach (int InnerValue : Range)
		{
			int CurrentIteration = InnerIteration;
			++InnerIteration;
			Trace += 1;
			Trace += InnerValue - InnerValue;
			int Zero = 0;
			Trace += 1 / Zero;
		}
	}
	return Trace;
}

int EntryLangFeTransferLifetimeReturnInsideForPrimitiveValue()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeReturnInsideForPrimitiveValue Range;
	Range.Native.Count = 3;
	int Trace = 0;
	for (int OuterIndex = 0; OuterIndex < 2; ++OuterIndex)
	{
		Trace += 10;
		int Iteration = 0;
		foreach (int Value : Range)
		{
			int CurrentIteration = Iteration;
			++Iteration;
			Trace += 1;
			Trace += Value - Value;
			return Trace;
		}
	}
	return Trace;
}

int EntryLangFeTransferLifetimeBreakFirstNestedDistinctValueObjectCopy()
{
	FForeachTransferRange_EntryLangFeTransferLifetimeBreakFirstNestedDistinctValueObjectCopy Range;
	Range.Native.Count = 3;
	FForeachTransferRange_EntryLangFeTransferLifetimeBreakFirstNestedDistinctValueObjectCopy OtherRange;
	OtherRange.Native.Count = 2;
	int Trace = 0;
	int OuterIteration = 0;
	foreach (FNativeCaseValue OuterValue : Range)
	{
		int CurrentOuterIteration = OuterIteration;
		++OuterIteration;
		Trace += 100;
		int InnerIteration = 0;
		foreach (FNativeCaseValue InnerValue : OtherRange)
		{
			int CurrentIteration = InnerIteration;
			++InnerIteration;
			Trace += 1;
			Trace += InnerValue.Value - InnerValue.Value;
			if (CurrentIteration == 0)
			{
				break;
			}
		}
	}
	return Trace;
}
