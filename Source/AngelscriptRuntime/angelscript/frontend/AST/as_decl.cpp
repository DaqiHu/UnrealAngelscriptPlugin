#include "frontend/AST/as_decl.h"
#include "frontend/AST/as_attr.h"
#include "frontend/AST/as_ast_context.h"

BEGIN_AS_NAMESPACE

asCQualType asCTypeAliasDecl::GetResolvedQualType() const
	{
		asCQualType Result;
		asETypeQualifiers Qualifiers = TypeSyntax.bConst ? asETypeQualifiers::Const : asETypeQualifiers::None;
		if (TypeSyntax.bHandle) Qualifiers = Qualifiers | asETypeQualifiers::Handle | asETypeQualifiers::Nullable;
		if (TypeSyntax.Reference != asETypeReferenceDirection::None) Qualifiers = Qualifiers | asETypeQualifiers::Reference;
		asCQualType::TryCreate(AliasedType, Qualifiers, Result);
		return Result;
	}

	FString asCNamedDecl::GetSemanticKey() const
	{
		if (!GetStableIdentity().IsZero()) return GetStableIdentity().ToHexString();
		FStringView LogicalSourceKey;
		if (GetOwnerContext()) GetOwnerContext()->GetSources().TryGetLogicalSourceKey(GetSourceRange().Begin.FileID, LogicalSourceKey);
		return FString::Printf(TEXT("source-decl:%s:%s:%u:%u"), *FString(LogicalSourceKey), *GetName(),
			GetSourceRange().Begin.ByteOffset, GetSourceRange().End.ByteOffset);
	}

	bool asCDecl::AddAttr(asCAttr* Attr)
	{
		if (!CanMutate() || !Attr || !Attr->AllowsTarget(GetKind())) return false;
		Attrs.Add(Attr);
		return true;
	}

	bool asCNamedDecl::classof(const asCASTNode* Node)
	{
		if (!asCDecl::classof(Node)) return false;
		const asEDeclKind Kind = static_cast<asEDeclKind>(Node->GetRawKind());
		if (Kind == asEDeclKind::TypeAlias || Kind == asEDeclKind::AccessSpecifier) return true;
		return Kind == asEDeclKind::Namespace || Kind == asEDeclKind::Record || Kind == asEDeclKind::EnumConstant || Kind == asEDeclKind::Function || Kind == asEDeclKind::Var || Kind == asEDeclKind::ParmVar || Kind == asEDeclKind::Delegate || Kind == asEDeclKind::Event;
	}

	namespace
	{
		bool IsOrderedAccessUse(
			const FString& Name,
			const asCSourceRange& KeywordRange,
			const asCSourceRange& ColonRange,
			const asCSourceRange& NameRange)
		{
			return !Name.IsEmpty() && KeywordRange.IsOrdered() && ColonRange.IsOrdered()
				&& NameRange.IsOrdered()
				&& KeywordRange.Begin.SnapshotID == ColonRange.Begin.SnapshotID
				&& KeywordRange.Begin.FileID == ColonRange.Begin.FileID
				&& ColonRange.Begin.SnapshotID == NameRange.Begin.SnapshotID
				&& ColonRange.Begin.FileID == NameRange.Begin.FileID
				&& KeywordRange.End.ByteOffset <= ColonRange.Begin.ByteOffset
				&& ColonRange.End.ByteOffset <= NameRange.Begin.ByteOffset;
		}

		bool ResolveAccessUse(
			asCDecl& Member,
			asSAuthoredAccessUse* Use,
			asCAccessSpecifierDecl* Access)
		{
			if (!Member.CanMutate() || !Use || Use->Resolved || !Access
				|| !Member.GetDeclContext() || Member.GetDeclContext() != Access->GetDeclContext()
				|| Use->Name != Access->GetName()) return false;
			Use->Resolved = Access;
			return true;
		}
	}

	bool asCVarDecl::SetAuthoredAccessUse(
		FString InName,
		asCSourceRange KeywordRange,
		asCSourceRange ColonRange,
		asCSourceRange NameRange)
	{
		if (!CanMutate() || GetKind() != asEDeclKind::Var || GetAuthoredAccessUse()
			|| !IsOrderedAccessUse(InName, KeywordRange, ColonRange, NameRange)) return false;
		if (!SourceInfo) SourceInfo = MakeUnique<asSVariableSourceInfo>();
		SourceInfo->AuthoredAccessUse = MakeUnique<asSAuthoredAccessUse>();
		SourceInfo->AuthoredAccessUse->Name = MoveTemp(InName);
		SourceInfo->AuthoredAccessUse->KeywordRange = MoveTemp(KeywordRange);
		SourceInfo->AuthoredAccessUse->ColonRange = MoveTemp(ColonRange);
		SourceInfo->AuthoredAccessUse->NameRange = MoveTemp(NameRange);
		return true;
	}

	bool asCVarDecl::ResolveAuthoredAccessUse(asCAccessSpecifierDecl* Access)
	{
		return ResolveAccessUse(*this, SourceInfo ? SourceInfo->AuthoredAccessUse.Get() : nullptr, Access);
	}

	bool asCFunctionDecl::SetAuthoredAccessUse(
		FString InName,
		asCSourceRange KeywordRange,
		asCSourceRange ColonRange,
		asCSourceRange NameRange)
	{
		if (!CanMutate() || GetAuthoredAccessUse()
			|| !IsOrderedAccessUse(InName, KeywordRange, ColonRange, NameRange)) return false;
		if (!SourceInfo) SourceInfo = MakeUnique<asSFunctionSourceInfo>();
		SourceInfo->AuthoredAccessUse = MakeUnique<asSAuthoredAccessUse>();
		SourceInfo->AuthoredAccessUse->Name = MoveTemp(InName);
		SourceInfo->AuthoredAccessUse->KeywordRange = MoveTemp(KeywordRange);
		SourceInfo->AuthoredAccessUse->ColonRange = MoveTemp(ColonRange);
		SourceInfo->AuthoredAccessUse->NameRange = MoveTemp(NameRange);
		return true;
	}

	bool asCFunctionDecl::ResolveAuthoredAccessUse(asCAccessSpecifierDecl* Access)
	{
		return ResolveAccessUse(*this, SourceInfo ? SourceInfo->AuthoredAccessUse.Get() : nullptr, Access);
	}

	bool asCDeclContext::AddDecl(asCDecl* Decl)
	{
		if ((Owner && !Owner->CanMutate()) || !Decl || Decl->GetDeclContext()) return false;
		Decl->SetDeclContext(this);
		Decls.Add(Decl);
		if (asCNamedDecl::classof(Decl))
		{
			asCNamedDecl* Named = static_cast<asCNamedDecl*>(Decl);
			LookupTable.FindOrAdd(Named->GetName()).Add(Decl);
		}
		return true;
	}

	TConstArrayView<asCDecl*> asCDeclContext::Lookup(const FString& Name) const
	{
		if (const TArray<asCDecl*>* Found = LookupTable.Find(Name)) return MakeArrayView(*Found);
		return {};
	}

	bool asCFunctionDecl::AddParameter(asCVarDecl* Parameter)
	{
		if (!CanMutate() || !Parameter || Parameter->GetKind() != asEDeclKind::ParmVar || !AddDecl(Parameter)) return false;
		Parameters.Add(Parameter);
		return true;
	}

	bool asCCallableTypeDecl::AddParameter(asCVarDecl* Parameter)
	{
		if (!CanMutate() || !Parameter || Parameter->GetKind() != asEDeclKind::ParmVar || !AddDecl(Parameter)) return false;
		Parameters.Add(Parameter);
		return true;
	}

END_AS_NAMESPACE
