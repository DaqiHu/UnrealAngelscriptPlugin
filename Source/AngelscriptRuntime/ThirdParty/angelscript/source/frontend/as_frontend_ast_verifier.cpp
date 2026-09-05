#include "frontend/as_ast_verifier.h"

#include "frontend/as_ast_context.h"
#include "frontend/as_ast_visitor.h"
#include "frontend/as_constant_evaluator.h"

#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"

BEGIN_AS_NAMESPACE

// Keep the exact registry-backed type validator private to asCASTContext.
	// File-local verification helpers reach it only through this named friend.
	class asCASTVerifierAccess
	{
	public:
		static bool ValidateTypeIdentity(const asCASTContext& Context, const asCType& Type)
		{
			return Context.ValidateTypeIdentity(Type);
		}
	};

	namespace
	{
		bool Fail(
			asSASTVerificationResult& Out,
			asEASTVerificationStatus Status,
			const asCASTNode* Node,
			const TCHAR* Message)
		{
			Out.Status = Status;
			Out.LocalNodeID = Node ? Node->GetContextLocalID() : 0;
			Out.Diagnostic = Message;
			return false;
		}

		bool IsValidRange(const asCASTContext& Context, const asCSourceRange& Range)
		{
			if (!Range.IsOrdered()
				|| Range.Begin.SnapshotID != Context.GetSourceSnapshot()->GetSnapshotID())
			{
				return false;
			}
			TConstArrayView<uint8> Bytes;
			return Context.GetSourceSnapshot()->TryGetBytes(Range.Begin.FileID, Bytes)
				&& Range.End.ByteOffset <= static_cast<asUINT>(Bytes.Num());
		}

		bool IsContainedRange(const asCSourceRange& Outer, const asCSourceRange& Inner)
		{
			return Outer.IsOrdered() && Inner.IsOrdered()
				&& Outer.Begin.SnapshotID == Inner.Begin.SnapshotID
				&& Outer.Begin.FileID == Inner.Begin.FileID
				&& Outer.Begin.ByteOffset <= Inner.Begin.ByteOffset
				&& Inner.End.ByteOffset <= Outer.End.ByteOffset;
		}

		bool IsRangeBefore(const asCSourceRange& Left, const asCSourceRange& Right)
		{
			return Left.IsOrdered() && Right.IsOrdered()
				&& Left.Begin.SnapshotID == Right.Begin.SnapshotID
				&& Left.Begin.FileID == Right.Begin.FileID
				&& Left.End.ByteOffset <= Right.Begin.ByteOffset;
		}

		const asSAuthoredAccessUse* GetAuthoredAccessUse(const asCDecl& Decl)
		{
			if (const auto* Variable = dyn_cast<asCVarDecl>(&Decl))
				return Variable->GetAuthoredAccessUse();
			if (const auto* Function = dyn_cast<asCFunctionDecl>(&Decl))
				return Function->GetAuthoredAccessUse();
			return nullptr;
		}

		bool ValidateAuthoredAccessUse(
			const asCASTContext& Context,
			const asCDecl& Member,
			const asSAuthoredAccessUse& Use)
		{
			const asCDeclContext* Parent = Member.GetDeclContext();
			const auto* Record = Parent ? dyn_cast<asCRecordDecl>(Parent->GetOwnerDecl()) : nullptr;
			return Record && Member.GetAccess() == asEAccessSpecifier::Public
				&& !Use.Name.IsEmpty() && Use.Resolved && Context.Owns(Use.Resolved)
				&& Use.Resolved->GetDeclContext() == Parent
				&& Use.Resolved->GetName() == Use.Name
				&& Use.Resolved->GetStableIdentity().IsZero()
				&& IsValidRange(Context, Use.KeywordRange)
				&& IsValidRange(Context, Use.ColonRange)
				&& IsValidRange(Context, Use.NameRange)
				&& IsContainedRange(Member.GetSourceRange(), Use.KeywordRange)
				&& IsContainedRange(Member.GetSourceRange(), Use.ColonRange)
				&& IsContainedRange(Member.GetSourceRange(), Use.NameRange)
				&& IsRangeBefore(Use.KeywordRange, Use.ColonRange)
				&& IsRangeBefore(Use.ColonRange, Use.NameRange);
		}

		bool ValidateAccessSpecifier(
			const asCASTContext& Context,
			const asCAccessSpecifierDecl& Access)
		{
			const asCDeclContext* Parent = Access.GetDeclContext();
			const auto* Record = Parent ? dyn_cast<asCRecordDecl>(Parent->GetOwnerDecl()) : nullptr;
			const asSAccessSpecifierSourceInfo& Source = Access.GetSourceInfo();
			if (!Record || Access.GetName().IsEmpty() || !Access.GetStableIdentity().IsZero()
				|| Access.GetAccess() != asEAccessSpecifier::Public
				|| (Access.GetBaseAccess() != asEAccessSpecifier::Private
					&& Access.GetBaseAccess() != asEAccessSpecifier::Protected)
				|| !IsValidRange(Context, Source.KeywordRange)
				|| !IsValidRange(Context, Source.NameRange)
				|| !IsValidRange(Context, Source.EqualsRange)
				|| !IsValidRange(Context, Source.BaseAccessRange)
				|| !IsValidRange(Context, Source.SemicolonRange)
				|| !IsContainedRange(Access.GetSourceRange(), Source.KeywordRange)
				|| !IsContainedRange(Access.GetSourceRange(), Source.NameRange)
				|| !IsContainedRange(Access.GetSourceRange(), Source.EqualsRange)
				|| !IsContainedRange(Access.GetSourceRange(), Source.BaseAccessRange)
				|| !IsContainedRange(Access.GetSourceRange(), Source.SemicolonRange)
				|| !IsRangeBefore(Source.KeywordRange, Source.NameRange)
				|| !IsRangeBefore(Source.NameRange, Source.EqualsRange)
				|| !IsRangeBefore(Source.EqualsRange, Source.BaseAccessRange)
				|| Access.GetPermissions().Num() > static_cast<int32>(asCStableIdentityRegistry::MaxDescriptorEdges))
			{
				return false;
			}

			asCSourceRange Previous = Source.BaseAccessRange;
			uint32 EdgeCount = 0;
			for (const asSAuthoredAccessPermission& Permission : Access.GetPermissions())
			{
				if (++EdgeCount > asCStableIdentityRegistry::MaxDescriptorEdges
					|| Permission.Subject > asEAccessPermissionSubject::Wildcard
					|| ((Permission.Subject == asEAccessPermissionSubject::Named) != !Permission.Name.IsEmpty())
					|| !IsValidRange(Context, Permission.SubjectRange)
					|| !IsValidRange(Context, Permission.Range)
					|| !IsContainedRange(Access.GetSourceRange(), Permission.Range)
					|| !IsContainedRange(Permission.Range, Permission.SubjectRange)
					|| !IsRangeBefore(Previous, Permission.Range)
					|| !IsRangeBefore(Permission.Range, Source.SemicolonRange)) return false;

				asCSourceRange PreviousModifier = Permission.SubjectRange;
				for (const asSAccessPermissionModifier& Modifier : Permission.Modifiers)
				{
					if (++EdgeCount > asCStableIdentityRegistry::MaxDescriptorEdges
						|| Modifier.Kind > asEAccessPermissionModifier::Inherited
						|| !IsValidRange(Context, Modifier.Range)
						|| !IsContainedRange(Permission.Range, Modifier.Range)
						|| !IsRangeBefore(PreviousModifier, Modifier.Range)) return false;
					PreviousModifier = Modifier.Range;
				}
				Previous = Permission.Range;
			}
			return IsRangeBefore(Previous, Source.SemicolonRange);
		}

		bool IsConversionFunctionName(const FString& Name)
		{
			return Name == TEXT("opConv") || Name == TEXT("opImplConv")
				|| Name == TEXT("opCast") || Name == TEXT("opImplCast");
		}

		bool IsImplicitConversionFunctionName(const FString& Name)
		{
			return Name == TEXT("opImplConv") || Name == TEXT("opImplCast");
		}

		bool ValidateTypeSyntax(
			const asCASTContext& Context,
			const asSTypeSyntax& Syntax,
			uint32 Depth = 0,
			uint32* SharedCount = nullptr)
		{
			uint32 LocalCount = 0;
			if (!SharedCount) SharedCount = &LocalCount;
			const uint64 AddedEdges = 1ull + static_cast<uint64>(Syntax.NameParts.Num())
				+ static_cast<uint64>(Syntax.GenericArguments.Num())
				+ static_cast<uint64>(Syntax.ArrayDimensions.Num());
			if (Depth > 64
				|| static_cast<uint64>(*SharedCount) + AddedEdges
					> asCStableIdentityRegistry::MaxDescriptorEdges)
				return false;
			*SharedCount += static_cast<uint32>(AddedEdges);
			if (!Syntax.IsValid()) return false;
			if (Syntax.NameParts.Num() != Syntax.ComponentRanges.Num()
				|| Syntax.Reference > asETypeReferenceDirection::InOut
				|| !IsValidRange(Context, Syntax.Range))
			{
				return false;
			}
			for (int32 Index = 0; Index < Syntax.NameParts.Num(); ++Index)
			{
				if (Syntax.NameParts[Index].IsEmpty()
					|| !IsValidRange(Context, Syntax.ComponentRanges[Index])
					|| !IsContainedRange(Syntax.Range, Syntax.ComponentRanges[Index]))
				{
					return false;
				}
			}
			for (const asSTypeSyntax& Argument : Syntax.GenericArguments)
			{
				if (!IsContainedRange(Syntax.Range, Argument.Range)
					|| !ValidateTypeSyntax(Context, Argument, Depth + 1, SharedCount))
				{
					return false;
				}
			}
			for (const asCSourceRange& Dimension : Syntax.ArrayDimensions)
			{
				if (!IsValidRange(Context, Dimension)
					|| !IsContainedRange(Syntax.Range, Dimension))
				{
					return false;
				}
			}
			return true;
		}

		bool ValidateAnnotationArguments(
			const asCASTContext& Context,
			TConstArrayView<asSAnnotationArgument> Arguments,
			uint32 Depth,
			uint32& Count,
			const asCSourceRange* ParentRange = nullptr)
		{
			if (Depth > 64) return false;
			for (const asSAnnotationArgument& Argument : Arguments)
			{
				if (++Count > asCStableIdentityRegistry::MaxDescriptorEdges
					|| Argument.Name.IsEmpty()
					|| Argument.Kind > asEAnnotationValueKind::Group
					|| !IsValidRange(Context, Argument.Range)
					|| (ParentRange && !IsContainedRange(*ParentRange, Argument.Range)))
				{
					return false;
				}
				if (Argument.Kind == asEAnnotationValueKind::Group)
				{
					if (!Argument.Value.IsEmpty()
						|| !IsValidRange(Context, Argument.ValueRange)
						|| !IsContainedRange(Argument.Range, Argument.ValueRange)
						|| !ValidateAnnotationArguments(Context, Argument.Arguments, Depth + 1, Count, &Argument.ValueRange))
					{
						return false;
					}
				}
				else
				{
					if (!Argument.Arguments.IsEmpty()) return false;
					if (Argument.Kind == asEAnnotationValueKind::Flag)
					{
						if (!Argument.Value.IsEmpty() || Argument.ValueRange.IsOrdered()) return false;
					}
					else if ((Argument.Kind == asEAnnotationValueKind::Scalar && Argument.Value.IsEmpty())
						|| !IsValidRange(Context, Argument.ValueRange)
						|| !IsContainedRange(Argument.Range, Argument.ValueRange))
					{
						return false;
					}
				}
			}
			return true;
		}

		bool ValidateCallableSignature(
			const asCFunctionType& Signature,
			TConstArrayView<asCVarDecl*> Parameters,
			bool bAllowInferredParameterSyntax = false)
		{
			if (Signature.GetParameterTypes().Num() != Parameters.Num()
				|| Signature.GetParameterPassingModes().Num() != Parameters.Num())
			{
				return false;
			}
			for (int32 Index = 0; Index < Parameters.Num(); ++Index)
			{
				const asCVarDecl* Parameter = Parameters[Index];
				if (!Parameter || Parameter->GetResolvedQualType() != Signature.GetParameterTypes()[Index]) return false;
				if (!Parameter->GetTypeSyntax().IsValid())
				{
					if (!bAllowInferredParameterSyntax
						|| Signature.GetParameterPassingModes()[Index] > asEParameterPassingMode::InOut) return false;
					continue;
				}
				asEParameterPassingMode ExpectedMode = asEParameterPassingMode::Value;
				switch (Parameter->GetTypeSyntax().Reference)
				{
				case asETypeReferenceDirection::In: ExpectedMode = asEParameterPassingMode::In; break;
				case asETypeReferenceDirection::Out: ExpectedMode = asEParameterPassingMode::Out; break;
				case asETypeReferenceDirection::InOut: ExpectedMode = asEParameterPassingMode::InOut; break;
				default: break;
				}
				if (Signature.GetParameterPassingModes()[Index] != ExpectedMode) return false;
			}
			return true;
		}

		const asCFunctionType* ResolveCallSignature(
			const asCASTContext& Context,
			const asCExpr* Callee)
		{
			if (!Callee) return nullptr;
			if (const auto* Signature = dyn_cast<asCFunctionType>(Callee->GetExpressionType()))
				return Signature;
			const auto* Nominal = dyn_cast<asCNominalType>(Callee->GetExpressionType());
			if (!Nominal) return nullptr;
			for (asCASTNode* Node : Context.GetNodes())
			{
				const auto* Callable = dyn_cast<asCCallableTypeDecl>(Node);
				if (Callable && Callable->GetStableIdentity() == Nominal->GetDeclarationIdentity())
					return Callable->GetResolvedSignature();
			}
			return nullptr;
		}

		asCQualType ControlValueType(asCQualType Type)
		{
			asCQualType Result;
			asCQualType::TryCreateRaw(Type.GetType(),
				static_cast<uint8>(Type.GetQualifiers()) & ~static_cast<uint8>(asETypeQualifiers::Reference),
				Result);
			return Result;
		}

		bool ControlSameType(asCQualType Left, asCQualType Right)
		{
			if (!Left.IsValid() || !Right.IsValid()
				|| static_cast<uint8>(Left.GetQualifiers()) != static_cast<uint8>(Right.GetQualifiers())) return false;
			if (Left.GetType() == Right.GetType()) return true;
			return Left.GetType()->HasStableIdentity() && Right.GetType()->HasStableIdentity()
				&& Left.GetType()->GetStableIdentity() == Right.GetType()->GetStableIdentity();
		}

		bool ControlSameUnqualifiedType(asCQualType Left, asCQualType Right)
		{
			if (!Left.IsValid() || !Right.IsValid()) return false;
			if (Left.GetType() == Right.GetType()) return true;
			return Left.GetType()->HasStableIdentity() && Right.GetType()->HasStableIdentity()
				&& Left.GetType()->GetStableIdentity() == Right.GetType()->GetStableIdentity();
		}

		bool ControlIsNumeric(const asCASTContext& Context, const asCType* Type)
		{
			if (const auto* Builtin = dyn_cast<asCBuiltinType>(Type))
				return Builtin->GetPrimitive() > asEPrimitiveType::Bool
					&& Builtin->GetPrimitive() <= asEPrimitiveType::Float64;
			const auto* Nominal = dyn_cast<asCNominalType>(Type);
			asSNominalTypeDescriptor Descriptor;
			return Nominal && Context.GetTypeContext()->GetIdentityRegistry().ResolveNominalType(
				Nominal->GetDeclarationIdentity(), Descriptor) == asETypeIdentityStatus::Succeeded
				&& Descriptor.Kind == asETypeDeclKind::Enum;
		}

		bool ControlIsBool(const asCType* Type)
		{
			const auto* Builtin = dyn_cast<asCBuiltinType>(Type);
			return Builtin && Builtin->GetPrimitive() == asEPrimitiveType::Bool;
		}

		bool ControlIsSwitchSelector(const asCASTContext& Context, const asCType* Type)
		{
			if (const auto* Builtin = dyn_cast<asCBuiltinType>(Type))
				return Builtin->GetPrimitive() >= asEPrimitiveType::Int8
					&& Builtin->GetPrimitive() <= asEPrimitiveType::UInt64;
			const auto* Nominal = dyn_cast<asCNominalType>(Type);
			asSNominalTypeDescriptor Descriptor;
			return Nominal && Context.GetTypeContext()->GetIdentityRegistry().ResolveNominalType(
				Nominal->GetDeclarationIdentity(), Descriptor) == asETypeIdentityStatus::Succeeded
				&& Descriptor.Kind == asETypeDeclKind::Enum;
		}

		const asCRecordDecl* ControlProtocolOwner(const asCFunctionDecl* Function)
		{
			const asCDeclContext* Context = Function ? Function->GetDeclContext() : nullptr;
			return Context ? dyn_cast<asCRecordDecl>(Context->GetOwnerDecl()) : nullptr;
		}

		const asCRecordDecl* ControlFindRecord(
			const asCASTContext& Context,
			const asSStableKey& Identity)
		{
			for (asCASTNode* Node : Context.GetNodes())
			{
				const auto* Record = dyn_cast<asCRecordDecl>(Node);
				if (Record && !Record->GetStableIdentity().IsZero() && Record->GetStableIdentity() == Identity)
					return Record;
			}
			return nullptr;
		}

		bool ControlOwnerIsReachableBase(
			const asCASTContext& Context,
			const asCRecordDecl* Container,
			const asCRecordDecl* Owner,
			TSet<const asCRecordDecl*>& Visited)
		{
			if (!Container || !Owner || Visited.Contains(Container)) return false;
			if (Container == Owner || (!Container->GetStableIdentity().IsZero() && !Owner->GetStableIdentity().IsZero()
				&& Container->GetStableIdentity() == Owner->GetStableIdentity())) return true;
			Visited.Add(Container);
			for (const asCType* BaseType : Container->GetResolvedBases())
			{
				if (const auto* Generic = dyn_cast<asCGenericType>(BaseType)) BaseType = Generic->GetTemplateType();
				const auto* Nominal = dyn_cast<asCNominalType>(BaseType);
				if (Nominal && ControlOwnerIsReachableBase(Context,
					ControlFindRecord(Context, Nominal->GetDeclarationIdentity()), Owner, Visited)) return true;
			}
			return false;
		}

		bool ControlProtocolParameterMatches(const asCFunctionDecl* Function, asCQualType Iterator)
		{
			if (!Function || !Function->GetResolvedSignature()
				|| Function->GetResolvedSignature()->GetParameterTypes().Num() != 1
				|| Function->GetResolvedSignature()->GetParameterPassingModes().Num() != 1) return false;
			const asEParameterPassingMode Mode =
				Function->GetResolvedSignature()->GetParameterPassingModes()[0];
			if (Mode != asEParameterPassingMode::Value && Mode != asEParameterPassingMode::In)
				return false;
			return ControlSameType(
				ControlValueType(Function->GetResolvedSignature()->GetParameterTypes()[0]),
				ControlValueType(Iterator));
		}

		bool ControlCanInitialize(
			const asCASTContext& Context,
			asCQualType Target,
			asCQualType Value)
		{
			const asCQualType TargetValue = ControlValueType(Target);
			const asCQualType SourceValue = ControlValueType(Value);
			if (ControlSameType(TargetValue, SourceValue)) return true;
			if (TargetValue.IsHandle() && SourceValue.IsHandle()
				&& ControlSameUnqualifiedType(TargetValue, SourceValue)
				&& (!SourceValue.IsConst() || TargetValue.IsConst())) return true;
			return !TargetValue.IsHandle() && !SourceValue.IsHandle()
				&& ControlIsNumeric(Context, TargetValue.GetType())
				&& ControlIsNumeric(Context, SourceValue.GetType());
		}

		bool ControlValidateForeachProtocol(
			const asCASTContext& Context,
			const asCForeachStmt& Foreach)
		{
			const asSForeachProtocol& Protocol = Foreach.GetProtocol();
			if (!Foreach.GetValueStatement() || !Foreach.GetRangeExpression() || !Foreach.GetBody()
				|| !Protocol.ContainerType.IsValid() || !Protocol.IteratorType.IsValid()
				|| !Protocol.BeginFunction || !Protocol.EndFunction || !Protocol.NextFunction
				|| !Protocol.ValueFunction || ((Foreach.GetKeyStatement() != nullptr) != (Protocol.KeyFunction != nullptr))
				|| !ControlSameType(Protocol.ContainerType, Foreach.GetRangeExpression()->GetExpressionQualType())) return false;
			const asCType* Container = Foreach.GetRangeExpression()->GetExpressionType();
			if (const auto* Generic = dyn_cast<asCGenericType>(Container)) Container = Generic->GetTemplateType();
			const auto* NominalContainer = dyn_cast<asCNominalType>(Container);
			const asCRecordDecl* ContainerRecord = NominalContainer
				? ControlFindRecord(Context, NominalContainer->GetDeclarationIdentity()) : nullptr;
			for (const asCFunctionDecl* Function : {Protocol.BeginFunction, Protocol.EndFunction,
				Protocol.NextFunction, Protocol.ValueFunction, Protocol.KeyFunction})
			{
				if (!Function) continue;
				TSet<const asCRecordDecl*> Visited;
				if (!ControlOwnerIsReachableBase(Context, ContainerRecord,
					ControlProtocolOwner(Function), Visited)) return false;
			}
			if (Protocol.BeginFunction->GetName() != TEXT("opForBegin")
				|| Protocol.EndFunction->GetName() != TEXT("opForEnd")
				|| Protocol.NextFunction->GetName() != TEXT("opForNext")
				|| Protocol.ValueFunction->GetName() != TEXT("opForValue")
				|| (Protocol.KeyFunction && Protocol.KeyFunction->GetName() != TEXT("opForKey"))) return false;
			const asCFunctionType* BeginSignature = Protocol.BeginFunction->GetResolvedSignature();
			const asCFunctionType* EndSignature = Protocol.EndFunction->GetResolvedSignature();
			const asCFunctionType* ValueSignature = Protocol.ValueFunction->GetResolvedSignature();
			if (!BeginSignature || !EndSignature || !ValueSignature
				|| !BeginSignature->GetParameterTypes().IsEmpty()
				|| !ControlSameType(ControlValueType(BeginSignature->GetReturnType()), ControlValueType(Protocol.IteratorType))
				|| !ControlProtocolParameterMatches(Protocol.EndFunction, Protocol.IteratorType)
				|| !ControlProtocolParameterMatches(Protocol.NextFunction, Protocol.IteratorType)
				|| !ControlProtocolParameterMatches(Protocol.ValueFunction, Protocol.IteratorType)
				|| !ControlIsBool(EndSignature->GetReturnType().GetType())
				|| !ControlCanInitialize(Context, Foreach.GetValueStatement()->GetDeclaration()->GetResolvedQualType(), ValueSignature->GetReturnType())) return false;
			if (Protocol.KeyFunction)
			{
				const asCFunctionType* KeySignature = Protocol.KeyFunction->GetResolvedSignature();
				if (!KeySignature || !ControlProtocolParameterMatches(Protocol.KeyFunction, Protocol.IteratorType)
					|| !ControlCanInitialize(Context, Foreach.GetKeyStatement()->GetDeclaration()->GetResolvedQualType(), KeySignature->GetReturnType())) return false;
			}
			return true;
		}

		bool ValidateDeclPayload(
			const asCASTContext& Context,
			const asCDecl& Decl,
			asSASTVerificationResult& Out)
		{
			if (Decl.GetAccess() > asEAccessSpecifier::Private)
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
					TEXT("declaration has an invalid access specifier"));

			if (const auto* Named = dyn_cast<asCNamedDecl>(&Decl); Named && Named->GetName().IsEmpty())
			{
				const auto* Function = dyn_cast<asCFunctionDecl>(&Decl);
				const bool bImplicitFunction = Function
					&& (Function->GetFunctionKind() == asEFunctionKind::Lambda
						|| Function->GetFunctionKind() == asEFunctionKind::ClassDefault);
				if (Decl.GetKind() != asEDeclKind::ParmVar && !bImplicitFunction)
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("named declaration requires a non-empty name"));
			}

			if (const asCDeclContext* Parent = Decl.GetDeclContext())
			{
				const asCDecl* Owner = Parent->GetOwnerDecl();
				if (!Owner || !Context.Owns(Owner))
					return Fail(Out, asEASTVerificationStatus::InvalidDeclarationContext, &Decl,
						TEXT("declaration context owner is absent or foreign"));
			}

			if (const asSAuthoredAccessUse* Access = GetAuthoredAccessUse(Decl);
				Access && !ValidateAuthoredAccessUse(Context, Decl, *Access))
			{
				return Fail(Out, asEASTVerificationStatus::InvalidCrossReference, &Decl,
					TEXT("custom member access does not resolve to its authored record-local policy"));
			}

			for (asCAttr* Attr : Decl.GetAttrs())
			{
				if (!Attr || !Attr->AllowsTarget(Decl.GetKind()))
					return Fail(Out, asEASTVerificationStatus::InvalidAttributeTarget, &Decl,
						TEXT("declaration attribute is missing or rejects its target kind"));
			}

			const asCDeclContext* DeclContext = nullptr;
			switch (Decl.GetKind())
			{
			case asEDeclKind::TranslationUnit: DeclContext = static_cast<const asCTranslationUnitDecl*>(&Decl); break;
			case asEDeclKind::Namespace: DeclContext = static_cast<const asCNamespaceDecl*>(&Decl); break;
			case asEDeclKind::Record:
			{
				const auto& Record = static_cast<const asCRecordDecl&>(Decl);
				constexpr uint8 ValidRecordModifiers =
					static_cast<uint8>(asERecordModifiers::Final)
					| static_cast<uint8>(asERecordModifiers::Abstract)
					| static_cast<uint8>(asERecordModifiers::Mixin)
					| static_cast<uint8>(asERecordModifiers::Local);
				if (Record.GetDeclaredTypeKind() != asETypeDeclKind::Class
					&& Record.GetDeclaredTypeKind() != asETypeDeclKind::Struct
					&& Record.GetDeclaredTypeKind() != asETypeDeclKind::Interface
					&& Record.GetDeclaredTypeKind() != asETypeDeclKind::Enum)
				{
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("record declaration has an invalid nominal type kind"));
				}
				if ((static_cast<uint8>(Record.GetModifiers()) & ~ValidRecordModifiers) != 0)
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("record declaration has invalid modifier bits"));
				DeclContext = &Record;
				break;
			}
			case asEDeclKind::Function:
			{
				const auto& Function = static_cast<const asCFunctionDecl&>(Decl);
				constexpr uint16 ValidFunctionModifiers =
					static_cast<uint16>(asEFunctionModifiers::Final)
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
				if (Function.GetFunctionKind() > asEFunctionKind::Conversion
					|| (static_cast<uint16>(Function.GetModifiers()) & ~ValidFunctionModifiers) != 0)
				{
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("function declaration has an invalid kind or modifier bits"));
				}
				const bool bLambda = Function.GetFunctionKind() == asEFunctionKind::Lambda;
				const bool bClassDefault = Function.GetFunctionKind() == asEFunctionKind::ClassDefault;
				const bool bConversion = Function.GetFunctionKind() == asEFunctionKind::Conversion;
				const bool bImplicitFunction = bLambda || bClassDefault;
				if (Function.GetName().IsEmpty() != bImplicitFunction)
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("implicit and authored functions disagree with their naming contract"));
				if (!Function.GetResolvedSignature()
					|| (!bImplicitFunction && (!ValidateTypeSyntax(Context, Function.GetReturnTypeSyntax())
						|| !IsContainedRange(Function.GetSourceRange(), Function.GetReturnTypeSyntax().Range)))
					|| (bImplicitFunction && Function.GetReturnTypeSyntax().IsValid()))
				{
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("function declaration requires resolved signature and authored return type"));
				}
				if (!Function.GetResolvedReturnType()
					|| Function.GetResolvedSignature()->GetReturnType().GetType() != Function.GetResolvedReturnType()
					|| !ValidateCallableSignature(*Function.GetResolvedSignature(), Function.GetParameters(), bLambda))
				{
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("function declaration and resolved callable signature disagree"));
				}
				if (bClassDefault)
				{
					const auto* ReturnType = dyn_cast<asCBuiltinType>(Function.GetResolvedReturnType());
					if (!ReturnType || ReturnType->GetPrimitive() != asEPrimitiveType::Void
						|| !Function.GetParameters().IsEmpty())
					{
						return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
							TEXT("class-default function requires an implicit void zero-parameter signature"));
					}
				}
				if (bConversion)
				{
					const asCDeclContext* Parent = Function.GetDeclContext();
					const auto* Owner = Parent ? dyn_cast<asCRecordDecl>(Parent->GetOwnerDecl()) : nullptr;
					asSFunctionIdentityDescriptor Descriptor;
					asSStableKey ReturnIdentity;
					if (!Owner || Owner->GetStableIdentity().IsZero()
						|| !IsConversionFunctionName(Function.GetName())
						|| !Function.GetParameters().IsEmpty()
						|| !Function.GetResolvedSignature()->GetParameterTypes().IsEmpty()
						|| Function.GetResolvedSignature()->GetReturnType().GetStableIdentity(
							Context.GetTypeContext()->GetIdentityRegistry(), ReturnIdentity)
							!= asETypeIdentityStatus::Succeeded
						|| Context.GetTypeContext()->GetIdentityRegistry().ResolveFunction(
							Function.GetStableIdentity(), Descriptor) != asETypeIdentityStatus::Succeeded
						|| Descriptor.Kind != asEFunctionIdentityKind::Conversion
						|| Descriptor.Name != Function.GetName()
						|| Descriptor.Owner != Owner->GetStableIdentity()
						|| Descriptor.ReturnType != ReturnIdentity
						|| Descriptor.ConversionDestination != ReturnIdentity
						|| !Descriptor.Parameters.IsEmpty())
					{
						return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
							TEXT("conversion function does not authenticate its owner and destination identity"));
					}
				}
				if (Function.HasDeferredBody()
					&& (!IsValidRange(Context, Function.GetDeferredBodyRange())
						|| !IsContainedRange(Function.GetSourceRange(), Function.GetDeferredBodyRange())))
					return Fail(Out, asEASTVerificationStatus::InvalidSourceRange, &Decl,
						TEXT("function deferred body range is invalid"));
				if (Function.GetFunctionKind() == asEFunctionKind::Lambda)
				{
					if (!Function.GetEnclosingFunction() || Function.GetLambdaOrdinal() == MAX_uint32)
						return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
							TEXT("lambda function requires a stable enclosing coordinate"));
					for (asCVarDecl* Capture : Function.GetCaptures())
						if (!Capture || !Context.Owns(Capture))
							return Fail(Out, asEASTVerificationStatus::InvalidCrossReference, &Decl,
								TEXT("lambda capture is absent or foreign"));
				}
				else if (Function.GetEnclosingFunction() || Function.GetLambdaOrdinal() != MAX_uint32
					|| !Function.GetCaptures().IsEmpty() || Function.GetCapturedThisRecord())
				{
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("non-lambda function carries lambda-only source state"));
				}
				if (Function.GetFunctionKind() == asEFunctionKind::ClassDefault)
				{
					for (const asCSourceRange& Range : Function.GetDeferredStatementRanges())
						if (!IsValidRange(Context, Range) || !IsContainedRange(Function.GetSourceRange(), Range))
							return Fail(Out, asEASTVerificationStatus::InvalidSourceRange, &Decl,
								TEXT("class-default statement range is invalid"));
				}
				else if (!Function.GetDeferredStatementRanges().IsEmpty())
				{
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("non-class-default function carries class-default source state"));
				}
				DeclContext = &Function;
				break;
			}
			case asEDeclKind::Delegate:
			case asEDeclKind::Event:
			{
				const auto& Callable = static_cast<const asCCallableTypeDecl&>(Decl);
				if (!Callable.GetResolvedSignature()
					|| !ValidateTypeSyntax(Context, Callable.GetReturnTypeSyntax())
					|| !IsContainedRange(Callable.GetSourceRange(), Callable.GetReturnTypeSyntax().Range))
				{
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("callable declaration requires resolved signature and authored return type"));
				}
				if (!ValidateCallableSignature(*Callable.GetResolvedSignature(), Callable.GetParameters()))
				{
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("callable declaration and resolved signature parameters disagree"));
				}
				DeclContext = &Callable;
				break;
			}
			case asEDeclKind::TypeAlias:
			{
				const auto& Alias = static_cast<const asCTypeAliasDecl&>(Decl);
				const auto* Primitive = dyn_cast<asCBuiltinType>(Alias.GetResolvedAliasedType());
				const asSTypeSyntax& Syntax = Alias.GetTypeSyntax();
				if (!ValidateTypeSyntax(Context, Syntax)
					|| !IsContainedRange(Alias.GetSourceRange(), Syntax.Range)
					|| !Primitive
					|| Primitive->GetPrimitive() <= asEPrimitiveType::Void
					|| Primitive->GetPrimitive() > asEPrimitiveType::Float64
					|| Syntax.bConst || Syntax.bHandle
					|| Syntax.Reference != asETypeReferenceDirection::None
					|| !Syntax.GenericArguments.IsEmpty()
					|| !Syntax.ArrayDimensions.IsEmpty())
				{
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("type alias requires an unqualified non-void primitive target"));
				}
				break;
			}
			case asEDeclKind::AccessSpecifier:
				if (!ValidateAccessSpecifier(Context, static_cast<const asCAccessSpecifierDecl&>(Decl)))
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("custom access policy has an invalid owner, payload, or authored range"));
				break;
			case asEDeclKind::Var:
			case asEDeclKind::ParmVar:
			{
				const auto& Variable = static_cast<const asCVarDecl&>(Decl);
				const bool bMissingSyntax = !Variable.GetTypeSyntax().IsValid();
				const asCDecl* ParentOwner = Variable.GetDeclContext()
					? Variable.GetDeclContext()->GetOwnerDecl() : nullptr;
				const auto* ParentFunction = dyn_cast<asCFunctionDecl>(ParentOwner);
				const bool bInferredLambdaParameter = Variable.GetKind() == asEDeclKind::ParmVar
					&& ParentFunction && ParentFunction->GetFunctionKind() == asEFunctionKind::Lambda;
				if (!Variable.GetResolvedType() || (bMissingSyntax && !bInferredLambdaParameter)
					|| (Variable.GetTypeSyntax().IsValid()
						&& (!ValidateTypeSyntax(Context, Variable.GetTypeSyntax())
							|| !IsContainedRange(Variable.GetSourceRange(), Variable.GetTypeSyntax().Range))))
				{
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("variable declaration requires a valid resolved type"));
				}
				if (Variable.HasDeferredInitializer()
					&& (!IsValidRange(Context, Variable.GetDeferredInitializerRange())
						|| !IsContainedRange(Decl.GetSourceRange(), Variable.GetDeferredInitializerRange())))
				{
					return Fail(Out, asEASTVerificationStatus::InvalidSourceRange, &Decl,
						TEXT("variable deferred initializer range is invalid"));
				}
				if (Variable.HasDeferredInitializer() && !Variable.GetInitializer())
				{
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("variable deferred initializer was not materialized before sealing"));
				}
				if (Variable.GetInitializer() && Variable.HasDeferredInitializer()
					&& !IsContainedRange(Variable.GetDeferredInitializerRange(),
						Variable.GetInitializer()->GetSourceRange()))
				{
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("variable initializer is outside its retained authored range"));
				}
				break;
			}
			case asEDeclKind::EnumConstant:
			{
				const auto& Constant = static_cast<const asCEnumConstantDecl&>(Decl);
				if (!Constant.HasResolvedValue())
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("enum constant value was not resolved before sealing"));
				if (Constant.HasDeferredInitializer()
					&& (!IsValidRange(Context, Constant.GetDeferredInitializerRange())
						|| !IsContainedRange(Decl.GetSourceRange(), Constant.GetDeferredInitializerRange())))
					return Fail(Out, asEASTVerificationStatus::InvalidSourceRange, &Decl,
						TEXT("enum constant initializer range is invalid"));
				if (Constant.HasDeferredInitializer() && !Constant.GetInitializer())
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("enum constant initializer was not materialized before sealing"));
				if (Constant.GetInitializer() && Constant.HasDeferredInitializer()
					&& !IsContainedRange(Constant.GetDeferredInitializerRange(), Constant.GetInitializer()->GetSourceRange()))
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Decl,
						TEXT("enum constant initializer is outside its authored range"));
				break;
			}
			case asEDeclKind::Recovery: break;
			default:
				return Fail(Out, asEASTVerificationStatus::UnhandledKind, &Decl,
					TEXT("declaration kind is not handled by structural verification"));
			}

			if (DeclContext)
			{
				for (asCDecl* Child : DeclContext->GetDecls())
				{
					if (!Child || Child->GetDeclContext() != DeclContext)
						return Fail(Out, asEASTVerificationStatus::InvalidDeclarationContext, &Decl,
							TEXT("lexical declaration child has a mismatched declaration context"));
				}
			}
			return true;
		}

		bool ValidateStmtPayload(
			const asCASTContext& Context,
			const asCStmt& Stmt,
			asSASTVerificationResult& Out)
		{
			switch (Stmt.GetKind())
			{
			case asEStmtKind::Compound:
			case asEStmtKind::Return:
			case asEStmtKind::Break:
			case asEStmtKind::Continue:
			case asEStmtKind::RecoveryStmt:
			case asEStmtKind::NullStmt:
			case asEStmtKind::DeclRefExpr:
			case asEStmtKind::RecoveryExpr:
				return true;
			case asEStmtKind::IntegerLiteral:
			{
				const auto& Literal = static_cast<const asCIntegerLiteral&>(Stmt);
				if (!Literal.GetExpressionType()) return true;
				const auto* Type = dyn_cast<asCBuiltinType>(Literal.GetExpressionType());
				if (!Type) break;
				const int64 Signed = Literal.GetValue();
				const uint64 Unsigned = Literal.GetUnsignedValue();
				switch (Type->GetPrimitive())
				{
				case asEPrimitiveType::Int8: if (Signed >= -128 && Signed <= 127) return true; break;
				case asEPrimitiveType::Int16: if (Signed >= -32768 && Signed <= 32767) return true; break;
				case asEPrimitiveType::Int32: if (Signed >= MIN_int32 && Signed <= MAX_int32) return true; break;
				case asEPrimitiveType::Int64: return true;
				case asEPrimitiveType::UInt8: if (Unsigned <= MAX_uint8) return true; break;
				case asEPrimitiveType::UInt16: if (Unsigned <= MAX_uint16) return true; break;
				case asEPrimitiveType::UInt32: if (Unsigned <= MAX_uint32) return true; break;
				case asEPrimitiveType::UInt64: return true;
				default: break;
				}
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("integer literal payload is not representable by its semantic type"));
			}
			case asEStmtKind::BooleanLiteral:
			{
				const auto& Literal = static_cast<const asCBooleanLiteral&>(Stmt);
				if (!Literal.GetExpressionType()) return true;
				const auto* Type = dyn_cast<asCBuiltinType>(Literal.GetExpressionType());
				if (Type && Type->GetPrimitive() == asEPrimitiveType::Bool) return true;
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("boolean literal requires Bool semantic type"));
			}
			case asEStmtKind::FloatingLiteral:
			{
				const auto& Literal = static_cast<const asCFloatingLiteral&>(Stmt);
				if (!Literal.GetExpressionType()) return true;
				const auto* Type = dyn_cast<asCBuiltinType>(Literal.GetExpressionType());
				if (Type && (Type->GetPrimitive() == asEPrimitiveType::Float32
						|| Type->GetPrimitive() == asEPrimitiveType::Float64)
					&& FMath::IsFinite(Literal.GetValue())) return true;
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("floating literal requires a finite floating semantic payload"));
			}
			case asEStmtKind::If:
			{
				const auto& If = static_cast<const asCIfStmt&>(Stmt);
				if (If.GetCondition() && If.GetThen()) return true;
				break;
			}
			case asEStmtKind::While:
			{
				const auto& While = static_cast<const asCWhileStmt&>(Stmt);
				if (While.GetCondition() && While.GetBody()) return true;
				break;
			}
			case asEStmtKind::For:
			{
				const auto& For = static_cast<const asCForStmt&>(Stmt);
				if (!For.GetBody() || !IsValidRange(Context, For.GetHeaderRange())
					|| !IsContainedRange(Stmt.GetSourceRange(), For.GetHeaderRange())) break;
				for (asCStmt* Initializer : For.GetInitializers()) if (!Initializer) return Fail(
					Out, asEASTVerificationStatus::MissingRequiredChild, &Stmt,
					TEXT("for statement contains an absent initializer phase"));
				for (asCExpr* Increment : For.GetIncrements()) if (!Increment) return Fail(
					Out, asEASTVerificationStatus::MissingRequiredChild, &Stmt,
					TEXT("for statement contains an absent increment phase"));
				return true;
			}
			case asEStmtKind::Do:
			{
				const auto& Do = static_cast<const asCDoStmt&>(Stmt);
				if (Do.GetBody() && Do.GetCondition()) return true;
				break;
			}
			case asEStmtKind::DeclStmt:
				if (static_cast<const asCDeclStmt&>(Stmt).GetDeclaration()) return true;
				break;
			case asEStmtKind::CallExpr:
			{
				const auto& Call = static_cast<const asCCallExpr&>(Stmt);
				const asCFunctionType* Signature = ResolveCallSignature(Context, Call.GetCallee());
				if (!Signature || Signature->GetParameterTypes().Num() != Call.GetArguments().Num())
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
						TEXT("call expression does not carry a resolved callable signature"));
				const TConstArrayView<int32> Ordinals = Call.GetArgumentSourceOrdinals();
				if (Ordinals.Num() != Call.GetArguments().Num())
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
						TEXT("call argument-source mapping has the wrong size"));
				TSet<int32> Authored;
				for (int32 Index = 0; Index < Ordinals.Num(); ++Index)
				{
					const int32 Ordinal = Ordinals[Index];
					const bool bDefault = isa<asCDefaultArgumentExpr>(Call.GetArguments()[Index]);
					if ((Ordinal == INDEX_NONE) != bDefault
						|| Ordinal < INDEX_NONE || Ordinal >= Ordinals.Num()
						|| (Ordinal != INDEX_NONE && Authored.Contains(Ordinal)))
					{
						return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
							TEXT("call argument-source mapping is not a unique authored permutation"));
					}
					if (Ordinal != INDEX_NONE) Authored.Add(Ordinal);
				}
				for (int32 Ordinal = 0; Ordinal < Authored.Num(); ++Ordinal)
					if (!Authored.Contains(Ordinal))
						return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
							TEXT("call argument-source mapping has a gap"));
				return true;
			}
			case asEStmtKind::ImplicitCastExpr:
			{
				const auto& Cast = static_cast<const asCImplicitCastExpr&>(Stmt);
				const asCExpr* Operand = Cast.GetOperand();
				const asCQualType Source = Operand ? Operand->GetExpressionQualType() : asCQualType{};
				const asCQualType Target = Cast.GetExpressionQualType();
				if (!Operand || !Source.IsValid() || !Target.IsValid())
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
						TEXT("implicit cast requires typed source and target expressions"));
				switch (Cast.GetConversionKind())
				{
				case asEImplicitConversionKind::Numeric:
					if (!Source.IsHandle() && !Source.IsReference()
						&& !Target.IsHandle() && !Target.IsReference()
						&& ControlIsNumeric(Context, Source.GetType()) && ControlIsNumeric(Context, Target.GetType())
						&& Cast.GetValueCategory() == asEValueCategory::PRValue) return true;
					break;
				case asEImplicitConversionKind::LValueToRValue:
					if (Operand->GetValueCategory() == asEValueCategory::LValue
						&& Cast.GetValueCategory() == asEValueCategory::PRValue
						&& ControlSameUnqualifiedType(Source, Target)
						&& Source.IsHandle() == Target.IsHandle()) return true;
					break;
				case asEImplicitConversionKind::NullToHandle:
					if (isa<asCNullPtrLiteral>(Operand) && Target.IsHandle() && !Target.IsReference()
						&& Cast.GetValueCategory() == asEValueCategory::PRValue
						&& ControlSameType(Source, Target)) return true;
					break;
				case asEImplicitConversionKind::ReferenceBinding:
				{
					const uint8 SourceQualifiers = static_cast<uint8>(Source.GetQualifiers());
					const uint8 TargetQualifiers = static_cast<uint8>(Target.GetQualifiers());
					constexpr uint8 ConstBit = static_cast<uint8>(asETypeQualifiers::Const);
					constexpr uint8 HandleBit = static_cast<uint8>(asETypeQualifiers::Handle);
					constexpr uint8 ReferenceBit = static_cast<uint8>(asETypeQualifiers::Reference);
					if (isa<asCThisExpr>(Operand)
						&& Operand->GetValueCategory() == asEValueCategory::PRValue
						&& Cast.GetValueCategory() == asEValueCategory::LValue
						&& (SourceQualifiers & ~ConstBit) == HandleBit
						&& (TargetQualifiers & ~ConstBit) == ReferenceBit
						&& (SourceQualifiers & ConstBit) == (TargetQualifiers & ConstBit)
						&& ControlSameUnqualifiedType(Source, Target)) return true;
					break;
				}
				default: break;
				}
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("implicit cast payload does not authenticate its conversion semantics"));
			}
			case asEStmtKind::UserConversionExpr:
			{
				const auto& Conversion = static_cast<const asCUserConversionExpr&>(Stmt);
				const asCExpr* Operand = Conversion.GetOperand();
				const asCFunctionDecl* Function = Conversion.GetConversionFunction();
				const asCFunctionType* Signature = Function ? Function->GetResolvedSignature() : nullptr;
				const asCDeclContext* Parent = Function ? Function->GetDeclContext() : nullptr;
				const auto* Owner = Parent ? dyn_cast<asCRecordDecl>(Parent->GetOwnerDecl()) : nullptr;
				const asCType* OperandType = Operand ? Operand->GetExpressionType() : nullptr;
				if (const auto* Generic = dyn_cast<asCGenericType>(OperandType)) OperandType = Generic->GetTemplateType();
				const auto* Nominal = dyn_cast<asCNominalType>(OperandType);
				const asCRecordDecl* OperandRecord = Nominal
					? ControlFindRecord(Context, Nominal->GetDeclarationIdentity()) : nullptr;
				TSet<const asCRecordDecl*> VisitedOwners;
				asSFunctionIdentityDescriptor Descriptor;
				asSStableKey TargetIdentity;
				const asCQualType Target = Conversion.GetExpressionQualType();
				const bool bExpectedCategory = Target.IsReference()
					? Conversion.GetValueCategory() == asEValueCategory::LValue
					: Conversion.GetValueCategory() == asEValueCategory::PRValue;
				if (!Operand || !Function || !Signature || !Owner || !Nominal
					|| Conversion.GetMode() > asEUserConversionMode::Explicit
					|| Function->GetFunctionKind() != asEFunctionKind::Conversion
					|| !IsConversionFunctionName(Function->GetName())
					|| (Conversion.GetMode() == asEUserConversionMode::Implicit
						&& !IsImplicitConversionFunctionName(Function->GetName()))
					|| !ControlOwnerIsReachableBase(Context, OperandRecord, Owner, VisitedOwners)
					|| ((Operand->IsReadOnly() || Operand->GetExpressionQualType().IsConst())
						&& !Function->IsConstMethod())
					|| !Signature->GetParameterTypes().IsEmpty()
					|| !ControlSameType(Target, Signature->GetReturnType())
					|| Function->GetResolvedReturnType() != Target.GetType()
					|| !bExpectedCategory || Conversion.IsReadOnly() != Target.IsConst()
					|| Target.GetStableIdentity(Context.GetTypeContext()->GetIdentityRegistry(), TargetIdentity)
						!= asETypeIdentityStatus::Succeeded
					|| Context.GetTypeContext()->GetIdentityRegistry().ResolveFunction(
						Function->GetStableIdentity(), Descriptor) != asETypeIdentityStatus::Succeeded
					|| Descriptor.Kind != asEFunctionIdentityKind::Conversion
					|| Descriptor.Owner != Owner->GetStableIdentity()
					|| Descriptor.Name != Function->GetName()
					|| Descriptor.ReturnType != TargetIdentity
					|| Descriptor.ConversionDestination != TargetIdentity
					|| !Descriptor.Parameters.IsEmpty())
				{
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
						TEXT("user conversion does not authenticate its operand, selected function, mode, or destination"));
				}
				return true;
			}
			case asEStmtKind::ParenExpr:
				if (static_cast<const asCParenExpr&>(Stmt).GetSubExpr()) return true;
				break;
			case asEStmtKind::UnaryOperator:
			{
				const auto& Unary = static_cast<const asCUnaryOperator&>(Stmt);
				if (Unary.GetOperand()
					&& Unary.GetOperator() <= asEUnaryOperator::PostDecrement
					&& IsValidRange(Context, Unary.GetOperatorRange())
					&& IsContainedRange(Stmt.GetSourceRange(), Unary.GetOperatorRange())) return true;
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("unary operator has invalid payload or operator range"));
			}
			case asEStmtKind::BinaryOperator:
			{
				const auto& Binary = static_cast<const asCBinaryOperator&>(Stmt);
				if (Binary.GetLeft() && Binary.GetRight()
					&& Binary.GetOperator() <= asEBinaryOperator::BitwiseOrAssign
					&& IsValidRange(Context, Binary.GetOperatorRange())
					&& IsContainedRange(Stmt.GetSourceRange(), Binary.GetOperatorRange())) return true;
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("binary operator has invalid payload or operator range"));
			}
			case asEStmtKind::ConditionalOperator:
			{
				const auto& Conditional = static_cast<const asCConditionalOperator&>(Stmt);
				if (Conditional.GetCondition() && Conditional.GetTrueExpr() && Conditional.GetFalseExpr()) return true;
				break;
			}
			case asEStmtKind::MemberExpr:
			{
				const auto& Member = static_cast<const asCMemberExpr&>(Stmt);
				if (Member.GetBase() && Member.GetMember()
					&& IsValidRange(Context, Member.GetMemberRange())
					&& IsContainedRange(Stmt.GetSourceRange(), Member.GetMemberRange())) return true;
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("member expression has an invalid base, declaration, or member range"));
			}
			case asEStmtKind::ArraySubscriptExpr:
			{
				const auto& Subscript = static_cast<const asCArraySubscriptExpr&>(Stmt);
				if (Subscript.GetBase() && !Subscript.GetArguments().IsEmpty()) return true;
				break;
			}
			case asEStmtKind::ExplicitCastExpr:
			{
				const auto& Cast = static_cast<const asCExplicitCastExpr&>(Stmt);
				if (Cast.GetOperand() && ValidateTypeSyntax(Context, Cast.GetTypeSyntax())
					&& IsContainedRange(Stmt.GetSourceRange(), Cast.GetTypeSyntax().Range)) return true;
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("explicit cast has an invalid operand or authored target type"));
			}
			case asEStmtKind::ConstructExpr:
			{
				const auto& Construct = static_cast<const asCConstructExpr&>(Stmt);
				if (ValidateTypeSyntax(Context, Construct.GetTypeSyntax())
					&& IsContainedRange(Stmt.GetSourceRange(), Construct.GetTypeSyntax().Range)) return true;
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("construction expression has invalid authored type syntax"));
			}
			case asEStmtKind::StringLiteral:
				return true; // the decoded value may intentionally be empty
			case asEStmtKind::NullPtrLiteral:
			{
				const asCQualType Type = static_cast<const asCNullPtrLiteral&>(Stmt).GetExpressionQualType();
				if (Type.IsValid() && Type.IsHandle()
					&& static_cast<const asCExpr&>(Stmt).GetValueCategory() == asEValueCategory::PRValue)
					return true;
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("null literal requires a contextual handle type"));
			}
			case asEStmtKind::DefaultArgumentExpr:
			{
				const auto& Default = static_cast<const asCDefaultArgumentExpr&>(Stmt);
				const asCVarDecl* Parameter = Default.GetParameter();
				asCQualType ExpectedValueType;
				const asCQualType ParameterType = Parameter ? Parameter->GetResolvedQualType() : asCQualType{};
				const uint8 ValueQualifiers = static_cast<uint8>(ParameterType.GetQualifiers())
					& ~static_cast<uint8>(asETypeQualifiers::Reference);
				if (Parameter && Parameter->GetKind() == asEDeclKind::ParmVar
					&& Parameter->GetInitializer()
					&& asCQualType::TryCreateRaw(ParameterType.GetType(), ValueQualifiers, ExpectedValueType)
					&& Default.GetExpressionQualType() == ExpectedValueType
					&& Default.GetValueCategory() == asEValueCategory::PRValue) return true;
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("default argument does not reference a typed parameter initializer"));
			}
			case asEStmtKind::ThisExpr:
			{
				const auto& This = static_cast<const asCThisExpr&>(Stmt);
				const auto* Nominal = dyn_cast<asCNominalType>(This.GetExpressionType());
				const asCQualType Type = This.GetExpressionQualType();
				const uint8 Qualifiers = static_cast<uint8>(Type.GetQualifiers());
				constexpr uint8 ConstBit = static_cast<uint8>(asETypeQualifiers::Const);
				constexpr uint8 HandleBit = static_cast<uint8>(asETypeQualifiers::Handle);
				if (This.GetRecord() && Nominal && !This.GetRecord()->GetStableIdentity().IsZero()
					&& Nominal->GetDeclarationIdentity() == This.GetRecord()->GetStableIdentity()
					&& Type.IsHandle() && !Type.IsReference() && !Type.IsNullable()
					&& (Qualifiers & ~(ConstBit | HandleBit)) == 0
					&& This.GetValueCategory() == asEValueCategory::PRValue
					&& This.IsReadOnly() == Type.IsConst()) return true;
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("this expression does not reference its canonical record type"));
			}
			case asEStmtKind::LambdaExpr:
			{
				const auto& Lambda = static_cast<const asCLambdaExpr&>(Stmt);
				const asCFunctionDecl* Function = Lambda.GetFunction();
				bool bTypeMatches = Function && Lambda.GetExpressionType() == Function->GetResolvedSignature();
				if (!bTypeMatches && Function)
				{
					const auto* Nominal = dyn_cast<asCNominalType>(Lambda.GetExpressionType());
					asSNominalTypeDescriptor Descriptor;
					if (Nominal && Context.GetTypeContext()->GetIdentityRegistry().ResolveNominalType(
							Nominal->GetDeclarationIdentity(), Descriptor) == asETypeIdentityStatus::Succeeded
						&& (Descriptor.Kind == asETypeDeclKind::Delegate || Descriptor.Kind == asETypeDeclKind::Event))
					{
						for (asCASTNode* Candidate : Context.GetNodes())
						{
							const auto* Callable = dyn_cast<asCCallableTypeDecl>(Candidate);
							if (Callable && Callable->GetStableIdentity() == Nominal->GetDeclarationIdentity()
								&& Callable->GetResolvedSignature() == Function->GetResolvedSignature())
							{
								bTypeMatches = true;
								break;
							}
						}
					}
				}
				if (Function && Function->GetFunctionKind() == asEFunctionKind::Lambda && bTypeMatches) return true;
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("lambda expression requires its owned typed lambda function"));
			}
			case asEStmtKind::InitListExpr:
			{
				const auto& Init = static_cast<const asCInitListExpr&>(Stmt);
				asEStableIdentityRole InitializerRole = asEStableIdentityRole::Invalid;
				if (Context.GetTypeContext()->GetIdentityRegistry().ResolveIdentityRole(
						Init.GetInitializerIdentity(), InitializerRole) != asETypeIdentityStatus::Succeeded
					|| InitializerRole != asEStableIdentityRole::Function
					|| Init.GetElementPatternIndices().Num() != Init.GetElements().Num())
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
						TEXT("initializer list requires a typed initializer binding"));
				for (int32 Index : Init.GetElementPatternIndices())
				{
					if (Index < 0)
						return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
							TEXT("initializer list element mapping contains an invalid pattern index"));
				}
				for (asCExpr* Element : Init.GetElements()) if (!Element)
					return Fail(Out, asEASTVerificationStatus::MissingRequiredChild, &Stmt,
						TEXT("initializer list contains an absent element"));
				return true;
			}
			case asEStmtKind::ValueInitExpr:
				return true;
			case asEStmtKind::DeclGroupStmt:
			{
				const auto Declarations = static_cast<const asCDeclGroupStmt&>(Stmt).GetDeclarations();
				if (Declarations.Num() < 2) return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("declaration group must preserve multiple authored declarations"));
				for (asCStmt* Declaration : Declarations)
					if (!Declaration || (!isa<asCDeclStmt>(Declaration) && !isa<asCRecoveryStmt>(Declaration)))
						return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
							TEXT("declaration group contains a non-declaration statement"));
				return true;
			}
			case asEStmtKind::ClassDefaultStmt:
			{
				const auto& Default = static_cast<const asCClassDefaultStmt&>(Stmt);
				if (Default.GetStatement() && IsValidRange(Context, Default.GetKeywordRange())
					&& IsContainedRange(Stmt.GetSourceRange(), Default.GetKeywordRange())) return true;
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("class-default statement requires its authored keyword and statement"));
			}
			case asEStmtKind::Switch:
			{
				const auto& Switch = static_cast<const asCSwitchStmt&>(Stmt);
				if (!Switch.GetSelector() || !ControlIsSwitchSelector(Context, Switch.GetSelector()->GetExpressionType())
					|| !IsValidRange(Context, Switch.GetHeaderRange())
					|| !IsContainedRange(Stmt.GetSourceRange(), Switch.GetHeaderRange()))
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
						TEXT("switch requires an integral or enum selector and authored header"));
				TSet<uint64> Values;
				int32 DefaultCount = 0;
				for (int32 ClauseIndex = 0; ClauseIndex < Switch.GetClauses().Num(); ++ClauseIndex)
				{
					const asCSwitchClause* Clause = Switch.GetClauses()[ClauseIndex];
					if (!Clause || Clause->GetParentSwitch() != &Switch
						|| !IsContainedRange(Stmt.GetSourceRange(), Clause->GetSourceRange())
						|| !IsValidRange(Context, Clause->GetKeywordRange())
						|| !IsValidRange(Context, Clause->GetColonRange())
						|| !IsContainedRange(Clause->GetSourceRange(), Clause->GetKeywordRange())
						|| !IsContainedRange(Clause->GetSourceRange(), Clause->GetColonRange()))
						return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
							TEXT("switch clause does not belong to its authored switch"));
					if (const auto* Case = dyn_cast<asCCaseStmt>(Clause))
					{
						if (!Case->GetLabel() || !Case->HasResolvedValue())
							return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
								TEXT("case clause requires a resolved integral label"));
						const asSIntegralConstantEvaluation Evaluation = asEvaluateIntegralConstant(*Case->GetLabel());
						uint64 Converted = 0;
						if (Evaluation.Status != asEConstantEvaluationStatus::Succeeded
							|| !asTryConvertIntegralConstant(Evaluation, Switch.GetSelector()->GetExpressionQualType(), Converted)
							|| Converted != Case->GetResolvedValueBits() || Values.Contains(Converted))
							return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
								TEXT("case label value is invalid, unauthenticated, or duplicated"));
						Values.Add(Converted);
					}
					else
					{
						++DefaultCount;
					}
					for (int32 StatementIndex = 0; StatementIndex < Clause->GetStatements().Num(); ++StatementIndex)
					{
						asCStmt* Statement = Clause->GetStatements()[StatementIndex];
						if (!Statement || isa<asCDeclStmt>(Statement) || isa<asCDeclGroupStmt>(Statement))
							return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
								TEXT("switch clause declarations require an explicit compound scope"));
						if (const auto* Fallthrough = dyn_cast<asCFallthroughStmt>(Statement))
						{
							if (StatementIndex + 1 != Clause->GetStatements().Num()
								|| ClauseIndex + 1 >= Switch.GetClauses().Num()
								|| Fallthrough->GetTarget() != Switch.GetClauses()[ClauseIndex + 1])
								return Fail(Out, asEASTVerificationStatus::InvalidCrossReference, &Stmt,
									TEXT("fallthrough must terminate a clause and target its immediate successor"));
						}
					}
				}
				if (Switch.GetClauses().IsEmpty() || DefaultCount > 1)
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
						TEXT("switch requires at least one clause and at most one default"));
				return true;
			}
			case asEStmtKind::Case:
			case asEStmtKind::Default:
			{
				const auto& Clause = static_cast<const asCSwitchClause&>(Stmt);
				if (Clause.GetParentSwitch()) return true;
				break;
			}
			case asEStmtKind::Fallthrough:
				if (static_cast<const asCFallthroughStmt&>(Stmt).GetTarget()) return true;
				break;
			case asEStmtKind::Foreach:
			{
				const auto& Foreach = static_cast<const asCForeachStmt&>(Stmt);
				if (IsValidRange(Context, Foreach.GetHeaderRange())
					&& IsValidRange(Context, Foreach.GetColonRange())
					&& IsContainedRange(Stmt.GetSourceRange(), Foreach.GetHeaderRange())
					&& IsContainedRange(Foreach.GetHeaderRange(), Foreach.GetColonRange())
					&& ControlValidateForeachProtocol(Context, Foreach)) return true;
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Stmt,
					TEXT("foreach statement has incomplete authored or protocol semantics"));
			}
			default:
				return Fail(Out, asEASTVerificationStatus::UnhandledKind, &Stmt,
					TEXT("statement kind is not handled by structural verification"));
			}
			return Fail(Out, asEASTVerificationStatus::MissingRequiredChild, &Stmt,
				TEXT("typed statement is missing a required owning child"));
		}

		bool ValidateTypePayload(
			const asCASTContext& Context,
			const asCType& Type,
			asSASTVerificationResult& Out)
		{
			if (!Type.HasStableIdentity() || !asCASTVerifierAccess::ValidateTypeIdentity(Context, Type))
				return Fail(Out, asEASTVerificationStatus::InvalidSemanticReference, &Type,
					TEXT("canonical type payload does not match its registry-owned identity"));
			return true;
		}

		bool ValidateAttrPayload(
			const asCASTContext& Context,
			const asCAttr& Attr,
			asSASTVerificationResult& Out)
		{
			const uint8 TargetBits = static_cast<uint8>(Attr.GetAllowedTargets());
			if (TargetBits == 0 || (TargetBits & ~static_cast<uint8>(asEAttrTarget::Any)) != 0)
				return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Attr,
					TEXT("attribute target mask is empty or invalid"));
			if (const auto* Flag = dyn_cast<asCFlagAttr>(&Attr))
				return !Flag->GetName().IsEmpty() || Fail(Out, asEASTVerificationStatus::InvalidPayload, &Attr,
					TEXT("flag attribute requires a name"));
			if (const auto* String = dyn_cast<asCStringAttr>(&Attr))
				return !String->GetName().IsEmpty()
					|| Fail(Out, asEASTVerificationStatus::InvalidPayload, &Attr,
						TEXT("string attribute requires a name"));
			if (const auto* Annotation = dyn_cast<asCAnnotationAttr>(&Attr))
			{
				if (Annotation->GetAnnotationKind() > asEAnnotationKind::UMeta)
					return Fail(Out, asEASTVerificationStatus::InvalidPayload, &Attr,
						TEXT("annotation attribute kind is invalid"));
				uint32 Count = 0;
				return ValidateAnnotationArguments(Context, Annotation->GetArguments(), 0, Count)
					|| Fail(Out, asEASTVerificationStatus::InvalidPayload, &Attr,
						TEXT("annotation arguments are structurally invalid"));
			}
			return Fail(Out, asEASTVerificationStatus::UnhandledKind, &Attr,
				TEXT("attribute kind is not handled by structural verification"));
		}

		bool ValidateCrossReference(
			const asCASTContext& Context,
			const asSASTCrossReference& Reference,
			const TSet<const asCASTNode*>& Active)
		{
			if (!Reference.Target)
				return !Reference.Identity.IsZero() && Reference.Kind == asEASTCrossReferenceKind::Declaration;
			if (!Context.Owns(Reference.Target)) return false;
			switch (Reference.Kind)
			{
			case asEASTCrossReferenceKind::Declaration:
				return isa<asCDecl>(Reference.Target);
			case asEASTCrossReferenceKind::ReturnTarget:
				return isa<asCFunctionDecl>(Reference.Target) && Active.Contains(Reference.Target);
			case asEASTCrossReferenceKind::BreakTarget:
				if (!Active.Contains(Reference.Target) || !isa<asCStmt>(Reference.Target)) return false;
				switch (static_cast<const asCStmt*>(Reference.Target)->GetKind())
				{
				case asEStmtKind::While:
				case asEStmtKind::For:
				case asEStmtKind::Do:
				case asEStmtKind::Foreach:
				case asEStmtKind::Switch: return true;
				default: return false;
				}
			case asEASTCrossReferenceKind::ContinueTarget:
				if (!Active.Contains(Reference.Target) || !isa<asCStmt>(Reference.Target)) return false;
				switch (static_cast<const asCStmt*>(Reference.Target)->GetKind())
				{
				case asEStmtKind::While:
				case asEStmtKind::For:
				case asEStmtKind::Do:
				case asEStmtKind::Foreach: return true;
				default: return false;
				}
			case asEASTCrossReferenceKind::MemberDeclaration:
				return isa<asCNamedDecl>(Reference.Target);
			case asEASTCrossReferenceKind::OperatorFunction:
				return isa<asCFunctionDecl>(Reference.Target);
			case asEASTCrossReferenceKind::Constructor:
			{
				const auto* Constructor = dyn_cast<asCFunctionDecl>(Reference.Target);
				return Constructor && Constructor->GetFunctionKind() == asEFunctionKind::Constructor;
			}
			case asEASTCrossReferenceKind::DefaultParameter:
			{
				const auto* Parameter = dyn_cast<asCVarDecl>(Reference.Target);
				return Parameter && Parameter->GetKind() == asEDeclKind::ParmVar
					&& Parameter->GetInitializer();
			}
			case asEASTCrossReferenceKind::SwitchParent:
				return Active.Contains(Reference.Target) && isa<asCSwitchStmt>(Reference.Target);
			case asEASTCrossReferenceKind::FallthroughTarget:
				return isa<asCSwitchClause>(Reference.Target);
			case asEASTCrossReferenceKind::ForeachBeginFunction:
			case asEASTCrossReferenceKind::ForeachEndFunction:
			case asEASTCrossReferenceKind::ForeachNextFunction:
			case asEASTCrossReferenceKind::ForeachValueFunction:
			case asEASTCrossReferenceKind::ForeachKeyFunction:
				return isa<asCFunctionDecl>(Reference.Target);
			case asEASTCrossReferenceKind::CustomAccessSpecifier:
				return isa<asCAccessSpecifierDecl>(Reference.Target);
			case asEASTCrossReferenceKind::ConversionFunction:
			{
				const auto* Function = dyn_cast<asCFunctionDecl>(Reference.Target);
				return Function && Function->GetFunctionKind() == asEFunctionKind::Conversion;
			}
			case asEASTCrossReferenceKind::ThisRecord:
			case asEASTCrossReferenceKind::LambdaCapturedThis:
				return isa<asCRecordDecl>(Reference.Target);
			case asEASTCrossReferenceKind::LambdaCapture:
				return isa<asCVarDecl>(Reference.Target);
			case asEASTCrossReferenceKind::LambdaEnclosingFunction:
				return Active.Contains(Reference.Target) && isa<asCFunctionDecl>(Reference.Target);
			default: return false;
			}
		}

		bool VerifyNode(
			const asCASTContext& Context,
			const asCASTNode* Node,
			TSet<const asCASTNode*>& Visited,
			TSet<const asCASTNode*>& Active,
			asSASTVerificationResult& Out)
		{
			if (!Context.Owns(Node))
				return Fail(Out, asEASTVerificationStatus::ForeignNode, Node,
					TEXT("owning edge references a foreign AST context"));
			if (Active.Contains(Node))
				return Fail(Out, asEASTVerificationStatus::OwningCycle, Node,
					TEXT("owning traversal contains a cycle"));
			if (Visited.Contains(Node))
				return Fail(Out, asEASTVerificationStatus::DuplicateOwningEdge, Node,
					TEXT("node is reached by more than one owning edge"));
			if (Node->GetFamily() != asEASTNodeFamily::Type
				&& !IsValidRange(Context, Node->GetSourceRange()))
			{
				return Fail(Out, asEASTVerificationStatus::InvalidSourceRange, Node,
					TEXT("node source range does not belong to the context snapshot"));
			}
			if (Node->IsInvalid())
				return Fail(Out, asEASTVerificationStatus::RecoveryReachable, Node,
					TEXT("recovery or error node is reachable from the semantic root"));

			switch (Node->GetFamily())
			{
			case asEASTNodeFamily::Decl:
				if (!ValidateDeclPayload(Context, static_cast<const asCDecl&>(*Node), Out)) return false;
				break;
			case asEASTNodeFamily::Stmt:
				if (!ValidateStmtPayload(Context, static_cast<const asCStmt&>(*Node), Out)) return false;
				break;
			case asEASTNodeFamily::Type:
				if (!ValidateTypePayload(Context, static_cast<const asCType&>(*Node), Out)) return false;
				break;
			case asEASTNodeFamily::Attr:
				if (!ValidateAttrPayload(Context, static_cast<const asCAttr&>(*Node), Out)) return false;
				break;
			default:
				return Fail(Out, asEASTVerificationStatus::UnhandledKind, Node,
					TEXT("AST node family is not handled by structural verification"));
			}

			TArray<asSASTSemanticTypeReference> SemanticReferences;
			if (!asCASTVisitor::CollectSemanticTypeReferences(*Node, SemanticReferences))
				return Fail(Out, asEASTVerificationStatus::UnhandledKind, Node,
					TEXT("typed AST taxonomy entry has no semantic-reference handling"));
			for (const asSASTSemanticTypeReference& Reference : SemanticReferences)
			{
				if (!Reference.Type.IsValid())
					return Fail(Out, asEASTVerificationStatus::MissingSemanticReference, Node,
						TEXT("typed AST node is missing a required semantic type reference"));
				if (!Context.Owns(Reference.Type.GetType())
					|| !asCASTVerifierAccess::ValidateTypeIdentity(Context, *Reference.Type.GetType()))
				{
					return Fail(Out, asEASTVerificationStatus::InvalidSemanticReference, Node,
						TEXT("semantic type reference is foreign or disagrees with its stable identity"));
				}
				asSStableKey QualifiedIdentity;
				if (Reference.Type.GetStableIdentity(
						Context.GetTypeContext()->GetIdentityRegistry(), QualifiedIdentity)
					!= asETypeIdentityStatus::Succeeded)
				{
					return Fail(Out, asEASTVerificationStatus::InvalidSemanticReference, Node,
						TEXT("semantic type reference cannot be authenticated by the context registry"));
				}
			}

			TArray<asSASTCrossReference> CrossReferences;
			if (!asCASTVisitor::CollectCrossReferences(*Node, CrossReferences))
				return Fail(Out, asEASTVerificationStatus::UnhandledKind, Node,
					TEXT("typed AST taxonomy entry has no cross-reference handling"));
			for (const asSASTCrossReference& Reference : CrossReferences)
			{
				if (!ValidateCrossReference(Context, Reference, Active))
					return Fail(Out, asEASTVerificationStatus::InvalidCrossReference, Node,
						TEXT("non-owning cross reference has a foreign or invalid semantic target"));
			}

			Active.Add(Node);
			Visited.Add(Node);
			TArray<const asCASTNode*> Children;
			if (!asCASTVisitor::CollectOwningChildren(*Node, Children))
				return Fail(Out, asEASTVerificationStatus::UnhandledKind, Node,
					TEXT("typed AST taxonomy entry has no verifier traversal"));
			for (const asCASTNode* Child : Children)
			{
				if (!Child)
					return Fail(Out, asEASTVerificationStatus::MissingRequiredChild, Node,
						TEXT("owning child is null"));
				if (!VerifyNode(Context, Child, Visited, Active, Out)) return false;
			}
			Active.Remove(Node);
			return true;
		}
	}

	bool asCASTVerifier::Verify(
		const asCASTContext& Context,
		const asCASTNode* Root,
		asSASTVerificationResult& OutResult)
	{
		OutResult = {};
		if (!Root)
			return Fail(OutResult, asEASTVerificationStatus::NullRoot, nullptr,
				TEXT("AST root is null"));
		TSet<const asCASTNode*> Visited;
		TSet<const asCASTNode*> Active;
		if (!VerifyNode(Context, Root, Visited, Active, OutResult)) return false;
		OutResult.Status = asEASTVerificationStatus::Succeeded;
		return true;
	}

END_AS_NAMESPACE
