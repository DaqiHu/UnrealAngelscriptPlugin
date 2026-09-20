int EntryLangVarLoopDeclLifetimeNormalZeroForInitializerScriptValue()
{
	int Limit = 0;
	int Index = 0;
	int BodyCount = 0;
	for (FScriptLoopValue Owner(100); Index < Limit; Index = AdvanceVariableLoop(Index))
	{
		++BodyCount;
		RecordVariableLoopEvent(1000 + Index);
		BodyCount += Owner.Value - Owner.Value;
		RecordVariableLoopEvent(3000 + Index);
	}
	return 7000 + BodyCount;
}

int EntryLangVarLoopDeclLifetimeNormalOneDoBodyNativeValue()
{
	int Limit = 1;
	int Index = 0;
	int BodyCount = 0;
	do
	{
		++BodyCount;
		RecordVariableLoopEvent(1000 + Index);
		FNativeCaseValue Owner(100 + Index);
		BodyCount += Owner.Value - Owner.Value;
		RecordVariableLoopEvent(3000 + Index);
		Index = AdvanceVariableLoop(Index);
	}
	while (Index < Limit);
	return 7000 + BodyCount;
}

int EntryLangVarLoopDeclLifetimeBreakThreeForBodyScriptValue()
{
	int Limit = 3;
	int Index = 0;
	int BodyCount = 0;
	for (; Index < Limit; Index = AdvanceVariableLoop(Index))
	{
		++BodyCount;
		RecordVariableLoopEvent(1000 + Index);
		FScriptLoopValue Owner(100 + Index);
		BodyCount += Owner.Value - Owner.Value;
		RecordVariableLoopEvent(3100 + Index);
		break;
	}
	return 7100 + BodyCount;
}

int EntryLangVarLoopDeclLifetimeContinueEightWhileBodyNativeValue()
{
	int Limit = 8;
	int Index = 0;
	int BodyCount = 0;
	while (Index < Limit)
	{
		++BodyCount;
		RecordVariableLoopEvent(1000 + Index);
		FNativeCaseValue Owner(100 + Index);
		BodyCount += Owner.Value - Owner.Value;
		RecordVariableLoopEvent(3200 + Index);
		Index = AdvanceVariableLoop(Index);
		continue;
	}
	return 7200 + BodyCount;
}

int EntryLangVarLoopDeclLifetimeReturnOneNestedBodyScriptValue()
{
	int Limit = 1;
	int Index = 0;
	int BodyCount = 0;
	for (; Index < Limit; Index = AdvanceVariableLoop(Index))
	{
		{
			++BodyCount;
			RecordVariableLoopEvent(1000 + Index);
			FScriptLoopValue Owner(100 + Index);
			BodyCount += Owner.Value - Owner.Value;
			RecordVariableLoopEvent(3300 + Index);
			return 7300 + BodyCount;
		}
	}
	return 7300 + BodyCount;
}

int EntryLangVarLoopDeclLifetimeExceptionZeroForInitializerScriptValue()
{
	int Limit = 0;
	int Index = 0;
	int BodyCount = 0;
	for (FScriptLoopValue Owner(100); Index < Limit; Index = AdvanceVariableLoop(Index))
	{
		++BodyCount;
		RecordVariableLoopEvent(1000 + Index);
		BodyCount += Owner.Value - Owner.Value;
		RecordVariableLoopEvent(3400 + Index);
		BodyCount += RaiseVariableLoopException();
	}
	return 7400 + BodyCount;
}

int EntryLangVarLoopDeclLifetimeExceptionOneForBodyScriptValue()
{
	int Limit = 1;
	int Index = 0;
	int BodyCount = 0;
	for (; Index < Limit; Index = AdvanceVariableLoop(Index))
	{
		++BodyCount;
		RecordVariableLoopEvent(1000 + Index);
		FScriptLoopValue Owner(100 + Index);
		BodyCount += Owner.Value - Owner.Value;
		RecordVariableLoopEvent(3400 + Index);
		BodyCount += RaiseVariableLoopException();
	}
	return 7400 + BodyCount;
}
