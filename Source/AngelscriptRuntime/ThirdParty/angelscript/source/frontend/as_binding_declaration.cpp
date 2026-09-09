#include "frontend/as_binding_declaration.h"
#include "frontend/as_type_syntax_parser.h"
#include "frontend/as_tokenizer.h"
#include "Containers/StringConv.h"

BEGIN_AS_NAMESPACE
namespace
{
	struct asSHostDeclarationReader
	{
		const asSBindingDeclarationContext& Environment;
		asCTypeContext& Context;
		asFBindingNominalLookup Lookup;
		asSBindingDeclaration& Result;
		asSBindingDeclarationDiagnostic& Diagnostic;
		TArray<asCToken> Tokens;
		int32 Cursor = 0;

		asETokenKind Kind() const { return Tokens[Cursor].Kind; }
		bool Take(asETokenKind Expected) { if (Kind() != Expected) return false; ++Cursor; return true; }
		FString Text(const asCSourceRange& Range) const
		{
			TConstArrayView<uint8> Bytes;
			if (!Result.Source->TryGetBytes(Range, Bytes)) return {};
			FUTF8ToTCHAR Value(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
			return FString(Value.Length(), Value.Get());
		}
		bool Fail(const TCHAR* Message, const asCSourceRange& Range)
		{
			Diagnostic = {Message, Range};
			return false;
		}
		bool Fail(const TCHAR* Message) { return Fail(Message, Tokens[FMath::Min(Cursor, Tokens.Num() - 1)].Range); }

		bool Resolve(const asSTypeSyntax& Syntax, asSStableKey& Key)
		{
			asSStableTypeUseDescriptor Type;
			const FString Name = Syntax.GetQualifiedName();
			if (Syntax.bWildcard)
			{
				if (Syntax.bGlobal || Syntax.bHandle || !Syntax.GenericArguments.IsEmpty() || !Syntax.ArrayDimensions.IsEmpty())
					return Fail(TEXT("invalid wildcard type qualifiers or arguments"), Syntax.Range);
				Type.Kind = asETypeUseKind::Wildcard;
			}
			else if (Syntax.NameParts.Num() == 1 && !Syntax.bGlobal)
			{
				struct asSPrimitiveName { const TCHAR* Name; asEPrimitiveType Type; };
				static const asSPrimitiveName Primitives[] = {
					{TEXT("void"), asEPrimitiveType::Void}, {TEXT("bool"), asEPrimitiveType::Bool},
					{TEXT("int8"), asEPrimitiveType::Int8}, {TEXT("int16"), asEPrimitiveType::Int16},
					{TEXT("int"), asEPrimitiveType::Int32}, {TEXT("int32"), asEPrimitiveType::Int32}, {TEXT("int64"), asEPrimitiveType::Int64},
					{TEXT("uint8"), asEPrimitiveType::UInt8}, {TEXT("uint16"), asEPrimitiveType::UInt16},
					{TEXT("uint"), asEPrimitiveType::UInt32}, {TEXT("uint32"), asEPrimitiveType::UInt32}, {TEXT("uint64"), asEPrimitiveType::UInt64},
					{TEXT("float32"), asEPrimitiveType::Float32}, {TEXT("float64"), asEPrimitiveType::Float64}, {TEXT("double"), asEPrimitiveType::Float64}
				};
				for (const auto& Primitive : Primitives)
					if (Name.Equals(Primitive.Name, ESearchCase::CaseSensitive)) Type.Primitive = Primitive.Type;
				if (Name.Equals(TEXT("float"), ESearchCase::CaseSensitive))
					Type.Primitive = Context.GetOptions().FloatIsFloat64 ? asEPrimitiveType::Float64 : asEPrimitiveType::Float32;
			}
			if (Type.Kind == asETypeUseKind::Wildcard)
			{
			}
			else if (Type.Primitive != asEPrimitiveType::Invalid)
			{
				Type.Kind = asETypeUseKind::Primitive;
				if (!Syntax.GenericArguments.IsEmpty() || Syntax.bHandle ||
					(Type.Primitive == asEPrimitiveType::Void && (Syntax.bConst || Syntax.Reference != asETypeReferenceDirection::None || !Syntax.ArrayDimensions.IsEmpty())))
					return Fail(TEXT("invalid primitive type qualifiers or arguments"), Syntax.Range);
			}
			else
			{
				Type.Kind = asETypeUseKind::Nominal;
				Type.Declaration = Lookup((Syntax.bGlobal ? TEXT("::") : TEXT("")) + Name, Environment.Namespace);
				asSNominalTypeDescriptor Nominal;
				if (Context.GetIdentityRegistry().ResolveNominalType(Type.Declaration, Nominal) != asETypeIdentityStatus::Succeeded)
					return Fail(TEXT("unknown nominal type"), Syntax.Range);
				if (Nominal.GenericArity != static_cast<asUINT>(Syntax.GenericArguments.Num()))
					return Fail(TEXT("template argument count does not match declaration"), Syntax.Range);
				for (const auto& Argument : Syntax.GenericArguments)
				{
					asSStableKey ArgumentKey;
					if (!Resolve(Argument, ArgumentKey)) return false;
					asSStableTypeUseDescriptor ArgumentType;
					Context.GetIdentityRegistry().ResolveTypeUse(ArgumentKey, ArgumentType);
					if (ArgumentType.Primitive == asEPrimitiveType::Void) return Fail(TEXT("void template argument"), Argument.Range);
					Type.Arguments.Add(ArgumentKey);
				}
				if (Syntax.bHandle && Nominal.Kind != asETypeDeclKind::Class && Nominal.Kind != asETypeDeclKind::Interface)
					return Fail(TEXT("handle requires an object type"), Syntax.Range);
			}
			if (Context.GetIdentityRegistry().InternTypeUse(Type, Key) != asETypeIdentityStatus::Succeeded)
				return Fail(TEXT("invalid canonical type"), Syntax.Range);
			for (const auto& Dimension : Syntax.ArrayDimensions)
			{
				if (!Context.GetOptions().ExpandDefaultArrayToTemplate) return Fail(TEXT("default array syntax is disabled"), Dimension);
				asSStableTypeUseDescriptor Array;
				Array.Kind = asETypeUseKind::Nominal;
				Array.Declaration = Lookup(Context.GetOptions().ArrayTemplateTypeName, Environment.Namespace);
				asSNominalTypeDescriptor ArrayDeclaration;
				if (Context.GetIdentityRegistry().ResolveNominalType(Array.Declaration, ArrayDeclaration) != asETypeIdentityStatus::Succeeded || ArrayDeclaration.GenericArity != 1)
					return Fail(TEXT("default array template is unavailable"), Dimension);
				Array.Arguments.Add(Key);
				if (Context.GetIdentityRegistry().InternTypeUse(Array, Key) != asETypeIdentityStatus::Succeeded)
					return Fail(TEXT("invalid array type"), Dimension);
			}
			asETypeUseQualifiers Qualifiers = Syntax.bConst ? asETypeUseQualifiers::Const : asETypeUseQualifiers::None;
			if (Syntax.bHandle) Qualifiers = Qualifiers | asETypeUseQualifiers::Handle | asETypeUseQualifiers::Nullable;
			if (Syntax.Reference != asETypeReferenceDirection::None) Qualifiers = Qualifiers | asETypeUseQualifiers::Reference;
			asSStableKey Qualified;
			if (Context.InternQualifiedTypeUse(Key, Qualifiers, Qualified) != asETypeIdentityStatus::Succeeded)
				return Fail(TEXT("invalid qualified type"), Syntax.Range);
			Key = Qualified;
			return true;
		}
		bool ReadType(asSTypeSyntax& Syntax, asSStableKey& Key)
		{
			if (!asCTypeSyntaxParser(Tokens).ParseTypeSyntax(Cursor, Tokens.Num() - 1, Syntax))
				return Fail(TEXT("malformed type syntax"));
			return Resolve(Syntax, Key);
		}
		bool ReadDefault(FString& Out)
		{
			const int32 Begin = Cursor;
			TArray<asETokenKind> Closers;
			while (Kind() != asETokenKind::EndOfFile)
			{
				const auto TokenKind = Kind();
				if (Closers.IsEmpty() && (TokenKind == asETokenKind::Comma || TokenKind == asETokenKind::RightParen)) break;
				if (TokenKind == asETokenKind::LeftParen) Closers.Add(asETokenKind::RightParen);
				else if (TokenKind == asETokenKind::LeftBracket) Closers.Add(asETokenKind::RightBracket);
				else if (TokenKind == asETokenKind::LeftBrace) Closers.Add(asETokenKind::RightBrace);
				else if (TokenKind == asETokenKind::RightParen || TokenKind == asETokenKind::RightBracket || TokenKind == asETokenKind::RightBrace)
				{
					if (Closers.IsEmpty() || Closers.Pop(EAllowShrinking::No) != TokenKind) return Fail(TEXT("unbalanced default expression"));
				}
				++Cursor;
			}
			if (Begin == Cursor || !Closers.IsEmpty()) return Fail(TEXT("missing or unbalanced default expression"));
			Out = Text({Tokens[Begin].Range.Begin, Tokens[Cursor - 1].Range.End});
			return true;
		}
		bool Read()
		{
			asSTypeSyntax ReturnSyntax;
			if (!ReadType(ReturnSyntax, Result.Type)) return false;
			if (Kind() == asETokenKind::EndOfFile) return true;
			if (Kind() != asETokenKind::Identifier) return Fail(TEXT("declaration name required"));
			Result.Name = Text(Tokens[Cursor++].Range);
			if (!Take(asETokenKind::LeftParen))
			{
				if (Kind() != asETokenKind::EndOfFile) return Fail(TEXT("unexpected tokens after property"));
				asSStableTypeUseDescriptor Type; Context.GetIdentityRegistry().ResolveTypeUse(Result.Type, Type);
				return Type.Primitive != asEPrimitiveType::Void || Fail(TEXT("property cannot have void type"), ReturnSyntax.Range);
			}
			bool SawDefault = false;
			if (Kind() == asETokenKind::KwVoid && Tokens[Cursor + 1].Kind == asETokenKind::RightParen) ++Cursor;
			while (Kind() != asETokenKind::RightParen)
			{
				asSBindingParameter Parameter;
				const int32 Begin = Cursor;
				asSTypeSyntax Syntax;
				if (!ReadType(Syntax, Parameter.Identity.Type)) return false;
				asSStableTypeUseDescriptor Type; Context.GetIdentityRegistry().ResolveTypeUse(Parameter.Identity.Type, Type);
				if (Type.Primitive == asEPrimitiveType::Void) return Fail(TEXT("parameter cannot have void type"), Syntax.Range);
				if (Syntax.bConst && Syntax.Reference == asETypeReferenceDirection::Out) return Fail(TEXT("out parameter cannot be const"), Syntax.Range);
				switch (Syntax.Reference)
				{
				case asETypeReferenceDirection::In: Parameter.Identity.PassingMode = asEParameterPassingMode::In; break;
				case asETypeReferenceDirection::Out: Parameter.Identity.PassingMode = asEParameterPassingMode::Out; break;
				case asETypeReferenceDirection::InOut: Parameter.Identity.PassingMode = asEParameterPassingMode::InOut; break;
				default: break;
				}
				if (Kind() == asETokenKind::Identifier) Parameter.Name = Text(Tokens[Cursor++].Range);
				if (Take(asETokenKind::Equal))
				{
					SawDefault = true;
					if (!ReadDefault(Parameter.DefaultText)) return false;
				}
				else if (SawDefault) return Fail(TEXT("parameter after default requires a default"));
				Parameter.Range = {Tokens[Begin].Range.Begin, Tokens[Cursor - 1].Range.End};
				Result.Parameters.Add(MoveTemp(Parameter));
				if (Kind() == asETokenKind::RightParen) break;
				if (!Take(asETokenKind::Comma) || Kind() == asETokenKind::RightParen) return Fail(TEXT("parameter separator or type required"));
			}
			++Cursor;
			auto& Signature = Result.Signature;
			Signature.StableScope = Environment.StableScope;
			Environment.Namespace.ParseIntoArray(Signature.NamespacePath, TEXT("::"), true);
			Signature.Owner = Environment.Owner;
			Signature.Kind = Environment.FunctionKind;
			Signature.DefinitionModifiers = Environment.DefinitionModifiers;
			Signature.Name = Result.Name;
			Signature.ReturnType = Result.Type;
			if (Signature.Kind == asEFunctionIdentityKind::Conversion) Signature.ConversionDestination = Result.Type;
			for (const auto& Parameter : Result.Parameters) Signature.Parameters.Add(Parameter.Identity);
			if (Take(asETokenKind::KwConst))
			{
				if (Signature.Kind != asEFunctionIdentityKind::Method && Signature.Kind != asEFunctionIdentityKind::Conversion)
					return Fail(TEXT("const method requires a method owner"));
				Signature.Modifiers = asEFunctionIdentityModifiers::ConstMethod;
			}
			bool SawAcceptTemporaryThis = false;
			bool SawNoDiscard = false;
			bool SawAllowDiscard = false;
			while (Kind() == asETokenKind::Identifier)
			{
				const FString Suffix = Text(Tokens[Cursor].Range);
				if (Suffix.Equals(TEXT("accept_temporary_this"), ESearchCase::CaseSensitive) && !SawAcceptTemporaryThis)
					SawAcceptTemporaryThis = true;
				else if (Suffix.Equals(TEXT("no_discard"), ESearchCase::CaseSensitive) && !SawNoDiscard)
					SawNoDiscard = true;
				else if (Suffix.Equals(TEXT("allow_discard"), ESearchCase::CaseSensitive) && !SawAllowDiscard)
					SawAllowDiscard = true;
				else
					break;
				++Cursor;
			}
			if (Kind() != asETokenKind::EndOfFile) return Fail(TEXT("unexpected tokens after callable declaration"));
			const auto Status = Context.GetIdentityRegistry().InternFunction(Signature, Result.Function);
			if (Status == asETypeIdentityStatus::ReturnTypeConflict) return Fail(TEXT("conflicting return type for existing callable"), Result.Range);
			return Status == asETypeIdentityStatus::Succeeded || Fail(TEXT("invalid callable identity"), Result.Range);
		}
	};
}

bool asCBindingDeclarationParser::Parse(const FString& Text, const asSBindingDeclarationContext& Environment,
	asCTypeContext& Context, asFBindingNominalLookup Lookup, asSBindingDeclaration& Out,
	asSBindingDeclarationDiagnostic& Diagnostic)
{
	Out = {}; Diagnostic = {};
	const FTCHARToUTF8 Utf8(*Text, Text.Len());
	auto Snapshot = MakeShared<asCSourceSnapshot, ESPMode::ThreadSafe>();
	const auto File = Snapshot->AddFile(TEXT("binding-declaration"), MakeArrayView(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length()));
	Snapshot->Freeze();
	Out.Source = MakeShared<asCSourceManager, ESPMode::ThreadSafe>(Snapshot);
	Out.Range = Out.Source->MakeRange(File, 0, Utf8.Length());
	asSBindingDeclaration Candidate;
	Candidate.Source = Out.Source; Candidate.Range = Out.Range;
	asSHostDeclarationReader Reader{Environment, Context, Lookup, Candidate, Diagnostic};
	asCIdentifierTable Identifiers;
	asCDiagnosticsEngine LexDiagnostics(*Out.Source);
	asCTokenizer Lexer(*Out.Source, Out.Range, asSLexOptions{}, Identifiers, LexDiagnostics);
	asCToken Token;
	while (Lexer.Lex(Token))
	{
		Reader.Tokens.Add(Token);
		if (Token.Kind == asETokenKind::EndOfFile) break;
	}
	Lexer.FlushDiagnostics();
	if (LexDiagnostics.GetPendingCount())
	{
		asCCollectingDiagnosticConsumer Consumer; LexDiagnostics.EmitTo(Consumer);
		Diagnostic = {TEXT("invalid token in binding declaration"), Consumer.GetRecords()[0].PrimaryRange};
		return false;
	}
	if (Reader.Tokens.IsEmpty() || Reader.Tokens.Last().Kind != asETokenKind::EndOfFile)
	{
		Diagnostic = {TEXT("unable to tokenize binding declaration"), Out.Range};
		return false;
	}
	if (!Reader.Read()) return false;
	Out = MoveTemp(Candidate);
	return true;
}
END_AS_NAMESPACE
