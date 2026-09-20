class FRulePrimaryConcreteInstantiateRuntime
{
	int Value()
	{
		return 41;
	}
}

int EntryLangInhClassRuleConcreteInstantiateRuntime()
{
	FRulePrimaryConcreteInstantiateRuntime Object;
	return Object.Value();
}

class FRuleBaseConcreteBaseInheritRuntime
{
	int Value()
	{
		return 31;
	}
}

class FRulePrimaryConcreteBaseInheritRuntime : FRuleBaseConcreteBaseInheritRuntime
{
}

int EntryLangInhClassRuleConcreteBaseInheritRuntime()
{
	FRulePrimaryConcreteBaseInheritRuntime Object;
	return Object.Value();
}

class FRuleBaseFinalMethodInheritRuntime
{
	int Value() final
	{
		return 61;
	}
}

class FRulePrimaryFinalMethodInheritRuntime : FRuleBaseFinalMethodInheritRuntime
{
}

int EntryLangInhClassRuleFinalMethodInheritRuntime()
{
	FRulePrimaryFinalMethodInheritRuntime Object;
	return Object.Value();
}

class FRuleBaseImplicitOverrideWithoutKeywordRuntime
{
	int Value()
	{
		return 121;
	}
}

class FRulePrimaryImplicitOverrideWithoutKeywordRuntime : FRuleBaseImplicitOverrideWithoutKeywordRuntime
{
	int Value()
	{
		return 122;
	}
}

int EntryLangInhClassRuleImplicitOverrideWithoutKeywordRuntime()
{
	FRulePrimaryImplicitOverrideWithoutKeywordRuntime Object;
	return Object.Value();
}

class FRuleBaseExactOverrideRuntime
{
	int Value()
	{
		return 131;
	}
}

class FRulePrimaryExactOverrideRuntime : FRuleBaseExactOverrideRuntime
{
	int Value() override
	{
		return 132;
	}
}

int EntryLangInhClassRuleExactOverrideRuntime()
{
	FRulePrimaryExactOverrideRuntime Object;
	return Object.Value();
}

class FRuleBaseDeepOverrideRuntime
{
	int Value()
	{
		return 161;
	}
}

class FRuleMiddleDeepOverrideRuntime : FRuleBaseDeepOverrideRuntime
{
	int Value() override
	{
		return 162;
	}
}

class FRulePrimaryDeepOverrideRuntime : FRuleMiddleDeepOverrideRuntime
{
	int Value() override
	{
		return 163;
	}
}

int EntryLangInhClassRuleDeepOverrideRuntime()
{
	FRulePrimaryDeepOverrideRuntime Object;
	return Object.Value();
}

abstract class FRulePrimaryAbstractClassKeywordRejectedCompile
{
	int Value()
	{
		return 11;
	}
}

int EntryLangInhClassRuleAbstractClassKeywordRejectedCompile()
{
	FRulePrimaryAbstractClassKeywordRejectedCompile Object;
	return Object.Value();
}
