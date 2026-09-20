/* Maintained Unreal AngelScript fork — concrete typed declaration hierarchy. */
#pragma once

#include "frontend/AST/as_ast_fwd.h"
#include "frontend/Basic/as_source_location.h"
#include "frontend/Sema/as_type_identity.h"
#include "frontend/Parser/as_type_syntax.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Templates/UniquePtr.h"

BEGIN_AS_NAMESPACE

struct asSAccessPermissionModifier
	{
		asEAccessPermissionModifier Kind = asEAccessPermissionModifier::ReadOnly;
		asCSourceRange Range;
	};

	// One authored subject in an access policy. Modifiers remain ordered source
	// facts; semantic publication deliberately ignores Inherited on Wildcard,
	// matching the maintained language without erasing the authored token.
	struct asSAuthoredAccessPermission
	{
		asEAccessPermissionSubject Subject = asEAccessPermissionSubject::Named;
		FString Name;
		asCSourceRange SubjectRange;
		asCSourceRange Range;
		TArray<asSAccessPermissionModifier> Modifiers;
	};

	struct asSAccessSpecifierSourceInfo
	{
		asCSourceRange KeywordRange;
		asCSourceRange NameRange;
		asCSourceRange EqualsRange;
		asCSourceRange BaseAccessRange;
		asCSourceRange SemicolonRange;
	};

	// `access:Policy` is source syntax plus a non-owning, record-local semantic
	// edge. It is intentionally not another durable identity family.
	struct asSAuthoredAccessUse
	{
		FString Name;
		asCSourceRange KeywordRange;
		asCSourceRange ColonRange;
		asCSourceRange NameRange;
		asCAccessSpecifierDecl* Resolved = nullptr;
	};

	class ANGELSCRIPTRUNTIME_API asCASTNode
	{
	public:
		asEASTNodeFamily GetFamily() const { return Family; }
		uint16 GetRawKind() const { return RawKind; }
		const asCSourceRange& GetSourceRange() const { return SourceRange; }
		bool SetSourceRange(asCSourceRange Range)
		{
			if (!CanMutate() || !Range.IsOrdered() || (SourceRange.IsOrdered() &&
				(Range.Begin.SnapshotID != SourceRange.Begin.SnapshotID || Range.Begin.FileID != SourceRange.Begin.FileID))) return false;
			SourceRange = Range; return true;
		}
#if WITH_ANGELSCRIPT_TESTS
		void DebugForceSourceRange(asCSourceRange Range)
		{
			SourceRange = Range;
		}
#endif
		bool IsInvalid() const { return (NodeFlags & InvalidFlag) != 0; }
		bool IsSealed() const { return (NodeFlags & SealedFlag) != 0; }
		bool CanMutate() const { return !IsSealed(); }
		asCASTContext* GetOwnerContext() const { return OwnerContext; }
		uint32 GetContextLocalID() const { return ContextLocalID; }
		bool MarkInvalid() { if (!CanMutate()) return false; NodeFlags |= InvalidFlag; return true; }

	protected:
		uint8 GetAccessBits() const { return (NodeFlags >> 2) & 0x3; }
		bool SetAccessBits(uint8 Value) { if (!CanMutate() || Value > 2) return false; NodeFlags = (NodeFlags & ~uint8(0xC)) | (Value << 2); return true; }
		uint8 GetSubclassBits() const { return NodeFlags >> 4; }
		bool SetSubclassBits(uint8 Value)
		{
			if (!CanMutate() || Value > 0xF) return false;
			NodeFlags = (NodeFlags & uint8(0xF)) | (Value << 4);
			return true;
		}
		asCASTNode(asEASTNodeFamily InFamily, uint16 InKind, asCSourceRange InRange)
			: SourceRange(MoveTemp(InRange)), RawKind(InKind), Family(InFamily) {}

	private:
		enum : uint8
		{
			InvalidFlag = 1 << 0,
			SealedFlag = 1 << 1,
		};

		friend class asCASTContext;
		void BindOwner(asCASTContext* InOwner, uint32 InLocalID) { OwnerContext = InOwner; ContextLocalID = InLocalID; }
		void SealNode() { NodeFlags |= SealedFlag; }

		// Keep the ubiquitous AST header compact: pointer/range first, then a
		// single packed tail for identity, kind, family, and state flags.
		asCASTContext* OwnerContext = nullptr;
		asCSourceRange SourceRange;
		uint32 ContextLocalID = 0;
		uint16 RawKind = 0;
		asEASTNodeFamily Family;
		uint8 NodeFlags = 0;
	};

	class ANGELSCRIPTRUNTIME_API asCDecl : public asCASTNode
	{
	public:
		asEDeclKind GetKind() const { return static_cast<asEDeclKind>(GetRawKind()); }
		asEAccessSpecifier GetAccess() const { return static_cast<asEAccessSpecifier>(GetAccessBits()); }
		bool SetAccess(asEAccessSpecifier Value) { return SetAccessBits(static_cast<uint8>(Value)); }
		asCDeclContext* GetDeclContext() const { return DeclContext; }
		bool AddAttr(asCAttr* Attr);
		TConstArrayView<asCAttr*> GetAttrs() const { return MakeArrayView(Attrs); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Decl; }

	protected:
		asCDecl(asEDeclKind InKind, asCSourceRange InRange)
			: asCASTNode(asEASTNodeFamily::Decl, static_cast<uint16>(InKind), MoveTemp(InRange)) {}

	private:
		friend class asCDeclContext;
		friend class asCFunctionDecl;
		void SetDeclContext(asCDeclContext* InContext) { DeclContext = InContext; }
		asCDeclContext* DeclContext = nullptr;
		TArray<asCAttr*> Attrs;
	};

	class ANGELSCRIPTRUNTIME_API asCNamedDecl : public asCDecl
	{
	public:
		const FString& GetName() const { return Name; }
		const asSStableKey& GetStableIdentity() const { return StableIdentity; }
		bool SetStableIdentity(asSStableKey Key) { if (!CanMutate() || Key.IsZero()) return false; StableIdentity = Key; return true; }
		// Observation only. Locals and other source declarations have no durable
		// entity identity; their authored location remains meaningful in AST dumps.
		FString GetSemanticKey() const;
		static bool classof(const asCASTNode* Node);

	protected:
		asCNamedDecl(asEDeclKind InKind, FString InName, asCSourceRange InRange)
			: asCDecl(InKind, MoveTemp(InRange)), Name(MoveTemp(InName)) {}

	private:
		FString Name;
		asSStableKey StableIdentity;
	};

	class ANGELSCRIPTRUNTIME_API asCDeclContext
	{
	public:
		explicit asCDeclContext(asCDecl* InOwner = nullptr) : Owner(InOwner) {}
		bool AddDecl(asCDecl* Decl);
		TConstArrayView<asCDecl*> GetDecls() const { return MakeArrayView(Decls); }
		TConstArrayView<asCDecl*> Lookup(const FString& Name) const;
		asCDecl* GetOwnerDecl() const { return Owner; }

	private:
		asCDecl* Owner = nullptr;
		TArray<asCDecl*> Decls;
		TMap<FString, TArray<asCDecl*>> LookupTable;
	};

	class ANGELSCRIPTRUNTIME_API asCTranslationUnitDecl final : public asCDecl, public asCDeclContext
	{
	public:
		explicit asCTranslationUnitDecl(asCSourceRange Range = {})
			: asCDecl(asEDeclKind::TranslationUnit, MoveTemp(Range)), asCDeclContext(this) {}
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Decl && Node->GetRawKind() == static_cast<uint16>(asEDeclKind::TranslationUnit); }
	};

	class ANGELSCRIPTRUNTIME_API asCNamespaceDecl final : public asCNamedDecl, public asCDeclContext
	{
	public:
		asCNamespaceDecl(FString Name, asCSourceRange Range)
			: asCNamedDecl(asEDeclKind::Namespace, MoveTemp(Name), MoveTemp(Range)), asCDeclContext(this) {}
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Decl && Node->GetRawKind() == static_cast<uint16>(asEDeclKind::Namespace); }
	};

	class ANGELSCRIPTRUNTIME_API asCRecordDecl final : public asCNamedDecl, public asCDeclContext
	{
	public:
		asCRecordDecl(FString Name, asCSourceRange Range, asETypeDeclKind InDeclaredTypeKind = asETypeDeclKind::Class)
			: asCNamedDecl(asEDeclKind::Record, MoveTemp(Name), MoveTemp(Range)), asCDeclContext(this), DeclaredTypeKind(InDeclaredTypeKind) {}
		asETypeDeclKind GetDeclaredTypeKind() const { return DeclaredTypeKind; }
		asERecordModifiers GetModifiers() const { return Modifiers; }
		bool SetModifiers(asERecordModifiers Value) { if (!CanMutate()) return false; Modifiers = Value; return true; }
		void AddUnresolvedBase(FString InName) { if (CanMutate()) UnresolvedBases.Add(MoveTemp(InName)); }
		TConstArrayView<FString> GetUnresolvedBases() const { return MakeArrayView(UnresolvedBases); }
		void AddResolvedBase(asCType* Type) { if (CanMutate() && Type) ResolvedBases.Add(Type); }
		TConstArrayView<asCType*> GetResolvedBases() const { return MakeArrayView(ResolvedBases); }
		bool SetTypeIdentity(asSStableKey Key) { return SetStableIdentity(Key); }
		const asSStableKey& GetTypeIdentity() const { return GetStableIdentity(); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Decl && Node->GetRawKind() == static_cast<uint16>(asEDeclKind::Record); }
	private:
		TArray<FString> UnresolvedBases;
		TArray<asCType*> ResolvedBases;
		asETypeDeclKind DeclaredTypeKind = asETypeDeclKind::Class;
		asERecordModifiers Modifiers = asERecordModifiers::None;
	};

	class ANGELSCRIPTRUNTIME_API asCAccessSpecifierDecl final : public asCNamedDecl
	{
	public:
		asCAccessSpecifierDecl(
			FString Name,
			asEAccessSpecifier InBaseAccess,
			TArray<asSAuthoredAccessPermission> InPermissions,
			asSAccessSpecifierSourceInfo InSourceInfo,
			asCSourceRange Range)
			: asCNamedDecl(asEDeclKind::AccessSpecifier, MoveTemp(Name), MoveTemp(Range))
			, Permissions(MoveTemp(InPermissions))
			, SourceInfo(MoveTemp(InSourceInfo))
			, BaseAccess(InBaseAccess) {}
		asEAccessSpecifier GetBaseAccess() const { return BaseAccess; }
		TConstArrayView<asSAuthoredAccessPermission> GetPermissions() const { return MakeArrayView(Permissions); }
		const asSAccessSpecifierSourceInfo& GetSourceInfo() const { return SourceInfo; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Decl && Node->GetRawKind() == static_cast<uint16>(asEDeclKind::AccessSpecifier); }
	private:
		TArray<asSAuthoredAccessPermission> Permissions;
		asSAccessSpecifierSourceInfo SourceInfo;
		asEAccessSpecifier BaseAccess = asEAccessSpecifier::Private;
	};

	class ANGELSCRIPTRUNTIME_API asCVarDecl final : public asCNamedDecl
	{
		struct asSVariableSourceInfo
		{
			asSTypeSyntax Type;
			asCSourceRange DeferredInitializerRange;
			asCExpr* Initializer = nullptr;
			TUniquePtr<asSAuthoredAccessUse> AuthoredAccessUse;
			uint8 SemanticQualifiers = 0;
			bool bHasSemanticQualifiers = false;
		};
	public:
		asCVarDecl(asEDeclKind Kind, FString Name, asCSourceRange Range)
			: asCNamedDecl(Kind, MoveTemp(Name), MoveTemp(Range)) { asASSERT(Kind == asEDeclKind::Var || Kind == asEDeclKind::ParmVar); }
		void SetTypeSpelling(FString Spelling) { if (CanMutate()) TypeSpelling = MoveTemp(Spelling); }
		const FString& GetTypeSpelling() const { return TypeSpelling; }
		bool SetTypeSyntax(asSTypeSyntax Syntax)
		{
			if (!CanMutate() || !Syntax.IsValid()) return false;
			TypeSpelling = Syntax.GetSpelling();
			if (!SourceInfo) SourceInfo = MakeUnique<asSVariableSourceInfo>();
			SourceInfo->Type = MoveTemp(Syntax);
			return true;
		}
		const asSTypeSyntax& GetTypeSyntax() const
		{
			static const asSTypeSyntax UnwrittenType;
			return SourceInfo ? SourceInfo->Type : UnwrittenType;
		}
		bool SetDeferredInitializerRange(asCSourceRange Range)
		{
			if (!CanMutate() || !Range.IsOrdered()) return false;
			if (!SourceInfo) SourceInfo = MakeUnique<asSVariableSourceInfo>();
			SourceInfo->DeferredInitializerRange = MoveTemp(Range); return true;
		}
		const asCSourceRange& GetDeferredInitializerRange() const { static const asCSourceRange Empty; return SourceInfo ? SourceInfo->DeferredInitializerRange : Empty; }
		bool HasDeferredInitializer() const { return GetDeferredInitializerRange().IsOrdered(); }
		bool SetInitializer(asCExpr* Expr)
		{
			if (!CanMutate() || !Expr) return false;
			if (!SourceInfo) SourceInfo = MakeUnique<asSVariableSourceInfo>();
			if (SourceInfo->Initializer) return false;
			SourceInfo->Initializer = Expr; return true;
		}
		asCExpr* GetInitializer() const { return SourceInfo ? SourceInfo->Initializer : nullptr; }
		bool SetAuthoredAccessUse(FString Name, asCSourceRange KeywordRange,
			asCSourceRange ColonRange, asCSourceRange NameRange);
		bool ResolveAuthoredAccessUse(asCAccessSpecifierDecl* Access);
		const asSAuthoredAccessUse* GetAuthoredAccessUse() const
		{
			return SourceInfo ? SourceInfo->AuthoredAccessUse.Get() : nullptr;
		}
		bool SetResolvedType(asCType* Type) { if (!CanMutate() || !Type) return false; ResolvedType = Type; return true; }
		asCType* GetResolvedType() const { return ResolvedType; }
		bool SetResolvedQualType(asCQualType Type);
		asCQualType GetResolvedQualType() const;
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Decl && (Node->GetRawKind() == static_cast<uint16>(asEDeclKind::Var) || Node->GetRawKind() == static_cast<uint16>(asEDeclKind::ParmVar)); }
	private:
		FString TypeSpelling;
		// As with Clang's TypeSourceInfo, authored location payload is separate
		// from the compact declaration. Synthesized declarations need none;
		// the concrete node destructor releases all source-owned UE containers.
		TUniquePtr<asSVariableSourceInfo> SourceInfo;
		asCType* ResolvedType = nullptr;
	};

	class ANGELSCRIPTRUNTIME_API asCEnumConstantDecl final : public asCNamedDecl
	{
		struct asSConstantSourceInfo
		{
			asCSourceRange DeferredInitializerRange;
			asCExpr* Initializer = nullptr;
		};
	public:
		asCEnumConstantDecl(FString Name, int64 InValue, bool bInExplicitValue, asCSourceRange Range)
			: asCNamedDecl(asEDeclKind::EnumConstant, MoveTemp(Name), MoveTemp(Range))
			, Value(InValue)
			, bExplicitValue(bInExplicitValue) {}
		int64 GetValue() const { return Value; }
		bool HasExplicitValue() const { return bExplicitValue; }
		bool HasResolvedValue() const { return bResolvedValue; }
		bool MarkValueUnresolved() { if (!CanMutate()) return false; bResolvedValue = false; return true; }
		bool SetResolvedValue(int64 InValue)
		{
			if (!CanMutate() || InValue < MIN_int32 || InValue > MAX_int32) return false;
			Value = InValue; bResolvedValue = true; return true;
		}
		bool SetDeferredInitializerRange(asCSourceRange Range)
		{
			if (!CanMutate() || !Range.IsOrdered()) return false;
			if (!SourceInfo) SourceInfo = MakeUnique<asSConstantSourceInfo>();
			SourceInfo->DeferredInitializerRange = MoveTemp(Range); bResolvedValue = false; return true;
		}
		const asCSourceRange& GetDeferredInitializerRange() const { static const asCSourceRange Empty; return SourceInfo ? SourceInfo->DeferredInitializerRange : Empty; }
		bool HasDeferredInitializer() const { return GetDeferredInitializerRange().IsOrdered(); }
		bool SetInitializer(asCExpr* Expr)
		{
			if (!CanMutate() || !Expr) return false;
			if (!SourceInfo) SourceInfo = MakeUnique<asSConstantSourceInfo>();
			if (SourceInfo->Initializer) return false;
			SourceInfo->Initializer = Expr; return true;
		}
		asCExpr* GetInitializer() const { return SourceInfo ? SourceInfo->Initializer : nullptr; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Decl && Node->GetRawKind() == static_cast<uint16>(asEDeclKind::EnumConstant); }
	private:
		int64 Value = 0;
		bool bExplicitValue = false;
		bool bResolvedValue = true;
		TUniquePtr<asSConstantSourceInfo> SourceInfo;
	};

	class ANGELSCRIPTRUNTIME_API asCFunctionDecl final : public asCNamedDecl, public asCDeclContext
	{
		struct asSFunctionSourceInfo
		{
			TUniquePtr<asSTypeSyntax> ReturnTypeSyntax;
			TUniquePtr<asSAuthoredAccessUse> AuthoredAccessUse;
		};
		struct asSImplicitFunctionSource
		{
			TArray<asCSourceRange> DeferredStatements;
		};
	public:
		asCFunctionDecl(FString Name, asCSourceRange Range)
			: asCNamedDecl(asEDeclKind::Function, MoveTemp(Name), MoveTemp(Range)), asCDeclContext(this) {}
		bool AddParameter(asCVarDecl* Parameter);
		TConstArrayView<asCVarDecl*> GetParameters() const { return MakeArrayView(Parameters); }
		bool SetDeferredBodyRange(asCSourceRange Range) { if (!CanMutate()) return false; DeferredBodyRange = MoveTemp(Range); bHasDeferredBody = DeferredBodyRange.IsOrdered(); return bHasDeferredBody; }
		bool HasDeferredBody() const { return bHasDeferredBody; }
		const asCSourceRange& GetDeferredBodyRange() const { return DeferredBodyRange; }
		void SetReturnTypeSpelling(FString Spelling) { if (CanMutate()) ReturnTypeSpelling = MoveTemp(Spelling); }
		const FString& GetReturnTypeSpelling() const { return ReturnTypeSpelling; }
		bool SetReturnTypeSyntax(asSTypeSyntax Syntax)
		{
			if (!CanMutate() || !Syntax.IsValid()) return false;
			ReturnTypeSpelling = Syntax.GetSpelling();
			if (!SourceInfo) SourceInfo = MakeUnique<asSFunctionSourceInfo>();
			SourceInfo->ReturnTypeSyntax = MakeUnique<asSTypeSyntax>(MoveTemp(Syntax));
			return true;
		}
		const asSTypeSyntax& GetReturnTypeSyntax() const { static const asSTypeSyntax Empty; return SourceInfo && SourceInfo->ReturnTypeSyntax ? *SourceInfo->ReturnTypeSyntax : Empty; }
		bool SetAuthoredAccessUse(FString Name, asCSourceRange KeywordRange,
			asCSourceRange ColonRange, asCSourceRange NameRange);
		bool ResolveAuthoredAccessUse(asCAccessSpecifierDecl* Access);
		const asSAuthoredAccessUse* GetAuthoredAccessUse() const
		{
			return SourceInfo ? SourceInfo->AuthoredAccessUse.Get() : nullptr;
		}
		bool SetResolvedSignature(asCFunctionType* Signature) { if (!CanMutate() || !Signature) return false; ResolvedSignature = Signature; return true; }
		asCFunctionType* GetResolvedSignature() const { return ResolvedSignature; }
		bool SetConstMethod(bool Value) { if (!CanMutate()) return false; bConstMethod = Value; return true; }
		bool IsConstMethod() const { return bConstMethod; }
		asEFunctionKind GetFunctionKind() const { return FunctionKind; }
		bool SetFunctionKind(asEFunctionKind Value) { if (!CanMutate() || Value == asEFunctionKind::ReservedAnonymousFunction || Value > asEFunctionKind::Conversion) return false; FunctionKind = Value; return true; }
		bool IsImplicit() const { return FunctionKind == asEFunctionKind::ClassDefault; }
		bool AddDeferredStatementRange(asCSourceRange Range)
		{
			if (!CanMutate() || FunctionKind != asEFunctionKind::ClassDefault || !Range.IsOrdered()) return false;
			if (!ImplicitSource) ImplicitSource = MakeUnique<asSImplicitFunctionSource>();
			ImplicitSource->DeferredStatements.Add(MoveTemp(Range)); return true;
		}
		TConstArrayView<asCSourceRange> GetDeferredStatementRanges() const { return ImplicitSource ? MakeArrayView(ImplicitSource->DeferredStatements) : TConstArrayView<asCSourceRange>{}; }
		asEFunctionModifiers GetModifiers() const { return Modifiers; }
		bool SetModifiers(asEFunctionModifiers Value) { if (!CanMutate()) return false; Modifiers = Value; return true; }
		bool SetResolvedReturnType(asCType* Type) { if (!CanMutate() || !Type) return false; ResolvedReturnType = Type; return true; }
		asCType* GetResolvedReturnType() const { return ResolvedReturnType; }
		bool SetBody(asCStmt* InBody) { if (!CanMutate()) return false; Body = InBody; return true; }
		asCStmt* GetBody() const { return Body; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Decl && Node->GetRawKind() == static_cast<uint16>(asEDeclKind::Function); }

	private:
		TArray<asCVarDecl*> Parameters;
		asCSourceRange DeferredBodyRange;
		bool bHasDeferredBody = false;
		FString ReturnTypeSpelling;
		TUniquePtr<asSFunctionSourceInfo> SourceInfo;
		asCFunctionType* ResolvedSignature = nullptr;
		bool bConstMethod = false;
		asEFunctionKind FunctionKind = asEFunctionKind::Ordinary;
		asEFunctionModifiers Modifiers = asEFunctionModifiers::None;
		TUniquePtr<asSImplicitFunctionSource> ImplicitSource;
		asCType* ResolvedReturnType = nullptr;
		asCStmt* Body = nullptr;
	};


	class ANGELSCRIPTRUNTIME_API asCTypeAliasDecl final : public asCNamedDecl
	{
	public:
		asCTypeAliasDecl(FString Name, asSTypeSyntax TypeSyntax, asCSourceRange Range)
			: asCNamedDecl(asEDeclKind::TypeAlias, MoveTemp(Name), MoveTemp(Range)), TypeSyntax(MoveTemp(TypeSyntax)) {}
		const asSTypeSyntax& GetTypeSyntax() const { return TypeSyntax; }
		bool SetResolvedAliasedType(asCType* Type) { if (!CanMutate() || !Type) return false; AliasedType = Type; return true; }
		asCType* GetResolvedAliasedType() const { return AliasedType; }
		asCQualType GetResolvedQualType() const;
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Decl && Node->GetRawKind() == static_cast<uint16>(asEDeclKind::TypeAlias); }
	private:
		asSTypeSyntax TypeSyntax;
		asCType* AliasedType = nullptr;
	};


	class ANGELSCRIPTRUNTIME_API asCRecoveryDecl final : public asCDecl
	{
	public:
		explicit asCRecoveryDecl(asCSourceRange Range) : asCDecl(asEDeclKind::Recovery, MoveTemp(Range)) { MarkInvalid(); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Decl && Node->GetRawKind() == static_cast<uint16>(asEDeclKind::Recovery); }
	};

	class ANGELSCRIPTRUNTIME_API asCCallableTypeDecl : public asCNamedDecl, public asCDeclContext
	{
	public:
		bool IsMulticast() const { return GetKind() == asEDeclKind::Event; }
		asECallableFlavor GetFlavor() const { return Flavor; }
		bool SetFlavor(asECallableFlavor InFlavor) { if (!CanMutate()) return false; Flavor = InFlavor; return true; }
		asETypeDeclKind GetDeclaredTypeKind() const { return IsMulticast() ? asETypeDeclKind::Event : asETypeDeclKind::Delegate; }
		const asSTypeSyntax& GetReturnTypeSyntax() const { return ReturnTypeSyntax; }
		bool SetReturnTypeSyntax(asSTypeSyntax Syntax)
		{
			if (!CanMutate() || !Syntax.IsValid()) return false;
			ReturnTypeSyntax = MoveTemp(Syntax);
			return true;
		}
		bool AddParameter(asCVarDecl* Parameter);
		TConstArrayView<asCVarDecl*> GetParameters() const { return MakeArrayView(Parameters); }
		bool SetTypeIdentity(asSStableKey Key) { return SetStableIdentity(Key); }
		const asSStableKey& GetTypeIdentity() const { return GetStableIdentity(); }
		bool SetResolvedSignature(asCFunctionType* Signature) { if (!CanMutate() || !Signature) return false; ResolvedSignature = Signature; return true; }
		asCFunctionType* GetResolvedSignature() const { return ResolvedSignature; }
		static bool classof(const asCASTNode* Node)
		{
			return Node && Node->GetFamily() == asEASTNodeFamily::Decl &&
				(Node->GetRawKind() == static_cast<uint16>(asEDeclKind::Delegate) || Node->GetRawKind() == static_cast<uint16>(asEDeclKind::Event));
		}

	protected:
		asCCallableTypeDecl(asEDeclKind Kind, FString Name, asCSourceRange Range)
			: asCNamedDecl(Kind, MoveTemp(Name), MoveTemp(Range)), asCDeclContext(this) {}

	private:
		asSTypeSyntax ReturnTypeSyntax;
		TArray<asCVarDecl*> Parameters;
		asCFunctionType* ResolvedSignature = nullptr;
		asECallableFlavor Flavor = asECallableFlavor::Ordinary;
	};

	class ANGELSCRIPTRUNTIME_API asCDelegateDecl final : public asCCallableTypeDecl
	{
	public:
		asCDelegateDecl(FString Name, asCSourceRange Range)
			: asCCallableTypeDecl(asEDeclKind::Delegate, MoveTemp(Name), MoveTemp(Range)) {}
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Decl && Node->GetRawKind() == static_cast<uint16>(asEDeclKind::Delegate); }
	};

	class ANGELSCRIPTRUNTIME_API asCEventDecl final : public asCCallableTypeDecl
	{
	public:
		asCEventDecl(FString Name, asCSourceRange Range)
			: asCCallableTypeDecl(asEDeclKind::Event, MoveTemp(Name), MoveTemp(Range)) {}
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Decl && Node->GetRawKind() == static_cast<uint16>(asEDeclKind::Event); }
	};

END_AS_NAMESPACE
