/* Maintained Unreal AngelScript fork — immutable pointer-free AST projection. */
#pragma once

#include "frontend/AST/as_ast_visitor.h"

#include "Misc/TVariant.h"

BEGIN_AS_NAMESPACE

struct asSASTProjectedTypeSyntax
	{
		bool bPresent = false;
		FString Spelling;
		TArray<FString> NameParts;
		asSStableSourceAnchor Source;
		TArray<asSStableSourceAnchor> ComponentRanges;
		TArray<asSASTProjectedTypeSyntax> GenericArguments;
		TArray<asSStableSourceAnchor> ArrayDimensions;
		bool bGlobal = false;
		bool bConst = false;
		bool bHandle = false;
		asETypeReferenceDirection Reference = asETypeReferenceDirection::None;

		bool Equals(const asSASTProjectedTypeSyntax& Other) const;
	};

	struct asSASTProjectedAnnotationArgument
	{
		static constexpr uint32 NoParent = MAX_uint32;

		uint32 ParentIndex = NoParent;
		FString Name;
		asEAnnotationValueKind Kind = asEAnnotationValueKind::Flag;
		FString Value;
		asSStableSourceAnchor Source;
		asSStableSourceAnchor ValueSource;

		bool Equals(const asSASTProjectedAnnotationArgument& Other) const;
	};

	struct asSASTEmptyPayload {};
	struct asSASTNamedDeclPayload { FString Name; };
	struct asSASTRecordDeclPayload
	{
		FString Name;
		asETypeDeclKind DeclaredTypeKind = asETypeDeclKind::Invalid;
		TArray<FString> UnresolvedBases;
		asERecordModifiers Modifiers = asERecordModifiers::None;
	};
	struct asSASTVarDeclPayload
	{
		FString Name;
		asSASTProjectedTypeSyntax TypeSyntax;
		bool bHasDeferredInitializer = false;
		asSStableSourceAnchor DeferredInitializerSource;
	};
	struct asSASTEnumConstantDeclPayload
	{
		FString Name;
		int64 Value = 0;
		bool bExplicitValue = false;
		bool bResolvedValue = false;
		bool bHasDeferredInitializer = false;
		asSStableSourceAnchor DeferredInitializerSource;
	};
	struct asSASTFunctionDeclPayload
	{
		FString Name;
		asSASTProjectedTypeSyntax ReturnTypeSyntax;
		bool bConstMethod = false;
		asEFunctionKind FunctionKind = asEFunctionKind::Ordinary;
		asEFunctionModifiers Modifiers = asEFunctionModifiers::None;
		bool bHasDeferredBody = false;
		asSStableSourceAnchor DeferredBodySource;
		TArray<asSStableSourceAnchor> DeferredStatementSources;
	};
	struct asSASTCallableTypeDeclPayload
	{
		FString Name;
		asSASTProjectedTypeSyntax ReturnTypeSyntax;
		bool bMulticast = false;
	};
	struct asSASTTypeAliasDeclPayload
	{
		FString Name;
		asSASTProjectedTypeSyntax TypeSyntax;
	};
	struct asSASTProjectedAccessPermissionModifier
	{
		asEAccessPermissionModifier Kind = asEAccessPermissionModifier::ReadOnly;
		asSStableSourceAnchor Source;
		bool Equals(const asSASTProjectedAccessPermissionModifier& Other) const;
	};
	struct asSASTProjectedAccessPermission
	{
		asEAccessPermissionSubject Subject = asEAccessPermissionSubject::Named;
		FString Name;
		asSStableSourceAnchor SubjectSource;
		asSStableSourceAnchor Source;
		TArray<asSASTProjectedAccessPermissionModifier> Modifiers;
		bool Equals(const asSASTProjectedAccessPermission& Other) const;
	};
	struct asSASTAccessSpecifierDeclPayload
	{
		FString Name;
		asEAccessSpecifier BaseAccess = asEAccessSpecifier::Private;
		asSStableSourceAnchor KeywordSource;
		asSStableSourceAnchor NameSource;
		asSStableSourceAnchor EqualsSource;
		asSStableSourceAnchor BaseAccessSource;
		asSStableSourceAnchor SemicolonSource;
		TArray<asSASTProjectedAccessPermission> Permissions;
	};
	struct asSASTIntegerLiteralPayload { int64 Value = 0; };
	struct asSASTBooleanLiteralPayload { bool Value = false; };
	struct asSASTFloatingLiteralPayload { uint64 IEEE754Bits = 0; };
	struct asSASTImplicitCastPayload { asEImplicitConversionKind Conversion = asEImplicitConversionKind::None; };
	struct asSASTUserConversionPayload { asEUserConversionMode Mode = asEUserConversionMode::Implicit; };
	struct asSASTUnaryOperatorPayload
	{
		asEUnaryOperator Operator = asEUnaryOperator::Plus;
		asSStableSourceAnchor OperatorSource;
	};
	struct asSASTBinaryOperatorPayload
	{
		asEBinaryOperator Operator = asEBinaryOperator::Add;
		asSStableSourceAnchor OperatorSource;
	};
	struct asSASTCallExprPayload { TArray<int32> ArgumentSourceOrdinals; };
	struct asSASTMemberExprPayload { asSStableSourceAnchor MemberSource; };
	struct asSASTExplicitCastPayload { asSASTProjectedTypeSyntax TypeSyntax; };
	struct asSASTConstructExprPayload { asSASTProjectedTypeSyntax TypeSyntax; TArray<int32> ArgumentSourceOrdinals; };
	struct asSASTStringLiteralPayload { FString Value; };
	struct asSASTThisExprPayload { bool bImplicit = false; };
	struct asSASTInitListExprPayload
	{
		asSStableKey InitializerIdentity;
		TArray<int32> ElementPatternIndices;
	};
	struct asSASTValueInitExprPayload {};
	struct asSASTDeclGroupStmtPayload { uint32 DeclarationCount = 0; };
	struct asSASTClassDefaultStmtPayload { asSStableSourceAnchor KeywordSource; };
	struct asSASTForStmtPayload
	{
		uint32 InitializerCount = 0;
		bool bHasCondition = false;
		uint32 IncrementCount = 0;
		asSStableSourceAnchor HeaderSource;
	};
	struct asSASTSwitchStmtPayload
	{
		uint32 ClauseCount = 0;
		uint32 DefaultOrdinal = MAX_uint32;
		asSStableSourceAnchor HeaderSource;
	};
	struct asSASTCaseStmtPayload
	{
		uint32 StatementCount = 0;
		uint64 ResolvedValueBits = 0;
		asSStableSourceAnchor KeywordSource;
		asSStableSourceAnchor ColonSource;
	};
	struct asSASTDefaultStmtPayload
	{
		uint32 StatementCount = 0;
		asSStableSourceAnchor KeywordSource;
		asSStableSourceAnchor ColonSource;
	};
	struct asSASTFallthroughStmtPayload {};
	struct asSASTForeachStmtPayload
	{
		bool bHasKey = false;
		asSStableSourceAnchor HeaderSource;
		asSStableSourceAnchor ColonSource;
	};
	struct asSASTBuiltinTypePayload { asEPrimitiveType Primitive = asEPrimitiveType::Invalid; };
	struct asSASTNominalTypePayload { asSStableKey DeclarationIdentity; };
	struct asSASTFunctionTypePayload { TArray<asEParameterPassingMode> PassingModes; };
	struct asSASTFlagAttrPayload
	{
		FString Name;
		asEAttrTarget AllowedTargets = asEAttrTarget::None;
	};
	struct asSASTStringAttrPayload
	{
		FString Name;
		FString Value;
		asEAttrTarget AllowedTargets = asEAttrTarget::None;
	};
	struct asSASTAnnotationAttrPayload
	{
		asEAnnotationKind AnnotationKind = asEAnnotationKind::UClass;
		asEAttrTarget AllowedTargets = asEAttrTarget::None;
		TArray<asSASTProjectedAnnotationArgument> Arguments;
	};

	using asFASTProjectionPayload = TVariant<
		asSASTEmptyPayload,
		asSASTNamedDeclPayload,
		asSASTRecordDeclPayload,
		asSASTVarDeclPayload,
		asSASTEnumConstantDeclPayload,
		asSASTFunctionDeclPayload,
		asSASTCallableTypeDeclPayload,
		asSASTAccessSpecifierDeclPayload,
		asSASTIntegerLiteralPayload,
		asSASTBooleanLiteralPayload,
		asSASTFloatingLiteralPayload,
		asSASTImplicitCastPayload,
		asSASTUserConversionPayload,
		asSASTUnaryOperatorPayload,
		asSASTBinaryOperatorPayload,
		asSASTBuiltinTypePayload,
		asSASTNominalTypePayload,
		asSASTFunctionTypePayload,
		asSASTFlagAttrPayload,
		asSASTStringAttrPayload,
		asSASTAnnotationAttrPayload,
		asSASTTypeAliasDeclPayload,
		asSASTCallExprPayload,
		asSASTMemberExprPayload,
		asSASTExplicitCastPayload,
		asSASTConstructExprPayload,
		asSASTStringLiteralPayload,
		asSASTThisExprPayload,
		asSASTInitListExprPayload,
		asSASTValueInitExprPayload,
		asSASTDeclGroupStmtPayload,
		asSASTClassDefaultStmtPayload,
		asSASTForStmtPayload,
		asSASTSwitchStmtPayload,
		asSASTCaseStmtPayload,
		asSASTDefaultStmtPayload,
		asSASTFallthroughStmtPayload,
		asSASTForeachStmtPayload>;

	struct asSASTProjectedAuthoredAccessUse
	{
		bool bPresent = false;
		FString Name;
		asSStableSourceAnchor KeywordSource;
		asSStableSourceAnchor ColonSource;
		asSStableSourceAnchor NameSource;

		bool Equals(const asSASTProjectedAuthoredAccessUse& Other) const;
	};

	struct asSASTProjectedDeclarationSemantics
	{
		bool bPresent = false;
		asEAccessSpecifier Access = asEAccessSpecifier::Public;
		asSASTProjectedAuthoredAccessUse AuthoredAccess;

		bool Equals(const asSASTProjectedDeclarationSemantics& Other) const;
	};

	struct asSASTProjectedExpressionSemantics
	{
		bool bPresent = false;
		asSStableKey TypeIdentity;
		asEValueCategory ValueCategory = asEValueCategory::Invalid;
		bool bReadOnly = false;

		bool Equals(const asSASTProjectedExpressionSemantics& Other) const;
	};

	struct asSASTProjectedSemanticReference
	{
		asEASTSemanticTypeReferenceKind Kind = asEASTSemanticTypeReferenceKind::VariableType;
		uint32 Ordinal = 0;
		asSStableKey TypeIdentity;
		asEParameterPassingMode PassingMode = asEParameterPassingMode::Value;

		bool Equals(const asSASTProjectedSemanticReference& Other) const;
	};

	struct asSASTProjectedCrossReference
	{
		asEASTCrossReferenceKind Kind = asEASTCrossReferenceKind::Declaration;
		uint32 LocalIndex = 0; // projection index + 1; zero means external
		asSStableKey StableIdentity;

		bool Equals(const asSASTProjectedCrossReference& Other) const;
	};

	struct asSASTProjectionRecord
	{
		asEASTNodeFamily Family = asEASTNodeFamily::Decl;
		uint16 Kind = 0;
		asSStableSourceAnchor Source;
		TArray<uint32> Children;
		TArray<asSASTProjectedCrossReference> CrossReferences;
		TArray<asSASTProjectedSemanticReference> SemanticReferences;
		asSASTProjectedDeclarationSemantics Declaration;
		asSASTProjectedExpressionSemantics Expression;
		asFASTProjectionPayload Payload;
		asSStableKey StableIdentity;

		template <typename T>
		const T* TryGetPayload() const { return Payload.template TryGet<T>(); }

		bool Equals(const asSASTProjectionRecord& Other) const;
	};

	class ANGELSCRIPTRUNTIME_API asCASTProjection
	{
	public:
		static bool Build(
			const asCASTContext& Context,
			const asCASTNode* Root,
			asCASTProjection& Out,
			FString& OutError);
		TConstArrayView<asSASTProjectionRecord> GetRecords() const { return MakeArrayView(Records); }
		bool Equals(const asCASTProjection& Other) const;
		// Decode proves wire structure only. A consumer must retain an explicit
		// registry and validate its identity references before semantic admission.
		bool ValidateIdentities(const asCStableIdentityRegistry& Registry, FString& OutError) const;
		FString DumpText() const;
		FString DumpJson() const;

	private:
		friend class asCASTCodec;
		TArray<asSASTProjectionRecord> Records;
	};

END_AS_NAMESPACE
