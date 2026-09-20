int EntryLangFnDirectionDefaultInt8ValueNoneExplicitGlobal()
{
	int8 Value = 0;
	return ProbeEntryLangFnDirectionDefaultInt8ValueNoneExplicitGlobal(Value) ? 1 : 0;
}

int EntryLangFnDirectionDefaultFloat32InPresentOmittedGlobal()
{
	return ProbeEntryLangFnDirectionDefaultFloat32InPresentOmittedGlobal() ? 1 : 0;
}

int EntryLangFnDirectionDefaultBoolOutPresentExplicitNamespaceGlobal()
{
	bool Value = false;
	return (DirectionDefaultNamespaceEntryLangFnDirectionDefaultBoolOutPresentExplicitNamespaceGlobal::ProbeEntryLangFnDirectionDefaultBoolOutPresentExplicitNamespaceGlobal(Value) && (Value == true)) ? 1 : 0;
}
