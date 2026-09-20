int EntryLangExprAssociativityUnparenthesizedMultiplicativeRepeatedOperator()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;

	return ObservePrecedence(A * B * C);
}

int EntryLangExprAssociativityUnparenthesizedMultiplicativeMixedSameLevel()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;

	return ObservePrecedence(A * B / C);
}

int EntryLangExprAssociativityLeftParenthesizedMultiplicativeRepeatedOperator()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;

	return ObservePrecedence((A * B) * C);
}

int EntryLangExprAssociativityRightParenthesizedMultiplicativeRepeatedOperator()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;

	return ObservePrecedence(A * (B * C));
}

int EntryLangExprAssociativityUnparenthesizedBitwiseAndMixedSameLevel()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;

	return ObservePrecedence(A & Q & C);
}

int EntryLangExprAssociativityUnparenthesizedLogicalAndMixedSameLevel()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;

	return ObservePrecedence(A && Q && C);
}

int EntryLangExprAssociativityUnparenthesizedConditionalRepeatedOperator()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;

	return ObservePrecedence(A ? B : C ? D : E);
}

int EntryLangExprAssociativityUnparenthesizedConditionalMixedSameLevel()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;

	return ObservePrecedence(A ? B ? C : D : E);
}

int EntryLangExprAssociativityUnparenthesizedLogicalAndRepeatedOperator()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;

	return ObservePrecedence(A && B && C);
}

int EntryLangExprAssociativityUnparenthesizedAssignmentRepeatedOperator()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;

	return ObservePrecedence(A = B = C);
}

int EntryLangExprAssociativityUnparenthesizedAssignmentMixedSameLevel()
{
	FPrecedenceProbe A(2, true);
	FPrecedenceProbe B(3, false);
	FPrecedenceProbe C(5, true);
	FPrecedenceProbe D(7, false);
	FPrecedenceProbe E(11, true);
	bool Q = false;

	return ObservePrecedence(A += B = C);
}
