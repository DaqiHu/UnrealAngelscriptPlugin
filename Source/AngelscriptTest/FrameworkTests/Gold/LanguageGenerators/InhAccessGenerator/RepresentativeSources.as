class FAccessBaseDefaultFieldOwner
{
	int Value = 41;

	int ProbeAccess()
	{
		return Value;
	}
}

class FAccessDerivedDefaultFieldOwner : FAccessBaseDefaultFieldOwner
{
}

class FAccessDeepDefaultFieldOwner : FAccessDerivedDefaultFieldOwner
{
}

int EntryLangInhAccessDefaultFieldOwner()
{
	FAccessBaseDefaultFieldOwner Object;
	return Object.ProbeAccess();
}

class FAccessBaseDefaultMethodOwner
{
	int ReadValue()
	{
		return 42;
	}

	int ProbeAccess()
	{
		return ReadValue();
	}
}

class FAccessDerivedDefaultMethodOwner : FAccessBaseDefaultMethodOwner
{
}

class FAccessDeepDefaultMethodOwner : FAccessDerivedDefaultMethodOwner
{
}

int EntryLangInhAccessDefaultMethodOwner()
{
	FAccessBaseDefaultMethodOwner Object;
	return Object.ProbeAccess();
}

class FAccessBaseDefaultGetterSetterMethodOwner
{
	int StoredValue = 43;

	int GetValue()
	{
		return StoredValue;
	}

	void SetValue(int InValue)
	{
		StoredValue = InValue;
	}

	int ProbeAccess()
	{
		SetValue(73);
		return GetValue();
	}
}

class FAccessDerivedDefaultGetterSetterMethodOwner : FAccessBaseDefaultGetterSetterMethodOwner
{
}

class FAccessDeepDefaultGetterSetterMethodOwner : FAccessDerivedDefaultGetterSetterMethodOwner
{
}

int EntryLangInhAccessDefaultGetterSetterMethodOwner()
{
	FAccessBaseDefaultGetterSetterMethodOwner Object;
	return Object.ProbeAccess();
}

class FAccessBaseDefaultConstructorOwner
{
	int StoredValue = 44;

	FAccessBaseDefaultConstructorOwner()
	{
		StoredValue = 45;
	}

	FAccessBaseDefaultConstructorOwner MakeOwner()
	{
		return FAccessBaseDefaultConstructorOwner();
	}
}

class FAccessDerivedDefaultConstructorOwner : FAccessBaseDefaultConstructorOwner
{
	FAccessDerivedDefaultConstructorOwner()
	{
		super();
	}
}

class FAccessDeepDefaultConstructorOwner : FAccessDerivedDefaultConstructorOwner
{
	FAccessDeepDefaultConstructorOwner()
	{
		super();
	}
}

int EntryLangInhAccessDefaultConstructorOwner()
{
	FAccessBaseDefaultConstructorOwner Owner;
	FAccessBaseDefaultConstructorOwner Created = Owner.MakeOwner();
	return 47;
}

class FAccessBaseDefaultConstructorDirectDerived
{
	int StoredValue = 44;

	FAccessBaseDefaultConstructorDirectDerived()
	{
		StoredValue = 45;
	}
}

class FAccessDerivedDefaultConstructorDirectDerived : FAccessBaseDefaultConstructorDirectDerived
{
	FAccessDerivedDefaultConstructorDirectDerived()
	{
		super();
	}
}

class FAccessDeepDefaultConstructorDirectDerived : FAccessDerivedDefaultConstructorDirectDerived
{
	FAccessDeepDefaultConstructorDirectDerived()
	{
		super();
	}
}

int EntryLangInhAccessDefaultConstructorDirectDerived()
{
	FAccessDerivedDefaultConstructorDirectDerived Created;
	return 48;
}

class FAccessBaseDefaultConstructorUnrelated
{
	int StoredValue = 44;

	FAccessBaseDefaultConstructorUnrelated()
	{
		StoredValue = 45;
	}
}

class FAccessDerivedDefaultConstructorUnrelated : FAccessBaseDefaultConstructorUnrelated
{
	FAccessDerivedDefaultConstructorUnrelated()
	{
		super();
	}
}

class FAccessDeepDefaultConstructorUnrelated : FAccessDerivedDefaultConstructorUnrelated
{
	FAccessDeepDefaultConstructorUnrelated()
	{
		super();
	}
}

class FAccessUnrelatedDefaultConstructorUnrelated
{
	int ProbeAccess()
	{
		FAccessBaseDefaultConstructorUnrelated Object;
		return 46;
	}
}

int EntryLangInhAccessDefaultConstructorUnrelated()
{
	FAccessUnrelatedDefaultConstructorUnrelated Observer;
	return Observer.ProbeAccess();
}

class FAccessBaseDefaultFieldGlobal
{
	int Value = 41;
}

class FAccessDerivedDefaultFieldGlobal : FAccessBaseDefaultFieldGlobal
{
}

class FAccessDeepDefaultFieldGlobal : FAccessDerivedDefaultFieldGlobal
{
}

int ProbeGlobalAccessDefaultFieldGlobal(FAccessBaseDefaultFieldGlobal Object)
{
	return Object.Value;
}

int EntryLangInhAccessDefaultFieldGlobal()
{
	FAccessBaseDefaultFieldGlobal Object;
	return ProbeGlobalAccessDefaultFieldGlobal(Object);
}

class FAccessBasePrivateFieldDirectDerived
{
	private int Value = 41;
}

class FAccessDerivedPrivateFieldDirectDerived : FAccessBasePrivateFieldDirectDerived
{
	int ProbeAccess()
	{
		return Value;
	}
}

class FAccessDeepPrivateFieldDirectDerived : FAccessDerivedPrivateFieldDirectDerived
{
}

int EntryLangInhAccessPrivateFieldDirectDerived()
{
	FAccessDerivedPrivateFieldDirectDerived Object;
	return Object.ProbeAccess();
}
