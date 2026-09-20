int EntryLangFeSizeVariableEmptyPrimitiveValueComplete()
{
	FForeachIterationRange_EntryLangFeSizeVariableEmptyPrimitiveValueComplete Range;
	Range.Native.Count = 0;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOnePrimitiveValueComplete()
{
	FForeachIterationRange_EntryLangFeSizeVariableOnePrimitiveValueComplete Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableTwoPrimitiveValueComplete()
{
	FForeachIterationRange_EntryLangFeSizeVariableTwoPrimitiveValueComplete Range;
	Range.Native.Count = 2;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableManyPrimitiveValueComplete()
{
	FForeachIterationRange_EntryLangFeSizeVariableManyPrimitiveValueComplete Range;
	Range.Native.Count = 4;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOnePrimitiveAutoComplete()
{
	FForeachIterationRange_EntryLangFeSizeVariableOnePrimitiveAutoComplete Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (auto Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOnePrimitiveMutableReferenceComplete()
{
	FForeachIterationRange_EntryLangFeSizeVariableOnePrimitiveMutableReferenceComplete Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int& Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		++Value;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOnePrimitiveConstReferenceComplete()
{
	FForeachIterationRange_EntryLangFeSizeVariableOnePrimitiveConstReferenceComplete Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (const int& Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOnePrimitiveValueBreakFirst()
{
	FForeachIterationRange_EntryLangFeSizeVariableOnePrimitiveValueBreakFirst Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		if (CurrentIteration == 0)
		{
			break;
		}
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOnePrimitiveValueBreakMiddle()
{
	FForeachIterationRange_EntryLangFeSizeVariableOnePrimitiveValueBreakMiddle Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		if (CurrentIteration == 1)
		{
			break;
		}
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOnePrimitiveValueBreakLast()
{
	FForeachIterationRange_EntryLangFeSizeVariableOnePrimitiveValueBreakLast Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		if (CurrentIteration + 1 >= Limit)
		{
			break;
		}
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOnePrimitiveValueContinueFirst()
{
	FForeachIterationRange_EntryLangFeSizeVariableOnePrimitiveValueContinueFirst Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		if (CurrentIteration == 0)
		{
			continue;
		}
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOnePrimitiveValueContinueMiddle()
{
	FForeachIterationRange_EntryLangFeSizeVariableOnePrimitiveValueContinueMiddle Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		if (CurrentIteration == 1)
		{
			continue;
		}
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOnePrimitiveValueReturn()
{
	FForeachIterationRange_EntryLangFeSizeVariableOnePrimitiveValueReturn Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		return Trace + Contribution;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOnePrimitiveValueException()
{
	FForeachIterationRange_EntryLangFeSizeVariableOnePrimitiveValueException Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		int Zero = 0;
		Trace += 1 / Zero;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableEmptyPrimitiveValueException()
{
	FForeachIterationRange_EntryLangFeSizeVariableEmptyPrimitiveValueException Range;
	Range.Native.Count = 0;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		int Zero = 0;
		Trace += 1 / Zero;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOneValueObjectValueComplete()
{
	FForeachIterationRange_EntryLangFeSizeVariableOneValueObjectValueComplete Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (FForeachValueElement_EntryLangFeSizeVariableOneValueObjectValueComplete Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value.Value;
		int Contribution = ElementBias + 1;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOneValueObjectMutableReferenceComplete()
{
	FForeachIterationRange_EntryLangFeSizeVariableOneValueObjectMutableReferenceComplete Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (FForeachValueElement_EntryLangFeSizeVariableOneValueObjectMutableReferenceComplete& Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		++Value.Value;
		int ElementBias = Value.Value;
		int Contribution = ElementBias + 1;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOneConstElementValueComplete()
{
	FForeachIterationRange_EntryLangFeSizeVariableOneConstElementValueComplete Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOneConstElementConstReferenceComplete()
{
	FForeachIterationRange_EntryLangFeSizeVariableOneConstElementConstReferenceComplete Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (const int& Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOnePrimitiveIncompatibleComplete()
{
	FForeachIterationRange_EntryLangFeSizeVariableOnePrimitiveIncompatibleComplete Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (FForeachIncompatibleElement_EntryLangFeSizeVariableOnePrimitiveIncompatibleComplete Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOneReferenceObjectValueComplete()
{
	FForeachIterationRange_EntryLangFeSizeVariableOneReferenceObjectValueComplete Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (FNativeCaseReference@ Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		int ElementBias = Value.Value;
		int Contribution = ElementBias + 1;
		Trace += Contribution;
	}
	return Trace;
}

int EntryLangFeSizeVariableOneConstElementMutableReferenceComplete()
{
	FForeachIterationRange_EntryLangFeSizeVariableOneConstElementMutableReferenceComplete Range;
	Range.Native.Count = 1;
	int Limit = Range.Native.Count;
	int Iteration = 0;
	int Trace = 0;
	foreach (int& Value : Range)
	{
		int CurrentIteration = Iteration;
		++Iteration;
		++Value;
		int ElementBias = Value;
		int Contribution = ElementBias + 1;
		Trace += Contribution;
	}
	return Trace;
}
