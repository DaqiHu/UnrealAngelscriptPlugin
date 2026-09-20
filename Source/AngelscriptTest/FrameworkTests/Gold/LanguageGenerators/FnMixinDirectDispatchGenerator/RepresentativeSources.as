int EntryLangFnMixinDirectDispatchGlobal()
{
	CounterEntryLangFnMixinDirectDispatchGlobal Value;
	Value.AddToCounterEntryLangFnMixinDirectDispatchGlobal(3);
	return Value.Value;
}

int EntryLangFnMixinDirectDispatchNested()
{
	ToolsEntryLangFnMixinDirectDispatchNested::Counter Value;
	Value.AddToCounter(3);
	return Value.Value;
}
