#include "AngelscriptBindingDump.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	using FObject = TSharedPtr<FJsonObject>;
	FString ScopeName(EAngelscriptBindingInspectionScope Scope)
	{
		switch (Scope)
		{
		case EAngelscriptBindingInspectionScope::CollectionOnly: return TEXT("collection-only");
		case EAngelscriptBindingInspectionScope::SelectedFamily: return TEXT("selected-family");
		case EAngelscriptBindingInspectionScope::FullRuntime: return TEXT("full-runtime");
		}
		return TEXT("opaque");
	}
	FObject SourceJson(const FAngelscriptBindingInspectionSource& Source)
	{
		FObject Out = MakeShared<FJsonObject>();
		Out->SetStringField(TEXT("ownerModule"), Source.OwnerModule);
		Out->SetStringField(TEXT("provider"), Source.Provider);
		Out->SetStringField(TEXT("file"), Source.File);
		Out->SetNumberField(TEXT("line"), Source.Line);
		Out->SetStringField(TEXT("phase"), Source.Phase);
		return Out;
	}
	TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (const FString& Value : Values) Out.Add(MakeShared<FJsonValueString>(Value));
		return Out;
	}
	FString CsvField(const FString& Value)
	{
		if (!Value.Contains(TEXT(",")) && !Value.Contains(TEXT("\""))
			&& !Value.Contains(TEXT("\n")) && !Value.Contains(TEXT("\r"))) return Value;
		return TEXT("\"") + Value.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\"");
	}
	void CsvRow(FString& Out, std::initializer_list<FString> Fields)
	{
		bool First = true;
		for (const FString& Field : Fields)
		{
			if (!First) Out += TEXT(",");
			First = false; Out += CsvField(Field);
		}
		Out += TEXT("\r\n");
	}
	FString Join(const TArray<FString>& Values) { return FString::Join(Values, TEXT("|")); }
	bool Save(const FString& Filename, const FString& Content, FString& Diagnostic)
	{
		if (FFileHelper::SaveStringToFile(Content, *Filename, FFileHelper::EEncodingOptions::ForceUTF8)) return true;
		Diagnostic = FString::Printf(TEXT("Failed to write binding dump file '%s'."), *Filename); return false;
	}
}

