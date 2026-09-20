namespace CollisionLeftThenRightSameFunctionOverload
{
	int Clash()
	{
		return 11;
	}

	int Clash(int Value)
	{
		return Value + 12;
	}

}

int EntryLangDeclCollisionLeftThenRightSameFunctionOverload()
{
	return 71;
}

namespace CollisionLeftThenRightSameMethodOverload
{
	class FOwnerLeftThenRightSameMethodOverload
	{
		int Clash()
		{
			return 21;
		}

		int Clash(int Value)
		{
			return Value + 22;
		}

	}
}

int EntryLangDeclCollisionLeftThenRightSameMethodOverload()
{
	return 71;
}

namespace CollisionLeftLeftThenRightDifferentFunctionDuplicate
{
	int Clash()
	{
		return 11;
	}

}

namespace CollisionRightLeftThenRightDifferentFunctionDuplicate
{
	int Clash()
	{
		return 11;
	}

}

int EntryLangDeclCollisionLeftThenRightDifferentFunctionDuplicate()
{
	return 71;
}

namespace CollisionLeftThenRightSameFunctionDuplicate
{
	int Clash()
	{
		return 11;
	}

	int Clash()
	{
		return 11;
	}

}

int EntryLangDeclCollisionLeftThenRightSameFunctionDuplicate()
{
	return 71;
}

namespace CollisionLeftThenRightSamePropertyGetSet
{
	class FOwnerLeftThenRightSamePropertyGetSet
	{
		int Clash
		{
			get
			{
				return 24;
			}
		}

		int Clash
		{
			set
			{
			}
		}

	}
}

int EntryLangDeclCollisionLeftThenRightSamePropertyGetSet()
{
	return 71;
}
