enum ESelector
{
	Zero = 0,
	One = 1,
	Two = 2,
	Three = 3,
	Four = 4,
	Five = 5,
	Six = 6
}

typedef int SelectorAlias;

class FUnsupported
{
}

int BoundaryControl()
{
	int Trace = 0;
	int Lower = 2147483642;
	switch (Lower)
	{
	case 2147483642:
		Trace += 1;
		break;
	default:
		Trace += 100;
		break;
	}
	int Trigger = 2147483643;
	switch (Trigger)
	{
	case 2147483643:
		Trace += 2;
		break;
	default:
		Trace += 200;
		break;
	}
	int Dense = 2147483647;
	switch (Dense)
	{
	case 2147483645:
		Trace += 400;
		break;
	case 2147483646:
		Trace += 500;
		break;
	case 2147483647:
		Trace += 3;
		break;
	default:
		Trace += 800;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchInt8FirstBreak()
{
	int Trace = 0;
	int8 Value = int8(1);
	switch (Value)
	{
	case int8(1):
		Trace += 11;
		break;
	default:
		Trace += 5000;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchInt8FallthroughBreak()
{
	int Trace = 0;
	int8 Value = int8(1);
	switch (Value)
	{
	case int8(1):
		Trace += 16;
	case int8(2):
		Trace += 1;
		break;
	default:
		Trace += 5000;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchInt8GroupedBreak()
{
	int Trace = 0;
	int8 Value = int8(2);
	switch (Value)
	{
	case int8(1):
	case int8(2):
		Trace += 17;
		break;
	default:
		Trace += 5000;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchInt8FirstFallthrough()
{
	int Trace = 0;
	int8 Value = int8(1);
	switch (Value)
	{
	case int8(1):
		Trace += 11;
	case int8(6):
		Trace += 100;
		break;
	default:
		Trace += 5000;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchInt8FirstReturn()
{
	int Trace = 0;
	int8 Value = int8(1);
	switch (Value)
	{
	case int8(1):
		Trace += 11;
		return Trace + 200;
	default:
		Trace += 5000;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchInt8FirstException()
{
	int Trace = 0;
	int8 Value = int8(1);
	switch (Value)
	{
	case int8(1):
		Trace += 11;
		{
			int ExceptionDivisor = 0;
			Trace += 1 / ExceptionDivisor;
		}
		break;
	default:
		Trace += 5000;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchEnumMiddleBreak()
{
	int Trace = 0;
	ESelector Value = ESelector::Two;
	switch (Value)
	{
	case ESelector::Two:
		Trace += 12;
		break;
	default:
		Trace += 5000;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchTypedefLastBreak()
{
	int Trace = 0;
	SelectorAlias Value = SelectorAlias(3);
	switch (Value)
	{
	case SelectorAlias(3):
		Trace += 13;
		break;
	default:
		Trace += 5000;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchBoundaryFirstBreak()
{
	int Trace = BoundaryControl();
	int Value = 2147483642;
	switch (Value)
	{
	case 2147483642:
		Trace += 11;
		break;
	default:
		Trace += 5000;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchInt8DefaultBreak()
{
	int Trace = 0;
	int8 Value = int8(4);
	switch (Value)
	{
	case int8(0):
		Trace += 1000;
		break;
	default:
		Trace += 14;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchInt8NoMatchBreak()
{
	int Trace = 0;
	int8 Value = int8(5);
	switch (Value)
	{
	case int8(0):
		Trace += 1000;
		break;
	default:
		Trace += 15;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchUnsupportedFirstBreak()
{
	int Trace = 0;
	FUnsupported Value;
	switch (Value)
	{
	case 1:
		Trace += 11;
		break;
	default:
		Trace += 5000;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchInt8DuplicateBreak()
{
	int Trace = 0;
	int8 Value = int8(5);
	switch (Value)
	{
	case int8(1):
		Trace += 17;
		break;
	case int8(1):
		Trace += 1000;
		break;
	default:
		Trace += 5000;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchInt8NonConstantBreak()
{
	int Trace = 0;
	int8 Value = int8(5);
	int DynamicCase = 1;
	switch (Value)
	{
	case DynamicCase:
		Trace += 17;
		break;
	default:
		Trace += 5000;
		break;
	}
	return Trace;
}

int EntryLangCfSwitchInt8DefaultFallthrough()
{
	int Trace = 0;
	int8 Value = int8(4);
	switch (Value)
	{
	case int8(0):
		Trace += 1000;
		break;
	default:
		Trace += 14;
	case int8(6):
		Trace += 100;
		break;
	}
	return Trace;
}
