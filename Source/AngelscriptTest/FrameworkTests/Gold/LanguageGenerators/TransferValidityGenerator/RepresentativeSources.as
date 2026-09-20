int EntryLangCfTransferValidityFunctionBreak()
{
	break;
	return 0;
}

int EntryLangCfTransferValidityFunctionContinue()
{
	continue;
	return 0;
}

int EntryLangCfTransferValidityBranchBreak()
{
	if (true)
	{
		break;
	}
	return 0;
}

int EntryLangCfTransferValidityBranchContinue()
{
	if (true)
	{
		continue;
	}
	return 0;
}

int EntryLangCfTransferValiditySwitchBreak()
{
	switch (1)
	{
	case 1:
	{
		break;
		break;
	}
	default:
	{
		break;
	}
	}
	return 3;
}

int EntryLangCfTransferValiditySwitchContinue()
{
	switch (1)
	{
	case 1:
	{
		continue;
		break;
	}
	default:
	{
		break;
	}
	}
	return 3;
}

int EntryLangCfTransferValidityLoopBreak()
{
	for (int Index = 0; Index < 2; ++Index)
	{
		if (Index == 0)
		{
			break;
		}
		return 5;
	}
	return 3;
}

int EntryLangCfTransferValidityLoopContinue()
{
	for (int Index = 0; Index < 2; ++Index)
	{
		if (Index == 0)
		{
			continue;
		}
		return 5;
	}
	return 3;
}
