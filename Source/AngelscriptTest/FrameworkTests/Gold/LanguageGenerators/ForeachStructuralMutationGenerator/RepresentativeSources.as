int EntryLangFeStructuralMutationOneStablePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeStructuralMutationOneStablePrimitiveValue Range;
	Range.Native.Count = 1;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
	}
	return Trace;
}

int EntryLangFeStructuralMutationOneShrinkFirstPrimitiveValue()
{
	FForeachTransferRange_EntryLangFeStructuralMutationOneShrinkFirstPrimitiveValue Range;
	Range.Native.Count = 1;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		if (CurrentIteration == 0)
		{
			Range.Native.Count = 1;
		}
	}
	return Trace;
}

int EntryLangFeStructuralMutationOneShrinkMiddlePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeStructuralMutationOneShrinkMiddlePrimitiveValue Range;
	Range.Native.Count = 1;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		if (CurrentIteration == 1)
		{
			Range.Native.Count = 2;
		}
	}
	return Trace;
}

int EntryLangFeStructuralMutationOneClearAfterFirstPrimitiveValue()
{
	FForeachTransferRange_EntryLangFeStructuralMutationOneClearAfterFirstPrimitiveValue Range;
	Range.Native.Count = 1;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		if (CurrentIteration == 0)
		{
			Range.Native.Count = 0;
		}
	}
	return Trace;
}

int EntryLangFeStructuralMutationTwoStablePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeStructuralMutationTwoStablePrimitiveValue Range;
	Range.Native.Count = 2;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
	}
	return Trace;
}

int EntryLangFeStructuralMutationTwoShrinkFirstPrimitiveValue()
{
	FForeachTransferRange_EntryLangFeStructuralMutationTwoShrinkFirstPrimitiveValue Range;
	Range.Native.Count = 2;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		if (CurrentIteration == 0)
		{
			Range.Native.Count = 1;
		}
	}
	return Trace;
}

int EntryLangFeStructuralMutationTwoShrinkMiddlePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeStructuralMutationTwoShrinkMiddlePrimitiveValue Range;
	Range.Native.Count = 2;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		if (CurrentIteration == 1)
		{
			Range.Native.Count = 2;
		}
	}
	return Trace;
}

int EntryLangFeStructuralMutationManyStablePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeStructuralMutationManyStablePrimitiveValue Range;
	Range.Native.Count = 4;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
	}
	return Trace;
}

int EntryLangFeStructuralMutationManyShrinkMiddlePrimitiveValue()
{
	FForeachTransferRange_EntryLangFeStructuralMutationManyShrinkMiddlePrimitiveValue Range;
	Range.Native.Count = 4;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		if (CurrentIteration == 1)
		{
			Range.Native.Count = 2;
		}
	}
	return Trace;
}

int EntryLangFeStructuralMutationManyClearAfterFirstPrimitiveValue()
{
	FForeachTransferRange_EntryLangFeStructuralMutationManyClearAfterFirstPrimitiveValue Range;
	Range.Native.Count = 4;
	int Trace = 0;
	int Iteration = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		if (CurrentIteration == 0)
		{
			Range.Native.Count = 0;
		}
	}
	return Trace;
}

int EntryLangFeStructuralMutationOneStableValueObjectCopy()
{
	FForeachTransferRange_EntryLangFeStructuralMutationOneStableValueObjectCopy Range;
	Range.Native.Count = 1;
	int Trace = 0;
	int Iteration = 0;
	foreach (FNativeCaseValue Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
	}
	return Trace;
}

int EntryLangFeStructuralMutationOneStableValueObjectConstRef()
{
	FForeachTransferRange_EntryLangFeStructuralMutationOneStableValueObjectConstRef Range;
	Range.Native.Count = 1;
	int Trace = 0;
	int Iteration = 0;
	foreach (const FNativeCaseValue& Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
	}
	return Trace;
}

int EntryLangFeStructuralMutationOneShrinkFirstValueObjectCopy()
{
	FForeachTransferRange_EntryLangFeStructuralMutationOneShrinkFirstValueObjectCopy Range;
	Range.Native.Count = 1;
	int Trace = 0;
	int Iteration = 0;
	foreach (FNativeCaseValue Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		Trace += 1;
		if (CurrentIteration == 0)
		{
			Range.Native.Count = 1;
		}
	}
	return Trace;
}
