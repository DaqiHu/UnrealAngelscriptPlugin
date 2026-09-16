#include "frontend/Compile/as_compilation_session.h"
#include "frontend/AST/as_ast_cast.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"

BEGIN_AS_NAMESPACE
bool asCCompilationSession::ResolveListInitializer(asCQualType Type, asSListInitialization& Out)
	{
		Out = {};
		const auto* Nominal = dyn_cast<asCNominalType>(Type.GetType());
		TConstArrayView<asCQualType> Arguments;
		if (const auto* Generic = dyn_cast<asCGenericType>(Type.GetType()))
		{
			Nominal = dyn_cast<asCNominalType>(Generic->GetTemplateType()); Arguments = Generic->GetArguments();
		}
		if (!Nominal) return false;
		const asCObjectType* Target = nullptr;
		for (const auto& Set : ExternalDefinitions)
			if (const auto* Candidate = Set->FindType(Nominal->GetDeclarationIdentity());
				Candidate && (Candidate->flags & (asOBJ_REF | asOBJ_VALUE)) && !(Candidate->flags & asOBJ_CALLABLE_TYPE))
			{ Target = static_cast<const asCObjectType*>(Candidate); break; }
		if (!Target) return false;
		const auto* Initializer = Target->GetDefinitions()->GetListInitializer(*Target);
		if (!Initializer || !Initializer->Factory) return false;
		auto& Registry = Context.GetTypeContext()->GetIdentityRegistry();
		TFunction<asCQualType(const asCDataType&, int)> ConvertType = [&](const asCDataType& Input, int Depth) -> asCQualType
		{
			if (Depth > 64) return {};
			asCType* Result = nullptr;
			if (const auto* Info = Input.GetTypeInfo())
			{
				const asCObjectType* Instance = (Info->flags & (asOBJ_VALUE | asOBJ_REF)) && !(Info->flags & asOBJ_CALLABLE_TYPE) ? static_cast<const asCObjectType*>(Info) : nullptr;
				const auto* Definition = Instance ? Instance->GetTemplateDefinition() : nullptr;
				asSStableTypeUseDescriptor Use; Use.Kind = asETypeUseKind::Nominal; Use.Declaration = Definition ? Definition->GetStableKey() : Info->GetStableKey();
				asSStableKey BaseKey;
				if (Registry.InternTypeUse(Use, BaseKey) != asETypeIdentityStatus::Succeeded) return {};
				auto* Base = Context.GetNominalType(Use.Declaration, BaseKey);
				Result = Base;
				if (Definition)
				{
					TArray<asCQualType> Concrete;
					for (asUINT Index = 0; Index < Instance->templateSubTypes.GetLength(); ++Index)
					{
						auto Argument = ConvertType(Instance->templateSubTypes[Index], Depth + 1); asSStableKey Key;
						if (!Argument.IsValid() || Argument.GetStableIdentity(Registry, Key) != asETypeIdentityStatus::Succeeded) return {};
						Concrete.Add(Argument); Use.Arguments.Add(Key);
					}
					asSStableKey Key;
					if (Registry.InternTypeUse(Use, Key) != asETypeIdentityStatus::Succeeded) return {};
					Result = Context.GetGenericType(Base, MoveTemp(Concrete), Key);
				}
			}
			else
			{
				asEPrimitiveType Primitive = asEPrimitiveType::Invalid;
				switch (Input.GetTokenType())
				{
				case ttBool: Primitive = asEPrimitiveType::Bool; break;
				case ttInt8: Primitive = asEPrimitiveType::Int8; break;
				case ttInt16: Primitive = asEPrimitiveType::Int16; break;
				case ttInt: Primitive = asEPrimitiveType::Int32; break;
				case ttInt64: Primitive = asEPrimitiveType::Int64; break;
				case ttUInt8: Primitive = asEPrimitiveType::UInt8; break;
				case ttUInt16: Primitive = asEPrimitiveType::UInt16; break;
				case ttUInt: Primitive = asEPrimitiveType::UInt32; break;
				case ttUInt64: Primitive = asEPrimitiveType::UInt64; break;
				case ttFloat32: Primitive = asEPrimitiveType::Float32; break;
				case ttFloat64: case ttDouble: Primitive = asEPrimitiveType::Float64; break;
				case ttFloat: Primitive = Input.IsFloatExtendedToDouble() ? asEPrimitiveType::Float64 : asEPrimitiveType::Float32; break;
				default: return {};
				}
				Result = Context.GetBuiltinType(Primitive);
			}
			asETypeQualifiers Qualifiers = asETypeQualifiers::None;
			if (Input.IsReadOnly() || Input.IsHandleToConst()) Qualifiers = Qualifiers | asETypeQualifiers::Const;
			if (Input.IsReference()) Qualifiers = Qualifiers | asETypeQualifiers::Reference;
			if (Input.IsObjectHandle()) Qualifiers = Qualifiers | asETypeQualifiers::Handle;
			if (Input.IsMetadataNullable()) Qualifiers = Qualifiers | asETypeQualifiers::Nullable;
			asCQualType Converted; asCQualType::TryCreate(Result, Qualifiers, Converted); return Converted;
		};
		TFunction<bool(const asSMetadataListPattern&, asSResolvedListPattern&)> Resolve = [&](const asSMetadataListPattern& Input, asSResolvedListPattern& Result)
		{
			Result.Kind = Input.Kind;
			if (Input.Kind == asEMetadataListPatternKind::Value)
			{
				if (Input.TemplateParameterIndex != ~asUINT(0))
				{
					if (Input.TemplateParameterIndex >= asUINT(Arguments.Num())) return false;
					Result.ValueType = Arguments[Input.TemplateParameterIndex];
				}
				else Result.ValueType = ConvertType(Input.ValueType, 0);
				return Result.ValueType.IsValid();
			}
			for (const auto& Child : Input.Children)
				if (!Resolve(Child, Result.Children.AddDefaulted_GetRef())) return false;
			return true;
		};
		asSListInitialization Result;
		Result.Factory = Initializer->Factory->GetStableKey();
		if (!Resolve(Initializer->Pattern, Result.Pattern)) return false;
		Out = MoveTemp(Result); return true;
	}
END_AS_NAMESPACE
