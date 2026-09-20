int EntryLangCfNestedTargetsNestedLoopBreakInner()
{
	int Trace = 0;
	for (int Outer = 0; Outer < 2; ++Outer)
	{
		Trace += 1;
		for (int Inner = 0; Inner < 2; ++Inner)
		{
			Trace += 10;
			break;
		}
		Trace += 1000;
		Trace += 10000;
	}
	return Trace;
}

int EntryLangCfNestedTargetsNestedLoopReturnInner()
{
	int Trace = 0;
	for (int Outer = 0; Outer < 2; ++Outer)
	{
		Trace += 1;
		for (int Inner = 0; Inner < 2; ++Inner)
		{
			Trace += 10;
			return Trace;
		}
		Trace += 1000;
		Trace += 10000;
	}
	return Trace;
}

int EntryLangCfNestedTargetsBranchLoopContinueInner()
{
	int Trace = 0;
	for (int Outer = 0; Outer < 2; ++Outer)
	{
		Trace += 1;
		if (Outer >= 0)
		{
			Trace += 2;
			for (int Inner = 0; Inner < 2; ++Inner)
			{
				Trace += 10;
				continue;
			}
		}
		Trace += 1000;
		Trace += 10000;
	}
	return Trace;
}

int EntryLangCfNestedTargetsThreeLevelBreakOuter()
{
	int Trace = 0;
	for (int Outer = 0; Outer < 2; ++Outer)
	{
		Trace += 1;
		if (Outer >= 0)
		{
			for (int Middle = 0; Middle < 2; ++Middle)
			{
				Trace += 10;
				for (int Inner = 0; Inner < 2; ++Inner)
				{
					Trace += 100;
				}
			}
		}
		Trace += 1000;
		break;
		Trace += 10000;
	}
	return Trace;
}
