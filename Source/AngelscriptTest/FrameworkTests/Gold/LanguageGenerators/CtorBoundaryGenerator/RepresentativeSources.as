void RecordConstructorBoundaryMarker(int Marker)
{
}

int EntryLangCtorBoundaryDuplicateDefaultSignatureCompileOrExecutionState()
{
	FBoundaryValueDUPLICATEDEFAULTSIGNATURE Value = FBoundaryValueDUPLICATEDEFAULTSIGNATURE();
	return Value.Value;
}

int EntryLangCtorBoundaryDuplicateParameterSignatureCompileOrExecutionState()
{
	return 0;
}

int EntryLangCtorBoundarySuperAfterStatementCompileOrExecutionState()
{
	FBoundaryDerivedSUPERAFTERSTATEMENT Object = FBoundaryDerivedSUPERAFTERSTATEMENT();
	return Object.Value;
}

int EntryLangCtorBoundaryRecursiveConstructorDirectCompileOrExecutionState()
{
	FBoundaryRecursiveRECURSIVECONSTRUCTORDIRECT Root = FBoundaryRecursiveRECURSIVECONSTRUCTORDIRECT();
	return 1;
}

int EntryLangCtorBoundaryModuleDiscardConstValueGlobalCompileOrExecutionState()
{
	return RetainedGlobal.Value;
}

int EntryLangCtorBoundaryEngineShutdownConstValueGlobalCompileOrExecutionState()
{
	return RetainedGlobal.Value;
}

int EntryLangCtorBoundaryMismatchedConstructorNameCompileOrExecutionState()
{
	return 0;
}
