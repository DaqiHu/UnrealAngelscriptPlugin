struct Counter
{
	int Value = 0;
}

mixin void AddToCounter(Counter& Self, int Delta)
{
	Self.Value += Delta;
}

int EntryLangFnMixinFreeCallRejectionGlobal()
{
	Counter Value;
	AddToCounter(Value, 3);
	return Value.Value == 3 ? 1 : 0;
}

namespace Tools
{
struct Counter
{
	int Value = 0;
}

mixin void AddToCounter(Counter& Self, int Delta)
{
	Self.Value += Delta;
}

int EntryLangFnMixinFreeCallRejectionNested()
{
	Counter Value;
	AddToCounter(Value, 3);
	return Value.Value == 3 ? 1 : 0;
}
}
