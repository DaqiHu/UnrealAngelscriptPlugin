typedef int NativeCaseAlias;

int EntryLangCtorParamSelectOneExactInt8()
{
	FConstructorProbeONEEXACTINT8 Value(ObserveINT8(0, int8(1)));
	return Value.Checksum;
}

int EntryLangCtorParamSelectTwoPromotionInt16()
{
	FConstructorProbeTWOPROMOTIONINT16 Value(ObserveINT16(0, int16(1)), ObserveINT16(1, int16(2)));
	return Value.Checksum;
}

int EntryLangCtorParamSelectFiveExplicitConversionBool()
{
	FConstructorProbeFIVEEXPLICITCONVERSIONBOOL Value(int(ObserveBOOL(0, true)), int(ObserveBOOL(1, false)), int(ObserveBOOL(2, true)), int(ObserveBOOL(3, false)), int(ObserveBOOL(4, true)));
	return Value.Checksum;
}

int EntryLangCtorParamSelectSixteenExactInt()
{
	FConstructorProbeSIXTEENEXACTINT Value(ObserveINT(0, int(1)), ObserveINT(1, int(2)), ObserveINT(2, int(3)), ObserveINT(3, int(4)), ObserveINT(4, int(5)), ObserveINT(5, int(6)), ObserveINT(6, int(7)), ObserveINT(7, int(8)), ObserveINT(8, int(9)), ObserveINT(9, int(10)), ObserveINT(10, int(11)), ObserveINT(11, int(12)), ObserveINT(12, int(13)), ObserveINT(13, int(14)), ObserveINT(14, int(15)), ObserveINT(15, int(16)));
	return Value.Checksum;
}

int EntryLangCtorParamSelectOneExactTypedef()
{
	FConstructorProbeONEEXACTTYPEDEF Value(ObserveTYPEDEF(0, NativeCaseAlias(1)));
	return Value.Checksum;
}

int EntryLangCtorParamSelectOneExactScriptValue()
{
	FConstructorProbeONEEXACTSCRIPTVALUE Value(ObserveSCRIPTVALUE(0, FScriptCaseValue(1)));
	return Value.Checksum;
}

int EntryLangCtorParamSelectOneMissingInt8()
{
	FConstructorProbeONEMISSINGINT8 Value(ObserveINT8(0, int8(1)));
	return Value.Checksum;
}

int EntryLangCtorParamSelectOneAmbiguousInt8()
{
	FConstructorProbeONEAMBIGUOUSINT8 Value(ObserveINT8(0, int8(1)));
	return Value.Checksum;
}
