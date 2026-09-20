#include "frontend/Compile/as_descriptor_consumer.h"

#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_attr.h"
#include "frontend/AST/as_decl.h"
#include "frontend/AST/as_type.h"
#include "frontend/Compile/as_compilation_session.h"
#include "Templates/Function.h"

BEGIN_AS_NAMESPACE

namespace
	{
		FString AttrName(const asCAttr& Attr)
		{
			if (const asCAnnotationAttr* Annotation = dyn_cast<asCAnnotationAttr>(&Attr)) return Annotation->GetName();
			if (const asCFlagAttr* Flag = dyn_cast<asCFlagAttr>(&Attr)) return Flag->GetName();
			if (const asCStringAttr* String = dyn_cast<asCStringAttr>(&Attr)) return String->GetName();
			return {};
		}

		bool HasAttr(const asCDecl& Decl, FStringView Name)
		{
			const FString OwnedName(Name);
			for (const asCAttr* Attr : Decl.GetAttrs())
			{
				if (Attr && AttrName(*Attr).Equals(OwnedName, ESearchCase::IgnoreCase)) return true;
			}
			return false;
		}

		void ProjectMetadataArguments(TConstArrayView<asSAnnotationArgument> Arguments, TMap<FName, FString>& Out)
		{
			for (const asSAnnotationArgument& Argument : Arguments)
			{
				if (Argument.Kind == asEAnnotationValueKind::Group)
				{
					// Meta is the explicit host metadata group; arbitrary groups stay in the AST.
					if (Argument.Name.Equals(TEXT("Meta"), ESearchCase::IgnoreCase)) ProjectMetadataArguments(Argument.Arguments, Out);
				}
				else Out.Add(FName(*Argument.Name), Argument.Value);
			}
		}

		void CopyMetadata(const asCDecl& Decl, TMap<FName, FString>& Out)
		{
			for (const asCAttr* Attr : Decl.GetAttrs())
			{
				if (const asCStringAttr* String = dyn_cast<asCStringAttr>(Attr))
				{
					Out.Add(FName(*String->GetName()), String->GetValue());
				}
				else if (const asCAnnotationAttr* Annotation = dyn_cast<asCAnnotationAttr>(Attr))
				{
					ProjectMetadataArguments(Annotation->GetArguments(), Out);
				}
			}
		}

		void CopyEnumMetadata(const asCDecl& Decl, int32 ValueIndex, TMap<TPair<FName, int32>, FString>& Out)
		{
			TMap<FName, FString> Metadata;
			CopyMetadata(Decl, Metadata);
			for (const auto& Entry : Metadata) Out.Add(TPair<FName, int32>(Entry.Key, ValueIndex), Entry.Value);
		}

		asSStableKey TypeUseKey(const asCType* Type)
		{
			return Type ? Type->GetStableIdentity() : asSStableKey{};
		}

		asSStableKey QualifiedTypeUseKey(const asCVarDecl& Variable)
		{
			asSStableKey Key;
			if (Variable.GetOwnerContext())
				Variable.GetResolvedQualType().GetStableIdentity(Variable.GetOwnerContext()->GetTypeContext()->GetIdentityRegistry(), Key);
			return Key;
		}

		bool MakeAnchor(
			const asCSourceManager& SourceManager,
			const asCSourceRange& Range,
			FAngelscriptResolvedSourceAnchor& Out)
		{
			asSStableSourceAnchor Anchor;
			if (!SourceManager.TryCreateStableAnchor(Range, Anchor)) return false;
			Out.LogicalSourceKey = MoveTemp(Anchor.LogicalSourceKey);
			Out.ContentRevision = Anchor.ContentRevision;
			Out.BeginByteOffset = Anchor.BeginByteOffset;
			Out.EndByteOffset = Anchor.EndByteOffset;
			return Out.IsValid();
		}

		bool FillSemantic(
			const asCNamedDecl& Decl,
			const asCSourceManager& SourceManager,
			asSStableKey OwnerKey,
			asSStableKey TypeKey,
			FAngelscriptDescriptorSemanticInfo& Out)
		{
			Out.StableDeclarationKey = Decl.GetStableIdentity();
			Out.StableOwnerKey = MoveTemp(OwnerKey);
			Out.StableTypeUseKey = MoveTemp(TypeKey);
			if (Out.StableDeclarationKey.IsZero() || !MakeAnchor(SourceManager, Decl.GetSourceRange(), Out.SourceAnchor)) return false;
			Out.Lifecycle = EAngelscriptDescriptorLifecycle::Resolved;
			return true;
		}

		int32 SourceLine(const asCSourceManager& SourceManager, const asCSourceRange& Range)
		{
			TConstArrayView<uint8> Bytes;
			if (!SourceManager.TryGetBytes(Range.Begin.FileID, Bytes))
			{
				return 1;
			}
			asUINT Newlines = 0;
			const asUINT Limit = Range.Begin.ByteOffset < static_cast<asUINT>(Bytes.Num())
				? Range.Begin.ByteOffset
				: static_cast<asUINT>(Bytes.Num());
			for (asUINT Index = 0; Index < Limit; ++Index)
			{
				if (Bytes[Index] == static_cast<uint8>('\n'))
				{
					++Newlines;
				}
			}
			// Match the previous TryGetLineColumn path: 1-based line, then +1 for host LineNumber.
			return static_cast<int32>(Newlines + 2);
		}

		bool ProjectFunction(
			const asCFunctionDecl& Function,
			const asCSourceManager& SourceManager,
			const asSStableKey& OwnerKey,
			TSharedRef<FAngelscriptFunctionDesc>& Out)
		{
			Out->FunctionName = Function.GetName();
			Out->ScriptFunctionName = Function.GetName();
			Out->LineNumber = SourceLine(SourceManager, Function.GetSourceRange());
			Out->bBlueprintCallable = HasAttr(Function, TEXT("UFUNCTION"));
			CopyMetadata(Function, Out->Meta);
			if (!FillSemantic(Function, SourceManager, OwnerKey, TypeUseKey(Function.GetResolvedSignature()), Out->Semantic)) return false;

			int32 ParameterIndex = 0;
			for (const asCVarDecl* Parameter : Function.GetParameters())
			{
				if (!Parameter || Parameter->IsInvalid() || !Parameter->GetResolvedType()) return false;
				FAngelscriptArgumentDesc& Argument = Out->Arguments.AddDefaulted_GetRef();
				Argument.ArgumentName = Parameter->GetName();
				Argument.Semantic.StableDeclarationKey = Parameter->GetStableIdentity();
				Argument.Semantic.StableOwnerKey = Function.GetStableIdentity();
				Argument.Semantic.StableTypeUseKey = QualifiedTypeUseKey(*Parameter);
				if (Argument.Semantic.StableDeclarationKey.IsZero()
					|| Argument.Semantic.StableTypeUseKey.IsZero()
					|| !MakeAnchor(SourceManager, Parameter->GetSourceRange(), Argument.Semantic.SourceAnchor)) return false;
				Argument.Semantic.Lifecycle = EAngelscriptDescriptorLifecycle::Resolved;
				++ParameterIndex;
			}
			return true;
		}

		bool ProjectRecord(
			const asCRecordDecl& Record,
			const asCCompilationSession& Session,
			const asCSourceManager& SourceManager,
			FAngelscriptModuleDesc& Module,
			bool bIncludeUnattributedRecords)
		{
			if (HasAttr(Record, TEXT("UENUM")))
			{
				TSharedRef<FAngelscriptEnumDesc> Enum = MakeShared<FAngelscriptEnumDesc>();
				Enum->EnumName = Record.GetName();
				Enum->LineNumber = SourceLine(SourceManager, Record.GetSourceRange());
				if (!FillSemantic(Record, SourceManager, Module.Semantic.StableDeclarationKey, TypeUseKey(Session.GetDeclaredType(Record.GetStableIdentity())), Enum->Semantic)) return false;
				CopyEnumMetadata(Record, INDEX_NONE, Enum->Meta);
				for (const asCDecl* Member : Record.GetDecls())
				{
					const asCEnumConstantDecl* Constant = dyn_cast<asCEnumConstantDecl>(Member);
					if (!Constant || Constant->IsInvalid() || !Constant->HasResolvedValue()) return false;
					const int32 ValueIndex = Enum->ValueNames.Add(FName(*Constant->GetName()));
					Enum->EnumValues.Add(static_cast<int32>(Constant->GetValue()));
					CopyEnumMetadata(*Constant, ValueIndex, Enum->Meta);
				}
				Module.Enums.Add(MoveTemp(Enum));
				return true;
			}

			if (!HasAttr(Record, TEXT("UCLASS")) && !HasAttr(Record, TEXT("USTRUCT")) && !bIncludeUnattributedRecords) return true;
			TSharedRef<FAngelscriptClassDesc> Class = MakeShared<FAngelscriptClassDesc>();
			Class->ClassName = Record.GetName();
			Class->bIsStruct = HasAttr(Record, TEXT("USTRUCT"));
			Class->LineNumber = SourceLine(SourceManager, Record.GetSourceRange());
			CopyMetadata(Record, Class->Meta);
			const asSStableKey DeclKey = TypeUseKey(Session.GetDeclaredType(Record.GetStableIdentity()));
			if (!FillSemantic(Record, SourceManager, Module.Semantic.StableDeclarationKey, DeclKey, Class->Semantic)) return false;

			for (const asCType* Base : Record.GetResolvedBases())
			{
				const asCNominalType* Nominal = dyn_cast<asCNominalType>(Base);
				if (!Nominal) continue;
				asCRecordDecl* Parent = Session.FindRecordDeclaration(Nominal->GetDeclarationIdentity());
				if (!Parent) continue;
				if (Parent->GetDeclaredTypeKind() == asETypeDeclKind::Interface)
				{
					Class->ImplementedInterfaces.AddUnique(Parent->GetName());
				}
				else if (Class->SuperClass.IsEmpty())
				{
					Class->SuperClass = Parent->GetName();
				}
			}

			for (const asCDecl* Member : Record.GetDecls())
			{
				if (!Member || Member->IsInvalid()) return false;
				if (const asCVarDecl* Variable = dyn_cast<asCVarDecl>(Member))
				{
					if (!HasAttr(*Variable, TEXT("UPROPERTY"))) continue;
					TSharedRef<FAngelscriptPropertyDesc> Property = MakeShared<FAngelscriptPropertyDesc>();
					Property->PropertyName = Variable->GetName();
					Property->LiteralType = Variable->GetTypeSpelling();
					Property->LineNumber = SourceLine(SourceManager, Variable->GetSourceRange());
					CopyMetadata(*Variable, Property->Meta);
					if (!FillSemantic(*Variable, SourceManager, Class->Semantic.StableDeclarationKey, QualifiedTypeUseKey(*Variable), Property->Semantic)) return false;
					Class->Properties.Add(MoveTemp(Property));
				}
				else if (const asCFunctionDecl* Function = dyn_cast<asCFunctionDecl>(Member))
				{
					if (!HasAttr(*Function, TEXT("UFUNCTION"))) continue;
					TSharedRef<FAngelscriptFunctionDesc> Method = MakeShared<FAngelscriptFunctionDesc>();
					if (!ProjectFunction(*Function, SourceManager, Class->Semantic.StableDeclarationKey, Method)) return false;
					Class->Methods.Add(MoveTemp(Method));
				}
			}
			Module.Classes.Add(MoveTemp(Class));
			return true;
		}

		bool ProjectDeclarations(
			TConstArrayView<asCDecl*> Declarations,
			const asCCompilationSession& Session,
			const asCSourceManager& SourceManager,
			FAngelscriptModuleDesc& Module,
			bool bIncludeUnattributedRecords)
		{
			for (const asCDecl* Declaration : Declarations)
			{
				if (!Declaration || Declaration->IsInvalid()) return false;
				if (const asCNamespaceDecl* Namespace = dyn_cast<asCNamespaceDecl>(Declaration))
				{
					if (!ProjectDeclarations(Namespace->GetDecls(), Session, SourceManager, Module, bIncludeUnattributedRecords)) return false;
				}
				else if (const asCRecordDecl* Record = dyn_cast<asCRecordDecl>(Declaration))
				{
					if (!ProjectRecord(*Record, Session, SourceManager, Module, bIncludeUnattributedRecords)) return false;
				}
				else if (const asCCallableTypeDecl* Callable = dyn_cast<asCCallableTypeDecl>(Declaration))
				{
					const asCFunctionType* FunctionType = Callable->GetResolvedSignature();
					if (!FunctionType) return false;
					TSharedRef<FAngelscriptDelegateDesc> Delegate = MakeShared<FAngelscriptDelegateDesc>();
					Delegate->DelegateName = Callable->GetName();
					Delegate->bIsMulticast = Callable->IsMulticast();
					Delegate->bIsDynamic = Callable->GetFlavor() == asECallableFlavor::Dynamic;
					Delegate->LineNumber = SourceLine(SourceManager, Callable->GetSourceRange());
					Delegate->Signature = MakeShared<FAngelscriptFunctionDesc>();
					TSharedRef<FAngelscriptFunctionDesc> Signature = Delegate->Signature.ToSharedRef();
					Signature->FunctionName = Callable->GetName();
					Signature->ScriptFunctionName = Callable->GetName();
					Signature->LineNumber = Delegate->LineNumber;
					if (!FillSemantic(*Callable, SourceManager, Module.Semantic.StableDeclarationKey, TypeUseKey(Session.GetDeclaredType(Callable->GetStableIdentity())), Delegate->Semantic)) return false;
					if (!FillSemantic(*Callable, SourceManager, Callable->GetStableIdentity(), TypeUseKey(FunctionType), Signature->Semantic)) return false;
					for (const asCVarDecl* Parameter : Callable->GetParameters())
					{
						FAngelscriptArgumentDesc& Argument = Signature->Arguments.AddDefaulted_GetRef();
						Argument.ArgumentName = Parameter->GetName();
						if (!FillSemantic(*Parameter, SourceManager, Callable->GetStableIdentity(), QualifiedTypeUseKey(*Parameter), Argument.Semantic)) return false;
					}
					Module.Delegates.Add(MoveTemp(Delegate));
				}
			}
			return true;
		}
	}

	bool FAngelscriptDescriptorConsumer::Collect(
		const asCCompilationSession& Session,
		const asCSourceManager& SourceManager,
		TArray<asSCollectedDeclarationDescription>& Out) const
	{
		Out.Reset();
		if (Session.GetPhase() != asECompilationPhase::DeclarationsCollected) return false;
		TFunction<bool(TConstArrayView<asCDecl*>, const FString&)> Visit;
		Visit = [&](TConstArrayView<asCDecl*> Declarations, const FString& Scope)
		{
			for (const asCDecl* Decl : Declarations)
			{
				if (!Decl || Decl->IsInvalid()) return false;
				const asCNamedDecl* Named = dyn_cast<asCNamedDecl>(Decl);
				if (!Named) continue;
				asSCollectedDeclarationDescription Description;
				Description.Kind = Decl->GetKind();
				Description.QualifiedName = Scope.IsEmpty() ? Named->GetName() : Scope + TEXT("::") + Named->GetName();
				if (!MakeAnchor(SourceManager, Decl->GetSourceRange(), Description.Source)) return false;
				if (const asCCallableTypeDecl* Callable = dyn_cast<asCCallableTypeDecl>(Decl))
				{
					Description.ReturnType = Callable->GetReturnTypeSyntax();
					for (const asCVarDecl* Parameter : Callable->GetParameters())
					{
						Description.Parameters.Add({Parameter->GetName(), Parameter->GetTypeSyntax(), Parameter->GetSourceRange()});
					}
				}
				const FString QualifiedName = Description.QualifiedName;
				Out.Add(MoveTemp(Description));
				if (const asCNamespaceDecl* Namespace = dyn_cast<asCNamespaceDecl>(Decl))
				{
					if (!Visit(Namespace->GetDecls(), QualifiedName)) return false;
				}
				else if (const asCRecordDecl* Record = dyn_cast<asCRecordDecl>(Decl))
				{
					if (!Visit(Record->GetDecls(), QualifiedName)) return false;
				}
			}
			return true;
		};
		for (const asCDeclarationFragment& Fragment : Session.GetFragments())
		{
			if (Fragment.HasErrors() || !Visit(Fragment.GetTopLevelDecls(), {})) { Out.Reset(); return false; }
		}
		return true;
	}

	bool FAngelscriptDescriptorConsumer::Project(
		const asCCompilationSession& Session,
		const asCSourceManager& SourceManager,
		FAngelscriptPreprocessResult& InOutResult,
		bool bIncludeUnattributedRecords) const
	{
		InOutResult.Modules.Reset();
		if (!Session.IsPublishable())
		{
			InOutResult.RejectPublishableOutput();
			return false;
		}

		for (const asCDeclarationFragment& Fragment : Session.GetFragments())
		{
			if (Fragment.HasErrors() || Fragment.GetTopLevelDecls().IsEmpty()) continue;
			TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
			Module->ModuleName = Fragment.GetLogicalSourceKey();
			Module->Semantic.StableDeclarationKey = Session.GetModuleIdentity(Fragment.GetLogicalSourceKey());
			Module->Semantic.StableOwnerKey = {};
			if (!MakeAnchor(SourceManager, Fragment.GetTopLevelDecls()[0]->GetSourceRange(), Module->Semantic.SourceAnchor))
			{
				InOutResult.RejectPublishableOutput();
				return false;
			}
			Module->Semantic.Lifecycle = EAngelscriptDescriptorLifecycle::Resolved;
			if (!ProjectDeclarations(Fragment.GetTopLevelDecls(), Session, SourceManager, Module.Get(), bIncludeUnattributedRecords))
			{
				InOutResult.RejectPublishableOutput();
				return false;
			}
			InOutResult.Modules.Add(MoveTemp(Module));
		}

		InOutResult.Modules.Sort([](
			const TSharedRef<FAngelscriptModuleDesc>& Left,
			const TSharedRef<FAngelscriptModuleDesc>& Right)
		{
			return Left->ModuleName.Compare(Right->ModuleName, ESearchCase::CaseSensitive) < 0;
		});
		return true;
	}

END_AS_NAMESPACE
