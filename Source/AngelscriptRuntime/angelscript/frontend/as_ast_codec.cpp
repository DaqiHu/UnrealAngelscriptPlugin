#include "frontend/as_ast_codec.h"

#include "Containers/Set.h"
#include "Containers/StringConv.h"

BEGIN_AS_NAMESPACE

namespace
	{
		enum class asEASTCodecPayloadTag : uint8
		{
			Empty, NamedDecl, RecordDecl, VarDecl, EnumConstantDecl, FunctionDecl,
			CallableTypeDecl, IntegerLiteral, BooleanLiteral, FloatingLiteral,
			ImplicitCast, UnaryOperator, BinaryOperator, BuiltinType, NominalType,
			FunctionType, FlagAttr, StringAttr, AnnotationAttr,
			TypeAliasDecl, CallExpr, MemberExpr, ExplicitCast, ConstructExpr,
			StringLiteral,
			ThisExpr, ReservedAnonymousFunction, InitListExpr, ValueInitExpr, DeclGroupStmt,
			ClassDefaultStmt, ForStmt, SwitchStmt, CaseStmt, DefaultStmt,
			FallthroughStmt, ForeachStmt,
			AccessSpecifierDecl, UserConversion,
		};

		static constexpr uint32 CodecMaxTypeSyntaxDepth = 32;

		class asSASTCodecWriter
		{
		public:
			TArray<uint8> Bytes;
			void U8(uint8 V) { Bytes.Add(V); }
			void U16(uint16 V) { U8(static_cast<uint8>(V)); U8(static_cast<uint8>(V >> 8)); }
			void U32(uint32 V) { for (int32 I = 0; I < 4; ++I) U8(static_cast<uint8>(V >> (I * 8))); }
			void U64(uint64 V) { for (int32 I = 0; I < 8; ++I) U8(static_cast<uint8>(V >> (I * 8))); }
			void Bool(bool V) { U8(V ? 1 : 0); }
			void Key(const asSStableKey& V) { Bytes.Append(V.GetBytes()); }
			void String(const FString& V)
			{
				FTCHARToUTF8 UTF8(*V);
				U32(static_cast<uint32>(UTF8.Length()));
				Bytes.Append(reinterpret_cast<const uint8*>(UTF8.Get()), UTF8.Length());
			}
		};

		class asSASTCodecReader
		{
		public:
			explicit asSASTCodecReader(TConstArrayView<uint8> In) : Bytes(In) {}
			bool U8(uint8& V) { if (Pos >= Bytes.Num()) return false; V = Bytes[Pos++]; return true; }
			bool U16(uint16& V)
			{
				uint8 A = 0, B = 0; if (!U8(A) || !U8(B)) return false;
				V = static_cast<uint16>(A) | (static_cast<uint16>(B) << 8); return true;
			}
			bool U32(uint32& V)
			{
				V = 0; for (int32 I = 0; I < 4; ++I) { uint8 B = 0; if (!U8(B)) return false; V |= static_cast<uint32>(B) << (I * 8); }
				return true;
			}
			bool U64(uint64& V)
			{
				V = 0; for (int32 I = 0; I < 8; ++I) { uint8 B = 0; if (!U8(B)) return false; V |= static_cast<uint64>(B) << (I * 8); }
				return true;
			}
			bool Bool(bool& V) { uint8 B = 0; if (!U8(B) || B > 1) return false; V = B != 0; return true; }
			bool Key(asSStableKey& Out)
			{
				if (Remaining() < static_cast<int32>(asSStableKey::ByteCount)) return false;
				if (!asSStableKey::TryCreate(MakeArrayView(Bytes.GetData() + Pos, asSStableKey::ByteCount), Out)) return false;
				Pos += asSStableKey::ByteCount; return true;
			}
			bool String(FString& Out)
			{
				uint32 N = 0; if (!U32(N) || N > static_cast<uint32>(Remaining())) return false;
				FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + Pos), static_cast<int32>(N));
				Out = FString(Converted.Length(), Converted.Get()); Pos += static_cast<int32>(N); return true;
			}
			int32 Remaining() const { return Bytes.Num() - Pos; }
			bool AtEnd() const { return Pos == Bytes.Num(); }
		private:
			TConstArrayView<uint8> Bytes;
			int32 Pos = 0;
		};

		bool CodecIsEmptyAnchor(const asSStableSourceAnchor& A)
		{
			return A.LogicalSourceKey.IsEmpty() && A.ContentRevision == 0
				&& A.BeginByteOffset == 0 && A.EndByteOffset == 0;
		}

		void CodecWriteAnchor(asSASTCodecWriter& W, const asSStableSourceAnchor& A)
		{
			W.String(A.LogicalSourceKey); W.U64(A.ContentRevision); W.U32(A.BeginByteOffset); W.U32(A.EndByteOffset);
		}

		bool CodecReadAnchor(asSASTCodecReader& R, asSStableSourceAnchor& A)
		{
			return R.String(A.LogicalSourceKey) && R.U64(A.ContentRevision)
				&& R.U32(A.BeginByteOffset) && R.U32(A.EndByteOffset);
		}

		bool CodecIsKnownKind(asEASTNodeFamily Family, uint16 Kind)
		{
			switch (Family)
			{
			case asEASTNodeFamily::Decl:
				switch (static_cast<asEDeclKind>(Kind))
				{
#define AS_DECL_NODE(Class, Base, Name) case asEDeclKind::Name: return true;
#include "frontend/as_decl_nodes.def"
#undef AS_DECL_NODE
				case asEDeclKind::ParmVar: return true; default: return false;
				}
			case asEASTNodeFamily::Stmt:
				switch (static_cast<asEStmtKind>(Kind))
				{
#define AS_STMT_NODE(Class, Base, Name) case asEStmtKind::Name: return true;
#include "frontend/as_stmt_nodes.def"
#undef AS_STMT_NODE
				default: return false;
				}
			case asEASTNodeFamily::Type:
				switch (static_cast<asETypeKind>(Kind))
				{
#define AS_TYPE_NODE(Class, Base, Name) case asETypeKind::Name: return true;
#include "frontend/as_type_nodes.def"
#undef AS_TYPE_NODE
				default: return false;
				}
			case asEASTNodeFamily::Attr:
				switch (static_cast<asEAttrKind>(Kind))
				{
#define AS_ATTR_NODE(Class, Base, Name) case asEAttrKind::Name: return true;
#include "frontend/as_attr_nodes.def"
#undef AS_ATTR_NODE
				default: return false;
				}
			default: return false;
			}
		}

		template <typename Enum>
		bool CodecEnumAtMost(Enum V, Enum Last) { return static_cast<uint8>(V) <= static_cast<uint8>(Last); }

		bool CodecIsExpressionKind(uint16 Kind)
		{
			const asEStmtKind K = static_cast<asEStmtKind>(Kind);
			return (K >= asEStmtKind::IntegerLiteral && K <= asEStmtKind::ValueInitExpr)
				|| K == asEStmtKind::UserConversionExpr;
		}

		bool CodecIsValidAttrTarget(asEAttrTarget V)
		{
			const uint8 Bits = static_cast<uint8>(V);
			return Bits != 0 && (Bits & ~static_cast<uint8>(asEAttrTarget::Any)) == 0;
		}

		bool CodecAnchorWithin(
			const asSStableSourceAnchor& Inner,
			const asSStableSourceAnchor& Outer)
		{
			return Inner.IsValid() && Outer.IsValid()
				&& Inner.LogicalSourceKey == Outer.LogicalSourceKey
				&& Inner.ContentRevision == Outer.ContentRevision
				&& Inner.BeginByteOffset >= Outer.BeginByteOffset
				&& Inner.EndByteOffset <= Outer.EndByteOffset;
		}

		bool CodecAnchorOrderedBefore(
			const asSStableSourceAnchor& Left,
			const asSStableSourceAnchor& Right)
		{
			return Left.IsValid() && Right.IsValid()
				&& Left.LogicalSourceKey == Right.LogicalSourceKey
				&& Left.ContentRevision == Right.ContentRevision
				&& Left.EndByteOffset <= Right.BeginByteOffset;
		}

		bool CodecIsValidTypeSyntax(const asSASTProjectedTypeSyntax& S, uint32 Depth = 1)
		{
			if (Depth > CodecMaxTypeSyntaxDepth) return false;
			if (!S.bPresent)
				return S.Spelling.IsEmpty() && S.NameParts.IsEmpty() && S.ComponentRanges.IsEmpty()
					&& S.GenericArguments.IsEmpty() && S.ArrayDimensions.IsEmpty()
					&& CodecIsEmptyAnchor(S.Source) && !S.bGlobal && !S.bConst && !S.bHandle
					&& S.Reference == asETypeReferenceDirection::None;
			if (S.Spelling.IsEmpty() || S.NameParts.IsEmpty() || S.NameParts.Num() != S.ComponentRanges.Num()
				|| !S.Source.IsValid() || !CodecEnumAtMost(S.Reference, asETypeReferenceDirection::InOut)) return false;
			for (int32 I = 0; I < S.NameParts.Num(); ++I)
				if (S.NameParts[I].IsEmpty() || !CodecAnchorWithin(S.ComponentRanges[I], S.Source)) return false;
			for (const asSASTProjectedTypeSyntax& Argument : S.GenericArguments)
				if (!Argument.bPresent || !CodecAnchorWithin(Argument.Source, S.Source)
					|| !CodecIsValidTypeSyntax(Argument, Depth + 1)) return false;
			for (const asSStableSourceAnchor& Dimension : S.ArrayDimensions)
				if (!CodecAnchorWithin(Dimension, S.Source)) return false;

			FString Expected = S.bConst ? TEXT("const ") : TEXT("");
			if (S.bGlobal) Expected += TEXT("::");
			Expected += FString::Join(S.NameParts, TEXT("::"));
			if (!S.GenericArguments.IsEmpty())
			{
				Expected += TEXT("<");
				for (int32 I = 0; I < S.GenericArguments.Num(); ++I)
				{
					if (I) Expected += TEXT(",");
					Expected += S.GenericArguments[I].Spelling;
				}
				Expected += TEXT(">");
			}
			for (int32 I = 0; I < S.ArrayDimensions.Num(); ++I) Expected += TEXT("[]");
			if (S.bHandle) Expected += TEXT("@");
			if (S.Reference == asETypeReferenceDirection::In) Expected += TEXT("&in");
			else if (S.Reference == asETypeReferenceDirection::Out) Expected += TEXT("&out");
			else if (S.Reference == asETypeReferenceDirection::InOut) Expected += TEXT("&inout");
			return S.Spelling == Expected;
		}

		bool CodecIsValidTypeSyntaxForRecord(
			const asSASTProjectedTypeSyntax& Syntax,
			const asSASTProjectionRecord& Record)
		{
			return CodecIsValidTypeSyntax(Syntax)
				&& (!Syntax.bPresent || CodecAnchorWithin(Syntax.Source, Record.Source));
		}

		bool CodecIsValidAnnotationArguments(
			TConstArrayView<asSASTProjectedAnnotationArgument> Arguments,
			const asSStableSourceAnchor& AttributeSource)
		{
			for (int32 I = 0; I < Arguments.Num(); ++I)
			{
				const auto& A = Arguments[I];
				if (A.Name.IsEmpty() || !CodecEnumAtMost(A.Kind, asEAnnotationValueKind::Group)
					|| !CodecAnchorWithin(A.Source, AttributeSource)) return false;
				if (A.ParentIndex != asSASTProjectedAnnotationArgument::NoParent
					&& (A.ParentIndex >= static_cast<uint32>(I) || Arguments[A.ParentIndex].Kind != asEAnnotationValueKind::Group)) return false;
				if (!CodecIsEmptyAnchor(A.ValueSource) && !CodecAnchorWithin(A.ValueSource, A.Source)) return false;
			}
			return true;
		}

		asEASTCodecPayloadTag CodecExpectedPayloadTag(asEASTNodeFamily Family, uint16 Kind)
		{
			switch (Family)
			{
			case asEASTNodeFamily::Decl:
				switch (static_cast<asEDeclKind>(Kind))
				{
				case asEDeclKind::TranslationUnit: case asEDeclKind::Recovery: return asEASTCodecPayloadTag::Empty;
				case asEDeclKind::Namespace: return asEASTCodecPayloadTag::NamedDecl;
				case asEDeclKind::Record: return asEASTCodecPayloadTag::RecordDecl;
				case asEDeclKind::Var: case asEDeclKind::ParmVar: return asEASTCodecPayloadTag::VarDecl;
				case asEDeclKind::EnumConstant: return asEASTCodecPayloadTag::EnumConstantDecl;
				case asEDeclKind::Function: return asEASTCodecPayloadTag::FunctionDecl;
				case asEDeclKind::Delegate: case asEDeclKind::Event: return asEASTCodecPayloadTag::CallableTypeDecl;
				case asEDeclKind::TypeAlias: return asEASTCodecPayloadTag::TypeAliasDecl;
				case asEDeclKind::AccessSpecifier: return asEASTCodecPayloadTag::AccessSpecifierDecl;
				default: return asEASTCodecPayloadTag::Empty;
				}
			case asEASTNodeFamily::Stmt:
				switch (static_cast<asEStmtKind>(Kind))
				{
				case asEStmtKind::IntegerLiteral: return asEASTCodecPayloadTag::IntegerLiteral;
				case asEStmtKind::BooleanLiteral: return asEASTCodecPayloadTag::BooleanLiteral;
				case asEStmtKind::FloatingLiteral: return asEASTCodecPayloadTag::FloatingLiteral;
				case asEStmtKind::ImplicitCastExpr: return asEASTCodecPayloadTag::ImplicitCast;
				case asEStmtKind::UserConversionExpr: return asEASTCodecPayloadTag::UserConversion;
				case asEStmtKind::UnaryOperator: return asEASTCodecPayloadTag::UnaryOperator;
				case asEStmtKind::BinaryOperator: return asEASTCodecPayloadTag::BinaryOperator;
				case asEStmtKind::CallExpr: return asEASTCodecPayloadTag::CallExpr;
				case asEStmtKind::MemberExpr: return asEASTCodecPayloadTag::MemberExpr;
				case asEStmtKind::ExplicitCastExpr: return asEASTCodecPayloadTag::ExplicitCast;
				case asEStmtKind::ConstructExpr: return asEASTCodecPayloadTag::ConstructExpr;
				case asEStmtKind::StringLiteral: return asEASTCodecPayloadTag::StringLiteral;
				case asEStmtKind::ThisExpr: return asEASTCodecPayloadTag::ThisExpr;
				case asEStmtKind::InitListExpr: return asEASTCodecPayloadTag::InitListExpr;
				case asEStmtKind::ValueInitExpr: return asEASTCodecPayloadTag::ValueInitExpr;
				case asEStmtKind::DeclGroupStmt: return asEASTCodecPayloadTag::DeclGroupStmt;
				case asEStmtKind::ClassDefaultStmt: return asEASTCodecPayloadTag::ClassDefaultStmt;
				case asEStmtKind::For: return asEASTCodecPayloadTag::ForStmt;
				case asEStmtKind::Switch: return asEASTCodecPayloadTag::SwitchStmt;
				case asEStmtKind::Case: return asEASTCodecPayloadTag::CaseStmt;
				case asEStmtKind::Default: return asEASTCodecPayloadTag::DefaultStmt;
				case asEStmtKind::Fallthrough: return asEASTCodecPayloadTag::FallthroughStmt;
				case asEStmtKind::Foreach: return asEASTCodecPayloadTag::ForeachStmt;
				default: return asEASTCodecPayloadTag::Empty;
				}
			case asEASTNodeFamily::Type:
				switch (static_cast<asETypeKind>(Kind))
				{
				case asETypeKind::Builtin: return asEASTCodecPayloadTag::BuiltinType;
				case asETypeKind::Nominal: return asEASTCodecPayloadTag::NominalType;
				case asETypeKind::Function: return asEASTCodecPayloadTag::FunctionType;
				default: return asEASTCodecPayloadTag::Empty;
				}
			case asEASTNodeFamily::Attr:
				switch (static_cast<asEAttrKind>(Kind))
				{
				case asEAttrKind::Flag: return asEASTCodecPayloadTag::FlagAttr;
				case asEAttrKind::String: return asEASTCodecPayloadTag::StringAttr;
				case asEAttrKind::Annotation: return asEASTCodecPayloadTag::AnnotationAttr;
				default: return asEASTCodecPayloadTag::Empty;
				}
			default: return asEASTCodecPayloadTag::Empty;
			}
		}

		asEASTCodecPayloadTag CodecGetPayloadTag(const asFASTProjectionPayload& P)
		{
			// Projection groups the new payloads beside their related AST shapes.
			// The wire keeps every v6 tag stable, appends both v7 alternatives,
			// and reserves the removed anonymous-function tag in v9.
			if (P.TryGet<asSASTAccessSpecifierDeclPayload>())
				return asEASTCodecPayloadTag::AccessSpecifierDecl;
			if (P.TryGet<asSASTUserConversionPayload>())
				return asEASTCodecPayloadTag::UserConversion;
			const int32 VariantIndex = P.GetIndex();
			if (VariantIndex < 7)
				return static_cast<asEASTCodecPayloadTag>(VariantIndex);
			if (VariantIndex < 12)
				return static_cast<asEASTCodecPayloadTag>(VariantIndex - 1);
			if (VariantIndex < 28)
				return static_cast<asEASTCodecPayloadTag>(VariantIndex - 2);
			return static_cast<asEASTCodecPayloadTag>(VariantIndex - 1);
		}

		bool CodecValidateAccessSpecifierPayload(
			const asSASTAccessSpecifierDeclPayload& Access,
			const asSASTProjectionRecord& Record)
		{
			if (Access.Name.IsEmpty()
				|| (Access.BaseAccess != asEAccessSpecifier::Private
					&& Access.BaseAccess != asEAccessSpecifier::Protected)
				|| !CodecAnchorWithin(Access.KeywordSource, Record.Source)
				|| !CodecAnchorWithin(Access.NameSource, Record.Source)
				|| !CodecAnchorWithin(Access.EqualsSource, Record.Source)
				|| !CodecAnchorWithin(Access.BaseAccessSource, Record.Source)
				|| !CodecAnchorWithin(Access.SemicolonSource, Record.Source)
				|| !CodecAnchorOrderedBefore(Access.KeywordSource, Access.NameSource)
				|| !CodecAnchorOrderedBefore(Access.NameSource, Access.EqualsSource)
				|| !CodecAnchorOrderedBefore(Access.EqualsSource, Access.BaseAccessSource)
				|| !CodecAnchorOrderedBefore(Access.BaseAccessSource, Access.SemicolonSource))
			{
				return false;
			}

			const asSStableSourceAnchor* PreviousPermission = &Access.BaseAccessSource;
			for (const asSASTProjectedAccessPermission& Permission : Access.Permissions)
			{
				if (!CodecEnumAtMost(Permission.Subject, asEAccessPermissionSubject::Wildcard)
					|| ((Permission.Subject == asEAccessPermissionSubject::Named)
						!= !Permission.Name.IsEmpty())
					|| !CodecAnchorWithin(Permission.SubjectSource, Permission.Source)
					|| !CodecAnchorWithin(Permission.Source, Record.Source)
					|| !CodecAnchorOrderedBefore(*PreviousPermission, Permission.Source)
					|| !CodecAnchorOrderedBefore(Permission.Source, Access.SemicolonSource))
				{
					return false;
				}
				const asSStableSourceAnchor* PreviousModifier = &Permission.SubjectSource;
				for (const asSASTProjectedAccessPermissionModifier& Modifier : Permission.Modifiers)
				{
					if (!CodecEnumAtMost(Modifier.Kind, asEAccessPermissionModifier::Inherited)
						|| !CodecAnchorWithin(Modifier.Source, Permission.Source)
						|| !CodecAnchorOrderedBefore(*PreviousModifier, Modifier.Source))
					{
						return false;
					}
					PreviousModifier = &Modifier.Source;
				}
				PreviousPermission = &Permission.Source;
			}
			return true;
		}

		bool CodecValidateAuthoredAccess(
			const asSASTProjectedAuthoredAccessUse& Access,
			const asSASTProjectionRecord& Record)
		{
			if (!Access.bPresent)
			{
				return Access.Name.IsEmpty()
					&& CodecIsEmptyAnchor(Access.KeywordSource)
					&& CodecIsEmptyAnchor(Access.ColonSource)
					&& CodecIsEmptyAnchor(Access.NameSource);
			}
			const bool bMemberKind = Record.Family == asEASTNodeFamily::Decl
				&& (Record.Kind == static_cast<uint16>(asEDeclKind::Var)
					|| Record.Kind == static_cast<uint16>(asEDeclKind::Function));
			return bMemberKind && !Access.Name.IsEmpty()
				&& CodecAnchorWithin(Access.KeywordSource, Record.Source)
				&& CodecAnchorWithin(Access.ColonSource, Record.Source)
				&& CodecAnchorWithin(Access.NameSource, Record.Source)
				&& CodecAnchorOrderedBefore(Access.KeywordSource, Access.ColonSource)
				&& CodecAnchorOrderedBefore(Access.ColonSource, Access.NameSource);
		}

		bool CodecIsConversionFunctionName(const FString& Name)
		{
			return Name == TEXT("opConv") || Name == TEXT("opImplConv")
				|| Name == TEXT("opCast") || Name == TEXT("opImplCast");
		}

		bool CodecValidatePayload(const asSASTProjectionRecord& R)
		{
			if (CodecGetPayloadTag(R.Payload) != CodecExpectedPayloadTag(R.Family, R.Kind)) return false;
			if (const auto* Named = R.Payload.TryGet<asSASTNamedDeclPayload>()) return !Named->Name.IsEmpty();
			if (const auto* Record = R.Payload.TryGet<asSASTRecordDeclPayload>())
			{
				if (Record->Name.IsEmpty()
					|| Record->DeclaredTypeKind < asETypeDeclKind::Class
					|| Record->DeclaredTypeKind > asETypeDeclKind::Enum) return false;
				constexpr uint8 ValidModifiers = static_cast<uint8>(asERecordModifiers::Final)
					| static_cast<uint8>(asERecordModifiers::Abstract)
					| static_cast<uint8>(asERecordModifiers::Mixin)
					| static_cast<uint8>(asERecordModifiers::Local);
				if ((static_cast<uint8>(Record->Modifiers) & ~ValidModifiers) != 0) return false;
				for (const FString& Base : Record->UnresolvedBases) if (Base.IsEmpty()) return false;
				return true;
			}
			if (const auto* Variable = R.Payload.TryGet<asSASTVarDeclPayload>())
				return (!Variable->Name.IsEmpty() || R.Kind == static_cast<uint16>(asEDeclKind::ParmVar))
					&& (R.Kind != static_cast<uint16>(asEDeclKind::Var) || Variable->TypeSyntax.bPresent)
					&& CodecIsValidTypeSyntaxForRecord(Variable->TypeSyntax, R)
					&& ((!Variable->bHasDeferredInitializer && CodecIsEmptyAnchor(Variable->DeferredInitializerSource))
						|| (Variable->bHasDeferredInitializer && CodecAnchorWithin(Variable->DeferredInitializerSource, R.Source)));
			if (const auto* Constant = R.Payload.TryGet<asSASTEnumConstantDeclPayload>())
				return !Constant->Name.IsEmpty() && Constant->bResolvedValue
					&& ((!Constant->bHasDeferredInitializer && CodecIsEmptyAnchor(Constant->DeferredInitializerSource))
						|| (Constant->bHasDeferredInitializer && CodecAnchorWithin(Constant->DeferredInitializerSource, R.Source)));
			if (const auto* Function = R.Payload.TryGet<asSASTFunctionDeclPayload>())
			{
				constexpr uint16 ValidModifiers = static_cast<uint16>(asEFunctionModifiers::Final)
					| static_cast<uint16>(asEFunctionModifiers::Override)
					| static_cast<uint16>(asEFunctionModifiers::Property)
					| static_cast<uint16>(asEFunctionModifiers::Explicit)
					| static_cast<uint16>(asEFunctionModifiers::Local)
					| static_cast<uint16>(asEFunctionModifiers::Mixin)
					| static_cast<uint16>(asEFunctionModifiers::AcceptTemporary)
					| static_cast<uint16>(asEFunctionModifiers::ExternalImplicitThis)
					| static_cast<uint16>(asEFunctionModifiers::NoDiscard)
					| static_cast<uint16>(asEFunctionModifiers::AllowDiscard)
					| static_cast<uint16>(asEFunctionModifiers::Generated)
					| static_cast<uint16>(asEFunctionModifiers::Deprecated)
					| static_cast<uint16>(asEFunctionModifiers::Defaults)
					| static_cast<uint16>(asEFunctionModifiers::UnsafeDuringConstruction);
				const bool bImplicitFunction = Function->FunctionKind == asEFunctionKind::ClassDefault;
				if (Function->Name.IsEmpty() != bImplicitFunction
					|| bImplicitFunction == Function->ReturnTypeSyntax.bPresent
					|| !CodecIsValidTypeSyntaxForRecord(Function->ReturnTypeSyntax, R)
					|| !CodecEnumAtMost(Function->FunctionKind, asEFunctionKind::Conversion)
					|| Function->FunctionKind == asEFunctionKind::ReservedAnonymousFunction
					|| (Function->FunctionKind == asEFunctionKind::Conversion
						&& !CodecIsConversionFunctionName(Function->Name))
					|| (static_cast<uint16>(Function->Modifiers) & ~ValidModifiers) != 0
					|| !((!Function->bHasDeferredBody && CodecIsEmptyAnchor(Function->DeferredBodySource))
						|| (Function->bHasDeferredBody && CodecAnchorWithin(Function->DeferredBodySource, R.Source)))) return false;
				if (Function->FunctionKind != asEFunctionKind::ClassDefault && !Function->DeferredStatementSources.IsEmpty()) return false;
				for (const asSStableSourceAnchor& Source : Function->DeferredStatementSources)
					if (!CodecAnchorWithin(Source, R.Source)) return false;
				return true;
			}
			if (const auto* Callable = R.Payload.TryGet<asSASTCallableTypeDeclPayload>()) return !Callable->Name.IsEmpty() && Callable->ReturnTypeSyntax.bPresent && CodecIsValidTypeSyntaxForRecord(Callable->ReturnTypeSyntax, R);
			if (const auto* Access = R.Payload.TryGet<asSASTAccessSpecifierDeclPayload>())
				return CodecValidateAccessSpecifierPayload(*Access, R);
			if (const auto* Alias = R.Payload.TryGet<asSASTTypeAliasDeclPayload>())
				return !Alias->Name.IsEmpty() && Alias->TypeSyntax.bPresent
					&& CodecIsValidTypeSyntaxForRecord(Alias->TypeSyntax, R)
					&& !Alias->TypeSyntax.bConst && !Alias->TypeSyntax.bHandle
					&& Alias->TypeSyntax.Reference == asETypeReferenceDirection::None
					&& Alias->TypeSyntax.GenericArguments.IsEmpty() && Alias->TypeSyntax.ArrayDimensions.IsEmpty();
			if (const auto* Cast = R.Payload.TryGet<asSASTImplicitCastPayload>()) return Cast->Conversion > asEImplicitConversionKind::None && CodecEnumAtMost(Cast->Conversion, asEImplicitConversionKind::ReferenceBinding);
			if (const auto* Conversion = R.Payload.TryGet<asSASTUserConversionPayload>())
				return CodecEnumAtMost(Conversion->Mode, asEUserConversionMode::Explicit);
			if (const auto* Unary = R.Payload.TryGet<asSASTUnaryOperatorPayload>()) return CodecEnumAtMost(Unary->Operator, asEUnaryOperator::PostDecrement) && CodecAnchorWithin(Unary->OperatorSource, R.Source);
			if (const auto* Binary = R.Payload.TryGet<asSASTBinaryOperatorPayload>()) return CodecEnumAtMost(Binary->Operator, asEBinaryOperator::BitwiseOrAssign) && CodecAnchorWithin(Binary->OperatorSource, R.Source);
			if (const auto* Call = R.Payload.TryGet<asSASTCallExprPayload>())
			{
				if (R.Children.IsEmpty() || Call->ArgumentSourceOrdinals.Num() != R.Children.Num() - 1) return false;
				TSet<int32> Authored;
				for (int32 Ordinal : Call->ArgumentSourceOrdinals)
				{
					if (Ordinal < INDEX_NONE || Ordinal >= Call->ArgumentSourceOrdinals.Num()) return false;
					if (Ordinal != INDEX_NONE && Authored.Contains(Ordinal)) return false;
					if (Ordinal != INDEX_NONE) Authored.Add(Ordinal);
				}
				for (int32 Ordinal = 0; Ordinal < Authored.Num(); ++Ordinal) if (!Authored.Contains(Ordinal)) return false;
				return true;
			}
			if (const auto* Member = R.Payload.TryGet<asSASTMemberExprPayload>()) return CodecAnchorWithin(Member->MemberSource, R.Source);
			if (const auto* ExplicitCast = R.Payload.TryGet<asSASTExplicitCastPayload>()) return ExplicitCast->TypeSyntax.bPresent && CodecIsValidTypeSyntaxForRecord(ExplicitCast->TypeSyntax, R);
			if (const auto* Construct = R.Payload.TryGet<asSASTConstructExprPayload>())
			{
				if (!Construct->TypeSyntax.bPresent || !CodecIsValidTypeSyntaxForRecord(Construct->TypeSyntax, R)) return false;
				if (Construct->ArgumentSourceOrdinals.Num() != R.Children.Num()) return false;
				TSet<int32> Authored;
				for (int32 Ordinal : Construct->ArgumentSourceOrdinals)
				{
					if (Ordinal < INDEX_NONE || Ordinal >= Construct->ArgumentSourceOrdinals.Num()) return false;
					if (Ordinal != INDEX_NONE && Authored.Contains(Ordinal)) return false;
					if (Ordinal != INDEX_NONE) Authored.Add(Ordinal);
				}
				for (int32 Ordinal = 0; Ordinal < Authored.Num(); ++Ordinal) if (!Authored.Contains(Ordinal)) return false;
				return true;
			}
			if (const auto* Initializer = R.Payload.TryGet<asSASTInitListExprPayload>())
			{
				if (Initializer->InitializerIdentity.IsZero()
					|| Initializer->ElementPatternIndices.Num() != R.Children.Num()) return false;
				for (int32 Index : Initializer->ElementPatternIndices)
					if (Index < 0) return false;
				return true;
			}
			if (const auto* Group = R.Payload.TryGet<asSASTDeclGroupStmtPayload>())
				return Group->DeclarationCount >= 2 && Group->DeclarationCount == static_cast<uint32>(R.Children.Num());
			if (const auto* ClassDefault = R.Payload.TryGet<asSASTClassDefaultStmtPayload>())
				return CodecAnchorWithin(ClassDefault->KeywordSource, R.Source);
			if (const auto* For = R.Payload.TryGet<asSASTForStmtPayload>())
				return CodecAnchorWithin(For->HeaderSource, R.Source);
			if (const auto* Switch = R.Payload.TryGet<asSASTSwitchStmtPayload>())
				return Switch->ClauseCount > 0 && CodecAnchorWithin(Switch->HeaderSource, R.Source)
					&& (Switch->DefaultOrdinal == MAX_uint32 || Switch->DefaultOrdinal < Switch->ClauseCount);
			if (const auto* Case = R.Payload.TryGet<asSASTCaseStmtPayload>())
				return CodecAnchorWithin(Case->KeywordSource, R.Source)
					&& CodecAnchorWithin(Case->ColonSource, R.Source)
					&& Case->KeywordSource.EndByteOffset <= Case->ColonSource.BeginByteOffset;
			if (const auto* Default = R.Payload.TryGet<asSASTDefaultStmtPayload>())
				return CodecAnchorWithin(Default->KeywordSource, R.Source)
					&& CodecAnchorWithin(Default->ColonSource, R.Source)
					&& Default->KeywordSource.EndByteOffset <= Default->ColonSource.BeginByteOffset;
			if (const auto* Foreach = R.Payload.TryGet<asSASTForeachStmtPayload>())
				return CodecAnchorWithin(Foreach->HeaderSource, R.Source)
					&& CodecAnchorWithin(Foreach->ColonSource, Foreach->HeaderSource);
			if (const auto* Builtin = R.Payload.TryGet<asSASTBuiltinTypePayload>()) return Builtin->Primitive > asEPrimitiveType::Invalid && Builtin->Primitive <= asEPrimitiveType::Float64;
			if (const auto* Nominal = R.Payload.TryGet<asSASTNominalTypePayload>()) return !Nominal->DeclarationIdentity.IsZero();
			if (const auto* FunctionType = R.Payload.TryGet<asSASTFunctionTypePayload>()) { for (auto Mode : FunctionType->PassingModes) if (!CodecEnumAtMost(Mode, asEParameterPassingMode::InOut)) return false; return true; }
			if (const auto* Flag = R.Payload.TryGet<asSASTFlagAttrPayload>()) return !Flag->Name.IsEmpty() && CodecIsValidAttrTarget(Flag->AllowedTargets);
			if (const auto* String = R.Payload.TryGet<asSASTStringAttrPayload>()) return !String->Name.IsEmpty() && CodecIsValidAttrTarget(String->AllowedTargets);
			if (const auto* Annotation = R.Payload.TryGet<asSASTAnnotationAttrPayload>()) return CodecEnumAtMost(Annotation->AnnotationKind, asEAnnotationKind::UMeta) && CodecIsValidAttrTarget(Annotation->AllowedTargets) && CodecIsValidAnnotationArguments(Annotation->Arguments, R.Source);
			return true;
		}

		bool CodecValidateRecord(const asSASTProjectionRecord& R, uint32 Count)
		{
			if (!CodecIsKnownKind(R.Family, R.Kind)) return false;
			if (R.Family == asEASTNodeFamily::Type)
			{
				if (!CodecIsEmptyAnchor(R.Source) || R.StableIdentity.IsZero() || !R.Children.IsEmpty()) return false;
			}
			else if (!R.Source.IsValid()) return false;
			if (R.Family == asEASTNodeFamily::Attr && !R.Children.IsEmpty()) return false;
			for (uint32 Child : R.Children) if (Child >= Count) return false;
			for (const auto& Ref : R.CrossReferences)
				if (!CodecEnumAtMost(Ref.Kind, asEASTCrossReferenceKind::ConversionFunction) || (Ref.Kind >= asEASTCrossReferenceKind::ReservedAnonymousEnclosingFunction && Ref.Kind <= asEASTCrossReferenceKind::ReservedAnonymousCapturedThis) || Ref.LocalIndex > Count || (Ref.LocalIndex == 0 && Ref.StableIdentity.IsZero())) return false;
			for (const auto& Ref : R.SemanticReferences)
				if (!CodecEnumAtMost(Ref.Kind, asEASTSemanticTypeReferenceKind::ForeachIteratorType) || !CodecEnumAtMost(Ref.PassingMode, asEParameterPassingMode::InOut) || Ref.TypeIdentity.IsZero()) return false;
			if (R.Declaration.bPresent != (R.Family == asEASTNodeFamily::Decl)
				|| (R.Declaration.bPresent && !CodecEnumAtMost(R.Declaration.Access, asEAccessSpecifier::Private))
				|| (!R.Declaration.bPresent && R.Declaration.Access != asEAccessSpecifier::Public)
				|| !CodecValidateAuthoredAccess(R.Declaration.AuthoredAccess, R)) return false;
			int32 CustomAccessReferences = 0;
			for (const asSASTProjectedCrossReference& Reference : R.CrossReferences)
				if (Reference.Kind == asEASTCrossReferenceKind::CustomAccessSpecifier)
				{
					if (Reference.LocalIndex == 0) return false;
					++CustomAccessReferences;
				}
			if (R.Declaration.AuthoredAccess.bPresent != (CustomAccessReferences == 1)) return false;
			const bool bExpr = R.Family == asEASTNodeFamily::Stmt && CodecIsExpressionKind(R.Kind);
			if (R.Expression.bPresent != bExpr) return false;
			if (R.Expression.bPresent && (R.Expression.TypeIdentity.IsZero() || R.Expression.ValueCategory <= asEValueCategory::Invalid || R.Expression.ValueCategory > asEValueCategory::PRValue)) return false;
			if (!R.Expression.bPresent && (!R.Expression.TypeIdentity.IsZero()
				|| R.Expression.ValueCategory != asEValueCategory::Invalid || R.Expression.bReadOnly)) return false;
			return CodecValidatePayload(R);
		}

		bool CodecIsStmtRecord(const asSASTProjectionRecord& Record)
		{
			return Record.Family == asEASTNodeFamily::Stmt;
		}

		bool CodecIsExpressionRecord(const asSASTProjectionRecord& Record)
		{
			return CodecIsStmtRecord(Record) && CodecIsExpressionKind(Record.Kind);
		}

		bool CodecIsStmtKind(const asSASTProjectionRecord& Record, asEStmtKind Kind)
		{
			return CodecIsStmtRecord(Record) && Record.Kind == static_cast<uint16>(Kind);
		}

		bool CodecValidateStatementChildren(
			const asSASTProjectionRecord& Record,
			TConstArrayView<asSASTProjectionRecord> Records)
		{
			auto Child = [&Record, Records](int32 Index) -> const asSASTProjectionRecord&
			{
				return Records[static_cast<int32>(Record.Children[Index])];
			};
			auto AllChildrenAreExpressions = [&Record, Records](int32 Begin = 0)
			{
				for (int32 Index = Begin; Index < Record.Children.Num(); ++Index)
					if (!CodecIsExpressionRecord(Records[static_cast<int32>(Record.Children[Index])])) return false;
				return true;
			};

			switch (static_cast<asEStmtKind>(Record.Kind))
			{
			case asEStmtKind::Compound:
				for (uint32 Index : Record.Children) if (!CodecIsStmtRecord(Records[static_cast<int32>(Index)])) return false;
				return true;
			case asEStmtKind::If:
				return (Record.Children.Num() == 2 || Record.Children.Num() == 3)
					&& CodecIsExpressionRecord(Child(0)) && CodecIsStmtRecord(Child(1))
					&& (Record.Children.Num() == 2 || CodecIsStmtRecord(Child(2)));
			case asEStmtKind::Return:
				return Record.Children.Num() <= 1 && (Record.Children.IsEmpty() || CodecIsExpressionRecord(Child(0)));
			case asEStmtKind::While:
				return Record.Children.Num() == 2 && CodecIsExpressionRecord(Child(0)) && CodecIsStmtRecord(Child(1));
			case asEStmtKind::Break:
			case asEStmtKind::Continue:
			case asEStmtKind::RecoveryStmt:
			case asEStmtKind::NullStmt:
			case asEStmtKind::Fallthrough:
				return Record.Children.IsEmpty();
			case asEStmtKind::DeclStmt:
				return (Record.Children.Num() == 1 || Record.Children.Num() == 2)
					&& Child(0).Family == asEASTNodeFamily::Decl
					&& (Record.Children.Num() == 1 || CodecIsExpressionRecord(Child(1)));
			case asEStmtKind::For:
			{
				const auto* Payload = Record.TryGetPayload<asSASTForStmtPayload>();
				if (!Payload) return false;
				const uint64 Expected = static_cast<uint64>(Payload->InitializerCount)
					+ (Payload->bHasCondition ? 1u : 0u) + Payload->IncrementCount + 1u;
				if (Expected != static_cast<uint64>(Record.Children.Num())) return false;
				int32 Index = 0;
				for (uint32 Initializer = 0; Initializer < Payload->InitializerCount; ++Initializer, ++Index)
					if (!CodecIsStmtRecord(Child(Index))) return false;
				if (Payload->bHasCondition && !CodecIsExpressionRecord(Child(Index++))) return false;
				for (uint32 Increment = 0; Increment < Payload->IncrementCount; ++Increment, ++Index)
					if (!CodecIsExpressionRecord(Child(Index))) return false;
				return CodecIsStmtRecord(Child(Index));
			}
			case asEStmtKind::Do:
				return Record.Children.Num() == 2 && CodecIsStmtRecord(Child(0)) && CodecIsExpressionRecord(Child(1));
			case asEStmtKind::CallExpr:
				return !Record.Children.IsEmpty() && AllChildrenAreExpressions();
			case asEStmtKind::ImplicitCastExpr:
			case asEStmtKind::ParenExpr:
			case asEStmtKind::UnaryOperator:
			case asEStmtKind::MemberExpr:
			case asEStmtKind::ExplicitCastExpr:
				return Record.Children.Num() == 1 && CodecIsExpressionRecord(Child(0));
			case asEStmtKind::BinaryOperator:
				return Record.Children.Num() == 2 && AllChildrenAreExpressions();
			case asEStmtKind::ConditionalOperator:
				return Record.Children.Num() == 3 && AllChildrenAreExpressions();
			case asEStmtKind::ArraySubscriptExpr:
				return Record.Children.Num() >= 2 && AllChildrenAreExpressions();
			case asEStmtKind::ConstructExpr:
			case asEStmtKind::InitListExpr:
				return AllChildrenAreExpressions();
			case asEStmtKind::DeclGroupStmt:
			{
				const auto* Payload = Record.TryGetPayload<asSASTDeclGroupStmtPayload>();
				if (!Payload || Payload->DeclarationCount != static_cast<uint32>(Record.Children.Num())) return false;
				for (uint32 Index : Record.Children)
				{
					const auto& Declaration = Records[static_cast<int32>(Index)];
					if (!CodecIsStmtKind(Declaration, asEStmtKind::DeclStmt)
						&& !CodecIsStmtKind(Declaration, asEStmtKind::RecoveryStmt)) return false;
				}
				return true;
			}
			case asEStmtKind::ClassDefaultStmt:
				return Record.Children.Num() == 1 && CodecIsStmtRecord(Child(0));
			case asEStmtKind::Switch:
			{
				const auto* Payload = Record.TryGetPayload<asSASTSwitchStmtPayload>();
				if (!Payload || static_cast<uint64>(Record.Children.Num())
						!= static_cast<uint64>(Payload->ClauseCount) + 1u
					|| !CodecIsExpressionRecord(Child(0))) return false;
				uint32 DefaultOrdinal = MAX_uint32;
				TSet<uint64> CaseValues;
				for (uint32 Ordinal = 0; Ordinal < Payload->ClauseCount; ++Ordinal)
				{
					const auto& Clause = Child(static_cast<int32>(Ordinal + 1u));
					if (CodecIsStmtKind(Clause, asEStmtKind::Case))
					{
						const auto* CasePayload = Clause.TryGetPayload<asSASTCaseStmtPayload>();
						if (!CasePayload || CaseValues.Contains(CasePayload->ResolvedValueBits)) return false;
						CaseValues.Add(CasePayload->ResolvedValueBits);
					}
					else if (CodecIsStmtKind(Clause, asEStmtKind::Default))
					{
						if (DefaultOrdinal != MAX_uint32) return false;
						DefaultOrdinal = Ordinal;
					}
					else return false;
				}
				return Payload->DefaultOrdinal == DefaultOrdinal;
			}
			case asEStmtKind::Case:
			{
				const auto* Payload = Record.TryGetPayload<asSASTCaseStmtPayload>();
				if (!Payload || static_cast<uint64>(Record.Children.Num())
						!= static_cast<uint64>(Payload->StatementCount) + 1u
					|| !CodecIsExpressionRecord(Child(0))) return false;
				for (int32 Index = 1; Index < Record.Children.Num(); ++Index)
				{
					const auto& Statement = Child(Index);
					if (!CodecIsStmtRecord(Statement)
						|| CodecIsStmtKind(Statement, asEStmtKind::DeclStmt)
						|| CodecIsStmtKind(Statement, asEStmtKind::DeclGroupStmt)
						|| (CodecIsStmtKind(Statement, asEStmtKind::Fallthrough)
							&& Index + 1 != Record.Children.Num())) return false;
				}
				return true;
			}
			case asEStmtKind::Default:
			{
				const auto* Payload = Record.TryGetPayload<asSASTDefaultStmtPayload>();
				if (!Payload || static_cast<uint64>(Record.Children.Num()) != Payload->StatementCount) return false;
				for (int32 Index = 0; Index < Record.Children.Num(); ++Index)
				{
					const auto& Statement = Child(Index);
					if (!CodecIsStmtRecord(Statement)
						|| CodecIsStmtKind(Statement, asEStmtKind::DeclStmt)
						|| CodecIsStmtKind(Statement, asEStmtKind::DeclGroupStmt)
						|| (CodecIsStmtKind(Statement, asEStmtKind::Fallthrough)
							&& Index + 1 != Record.Children.Num())) return false;
				}
				return true;
			}
			case asEStmtKind::Foreach:
			{
				const auto* Payload = Record.TryGetPayload<asSASTForeachStmtPayload>();
				const int32 Expected = Payload && Payload->bHasKey ? 4 : 3;
				if (!Payload || Record.Children.Num() != Expected
					|| !CodecIsStmtKind(Child(0), asEStmtKind::DeclStmt)) return false;
				int32 Index = 1;
				if (Payload->bHasKey && !CodecIsStmtKind(Child(Index++), asEStmtKind::DeclStmt)) return false;
				return CodecIsExpressionRecord(Child(Index++)) && CodecIsStmtRecord(Child(Index));
			}
			case asEStmtKind::IntegerLiteral:
			case asEStmtKind::DeclRefExpr:
			case asEStmtKind::RecoveryExpr:
			case asEStmtKind::BooleanLiteral:
			case asEStmtKind::FloatingLiteral:
			case asEStmtKind::StringLiteral:
			case asEStmtKind::NullPtrLiteral:
			case asEStmtKind::DefaultArgumentExpr:
			case asEStmtKind::ThisExpr:
			case asEStmtKind::ValueInitExpr:
				return Record.Children.IsEmpty();
			case asEStmtKind::UserConversionExpr:
				return Record.Children.Num() == 1 && CodecIsExpressionRecord(Child(0));
			default:
				return false;
			}
		}

		bool CodecIsAncestor(uint32 Ancestor, uint32 Node, TConstArrayView<uint32> Parents)
		{
			while (Node != MAX_uint32)
			{
				if (Node == Ancestor) return true;
				Node = Parents[static_cast<int32>(Node)];
			}
			return false;
		}

		bool CodecIsLocalTargetKind(
			const asSASTProjectedCrossReference& Reference,
			TConstArrayView<asSASTProjectionRecord> Records,
			uint32 NodeIndex,
			TConstArrayView<uint32> Parents)
		{
			if (Reference.LocalIndex == 0)
			{
				switch (Reference.Kind)
				{
				case asEASTCrossReferenceKind::ReturnTarget:
				case asEASTCrossReferenceKind::BreakTarget:
				case asEASTCrossReferenceKind::ContinueTarget:
				case asEASTCrossReferenceKind::SwitchParent:
				case asEASTCrossReferenceKind::FallthroughTarget:
					return false;
				default:
					return !Reference.StableIdentity.IsZero();
				}
			}

			const uint32 TargetIndex = Reference.LocalIndex - 1u;
			if (TargetIndex >= static_cast<uint32>(Records.Num())) return false;
			const asSASTProjectionRecord& Target = Records[static_cast<int32>(TargetIndex)];
			if (Reference.StableIdentity != Target.StableIdentity) return false;
			auto IsDeclKind = [&Target](asEDeclKind Kind)
			{
				return Target.Family == asEASTNodeFamily::Decl && Target.Kind == static_cast<uint16>(Kind);
			};
			auto IsControlKind = [&Target](bool bAllowSwitch)
			{
				if (Target.Family != asEASTNodeFamily::Stmt) return false;
				const asEStmtKind Kind = static_cast<asEStmtKind>(Target.Kind);
				return Kind == asEStmtKind::While || Kind == asEStmtKind::For
					|| Kind == asEStmtKind::Do || Kind == asEStmtKind::Foreach
					|| (bAllowSwitch && Kind == asEStmtKind::Switch);
			};

			switch (Reference.Kind)
			{
			case asEASTCrossReferenceKind::Declaration:
			case asEASTCrossReferenceKind::MemberDeclaration:
				return Target.Family == asEASTNodeFamily::Decl;
			case asEASTCrossReferenceKind::OperatorFunction:
			case asEASTCrossReferenceKind::ForeachBeginFunction:
			case asEASTCrossReferenceKind::ForeachEndFunction:
			case asEASTCrossReferenceKind::ForeachNextFunction:
			case asEASTCrossReferenceKind::ForeachValueFunction:
			case asEASTCrossReferenceKind::ForeachKeyFunction:
				return IsDeclKind(asEDeclKind::Function);
			case asEASTCrossReferenceKind::ReturnTarget:
				return IsDeclKind(asEDeclKind::Function)
					&& CodecIsAncestor(TargetIndex, NodeIndex, Parents);
			case asEASTCrossReferenceKind::Constructor:
			{
				const auto* Function = Target.TryGetPayload<asSASTFunctionDeclPayload>();
				return IsDeclKind(asEDeclKind::Function) && Function
					&& Function->FunctionKind == asEFunctionKind::Constructor;
			}
			case asEASTCrossReferenceKind::DefaultParameter:
				return IsDeclKind(asEDeclKind::ParmVar);
			case asEASTCrossReferenceKind::ThisRecord:
				return IsDeclKind(asEDeclKind::Record);
			case asEASTCrossReferenceKind::BreakTarget:
				return IsControlKind(true) && CodecIsAncestor(TargetIndex, NodeIndex, Parents);
			case asEASTCrossReferenceKind::ContinueTarget:
				return IsControlKind(false) && CodecIsAncestor(TargetIndex, NodeIndex, Parents);
			case asEASTCrossReferenceKind::SwitchParent:
				return CodecIsStmtKind(Target, asEStmtKind::Switch)
					&& Parents[static_cast<int32>(NodeIndex)] == TargetIndex;
			case asEASTCrossReferenceKind::FallthroughTarget:
				return CodecIsStmtKind(Target, asEStmtKind::Case)
					|| CodecIsStmtKind(Target, asEStmtKind::Default);
			case asEASTCrossReferenceKind::CustomAccessSpecifier:
				return IsDeclKind(asEDeclKind::AccessSpecifier);
			case asEASTCrossReferenceKind::ConversionFunction:
			{
				const auto* Function = Target.TryGetPayload<asSASTFunctionDeclPayload>();
				return IsDeclKind(asEDeclKind::Function) && Function
					&& Function->FunctionKind == asEFunctionKind::Conversion;
			}
			default:
				return false;
			}
		}

		bool CodecValidateCrossReferenceRoles(
			const asSASTProjectionRecord& Record,
			uint32 NodeIndex,
			TConstArrayView<asSASTProjectionRecord> Records,
			TConstArrayView<uint32> Parents)
		{
			for (const asSASTProjectedCrossReference& Reference : Record.CrossReferences)
				if (!CodecIsLocalTargetKind(Reference, Records, NodeIndex, Parents)) return false;

			auto IsOnly = [&Record](asEASTCrossReferenceKind Kind)
			{
				return Record.CrossReferences.Num() == 1 && Record.CrossReferences[0].Kind == Kind;
			};
			if (Record.Family == asEASTNodeFamily::Decl)
			{
				int32 Start = 0;
				if (Record.Declaration.AuthoredAccess.bPresent)
				{
					if ((Record.Kind != static_cast<uint16>(asEDeclKind::Var)
							&& Record.Kind != static_cast<uint16>(asEDeclKind::Function))
						|| Record.CrossReferences.IsEmpty()
						|| Record.CrossReferences[0].Kind != asEASTCrossReferenceKind::CustomAccessSpecifier)
					{
						return false;
					}
					Start = 1;
				}
				return Record.CrossReferences.Num() == Start;
			}
			if (Record.Family != asEASTNodeFamily::Stmt) return Record.CrossReferences.IsEmpty();

			switch (static_cast<asEStmtKind>(Record.Kind))
			{
			case asEStmtKind::DeclRefExpr: return IsOnly(asEASTCrossReferenceKind::Declaration);
			case asEStmtKind::Return:
				return Record.CrossReferences.IsEmpty() || IsOnly(asEASTCrossReferenceKind::ReturnTarget);
			case asEStmtKind::Break: return IsOnly(asEASTCrossReferenceKind::BreakTarget);
			case asEStmtKind::Continue: return IsOnly(asEASTCrossReferenceKind::ContinueTarget);
			case asEStmtKind::MemberExpr: return IsOnly(asEASTCrossReferenceKind::MemberDeclaration);
			case asEStmtKind::ArraySubscriptExpr:
				return Record.CrossReferences.IsEmpty() || IsOnly(asEASTCrossReferenceKind::OperatorFunction);
			case asEStmtKind::ConstructExpr:
				return Record.CrossReferences.IsEmpty() || IsOnly(asEASTCrossReferenceKind::Constructor);
			case asEStmtKind::DefaultArgumentExpr: return IsOnly(asEASTCrossReferenceKind::DefaultParameter);
			case asEStmtKind::ThisExpr: return IsOnly(asEASTCrossReferenceKind::ThisRecord);
			case asEStmtKind::UserConversionExpr: return IsOnly(asEASTCrossReferenceKind::ConversionFunction);
			case asEStmtKind::Case:
			case asEStmtKind::Default: return IsOnly(asEASTCrossReferenceKind::SwitchParent);
			case asEStmtKind::Fallthrough: return IsOnly(asEASTCrossReferenceKind::FallthroughTarget);
			case asEStmtKind::Foreach:
			{
				const auto* Payload = Record.TryGetPayload<asSASTForeachStmtPayload>();
				const int32 Expected = Payload && Payload->bHasKey ? 5 : 4;
				if (!Payload || Record.CrossReferences.Num() != Expected) return false;
				const asEASTCrossReferenceKind Roles[] = {
					asEASTCrossReferenceKind::ForeachBeginFunction,
					asEASTCrossReferenceKind::ForeachEndFunction,
					asEASTCrossReferenceKind::ForeachNextFunction,
					asEASTCrossReferenceKind::ForeachValueFunction,
					asEASTCrossReferenceKind::ForeachKeyFunction,
				};
				for (int32 Index = 0; Index < Expected; ++Index)
					if (Record.CrossReferences[Index].Kind != Roles[Index]) return false;
				return true;
			}
			default:
				return Record.CrossReferences.IsEmpty();
			}
		}

		bool CodecValidateSemanticReferenceRoles(const asSASTProjectionRecord& Record)
		{
			auto IsSingle = [&Record](asEASTSemanticTypeReferenceKind Kind)
			{
				return Record.SemanticReferences.Num() == 1
					&& Record.SemanticReferences[0].Kind == Kind
					&& Record.SemanticReferences[0].Ordinal == 0
					&& Record.SemanticReferences[0].PassingMode == asEParameterPassingMode::Value;
			};
			if (Record.Family == asEASTNodeFamily::Decl)
			{
				switch (static_cast<asEDeclKind>(Record.Kind))
				{
				case asEDeclKind::Record:
					for (int32 Index = 0; Index < Record.SemanticReferences.Num(); ++Index)
					{
						const auto& Reference = Record.SemanticReferences[Index];
						if (Reference.Kind != asEASTSemanticTypeReferenceKind::BaseType
							|| Reference.Ordinal != static_cast<uint32>(Index)
							|| Reference.PassingMode != asEParameterPassingMode::Value) return false;
					}
					return true;
				case asEDeclKind::Var:
				case asEDeclKind::ParmVar: return IsSingle(asEASTSemanticTypeReferenceKind::VariableType);
				case asEDeclKind::Function:
				case asEDeclKind::Delegate:
				case asEDeclKind::Event: return IsSingle(asEASTSemanticTypeReferenceKind::SignatureType);
				case asEDeclKind::TypeAlias: return IsSingle(asEASTSemanticTypeReferenceKind::AliasedType);
				default: return Record.SemanticReferences.IsEmpty();
				}
			}
			if (Record.Family == asEASTNodeFamily::Type)
			{
				switch (static_cast<asETypeKind>(Record.Kind))
				{
				case asETypeKind::Generic:
					if (Record.SemanticReferences.IsEmpty()
						|| Record.SemanticReferences[0].Kind != asEASTSemanticTypeReferenceKind::GenericTemplate
						|| Record.SemanticReferences[0].Ordinal != 0
						|| Record.SemanticReferences[0].PassingMode != asEParameterPassingMode::Value) return false;
					for (int32 Index = 1; Index < Record.SemanticReferences.Num(); ++Index)
					{
						const auto& Reference = Record.SemanticReferences[Index];
						if (Reference.Kind != asEASTSemanticTypeReferenceKind::GenericArgument
							|| Reference.Ordinal != static_cast<uint32>(Index - 1)
							|| Reference.PassingMode != asEParameterPassingMode::Value) return false;
					}
					return true;
				case asETypeKind::Function:
				{
					const auto* Payload = Record.TryGetPayload<asSASTFunctionTypePayload>();
					if (!Payload || Record.SemanticReferences.Num() != Payload->PassingModes.Num() + 1
						|| Record.SemanticReferences[0].Kind != asEASTSemanticTypeReferenceKind::FunctionReturn
						|| Record.SemanticReferences[0].Ordinal != 0
						|| Record.SemanticReferences[0].PassingMode != asEParameterPassingMode::Value) return false;
					for (int32 Index = 0; Index < Payload->PassingModes.Num(); ++Index)
					{
						const auto& Reference = Record.SemanticReferences[Index + 1];
						if (Reference.Kind != asEASTSemanticTypeReferenceKind::FunctionParameter
							|| Reference.Ordinal != static_cast<uint32>(Index)
							|| Reference.PassingMode != Payload->PassingModes[Index]) return false;
					}
					return true;
				}
				default: return Record.SemanticReferences.IsEmpty();
				}
			}
			if (Record.Family == asEASTNodeFamily::Stmt
				&& Record.Kind == static_cast<uint16>(asEStmtKind::Foreach))
			{
				return Record.SemanticReferences.Num() == 2
					&& Record.SemanticReferences[0].Kind == asEASTSemanticTypeReferenceKind::ForeachContainerType
					&& Record.SemanticReferences[0].Ordinal == 0
					&& Record.SemanticReferences[0].PassingMode == asEParameterPassingMode::Value
					&& Record.SemanticReferences[1].Kind == asEASTSemanticTypeReferenceKind::ForeachIteratorType
					&& Record.SemanticReferences[1].Ordinal == 0
					&& Record.SemanticReferences[1].PassingMode == asEParameterPassingMode::Value;
			}
			return Record.SemanticReferences.IsEmpty();
		}

		bool CodecValidateProjectionStructure(TConstArrayView<asSASTProjectionRecord> Records)
		{
			if (Records.IsEmpty()) return true;
			TArray<uint32> Parents;
			Parents.Init(MAX_uint32, Records.Num());
			for (int32 ParentIndex = 0; ParentIndex < Records.Num(); ++ParentIndex)
			{
				const asSASTProjectionRecord& Parent = Records[ParentIndex];
				uint32 PreviousChild = static_cast<uint32>(ParentIndex);
				for (uint32 ChildIndex : Parent.Children)
				{
					if (ChildIndex <= PreviousChild || ChildIndex >= static_cast<uint32>(Records.Num())
						|| Parents[static_cast<int32>(ChildIndex)] != MAX_uint32) return false;
					const asSASTProjectionRecord& Child = Records[static_cast<int32>(ChildIndex)];
					if (Parent.Family != asEASTNodeFamily::Type && Child.Family != asEASTNodeFamily::Type
						&& !CodecAnchorWithin(Child.Source, Parent.Source)) return false;
					Parents[static_cast<int32>(ChildIndex)] = static_cast<uint32>(ParentIndex);
					PreviousChild = ChildIndex;
				}
			}
			if (Parents[0] != MAX_uint32) return false;
			for (int32 Index = 1; Index < Parents.Num(); ++Index) if (Parents[Index] == MAX_uint32) return false;

			for (int32 Index = 0; Index < Records.Num(); ++Index)
			{
				const asSASTProjectionRecord& Record = Records[Index];
				if (Record.Family == asEASTNodeFamily::Stmt
					&& !CodecValidateStatementChildren(Record, Records)) return false;
				if (!CodecValidateSemanticReferenceRoles(Record)
					|| !CodecValidateCrossReferenceRoles(Record, static_cast<uint32>(Index), Records, Parents)) return false;
			}

			for (int32 Index = 0; Index < Records.Num(); ++Index)
			{
				const asSASTProjectionRecord& Record = Records[Index];
				if (!CodecIsStmtKind(Record, asEStmtKind::Fallthrough)) continue;
				const uint32 ClauseIndex = Parents[Index];
				if (ClauseIndex == MAX_uint32) return false;
				const asSASTProjectionRecord& Clause = Records[static_cast<int32>(ClauseIndex)];
				if ((!CodecIsStmtKind(Clause, asEStmtKind::Case) && !CodecIsStmtKind(Clause, asEStmtKind::Default))
					|| Clause.Children.IsEmpty() || Clause.Children.Last() != static_cast<uint32>(Index)) return false;
				const uint32 SwitchIndex = Parents[static_cast<int32>(ClauseIndex)];
				if (SwitchIndex == MAX_uint32) return false;
				const asSASTProjectionRecord& Switch = Records[static_cast<int32>(SwitchIndex)];
				if (!CodecIsStmtKind(Switch, asEStmtKind::Switch)
					|| Record.CrossReferences.Num() != 1) return false;
				const int32 ClauseOrdinal = Switch.Children.IndexOfByKey(ClauseIndex);
				if (ClauseOrdinal < 1 || ClauseOrdinal + 1 >= Switch.Children.Num()
					|| Record.CrossReferences[0].LocalIndex != Switch.Children[ClauseOrdinal + 1] + 1u) return false;
			}

			return true;
		}

		void CodecWriteTypeSyntax(asSASTCodecWriter& W, const asSASTProjectedTypeSyntax& S)
		{
			W.Bool(S.bPresent); if (!S.bPresent) return;
			W.String(S.Spelling); W.U32(static_cast<uint32>(S.NameParts.Num())); for (const FString& Part : S.NameParts) W.String(Part);
			CodecWriteAnchor(W, S.Source); W.U32(static_cast<uint32>(S.ComponentRanges.Num())); for (const auto& Range : S.ComponentRanges) CodecWriteAnchor(W, Range);
			W.U32(static_cast<uint32>(S.GenericArguments.Num()));
			for (const asSASTProjectedTypeSyntax& Argument : S.GenericArguments) CodecWriteTypeSyntax(W, Argument);
			W.U32(static_cast<uint32>(S.ArrayDimensions.Num()));
			for (const asSStableSourceAnchor& Dimension : S.ArrayDimensions) CodecWriteAnchor(W, Dimension);
			W.Bool(S.bGlobal); W.Bool(S.bConst); W.Bool(S.bHandle); W.U8(static_cast<uint8>(S.Reference));
		}

		void CodecWritePayload(asSASTCodecWriter& W, const asFASTProjectionPayload& Payload)
		{
			W.U8(static_cast<uint8>(CodecGetPayloadTag(Payload)));
			if (const auto* Named = Payload.TryGet<asSASTNamedDeclPayload>()) W.String(Named->Name);
			else if (const auto* Record = Payload.TryGet<asSASTRecordDeclPayload>()) { W.String(Record->Name); W.U8(static_cast<uint8>(Record->DeclaredTypeKind)); W.U32(static_cast<uint32>(Record->UnresolvedBases.Num())); for (const FString& Base : Record->UnresolvedBases) W.String(Base); W.U8(static_cast<uint8>(Record->Modifiers)); }
			else if (const auto* Variable = Payload.TryGet<asSASTVarDeclPayload>()) { W.String(Variable->Name); CodecWriteTypeSyntax(W, Variable->TypeSyntax); W.Bool(Variable->bHasDeferredInitializer); if (Variable->bHasDeferredInitializer) CodecWriteAnchor(W, Variable->DeferredInitializerSource); }
			else if (const auto* Constant = Payload.TryGet<asSASTEnumConstantDeclPayload>()) { W.String(Constant->Name); W.U64(static_cast<uint64>(Constant->Value)); W.Bool(Constant->bExplicitValue); W.Bool(Constant->bResolvedValue); W.Bool(Constant->bHasDeferredInitializer); if (Constant->bHasDeferredInitializer) CodecWriteAnchor(W, Constant->DeferredInitializerSource); }
			else if (const auto* Function = Payload.TryGet<asSASTFunctionDeclPayload>()) { W.String(Function->Name); CodecWriteTypeSyntax(W, Function->ReturnTypeSyntax); W.Bool(Function->bConstMethod); W.U8(static_cast<uint8>(Function->FunctionKind)); W.U16(static_cast<uint16>(Function->Modifiers)); W.Bool(Function->bHasDeferredBody); if (Function->bHasDeferredBody) CodecWriteAnchor(W, Function->DeferredBodySource); W.U32(static_cast<uint32>(Function->DeferredStatementSources.Num())); for (const asSStableSourceAnchor& Source : Function->DeferredStatementSources) CodecWriteAnchor(W, Source); }
			else if (const auto* Callable = Payload.TryGet<asSASTCallableTypeDeclPayload>()) { W.String(Callable->Name); CodecWriteTypeSyntax(W, Callable->ReturnTypeSyntax); W.Bool(Callable->bMulticast); }
			else if (const auto* Access = Payload.TryGet<asSASTAccessSpecifierDeclPayload>())
			{
				W.String(Access->Name); W.U8(static_cast<uint8>(Access->BaseAccess));
				CodecWriteAnchor(W, Access->KeywordSource); CodecWriteAnchor(W, Access->NameSource);
				CodecWriteAnchor(W, Access->EqualsSource); CodecWriteAnchor(W, Access->BaseAccessSource);
				CodecWriteAnchor(W, Access->SemicolonSource);
				W.U32(static_cast<uint32>(Access->Permissions.Num()));
				for (const asSASTProjectedAccessPermission& Permission : Access->Permissions)
				{
					W.U8(static_cast<uint8>(Permission.Subject)); W.String(Permission.Name);
					CodecWriteAnchor(W, Permission.SubjectSource); CodecWriteAnchor(W, Permission.Source);
					W.U32(static_cast<uint32>(Permission.Modifiers.Num()));
					for (const asSASTProjectedAccessPermissionModifier& Modifier : Permission.Modifiers)
					{
						W.U8(static_cast<uint8>(Modifier.Kind)); CodecWriteAnchor(W, Modifier.Source);
					}
				}
			}
			else if (const auto* Integer = Payload.TryGet<asSASTIntegerLiteralPayload>()) W.U64(static_cast<uint64>(Integer->Value));
			else if (const auto* Boolean = Payload.TryGet<asSASTBooleanLiteralPayload>()) W.Bool(Boolean->Value);
			else if (const auto* Floating = Payload.TryGet<asSASTFloatingLiteralPayload>()) W.U64(Floating->IEEE754Bits);
			else if (const auto* Cast = Payload.TryGet<asSASTImplicitCastPayload>()) W.U8(static_cast<uint8>(Cast->Conversion));
			else if (const auto* Conversion = Payload.TryGet<asSASTUserConversionPayload>()) W.U8(static_cast<uint8>(Conversion->Mode));
			else if (const auto* Unary = Payload.TryGet<asSASTUnaryOperatorPayload>()) { W.U8(static_cast<uint8>(Unary->Operator)); CodecWriteAnchor(W, Unary->OperatorSource); }
			else if (const auto* Binary = Payload.TryGet<asSASTBinaryOperatorPayload>()) { W.U8(static_cast<uint8>(Binary->Operator)); CodecWriteAnchor(W, Binary->OperatorSource); }
			else if (const auto* Builtin = Payload.TryGet<asSASTBuiltinTypePayload>()) W.U8(static_cast<uint8>(Builtin->Primitive));
			else if (const auto* Nominal = Payload.TryGet<asSASTNominalTypePayload>()) W.Key(Nominal->DeclarationIdentity);
			else if (const auto* FunctionType = Payload.TryGet<asSASTFunctionTypePayload>()) { W.U32(static_cast<uint32>(FunctionType->PassingModes.Num())); for (auto M : FunctionType->PassingModes) W.U8(static_cast<uint8>(M)); }
			else if (const auto* Flag = Payload.TryGet<asSASTFlagAttrPayload>()) { W.String(Flag->Name); W.U8(static_cast<uint8>(Flag->AllowedTargets)); }
			else if (const auto* String = Payload.TryGet<asSASTStringAttrPayload>()) { W.String(String->Name); W.String(String->Value); W.U8(static_cast<uint8>(String->AllowedTargets)); }
			else if (const auto* Annotation = Payload.TryGet<asSASTAnnotationAttrPayload>())
			{
				W.U8(static_cast<uint8>(Annotation->AnnotationKind)); W.U8(static_cast<uint8>(Annotation->AllowedTargets)); W.U32(static_cast<uint32>(Annotation->Arguments.Num()));
				for (const auto& Argument : Annotation->Arguments) { W.U32(Argument.ParentIndex); W.String(Argument.Name); W.U8(static_cast<uint8>(Argument.Kind)); W.String(Argument.Value); CodecWriteAnchor(W, Argument.Source); CodecWriteAnchor(W, Argument.ValueSource); }
			}
			else if (const auto* Alias = Payload.TryGet<asSASTTypeAliasDeclPayload>()) { W.String(Alias->Name); CodecWriteTypeSyntax(W, Alias->TypeSyntax); }
			else if (const auto* Call = Payload.TryGet<asSASTCallExprPayload>()) { W.U32(static_cast<uint32>(Call->ArgumentSourceOrdinals.Num())); for (int32 Ordinal : Call->ArgumentSourceOrdinals) W.U32(static_cast<uint32>(Ordinal)); }
			else if (const auto* Member = Payload.TryGet<asSASTMemberExprPayload>()) CodecWriteAnchor(W, Member->MemberSource);
			else if (const auto* ExplicitCast = Payload.TryGet<asSASTExplicitCastPayload>()) CodecWriteTypeSyntax(W, ExplicitCast->TypeSyntax);
			else if (const auto* Construct = Payload.TryGet<asSASTConstructExprPayload>())
			{
				CodecWriteTypeSyntax(W, Construct->TypeSyntax);
				W.U32(static_cast<uint32>(Construct->ArgumentSourceOrdinals.Num()));
				for (int32 Ordinal : Construct->ArgumentSourceOrdinals) W.U32(static_cast<uint32>(Ordinal));
			}
			else if (const auto* StringLiteral = Payload.TryGet<asSASTStringLiteralPayload>()) W.String(StringLiteral->Value);
			else if (const auto* This = Payload.TryGet<asSASTThisExprPayload>()) W.Bool(This->bImplicit);
			else if (const auto* Initializer = Payload.TryGet<asSASTInitListExprPayload>()) { W.Key(Initializer->InitializerIdentity); W.U32(static_cast<uint32>(Initializer->ElementPatternIndices.Num())); for (int32 Index : Initializer->ElementPatternIndices) W.U32(static_cast<uint32>(Index)); }
			else if (const auto* Group = Payload.TryGet<asSASTDeclGroupStmtPayload>()) W.U32(Group->DeclarationCount);
			else if (const auto* ClassDefault = Payload.TryGet<asSASTClassDefaultStmtPayload>()) CodecWriteAnchor(W, ClassDefault->KeywordSource);
			else if (const auto* For = Payload.TryGet<asSASTForStmtPayload>()) { W.U32(For->InitializerCount); W.Bool(For->bHasCondition); W.U32(For->IncrementCount); CodecWriteAnchor(W, For->HeaderSource); }
			else if (const auto* Switch = Payload.TryGet<asSASTSwitchStmtPayload>()) { W.U32(Switch->ClauseCount); W.U32(Switch->DefaultOrdinal); CodecWriteAnchor(W, Switch->HeaderSource); }
			else if (const auto* Case = Payload.TryGet<asSASTCaseStmtPayload>()) { W.U32(Case->StatementCount); W.U64(Case->ResolvedValueBits); CodecWriteAnchor(W, Case->KeywordSource); CodecWriteAnchor(W, Case->ColonSource); }
			else if (const auto* Default = Payload.TryGet<asSASTDefaultStmtPayload>()) { W.U32(Default->StatementCount); CodecWriteAnchor(W, Default->KeywordSource); CodecWriteAnchor(W, Default->ColonSource); }
			else if (const auto* Foreach = Payload.TryGet<asSASTForeachStmtPayload>()) { W.Bool(Foreach->bHasKey); CodecWriteAnchor(W, Foreach->HeaderSource); CodecWriteAnchor(W, Foreach->ColonSource); }
		}

		asEASTCodecStatus CodecReadCount(asSASTCodecReader& R, uint32& Count, uint64& Items, const asSASTCodecBudgets& B)
		{
			if (!R.U32(Count)) return asEASTCodecStatus::Malformed;
			Items += Count;
			if (Items > B.MaxEdges) return asEASTCodecStatus::BudgetExceeded;
			// Every bounded collection item occupies at least one following byte.
			// Prove that Reserve cannot be driven beyond the remaining wire image.
			if (Count > static_cast<uint32>(R.Remaining())) return asEASTCodecStatus::Malformed;
			return asEASTCodecStatus::Succeeded;
		}

		asEASTCodecStatus CodecReadTypeSyntax(
			asSASTCodecReader& R,
			asSASTProjectedTypeSyntax& S,
			uint64& Items,
			const asSASTCodecBudgets& B,
			uint32 Depth = 1)
		{
			if (!R.Bool(S.bPresent)) return asEASTCodecStatus::Malformed;
			if (!S.bPresent) return asEASTCodecStatus::Succeeded;
			if (Depth > B.MaxTypeSyntaxDepth || Depth > CodecMaxTypeSyntaxDepth)
				return asEASTCodecStatus::BudgetExceeded;
			uint32 Count = 0; if (!R.String(S.Spelling)) return asEASTCodecStatus::Malformed;
			auto Status = CodecReadCount(R, Count, Items, B); if (Status != asEASTCodecStatus::Succeeded) return Status;
			S.NameParts.Reserve(Count); for (uint32 I = 0; I < Count; ++I) { FString Part; if (!R.String(Part)) return asEASTCodecStatus::Malformed; S.NameParts.Add(MoveTemp(Part)); }
			if (!CodecReadAnchor(R, S.Source)) return asEASTCodecStatus::Malformed;
			Status = CodecReadCount(R, Count, Items, B); if (Status != asEASTCodecStatus::Succeeded) return Status;
			S.ComponentRanges.Reserve(Count); for (uint32 I = 0; I < Count; ++I) { asSStableSourceAnchor A; if (!CodecReadAnchor(R, A)) return asEASTCodecStatus::Malformed; S.ComponentRanges.Add(MoveTemp(A)); }
			Status = CodecReadCount(R, Count, Items, B); if (Status != asEASTCodecStatus::Succeeded) return Status;
			S.GenericArguments.Reserve(Count);
			for (uint32 I = 0; I < Count; ++I)
			{
				asSASTProjectedTypeSyntax Argument;
				Status = CodecReadTypeSyntax(R, Argument, Items, B, Depth + 1);
				if (Status != asEASTCodecStatus::Succeeded) return Status;
				S.GenericArguments.Add(MoveTemp(Argument));
			}
			Status = CodecReadCount(R, Count, Items, B); if (Status != asEASTCodecStatus::Succeeded) return Status;
			S.ArrayDimensions.Reserve(Count);
			for (uint32 I = 0; I < Count; ++I)
			{
				asSStableSourceAnchor Dimension;
				if (!CodecReadAnchor(R, Dimension)) return asEASTCodecStatus::Malformed;
				S.ArrayDimensions.Add(MoveTemp(Dimension));
			}
			uint8 Ref = 0; if (!R.Bool(S.bGlobal) || !R.Bool(S.bConst) || !R.Bool(S.bHandle) || !R.U8(Ref)) return asEASTCodecStatus::Malformed;
			S.Reference = static_cast<asETypeReferenceDirection>(Ref);
			return CodecIsValidTypeSyntax(S) ? asEASTCodecStatus::Succeeded : asEASTCodecStatus::InvalidPayload;
		}

		template <typename T>
		void CodecSetPayload(asFASTProjectionPayload& Out, T Value)
		{
			Out.Set<T>(MoveTemp(Value));
		}

		asEASTCodecStatus CodecReadPayload(asSASTCodecReader& R, asFASTProjectionPayload& Out, uint64& Items, const asSASTCodecBudgets& B)
		{
			uint8 RawTag = 0; if (!R.U8(RawTag) || RawTag > static_cast<uint8>(asEASTCodecPayloadTag::UserConversion)) return asEASTCodecStatus::InvalidPayload;
			switch (static_cast<asEASTCodecPayloadTag>(RawTag))
			{
			case asEASTCodecPayloadTag::Empty: CodecSetPayload(Out, asSASTEmptyPayload{}); break;
			case asEASTCodecPayloadTag::NamedDecl: { asSASTNamedDeclPayload P; if (!R.String(P.Name)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::RecordDecl:
			{
				asSASTRecordDeclPayload P; uint8 K = 0; uint32 N = 0; if (!R.String(P.Name) || !R.U8(K)) return asEASTCodecStatus::Malformed; P.DeclaredTypeKind = static_cast<asETypeDeclKind>(K);
				auto S = CodecReadCount(R, N, Items, B); if (S != asEASTCodecStatus::Succeeded) return S; P.UnresolvedBases.Reserve(N);
				for (uint32 I = 0; I < N; ++I) { FString Base; if (!R.String(Base)) return asEASTCodecStatus::Malformed; P.UnresolvedBases.Add(MoveTemp(Base)); }
				uint8 Modifiers = 0; if (!R.U8(Modifiers)) return asEASTCodecStatus::Malformed; P.Modifiers = static_cast<asERecordModifiers>(Modifiers); CodecSetPayload(Out, MoveTemp(P)); break;
			}
			case asEASTCodecPayloadTag::VarDecl: { asSASTVarDeclPayload P; if (!R.String(P.Name)) return asEASTCodecStatus::Malformed; auto S = CodecReadTypeSyntax(R, P.TypeSyntax, Items, B); if (S != asEASTCodecStatus::Succeeded) return S; if (!R.Bool(P.bHasDeferredInitializer) || (P.bHasDeferredInitializer && !CodecReadAnchor(R, P.DeferredInitializerSource))) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::EnumConstantDecl: { asSASTEnumConstantDeclPayload P; uint64 V = 0; if (!R.String(P.Name) || !R.U64(V) || !R.Bool(P.bExplicitValue) || !R.Bool(P.bResolvedValue) || !R.Bool(P.bHasDeferredInitializer) || (P.bHasDeferredInitializer && !CodecReadAnchor(R, P.DeferredInitializerSource))) return asEASTCodecStatus::Malformed; P.Value = static_cast<int64>(V); CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::FunctionDecl:
			{
				asSASTFunctionDeclPayload P; if (!R.String(P.Name)) return asEASTCodecStatus::Malformed; auto S = CodecReadTypeSyntax(R, P.ReturnTypeSyntax, Items, B); if (S != asEASTCodecStatus::Succeeded) return S;
				uint8 FunctionKind = 0; uint16 Modifiers = 0;
				if (!R.Bool(P.bConstMethod) || !R.U8(FunctionKind) || !R.U16(Modifiers)
					|| !R.Bool(P.bHasDeferredBody) || (P.bHasDeferredBody && !CodecReadAnchor(R, P.DeferredBodySource))) return asEASTCodecStatus::Malformed;
				uint32 DeferredCount = 0; auto DeferredStatus = CodecReadCount(R, DeferredCount, Items, B);
				if (DeferredStatus != asEASTCodecStatus::Succeeded) return DeferredStatus;
				P.DeferredStatementSources.Reserve(DeferredCount);
				for (uint32 I = 0; I < DeferredCount; ++I) { asSStableSourceAnchor Source; if (!CodecReadAnchor(R, Source)) return asEASTCodecStatus::Malformed; P.DeferredStatementSources.Add(MoveTemp(Source)); }
				P.FunctionKind = static_cast<asEFunctionKind>(FunctionKind); P.Modifiers = static_cast<asEFunctionModifiers>(Modifiers); CodecSetPayload(Out, MoveTemp(P)); break;
			}
			case asEASTCodecPayloadTag::CallableTypeDecl: { asSASTCallableTypeDeclPayload P; if (!R.String(P.Name)) return asEASTCodecStatus::Malformed; auto S = CodecReadTypeSyntax(R, P.ReturnTypeSyntax, Items, B); if (S != asEASTCodecStatus::Succeeded) return S; if (!R.Bool(P.bMulticast)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::AccessSpecifierDecl:
			{
				asSASTAccessSpecifierDeclPayload P; uint8 Base = 0;
				if (!R.String(P.Name) || !R.U8(Base)
					|| !CodecReadAnchor(R, P.KeywordSource) || !CodecReadAnchor(R, P.NameSource)
					|| !CodecReadAnchor(R, P.EqualsSource) || !CodecReadAnchor(R, P.BaseAccessSource)
					|| !CodecReadAnchor(R, P.SemicolonSource)) return asEASTCodecStatus::Malformed;
				P.BaseAccess = static_cast<asEAccessSpecifier>(Base);
				uint32 PermissionCount = 0; auto PermissionStatus = CodecReadCount(R, PermissionCount, Items, B);
				if (PermissionStatus != asEASTCodecStatus::Succeeded) return PermissionStatus;
				P.Permissions.Reserve(PermissionCount);
				for (uint32 I = 0; I < PermissionCount; ++I)
				{
					asSASTProjectedAccessPermission Permission; uint8 Subject = 0;
					if (!R.U8(Subject) || !R.String(Permission.Name)
						|| !CodecReadAnchor(R, Permission.SubjectSource)
						|| !CodecReadAnchor(R, Permission.Source)) return asEASTCodecStatus::Malformed;
					Permission.Subject = static_cast<asEAccessPermissionSubject>(Subject);
					uint32 ModifierCount = 0; auto ModifierStatus = CodecReadCount(R, ModifierCount, Items, B);
					if (ModifierStatus != asEASTCodecStatus::Succeeded) return ModifierStatus;
					Permission.Modifiers.Reserve(ModifierCount);
					for (uint32 M = 0; M < ModifierCount; ++M)
					{
						asSASTProjectedAccessPermissionModifier Modifier; uint8 Kind = 0;
						if (!R.U8(Kind) || !CodecReadAnchor(R, Modifier.Source)) return asEASTCodecStatus::Malformed;
						Modifier.Kind = static_cast<asEAccessPermissionModifier>(Kind);
						Permission.Modifiers.Add(MoveTemp(Modifier));
					}
					P.Permissions.Add(MoveTemp(Permission));
				}
				CodecSetPayload(Out, MoveTemp(P)); break;
			}
			case asEASTCodecPayloadTag::IntegerLiteral: { asSASTIntegerLiteralPayload P; uint64 V = 0; if (!R.U64(V)) return asEASTCodecStatus::Malformed; P.Value = static_cast<int64>(V); CodecSetPayload(Out, P); break; }
			case asEASTCodecPayloadTag::BooleanLiteral: { asSASTBooleanLiteralPayload P; if (!R.Bool(P.Value)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, P); break; }
			case asEASTCodecPayloadTag::FloatingLiteral: { asSASTFloatingLiteralPayload P; if (!R.U64(P.IEEE754Bits)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, P); break; }
			case asEASTCodecPayloadTag::ImplicitCast: { uint8 V = 0; if (!R.U8(V)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, asSASTImplicitCastPayload{static_cast<asEImplicitConversionKind>(V)}); break; }
			case asEASTCodecPayloadTag::UserConversion: { uint8 V = 0; if (!R.U8(V)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, asSASTUserConversionPayload{static_cast<asEUserConversionMode>(V)}); break; }
			case asEASTCodecPayloadTag::UnaryOperator: { asSASTUnaryOperatorPayload P; uint8 V = 0; if (!R.U8(V) || !CodecReadAnchor(R, P.OperatorSource)) return asEASTCodecStatus::Malformed; P.Operator = static_cast<asEUnaryOperator>(V); CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::BinaryOperator: { asSASTBinaryOperatorPayload P; uint8 V = 0; if (!R.U8(V) || !CodecReadAnchor(R, P.OperatorSource)) return asEASTCodecStatus::Malformed; P.Operator = static_cast<asEBinaryOperator>(V); CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::BuiltinType: { uint8 V = 0; if (!R.U8(V)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, asSASTBuiltinTypePayload{static_cast<asEPrimitiveType>(V)}); break; }
			case asEASTCodecPayloadTag::NominalType: { asSASTNominalTypePayload P; if (!R.Key(P.DeclarationIdentity)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::FunctionType:
			{
				asSASTFunctionTypePayload P; uint32 N = 0; auto S = CodecReadCount(R, N, Items, B); if (S != asEASTCodecStatus::Succeeded) return S; P.PassingModes.Reserve(N);
				for (uint32 I = 0; I < N; ++I) { uint8 V = 0; if (!R.U8(V)) return asEASTCodecStatus::Malformed; P.PassingModes.Add(static_cast<asEParameterPassingMode>(V)); } CodecSetPayload(Out, MoveTemp(P)); break;
			}
			case asEASTCodecPayloadTag::FlagAttr: { asSASTFlagAttrPayload P; uint8 V = 0; if (!R.String(P.Name) || !R.U8(V)) return asEASTCodecStatus::Malformed; P.AllowedTargets = static_cast<asEAttrTarget>(V); CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::StringAttr: { asSASTStringAttrPayload P; uint8 V = 0; if (!R.String(P.Name) || !R.String(P.Value) || !R.U8(V)) return asEASTCodecStatus::Malformed; P.AllowedTargets = static_cast<asEAttrTarget>(V); CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::AnnotationAttr:
			{
				asSASTAnnotationAttrPayload P; uint8 K = 0, T = 0; uint32 N = 0; if (!R.U8(K) || !R.U8(T)) return asEASTCodecStatus::Malformed; P.AnnotationKind = static_cast<asEAnnotationKind>(K); P.AllowedTargets = static_cast<asEAttrTarget>(T);
				auto S = CodecReadCount(R, N, Items, B); if (S != asEASTCodecStatus::Succeeded) return S; P.Arguments.Reserve(N);
				for (uint32 I = 0; I < N; ++I) { asSASTProjectedAnnotationArgument A; uint8 V = 0; if (!R.U32(A.ParentIndex) || !R.String(A.Name) || !R.U8(V) || !R.String(A.Value) || !CodecReadAnchor(R, A.Source) || !CodecReadAnchor(R, A.ValueSource)) return asEASTCodecStatus::Malformed; A.Kind = static_cast<asEAnnotationValueKind>(V); P.Arguments.Add(MoveTemp(A)); }
				CodecSetPayload(Out, MoveTemp(P)); break;
			}
			case asEASTCodecPayloadTag::TypeAliasDecl: { asSASTTypeAliasDeclPayload P; if (!R.String(P.Name)) return asEASTCodecStatus::Malformed; auto S = CodecReadTypeSyntax(R, P.TypeSyntax, Items, B); if (S != asEASTCodecStatus::Succeeded) return S; CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::CallExpr:
			{
				asSASTCallExprPayload P; uint32 N = 0; auto S = CodecReadCount(R, N, Items, B); if (S != asEASTCodecStatus::Succeeded) return S; P.ArgumentSourceOrdinals.Reserve(N);
				for (uint32 I = 0; I < N; ++I) { uint32 Encoded = 0; if (!R.U32(Encoded)) return asEASTCodecStatus::Malformed; P.ArgumentSourceOrdinals.Add(static_cast<int32>(Encoded)); }
				CodecSetPayload(Out, MoveTemp(P)); break;
			}
			case asEASTCodecPayloadTag::MemberExpr: { asSASTMemberExprPayload P; if (!CodecReadAnchor(R, P.MemberSource)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::ExplicitCast: { asSASTExplicitCastPayload P; auto S = CodecReadTypeSyntax(R, P.TypeSyntax, Items, B); if (S != asEASTCodecStatus::Succeeded) return S; CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::ConstructExpr:
			{
				asSASTConstructExprPayload P;
				auto S = CodecReadTypeSyntax(R, P.TypeSyntax, Items, B);
				if (S != asEASTCodecStatus::Succeeded) return S;
				uint32 N = 0;
				S = CodecReadCount(R, N, Items, B);
				if (S != asEASTCodecStatus::Succeeded) return S;
				P.ArgumentSourceOrdinals.Reserve(N);
				for (uint32 I = 0; I < N; ++I)
				{
					uint32 Encoded = 0;
					if (!R.U32(Encoded)) return asEASTCodecStatus::Malformed;
					P.ArgumentSourceOrdinals.Add(static_cast<int32>(Encoded));
				}
				CodecSetPayload(Out, MoveTemp(P));
				break;
			}
			case asEASTCodecPayloadTag::StringLiteral: { asSASTStringLiteralPayload P; if (!R.String(P.Value)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::ThisExpr: { asSASTThisExprPayload P; if (!R.Bool(P.bImplicit)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, P); break; }
			case asEASTCodecPayloadTag::InitListExpr:
			{
				asSASTInitListExprPayload P; uint32 N = 0; if (!R.Key(P.InitializerIdentity)) return asEASTCodecStatus::Malformed;
				auto S = CodecReadCount(R, N, Items, B); if (S != asEASTCodecStatus::Succeeded) return S; P.ElementPatternIndices.Reserve(N);
				for (uint32 I = 0; I < N; ++I) { uint32 V = 0; if (!R.U32(V)) return asEASTCodecStatus::Malformed; P.ElementPatternIndices.Add(static_cast<int32>(V)); }
				CodecSetPayload(Out, MoveTemp(P)); break;
			}
			case asEASTCodecPayloadTag::ValueInitExpr: CodecSetPayload(Out, asSASTValueInitExprPayload{}); break;
			case asEASTCodecPayloadTag::DeclGroupStmt: { asSASTDeclGroupStmtPayload P; if (!R.U32(P.DeclarationCount)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, P); break; }
			case asEASTCodecPayloadTag::ClassDefaultStmt: { asSASTClassDefaultStmtPayload P; if (!CodecReadAnchor(R, P.KeywordSource)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::ForStmt: { asSASTForStmtPayload P; if (!R.U32(P.InitializerCount) || !R.Bool(P.bHasCondition) || !R.U32(P.IncrementCount) || !CodecReadAnchor(R, P.HeaderSource)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::SwitchStmt: { asSASTSwitchStmtPayload P; if (!R.U32(P.ClauseCount) || !R.U32(P.DefaultOrdinal) || !CodecReadAnchor(R, P.HeaderSource)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::CaseStmt: { asSASTCaseStmtPayload P; if (!R.U32(P.StatementCount) || !R.U64(P.ResolvedValueBits) || !CodecReadAnchor(R, P.KeywordSource) || !CodecReadAnchor(R, P.ColonSource)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::DefaultStmt: { asSASTDefaultStmtPayload P; if (!R.U32(P.StatementCount) || !CodecReadAnchor(R, P.KeywordSource) || !CodecReadAnchor(R, P.ColonSource)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, MoveTemp(P)); break; }
			case asEASTCodecPayloadTag::FallthroughStmt: CodecSetPayload(Out, asSASTFallthroughStmtPayload{}); break;
			case asEASTCodecPayloadTag::ForeachStmt: { asSASTForeachStmtPayload P; if (!R.Bool(P.bHasKey) || !CodecReadAnchor(R, P.HeaderSource) || !CodecReadAnchor(R, P.ColonSource)) return asEASTCodecStatus::Malformed; CodecSetPayload(Out, MoveTemp(P)); break; }
			}
			return asEASTCodecStatus::Succeeded;
		}
	}

	asEASTCodecStatus asCASTCodec::Encode(const asCASTProjection& Projection, TArray<uint8>& OutBytes)
	{
		OutBytes.Reset();
		const uint32 Count = static_cast<uint32>(Projection.Records.Num());
		for (const auto& Record : Projection.Records) if (!CodecValidateRecord(Record, Count)) return asEASTCodecStatus::InvalidPayload;
		if (!CodecValidateProjectionStructure(Projection.Records)) return asEASTCodecStatus::InvalidPayload;
		asSASTCodecWriter W; W.U8('A'); W.U8('S'); W.U8('T'); W.U8('C'); W.U16(CurrentVersion); W.U32(Count);
		for (const auto& Record : Projection.Records)
		{
			W.U8(static_cast<uint8>(Record.Family)); W.U16(Record.Kind); CodecWriteAnchor(W, Record.Source);
			W.U32(static_cast<uint32>(Record.Children.Num())); for (uint32 Child : Record.Children) W.U32(Child);
			W.U32(static_cast<uint32>(Record.CrossReferences.Num()));
			for (const auto& Ref : Record.CrossReferences) { W.U8(static_cast<uint8>(Ref.Kind)); W.U32(Ref.LocalIndex); W.Key(Ref.StableIdentity); }
			W.U32(static_cast<uint32>(Record.SemanticReferences.Num()));
			for (const auto& Ref : Record.SemanticReferences) { W.U8(static_cast<uint8>(Ref.Kind)); W.U32(Ref.Ordinal); W.U8(static_cast<uint8>(Ref.PassingMode)); W.Key(Ref.TypeIdentity); }
			W.Bool(Record.Declaration.bPresent);
			if (Record.Declaration.bPresent)
			{
				W.U8(static_cast<uint8>(Record.Declaration.Access));
				W.Bool(Record.Declaration.AuthoredAccess.bPresent);
				if (Record.Declaration.AuthoredAccess.bPresent)
				{
					W.String(Record.Declaration.AuthoredAccess.Name);
					CodecWriteAnchor(W, Record.Declaration.AuthoredAccess.KeywordSource);
					CodecWriteAnchor(W, Record.Declaration.AuthoredAccess.ColonSource);
					CodecWriteAnchor(W, Record.Declaration.AuthoredAccess.NameSource);
				}
			}
			W.Bool(Record.Expression.bPresent);
			if (Record.Expression.bPresent) { W.Key(Record.Expression.TypeIdentity); W.U8(static_cast<uint8>(Record.Expression.ValueCategory)); W.Bool(Record.Expression.bReadOnly); }
			CodecWritePayload(W, Record.Payload); W.Key(Record.StableIdentity);
		}
		OutBytes = MoveTemp(W.Bytes); return asEASTCodecStatus::Succeeded;
	}

	asEASTCodecStatus asCASTCodec::Decode(TConstArrayView<uint8> Bytes, asCASTProjection& Out, const asSASTCodecBudgets& B)
	{
		Out.Records.Reset();
		if (static_cast<uint64>(Bytes.Num()) > B.MaxBytes) return asEASTCodecStatus::BudgetExceeded;
		if (Bytes.Num() < 10) return asEASTCodecStatus::Malformed;
		asSASTCodecReader R(Bytes); uint8 Magic[4] = {}; for (uint8& V : Magic) if (!R.U8(V)) return asEASTCodecStatus::Malformed;
		if (Magic[0] != 'A' || Magic[1] != 'S' || Magic[2] != 'T' || Magic[3] != 'C') return asEASTCodecStatus::Malformed;
		uint16 Version = 0; if (!R.U16(Version)) return asEASTCodecStatus::Malformed;
		if (Version >= 1 && Version < CurrentVersion) return asEASTCodecStatus::IncompatibleVersion;
		if (Version != CurrentVersion) return asEASTCodecStatus::UnsupportedVersion;
		uint32 Count = 0; if (!R.U32(Count)) return asEASTCodecStatus::Malformed;
		if (Count > B.MaxNodes) return asEASTCodecStatus::BudgetExceeded;
		if (Count > static_cast<uint32>(R.Remaining() / 70)) return asEASTCodecStatus::Malformed;
		TArray<asSASTProjectionRecord> Temp; Temp.Reserve(Count); uint64 Items = 0;
		for (uint32 NodeIndex = 0; NodeIndex < Count; ++NodeIndex)
		{
			asSASTProjectionRecord P; uint8 Family = 0;
			if (!R.U8(Family) || !R.U16(P.Kind) || !CodecReadAnchor(R, P.Source)) return asEASTCodecStatus::Malformed;
			P.Family = static_cast<asEASTNodeFamily>(Family); if (!CodecIsKnownKind(P.Family, P.Kind)) return asEASTCodecStatus::Malformed;
			if ((P.Family == asEASTNodeFamily::Type && !CodecIsEmptyAnchor(P.Source))
				|| (P.Family != asEASTNodeFamily::Type && !P.Source.IsValid())) return asEASTCodecStatus::Malformed;
			uint32 N = 0; auto Status = CodecReadCount(R, N, Items, B); if (Status != asEASTCodecStatus::Succeeded) return Status; P.Children.Reserve(N);
			for (uint32 I = 0; I < N; ++I) { uint32 Child = 0; if (!R.U32(Child)) return asEASTCodecStatus::Malformed; if (Child >= Count) return asEASTCodecStatus::InvalidIndex; P.Children.Add(Child); }
			Status = CodecReadCount(R, N, Items, B); if (Status != asEASTCodecStatus::Succeeded) return Status; P.CrossReferences.Reserve(N);
			for (uint32 I = 0; I < N; ++I) { asSASTProjectedCrossReference Ref; uint8 K = 0; if (!R.U8(K) || !R.U32(Ref.LocalIndex) || !R.Key(Ref.StableIdentity)) return asEASTCodecStatus::Malformed; if (Ref.LocalIndex > Count) return asEASTCodecStatus::InvalidIndex; Ref.Kind = static_cast<asEASTCrossReferenceKind>(K); P.CrossReferences.Add(MoveTemp(Ref)); }
			Status = CodecReadCount(R, N, Items, B); if (Status != asEASTCodecStatus::Succeeded) return Status; P.SemanticReferences.Reserve(N);
			for (uint32 I = 0; I < N; ++I) { asSASTProjectedSemanticReference Ref; uint8 K = 0, M = 0; if (!R.U8(K) || !R.U32(Ref.Ordinal) || !R.U8(M) || !R.Key(Ref.TypeIdentity)) return asEASTCodecStatus::Malformed; Ref.Kind = static_cast<asEASTSemanticTypeReferenceKind>(K); Ref.PassingMode = static_cast<asEParameterPassingMode>(M); P.SemanticReferences.Add(MoveTemp(Ref)); }
			if (!R.Bool(P.Declaration.bPresent)) return asEASTCodecStatus::Malformed;
			if (P.Declaration.bPresent)
			{
				uint8 Access = 0;
				if (!R.U8(Access)) return asEASTCodecStatus::Malformed;
				P.Declaration.Access = static_cast<asEAccessSpecifier>(Access);
				if (!R.Bool(P.Declaration.AuthoredAccess.bPresent)) return asEASTCodecStatus::Malformed;
				if (P.Declaration.AuthoredAccess.bPresent)
				{
					if (!R.String(P.Declaration.AuthoredAccess.Name)
						|| !CodecReadAnchor(R, P.Declaration.AuthoredAccess.KeywordSource)
						|| !CodecReadAnchor(R, P.Declaration.AuthoredAccess.ColonSource)
						|| !CodecReadAnchor(R, P.Declaration.AuthoredAccess.NameSource))
					{
						return asEASTCodecStatus::Malformed;
					}
				}
			}
			if (!R.Bool(P.Expression.bPresent)) return asEASTCodecStatus::Malformed;
			if (P.Expression.bPresent) { uint8 C = 0; if (!R.Key(P.Expression.TypeIdentity) || !R.U8(C) || !R.Bool(P.Expression.bReadOnly)) return asEASTCodecStatus::Malformed; P.Expression.ValueCategory = static_cast<asEValueCategory>(C); }
			Status = CodecReadPayload(R, P.Payload, Items, B); if (Status != asEASTCodecStatus::Succeeded) return Status;
			if (!R.Key(P.StableIdentity)) return asEASTCodecStatus::Malformed;
			if (!CodecValidateRecord(P, Count)) return asEASTCodecStatus::InvalidPayload;
			Temp.Add(MoveTemp(P));
		}
		if (!R.AtEnd()) return asEASTCodecStatus::Malformed;
		if (!CodecValidateProjectionStructure(Temp)) return asEASTCodecStatus::InvalidPayload;
		Out.Records = MoveTemp(Temp); return asEASTCodecStatus::Succeeded;
	}

END_AS_NAMESPACE
