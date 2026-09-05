#include "as_metadata_image.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "frontend/as_type_context.h"

BEGIN_AS_NAMESPACE
bool asCMetadataImage::ValidateListPattern(const asSMetadataListPattern& Pattern, asUINT GenericArity, int Depth, int& NodeBudget) const
{
	if (Depth > 64 || --NodeBudget < 0) return false;
	if (Pattern.Kind == asEMetadataListPatternKind::Value)
	{
		if (!Pattern.Children.IsEmpty()) return false;
		if (Pattern.TemplateParameterIndex != ~asUINT(0))
			return !Pattern.ValueType.IsValid() && Pattern.TemplateParameterIndex < GenericArity;
		return ValidateDataType(Pattern.ValueType, false) == asEMetadataResult::Succeeded && !Pattern.ValueType.IsReference();
	}
	if (Pattern.ValueType.IsValid() || Pattern.TemplateParameterIndex != ~asUINT(0)) return false;
	if (Pattern.Kind == asEMetadataListPatternKind::Repeat || Pattern.Kind == asEMetadataListPatternKind::RepeatSame)
	{
		return Pattern.Children.Num() == 1 &&
			(Pattern.Children[0].Kind == asEMetadataListPatternKind::Value || Pattern.Children[0].Kind == asEMetadataListPatternKind::List) &&
			ValidateListPattern(Pattern.Children[0], GenericArity, Depth + 1, NodeBudget);
	}
	if (Pattern.Kind != asEMetadataListPatternKind::List) return false;
	for (int Index = 0; Index < Pattern.Children.Num(); ++Index)
	{
		const auto& Child = Pattern.Children[Index];
		if ((Child.Kind == asEMetadataListPatternKind::Repeat || Child.Kind == asEMetadataListPatternKind::RepeatSame) && Index + 1 != Pattern.Children.Num()) return false;
		if (!ValidateListPattern(Child, GenericArity, Depth + 1, NodeBudget)) return false;
	}
	return true;
}

asEMetadataResult asCMetadataImage::SetListInitializer(asCObjectType& Type, asCScriptFunction& Factory, const asSMetadataListPattern& Pattern)
{
std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEMetadataImageState::Building) return asEMetadataResult::InvalidState;
	if (!Owns(&Type) || !CanReference(&Factory)) return asEMetadataResult::ForeignOwner;
	if (Type.GetMetadataTemplateDefinition() || ListInitializers.Contains(&Type)) return asEMetadataResult::InvalidState;
	if (!IdentityContext.IsValid() || !Factory.metadataSignatureDefined || Factory.returnType.GetTypeInfo() != &Type) return asEMetadataResult::InvalidSignature;
	asSFunctionIdentityDescriptor Identity;
	if (IdentityContext->GetIdentityRegistry().ResolveFunction(Factory.GetStableKey(), Identity) != asETypeIdentityStatus::Succeeded ||
		Identity.Owner != Type.GetStableKey() ||
		(Identity.Kind != asEFunctionIdentityKind::Factory && Identity.Kind != asEFunctionIdentityKind::Behaviour)) return asEMetadataResult::InvalidSignature;
	int Budget = 4096;
	if (Pattern.Kind != asEMetadataListPatternKind::List || !ValidateListPattern(Pattern, Type.metadataGenericArity, 0, Budget)) return asEMetadataResult::InvalidSignature;
	asSMetadataListInitializer Entry; Entry.Factory = &Factory; Entry.Pattern = Pattern;
	ListInitializers.Add(&Type, MoveTemp(Entry));
	return asEMetadataResult::Succeeded;
}

const asSMetadataListInitializer* asCMetadataImage::GetListInitializer(const asCObjectType& Type) const
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	// A returned view is stable only for a frozen image. Draft mutation retains
	// its exact owner and never exposes a pointer into a growing map.
	if (!IsFrozen() || !Owns(&Type)) return nullptr;
	return ListInitializers.Find(&Type);
}

bool asCMetadataImage::ValidateListInitializers() const
{
	for (const auto& Entry : ListInitializers)
	{
		if (!Owns(Entry.Key) || !CanReference(Entry.Value.Factory) || Entry.Value.Factory->returnType.GetTypeInfo() != Entry.Key) return false;
		int Budget = 4096;
		if (Entry.Value.Pattern.Kind != asEMetadataListPatternKind::List || !ValidateListPattern(Entry.Value.Pattern, Entry.Key->metadataGenericArity, 0, Budget)) return false;
	}
	return true;
}
END_AS_NAMESPACE
