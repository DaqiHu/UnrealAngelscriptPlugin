class FBroken
{
	void Run()
	{
	}

int EntryLangDeclFailureRecoveryUnbalancedClassEntryBodyLf()
{
	return 23;
}

int Before()
{
	return 7;
}

void Broken(int A,, int B)
{
}

int EntryLangDeclFailureRecoveryBadParameterListAfterValidFunctionLf()
{
	return 23;
}

namespace FailureNamespace
{
class FBroken : MissingBase
{
}
}

int EntryLangDeclFailureRecoveryUnknownBaseInsideNamespaceLf()
{
	return 23;
}
