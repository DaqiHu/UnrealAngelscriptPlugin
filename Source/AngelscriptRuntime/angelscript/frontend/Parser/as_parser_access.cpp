#include "frontend/Parser/as_parser.h"
#include "frontend/AST/as_decl.h"

BEGIN_AS_NAMESPACE
bool asCParser::ParseAccessDeclaration()
	{
		const int32 Begin = Cursor++;
		asSAccessSpecifierSourceInfo Source;
		Source.KeywordRange = Tokens[Begin].Range;
		auto Reject = [&]() { Recover(Begin, TEXT("malformed-access-policy")); return false; };
		if (!Sema.GetCurrentRecord() || !Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Identifier) return Reject();
		const FString Name = Text(Cursor);
		Source.NameRange = Tokens[Cursor++].Range;
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Equal) return Reject();
		Source.EqualsRange = Tokens[Cursor++].Range;
		if (!Tokens.IsValidIndex(Cursor) || (Tokens[Cursor].Kind != asETokenKind::KwPrivate && Tokens[Cursor].Kind != asETokenKind::KwProtected)) return Reject();
		const asEAccessSpecifier Base = Tokens[Cursor].Kind == asETokenKind::KwPrivate ? asEAccessSpecifier::Private : asEAccessSpecifier::Protected;
		Source.BaseAccessRange = Tokens[Cursor++].Range;
		TArray<asSAuthoredAccessPermission> Permissions;
		while (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind == asETokenKind::Comma)
		{
			const int32 PermissionBegin = ++Cursor;
			if (!Tokens.IsValidIndex(Cursor)) return Reject();
			asSAuthoredAccessPermission Permission;
			if (Tokens[Cursor].Kind == asETokenKind::Star) Permission.Subject = asEAccessPermissionSubject::Wildcard;
			else if (Tokens[Cursor].Kind == asETokenKind::Identifier)
			{
				Permission.Subject = asEAccessPermissionSubject::Named;
				Permission.Name = Text(Cursor);
			}
			else return Reject();
			Permission.SubjectRange = Tokens[Cursor++].Range;
			if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind == asETokenKind::LeftParen)
			{
				++Cursor;
				while (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind != asETokenKind::RightParen)
				{
					const FString NameOfModifier = Text(Cursor);
					asSAccessPermissionModifier Modifier;
					if (NameOfModifier == TEXT("readonly")) Modifier.Kind = asEAccessPermissionModifier::ReadOnly;
					else if (NameOfModifier == TEXT("editdefaults")) Modifier.Kind = asEAccessPermissionModifier::EditDefaults;
					else if (NameOfModifier == TEXT("inherited")) Modifier.Kind = asEAccessPermissionModifier::Inherited;
					else return Reject();
					Modifier.Range = Tokens[Cursor++].Range;
					Permission.Modifiers.Add(MoveTemp(Modifier));
					if (!Tokens.IsValidIndex(Cursor)) return Reject();
					if (Tokens[Cursor].Kind == asETokenKind::RightParen) break;
					if (Tokens[Cursor++].Kind != asETokenKind::Comma) return Reject();
				}
				if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor++].Kind != asETokenKind::RightParen) return Reject();
			}
			Permission.Range = Span(PermissionBegin, Cursor - 1);
			Permissions.Add(MoveTemp(Permission));
		}
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Semicolon) return Reject();
		Source.SemicolonRange = Tokens[Cursor++].Range;
		Sema.ActOnAccessSpecifier(Name, Base, MoveTemp(Permissions), MoveTemp(Source), Span(Begin, Cursor - 1));
		return true;
	}
END_AS_NAMESPACE
