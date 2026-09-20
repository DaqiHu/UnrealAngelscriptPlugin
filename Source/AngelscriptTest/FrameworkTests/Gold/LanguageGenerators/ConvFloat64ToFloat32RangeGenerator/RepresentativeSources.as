int EntryLangConvFloat64ToFloat32RangeAssignmentAboveTargetMax()
{
	float SourceValue = 6.805646932e+38;
	float32 Converted;
	Converted = SourceValue;
	return ObserveFloat32Bits(Converted);
}

int EntryLangConvFloat64ToFloat32RangeArgumentBelowTargetMin()
{
	float SourceValue = -6.805646932e+38;
	float32 Converted = PassNumericBoundaryTargetFloat32(SourceValue);
	return ObserveFloat32Bits(Converted);
}

int EntryLangConvFloat64ToFloat32RangeReturnAboveTargetMax()
{
	float SourceValue = 6.805646932e+38;
	float32 Converted = ReturnNumericBoundaryTargetFloat64Float32(SourceValue);
	return ObserveFloat32Bits(Converted);
}

int EntryLangConvFloat64ToFloat32RangeExplicitCastBelowTargetMin()
{
	float SourceValue = -6.805646932e+38;
	float32 Converted = float32(SourceValue);
	return ObserveFloat32Bits(Converted);
}
