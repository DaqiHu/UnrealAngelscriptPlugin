int EntryLangFnIndirectMixinDeclarationMetadata()
{
	FMixinReceiverEntryLangFnIndirectMixinDeclarationMetadata Value;
	return Value.AddValueEntryLangFnIndirectMixinDeclarationMetadata(2);
}

int EntryLangFnIndirectMixinCompatibleNested()
{
	FMixinReceiverEntryLangFnIndirectMixinCompatibleNested Value;
	return InvokeMixinEntryLangFnIndirectMixinCompatibleNested(Value);
}

int EntryLangFnIndirectMixinNullOrUnbound()
{
	FMixinReceiverEntryLangFnIndirectMixinNullOrUnbound Value;
	return Value.MissingMixin(2);
}

int EntryLangFnIndirectMixinIncompatibleSignature()
{
	int Value = 40;
	return Value.AddValueEntryLangFnIndirectMixinIncompatibleSignature(2);
}