bool FAngelscriptBindingDump::Serialize(const FAngelscriptBindingInspection& Inspection,
	FString& OutJson, TMap<FString, FString>& OutTables, FString& Diagnostic)
{
	OutJson.Reset(); OutTables.Reset(); Diagnostic.Reset();
	FObject Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), FAngelscriptBindingInspection::SchemaVersion);
	Root->SetStringField(TEXT("scope"), ScopeName(Inspection.Scope));
	Root->SetStringField(TEXT("stage"), Inspection.Stage);
	Root->SetBoolField(TEXT("collectionOnly"), Inspection.bCollectionOnly);
	Root->SetBoolField(TEXT("sealed"), Inspection.bSealed);
	Root->SetBoolField(TEXT("recordingFailed"), Inspection.bRecordingFailed);
	Root->SetBoolField(TEXT("validationAttempted"), Inspection.bValidationAttempted);
	Root->SetBoolField(TEXT("valid"), Inspection.bValid);
	Root->SetStringField(TEXT("diagnostic"), Inspection.Diagnostic);
	FObject Policy = MakeShared<FJsonObject>();
	Policy->SetBoolField(TEXT("editor"), Inspection.Policy.bEditor);
	Policy->SetBoolField(TEXT("test"), Inspection.Policy.bTest);
	Policy->SetBoolField(TEXT("logging"), Inspection.Policy.bLogging);
	Policy->SetBoolField(TEXT("shipping"), Inspection.Policy.bShipping);
	Policy->SetBoolField(TEXT("simulateCooked"), Inspection.Policy.bSimulateCooked);
	Policy->SetBoolField(TEXT("forceConstWithinDevelopmentOnlyFunctions"), Inspection.Policy.bForceConstWithinDevelopmentOnlyFunctions);
	Policy->SetBoolField(TEXT("scriptFloatIsFloat64"), Inspection.Policy.bScriptFloatIsFloat64);
	Policy->SetStringField(TEXT("platform"), Inspection.Policy.Platform.ToString());
	Root->SetObjectField(TEXT("policy"), Policy);

	TArray<TSharedPtr<FJsonValue>> Providers;
	FString ProviderCsv = TEXT("owner_module,provider,phase,file,line,disposition\r\n");
	for (const auto& Provider : Inspection.Providers)
	{
		FObject Item = SourceJson(Provider.Source);
		Item->SetBoolField(TEXT("eligible"), Provider.bEligible);
		Item->SetStringField(TEXT("disposition"), Provider.Disposition);
		Providers.Add(MakeShared<FJsonValueObject>(Item));
		CsvRow(ProviderCsv, {Provider.Source.OwnerModule, Provider.Source.Provider, Provider.Source.Phase,
			Provider.Source.File, FString::FromInt(Provider.Source.Line), Provider.Disposition});
	}
	Root->SetArrayField(TEXT("providers"), Providers);
	OutTables.Add(TEXT("providers.csv"), MoveTemp(ProviderCsv));

	TArray<TSharedPtr<FJsonValue>> Types;
	FString TypeCsv = TEXT("identity,name,namespace,kind,base,interfaces,alias_target,template_type,template_declaration,native_size,native_alignment,object_flags\r\n");
	FString ClassCsv = TEXT("identity,kind,base,interfaces,native_size,native_alignment,object_flags\r\n");
	FString MemberCsv = TEXT("identity,owner,kind,declaration,property_offset,enum_value,constant_value,storage,native_target,recipes,provider,file,line\r\n");
	FString GlobalCsv = TEXT("identity,kind,declaration,constant_value,storage,native_target,provider,file,line\r\n");
	for (const auto& Type : Inspection.Types)
	{
		FObject Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("identity"), Type.Identity); Item->SetStringField(TEXT("name"), Type.Name);
		Item->SetStringField(TEXT("namespace"), Type.Namespace); Item->SetStringField(TEXT("kind"), Type.Kind);
		Item->SetStringField(TEXT("base"), Type.BaseType); Item->SetArrayField(TEXT("interfaces"), Strings(Type.Interfaces));
		Item->SetStringField(TEXT("aliasTarget"), Type.AliasTarget); Item->SetStringField(TEXT("templateType"), Type.TemplateType);
		Item->SetStringField(TEXT("templateDeclaration"), Type.TemplateDeclaration);
		Item->SetStringField(TEXT("nativeSize"), LexToString(Type.NativeSize));
		Item->SetStringField(TEXT("nativeAlignment"), LexToString(Type.NativeAlignment));
		Item->SetStringField(TEXT("objectFlags"), LexToString(Type.ObjectFlags));
		Item->SetStringField(TEXT("nativeOwner"), Type.NativeOwner); Item->SetStringField(TEXT("adapterName"), Type.AdapterName);
		Item->SetArrayField(TEXT("lifecycleRecipes"), Strings(Type.LifecycleRecipes));
		TArray<TSharedPtr<FJsonValue>> Sources;
		for (const auto& Value : Type.Sources) Sources.Add(MakeShared<FJsonValueObject>(SourceJson(Value)));
		Item->SetArrayField(TEXT("sources"), Sources);
		TArray<TSharedPtr<FJsonValue>> Members;
		for (const auto& Member : Type.Members)
		{
			FObject M = MakeShared<FJsonObject>();
			M->SetStringField(TEXT("identity"), Member.Identity); M->SetStringField(TEXT("owner"), Member.Owner);
			M->SetStringField(TEXT("kind"), Member.Kind); M->SetStringField(TEXT("declaration"), Member.Declaration);
			M->SetStringField(TEXT("typeReference"), Member.TypeReference); M->SetStringField(TEXT("typeCategory"), Member.TypeCategory);
			M->SetStringField(TEXT("propertyOffset"), LexToString(Member.PropertyOffset)); M->SetStringField(TEXT("enumValue"), Member.EnumValue);
			M->SetStringField(TEXT("constantValue"), Member.ConstantValue); M->SetStringField(TEXT("storage"), Member.Storage);
			M->SetStringField(TEXT("nativeTarget"), Member.NativeTarget); M->SetStringField(TEXT("functionLifetime"), Member.FunctionLifetime);
			M->SetStringField(TEXT("auxiliaryLifetime"), Member.AuxiliaryLifetime); M->SetStringField(TEXT("traits"), LexToString(Member.Traits));
			M->SetStringField(TEXT("firstParameter"), Member.FirstParameter); M->SetStringField(TEXT("compileOut"), Member.CompileOut);
			M->SetBoolField(TEXT("canRead"), Member.bCanRead); M->SetBoolField(TEXT("canWrite"), Member.bCanWrite);
			M->SetBoolField(TEXT("canEdit"), Member.bCanEdit); M->SetBoolField(TEXT("protected"), Member.bProtected);
			M->SetStringField(TEXT("deprecationMessage"), Member.DeprecationMessage); M->SetStringField(TEXT("documentation"), Member.Documentation);
			M->SetStringField(TEXT("category"), Member.Category); M->SetArrayField(TEXT("recipes"), Strings(Member.Recipes));
			M->SetObjectField(TEXT("source"), SourceJson(Member.Source)); Members.Add(MakeShared<FJsonValueObject>(M));
			CsvRow(MemberCsv, {Member.Identity, Member.Owner, Member.Kind, Member.Declaration,
				LexToString(Member.PropertyOffset), Member.EnumValue, Member.ConstantValue, Member.Storage,
				Member.NativeTarget, Join(Member.Recipes), Member.Source.Provider, Member.Source.File,
				FString::FromInt(Member.Source.Line)});
			if (Member.Kind.StartsWith(TEXT("global-")))
				CsvRow(GlobalCsv, {Member.Identity, Member.Kind, Member.Declaration, Member.ConstantValue,
					Member.Storage, Member.NativeTarget, Member.Source.Provider, Member.Source.File,
					FString::FromInt(Member.Source.Line)});
		}
		Item->SetArrayField(TEXT("members"), Members); Types.Add(MakeShared<FJsonValueObject>(Item));
		CsvRow(TypeCsv, {Type.Identity, Type.Name, Type.Namespace, Type.Kind, Type.BaseType, Join(Type.Interfaces),
			Type.AliasTarget, Type.TemplateType, Type.TemplateDeclaration, LexToString(Type.NativeSize),
			LexToString(Type.NativeAlignment), LexToString(Type.ObjectFlags)});
		if (Type.Kind != TEXT("namespace")) CsvRow(ClassCsv, {Type.Identity, Type.Kind, Type.BaseType,
			Join(Type.Interfaces), LexToString(Type.NativeSize), LexToString(Type.NativeAlignment), LexToString(Type.ObjectFlags)});
	}
	Root->SetArrayField(TEXT("types"), Types);
	OutTables.Add(TEXT("types.csv"), MoveTemp(TypeCsv)); OutTables.Add(TEXT("classes.csv"), MoveTemp(ClassCsv));
	OutTables.Add(TEXT("members.csv"), MoveTemp(MemberCsv)); OutTables.Add(TEXT("globals.csv"), MoveTemp(GlobalCsv));

	TArray<TSharedPtr<FJsonValue>> Reflected;
	FString ReflectionCsv = TEXT("native_path,kind,intended_identity,eligible,parent,interfaces,properties,has_gc_references,needs_destruction\r\n");
	FString ExclusionCsv = TEXT("native_path,kind,intended_identity,reason\r\n");
	for (const auto& Value : Inspection.ReflectedTypes)
	{
		FObject Item = MakeShared<FJsonObject>(); Item->SetStringField(TEXT("nativePath"), Value.NativePath);
		Item->SetStringField(TEXT("kind"), Value.Kind); Item->SetStringField(TEXT("intendedIdentity"), Value.IntendedIdentity);
		Item->SetBoolField(TEXT("eligible"), Value.bEligible); Item->SetStringField(TEXT("exclusionReason"), Value.ExclusionReason);
		Item->SetStringField(TEXT("parentPath"), Value.ParentPath); Item->SetArrayField(TEXT("interfaces"), Strings(Value.Interfaces));
		Item->SetArrayField(TEXT("properties"), Strings(Value.Properties)); Item->SetBoolField(TEXT("hasGCReferences"), Value.bHasGCReferences);
		Item->SetBoolField(TEXT("needsDestruction"), Value.bNeedsDestruction); Item->SetArrayField(TEXT("lifetimeRecipes"), Strings(Value.LifetimeRecipes));
		TArray<TSharedPtr<FJsonValue>> EnumValues;
		for (const auto& EnumValue : Value.EnumValues) { FObject E = MakeShared<FJsonObject>(); E->SetStringField(TEXT("name"), EnumValue.Key); E->SetStringField(TEXT("value"), EnumValue.Value); EnumValues.Add(MakeShared<FJsonValueObject>(E)); }
		Item->SetArrayField(TEXT("enumValues"), EnumValues); Reflected.Add(MakeShared<FJsonValueObject>(Item));
		CsvRow(ReflectionCsv, {Value.NativePath, Value.Kind, Value.IntendedIdentity, Value.bEligible ? TEXT("true") : TEXT("false"),
			Value.ParentPath, Join(Value.Interfaces), Join(Value.Properties), Value.bHasGCReferences ? TEXT("true") : TEXT("false"), Value.bNeedsDestruction ? TEXT("true") : TEXT("false")});
		if (!Value.bEligible) CsvRow(ExclusionCsv, {Value.NativePath, Value.Kind, Value.IntendedIdentity, Value.ExclusionReason});
	}
	Root->SetArrayField(TEXT("reflection"), Reflected);
	OutTables.Add(TEXT("reflection.csv"), MoveTemp(ReflectionCsv)); OutTables.Add(TEXT("exclusions.csv"), MoveTemp(ExclusionCsv));

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)) { Diagnostic = TEXT("Failed to serialize binding manifest JSON."); return false; }
	FString Summary;
	CsvRow(Summary, {TEXT("schema_version"), FString::FromInt(FAngelscriptBindingInspection::SchemaVersion)});
	CsvRow(Summary, {TEXT("scope"), ScopeName(Inspection.Scope)}); CsvRow(Summary, {TEXT("stage"), Inspection.Stage});
	CsvRow(Summary, {TEXT("types"), FString::FromInt(Inspection.Types.Num())}); CsvRow(Summary, {TEXT("providers"), FString::FromInt(Inspection.Providers.Num())});
	CsvRow(Summary, {TEXT("reflection"), FString::FromInt(Inspection.ReflectedTypes.Num())}); CsvRow(Summary, {TEXT("valid"), Inspection.bValid ? TEXT("true") : TEXT("false")});
	OutTables.Add(TEXT("summary.csv"), MoveTemp(Summary)); return true;
}

