int ProbeLangFnOverloadTypeExact(int Value)
{
	return 101;
}

int ProbeLangFnOverloadTypeExact(double Value)
{
	return 111;
}

int EntryLangFnOverloadTypeExact()
{
	return ProbeLangFnOverloadTypeExact(7);
}

int ProbeLangFnOverloadTypePromotion(double Value)
{
	return 102;
}

int EntryLangFnOverloadTypePromotion()
{
	return ProbeLangFnOverloadTypePromotion(7);
}

int ProbeLangFnOverloadTypeAmbiguous(float Value)
{
	return 1;
}

int ProbeLangFnOverloadTypeAmbiguous(double Value)
{
	return 2;
}

int EntryLangFnOverloadTypeAmbiguous()
{
	return ProbeLangFnOverloadTypeAmbiguous(7);
}

struct FValue
{
	int Value = 0;
}

int ProbeLangFnOverloadTypeMissing(FValue Value)
{
	return 1;
}

int EntryLangFnOverloadTypeMissing()
{
	return ProbeLangFnOverloadTypeMissing(true);
}

int ProbeLangFnOverloadArityExact(int Value)
{
	return 211;
}

int ProbeLangFnOverloadArityExact(int Left, int Right)
{
	return 201;
}

int EntryLangFnOverloadArityExact()
{
	return ProbeLangFnOverloadArityExact(3, 4);
}

struct FOwnerLangFnOverloadConstExact
{
	int ProbeLangFnOverloadConstExact()
	{
		return 301;
	}

	int ProbeLangFnOverloadConstExact() const
	{
		return 311;
	}

	int Invoke()
	{
		return ProbeLangFnOverloadConstExact();
	}
}

int EntryLangFnOverloadConstExact()
{
	FOwnerLangFnOverloadConstExact Owner;
	return Owner.Invoke();
}

int ProbeLangFnOverloadDirectionExact(int& in Value)
{
	return 401;
}

int EntryLangFnOverloadDirectionExact()
{
	int Value = 7;
	return ProbeLangFnOverloadDirectionExact(Value);
}

namespace A
{
	int ProbeLangFnOverloadNamespaceExact(int Value)
	{
		return 501;
	}
}

namespace B
{
	int ProbeLangFnOverloadNamespaceExact(int Value)
	{
		return 599;
	}
}

int EntryLangFnOverloadNamespaceExact()
{
	return A::ProbeLangFnOverloadNamespaceExact(7);
}

int EntryLangFnOverloadNamespaceMissing()
{
	return Missing::Probe(7);
}

int ProbeLangFnOverloadConversionExact(int Value)
{
	return 701;
}

int ProbeLangFnOverloadConversionExact(double Value)
{
	return 711;
}

int EntryLangFnOverloadConversionExact()
{
	return ProbeLangFnOverloadConversionExact(int(7));
}

int ProbeLangFnOverloadArityPromotion(double Left, double Right)
{
	return 202;
}

int EntryLangFnOverloadArityPromotion()
{
	return ProbeLangFnOverloadArityPromotion(3, 4);
}

int ProbeLangFnOverloadArityAmbiguous(int Value)
{
	return 1;
}

int ProbeLangFnOverloadArityAmbiguous(int Value, int Extra = 2)
{
	return 2;
}

int EntryLangFnOverloadArityAmbiguous()
{
	return ProbeLangFnOverloadArityAmbiguous(3);
}

int ProbeLangFnOverloadArityMissing(int Left, int Right)
{
	return 1;
}

int EntryLangFnOverloadArityMissing()
{
	return ProbeLangFnOverloadArityMissing(1, 2, 3);
}

int ProbeLangFnOverloadDefaultExact(int Value)
{
	return 611;
}

int ProbeLangFnOverloadDefaultExact(int Left, int Right)
{
	return 601;
}

int EntryLangFnOverloadDefaultExact()
{
	return ProbeLangFnOverloadDefaultExact(3, 4);
}

int ProbeLangFnOverloadDefaultMissing(int Left, int Right)
{
	return 1;
}

int EntryLangFnOverloadDefaultMissing()
{
	return ProbeLangFnOverloadDefaultMissing();
}

struct FOwnerLangFnOverloadConstPromotion
{
	int ProbeLangFnOverloadConstPromotion() const
	{
		return 302;
	}

	int Invoke()
	{
		return ProbeLangFnOverloadConstPromotion();
	}
}

int EntryLangFnOverloadConstPromotion()
{
	FOwnerLangFnOverloadConstPromotion Owner;
	return Owner.Invoke();
}

struct FOwnerLangFnOverloadConstAmbiguous
{
	int ProbeLangFnOverloadConstAmbiguous(float Value) const
	{
		return 1;
	}

	int ProbeLangFnOverloadConstAmbiguous(double Value) const
	{
		return 2;
	}

	int Invoke() const
	{
		return ProbeLangFnOverloadConstAmbiguous(7);
	}
}

int EntryLangFnOverloadConstAmbiguous()
{
	FOwnerLangFnOverloadConstAmbiguous Owner;
	return Owner.Invoke();
}

struct FOwnerLangFnOverloadConstMissing
{
	int ProbeLangFnOverloadConstMissing()
	{
		return 1;
	}

	int Invoke() const
	{
		return ProbeLangFnOverloadConstMissing();
	}
}

int EntryLangFnOverloadConstMissing()
{
	FOwnerLangFnOverloadConstMissing Owner;
	return Owner.Invoke();
}

int ProbeLangFnOverloadDirectionPromotion(double& in Value)
{
	return 402;
}

int EntryLangFnOverloadDirectionPromotion()
{
	double Value = 7.0;
	return ProbeLangFnOverloadDirectionPromotion(Value);
}

int ProbeLangFnOverloadDirectionAmbiguous(float& in Value)
{
	return 1;
}

int ProbeLangFnOverloadDirectionAmbiguous(double& in Value)
{
	return 2;
}

int EntryLangFnOverloadDirectionAmbiguous()
{
	int Value = 7;
	return ProbeLangFnOverloadDirectionAmbiguous(Value);
}

int ProbeLangFnOverloadDirectionMissing(int& out Value)
{
	return 1;
}

int EntryLangFnOverloadDirectionMissing()
{
	const int Value = 7;
	return ProbeLangFnOverloadDirectionMissing(Value);
}

namespace A
{
	int ProbeLangFnOverloadNamespacePromotion(double Value)
	{
		return 502;
	}
}

namespace B
{
	int ProbeLangFnOverloadNamespacePromotion(int Value)
	{
		return 599;
	}
}

int EntryLangFnOverloadNamespacePromotion()
{
	return A::ProbeLangFnOverloadNamespacePromotion(7);
}

namespace A
{
	int ProbeLangFnOverloadNamespaceAmbiguous(float Value)
	{
		return 1;
	}

	int ProbeLangFnOverloadNamespaceAmbiguous(double Value)
	{
		return 2;
	}
}

namespace B
{
	int ProbeLangFnOverloadNamespaceAmbiguous(int Value)
	{
		return 599;
	}
}

int EntryLangFnOverloadNamespaceAmbiguous()
{
	return A::ProbeLangFnOverloadNamespaceAmbiguous(7);
}

int ProbeLangFnOverloadConversionPromotion(double Value)
{
	return 702;
}

int EntryLangFnOverloadConversionPromotion()
{
	return ProbeLangFnOverloadConversionPromotion(7);
}
