struct FScopedLifetimeProbe
{
	int Value = 0;
	int ObjectId = 0;

	FScopedLifetimeProbe(int InValue)
	{
		Value = InValue;
		ObjectId = BeginNativeScriptLifecycle(Value);
	}

	~FScopedLifetimeProbe()
	{
		EndNativeScriptLifecycle(ObjectId, Value);
	}
}

int Recovery()
{
	return 97;
}

int EntryLangCfLiveLocalCleanupLoopNormalOneOneLf()
{
	int Trace = 0;
	for (int Level1 = 0; Level1 < 1; ++Level1)
	{
		FScopedLifetimeProbe Local_1_1(101);
		Trace += 100;
	}
	Trace += 1000;
	return Trace + 97;
}

int EntryLangCfLiveLocalCleanupLoopBreakOneOneLf()
{
	int Trace = 0;
	for (int Level1 = 0; Level1 < 1; ++Level1)
	{
		FScopedLifetimeProbe Local_1_1(101);
		Trace += 100;
		Trace += 7;
		break;
	}
	Trace += 1000;
	return Trace + 97;
}

int EntryLangCfLiveLocalCleanupLoopContinueOneOneLf()
{
	int Trace = 0;
	for (int Level1 = 0; Level1 < 1; ++Level1)
	{
		FScopedLifetimeProbe Local_1_1(101);
		Trace += 100;
		Trace += 11;
		continue;
	}
	Trace += 1000;
	return Trace + 97;
}

int EntryLangCfLiveLocalCleanupLoopReturnOneOneLf()
{
	int Trace = 0;
	for (int Level1 = 0; Level1 < 1; ++Level1)
	{
		FScopedLifetimeProbe Local_1_1(101);
		Trace += 100;
		Trace += 13;
		return Trace;
	}
	Trace += 1000;
	return Trace + 97;
}

int EntryLangCfLiveLocalCleanupLoopExceptionOneOneLf()
{
	int Trace = 0;
	for (int Level1 = 0; Level1 < 1; ++Level1)
	{
		FScopedLifetimeProbe Local_1_1(101);
		Trace += 100;
		Trace += 17;
		int ExceptionDivisor = 0;
		Trace += 1 / ExceptionDivisor;
	}
	Trace += 1000;
	return Trace + 97;
}

int EntryLangCfLiveLocalCleanupBranchLoopNormalOneOneLf()
{
	int Trace = 0;
	if (true)
	{
		for (int Level1 = 0; Level1 < 1; ++Level1)
		{
			FScopedLifetimeProbe Local_1_1(101);
			Trace += 100;
		}
		Trace += 1000;
	}
	return Trace + 97;
}

int EntryLangCfLiveLocalCleanupNestedLoopNormalOneOneLf()
{
	int Trace = 0;
	for (int ShapeLoop = 0; ShapeLoop < 1; ++ShapeLoop)
	{
		if (ShapeLoop >= 0)
		{
			for (int Level1 = 0; Level1 < 1; ++Level1)
			{
				FScopedLifetimeProbe Local_1_1(101);
				Trace += 100;
			}
			Trace += 1000;
		}
	}
	return Trace + 97;
}

int EntryLangCfLiveLocalCleanupSwitchLoopNormalOneOneLf()
{
	int Trace = 0;
	switch (0)
	{
	case 0:
	{
		for (int Level1 = 0; Level1 < 1; ++Level1)
		{
			FScopedLifetimeProbe Local_1_1(101);
			Trace += 100;
		}
		Trace += 1000;
		break;
	}
	default:
	{
		break;
	}
	}
	return Trace + 97;
}

int EntryLangCfLiveLocalCleanupLoopNormalTwoOneLf()
{
	int Trace = 0;
	for (int Level1 = 0; Level1 < 1; ++Level1)
	{
		FScopedLifetimeProbe Local_1_1(101);
		Trace += 100;
		for (int Level2 = 0; Level2 < 1; ++Level2)
		{
			FScopedLifetimeProbe Local_2_1(201);
			Trace += 200;
		}
		Trace += 2000;
	}
	Trace += 1000;
	return Trace + 97;
}

int EntryLangCfLiveLocalCleanupLoopReturnTwoOneLf()
{
	int Trace = 0;
	for (int Level1 = 0; Level1 < 1; ++Level1)
	{
		FScopedLifetimeProbe Local_1_1(101);
		Trace += 100;
		for (int Level2 = 0; Level2 < 1; ++Level2)
		{
			FScopedLifetimeProbe Local_2_1(201);
			Trace += 200;
			Trace += 13;
			return Trace;
		}
		Trace += 2000;
	}
	Trace += 1000;
	return Trace + 97;
}

int EntryLangCfLiveLocalCleanupLoopNormalThreeTwoLf()
{
	int Trace = 0;
	for (int Level1 = 0; Level1 < 1; ++Level1)
	{
		FScopedLifetimeProbe Local_1_1(101);
		FScopedLifetimeProbe Local_1_2(102);
		Trace += 100;
		for (int Level2 = 0; Level2 < 1; ++Level2)
		{
			FScopedLifetimeProbe Local_2_1(201);
			FScopedLifetimeProbe Local_2_2(202);
			Trace += 200;
			for (int Level3 = 0; Level3 < 1; ++Level3)
			{
				FScopedLifetimeProbe Local_3_1(301);
				FScopedLifetimeProbe Local_3_2(302);
				Trace += 300;
			}
			Trace += 3000;
		}
		Trace += 2000;
	}
	Trace += 1000;
	return Trace + 97;
}
