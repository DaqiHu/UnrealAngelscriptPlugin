#include "frontend/as_sema.h"
#include "frontend/as_ast_cast.h"
#include "frontend/as_type_context.h"
#include "Containers/Set.h"

BEGIN_AS_NAMESPACE
namespace asAccessSema
	{
		bool HasModifier(const asCFunctionDecl& Function, asEFunctionModifiers Modifier)
		{
			return (static_cast<uint16>(Function.GetModifiers()) & static_cast<uint16>(Modifier)) != 0;
		}
		FString NamespaceName(const asCDecl* Decl)
		{
			TArray<FString> Parts;
			for (const asCDeclContext* Scope = Decl ? Decl->GetDeclContext() : nullptr; Scope; )
			{
				const asCDecl* Owner = Scope->GetOwnerDecl();
				if (!Owner) break;
				if (const auto* Namespace = dyn_cast<asCNamespaceDecl>(Owner)) Parts.Insert(Namespace->GetName(), 0);
				Scope = Owner->GetDeclContext();
			}
			return FString::Join(Parts, TEXT("::"));
		}
	}

	asCAccessSpecifierDecl* asCSema::ActOnAccessSpecifier(FString Name, asEAccessSpecifier BaseAccess,
		TArray<asSAuthoredAccessPermission> Permissions, asSAccessSpecifierSourceInfo Source, asCSourceRange Range)
	{
		auto* Decl = Context.Create<asCAccessSpecifierDecl>(MoveTemp(Name), BaseAccess, MoveTemp(Permissions), MoveTemp(Source), MoveTemp(Range));
		Attach(Decl, {});
		return Decl;
	}

	bool asCSema::IsEditDefaultsContext() const
	{
		const asCFunctionDecl* Function = GetCurrentFunction();
		if (!Function) return false;
		const FString& Name = Function->GetName();
		return Function->GetFunctionKind() == asEFunctionKind::ClassDefault
			|| asAccessSema::HasModifier(*Function, asEFunctionModifiers::Defaults)
			|| Name == TEXT("__InitDefaults") || Name == TEXT("ConstructionScript_Implementation")
			|| Name.StartsWith(TEXT("__Init_")) || Name == TEXT("OnActorModifiedInEditor_Implementation")
			|| Name == TEXT("OnComponentModifiedInEditor_Implementation")
			|| (Context.GetTypeContext()->GetOptions().AllowEditorOnlyAccess && Name.StartsWith(TEXT("Editor_")));
	}

	asCSema::asSMemberAccess asCSema::GetMemberAccess(const asCDecl* Member) const
	{
		if (!Member) return {};
		const asSAuthoredAccessUse* Use = nullptr;
		if (const auto* Variable = dyn_cast<asCVarDecl>(Member)) Use = Variable->GetAuthoredAccessUse();
		else if (const auto* Function = dyn_cast<asCFunctionDecl>(Member)) Use = Function->GetAuthoredAccessUse();
		const asCRecordDecl* Owner = Member->GetDeclContext() ? dyn_cast<asCRecordDecl>(Member->GetDeclContext()->GetOwnerDecl()) : nullptr;
		asCRecordDecl* Caller = GetCurrentRecord();
		if (Owner && Caller == Owner) return {true, true, true};
		if (Use && (!Use->Resolved || !Owner || Use->Resolved->GetDeclContext() != Owner)) return {};
		const asEAccessSpecifier Base = Use ? Use->Resolved->GetBaseAccess() : Member->GetAccess();
		if (!Use && Base == asEAccessSpecifier::Public) return {true, true, true};

		TArray<asCRecordDecl*> CallerBases;
		TArray<asCRecordDecl*> Pending;
		if (Caller) Pending.Add(Caller);
		TSet<asCRecordDecl*> Visited;
		while (!Pending.IsEmpty())
		{
			asCRecordDecl* Next = Pending.Pop();
			if (!Next || Visited.Contains(Next)) continue;
			Visited.Add(Next); CallerBases.Add(Next);
			for (asCType* Type : Next->GetResolvedBases()) Pending.Add(ResolveRecord(Type));
		}
		if (Base == asEAccessSpecifier::Protected && Owner && CallerBases.Contains(Owner)) return {true, true, true};
		if (!Use) return {};

		const asCFunctionDecl* CallingFunction = GetCurrentFunction();
		const FString Namespace = asAccessSema::NamespaceName(Caller ? static_cast<const asCDecl*>(Caller) : CallingFunction);
		asSMemberAccess Access;
		for (const asSAuthoredAccessPermission& Permission : Use->Resolved->GetPermissions())
		{
			bool ReadOnly = false, EditDefaults = false, Inherited = false;
			for (const asSAccessPermissionModifier& Modifier : Permission.Modifiers)
			{
				ReadOnly |= Modifier.Kind == asEAccessPermissionModifier::ReadOnly;
				EditDefaults |= Modifier.Kind == asEAccessPermissionModifier::EditDefaults;
				Inherited |= Modifier.Kind == asEAccessPermissionModifier::Inherited;
			}
			if (Permission.Subject == asEAccessPermissionSubject::Wildcard)
			{
				// A bare wildcard is not public access. Authored inherited on '*'
				// is preserved in the AST but has no effective language permission.
				Access.Read |= ReadOnly; Access.Edit |= EditDefaults;
				continue;
			}
			bool Matches = (Caller && Permission.Name == Caller->GetName())
				|| (!Caller && CallingFunction && Permission.Name == CallingFunction->GetName())
				|| (!Namespace.IsEmpty() && Permission.Name == Namespace);
			if (!Matches && Inherited)
				for (const asCRecordDecl* BaseType : CallerBases)
					if (Permission.Name == BaseType->GetName()) { Matches = true; break; }
			if (!Matches) continue;
			Access.Read |= ReadOnly || !EditDefaults;
			Access.Write |= !ReadOnly && !EditDefaults;
			Access.Edit |= EditDefaults || !ReadOnly;
		}
		return Access;
	}

	bool asCSema::CanAccessMember(const asCDecl* Member) const
	{
		const asSMemberAccess Access = GetMemberAccess(Member);
		if (Access.Edit && IsEditDefaultsContext()) return true;
		if (!Access.Read) return false;
		const auto* Function = dyn_cast<asCFunctionDecl>(Member);
		return !Function || Access.Write || Function->IsConstMethod();
	}

	bool asCSema::IsConstructionContext() const
	{
		const asCFunctionDecl* Caller = GetCurrentFunction();
		const asCRecordDecl* Record = GetCurrentRecord();
		return Caller && (Caller->GetFunctionKind() == asEFunctionKind::ClassDefault
			|| (Caller->GetFunctionKind() == asEFunctionKind::Constructor && Record && Record->GetDeclaredTypeKind() == asETypeDeclKind::Class));
	}

	bool asCSema::CheckCallContext(const asCFunctionDecl& Function, const asCSourceRange& Range)
	{
		if (!CanAccessMember(&Function)) { ReportBodyError(Range, TEXT("inaccessible-function")); return false; }
		if (asAccessSema::HasModifier(Function, asEFunctionModifiers::Defaults) && !IsEditDefaultsContext())
		{ ReportBodyError(Range, TEXT("defaults-only-call-outside-defaults")); return false; }
		if (IsConstructionContext() && asAccessSema::HasModifier(Function, asEFunctionModifiers::UnsafeDuringConstruction))
		{ ReportBodyError(Range, TEXT("unsafe-call-during-construction")); return false; }
		return true;
	}
END_AS_NAMESPACE
