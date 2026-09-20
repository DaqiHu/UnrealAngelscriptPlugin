int EntryLangExprPrecedenceUnparenthesizedMultiplicativeAdditive()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;
	return ObservePrecedence(A * B + C);
}

int EntryLangExprPrecedenceLeftParenthesizedMultiplicativeAdditive()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;
	return ObservePrecedence((A * B) + C);
}

int EntryLangExprPrecedenceRightParenthesizedMultiplicativeAdditive()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;
	return ObservePrecedence(A * (B + C));
}

int EntryLangExprPrecedenceUnparenthesizedConditionalConditional()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;
	return ObservePrecedence(A ? B : C ? D : E);
}

int EntryLangExprPrecedenceUnparenthesizedAdditiveConditional()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;
	return ObservePrecedence(A + B ? C : D);
}

int EntryLangExprPrecedenceUnparenthesizedLogicalAndAdditive()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;
	return ObservePrecedence(A && B + C);
}
