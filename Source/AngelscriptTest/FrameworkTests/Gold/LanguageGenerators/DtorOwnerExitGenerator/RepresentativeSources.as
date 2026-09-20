int EntryLangDtorOwnerExitLocalBlockEndOneEventOrder()
{
	FExitValue OwnerOne(101);
	ReachDestructorExitTerminal(1);
	return 1;
}

int EntryLangDtorOwnerExitLocalReturnOneEventOrder()
{
	FExitValue OwnerOne(101);
	ReachDestructorExitTerminal(2);
	return 2;
	return 2;
}

int EntryLangDtorOwnerExitLocalEarlyReturnOneEventOrder()
{
	FExitValue OwnerOne(101);
	if (101 > 0)
	{
		ReachDestructorExitTerminal(3);
		return 3;
	}
	return 3;
}

int EntryLangDtorOwnerExitLocalBreakOneEventOrder()
{
	while (true)
	{
		FExitValue OwnerOne(101);
		ReachDestructorExitTerminal(4);
		break;
	}
	return 4;
}

int EntryLangDtorOwnerExitLocalContinueOneEventOrder()
{
	for (int ExitIndex = 0; ExitIndex < 1; ++ExitIndex)
	{
		FExitValue OwnerOne(101);
		ReachDestructorExitTerminal(5);
		continue;
	}
	return 5;
}

int EntryLangDtorOwnerExitLocalSwitchExitOneEventOrder()
{
	switch (1)
	{
	case 1:
	{
		FExitValue OwnerOne(101);
		ReachDestructorExitTerminal(6);
		break;
	}
	default:
	{
		break;
	}
	}
	return 6;
}

int EntryLangDtorOwnerExitLocalExceptionOneEventOrder()
{
	FExitValue OwnerOne(101);
	ReachDestructorExitTerminal(7);
	int Zero = 0;
	return 1 / Zero;
}

int EntryLangDtorOwnerExitLocalAbortOneEventOrder()
{
	FExitValue OwnerOne(101);
	ReachDestructorExitTerminal(8);
	return 8;
}

int EntryLangDtorOwnerExitLocalUnprepareOneEventOrder()
{
	FExitValue OwnerOne(101);
	ReachDestructorExitTerminal(9);
	return 9;
}

int EntryLangDtorOwnerExitNestedLocalBlockEndOneEventOrder()
{
	FExitValue OuterOne(101);
	{
		FExitValue InnerOne(101 + 1);
		ReachDestructorExitTerminal(10);
	}
	return 10;
}

int EntryLangDtorOwnerExitNestedLocalReturnOneEventOrder()
{
	FExitValue OuterOne(101);
	{
		FExitValue InnerOne(101 + 1);
		ReachDestructorExitTerminal(11);
		return 11;
	}
	return 11;
}

int EntryLangDtorOwnerExitNestedLocalExceptionOneEventOrder()
{
	FExitValue OuterOne(101);
	{
		FExitValue InnerOne(101 + 1);
		ReachDestructorExitTerminal(12);
	}
	int Zero = 0;
	return 1 / Zero;
}

int EntryLangDtorOwnerExitFieldBlockEndOneEventOrder()
{
	FExitFieldOwner OwnerOne = FExitFieldOwner(101);
	ReachDestructorExitTerminal(13);
	return 13;
}

int EntryLangDtorOwnerExitFieldReturnOneEventOrder()
{
	FExitFieldOwner OwnerOne = FExitFieldOwner(101);
	ReachDestructorExitTerminal(14);
	return 14;
	return 14;
}

int EntryLangDtorOwnerExitFieldExceptionOneEventOrder()
{
	FExitFieldOwner OwnerOne = FExitFieldOwner(101);
	ReachDestructorExitTerminal(15);
	int Zero = 0;
	return 1 / Zero;
}

int EntryLangDtorOwnerExitFieldAbortOneEventOrder()
{
	FExitFieldOwner OwnerOne = FExitFieldOwner(101);
	ReachDestructorExitTerminal(16);
	return 16;
}

int EntryLangDtorOwnerExitBaseDerivedBlockEndOneEventOrder()
{
	FExitDerivedOwner OwnerOne = FExitDerivedOwner(101);
	ReachDestructorExitTerminal(17);
	return 17;
}

int EntryLangDtorOwnerExitBaseDerivedReturnOneEventOrder()
{
	FExitDerivedOwner OwnerOne = FExitDerivedOwner(101);
	ReachDestructorExitTerminal(18);
	return 18;
	return 18;
}

