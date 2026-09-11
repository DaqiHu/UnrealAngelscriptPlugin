#include "frontend/as_attr.h"

BEGIN_AS_NAMESPACE

bool asCAttr::AllowsTarget(asEDeclKind Kind) const
	{
		uint8 Required = 0;
		switch (Kind)
		{
		case asEDeclKind::TranslationUnit: Required = static_cast<uint8>(asEAttrTarget::TranslationUnit); break;
		case asEDeclKind::Namespace: Required = static_cast<uint8>(asEAttrTarget::Namespace); break;
		case asEDeclKind::Record: Required = static_cast<uint8>(asEAttrTarget::Record); break;
		case asEDeclKind::Function: Required = static_cast<uint8>(asEAttrTarget::Function); break;
		case asEDeclKind::EnumConstant: Required = static_cast<uint8>(asEAttrTarget::EnumConstant); break;
		case asEDeclKind::Var:
		case asEDeclKind::ParmVar: Required = static_cast<uint8>(asEAttrTarget::Variable); break;
		default: return false;
		}
		return (static_cast<uint8>(AllowedTargets) & Required) != 0;
	}

	const TCHAR* asCAnnotationAttr::GetName() const
	{
		switch (AnnotationKind)
		{
		case asEAnnotationKind::UClass: return TEXT("UCLASS");
		case asEAnnotationKind::UStruct: return TEXT("USTRUCT");
		case asEAnnotationKind::UEnum: return TEXT("UENUM");
		case asEAnnotationKind::UFunction: return TEXT("UFUNCTION");
		case asEAnnotationKind::UProperty: return TEXT("UPROPERTY");
		case asEAnnotationKind::UMeta: return TEXT("UMETA");
		default: return TEXT("");
		}
	}

END_AS_NAMESPACE
