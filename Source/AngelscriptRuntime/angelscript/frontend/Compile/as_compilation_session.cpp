#include "frontend/Compile/as_compilation_session.h"
#include "frontend/Basic/as_function_modifiers.h"

#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_ast_visitor.h"
#include "frontend/AST/as_ast_verifier.h"
#include "frontend/AST/as_type.h"
#include "as_module_definition_set.h"
#include "as_typeinfo.h"

#include "Templates/Function.h"
#include "Containers/Set.h"
#include <atomic>
#include <thread>
#include <vector>

BEGIN_AS_NAMESPACE

namespace
	{
		struct asSDeclarationInfo
		{
			asCDecl* Declaration = nullptr;
			FString Scope;
			FString QualifiedName;
			FString LogicalSourceKey;
			asUINT BeginOffset = 0;
			asUINT EndOffset = 0;
			int32 Ordinal = 0;
		};

		FString JoinScope(const FString& Scope, const FString& Name)
		{
			return Scope.IsEmpty() ? Name : Scope + TEXT("::") + Name;
		}

		FString MakeScopeLookupKey(const FString& Scope, const FString& Name)
		{
			return Scope + TEXT("\x1f") + Name;
		}

		asEPrimitiveType GetPrimitiveType(const FString& Name, const asSTypeContextOptions& Options)
		{
			if (Name == TEXT("void")) return asEPrimitiveType::Void;
			if (Name == TEXT("bool")) return asEPrimitiveType::Bool;
			if (Name == TEXT("int8")) return asEPrimitiveType::Int8;
			if (Name == TEXT("int16")) return asEPrimitiveType::Int16;
			if (Name == TEXT("int") || Name == TEXT("int32")) return asEPrimitiveType::Int32;
			if (Name == TEXT("int64")) return asEPrimitiveType::Int64;
			if (Name == TEXT("uint8")) return asEPrimitiveType::UInt8;
			if (Name == TEXT("uint16")) return asEPrimitiveType::UInt16;
			if (Name == TEXT("uint") || Name == TEXT("uint32")) return asEPrimitiveType::UInt32;
			if (Name == TEXT("uint64")) return asEPrimitiveType::UInt64;
			if (Name == TEXT("float")) return Options.FloatIsFloat64 ? asEPrimitiveType::Float64 : asEPrimitiveType::Float32;
			if (Name == TEXT("float32")) return asEPrimitiveType::Float32;
			if (Name == TEXT("double") || Name == TEXT("float64")) return asEPrimitiveType::Float64;
			return asEPrimitiveType::Invalid;
		}

		FString DeclKindName(const asCDecl& Declaration)
		{
			switch (Declaration.GetKind())
			{
			case asEDeclKind::TranslationUnit: return TEXT("translation-unit");
			case asEDeclKind::Namespace: return TEXT("namespace");
			case asEDeclKind::Record: return TEXT("record");
			case asEDeclKind::EnumConstant: return TEXT("enum-constant");
			case asEDeclKind::Function: return TEXT("function");
			case asEDeclKind::Var: return TEXT("variable");
			case asEDeclKind::ParmVar: return TEXT("parameter");
			case asEDeclKind::Recovery: return TEXT("recovery");
			case asEDeclKind::Delegate: return TEXT("delegate");
			case asEDeclKind::Event: return TEXT("event");
			case asEDeclKind::TypeAlias: return TEXT("type-alias");
			case asEDeclKind::AccessSpecifier: return TEXT("access-specifier");
			default: return TEXT("invalid");
			}
		}
	}

	asCCompilationSession::asCCompilationSession(
		TSharedRef<asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot,
		asCDiagnosticsEngine& InDiagnostics,
		FString InStableScope)
		: asCCompilationSession(MoveTemp(Snapshot), InDiagnostics, MoveTemp(InStableScope), MakeShared<asCTypeContext, ESPMode::ThreadSafe>())
	{
	}

	asCCompilationSession::asCCompilationSession(
		TSharedRef<asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot,
		asCDiagnosticsEngine& InDiagnostics, FString InStableScope,
		TSharedRef<asCTypeContext, ESPMode::ThreadSafe> TypeContext)
		: Context(MoveTemp(Snapshot), MoveTemp(TypeContext))
		, Diagnostics(InDiagnostics)
		, Sema(Context, InDiagnostics, InStableScope)
		, StableScope(MoveTemp(InStableScope))
	{
	}

	bool asCCompilationSession::CollectSource(
		asCPreprocessor&,
		const asSDirectiveProcessResult& Input,
		FString LogicalSourceKey)
	{
		if (Phase != asECompilationPhase::Collecting) return false;
		asCDeclarationFragment Fragment(MoveTemp(LogicalSourceKey));
		asCParser Parser(Sema);
		const bool bValid = Parser.Collect(Input, Fragment);
		if (!bValid) Fragment.MarkError();
		if (Input.IsValid()) SourceInputs.Add(Input);
		Fragments.Add(MoveTemp(Fragment));
		return bValid;
	}

	asSDirectiveProcessResult asCCompilationSession::GetRetainedBodyInput(const asCSourceRange& Range) const
	{
		if (!Range.IsOrdered()) return {};
		for (const asSDirectiveProcessResult& Source : SourceInputs)
		{
			if (Source.SnapshotID != Range.Begin.SnapshotID) continue;
			asSDirectiveProcessResult Input;
			Input.SnapshotID = Source.SnapshotID;
			Input.ConfigurationDigest = Source.ConfigurationDigest;
			Input.SourceSnapshot = Source.SourceSnapshot;
			Input.IdentifierOwner = Source.IdentifierOwner;
			for (const asCToken& Token : Source.ActiveTokens)
			{
				if (Token.Kind != asETokenKind::EndOfFile && Token.Range.Begin.FileID == Range.Begin.FileID &&
					Token.Range.Begin.ByteOffset >= Range.Begin.ByteOffset && Token.Range.End.ByteOffset <= Range.End.ByteOffset)
					Input.ActiveTokens.Add(Token);
			}
			if (!Input.ActiveTokens.IsEmpty() && Input.ActiveTokens[0].Range.Begin == Range.Begin && Input.ActiveTokens.Last().Range.End == Range.End)
			{ Input.bValid = true; return Input; }
		}
		return {};
	}

	bool asCCompilationSession::CloseDeclarationBarrier()
	{
		if (Phase != asECompilationPhase::Collecting) return false;
		Fragments.Sort([](const asCDeclarationFragment& Left, const asCDeclarationFragment& Right)
		{
			return Left.GetLogicalSourceKey() < Right.GetLogicalSourceKey();
		});

		asCSourceRange RootRange;
		auto ExpandRootRange = [&RootRange](const asCSourceRange& Range)
		{
			if (!Range.IsOrdered()) return;
			if (!RootRange.IsOrdered())
			{
				RootRange = Range;
				return;
			}
			if (Range.Begin.SnapshotID != RootRange.Begin.SnapshotID
				|| Range.Begin.FileID != RootRange.Begin.FileID)
			{
				return;
			}
			if (Range.Begin.ByteOffset < RootRange.Begin.ByteOffset) RootRange.Begin = Range.Begin;
			if (Range.End.ByteOffset > RootRange.End.ByteOffset) RootRange.End = Range.End;
		};
		for (const asCDeclarationFragment& Fragment : Fragments)
		{
			for (asCDecl* Declaration : Fragment.GetTopLevelDecls())
				ExpandRootRange(Declaration->GetSourceRange());
		}
		// An empty, valid translation unit still has a source anchor (EOF).
		if (!RootRange.IsOrdered() && !SourceInputs.IsEmpty() && !SourceInputs[0].RawTokens.IsEmpty())
			RootRange = SourceInputs[0].RawTokens[0].Range;

		TranslationUnit = Context.Create<asCTranslationUnitDecl>(RootRange);
		for (const asCDeclarationFragment& Fragment : Fragments)
		{
			for (asCDecl* Declaration : Fragment.GetTopLevelDecls())
			{
				TranslationUnit->AddDecl(Declaration);
			}
		}
		Phase = asECompilationPhase::DeclarationsCollected;
		return true;
	}

	bool asCCompilationSession::ResolveDeclarations(int32 WorkerCount)
	{
		if (Phase != asECompilationPhase::DeclarationsCollected || !TranslationUnit || WorkerCount <= 0) return false;
		CanonicalLookup.Reset();
		QualifiedLookup.Reset();
		ModuleIdentities.Reset();
		DeclaredTypes.Reset();
		RecordDeclarations.Reset();
		NominalDeclarations.Reset();
		StableProjection.Reset();
		StableDiagnostics.Reset();
		bHasErrors = false;
		bHasRecoveryNodes = false;
		asCStableIdentityRegistry& Registry = Context.GetTypeContext()->GetIdentityRegistry();
		const auto& Snapshot = Context.GetSourceSnapshot();
		for (const asCDeclarationFragment& Fragment : Fragments)
		{
			bHasErrors |= Fragment.HasErrors();
			asSDeclarationIdentityDescriptor Module;
			Module.StableScope = StableScope;
			Module.Kind = asEDeclarationIdentityKind::Module;
			Module.Name = Fragment.GetLogicalSourceKey();
			asSStableKey Key;
			if (Registry.InternDeclaration(Module, Key) == asETypeIdentityStatus::Succeeded) ModuleIdentities.Add(Module.Name, Key);
			else { bHasErrors = true; StableDiagnostics.Add(TEXT("invalid-module-identity:") + Module.Name); }
		}

		TArray<asSDeclarationInfo> Infos;
		int32 NextOrdinal = 0;
		TFunction<void(asCDeclContext&, const FString&)> Gather = [&](asCDeclContext& DeclarationContext, const FString& Scope)
		{
			for (asCDecl* Declaration : DeclarationContext.GetDecls())
			{
				asSDeclarationInfo Info;
				Info.Declaration = Declaration;
				Info.Scope = Scope;
				Info.Ordinal = NextOrdinal++;
				Info.BeginOffset = Declaration->GetSourceRange().Begin.ByteOffset;
				Info.EndOffset = Declaration->GetSourceRange().End.ByteOffset;
				FStringView LogicalSourceKey;
				if (Snapshot->TryGetLogicalSourceKey(Declaration->GetSourceRange().Begin.FileID, LogicalSourceKey)) Info.LogicalSourceKey = FString(LogicalSourceKey);
				if (const asCNamedDecl* Named = dyn_cast<asCNamedDecl>(Declaration)) Info.QualifiedName = JoinScope(Scope, Named->GetName());
				Infos.Add(MoveTemp(Info));
				if (auto* Namespace = dyn_cast<asCNamespaceDecl>(Declaration)) Gather(*Namespace, JoinScope(Scope, Namespace->GetName()));
				else if (auto* Record = dyn_cast<asCRecordDecl>(Declaration)) Gather(*Record, JoinScope(Scope, Record->GetName()));
				else if (auto* Function = dyn_cast<asCFunctionDecl>(Declaration)) Gather(*Function, Scope);
				else if (auto* Callable = dyn_cast<asCCallableTypeDecl>(Declaration)) Gather(*Callable, Scope);
			}
		};
		Gather(*TranslationUnit, {});
		Infos.Sort([](const asSDeclarationInfo& Left, const asSDeclarationInfo& Right)
		{
			if (Left.LogicalSourceKey != Right.LogicalSourceKey) return Left.LogicalSourceKey < Right.LogicalSourceKey;
			if (Left.BeginOffset != Right.BeginOffset) return Left.BeginOffset < Right.BeginOffset;
			if (Left.EndOffset != Right.EndOffset) return Left.EndOffset < Right.EndOffset;
			return Left.Ordinal < Right.Ordinal;
		});
		TMap<const asCDecl*, const asSDeclarationInfo*> InfoByDeclaration;
		for (const asSDeclarationInfo& Info : Infos) InfoByDeclaration.Add(Info.Declaration, &Info);
		auto Fail = [&](asCDecl* Decl, const FString& Message)
		{
			Decl->MarkInvalid();
			bHasErrors = true;
			StableDiagnostics.Add(Message);
		};
		auto Parent = [](const asCDecl* Decl) -> asCDecl*
		{
			return Decl->GetDeclContext() ? Decl->GetDeclContext()->GetOwnerDecl() : nullptr;
		};
		auto NamespacePath = [&](const asCDecl* Decl)
		{
			TArray<FString> Result;
			for (asCDecl* Owner = Parent(Decl); Owner; Owner = Parent(Owner))
				if (const auto* Namespace = dyn_cast<asCNamespaceDecl>(Owner)) Result.Insert(Namespace->GetName(), 0);
			return Result;
		};
		auto NominalOwner = [&](const asCDecl* Decl) -> asCRecordDecl*
		{
			for (asCDecl* Owner = Parent(Decl); Owner; Owner = Parent(Owner))
				if (auto* Record = dyn_cast<asCRecordDecl>(Owner)) return Record;
			return nullptr;
		};
		auto Duplicate = [&](asCNamedDecl* Decl, asCDecl* Existing, const asSDeclarationInfo& Info)
		{
			const auto* Primary = InfoByDeclaration.FindRef(Existing);
			Fail(Decl, FString::Printf(TEXT("duplicate:%s:%s:%s"), *Decl->GetName(),
				Primary ? *Primary->LogicalSourceKey : TEXT("<unknown>"), *Info.LogicalSourceKey));
		};

		// Pass 1: admit all nominals with semantic owner edges before resolving any
		// signature. Source order and authored type spelling never enter this key.
		TMap<FString, asCNamedDecl*> TypeByQualifiedName;
		for (const asSDeclarationInfo& Info : Infos)
		{
			if (isa<asCRecoveryDecl>(Info.Declaration))
			{
				bHasRecoveryNodes = true;
				Fail(Info.Declaration, FString::Printf(TEXT("recovery:%s:%u:%u"), *Info.LogicalSourceKey, Info.BeginOffset, Info.EndOffset));
				continue;
			}
			if (!isa<asCRecordDecl>(Info.Declaration) && !isa<asCCallableTypeDecl>(Info.Declaration) && !isa<asCTypeAliasDecl>(Info.Declaration)) continue;
			auto* Named = cast<asCNamedDecl>(Info.Declaration);
			if (asCNamedDecl** Existing = TypeByQualifiedName.Find(Info.QualifiedName)) Duplicate(Named, *Existing, Info);
			else TypeByQualifiedName.Add(Info.QualifiedName, Named);
		}
		TFunction<bool(asCNamedDecl*)> InternNominal = [&](asCNamedDecl* Decl)
		{
			if (Decl->IsInvalid()) return false;
			if (!Decl->GetStableIdentity().IsZero()) return true;
			asSNominalTypeDescriptor Descriptor;
			Descriptor.StableScope = StableScope;
			Descriptor.NamespacePath = NamespacePath(Decl);
			Descriptor.Name = Decl->GetName();
			Descriptor.Kind = isa<asCTypeAliasDecl>(Decl) ? asETypeDeclKind::Typedef :
				isa<asCRecordDecl>(Decl) ? cast<asCRecordDecl>(Decl)->GetDeclaredTypeKind() : cast<asCCallableTypeDecl>(Decl)->GetDeclaredTypeKind();
			if (auto* Owner = NominalOwner(Decl))
			{
				if (!InternNominal(Owner)) { Fail(Decl, TEXT("invalid-nominal-owner:") + Decl->GetName()); return false; }
				Descriptor.Owner = Owner->GetStableIdentity();
			}
			asSStableKey Key;
			if (Registry.InternNominalType(Descriptor, Key) != asETypeIdentityStatus::Succeeded)
			{
				Fail(Decl, TEXT("invalid-nominal-identity:") + Decl->GetName()); return false;
			}
			Decl->SetStableIdentity(Key);
			asSStableTypeUseDescriptor Use;
			Use.Kind = asETypeUseKind::Nominal;
			Use.Declaration = Key;
			asSStableKey UseKey;
			if (Registry.InternTypeUse(Use, UseKey) != asETypeIdentityStatus::Succeeded)
			{
				Fail(Decl, TEXT("invalid-nominal-type-use:") + Decl->GetName()); return false;
			}
			asCNominalType* Type = Context.GetNominalType(Key, UseKey);
			if (!Type) { Fail(Decl, TEXT("invalid-nominal-payload:") + Decl->GetName()); return false; }
			DeclaredTypes.Add(Key, Type);
			NominalDeclarations.Add(Key, Decl);
			if (auto* Record = dyn_cast<asCRecordDecl>(Decl)) RecordDeclarations.Add(Key, Record);
			return true;
		};
		for (const asSDeclarationInfo& Info : Infos)
			if (isa<asCRecordDecl>(Info.Declaration) || isa<asCCallableTypeDecl>(Info.Declaration) || isa<asCTypeAliasDecl>(Info.Declaration)) InternNominal(cast<asCNamedDecl>(Info.Declaration));

		TFunction<asCType*(asCTypeAliasDecl*)> ResolveAlias;
		TFunction<asCType*(const FString&, FString, bool)> ResolveNamedType = [&](const FString& TypeName, FString Scope, bool bGlobal) -> asCType*
		{
			const asEPrimitiveType Primitive = GetPrimitiveType(TypeName, Context.GetTypeContext()->GetOptions());
			if (Primitive != asEPrimitiveType::Invalid) return Context.GetBuiltinType(Primitive);
			if (bGlobal) Scope.Reset();
			for (;;)
			{
				if (asCNamedDecl** Found = TypeByQualifiedName.Find(JoinScope(Scope, TypeName)))
				{
					if ((*Found)->IsInvalid()) return nullptr;
					if (auto* Alias = dyn_cast<asCTypeAliasDecl>(*Found)) return ResolveAlias(Alias);
					return GetDeclaredType((*Found)->GetStableIdentity());
				}
				if (auto* External = ExternalTypes.Find(JoinScope(Scope, TypeName))) return *External;
				if (Scope.IsEmpty()) return nullptr;
				int32 Separator = INDEX_NONE;
				if (Scope.FindLastChar(TEXT(':'), Separator)) Scope.LeftInline(FMath::Max(0, Separator - 1));
				else Scope.Reset();
			}
		};
		TSet<asCTypeAliasDecl*> ResolvingAliases;
		ResolveAlias = [&](asCTypeAliasDecl* Alias) -> asCType*
		{
			if (Alias->IsInvalid()) return nullptr;
			if (Alias->GetResolvedAliasedType()) return Alias->GetResolvedAliasedType();
			if (ResolvingAliases.Contains(Alias)) { Fail(Alias, TEXT("cyclic-type-alias:") + Alias->GetName()); return nullptr; }
			ResolvingAliases.Add(Alias);
			const auto* Info = InfoByDeclaration.FindRef(Alias);
			const auto Type = ResolveStructuredType(Alias->GetTypeSyntax(), [&](const FString& Name, bool Global) { return ResolveNamedType(Name, Info ? Info->Scope : FString{}, Global); });
			ResolvingAliases.Remove(Alias);
			const auto* Builtin = dyn_cast<asCBuiltinType>(Type.GetType());
			if (!Builtin || Builtin->GetPrimitive() == asEPrimitiveType::Void || Type.GetQualifiers() != asETypeQualifiers::None)
			{ Fail(Alias, TEXT("invalid-type-alias:") + Alias->GetName()); return nullptr; }
			Alias->SetResolvedAliasedType(Type.GetType());
			DeclaredTypes.Add(Alias->GetStableIdentity(), Type.GetType());
			return Type.GetType();
		};
		for (const auto& Info : Infos) if (auto* Alias = dyn_cast<asCTypeAliasDecl>(Info.Declaration)) ResolveAlias(Alias);
		auto ResolveSyntax = [&](const asSTypeSyntax& Syntax, const FString& Scope, asCQualType& Out)
		{
			Out = ResolveStructuredType(Syntax, [&](const FString& Name, bool Global) { return ResolveNamedType(Name, Scope, Global); });
			return Out.IsValid();
		};
		auto PassingMode = [](const asSTypeSyntax& Syntax)
		{
			switch (Syntax.Reference)
			{
			case asETypeReferenceDirection::In: return asEParameterPassingMode::In;
			case asETypeReferenceDirection::Out: return asEParameterPassingMode::Out;
			case asETypeReferenceDirection::InOut: return asEParameterPassingMode::InOut;
			default: return asEParameterPassingMode::Value;
			}
		};

		// Non-type host coordinates use the same registry. Namespaces may reopen
		// in another file; their coordinate is still the same lexical scope.
		TFunction<bool(asCNamedDecl*)> InternCoordinate = [&](asCNamedDecl* Decl)
		{
			if (Decl->IsInvalid()) return false;
			if (!Decl->GetStableIdentity().IsZero()) return true;
			asSDeclarationIdentityDescriptor Descriptor;
			Descriptor.StableScope = StableScope;
			Descriptor.Name = Decl->GetName();
			if (isa<asCNamespaceDecl>(Decl)) Descriptor.Kind = asEDeclarationIdentityKind::Namespace;
			else if (isa<asCEnumConstantDecl>(Decl)) Descriptor.Kind = asEDeclarationIdentityKind::EnumConstant;
			else Descriptor.Kind = NominalOwner(Decl) ? asEDeclarationIdentityKind::Property : asEDeclarationIdentityKind::GlobalVariable;
			if (auto* Owner = dyn_cast<asCNamedDecl>(Parent(Decl)))
			{
				if (Owner->GetStableIdentity().IsZero() && !InternCoordinate(Owner)) return false;
				Descriptor.Owner = Owner->GetStableIdentity();
			}
			asSStableKey Key;
			if (Registry.InternDeclaration(Descriptor, Key) != asETypeIdentityStatus::Succeeded)
			{
				Fail(Decl, TEXT("invalid-declaration-identity:") + Decl->GetName()); return false;
			}
			return Decl->SetStableIdentity(Key);
		};

		// Pass 2: resolve qualified type uses and callable signatures. Param names
		// and source locations are retained on AST declarations, never hashed as
		// a substitute for resolved types or passing modes.
		TMap<asSStableKey, asCFunctionDecl*> UniqueSignatures;
		TMap<FString, asCDecl*> UniqueDeclarations;
		for (const asSDeclarationInfo& Info : Infos)
		{
			auto* Named = dyn_cast<asCNamedDecl>(Info.Declaration);
			if (!Named || Named->IsInvalid() || Named->GetKind() == asEDeclKind::ParmVar) continue;
			if (auto* Record = dyn_cast<asCRecordDecl>(Named))
			{
				for (const FString& BaseName : Record->GetUnresolvedBases())
				{
					const bool Global = BaseName.StartsWith(TEXT("::"));
					if (asCType* Type = ResolveNamedType(Global ? BaseName.Mid(2) : BaseName, Info.Scope, Global)) Record->AddResolvedBase(Type);
					else Fail(Record, TEXT("unresolved-base:") + Info.QualifiedName + TEXT(":") + BaseName);
				}
			}
			else if (isa<asCTypeAliasDecl>(Named)) { /* Resolved before callable signatures. */ }
			else if (isa<asCFunctionDecl>(Named) || isa<asCCallableTypeDecl>(Named))
			{
				auto* Function = dyn_cast<asCFunctionDecl>(Named);
				auto* Callable = dyn_cast<asCCallableTypeDecl>(Named);
				const asSTypeSyntax& ReturnSyntax = Function ? Function->GetReturnTypeSyntax() : Callable->GetReturnTypeSyntax();
				const auto Parameters = Function ? Function->GetParameters() : Callable->GetParameters();
				asCQualType ReturnType;
				const bool ClassDefault = Function && Function->GetFunctionKind() == asEFunctionKind::ClassDefault;
				bool bValid = ClassDefault ? asCQualType::TryCreate(Context.GetBuiltinType(asEPrimitiveType::Void), asETypeQualifiers::None, ReturnType)
					: ResolveSyntax(ReturnSyntax, Info.Scope, ReturnType);
				asSStableTypeUseDescriptor Signature;
				Signature.Kind = asETypeUseKind::Callable;
				bValid = bValid && ReturnType.GetStableIdentity(Registry, Signature.ReturnType) == asETypeIdentityStatus::Succeeded;
				if (!bValid) Fail(Named, (Function ? TEXT("unresolved-return:") : TEXT("unresolved-callable-return:")) + Info.QualifiedName);
				TArray<asCQualType> ParameterTypes;
				TSet<FString> ParameterNames;
				for (int32 Ordinal = 0; Ordinal < Parameters.Num(); ++Ordinal)
				{
					asCVarDecl* Parameter = Parameters[Ordinal];
					asCQualType Type;
					bool bParameterValid = ResolveSyntax(Parameter->GetTypeSyntax(), Info.Scope, Type);
					if (const auto* Builtin = dyn_cast<asCBuiltinType>(Type.GetType())) bParameterValid &= Builtin->GetPrimitive() != asEPrimitiveType::Void;
					if (!Parameter->GetName().IsEmpty())
					{
						bParameterValid &= !ParameterNames.Contains(Parameter->GetName());
						ParameterNames.Add(Parameter->GetName());
					}
					asSStableParameter Edge;
					Edge.PassingMode = PassingMode(Parameter->GetTypeSyntax());
					bParameterValid = bParameterValid && Type.GetStableIdentity(Registry, Edge.Type) == asETypeIdentityStatus::Succeeded;
					if (bParameterValid)
					{
						Parameter->SetResolvedType(Type.GetType());
						ParameterTypes.Add(Type);
						Signature.Parameters.Add(Edge);
					}
					else
					{
						Fail(Parameter, FString::Printf(TEXT("invalid-callable-parameter:%s:%d"), *Info.QualifiedName, Ordinal));
						bValid = false;
					}
				}
				if (!bValid) { Named->MarkInvalid(); continue; }
				asSStableKey SignatureKey;
				if (Registry.InternTypeUse(Signature, SignatureKey) != asETypeIdentityStatus::Succeeded)
				{
					Fail(Named, TEXT("invalid-callable-identity:") + Info.QualifiedName); continue;
				}
				TArray<asEParameterPassingMode> ParameterModes;
				for (const auto& Parameter : Signature.Parameters) ParameterModes.Add(Parameter.PassingMode);
				asCFunctionType* SignatureType = Context.GetFunctionType(ReturnType, MoveTemp(ParameterTypes), SignatureKey, MoveTemp(ParameterModes));
				if (!SignatureType || SignatureType->IsInvalid())
				{
					Fail(Named, TEXT("invalid-callable-payload:") + Info.QualifiedName); continue;
				}
				if (Callable) Callable->SetResolvedSignature(SignatureType);
				else
				{
					Function->SetResolvedReturnType(ReturnType.GetType());
					Function->SetResolvedSignature(SignatureType);
					asSFunctionIdentityDescriptor Descriptor;
					Descriptor.StableScope = StableScope;
					Descriptor.NamespacePath = NamespacePath(Function);
					Descriptor.Name = Function->GetName();
					Descriptor.ReturnType = Signature.ReturnType;
					Descriptor.Parameters = Signature.Parameters;
					Descriptor.Kind = asEFunctionIdentityKind::Global;
					if (auto* Owner = NominalOwner(Function)) { Descriptor.Owner = Owner->GetStableIdentity(); Descriptor.Kind = asEFunctionIdentityKind::Method; }
					if (Function->GetFunctionKind() == asEFunctionKind::Constructor) Descriptor.Kind = asEFunctionIdentityKind::Constructor;
					else if (Function->GetFunctionKind() == asEFunctionKind::Destructor) Descriptor.Kind = asEFunctionIdentityKind::Destructor;
					else if (ClassDefault) Descriptor.Kind = asEFunctionIdentityKind::ClassDefault;
					else if (Function->GetFunctionKind() == asEFunctionKind::Conversion)
					{
						Descriptor.Kind = asEFunctionIdentityKind::Conversion;
						Descriptor.ConversionDestination = Signature.ReturnType;
						const auto* ReturnBuiltin = dyn_cast<asCBuiltinType>(ReturnType.GetType());
						if (Parameters.Num() != 0 || (ReturnBuiltin && ReturnBuiltin->GetPrimitive() == asEPrimitiveType::Void))
						{
							Fail(Function, TEXT("invalid-conversion-shape:") + Info.QualifiedName); continue;
						}
					}
					Descriptor.DefinitionModifiers = asGetFunctionDefinitionModifiers(Function->GetModifiers());
					if (Function->IsConstMethod()) Descriptor.Modifiers = asEFunctionIdentityModifiers::ConstMethod;
					asSStableKey Key;
					const asETypeIdentityStatus Status = Registry.InternFunction(Descriptor, Key);
					if (Status != asETypeIdentityStatus::Succeeded)
					{
						Fail(Function, (Status == asETypeIdentityStatus::ReturnTypeConflict ? TEXT("return-type-conflict:") : TEXT("invalid-function-identity:")) + Info.QualifiedName); continue;
					}
					Function->SetStableIdentity(Key);
					if (auto** Existing = UniqueSignatures.Find(Key)) { Duplicate(Function, *Existing, Info); continue; }
					UniqueSignatures.Add(Key, Function);
				}
				for (int32 Ordinal = 0; Ordinal < Parameters.Num(); ++Ordinal)
				{
					asSDeclarationIdentityDescriptor Parameter;
					Parameter.StableScope = StableScope;
					Parameter.Owner = Named->GetStableIdentity();
					Parameter.Kind = asEDeclarationIdentityKind::Parameter;
					Parameter.Ordinal = static_cast<asUINT>(Ordinal);
					asSStableKey Key;
					if (Registry.InternDeclaration(Parameter, Key) == asETypeIdentityStatus::Succeeded) Parameters[Ordinal]->SetStableIdentity(Key);
					else { Fail(Named, TEXT("invalid-parameter-identity:") + Info.QualifiedName); Parameters[Ordinal]->MarkInvalid(); }
				}
			}
			else
			{
				if (auto* Variable = dyn_cast<asCVarDecl>(Named))
				{
					asCQualType Type;
					asSStableKey TypeKey;
					const auto& Syntax = Variable->GetTypeSyntax();
					const bool Auto = Syntax.NameParts.Num() == 1 && Syntax.NameParts[0] == TEXT("auto");
					if (Auto)
					{
						if (!Variable->HasDeferredInitializer()) Fail(Variable, TEXT("auto-requires-initializer:") + Info.QualifiedName);
						// A declaration shell with no resolved type is not a fake
						// semantic type. Infer exactly once after lookup is built.
					}
					else if (ResolveSyntax(Syntax, Info.Scope, Type) && Type.GetStableIdentity(Registry, TypeKey) == asETypeIdentityStatus::Succeeded) Variable->SetResolvedType(Type.GetType());
					else
					{
						Variable->SetResolvedType(Context.Create<asCErrorType>());
						Fail(Variable, TEXT("unresolved-type:") + Info.QualifiedName + TEXT(":") + Variable->GetTypeSpelling());
					}
				}
				if (!isa<asCAccessSpecifierDecl>(Named)) InternCoordinate(Named);
			}
			if (!isa<asCFunctionDecl>(Named) && !isa<asCRecordDecl>(Named) && !isa<asCCallableTypeDecl>(Named))
			{
				const FString UniqueKey = MakeScopeLookupKey(Info.Scope, Named->GetName());
				if (asCDecl** Existing = UniqueDeclarations.Find(UniqueKey))
				{
					if (isa<asCNamespaceDecl>(Named) && isa<asCNamespaceDecl>(*Existing)) continue;
					Duplicate(Named, *Existing, Info);
				}
				else UniqueDeclarations.Add(UniqueKey, Named);
			}
		}
		for (const asSDeclarationInfo& Info : Infos)
		{
			auto* Named = dyn_cast<asCNamedDecl>(Info.Declaration);
			if (Named && !Named->IsInvalid() && !Named->GetName().IsEmpty() && Named->GetKind() != asEDeclKind::ParmVar)
			{
				CanonicalLookup.FindOrAdd(Named->GetName()).Add(Named);
				QualifiedLookup.FindOrAdd(Info.QualifiedName).Add(Named);
			}
			StableProjection.Add(FString::Printf(TEXT("%s|%s|%s|%u|%u|%s"), *DeclKindName(*Info.Declaration), *Info.QualifiedName,
				*Info.LogicalSourceKey, Info.BeginOffset, Info.EndOffset, Named ? *Named->GetSemanticKey() : TEXT("")));
		}
		for (auto& Entry : CanonicalLookup)
			Entry.Value.Sort([](const asCDecl& Left, const asCDecl& Right)
			{
				return cast<asCNamedDecl>(&Left)->GetSemanticKey() < cast<asCNamedDecl>(&Right)->GetSemanticKey();
			});
		StableProjection.Sort();
		for (auto& Entry : QualifiedLookup)
			Entry.Value.Sort([](const asCDecl& Left, const asCDecl& Right)
			{
				return cast<asCNamedDecl>(&Left)->GetSemanticKey() < cast<asCNamedDecl>(&Right)->GetSemanticKey();
			});
		if (!bHasErrors && !ResolveAccessPolicies()) bHasErrors = true;
		if (!bHasErrors && !ValidateRecordDeclarations()) bHasErrors = true;
		if (!bHasErrors && !ResolveInferredDeclarations()) bHasErrors = true;
		if (!bHasErrors && !ResolveConstantDeclarations()) bHasErrors = true;
		StableDiagnostics.Sort();
		Phase = asECompilationPhase::DeclarationsResolved;
		return !bHasErrors;
	}

	bool asCCompilationSession::AnalyzeBodies(int32 WorkerCount)
	{
		// A direct session may collect diagnostics from otherwise valid bodies
		// after declaration recovery. The sticky error state still forbids a
		// successful body result, sealing or publication; Builder stops earlier.
		if (Phase != asECompilationPhase::DeclarationsResolved || WorkerCount <= 0 || Context.IsSealed()) return false;
		BodyFragments.Reset();
		StableBodyProjection.Reset();
		StableBodyDiagnostics.Reset();

		// Global, member and parameter default initializers are actual owned AST
		// expressions. Complete them before body workers can select defaults.
		TArray<asCVarDecl*> Initializers;
		for (asCASTNode* Node : Context.GetNodes())
			if (auto* Variable = dyn_cast<asCVarDecl>(Node); Variable && !Variable->IsInvalid() && Variable->HasDeferredInitializer()) Initializers.Add(Variable);
		Initializers.Sort([](const asCVarDecl& Left, const asCVarDecl& Right) { return Left.GetSemanticKey() < Right.GetSemanticKey(); });
		for (asCVarDecl* Variable : Initializers)
		{
			if (Variable->GetInitializer()) continue; // Already needed by an enum constant at declaration resolution.
			asCSema InitializerSema(Context, Diagnostics, StableScope);
			InitializerSema.BeginInitializer(*Variable,
				[this, Variable](const FString& Name) { return LookupInBodyScope(Name, *Variable); },
				[this, Variable](const FString& Name) {
					asSTypeSyntax Syntax; Syntax.NameParts.Add(Name); Syntax.Range = Variable->GetSourceRange();
					return ResolveTypeSyntax(Syntax, *Variable).GetType();
				},
				[this, Variable](const asSTypeSyntax& Syntax) { return ResolveTypeSyntax(Syntax, *Variable); },
				[this](const asSStableKey& Key) { return FindNominalDeclaration(Key); },
				[this](const asCSourceRange& Range) { return GetRetainedBodyInput(Range); },
				[this](asCQualType Type, asSListInitialization& Out) { return ResolveListInitializer(Type, Out); },
				[this](const FString& Name) { return FindExternalCallables(Name); });
			asCParser Parser(InitializerSema);
			const bool Parsed = Parser.ParseInitializer(GetRetainedBodyInput(Variable->GetDeferredInitializerRange()), *Variable);
			const bool Analyzed = InitializerSema.EndInitializer();
			if (!Parsed || !Analyzed) { bHasErrors = true; StableBodyDiagnostics.Add(Variable->GetSemanticKey() + TEXT(":initializer-analysis-failed")); }
		}

		// DefaultArgumentExpr is a non-owning reference to a parameter's actual
		// initializer. Resolve all initializers first, then reject expansion
		// cycles before parallel body readers can consume this immutable graph.
		TMap<asCVarDecl*, uint8> InitializerColors;
		TFunction<bool(asCVarDecl*, int32)> CheckInitializer = [&](asCVarDecl* Variable, int32 Depth)
		{
			if (!Variable || !Variable->HasDeferredInitializer()) return true;
			const uint8 Color = InitializerColors.FindRef(Variable);
			if (Color == 2) return true;
			if (Color == 1 || Depth > 256)
			{
				StableBodyDiagnostics.Add(Variable->GetSemanticKey() + (Color == 1 ? TEXT(":initializer-dependency-cycle") : TEXT(":initializer-dependency-budget")));
				Variable->MarkInvalid(); return false;
			}
			if (Variable->IsInvalid() || !Variable->GetInitializer()) return false;
			InitializerColors.Add(Variable, 1);
			bool Valid = true;
			asCASTVisitor::Walk(Variable->GetInitializer(), [&](const asCASTNode& Node)
			{
				if (const auto* Default = dyn_cast<asCDefaultArgumentExpr>(&Node))
					Valid = CheckInitializer(Default->GetParameter(), Depth + 1) && Valid;
			});
			InitializerColors.Add(Variable, 2);
			if (!Valid) Variable->MarkInvalid();
			return Valid;
		};
		for (asCVarDecl* Variable : Initializers) if (!CheckInitializer(Variable, 0)) bHasErrors = true;

		TArray<asCFunctionDecl*> Functions;
		for (asCASTNode* Node : Context.GetNodes())
		{
			if (asCFunctionDecl* Function = dyn_cast<asCFunctionDecl>(Node);
				Function && !Function->IsInvalid() && !Function->GetBody() &&
				(Function->HasDeferredBody() || !Function->GetDeferredStatementRanges().IsEmpty()))
			{
				Functions.Add(Function);
			}
		}
		Functions.Sort([](const asCFunctionDecl& Left, const asCFunctionDecl& Right)
		{
			return Left.GetSemanticKey() < Right.GetSemanticKey();
		});

		auto TypeLookup = [this](const FString& Spelling, const asCFunctionDecl& Function) -> asCType*
		{
			// This callback is a name lookup (for example the configured string
			// provider), never a parser for printed or generated type spellings.
			asSTypeSyntax Syntax; Syntax.NameParts.Add(Spelling); Syntax.Range = Function.GetSourceRange();
			return ResolveTypeSyntax(Syntax, Function).GetType();
		};

		// Work owns one Sema, parser and fragment per body. Only the immutable
		// declaration lookup and synchronized context/type allocation are shared.
		// Merge in stable function order after joining; allocation order is not identity.
		TArray<TUniquePtr<asCBodyFragment>> Results;
		for (asCFunctionDecl* Function : Functions)
		{
			asSDeferredBodyWorkItem Work;
			Work.Function = Function;
			Work.StableFunctionKey = Function->GetStableIdentity();
			Work.BodyRange = Function->GetDeferredBodyRange();
			if (!Function->GetDeferredStatementRanges().IsEmpty())
			{
				const auto Ranges = Function->GetDeferredStatementRanges();
				Work.BodyRange = {Ranges[0].Begin, Ranges.Last().End};
			}
			Results.Add(MakeUnique<asCBodyFragment>(MoveTemp(Work)));
		}
		BodyStatistics = {WorkerCount, FMath::Min(WorkerCount, Functions.Num()), 0};
		auto Analyze = [&](int32 WorkerIndex)
		{
			asCSema WorkerSema(Context, Diagnostics, StableScope);
			for (int32 Index = WorkerIndex; Index < Functions.Num(); Index += BodyStatistics.StartedWorkers)
			{
			asCFunctionDecl* Function = Functions[Index];
			asCBodyFragment& Fragment = *Results[Index];
			const asSDirectiveProcessResult Input = GetRetainedBodyInput(Function->GetDeferredBodyRange());
			asCParser Parser(WorkerSema);
			WorkerSema.BeginBody(
				Fragment,
				[this, Function](const FString& Name) { return LookupInBodyScope(Name, *Function); },
				[&TypeLookup, Function](const FString& Name) { return TypeLookup(Name, *Function); },
				[this, Function](const asSTypeSyntax& Syntax) { return ResolveTypeSyntax(Syntax, *Function); },
				[this](const asSStableKey& Key) { return FindNominalDeclaration(Key); },
				[this](const asCSourceRange& Range) { return GetRetainedBodyInput(Range); },
				[this](asCQualType Type, asSListInitialization& Out) { return ResolveListInitializer(Type, Out); },
				[this](const FString& Name) { return FindExternalCallables(Name); });
			if (Function->GetFunctionKind() == asEFunctionKind::ClassDefault)
			{
				auto* Body = Context.Create<asCCompoundStmt>(Fragment.GetWorkItem().BodyRange);
				WorkerSema.PushBodyScope();
				for (const auto& Range : Function->GetDeferredStatementRanges())
				{
					asCStmt* Statement = Parser.ParseClassDefaultStatement(GetRetainedBodyInput(Range));
					if (!Statement || !Body->AddStatement(Statement)) Fragment.MarkError(TEXT("class-default-analysis-failed"));
				}
				WorkerSema.PopBodyScope();
				Fragment.SetBody(Body);
			}
			else if (!Parser.ParseBody(Input, Fragment)) Fragment.MarkError(TEXT("body-analysis-failed"));
			WorkerSema.EndBody();

			if (Fragment.GetBody())
			{
				Fragment.GetLifetimePlan().Analyze(Fragment.GetBody());
			}
			else Fragment.MarkError(TEXT("body-result-missing"));
			}
		};
		if (BodyStatistics.StartedWorkers == 1) Analyze(0);
		else if (BodyStatistics.StartedWorkers > 1)
		{
			std::vector<std::thread> Workers;
			Workers.reserve(BodyStatistics.StartedWorkers);
			for (int32 Index = 0; Index < BodyStatistics.StartedWorkers; ++Index) Workers.emplace_back(Analyze, Index);
			for (std::thread& Worker : Workers) Worker.join();
		}
		for (int32 Index = 0; Index < Functions.Num(); ++Index)
		{
			asCFunctionDecl* Function = Functions[Index];
			asCBodyFragment& Fragment = *Results[Index];
			if (Fragment.GetBody()) Function->SetBody(Fragment.GetBody());
			++BodyStatistics.AnalyzedFragments;
			if (Fragment.HasErrors()) bHasErrors = true;
			for (const FString& Diagnostic : Fragment.GetStableDiagnostics())
			{
				StableBodyDiagnostics.Add(Function->GetSemanticKey() + TEXT(":") + Diagnostic);
			}
			BodyFragments.Add(MoveTemp(Fragment));
		}

		for (const asCVarDecl* Variable : Initializers)
		{
			if (!Variable->GetInitializer()) continue;
			asCASTVisitor::Walk(Variable->GetInitializer(), [&](const asCASTNode& Node)
			{
				StableBodyProjection.Add(FString::Printf(TEXT("%s|initializer|%u|%u|%u"), *Variable->GetSemanticKey(),
					Node.GetRawKind(), Node.GetSourceRange().Begin.ByteOffset, Node.GetSourceRange().End.ByteOffset));
			});
		}
		for (const asCBodyFragment& Fragment : BodyFragments)
		{
			if (!Fragment.GetBody()) continue;
			asCASTVisitor::Walk(Fragment.GetBody(), [&](const asCASTNode& Node)
			{
				const asCSourceRange& Range = Node.GetSourceRange();
				FString CrossReference;
				if (const asCASTNode* Cross = asCASTVisitor::GetCrossReference(Node))
				{
					if (const asCNamedDecl* Named = dyn_cast<asCNamedDecl>(Cross)) CrossReference = Named->GetSemanticKey();
					else CrossReference = FString::Printf(
						TEXT("stmt:%u:%u:%u"),
						Cross->GetRawKind(),
						Cross->GetSourceRange().Begin.ByteOffset,
						Cross->GetSourceRange().End.ByteOffset);
				}
				FString TypeFact;
				if (const asCExpr* Expression = dyn_cast<asCExpr>(&Node); Expression && Expression->GetExpressionType())
				{
					if (const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Expression->GetExpressionType()))
						TypeFact = FString::Printf(TEXT("builtin:%u"), static_cast<uint8>(Builtin->GetPrimitive()));
					else TypeFact = FString::Printf(TEXT("type-kind:%u"), static_cast<uint16>(Expression->GetExpressionType()->GetKind()));
				}
				StableBodyProjection.Add(FString::Printf(
					TEXT("%s|%u|%u|%u|%s|%s"),
					*Fragment.GetWorkItem().StableFunctionKey.ToHexString(),
					Node.GetRawKind(),
					Range.Begin.ByteOffset,
					Range.End.ByteOffset,
					*CrossReference,
					*TypeFact));
			});
			const FString Lifetime = Fragment.GetLifetimePlan().SerializeStable();
			if (!Lifetime.IsEmpty()) StableBodyProjection.Add(Fragment.GetWorkItem().StableFunctionKey.ToHexString() + TEXT("|lifetime|") + Lifetime);
		}
		StableBodyProjection.Sort();
		StableBodyDiagnostics.Sort();
		Phase = asECompilationPhase::BodiesAnalyzed;
		return !bHasErrors;
	}

	bool asCCompilationSession::VerifyAST()
	{
		if (Phase != asECompilationPhase::BodiesAnalyzed || bHasErrors) return false;
		asSASTVerificationResult Result;
		if (!Context.VerifyAndSeal(TranslationUnit, Result))
		{
			VerificationError = Result.Diagnostic;
			bHasErrors = true;
			Phase = asECompilationPhase::Failed;
			return false;
		}
		Phase = asECompilationPhase::ASTVerified;
		return true;
	}

	TConstArrayView<asCDecl*> asCCompilationSession::Lookup(const FString& Name) const
	{
		if (Phase != asECompilationPhase::DeclarationsResolved && Phase != asECompilationPhase::BodiesAnalyzed && Phase != asECompilationPhase::ASTVerified) return {};
		if (const TArray<asCDecl*>* Found = CanonicalLookup.Find(Name)) return MakeArrayView(*Found);
		return {};
	}

	TConstArrayView<asCDecl*> asCCompilationSession::LookupInBodyScope(const FString& Name, const asCDecl& Declaration) const
	{
		TArray<FString> Parts;
		for (const asCDeclContext* Scope = Declaration.GetDeclContext(); Scope; )
		{
			const asCDecl* Owner = Scope->GetOwnerDecl();
			if (!Owner) break;
			if (const asCNamedDecl* Named = dyn_cast<asCNamedDecl>(Owner); Named && !isa<asCFunctionDecl>(Owner) && !isa<asCCallableTypeDecl>(Owner)) Parts.Insert(Named->GetName(), 0);
			Scope = Owner->GetDeclContext();
		}
		FString ScopeName = FString::Join(Parts, TEXT("::"));
		FString LookupName = Name;
		if (LookupName.StartsWith(TEXT("::"))) { LookupName.RightChopInline(2); ScopeName.Reset(); }
		for (;;)
		{
			if (const auto* Found = QualifiedLookup.Find(JoinScope(ScopeName, LookupName))) return MakeArrayView(*Found);
			if (ScopeName.IsEmpty()) return {};
			int32 Separator = INDEX_NONE;
			if (ScopeName.FindLastChar(TEXT(':'), Separator)) ScopeName.LeftInline(FMath::Max(0, Separator - 1));
			else ScopeName.Reset();
		}
	}

	FString asCCompilationSession::SerializeStableProjection() const
	{
		return FString::Join(StableProjection, TEXT("\n"));
	}

	FString asCCompilationSession::SerializeStableDiagnostics() const
	{
		return FString::Join(StableDiagnostics, TEXT("\n"));
	}

	FString asCCompilationSession::SerializeStableBodyProjection() const
	{
		return FString::Join(StableBodyProjection, TEXT("\n"));
	}

	FString asCCompilationSession::SerializeStableBodyDiagnostics() const
	{
		return FString::Join(StableBodyDiagnostics, TEXT("\n"));
	}

END_AS_NAMESPACE
