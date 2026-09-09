#include "CQTest.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptTypeBindInfoCatalog.h"
#include "RuntimeBindingTestSupport.h"

#include "as_context.h"
#include "as_scriptengine.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace RuntimeBindingStringNameFixture
{
static const FName StringNameProviderNames[] = {
	TEXT("FString.ExplicitBindings"),
	TEXT("FName.Functions"),
};

struct FStringNameFixture
{
	TSharedRef<FAngelscriptTypeBindInfoStore> Store = MakeShared<FAngelscriptTypeBindInfoStore>();
	TUniquePtr<FAngelscriptEngine> Owner;
	FString Diagnostic;

	bool Record(bool bCreateOwner = true)
	{
		auto Fail = [&]() { UE_LOG(LogTemp, Error, TEXT("String/name fixture failed: %s"), *Diagnostic); return false; };
		if (!FAngelscriptBind::FinalizeRegisteredBinds(Diagnostic)) return Fail();
		const auto& Collection = FAngelscriptBind::GetRegisteredCollection();
		if (!FAngelscriptTypeBindInfoCatalog::RecordInstallableTypeDeclarations(Collection, *Store, Diagnostic)
			|| !FAngelscriptTypeBindInfoCatalog::RecordSelectedProviders(Collection, StringNameProviderNames, *Store, Diagnostic)
			|| !Store->Seal(Diagnostic)) return Fail();
		if (!bCreateOwner) return true;
		FAngelscriptBindingDiagnostic BindingDiagnostic;
		Owner = FAngelscriptEngine::CreateForBindings(Store, BindingDiagnostic);
		if (!Owner) Diagnostic = BindingDiagnostic.Message;
		if (!Owner) return Fail();
		return Owner.IsValid();
	}

	asCScriptFunction* Function(const TCHAR* OwnerName, const TCHAR* Declaration, const TCHAR* Namespace = TEXT("")) const
	{
		for (const auto& Installed : Owner->GetBindingInstallation()->GetMembers())
		{
			const auto* Record = Store->GetMember(Installed.Record);
			const auto* Type = Store->GetType(Installed.Record.Type);
			if (Installed.Function && Type && Record && Type->Declaration.Name == OwnerName
				&& Type->Declaration.Namespace == Namespace && Record->Declaration == Declaration)
				return Installed.Function;
		}
		return nullptr;
	}
};

struct FStringNameContext
{
	asIScriptContext* Ptr = nullptr;
	explicit FStringNameContext(FStringNameFixture& Fixture) : Ptr(Fixture.Owner->Engine->CreateContext()) {}
	~FStringNameContext() { if (Ptr) Ptr->Release(); }
	asIScriptContext* operator->() const { return Ptr; }
};
}