int EntryLangDtorOwnerExitBaseDerivedExceptionOneEventOrder()
{
	FExitDerivedOwner OwnerOne = FExitDerivedOwner(101);
	ReachDestructorExitTerminal(19);
	int Zero = 0;
	return 1 / Zero;
}

int EntryLangDtorOwnerExitBaseDerivedAbortOneEventOrder()
{
	FExitDerivedOwner OwnerOne = FExitDerivedOwner(101);
	ReachDestructorExitTerminal(20);
	return 20;
}

int EntryLangDtorOwnerExitTemporaryStatementEndOneEventOrder()
{
	ObserveExitValue(FExitValue(101));
	ReachDestructorExitTerminal(21);
	return 21;
}

int EntryLangDtorOwnerExitTemporaryEarlyReturnOneEventOrder()
{
	if (ObserveExitValue(FExitValue(101)) == 101)
	{
		ReachDestructorExitTerminal(22);
		return 22;
	}
	return 22;
}

int EntryLangDtorOwnerExitTemporaryExceptionOneEventOrder()
{
	ConsumeExitTemporaryAndTerminate(FExitValue(101), 23);
	int Zero = 0;
	return 1 / Zero;
	int Zero = 0;
	return 1 / Zero;
}

int EntryLangDtorOwnerExitReturnedValueConsumeOneEventOrder()
{
	FExitValue ReturnedOne = MakeExitValue(101);
	ReachDestructorExitTerminal(24);
	return 24;
}

int EntryLangDtorOwnerExitReturnedValueDiscardOneEventOrder()
{
	MakeExitValue(101);
	ReachDestructorExitTerminal(25);
	return 25;
}

int EntryLangDtorOwnerExitReturnedValueExceptionOneEventOrder()
{
	FExitValue ReturnedOne = MakeExitValue(101);
	ReachDestructorExitTerminal(26);
	int Zero = 0;
	return 1 / Zero;
}

int EntryLangDtorOwnerExitArgumentCopyReturnOneEventOrder()
{
	FExitValue SourceOne(101);
	ConsumeExitValueByValue(SourceOne, 27, true);
	return 27;
}

int EntryLangDtorOwnerExitArgumentCopyExceptionOneEventOrder()
{
	FExitValue SourceOne(101);
	ConsumeExitValueByValue(SourceOne, 28, true);
	int Zero = 0;
	return 1 / Zero;
	int Zero = 0;
	return 1 / Zero;
}

int EntryLangDtorOwnerExitArgumentCopyAbortOneEventOrder()
{
	FExitValue SourceOne(101);
	ConsumeExitValueByValue(SourceOne, 29, true);
	return 29;
}

int EntryLangDtorOwnerExitReferenceScopeEndOneEventOrder()
{
	FExitReference OwnerOne = FExitReference(101);
	ReachDestructorExitTerminal(30);
	return 30;
}

int EntryLangDtorOwnerExitReferenceAliasScopeEndOneEventOrder()
{
	FExitReference OwnerOne = FExitReference(101);
	FExitReference AliasOne = OwnerOne;
	ReachDestructorExitTerminal(31);
	return 31;
}

int EntryLangDtorOwnerExitReferenceReturnOneEventOrder()
{
	FExitReference OwnerOne = FExitReference(101);
	ReachDestructorExitTerminal(32);
	return 32;
	return 32;
}

int EntryLangDtorOwnerExitReferenceExceptionOneEventOrder()
{
	FExitReference OwnerOne = FExitReference(101);
	ReachDestructorExitTerminal(33);
	int Zero = 0;
	return 1 / Zero;
}

int EntryLangDtorOwnerExitReferenceAbortOneEventOrder()
{
	FExitReference OwnerOne = FExitReference(101);
	ReachDestructorExitTerminal(34);
	return 34;
}

int EntryLangDtorOwnerExitReferenceUnprepareOneEventOrder()
{
	FExitReference OwnerOne = FExitReference(101);
	ReachDestructorExitTerminal(35);
	return 35;
}

int EntryLangDtorOwnerExitModuleDiscardGlobalOneEventOrder()
{
	if (EntryLangDtorOwnerExitModuleDiscardGlobalOneEventOrderExitGlobalReady <= 0)
	{
		ReachDestructorExitTerminal(-1);
	}
	return 36;
}

