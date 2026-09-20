int EntryLangConvFailureAmbiguousConstructorFreshModule()
{
	// Recovery: FRESH_MODULE
	return SelectAmbiguousConstructor(int8(1)); // CONVERSION_CAUSE
}

int EntryLangConvFailureAmbiguousOperatorSameModuleOrContext()
{
	// Recovery: SAME_MODULE_OR_CONTEXT
	FAmbiguousConversionOperator SourceValue;
	return SourceValue + int8(1); // CONVERSION_CAUSE
}

int EntryLangConvFailureNullValueTargetFreshModule()
{
	// Recovery: FRESH_MODULE
	FNullConversionTarget Target = nullptr;
	return Target.Value; // CONVERSION_CAUSE
}

int EntryLangConvFailureConversionExceptionFreshModule()
{
	// Recovery: FRESH_MODULE
	FThrowingConversion SourceValue(9);
	int ConvertedValue = SourceValue; // CONVERSION_CAUSE
	return ConvertedValue;
}

int EntryLangConvFailureImplicitNarrowingFreshModule()
{
	// Recovery: FRESH_MODULE
	float SourceValue = 1.5;
	return RequireFailureEnum(SourceValue); // CONVERSION_CAUSE
}
