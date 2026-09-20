int EntryLangCfSwitchPlacementFunctionCaseOutside()
{
	int Value = 0;
	case 1:
		Value += 10;
	return Value;
}

int EntryLangCfSwitchPlacementFunctionDefaultOutside()
{
	int Value = 0;
	default:
		Value += 20;
	return Value;
}

int EntryLangCfSwitchPlacementFunctionDuplicateDefault()
{
	int Value = 0;
	switch (Value)
	{
	default:
		Value += 30;
	default:
		Value += 40;
	}
	return Value;
}

int EntryLangCfSwitchPlacementFunctionCaseAfterDefault()
{
	int Value = 0;
	switch (Value)
	{
	default:
		Value += 50;
	case 1:
		Value += 60;
	}
	return Value;
}

int EntryLangCfSwitchPlacementBranchCaseOutside()
{
	int Value = 0;
	if (true)
	{
		case 1:
			Value += 10;
	}
	return Value;
}

int EntryLangCfSwitchPlacementBranchDefaultOutside()
{
	int Value = 0;
	if (true)
	{
		default:
			Value += 20;
	}
	return Value;
}

int EntryLangCfSwitchPlacementBranchDuplicateDefault()
{
	int Value = 0;
	if (true)
	{
		switch (Value)
		{
		default:
			Value += 30;
		default:
			Value += 40;
		}
	}
	return Value;
}

int EntryLangCfSwitchPlacementBranchCaseAfterDefault()
{
	int Value = 0;
	if (true)
	{
		switch (Value)
		{
		default:
			Value += 50;
		case 1:
			Value += 60;
		}
	}
	return Value;
}

int EntryLangCfSwitchPlacementLoopCaseOutside()
{
	int Value = 0;
	while (false)
	{
		case 1:
			Value += 10;
	}
	return Value;
}

int EntryLangCfSwitchPlacementLoopDefaultOutside()
{
	int Value = 0;
	while (false)
	{
		default:
			Value += 20;
	}
	return Value;
}

int EntryLangCfSwitchPlacementLoopDuplicateDefault()
{
	int Value = 0;
	while (false)
	{
		switch (Value)
		{
		default:
			Value += 30;
		default:
			Value += 40;
		}
	}
	return Value;
}

int EntryLangCfSwitchPlacementLoopCaseAfterDefault()
{
	int Value = 0;
	while (false)
	{
		switch (Value)
		{
		default:
			Value += 50;
		case 1:
			Value += 60;
		}
	}
	return Value;
}

int EntryLangCfSwitchPlacementAfterSwitchCaseOutside()
{
	int Value = 0;
	switch (Value)
	{
	case 0:
		Value += 1;
		break;
	default:
		Value += 2;
		break;
	}
	case 1:
		Value += 10;
	return Value;
}

int EntryLangCfSwitchPlacementAfterSwitchDefaultOutside()
{
	int Value = 0;
	switch (Value)
	{
	case 0:
		Value += 1;
		break;
	default:
		Value += 2;
		break;
	}
	default:
		Value += 20;
	return Value;
}

int EntryLangCfSwitchPlacementAfterSwitchDuplicateDefault()
{
	int Value = 0;
	switch (Value)
	{
	case 0:
		Value += 1;
		break;
	default:
		Value += 2;
		break;
	}
	switch (Value)
	{
	default:
		Value += 30;
	default:
		Value += 40;
	}
	return Value;
}

int EntryLangCfSwitchPlacementAfterSwitchCaseAfterDefault()
{
	int Value = 0;
	switch (Value)
	{
	case 0:
		Value += 1;
		break;
	default:
		Value += 2;
		break;
	}
	switch (Value)
	{
	default:
		Value += 50;
	case 1:
		Value += 60;
	}
	return Value;
}
