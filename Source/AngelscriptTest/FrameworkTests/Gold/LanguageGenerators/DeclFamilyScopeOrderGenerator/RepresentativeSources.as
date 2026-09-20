int FamilyPublishedFunctionFunctionGlobalBeforeUse()
{
	return 100;
}

int FamilyWitnessFunctionGlobalBeforeUse()
{
	return FamilyPublishedFunctionFunctionGlobalBeforeUse();
}

int ScopeWitnessFunctionGlobalBeforeUse()
{
	return 200;
}

int OrderingTargetFunctionGlobalBeforeUse()
{
	return 300;
}

int OrderingWitnessFunctionGlobalBeforeUse()
{
	return OrderingTargetFunctionGlobalBeforeUse();
}

int EntryLangDeclFamilyScopeOrderFunctionGlobalBeforeUse()
{
	return FamilyWitnessFunctionGlobalBeforeUse() + ScopeWitnessFunctionGlobalBeforeUse() + OrderingWitnessFunctionGlobalBeforeUse();
}

typedef int FamilyPublishedAliasTypedefMemberSameSection;

int FamilyWitnessTypedefMemberSameSection()
{
	return FamilyPublishedAliasTypedefMemberSameSection(109);
}

class FScopeMemberTypedefMemberSameSection
{
	int Read()
	{
		return 203;
	}
}

int ScopeWitnessTypedefMemberSameSection()
{
	FScopeMemberTypedefMemberSameSection Value = FScopeMemberTypedefMemberSameSection();
	return Value.Read();
}

int OrderingTargetTypedefMemberSameSection()
{
	return 302;
}

int OrderingWitnessTypedefMemberSameSection()
{
	return OrderingTargetTypedefMemberSameSection();
}

int EntryLangDeclFamilyScopeOrderTypedefMemberSameSection()
{
	return FamilyWitnessTypedefMemberSameSection() + ScopeWitnessTypedefMemberSameSection() + OrderingWitnessTypedefMemberSameSection();
}

class FFamilyDestructorDestructorMultipleSectionsReversedSections
{
	int Published = 106;

	~FFamilyDestructorDestructorMultipleSectionsReversedSections()
	{
	}
}

int FamilyWitnessDestructorMultipleSectionsReversedSections()
{
	FFamilyDestructorDestructorMultipleSectionsReversedSections Value;
	return 106;
}

namespace ScopeSectionProviderDestructorMultipleSectionsReversedSections
{
	int Read()
	{
		return 204;
	}
}

int ScopeWitnessDestructorMultipleSectionsReversedSections()
{
	return ScopeSectionProviderDestructorMultipleSectionsReversedSections::Read();
}

int OrderingWitnessDestructorMultipleSectionsReversedSections()
{
	return OrderingTargetDestructorMultipleSectionsReversedSections();
}

int OrderingTargetDestructorMultipleSectionsReversedSections()
{
	return 304;
}

int EntryLangDeclFamilyScopeOrderDestructorMultipleSectionsReversedSections()
{
	return FamilyWitnessDestructorMultipleSectionsReversedSections() + ScopeWitnessDestructorMultipleSectionsReversedSections() + OrderingWitnessDestructorMultipleSectionsReversedSections();
}
