#if TEST
int SelectedValue()
{
	return 41;
}
#else
int SelectedValue()
{
	return missingReleaseSymbol;
}
#endif

int main()
{
	return SelectedValue();
}
