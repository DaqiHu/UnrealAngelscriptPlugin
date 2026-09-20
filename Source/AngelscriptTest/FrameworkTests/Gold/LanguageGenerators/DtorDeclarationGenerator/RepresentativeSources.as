int EntryLangDtorDeclarationImplicitScriptValueCompile()
{
	FImplicitValue Object;
	return Object.Value;
}

int EntryLangDtorDeclarationDeclaredScriptValueCompile()
{
	FDeclaredValue Object;
	return Object.Value;
}

int EntryLangDtorDeclarationImplicitScriptReferenceCompile()
{
	FImplicitReference Object = FImplicitReference();
	return Object.Value;
}

int EntryLangDtorDeclarationDeclaredScriptReferenceCompile()
{
	FDeclaredReference Object = FDeclaredReference();
	return Object.Value;
}

int EntryLangDtorDeclarationNativeValueCompile()
{
	FNativeCaseValue Object(45);
	return Object.Value;
}

int EntryLangDtorDeclarationNativeReferenceCompile()
{
	FNativeCaseReference Object = CreateNativeCaseReference(46);
	return Object.Value;
}

int EntryLangDtorDeclarationEmptyDestructorCompile()
{
	FEmptyDestructor Object;
	return Object.Value;
}

int EntryLangDtorDeclarationFieldDestructorCompile()
{
	FFieldOwner Object;
	return Object.Value;
}

int EntryLangDtorDeclarationBaseDerivedDestructorCompile()
{
	FDestructorDerived Object = FDestructorDerived();
	return Object.Value;
}

int EntryLangDtorDeclarationPrivateDestructorCompile()
{
	FPrivateDestructor Object = FPrivateDestructor();
	return Object.Value;
}

int EntryLangDtorDeclarationThrowingDestructorCompile()
{
	FThrowingDestructor Object = FThrowingDestructor();
	int Zero = 0;
	int Fault = 1 / Zero;
	return Object.Value + Fault;
}

int EntryLangDtorDeclarationThrowingDerivedMembersBaseCompile()
{
	FThrowingDerived Object = FThrowingDerived();
	int Zero = 0;
	int Fault = 1 / Zero;
	return Object.Value + Fault;
}

int EntryLangDtorDeclarationMalformedDestructorCompile()
{
	FMalformedDestructor Object = FMalformedDestructor();
	return 0;
}
