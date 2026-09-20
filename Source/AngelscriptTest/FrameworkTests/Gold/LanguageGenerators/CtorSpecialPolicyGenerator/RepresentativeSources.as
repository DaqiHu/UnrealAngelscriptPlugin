void RecordConstructorPolicyMarker(int Marker)
{
}

int EntryLangCtorSpecialPolicyCompileImplicitStructDefault()
{
	FPolicyValueIMPLICITSTRUCTDEFAULT Value;
	RecordConstructorPolicyMarker(101);
	return Value.Value + Value.Payload.Value;
}

int EntryLangCtorSpecialPolicyCompileDeclaredStructDefault()
{
	FPolicyValueDECLAREDSTRUCTDEFAULT Value;
	RecordConstructorPolicyMarker(202);
	return Value.Value + Value.Payload.Value;
}

int EntryLangCtorSpecialPolicyCompileParameterPreservesGeneratedDefault()
{
	FPolicyValuePARAMETERPRESERVESGENERATEDDEFAULT DefaultValue;
	FPolicyValuePARAMETERPRESERVESGENERATEDDEFAULT ParameterValue(13);
	RecordConstructorPolicyMarker(302);
	return DefaultValue.Value + ParameterValue.Value;
}

int EntryLangCtorSpecialPolicyCompileDeclaredStructCopy()
{
	FPolicyValueDECLAREDSTRUCTCOPY Source;
	Source.Value = 16;
	FPolicyValueDECLAREDSTRUCTCOPY Target(Source);
	RecordConstructorPolicyMarker(603);
	return Target.Value + Target.Payload.Value;
}

int EntryLangCtorSpecialPolicyCompileImplicitStructAssignment()
{
	FPolicyValueIMPLICITSTRUCTASSIGNMENT Source;
	Source.Value = 17;
	FPolicyValueIMPLICITSTRUCTASSIGNMENT Target;
	Target = Source;
	RecordConstructorPolicyMarker(701);
	return Target.Value + Target.Payload.Value;
}

int EntryLangCtorSpecialPolicyCompileDeclaredStructAssignment()
{
	FPolicyValueDECLAREDSTRUCTASSIGNMENT Source;
	Source.Value = 18;
	FPolicyValueDECLAREDSTRUCTASSIGNMENT Target;
	Target = Source;
	RecordConstructorPolicyMarker(802);
	return Target.Value + Target.Payload.Value;
}

int EntryLangCtorSpecialPolicyCompileClassFactoryDefault()
{
	FPolicyClassCLASSFACTORYDEFAULT Value = FPolicyClassCLASSFACTORYDEFAULT();
	RecordConstructorPolicyMarker(1002);
	return Value.Value;
}

int EntryLangCtorSpecialPolicyCompileClassParameterFactory()
{
	FPolicyClassCLASSPARAMETERFACTORY Value = FPolicyClassCLASSPARAMETERFACTORY(21);
	RecordConstructorPolicyMarker(1102);
	return Value.Value;
}

int EntryLangCtorSpecialPolicyCompileDerivedGeneratedDefault()
{
	FPolicyDerivedDERIVEDGENERATEDDEFAULT Value = FPolicyDerivedDERIVEDGENERATEDDEFAULT();
	RecordConstructorPolicyMarker(1202);
	return Value.BaseValue + Value.DerivedValue;
}

int EntryLangCtorSpecialPolicyCompileDerivedExplicitSuper()
{
	FPolicyDerivedDERIVEDEXPLICITSUPER Value = FPolicyDerivedDERIVEDEXPLICITSUPER();
	RecordConstructorPolicyMarker(1303);
	return Value.BaseValue + Value.DerivedValue;
}

int EntryLangCtorSpecialPolicyCompileAssignmentSelfStability()
{
	FPolicyValueASSIGNMENTSELFSTABILITY Value;
	Value.Value = 42;
	Value.Payload.Value = 42;
	Value = Value;
	RecordConstructorPolicyMarker(1602);
	return Value.Value + Value.Payload.Value;
}

int EntryLangCtorSpecialPolicyCompileImplicitStructCopy()
{
	FPolicyValueIMPLICITSTRUCTCOPY Source;
	Source.Value = 15;
	FPolicyValueIMPLICITSTRUCTCOPY Target(Source);
	RecordConstructorPolicyMarker(501);
	return Target.Value + Target.Payload.Value;
}