bool FAngelscriptBindingDump::ExportToDirectory(const FAngelscriptBindingInspection& Inspection,
	const FString& Directory, FAngelscriptBindingDumpResult& Out, FString& Diagnostic)
{
	Out = {}; Diagnostic.Reset();
	if (Directory.IsEmpty()) { Diagnostic = TEXT("A binding dump destination is required."); return false; }
	IFileManager& Files = IFileManager::Get();
	if (Files.DirectoryExists(*Directory) || Files.FileExists(*Directory))
	{
		Diagnostic = FString::Printf(TEXT("Binding dump destination '%s' already exists."), *Directory); return false;
	}
	FString Json; TMap<FString, FString> Tables;
	if (!Serialize(Inspection, Json, Tables, Diagnostic)) return false;
	const FString Staging = Directory + TEXT(".tmp-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	if (!Files.MakeDirectory(*Staging, true)) { Diagnostic = TEXT("Failed to create binding dump staging directory."); return false; }
	bool bSaved = Save(FPaths::Combine(Staging, TEXT("manifest.json")), Json, Diagnostic);
	for (const auto& Table : Tables) bSaved = bSaved && Save(FPaths::Combine(Staging, Table.Key), Table.Value, Diagnostic);
	if (!bSaved || !Files.Move(*Directory, *Staging, true, true, false, true))
	{
		Files.DeleteDirectory(*Staging, false, true);
		if (Diagnostic.IsEmpty()) Diagnostic = TEXT("Failed to publish the binding dump directory.");
		return false;
	}
	Out.Directory = Directory; Out.SemanticJson = Json; Out.Files.Add(TEXT("manifest.json"));
	TArray<FString> TableNames; Tables.GenerateKeyArray(TableNames); Out.Files.Append(TableNames); Out.Files.Sort(); return true;
}

bool FAngelscriptBindingDump::ExportStoreToDirectory(TSharedRef<const FAngelscriptTypeBindInfoStore> Store,
	EAngelscriptBindingInspectionScope Scope, const FString& Directory,
	FAngelscriptBindingDumpResult& Out, FString& Diagnostic)
{
	FAngelscriptBindingInspection Inspection;
	if (!FAngelscriptTypeBindInfoInspection::InspectStore(Store, Scope, Inspection, Diagnostic)) return false;
	return ExportToDirectory(Inspection, Directory, Out, Diagnostic);
}
