int EntryLangFnSignatureShapeGlobalAllValueHomogeneousIntOne()
{
	int Result = ProbeLangFnSignatureShapeGlobalAllValueHomogeneousIntOne(3);
	return (Result * 1000) + (true ? 1 : 0);
}

int EntryLangFnSignatureShapeGlobalAllInHomogeneousIntOne()
{
	int A0 = 3;
	int Result = ProbeLangFnSignatureShapeGlobalAllInHomogeneousIntOne(A0);
	return (Result * 1000) + (true ? 1 : 0);
}

int EntryLangFnSignatureShapeGlobalAllOutHomogeneousIntOne()
{
	int A0 = 7;
	int Result = ProbeLangFnSignatureShapeGlobalAllOutHomogeneousIntOne(A0);
	return (Result * 1000) + (A0 == 100 ? 1 : 0);
}

int EntryLangFnSignatureShapeGlobalAlternatingInoutOutHomogeneousIntTwo()
{
	int A0 = 7;
	int A1 = 8;
	int Result = ProbeLangFnSignatureShapeGlobalAlternatingInoutOutHomogeneousIntTwo(A0, A1);
	return (Result * 1000) + (A0 == 8 && A1 == 101 ? 1 : 0);
}

int EntryLangFnSignatureShapeGlobalAllValueHomogeneousFloatOne()
{
	int Result = ProbeLangFnSignatureShapeGlobalAllValueHomogeneousFloatOne(3.5f);
	return (Result * 1000) + (true ? 1 : 0);
}

int EntryLangFnSignatureShapeGlobalAllValueAlternatingIntFloatTwo()
{
	int Result = ProbeLangFnSignatureShapeGlobalAllValueAlternatingIntFloatTwo(3, 4.5f);
	return (Result * 1000) + (true ? 1 : 0);
}

int EntryLangFnSignatureShapeNamespaceGlobalAllValueHomogeneousIntOne()
{
	int Result = NLangFnSignatureShapeNamespaceGlobalAllValueHomogeneousIntOne::ProbeLangFnSignatureShapeNamespaceGlobalAllValueHomogeneousIntOne(3);
	return (Result * 1000) + (true ? 1 : 0);
}

int EntryLangFnSignatureShapeInstanceMethodAllValueHomogeneousIntOne()
{
	FOwnerLangFnSignatureShapeInstanceMethodAllValueHomogeneousIntOne Owner;
	int Result = Owner.ProbeLangFnSignatureShapeInstanceMethodAllValueHomogeneousIntOne(3);
	return (Result * 1000) + (true ? 1 : 0);
}

int EntryLangFnSignatureShapeGlobalAllValueHomogeneousIntThree()
{
	int Result = ProbeLangFnSignatureShapeGlobalAllValueHomogeneousIntThree(3, 4, 5);
	return (Result * 1000) + (true ? 1 : 0);
}

int EntryLangFnSignatureShapeGlobalAllValueHomogeneousIntFour()
{
	int Result = ProbeLangFnSignatureShapeGlobalAllValueHomogeneousIntFour(3, 4, 5, 6);
	return (Result * 1000) + (true ? 1 : 0);
}

int EntryLangFnSignatureShapeGlobalAllValueAlternatingFloatIntTwo()
{
	int Result = ProbeLangFnSignatureShapeGlobalAllValueAlternatingFloatIntTwo(3.5f, 4);
	return (Result * 1000) + (true ? 1 : 0);
}

int EntryLangFnSignatureShapeGlobalAllInHomogeneousFloatOne()
{
	float A0 = 3.5f;
	int Result = ProbeLangFnSignatureShapeGlobalAllInHomogeneousFloatOne(A0);
	return (Result * 1000) + (true ? 1 : 0);
}
