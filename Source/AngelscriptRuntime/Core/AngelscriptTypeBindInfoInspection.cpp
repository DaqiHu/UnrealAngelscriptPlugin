#include "AngelscriptTypeBindInfoInspection.h"

#include "AngelscriptBindsInternal.h"
#include "AngelscriptTypeBindInfoReflection.h"
#include "AngelscriptTypeBindInfoValidation.h"

namespace
{
	FString PhaseName(EAngelscriptBindPhase Value)
	{
		switch (Value)
		{
		case EAngelscriptBindPhase::TypeDeclarations: return TEXT("type-declarations");
		case EAngelscriptBindPhase::TypeInfrastructure: return TEXT("type-infrastructure");
		case EAngelscriptBindPhase::ExplicitBindings: return TEXT("explicit-bindings");
		case EAngelscriptBindPhase::GeneratedBindings: return TEXT("generated-bindings");
		case EAngelscriptBindPhase::ReflectionBindings: return TEXT("reflection-bindings");
		case EAngelscriptBindPhase::PostReflectionBindings: return TEXT("post-reflection-bindings");
		case EAngelscriptBindPhase::Finalization: return TEXT("finalization");
		}
		return TEXT("opaque");
	}
	FString TypeKind(EAngelscriptTypeBindKind Value)
	{
		switch (Value)
		{
		case EAngelscriptTypeBindKind::Value: return TEXT("value");
		case EAngelscriptTypeBindKind::ObjectHandle: return TEXT("object-handle");
		case EAngelscriptTypeBindKind::Enum: return TEXT("enum");
		case EAngelscriptTypeBindKind::Interface: return TEXT("interface");
		case EAngelscriptTypeBindKind::Template: return TEXT("template");
		case EAngelscriptTypeBindKind::Namespace: return TEXT("namespace");
		case EAngelscriptTypeBindKind::Alias: return TEXT("alias");
		}
		return TEXT("opaque");
	}
	FString MemberKind(EAngelscriptTypeBindMemberKind Value)
	{
		switch (Value)
		{
		case EAngelscriptTypeBindMemberKind::Constructor: return TEXT("constructor");
		case EAngelscriptTypeBindMemberKind::Destructor: return TEXT("destructor");
		case EAngelscriptTypeBindMemberKind::Method: return TEXT("method");
		case EAngelscriptTypeBindMemberKind::Property: return TEXT("property");
		case EAngelscriptTypeBindMemberKind::Behaviour: return TEXT("behaviour");
		case EAngelscriptTypeBindMemberKind::GlobalFunction: return TEXT("global-function");
		case EAngelscriptTypeBindMemberKind::GlobalProperty: return TEXT("global-property");
		case EAngelscriptTypeBindMemberKind::EnumValue: return TEXT("enum-value");
		}
		return TEXT("opaque");
	}
	FString HostLifetime(EAngelscriptBindHostLifetime Value)
	{
		switch (Value)
		{
		case EAngelscriptBindHostLifetime::None: return TEXT("none");
		case EAngelscriptBindHostLifetime::CopiedRecord: return TEXT("copied-record");
		case EAngelscriptBindHostLifetime::BorrowedHost: return TEXT("borrowed-host");
		case EAngelscriptBindHostLifetime::RetainedHost: return TEXT("retained-host");
		}
		return TEXT("opaque");
	}
	FString RecipeKind(EAngelscriptNativeBindRecipeKind Value)
	{
		static const TCHAR* Names[] = { TEXT("constructor"), TEXT("destructor"), TEXT("assignment"),
			TEXT("uobject-cast"), TEXT("method"), TEXT("function"), TEXT("function-header"),
			TEXT("external-call"), TEXT("ufunction"), TEXT("array-index"),
			TEXT("array-iterator-create"), TEXT("array-iterator-proceed"), TEXT("template-call"),
			TEXT("delegate-execute"), TEXT("multicast-execute"), TEXT("event-execute"),
			TEXT("push-argument"), TEXT("push-argument-ref") };
		const int32 Index = static_cast<int32>(Value);
		return Index >= 0 && Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("opaque");
	}
	FString ReflectedKind(EAngelscriptReflectedKind Value)
	{
		switch (Value)
		{
		case EAngelscriptReflectedKind::Class: return TEXT("class");
		case EAngelscriptReflectedKind::Interface: return TEXT("interface");
		case EAngelscriptReflectedKind::Struct: return TEXT("struct");
		case EAngelscriptReflectedKind::Enum: return TEXT("enum");
		case EAngelscriptReflectedKind::Delegate: return TEXT("delegate");
		}
		return TEXT("opaque");
	}
	FAngelscriptBindingInspectionSource Source(const FAngelscriptTypeBindSource& Value)
	{
		return {Value.OwnerModule.ToString(), Value.Provider.ToString(), Value.File, Value.Line, PhaseName(Value.Phase)};
	}
	FString Qualified(const FString& Namespace, const FString& Name)
	{
		if (Name.IsEmpty()) return Namespace;
		return Namespace.IsEmpty() ? Name : Namespace + TEXT("::") + Name;
	}
	FString NamespaceIdentity(const FString& Namespace)
	{
		return Namespace.IsEmpty() ? TEXT("namespace:<root>") : TEXT("namespace:") + Namespace;
	}
	FString SymbolicNative(const FAngelscriptTypeBindInfoMember& Member)
	{
		if (Member.Native.Function.flag != 0) return TEXT("native-callable");
		if (Member.bPureConstant) return TEXT("copied-constant");
		if (Member.GlobalAddress != nullptr) return TEXT("global-storage");
		return TEXT("none");
	}
	void DescribePrimaryTypeUse(const FAngelscriptTypeBindInfoMember& Value, FString& OutType, FString& OutCategory)
	{
		FString Declaration = Value.Declaration.TrimStartAndEnd();
		int32 Cut = INDEX_NONE;
		if (Value.Kind == EAngelscriptTypeBindMemberKind::Method
			|| Value.Kind == EAngelscriptTypeBindMemberKind::GlobalFunction
			|| Value.Kind == EAngelscriptTypeBindMemberKind::Constructor
			|| Value.Kind == EAngelscriptTypeBindMemberKind::Behaviour)
		{
			const int32 Paren = Declaration.Find(TEXT("("));
			if (Paren != INDEX_NONE) Cut = Declaration.Left(Paren).Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		}
		else
		{
			Cut = Declaration.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		}
		if (Cut == INDEX_NONE) return;
		OutType = Declaration.Left(Cut).TrimStartAndEnd();
		while (OutType.StartsWith(TEXT("const "))) OutType.RightChopInline(6);
		FString Base = OutType.Replace(TEXT("const"), TEXT("")).Replace(TEXT("&"), TEXT("")).Replace(TEXT("@"), TEXT("")).TrimStartAndEnd();
		static const TSet<FString> Builtins = {TEXT("void"), TEXT("bool"), TEXT("int"), TEXT("int8"), TEXT("int16"), TEXT("int32"), TEXT("int64"), TEXT("uint"), TEXT("uint8"), TEXT("uint16"), TEXT("uint32"), TEXT("uint64"), TEXT("float"), TEXT("float32"), TEXT("float64"), TEXT("double")};
		const FString FirstToken = Base.Left(Base.Find(TEXT(" ")) == INDEX_NONE ? Base.Len() : Base.Find(TEXT(" ")));
		const bool bTemplateParameter = FirstToken == TEXT("T") || FirstToken == TEXT("K") || FirstToken == TEXT("V");
		OutCategory = Builtins.Contains(Base) ? TEXT("builtin") : bTemplateParameter ? TEXT("template-parameter")
			: (Base.Contains(TEXT("<")) ? TEXT("template-instance") : TEXT("nominal"));
	}
	FString FirstParameter(EAngelscriptBindFirstParameter Value)
	{
		switch (Value) { case EAngelscriptBindFirstParameter::None: return TEXT("none"); case EAngelscriptBindFirstParameter::ScriptFunction: return TEXT("script-function"); case EAngelscriptBindFirstParameter::ScriptObjectType: return TEXT("script-object-type"); }
		return TEXT("opaque");
	}
	FString CompileOut(EAngelscriptBindCompileOut Value)
	{
		static const TCHAR* Names[] = {TEXT("none"), TEXT("entirely"), TEXT("method-chain"), TEXT("in-test"), TEXT("if-no-log"), TEXT("ensure"), TEXT("check"), TEXT("replace-first-argument-in-test")};
		const int32 Index = static_cast<int32>(Value); return Index >= 0 && Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("opaque");
	}
	FString ReflectedPath(const FAngelscriptReflectedTypeSnapshot& Snapshot, const UObject* Type)
	{
		if (Type == nullptr) return TEXT("opaque:null");
		if (const FAngelscriptReflectedTypeRecord* Record = Snapshot.Find(Type)) return Record->NativePath;
		return TEXT("opaque:unresolved");
	}
	FString ReflectedUse(const FAngelscriptReflectedTypeSnapshot& Snapshot, const FAngelscriptReflectedTypeUse& Use)
	{
		TArray<FString> Arguments; for (const auto& Argument : Use.Arguments) Arguments.Add(ReflectedUse(Snapshot, Argument));
		FString Out = Use.Kind; if (!Arguments.IsEmpty()) Out += TEXT("<") + FString::Join(Arguments, TEXT(";")) + TEXT(">");
		if (!Use.CppType.IsEmpty()) Out += TEXT("|cpp=") + Use.CppType;
		if (Use.ReferencedType != nullptr) Out += TEXT("|ref=") + ReflectedPath(Snapshot, Use.ReferencedType);
		return Out;
	}
}

