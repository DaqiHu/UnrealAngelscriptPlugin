#pragma once

#include "NativeEngine/NativeEngineTestSupport.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "frontend/Sema/as_tooling_session.h"

namespace AngelscriptNativeToolingTest
{
	inline TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> MakeInputs(
		TArray<TPair<FString, FString>> Files,
		TSharedPtr<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Types = nullptr,
		TSharedPtr<asCDefinitions, ESPMode::ThreadSafe> Host = nullptr)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Inputs =
			MakeShared<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe>();
		for (const TPair<FString, FString>& File : Files)
		{
			Inputs->Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(File.Key, File.Value));
		}
		Inputs->Options.StableScope = TEXT("tooling");
		if (Types.IsValid())
		{
			Inputs->Options.TypeContext = Types;
		}
		if (Host.IsValid())
		{
			Inputs->OwnedDefinitions.Add(Host);
			Inputs->Options.Dependencies.Add(Host.Get());
		}
		return Inputs;
	}

	inline TSharedPtr<asCDefinitions, ESPMode::ThreadSafe> MakeHostType(
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Types,
		const FString& Name,
		asUINT Arity = 0)
	{
		TUniquePtr<asCDefinitions> Set = MakeUnique<asCDefinitions>();
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = TEXT("tooling-host");
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
		Descriptor.Name = Name;
		Descriptor.GenericArity = Arity;
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		asCObjectType* Type = nullptr;
		const auto Narrow = StringCast<ANSICHAR>(*Name);
		if (Set->SetIdentityContext(Types) != asEDefinitionResult::Succeeded
			|| Types->GetIdentityRegistry().InternNominalType(Descriptor, Key)
				!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded
			|| Set->CreateObjectType(Key, Narrow.Get(), "", asOBJ_REF | asOBJ_SCRIPT_OBJECT, Type) != asEDefinitionResult::Succeeded
			|| Set->SetTypeDeclarationFacts(*Type, AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class, Arity)
				!= asEDefinitionResult::Succeeded
			|| Set->FinalizeLayouts() != asEDefinitionResult::Succeeded
			|| Set->Freeze() != asEDefinitionResult::Succeeded
			|| !Set->ValidateFrozenDefinitions())
		{
			return nullptr;
		}
		return TSharedPtr<asCDefinitions, ESPMode::ThreadSafe>(Set.Release());
	}
}
