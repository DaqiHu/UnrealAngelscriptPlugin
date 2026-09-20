class FSignatureBaseParameterTypeExactBase
{
	int Resolve(int Value)
	{
		return 101 + Value;
	}

	int ProbeBase()
	{
		return Resolve(1);
	}
}

class FSignaturePrimaryParameterTypeExactBase : FSignatureBaseParameterTypeExactBase
{
	int Resolve(int Value) override
	{
		return 202 + int(Value);
	}

	int ProbeDerived()
	{
		return Resolve(1);
	}

	int ProbeExplicitBase()
	{
		return FSignatureBaseParameterTypeExactBase::Resolve(1);
	}
}

int EntryLangInhOverrideSignatureParameterTypeExactBase()
{
	FSignaturePrimaryParameterTypeExactBase Object;
	FSignatureBaseParameterTypeExactBase BaseView = Object;
	return BaseView.ProbeBase();
}

class FSignatureBaseParameterTypeExactDerived
{
	int Resolve(int Value)
	{
		return 101 + Value;
	}

	int ProbeBase()
	{
		return Resolve(1);
	}
}

class FSignaturePrimaryParameterTypeExactDerived : FSignatureBaseParameterTypeExactDerived
{
	int Resolve(int Value) override
	{
		return 202 + int(Value);
	}

	int ProbeDerived()
	{
		return Resolve(1);
	}

	int ProbeExplicitBase()
	{
		return FSignatureBaseParameterTypeExactDerived::Resolve(1);
	}
}

int EntryLangInhOverrideSignatureParameterTypeExactDerived()
{
	FSignaturePrimaryParameterTypeExactDerived Object;
	return Object.ProbeDerived();
}

class FSignatureBaseParameterTypeExactExplicitBase
{
	int Resolve(int Value)
	{
		return 101 + Value;
	}

	int ProbeBase()
	{
		return Resolve(1);
	}
}

class FSignaturePrimaryParameterTypeExactExplicitBase : FSignatureBaseParameterTypeExactExplicitBase
{
	int Resolve(int Value) override
	{
		return 202 + int(Value);
	}

	int ProbeDerived()
	{
		return Resolve(1);
	}

	int ProbeExplicitBase()
	{
		return FSignatureBaseParameterTypeExactExplicitBase::Resolve(1);
	}
}

int EntryLangInhOverrideSignatureParameterTypeExactExplicitBase()
{
	FSignaturePrimaryParameterTypeExactExplicitBase Object;
	return Object.ProbeExplicitBase();
}

class FSignatureBaseParameterCountCompatibleOverloadDerived
{
	int Resolve(int Value)
	{
		return 101 + Value;
	}

	int ProbeBase()
	{
		return Resolve(1);
	}
}

class FSignaturePrimaryParameterCountCompatibleOverloadDerived : FSignatureBaseParameterCountCompatibleOverloadDerived
{
	int Resolve(int Left, int Right)
	{
		return 202 + Left + Right;
	}

	int ProbeDerived()
	{
		return Resolve(1, 2);
	}

	int ProbeExplicitBase()
	{
		return FSignatureBaseParameterCountCompatibleOverloadDerived::Resolve(1);
	}
}

int EntryLangInhOverrideSignatureParameterCountCompatibleOverloadDerived()
{
	FSignaturePrimaryParameterCountCompatibleOverloadDerived Object;
	return Object.ProbeDerived();
}

class FSignatureBaseNameHidingCompatibleOverloadDerived
{
	int Resolve(int Value)
	{
		return 101 + Value;
	}

	int ProbeBase()
	{
		return Resolve(1);
	}
}

class FSignaturePrimaryNameHidingCompatibleOverloadDerived : FSignatureBaseNameHidingCompatibleOverloadDerived
{
	int Resolve(float Value)
	{
		return 202 + int(Value);
	}

	int ProbeDerived()
	{
		return Resolve(2.5f);
	}

	int ProbeExplicitBase()
	{
		return FSignatureBaseNameHidingCompatibleOverloadDerived::Resolve(1);
	}
}

int EntryLangInhOverrideSignatureNameHidingCompatibleOverloadDerived()
{
	FSignaturePrimaryNameHidingCompatibleOverloadDerived Object;
	return Object.ProbeDerived();
}

class FSignatureBaseParameterTypeIncompatibleBase
{
	int Resolve(int Value)
	{
		return 101 + Value;
	}

	int ProbeBase()
	{
		return Resolve(1);
	}
}

class FSignaturePrimaryParameterTypeIncompatibleBase : FSignatureBaseParameterTypeIncompatibleBase
{
	int Resolve(float Value) override
	{
		return 202 + int(Value);
	}

	int ProbeDerived()
	{
		return Resolve(1.5f);
	}

	int ProbeExplicitBase()
	{
		return FSignatureBaseParameterTypeIncompatibleBase::Resolve(1);
	}
}

int EntryLangInhOverrideSignatureParameterTypeIncompatibleBase()
{
	FSignaturePrimaryParameterTypeIncompatibleBase Object;
	FSignatureBaseParameterTypeIncompatibleBase BaseView = Object;
	return BaseView.ProbeBase();
}
