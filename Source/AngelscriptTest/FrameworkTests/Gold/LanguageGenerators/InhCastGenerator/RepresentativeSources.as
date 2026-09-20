class FCastRoot
{
	int Kind = 1;
	int Identity = 1;

	int GetKind() const
	{
		return Kind;
	}

	int GetIdentity() const
	{
		return Identity;
	}
}

class FCastDerived : FCastRoot
{
	FCastDerived()
	{
		Kind = 2;
		Identity = 1;
	}
}

class FCastSibling : FCastRoot
{
	FCastSibling()
	{
		Kind = 3;
		Identity = 1;
	}
}

FCastRoot@ MakeCastRoot()
{
	return FCastRoot();
}

FCastDerived@ MakeCastDerived()
{
	return FCastDerived();
}

FCastRoot@ MakeCastDerivedAsRoot()
{
	FCastDerived@ Object = FCastDerived();
	return Object;
}

FCastRoot@ MakeCastSiblingAsRoot()
{
	FCastSibling@ Object = FCastSibling();
	return Object;
}


int ObserveCastMutableExactAssign(FCastDerived@ Value)
{
	return Value is null ? -1 : Value.GetKind();
}

FCastDerived@ ReturnCastMutableExactAssign(FCastDerived@ Value)
{
	return cast<FCastDerived>(Value);
}

int EntryLangInhCastMutableExactAssign()
{
	FCastDerived@ Source = MakeCastDerived();
	FCastDerived@ Result = cast<FCastDerived>(Source);
	return Result is null ? -1 : Result.GetKind();
}

int ObserveCastMutableExactArgument(FCastDerived@ Value)
{
	return Value is null ? -1 : Value.GetKind();
}

FCastDerived@ ReturnCastMutableExactArgument(FCastDerived@ Value)
{
	return cast<FCastDerived>(Value);
}

int EntryLangInhCastMutableExactArgument()
{
	FCastDerived@ Source = MakeCastDerived();
	return ObserveCastMutableExactArgument(cast<FCastDerived>(Source));
}

int ObserveCastMutableExactReturn(FCastDerived@ Value)
{
	return Value is null ? -1 : Value.GetKind();
}

FCastDerived@ ReturnCastMutableExactReturn(FCastDerived@ Value)
{
	return cast<FCastDerived>(Value);
}

int EntryLangInhCastMutableExactReturn()
{
	FCastDerived@ Source = MakeCastDerived();
	FCastDerived@ Result = ReturnCastMutableExactReturn(Source);
	return Result is null ? -1 : Result.GetKind();
}

int ObserveCastMutableExactIdentityCompare(FCastDerived@ Value)
{
	return Value is null ? -1 : Value.GetKind();
}

FCastDerived@ ReturnCastMutableExactIdentityCompare(FCastDerived@ Value)
{
	return cast<FCastDerived>(Value);
}

int EntryLangInhCastMutableExactIdentityCompare()
{
	FCastDerived@ Source = MakeCastDerived();
	FCastDerived@ Result = cast<FCastDerived>(Source);
	if (Result is null)
	{
		return Source is null ? 1 : 0;
	}
	return Result.GetIdentity() == Source.GetIdentity() ? 1 : 0;
}

int ObserveCastMutableExactMemberCall(FCastDerived@ Value)
{
	return Value is null ? -1 : Value.GetKind();
}

FCastDerived@ ReturnCastMutableExactMemberCall(FCastDerived@ Value)
{
	return cast<FCastDerived>(Value);
}

int EntryLangInhCastMutableExactMemberCall()
{
	FCastDerived@ Source = MakeCastDerived();
	FCastDerived@ Result = cast<FCastDerived>(Source);
	return Result is null ? -1 : Result.GetKind();
}

int ObserveCastMutableUpcastAssign(FCastRoot@ Value)
{
	return Value is null ? -1 : Value.GetKind();
}

FCastRoot@ ReturnCastMutableUpcastAssign(FCastDerived@ Value)
{
	return cast<FCastRoot>(Value);
}

int EntryLangInhCastMutableUpcastAssign()
{
	FCastDerived@ Source = MakeCastDerived();
	FCastRoot@ Result = cast<FCastRoot>(Source);
	return Result is null ? -1 : Result.GetKind();
}

int ObserveCastMutableDowncastFailureAssign(FCastDerived@ Value)
{
	return Value is null ? -1 : Value.GetKind();
}

FCastDerived@ ReturnCastMutableDowncastFailureAssign(FCastRoot@ Value)
{
	return cast<FCastDerived>(Value);
}

int EntryLangInhCastMutableDowncastFailureAssign()
{
	FCastRoot@ Source = MakeCastRoot();
	FCastDerived@ Result = cast<FCastDerived>(Source);
	return Result is null ? -1 : Result.GetKind();
}

int ObserveCastMutableSiblingIdentityCompare(FCastDerived@ Value)
{
	return Value is null ? -1 : Value.GetKind();
}

FCastDerived@ ReturnCastMutableSiblingIdentityCompare(FCastRoot@ Value)
{
	return cast<FCastDerived>(Value);
}

int EntryLangInhCastMutableSiblingIdentityCompare()
{
	FCastRoot@ Source = MakeCastSiblingAsRoot();
	FCastDerived@ Result = cast<FCastDerived>(Source);
	if (Result is null)
	{
		return Source is null ? 1 : 0;
	}
	return Result.GetIdentity() == Source.GetIdentity() ? 1 : 0;
}

int ObserveCastMutableNullIdentityCompare(FCastDerived@ Value)
{
	return Value is null ? -1 : Value.GetKind();
}

FCastDerived@ ReturnCastMutableNullIdentityCompare(FCastRoot@ Value)
{
	return cast<FCastDerived>(Value);
}

int EntryLangInhCastMutableNullIdentityCompare()
{
	FCastRoot@ Source = nullptr;
	FCastDerived@ Result = cast<FCastDerived>(Source);
	if (Result is null)
	{
		return Source is null ? 1 : 0;
	}
	return Result.GetIdentity() == Source.GetIdentity() ? 1 : 0;
}

int ObserveCastConstExactAssign(const FCastDerived@ Value)
{
	return Value is null ? -1 : Value.GetKind();
}

const FCastDerived@ ReturnCastConstExactAssign(const FCastDerived@ Value)
{
	return cast<FCastDerived>(Value);
}

int EntryLangInhCastConstExactAssign()
{
	const FCastDerived@ Source = MakeCastDerived();
	const FCastDerived@ Result = cast<FCastDerived>(Source);
	return Result is null ? -1 : Result.GetKind();
}