bool FAngelscriptTypeBindInfoInspection::InspectCollection(
	const UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
	FAngelscriptBindingInspection& Out, FString& Diagnostic)
{
	Out = {};
	Out.Scope = EAngelscriptBindingInspectionScope::CollectionOnly;
	Out.Stage = TEXT("collection");
	Out.bCollectionOnly = true;
	Out.bSealed = Collection.IsSealed();
	Out.bRecordingFailed = !Collection.GetCollectionFailureDiagnostic().IsEmpty();
	Out.Diagnostic = Collection.GetCollectionFailureDiagnostic();
	for (const FAngelscriptBindRecord& Record : Collection.GetRecords())
	{
		FAngelscriptBindingInspectionProvider Provider;
		Provider.Source.OwnerModule = Record.OwnerModule.ToString();
		Provider.Source.Provider = Record.BindName.ToString();
		Provider.Source.File = Record.SourceFile != nullptr ? UTF8_TO_TCHAR(Record.SourceFile) : FString();
		Provider.Source.Line = Record.SourceLine;
		Provider.Source.Phase = PhaseName(Record.Phase);
		Provider.bEligible = Record.Condition == nullptr;
		Provider.Disposition = Record.Condition == nullptr ? TEXT("unconditional") : TEXT("condition-not-evaluated");
		Out.Providers.Add(MoveTemp(Provider));
	}
	Out.Providers.Sort([](const auto& A, const auto& B)
	{
		return A.Source.OwnerModule + TEXT("|") + A.Source.Provider + TEXT("|") + A.Source.Phase + TEXT("|") + A.Source.File
			< B.Source.OwnerModule + TEXT("|") + B.Source.Provider + TEXT("|") + B.Source.Phase + TEXT("|") + B.Source.File;
	});
	Diagnostic.Reset();
	return true;
}

