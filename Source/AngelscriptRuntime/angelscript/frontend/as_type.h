/* Maintained Unreal AngelScript fork — canonical typed AST type forms. */
#pragma once

#include "frontend/as_decl.h"
#include "frontend/as_type_identity.h"

#include "Containers/Array.h"

BEGIN_AS_NAMESPACE

enum class asETypeQualifiers : uint8
	{
		None = 0,
		Const = 1 << 0,
		Reference = 1 << 1,
		Handle = 1 << 2,
		Nullable = 1 << 3,
	};

	inline asETypeQualifiers operator|(asETypeQualifiers Left, asETypeQualifiers Right)
	{
		return static_cast<asETypeQualifiers>(static_cast<uint8>(Left) | static_cast<uint8>(Right));
	}

	class ANGELSCRIPTRUNTIME_API asCType : public asCASTNode
	{
	public:
		asETypeKind GetKind() const { return static_cast<asETypeKind>(GetRawKind()); }
		bool HasStableIdentity() const { return !StableIdentity.IsZero(); }
		const asSStableKey& GetStableIdentity() const { return StableIdentity; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Type; }

	protected:
		explicit asCType(asETypeKind Kind);
		asCType(asETypeKind Kind, const asSStableKey& Identity);

	private:
		asSStableKey StableIdentity;
	};

	class ANGELSCRIPTRUNTIME_API asCQualType
	{
	public:
		static bool TryCreate(asCType* Type, asETypeQualifiers Qualifiers, asCQualType& Out);
		static bool TryCreateRaw(asCType* Type, uint8 Qualifiers, asCQualType& Out);
		bool IsValid() const { return Type != nullptr; }
		asCType* GetType() const { return Type; }
		asETypeQualifiers GetQualifiers() const { return static_cast<asETypeQualifiers>(Qualifiers); }
		asETypeIdentityStatus GetStableIdentity(asCStableIdentityRegistry& Registry, asSStableKey& Out) const;
		bool IsConst() const { return (Qualifiers & static_cast<uint8>(asETypeQualifiers::Const)) != 0; }
		bool IsReference() const { return (Qualifiers & static_cast<uint8>(asETypeQualifiers::Reference)) != 0; }
		bool IsHandle() const { return (Qualifiers & static_cast<uint8>(asETypeQualifiers::Handle)) != 0; }
		bool IsNullable() const { return (Qualifiers & static_cast<uint8>(asETypeQualifiers::Nullable)) != 0; }
		bool operator==(const asCQualType& Other) const { return Type == Other.Type && Qualifiers == Other.Qualifiers; }
		bool operator!=(const asCQualType& Other) const { return !(*this == Other); }

	private:
		asCType* Type = nullptr;
		uint8 Qualifiers = 0;
	};

	class ANGELSCRIPTRUNTIME_API asCBuiltinType final : public asCType
	{
	public:
		asCBuiltinType(asEPrimitiveType Primitive, asSStableKey Identity);
		asEPrimitiveType GetPrimitive() const { return Primitive; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Type && Node->GetRawKind() == static_cast<uint16>(asETypeKind::Builtin); }
	private:
		asEPrimitiveType Primitive = asEPrimitiveType::Invalid;
	};

	class ANGELSCRIPTRUNTIME_API asCNominalType final : public asCType
	{
	public:
		asCNominalType(asSStableKey Declaration, asSStableKey Use)
			: asCType(asETypeKind::Nominal, Use), Declaration(MoveTemp(Declaration)) {}
		const asSStableKey& GetDeclarationIdentity() const { return Declaration; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Type && Node->GetRawKind() == static_cast<uint16>(asETypeKind::Nominal); }
	private:
		asSStableKey Declaration;
	};

	class ANGELSCRIPTRUNTIME_API asCGenericType final : public asCType
	{
	public:
		asCGenericType(asCType* TemplateType, TArray<asCQualType> Arguments, asSStableKey Use)
			: asCType(asETypeKind::Generic, Use), TemplateType(TemplateType), Arguments(MoveTemp(Arguments)) {}
		asCType* GetTemplateType() const { return TemplateType; }
		TConstArrayView<asCQualType> GetArguments() const { return MakeArrayView(Arguments); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Type && Node->GetRawKind() == static_cast<uint16>(asETypeKind::Generic); }
	private:
		asCType* TemplateType = nullptr;
		TArray<asCQualType> Arguments;
	};

	class ANGELSCRIPTRUNTIME_API asCFunctionType final : public asCType
	{
	public:
		asCFunctionType(asCQualType ReturnType, TArray<asCQualType> Parameters, asSStableKey Use,
			TArray<asEParameterPassingMode> InPassingModes = {})
			: asCType(asETypeKind::Function, Use), ReturnType(ReturnType), Parameters(MoveTemp(Parameters)), PassingModes(MoveTemp(InPassingModes))
		{
			if (PassingModes.IsEmpty()) PassingModes.Init(asEParameterPassingMode::Value, this->Parameters.Num());
			if (PassingModes.Num() != this->Parameters.Num()) MarkInvalid();
		}
		const asCQualType& GetReturnType() const { return ReturnType; }
		TConstArrayView<asCQualType> GetParameterTypes() const { return MakeArrayView(Parameters); }
		TConstArrayView<asEParameterPassingMode> GetParameterPassingModes() const { return MakeArrayView(PassingModes); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Type && Node->GetRawKind() == static_cast<uint16>(asETypeKind::Function); }
	private:
		asCQualType ReturnType;
		TArray<asCQualType> Parameters;
		TArray<asEParameterPassingMode> PassingModes;
	};

	class ANGELSCRIPTRUNTIME_API asCErrorType final : public asCType
	{
	public:
		asCErrorType() : asCType(asETypeKind::Error) { MarkInvalid(); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Type && Node->GetRawKind() == static_cast<uint16>(asETypeKind::Error); }
	};

END_AS_NAMESPACE
