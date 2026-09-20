void RecordConstructorKindSelected(int Marker)
{
}

int EntryLangCtorKindCallLocalImplicitDefaultScriptValue()
{
	FKindObjectIMPLICITDEFAULTSCRIPTVALUE Selected;
	RecordConstructorCallRoute(11);
	return Selected.Value;
}

int EntryLangCtorKindCallTemporaryParameterizedScriptValue()
{
	RecordConstructorCallRoute(12);
	return (FKindObjectPARAMETERIZEDSCRIPTVALUE(7)).Value;
}

int EntryLangCtorKindCallFieldDeclaredDefaultScriptValue()
{
	FKindObjectDECLAREDDEFAULTSCRIPTVALUE Selected = FKindObjectDECLAREDDEFAULTSCRIPTVALUE();
	FKindObjectDECLAREDDEFAULTSCRIPTVALUE Stored = Selected;
	RecordConstructorCallRoute(13);
	return Stored.Value;
}

int EntryLangCtorKindCallReturnOverloadedScriptValue()
{
	RecordConstructorCallRoute(14);
	return (FKindObjectOVERLOADEDSCRIPTVALUE(3, 4)).Value;
}

int EntryLangCtorKindCallArgumentConversionScriptValue()
{
	FKindObjectCONVERSIONSCRIPTVALUE Selected = FKindObjectCONVERSIONSCRIPTVALUE(int64(int8(7)));
	RecordConstructorCallRoute(15);
	return Selected.Value;
}

int EntryLangCtorKindCallBaseCallDeclaredDefaultDerived()
{
	FKindObjectDECLAREDDEFAULTDERIVED Selected = FKindObjectDECLAREDDEFAULTDERIVED();
	super();
	RecordConstructorCallRoute(16);
	return Selected.Value;
}

int EntryLangCtorKindCallCopyDeclarationCopyScriptValue()
{
	FKindObjectCOPYSCRIPTVALUE CopySource;
	CopySource.Value = 7;
	FKindObjectCOPYSCRIPTVALUE Selected(CopySource);
	CopySource.Value = 9;
	RecordConstructorTransferObservation(CopySource.Value, Selected.Value);
	FKindObjectCOPYSCRIPTVALUE Copied = Selected;
	Selected.Value = 9;
	RecordConstructorTransferObservation(Selected.Value, Copied.Value);
	RecordConstructorCallRoute(17);
	return Copied.Value;
}

int EntryLangCtorKindCallAssignmentImplicitDefaultScriptValue()
{
	FKindObjectIMPLICITDEFAULTSCRIPTVALUE Selected;
	FKindObjectIMPLICITDEFAULTSCRIPTVALUE Assigned;
	Assigned = Selected;
	Selected.Value = 9;
	RecordConstructorTransferObservation(Selected.Value, Assigned.Value);
	RecordConstructorCallRoute(18);
	return Assigned.Value;
}

int EntryLangCtorKindCallLocalParameterizedNativeValue()
{
	FNativeCaseValue Selected = FNativeCaseValue(7);
	RecordConstructorCallRoute(11);
	return Selected.Value;
}

int EntryLangCtorKindCallBaseCallImplicitDefaultScriptValue()
{
	FKindObjectIMPLICITDEFAULTSCRIPTVALUE Selected = FKindObjectIMPLICITDEFAULTSCRIPTVALUE();
	super();
	RecordConstructorCallRoute(16);
	return Selected.Value;
}
