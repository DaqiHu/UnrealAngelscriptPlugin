/* Maintained Unreal AngelScript fork — typed declaration attributes. */
#pragma once

#include "frontend/AST/as_decl.h"

BEGIN_AS_NAMESPACE

enum class asEAttrTarget : uint8
	{
		None = 0,
		TranslationUnit = 1 << 0,
		Namespace = 1 << 1,
		Record = 1 << 2,
		Function = 1 << 3,
		Variable = 1 << 4,
		EnumConstant = 1 << 5,
		Any = 0x3f,
	};

	class ANGELSCRIPTRUNTIME_API asCAttr : public asCASTNode
	{
	public:
		asEAttrKind GetKind() const { return static_cast<asEAttrKind>(GetRawKind()); }
		bool AllowsTarget(asEDeclKind Kind) const;
		asEAttrTarget GetAllowedTargets() const { return AllowedTargets; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Attr; }
	protected:
		asCAttr(asEAttrKind Kind, asCSourceRange Range, asEAttrTarget Targets)
			: asCASTNode(asEASTNodeFamily::Attr, static_cast<uint16>(Kind), MoveTemp(Range)), AllowedTargets(Targets) {}
	private:
		asEAttrTarget AllowedTargets = asEAttrTarget::None;
	};

	class ANGELSCRIPTRUNTIME_API asCFlagAttr final : public asCAttr
	{
	public:
		asCFlagAttr(FString Name, asCSourceRange Range, asEAttrTarget Targets)
			: asCAttr(asEAttrKind::Flag, MoveTemp(Range), Targets), Name(MoveTemp(Name)) {}
		const FString& GetName() const { return Name; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Attr && Node->GetRawKind() == static_cast<uint16>(asEAttrKind::Flag); }
	private:
		FString Name;
	};

	class ANGELSCRIPTRUNTIME_API asCStringAttr final : public asCAttr
	{
	public:
		asCStringAttr(FString Name, FString Value, asCSourceRange Range, asEAttrTarget Targets)
			: asCAttr(asEAttrKind::String, MoveTemp(Range), Targets), Name(MoveTemp(Name)), Value(MoveTemp(Value)) {}
		const FString& GetName() const { return Name; }
		const FString& GetValue() const { return Value; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Attr && Node->GetRawKind() == static_cast<uint16>(asEAttrKind::String); }
	private:
		FString Name;
		FString Value;
	};

	enum class asEAnnotationKind : uint8 { UClass, UStruct, UEnum, UFunction, UProperty, UMeta };
	enum class asEAnnotationValueKind : uint8 { Flag, Scalar, String, Group };

	struct asSAnnotationArgument
	{
		FString Name;
		asEAnnotationValueKind Kind = asEAnnotationValueKind::Flag;
		FString Value;
		asCSourceRange Range;
		asCSourceRange ValueRange;
		TArray<asSAnnotationArgument> Arguments;
	};

	class ANGELSCRIPTRUNTIME_API asCAnnotationAttr final : public asCAttr
	{
	public:
		asCAnnotationAttr(asEAnnotationKind AnnotationKind, TArray<asSAnnotationArgument> Arguments,
			asCSourceRange Range, asEAttrTarget Targets)
			: asCAttr(asEAttrKind::Annotation, MoveTemp(Range), Targets), AnnotationKind(AnnotationKind), Arguments(MoveTemp(Arguments)) {}
		asEAnnotationKind GetAnnotationKind() const { return AnnotationKind; }
		const TCHAR* GetName() const;
		TConstArrayView<asSAnnotationArgument> GetArguments() const { return MakeArrayView(Arguments); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Attr && Node->GetRawKind() == static_cast<uint16>(asEAttrKind::Annotation); }

	private:
		asEAnnotationKind AnnotationKind;
		TArray<asSAnnotationArgument> Arguments;
	};

END_AS_NAMESPACE
