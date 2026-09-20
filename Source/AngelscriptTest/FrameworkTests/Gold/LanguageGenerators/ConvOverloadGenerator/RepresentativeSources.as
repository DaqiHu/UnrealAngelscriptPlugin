int EntryLangConvOverloadOverloadIdentityExact()
{
	int SourceValue = 11;
	return SelectResolutionOverloadIdentityExact(SourceValue);
}

int EntryLangConvOverloadOperatorPromotionSelectedConversion()
{
	int8 SourceValue = 12;
	FResolutionOperatorContextOperatorPromotionSelectedConversion Context;
	return Context + SourceValue;
}

int EntryLangConvOverloadDefaultArgumentConstructorExact()
{
	int SourceValue = 14;
	return DefaultResolutionDefaultArgumentConstructorExact(SourceValue);
}

int EntryLangConvOverloadPropertyOperatorExact()
{
	FResolutionOperatorSourcePropertyOperatorExact SourceValue(15);
	FResolutionHolderPropertyOperatorExact Holder;
	Holder.Value = SourceValue;
	return Holder.Result;
}

int EntryLangConvOverloadIndexReferenceCastExact()
{
	FResolutionReferenceDerivedIndexReferenceCastExact@ SourceValue = nullptr;
	FResolutionIndexIndexReferenceCastExact Index;
	return Index[SourceValue];
}

int EntryLangConvOverloadConditionalNarrowingSelectedConversion()
{
	int64 SourceValue = 13;
	int TargetValue = true ? SourceValue : 0;
	return ObserveResolutionConditionalNarrowingSelectedConversion(TargetValue);
}

int EntryLangConvOverloadOverloadIdentityAmbiguous()
{
	int SourceValue = 11;
	return SelectAmbiguousResolutionOverloadIdentityAmbiguous(SourceValue);
}

int EntryLangConvOverloadOverloadIdentityRejected()
{
	int SourceValue = 11;
	return SelectRejectedResolutionOverloadIdentityRejected(SourceValue);
}