int EntryLangDtorOwnerExitEngineShutdownGlobalOneEventOrder()
{
	if (EntryLangDtorOwnerExitEngineShutdownGlobalOneEventOrderExitGlobalReady <= 0)
	{
		ReachDestructorExitTerminal(-1);
	}
	return 37;
}

int EntryLangDtorOwnerExitLocalBlockEndSequentialEventOrder()
{
	{
		FExitValue OwnerSequential1(201);
	}
	{
		FExitValue OwnerSequential2(202);
	}
	{
		FExitValue OwnerSequential3(203);
		ReachDestructorExitTerminal(1);
	}
	return 1;
}

int EntryLangDtorOwnerExitLocalBlockEndNestedScopesEventOrder()
{
	{
		FExitValue OwnerScope1(301);
		{
			FExitValue OwnerScope2(302);
			{
				FExitValue OwnerScope3(303);
				ReachDestructorExitTerminal(1);
			}
		}
	}
	return 1;
}

int EntryLangDtorOwnerExitLocalBlockEndNestedCallsEventOrder()
{
	return EntryLangDtorOwnerExitLocalBlockEndNestedCallsEventOrderCall1();
}

int EntryLangDtorOwnerExitLocalBlockEndLoopEventOrder()
{
	for (int ShapeIndex = 1; ShapeIndex <= 3; ++ShapeIndex)
	{
		if (ShapeIndex < 3)
		{
			FExitValue OwnerLoopPassive(500 + ShapeIndex);
		}
		else
		{
			FExitValue OwnerLoopTerminal(500 + ShapeIndex);
			ReachDestructorExitTerminal(1);
		}
	}
	return 1;
}

int EntryLangDtorOwnerExitLocalBlockEndRecursionEventOrder()
{
	return EntryLangDtorOwnerExitLocalBlockEndRecursionEventOrderRecursive(3);
}

int EntryLangDtorOwnerExitModuleDiscardGlobalSequentialEventOrder()
{
	if (EntryLangDtorOwnerExitModuleDiscardGlobalSequentialEventOrderExitGlobalReady <= 0)
	{
		ReachDestructorExitTerminal(-1);
	}
	return 36;
}

int EntryLangDtorOwnerExitModuleDiscardGlobalNestedScopesEventOrder()
{
	if (EntryLangDtorOwnerExitModuleDiscardGlobalNestedScopesEventOrderExitGlobalReady <= 0)
	{
		ReachDestructorExitTerminal(-1);
	}
	return 36;
}

int EntryLangDtorOwnerExitModuleDiscardGlobalNestedCallsEventOrder()
{
	if (EntryLangDtorOwnerExitModuleDiscardGlobalNestedCallsEventOrderExitGlobalReady <= 0)
	{
		ReachDestructorExitTerminal(-1);
	}
	return 36;
}

int EntryLangDtorOwnerExitModuleDiscardGlobalLoopEventOrder()
{
	if (EntryLangDtorOwnerExitModuleDiscardGlobalLoopEventOrderExitGlobalReady <= 0)
	{
		ReachDestructorExitTerminal(-1);
	}
	return 36;
}

int EntryLangDtorOwnerExitModuleDiscardGlobalRecursionEventOrder()
{
	if (EntryLangDtorOwnerExitModuleDiscardGlobalRecursionEventOrderExitGlobalReady <= 0)
	{
		ReachDestructorExitTerminal(-1);
	}
	return 36;
}

int EntryLangDtorOwnerExitLocalExceptionLoopEventOrder()
{
	for (int ShapeIndex = 1; ShapeIndex <= 3; ++ShapeIndex)
	{
		if (ShapeIndex < 3)
		{
			FExitValue OwnerLoopPassive(500 + ShapeIndex);
		}
		else
		{
			FExitValue OwnerLoopTerminal(500 + ShapeIndex);
			ReachDestructorExitTerminal(7);
		}
	}
	int Zero = 0;
	return 1 / Zero;
}

int EntryLangDtorOwnerExitArgumentCopyExceptionSequentialEventOrder()
{
	{
		FExitValue SourceSequential1(201);
		ConsumeExitValueByValue(SourceSequential1, 28, false);
	}
	{
		FExitValue SourceSequential2(202);
		ConsumeExitValueByValue(SourceSequential2, 28, false);
	}
	{
		FExitValue SourceSequential3(203);
		ConsumeExitValueByValue(SourceSequential3, 28, true);
		int Zero = 0;
		return 1 / Zero;
	}
	int Zero = 0;
	return 1 / Zero;
}