bool FAngelscriptTypeBindInfoInspection::InspectStore(
	TSharedRef<const FAngelscriptTypeBindInfoStore> Store,
	EAngelscriptBindingInspectionScope Scope, FAngelscriptBindingInspection& Out,
	FString& Diagnostic)
{
	Out = {};
	Out.Scope = Scope;
	Out.Stage = TEXT("sealed-store");
	Out.bSealed = Store->IsSealed();
	Out.bRecordingFailed = Store->HasFailed();
	Out.Policy = Store->GetPolicy();
	if (!Out.bSealed || Out.bRecordingFailed)
	{
		Diagnostic = Out.bRecordingFailed ? Store->GetFailureDiagnostic()
			: TEXT("Binding inspection requires a sealed Store.");
		Out.Diagnostic = Diagnostic;
		return false;
	}

	const FAngelscriptTypeBindValidationResult Validation =
		FAngelscriptTypeBindInfoValidation::ValidateSealed(Store);
	Out.bValidationAttempted = true;
	Out.bValid = Validation.bValid;
	Out.Diagnostic = Validation.Message;
	for (const FAngelscriptRecordedProvider& Value : Store->GetProviders())
	{
		FAngelscriptBindingInspectionProvider Provider;
		Provider.Source = Source(Value.Source);
		Provider.bEligible = Value.bEligible;
		Provider.Disposition = Value.bEligible ? TEXT("recorded") : TEXT("excluded-by-condition");
		Out.Providers.Add(MoveTemp(Provider));
	}
	Out.Providers.Sort([](const auto& A, const auto& B)
	{
		return A.Source.OwnerModule + TEXT("|") + A.Source.Provider + TEXT("|") + A.Source.Phase + TEXT("|") + A.Source.File
			< B.Source.OwnerModule + TEXT("|") + B.Source.Provider + TEXT("|") + B.Source.Phase + TEXT("|") + B.Source.File;
	});

	for (const FAngelscriptTypeBindInfo& Value : Store->GetTypes())
	{
		FAngelscriptBindingInspectionType Type;
		Type.Identity = Value.Declaration.Kind == EAngelscriptTypeBindKind::Namespace
			? NamespaceIdentity(Value.Declaration.Namespace)
			: Qualified(Value.Declaration.Namespace, Value.Declaration.Name);
		Type.Name = Value.Declaration.Name;
		Type.Namespace = Value.Declaration.Namespace;
		Type.Kind = TypeKind(Value.Declaration.Kind);
		Type.BaseType = Value.Declaration.BaseType;
		Type.Interfaces = Value.Declaration.Interfaces;
		Type.AliasTarget = Value.Declaration.AliasTarget;
		Type.TemplateType = Value.Declaration.TemplateType;
		Type.TemplateDeclaration = Value.Declaration.TemplateDeclaration;
		Type.NativeSize = Value.Declaration.NativeSize;
		Type.NativeAlignment = Value.Declaration.NativeAlignment;
		Type.ObjectFlags = Value.Declaration.ObjectFlags;
		Type.NativeOwner = Value.Declaration.NativeOwner;
		Type.AdapterName = Value.Declaration.AdapterName;
		if (Value.Declaration.DefaultConstruct != nullptr) Type.LifecycleRecipes.Add(TEXT("default-construct"));
		if (Value.Declaration.CopyConstruct != nullptr) Type.LifecycleRecipes.Add(TEXT("copy-construct"));
		if (Value.Declaration.Destroy != nullptr) Type.LifecycleRecipes.Add(TEXT("destroy"));
		for (const FAngelscriptTypeBindSource& ValueSource : Value.Sources)
			Type.Sources.Add(Source(ValueSource));
		for (const FAngelscriptTypeBindInfoMember& ValueMember : Value.Members)
		{
			FAngelscriptBindingInspectionMember Member;
			Member.Owner = Type.Identity;
			Member.Kind = MemberKind(ValueMember.Kind);
			Member.Declaration = ValueMember.Declaration;
			DescribePrimaryTypeUse(ValueMember, Member.TypeReference, Member.TypeCategory);
			Member.Identity = Type.Identity + TEXT("|") + Member.Kind + TEXT("|") + Member.Declaration;
			Member.PropertyOffset = static_cast<uint64>(ValueMember.PropertyOffset);
			if (ValueMember.Kind == EAngelscriptTypeBindMemberKind::EnumValue)
				Member.EnumValue = FString::Printf(TEXT("%d"), ValueMember.EnumValue);
			if (ValueMember.bPureConstant)
				Member.ConstantValue = FString::Printf(TEXT("%llu"), ValueMember.ConstantValue);
			Member.Storage = HostLifetime(ValueMember.GlobalStorageLifetime);
			Member.NativeTarget = SymbolicNative(ValueMember);
			Member.FunctionLifetime = HostLifetime(ValueMember.Native.FunctionLifetime);
			Member.AuxiliaryLifetime = HostLifetime(ValueMember.Native.AuxiliaryLifetime);
			Member.Traits = static_cast<uint64>(ValueMember.Traits);
			Member.FirstParameter = FirstParameter(ValueMember.FirstParameter);
			Member.CompileOut = CompileOut(ValueMember.CompileOut);
			Member.bCanRead = ValueMember.bCanRead; Member.bCanWrite = ValueMember.bCanWrite;
			Member.bCanEdit = ValueMember.bCanEdit; Member.bProtected = ValueMember.bProtected;
			Member.DeprecationMessage = ValueMember.DeprecationMessage; Member.Documentation = ValueMember.Documentation;
			Member.Category = ValueMember.Category;
			for (const FAngelscriptNativeBindRecipe& Recipe : ValueMember.NativeRecipes)
				Member.Recipes.Add(RecipeKind(Recipe.Kind) + TEXT(":") + Recipe.Name);
			Member.Source = Source(ValueMember.Source);
			Type.Members.Add(MoveTemp(Member));
		}
		Type.Members.Sort([](const auto& A, const auto& B) { return A.Identity < B.Identity; });
		Out.Types.Add(MoveTemp(Type));
	}
	Out.Types.Sort([](const auto& A, const auto& B) { return A.Identity < B.Identity; });

	if (const TSharedPtr<const FAngelscriptReflectedTypeSnapshot>& Snapshot = Store->GetReflectedSnapshot())
	{
		for (const FAngelscriptReflectedTypeRecord& Value : Snapshot->GetTypes())
		{
			FAngelscriptBindingInspectionReflectedType Reflected;
			Reflected.NativePath = Value.NativePath;
			Reflected.Kind = ReflectedKind(Value.Kind);
			Reflected.IntendedIdentity = Qualified(Value.Declaration.Namespace, Value.Declaration.Name);
			Reflected.bEligible = Value.bEligible;
			Reflected.ExclusionReason = Value.ExclusionReason;
			Reflected.ParentPath = Value.Parent != nullptr ? ReflectedPath(*Snapshot, Value.Parent) : FString();
			for (const UClass* Interface : Value.Interfaces)
				Reflected.Interfaces.Add(ReflectedPath(*Snapshot, Interface));
			for (const FAngelscriptReflectedProperty& Property : Value.Properties)
				Reflected.Properties.Add(FString::Printf(TEXT("%s|%s|%d|%d|%d|%llu"), *Property.Name,
					*ReflectedUse(*Snapshot, Property.Type), Property.Offset, Property.ElementSize, Property.ArrayDim, Property.Flags));
			for (const TPair<FString, int64>& EnumValue : Value.EnumValues)
				Reflected.EnumValues.Emplace(EnumValue.Key, FString::Printf(TEXT("%lld"), EnumValue.Value));
			Reflected.bHasGCReferences = Value.Lifetime.bHasGCReferences;
			Reflected.bNeedsDestruction = Value.Lifetime.bNeedsDestruction;
			if (Value.Lifetime.Initialize != nullptr) Reflected.LifetimeRecipes.Add(TEXT("initialize"));
			if (Value.Lifetime.Copy != nullptr) Reflected.LifetimeRecipes.Add(TEXT("copy"));
			if (Value.Lifetime.Destroy != nullptr) Reflected.LifetimeRecipes.Add(TEXT("destroy"));
			Out.ReflectedTypes.Add(MoveTemp(Reflected));
		}
		Out.ReflectedTypes.Sort([](const auto& A, const auto& B) { return A.NativePath < B.NativePath; });
	}
	Diagnostic.Reset();
	return true;
}
