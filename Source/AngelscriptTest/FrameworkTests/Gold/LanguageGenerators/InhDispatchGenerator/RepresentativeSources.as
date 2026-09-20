class FDispatchRootBaseFieldDerivedObjectDirect
{
	int DispatchField = 701;

	int ReadDispatchField()
	{
		return DispatchField;
	}
}

class FDispatchPrimaryBaseFieldDerivedObjectDirect : FDispatchRootBaseFieldDerivedObjectDirect
{
	int ReadDispatchField() override
	{
		return DispatchField + 10;
	}
}

int ProbeDispatchBaseFieldDerivedObjectDirect(FDispatchPrimaryBaseFieldDerivedObjectDirect Object)
{
	return Object.DispatchField;
}

int EntryLangInhDispatchBaseFieldDerivedObjectDirect()
{
	FDispatchPrimaryBaseFieldDerivedObjectDirect Object;
	return ProbeDispatchBaseFieldDerivedObjectDirect(Object);
}

class FDispatchRootBaseNonvirtualMethodDerivedObjectDirect
{
	int StableValue() final
	{
		return 100;
	}
}

class FDispatchPrimaryBaseNonvirtualMethodDerivedObjectDirect : FDispatchRootBaseNonvirtualMethodDerivedObjectDirect
{
}

int ProbeDispatchBaseNonvirtualMethodDerivedObjectDirect(FDispatchPrimaryBaseNonvirtualMethodDerivedObjectDirect Object)
{
	return Object.StableValue();
}

int EntryLangInhDispatchBaseNonvirtualMethodDerivedObjectDirect()
{
	FDispatchPrimaryBaseNonvirtualMethodDerivedObjectDirect Object;
	return ProbeDispatchBaseNonvirtualMethodDerivedObjectDirect(Object);
}

class FDispatchRootBaseFieldExplicitBaseDirect
{
	int DispatchField = 701;

	int ReadDispatchField()
	{
		return DispatchField;
	}
}

class FDispatchPrimaryBaseFieldExplicitBaseDirect : FDispatchRootBaseFieldExplicitBaseDirect
{
	int ReadDispatchField() override
	{
		return DispatchField + 10;
	}

	int ProbeMember()
	{
		return FDispatchRootBaseFieldExplicitBaseDirect::ReadDispatchField();
	}
}

int ProbeDispatchBaseFieldExplicitBaseDirect(FDispatchPrimaryBaseFieldExplicitBaseDirect Object)
{
	return Object.ProbeMember();
}

int EntryLangInhDispatchBaseFieldExplicitBaseDirect()
{
	FDispatchPrimaryBaseFieldExplicitBaseDirect Object;
	return ProbeDispatchBaseFieldExplicitBaseDirect(Object);
}

class FDispatchRootTwoLevelsFieldBaseViewVirtualRoute
{
	int DispatchField = 701;

	int ReadDispatchField()
	{
		return DispatchField;
	}
}

class FDispatchLevel1TwoLevelsFieldBaseViewVirtualRoute : FDispatchRootTwoLevelsFieldBaseViewVirtualRoute
{
	int ReadDispatchField() override
	{
		return DispatchField + 10;
	}
}

class FDispatchPrimaryTwoLevelsFieldBaseViewVirtualRoute : FDispatchLevel1TwoLevelsFieldBaseViewVirtualRoute
{
	int ReadDispatchField() override
	{
		return DispatchField + 20;
	}
}

int ProbeDispatchTwoLevelsFieldBaseViewVirtualRoute(FDispatchRootTwoLevelsFieldBaseViewVirtualRoute Object)
{
	FDispatchRootTwoLevelsFieldBaseViewVirtualRoute BaseView = Object;
	return BaseView.ReadDispatchField();
}

int EntryLangInhDispatchTwoLevelsFieldBaseViewVirtualRoute()
{
	FDispatchPrimaryTwoLevelsFieldBaseViewVirtualRoute Object;
	return ProbeDispatchTwoLevelsFieldBaseViewVirtualRoute(Object);
}

class FDispatchRootThreeLevelsSetterMethodDerivedObjectDirect
{
	int SetValue(int InValue)
	{
		return 100 + InValue;
	}
}

class FDispatchLevel1ThreeLevelsSetterMethodDerivedObjectDirect : FDispatchRootThreeLevelsSetterMethodDerivedObjectDirect
{
	int SetValue(int InValue) override
	{
		return 200 + InValue;
	}
}

class FDispatchLevel2ThreeLevelsSetterMethodDerivedObjectDirect : FDispatchLevel1ThreeLevelsSetterMethodDerivedObjectDirect
{
	int SetValue(int InValue) override
	{
		return 300 + InValue;
	}
}

class FDispatchPrimaryThreeLevelsSetterMethodDerivedObjectDirect : FDispatchLevel2ThreeLevelsSetterMethodDerivedObjectDirect
{
	int SetValue(int InValue) override
	{
		return 400 + InValue;
	}
}

int ProbeDispatchThreeLevelsSetterMethodDerivedObjectDirect(FDispatchPrimaryThreeLevelsSetterMethodDerivedObjectDirect Object)
{
	return Object.SetValue(7);
}

int EntryLangInhDispatchThreeLevelsSetterMethodDerivedObjectDirect()
{
	FDispatchPrimaryThreeLevelsSetterMethodDerivedObjectDirect Object;
	return ProbeDispatchThreeLevelsSetterMethodDerivedObjectDirect(Object);
}