using namespace RuntimeBindingStringNameFixture;
TEST_CLASS_WITH_FLAGS(StringName, "Angelscript.UnitTest.RuntimeBindings.Values.StringName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CompleteFStringAndFNameProviderSurfacesAreRecorded)
	{
		FStringNameFixture F; ASSERT_THAT(IsTrue(F.Record(false), *F.Diagnostic));
		int32 StringMembers = 0, NameMembers = 0, NamespaceMembers = 0;
		for (const auto& Type : F.Store->GetTypes())
			for (const auto& Member : Type.Members)
			{
				if (Member.Source.Provider == TEXT("FString.ExplicitBindings"))
				{
					if (Type.Declaration.Name == TEXT("FString")) ++StringMembers;
					else if (Type.Declaration.Kind == EAngelscriptTypeBindKind::Namespace
						&& Type.Declaration.Namespace == TEXT("FString")) ++NamespaceMembers;
				}
				if (Member.Source.Provider == TEXT("FName.Functions")) ++NameMembers;
			}
		// The fixed task-4.7 surface excludes the nine wildcard/dynamic ToString
		// contributions owned by task 7.10.
		ASSERT_THAT(AreEqual(65, StringMembers));
		ASSERT_THAT(AreEqual(18, NamespaceMembers));
		ASSERT_THAT(AreEqual(17, NameMembers));
	}

	TEST_METHOD(CopyThenAppendProducesAbcdWithoutChangingOriginal)
	{
		FStringNameFixture F; ASSERT_THAT(IsTrue(F.Record(), *F.Diagnostic)); FStringNameContext C(F);
		FString Original(TEXT("ab")); alignas(FString) uint8 CopyStorage[sizeof(FString)];
		auto* Copy = F.Function(TEXT("FString"), TEXT("void f(const FString& Other)"));
		auto* Append = F.Function(TEXT("FString"), TEXT("FString& Append(const FString& Other) accept_temporary_this"));
		ASSERT_THAT(IsNotNull(Copy)); ASSERT_THAT(IsNotNull(Append));
		ASSERT_THAT(AreEqual(0, C->Prepare(Copy))); ASSERT_THAT(AreEqual(0, C->SetObject(CopyStorage)));
		ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Original))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		auto& Result = *reinterpret_cast<FString*>(CopyStorage); FString Suffix(TEXT("cd"));
		ASSERT_THAT(AreEqual(0, C->Prepare(Append))); ASSERT_THAT(AreEqual(0, C->SetObject(&Result)));
		ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Suffix))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		ASSERT_THAT(AreEqual(FString(TEXT("abcd")), Result)); ASSERT_THAT(AreEqual(FString(TEXT("ab")), Original));
		Result.~FString();
	}

	TEST_METHOD(ParseIntoArraySplitsACommaB)
	{
		FStringNameFixture F; ASSERT_THAT(IsTrue(F.Record(), *F.Diagnostic)); FStringNameContext C(F);
		FString Input(TEXT("a,b")), Delimiter(TEXT(",")); TArray<FString> Output;
		auto* Split = F.Function(TEXT("FString"), TEXT("int ParseIntoArray(TArray<FString>& OutArray, const FString& Delimiter, bool bCullEmpty = true) const"));
		ASSERT_THAT(IsNotNull(Split)); ASSERT_THAT(AreEqual(0, C->Prepare(Split))); ASSERT_THAT(AreEqual(0, C->SetObject(&Input)));
		ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Output))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(1, &Delimiter)));
		ASSERT_THAT(AreEqual(0, C->SetArgByte(2, 1))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		ASSERT_THAT(AreEqual(2, int32(C->GetReturnDWord()))); ASSERT_THAT(AreEqual(2, Output.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("a")), Output[0])); ASSERT_THAT(AreEqual(FString(TEXT("b")), Output[1]));
	}

	TEST_METHOD(JoinAAndBWithDash)
	{
		FStringNameFixture F; ASSERT_THAT(IsTrue(F.Record(), *F.Diagnostic)); FStringNameContext C(F);
		TArray<FString> Parts{TEXT("a"), TEXT("b")}; FString Separator(TEXT("-"));
		auto* Join = F.Function(TEXT(""), TEXT("FString Join(const TArray<FString>& StringArray, const FString& Separator) no_discard"), TEXT("FString"));
		ASSERT_THAT(IsNotNull(Join)); ASSERT_THAT(AreEqual(0, C->Prepare(Join))); ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Parts)));
		ASSERT_THAT(AreEqual(0, C->SetArgAddress(1, &Separator))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		const auto* Result = static_cast<const FString*>(C->GetReturnObject()); ASSERT_THAT(IsNotNull(Result));
		ASSERT_THAT(AreEqual(FString(TEXT("a-b")), *Result));
	}

	TEST_METHOD(FNameEqualityAndStringRoundTrip)
	{
		FStringNameFixture F; ASSERT_THAT(IsTrue(F.Record(), *F.Diagnostic)); FStringNameContext C(F);
		FString Text(TEXT("RuntimeBindingName")); alignas(FName) uint8 NameStorage[sizeof(FName)];
		auto* Construct = F.Function(TEXT("FName"), TEXT("void f(const FString& Other)"));
		auto* Equals = F.Function(TEXT("FName"), TEXT("bool opEquals(const FName& Other) const"));
		auto* Plain = F.Function(TEXT("FName"), TEXT("FString GetPlainNameString() const"));
		ASSERT_THAT(IsNotNull(Construct)); ASSERT_THAT(IsNotNull(Equals)); ASSERT_THAT(IsNotNull(Plain));
		ASSERT_THAT(AreEqual(0, C->Prepare(Construct))); ASSERT_THAT(AreEqual(0, C->SetObject(NameStorage)));
		ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Text))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		auto& Name = *reinterpret_cast<FName*>(NameStorage); FName Same(TEXT("RuntimeBindingName"));
		ASSERT_THAT(AreEqual(0, C->Prepare(Equals))); ASSERT_THAT(AreEqual(0, C->SetObject(&Name)));
		ASSERT_THAT(AreEqual(0, C->SetArgAddress(0, &Same))); ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute()));
		ASSERT_THAT(IsTrue(C->GetReturnByte() != 0)); ASSERT_THAT(AreEqual(0, C->Prepare(Plain))); ASSERT_THAT(AreEqual(0, C->SetObject(&Name)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_FINISHED), C->Execute())); const auto* Result = static_cast<const FString*>(C->GetReturnObject());
		ASSERT_THAT(IsNotNull(Result)); ASSERT_THAT(AreEqual(Text, *Result)); Name.~FName();
	}

	TEST_METHOD(InvalidStringIndexRaisesTheRuntimeIndexContract)
	{
		FStringNameFixture F; ASSERT_THAT(IsTrue(F.Record(), *F.Diagnostic)); FStringNameContext C(F); FString Value(TEXT("ab"));
		auto* Index = F.Function(TEXT("FString"), TEXT("int16& opIndex(int32 Index)")); ASSERT_THAT(IsNotNull(Index));
		ASSERT_THAT(AreEqual(0, C->Prepare(Index))); ASSERT_THAT(AreEqual(0, C->SetObject(&Value))); ASSERT_THAT(AreEqual(0, C->SetArgDWord(0, 2)));
		ASSERT_THAT(AreEqual(int(asEXECUTION_EXCEPTION), C->Execute()));
		ASSERT_THAT(IsTrue(FString(C->GetExceptionString()).Contains(TEXT("index"), ESearchCase::IgnoreCase)));
		ASSERT_THAT(AreEqual(FString(TEXT("ab")), Value));
	}
};
#endif
