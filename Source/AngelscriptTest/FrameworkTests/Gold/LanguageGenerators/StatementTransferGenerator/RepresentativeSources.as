int EntryLangCfStatementCountTransferIfOneNoneNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		if (Index >= 0)
		{
			Trace += 1;
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfElseOneNoneNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		if ((Index & 1) == 0)
		{
			Trace += 2;
		}
		else
		{
			Trace += 2;
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferElseIfOneNoneNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		if (Index < 0)
		{
			Trace += 1000;
		}
		else if (Index >= 0)
		{
			Trace += 3;
		}
		else
		{
			Trace += 2000;
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferWhileOneNoneNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		int Once = 0;
		while (Once < 1)
		{
			++Once;
			Trace += 4;
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferDoWhileOneNoneNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		int Once = 0;
		do
		{
			++Once;
			Trace += 5;
		}
		while (Once < 1);
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferForOneNoneNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		for (int Once = 0; Once < 1; ++Once)
		{
			Trace += 6;
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferSwitchOneNoneNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		switch (Index & 1)
		{
		case 0:
			Trace += 7;
			break;
		default:
			Trace += 7;
			break;
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferNestedBlockOneNoneNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		{
			{
				Trace += 8;
			}
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfOneNoneSameKind()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 10;
		for (int SameKind = 0; SameKind < 1; ++SameKind)
		{
			if (Index >= 0)
			{
				Trace += 1;
			}
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfOneNoneMixedLoop()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 20;
		int MixedLoop = 0;
		while (MixedLoop < 1)
		{
			++MixedLoop;
			if (Index >= 0)
			{
				Trace += 1;
			}
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfOneNoneLoopSwitch()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 30;
		switch (0)
		{
		case 0:
		{
			if (Index >= 0)
			{
				Trace += 1;
			}
		}
			break;
		default:
			break;
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfOneNoneBranchLoop()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 40;
		if (Index >= 0)
		{
			for (int BranchLoop = 0; BranchLoop < 1; ++BranchLoop)
			{
				if (Index >= 0)
				{
					Trace += 1;
				}
			}
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfOneNoneThreeLevel()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 50;
		if (Index >= 0)
		{
			switch (0)
			{
			case 0:
			{
				int ThreeLevel = 0;
				while (ThreeLevel < 1)
				{
					++ThreeLevel;
					if (Index >= 0)
					{
						Trace += 1;
					}
				}
				break;
			}
			default:
				break;
			}
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfOneBreakLoopNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		if (Index >= 0)
		{
			Trace += 1;
		}
		Trace += 10;
		break;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfOneBreakSwitchNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		if (Index >= 0)
		{
			Trace += 1;
		}
		switch (0)
		{
		case 0:
			Trace += 10;
			break;
		default:
			Trace += 1000;
			break;
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfOneContinueNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		if (Index >= 0)
		{
			Trace += 1;
		}
		Trace += 10;
		continue;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfOneEarlyReturnNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		if (Index >= 0)
		{
			Trace += 1;
		}
		return Trace + 20;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfOneNestedReturnNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		if (Index >= 0)
		{
			Trace += 1;
		}
		if (Trace >= 0)
		{
			return Trace + 30;
		}
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfOneFallthroughNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		if (Index >= 0)
		{
			Trace += 1;
		}
		switch (0)
		{
		case 0:
			Trace += 10;
		case 1:
			Trace += 1;
			break;
		default:
			break;
		}
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfOneExceptionNone()
{
	int Trace = 0;
	int Limit = 1;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		if (Index >= 0)
		{
			Trace += 1;
		}
		int ExceptionDivisor = 0;
		Trace += 1 / ExceptionDivisor;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfZeroNoneNone()
{
	int Trace = 0;
	int Limit = 0;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		if (Index >= 0)
		{
			Trace += 1;
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfTwoNoneNone()
{
	int Trace = 0;
	int Limit = 2;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		if (Index >= 0)
		{
			Trace += 1;
		}
		Trace += 1;
	}
	return Trace;
}

int EntryLangCfStatementCountTransferIfManyNoneNone()
{
	int Trace = 0;
	int Limit = 4;
	for (int Index = 0; Index < Limit; ++Index)
	{
		Trace += 0;
		if (Index >= 0)
		{
			Trace += 1;
		}
		Trace += 1;
	}
	return Trace;
}
