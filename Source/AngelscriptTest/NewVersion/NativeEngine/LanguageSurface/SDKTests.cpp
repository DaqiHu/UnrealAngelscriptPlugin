#include "CQTest.h"
#include "../Compiler/NativeSourceExecutionTestSupport.h"
#include "as_context.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace SDKSurfaceTests
{
 template<class T> constexpr bool HasShared = requires(T& V) { V.IsShared(); };
 template<class T> constexpr bool HasSetShared = requires(T& V) { V.SetShared(true); };
 template<class T> constexpr bool HasModules = requires(T& V) { V.GetModule("Old"); } || requires(T& V) { V.GetModuleCount(); } || requires(T& V) { V.GetModuleByIndex(0); } || requires(T& V) { V.DiscardModule("Old"); } || requires(T& V) { V.GetModuleFromFuncId(0); };
 template<class T> constexpr bool HasModulePointer = requires(T& V) { V.GetModule(); };
 template<class T> constexpr bool HasModuleName = requires(T& V) { V.GetModuleName(); };
 template<class T> constexpr bool HasCompile = requires(T& V) { V.AddScriptSection("Old", ""); } || requires(T& V) { V.Build(); } || requires(T& V) { V.CompileFunction("Old", "", 0, 0, nullptr); };
 template<class T> constexpr bool HasImports = requires(T& V) { V.BindAllImportedFunctions(); } || requires(T& V) { V.ClearImports(); } || requires(T& V) { V.ImportModule(nullptr); };
 static_assert(!HasShared<asIScriptFunction> && !HasShared<asCScriptFunction> && !HasShared<asCTypeInfo> && !HasSetShared<asCScriptFunction>);
 static_assert(!HasModules<asIScriptEngine> && !HasModules<asCScriptEngine>);
 static_assert(!HasModulePointer<asITypeInfo> && !HasModulePointer<asCTypeInfo> && !HasModulePointer<asIScriptFunction> && !HasModulePointer<asCScriptFunction>);
 static_assert(!HasModuleName<asIScriptFunction> && !HasModuleName<asCScriptFunction>);
 static_assert(!HasCompile<asIScriptModule> && !HasImports<asIScriptModule>);
 int32 Destructions = 0;
 void Mark(asIScriptGeneric*) { ++Destructions; }
 void Pause(asIScriptGeneric*) { if (auto* C = asGetActiveContext()) C->Suspend(); }
 void Add(asIScriptGeneric* G) { G->SetReturnDWord(G->GetArgDWord(0) + G->GetArgDWord(1)); }
 struct FHost
 {
  FDetachedDefinitionFixture Fixture;
  asCScriptFunction *MarkFn = nullptr, *PauseFn = nullptr, *AddFn = nullptr;
  bool Initialize()
  {
   Fixture.Scope = TEXT("sdk-surface-host");
   MarkFn = Fixture.DefineFunction(TEXT("Mark"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), {});
   PauseFn = Fixture.DefineFunction(TEXT("Pause"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), {});
   asSMetadataParameter P[2];
   P[0].Name = "A"; P[1].Name = "B"; P[0].Type = P[1].Type = Fixture.Image->Primitive(ttInt);
   AddFn = Fixture.DefineFunction(TEXT("Add"), TEXT("Host"), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(P, 2));
   return MarkFn && PauseFn && AddFn && Fixture.FinalizeAndFreeze();
  }
  bool Install(asCScriptEngine& E)
  {
   return E.RegisterMetadataImage(Fixture.Image) == asEMetadataRegistrationResult::Succeeded
    && E.BindNativeFunction(MarkFn->GetStableKey(), asFUNCTION(Mark), asCALL_GENERIC) == asEByteCodeLinkStatus::Succeeded
    && E.BindNativeFunction(PauseFn->GetStableKey(), asFUNCTION(Pause), asCALL_GENERIC) == asEByteCodeLinkStatus::Succeeded
    && E.BindNativeFunction(AddFn->GetStableKey(), asFUNCTION(Add), asCALL_GENERIC) == asEByteCodeLinkStatus::Succeeded;
  }
 };
}
TEST_CLASS_WITH_FLAGS(SDK, "Angelscript.UnitTest.NativeEngine.LanguageSurface", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
 TEST_METHOD(SharingPolicyInterfacesAreAbsent)
 {
  ASSERT_THAT(IsFalse(SDKSurfaceTests::HasShared<asIScriptFunction>));
  ASSERT_THAT(IsFalse(SDKSurfaceTests::HasShared<asCScriptFunction>));
  ASSERT_THAT(IsFalse(SDKSurfaceTests::HasShared<asCTypeInfo>));
  ASSERT_THAT(IsFalse(SDKSurfaceTests::HasSetShared<asCScriptFunction>));
 }
 TEST_METHOD(ModuleManagementAndMutableProvenanceAreAbsent)
 {
  ASSERT_THAT(IsFalse(SDKSurfaceTests::HasModules<asIScriptEngine>));
  ASSERT_THAT(IsFalse(SDKSurfaceTests::HasModules<asCScriptEngine>));
  ASSERT_THAT(IsFalse(SDKSurfaceTests::HasModulePointer<asITypeInfo>));
  ASSERT_THAT(IsFalse(SDKSurfaceTests::HasModulePointer<asCTypeInfo>));
  ASSERT_THAT(IsFalse(SDKSurfaceTests::HasModulePointer<asIScriptFunction>));
  ASSERT_THAT(IsFalse(SDKSurfaceTests::HasModulePointer<asCScriptFunction>));
  ASSERT_THAT(IsFalse(SDKSurfaceTests::HasModuleName<asIScriptFunction>));
  ASSERT_THAT(IsFalse(SDKSurfaceTests::HasModuleName<asCScriptFunction>));
 }
 TEST_METHOD(PublicModuleCompilationAndImportsAreAbsent)
 {
  ASSERT_THAT(IsFalse(SDKSurfaceTests::HasCompile<asIScriptModule>));
  ASSERT_THAT(IsFalse(SDKSurfaceTests::HasImports<asIScriptModule>));
 }
 TEST_METHOD(RemovedAccessorOptionCannotBeEnabledNumerically)
 {
  FNativeEngineOwner Engine;
  for (asPWORD Mode : {asPWORD(0), asPWORD(1), asPWORD(2), asPWORD(3)})
   ASSERT_THAT(AreEqual(asINVALID_ARG, Engine.Ptr->SetEngineProperty(static_cast<asEEngineProp>(14), Mode)));
  ASSERT_THAT(AreEqual(asPWORD(0), Engine.Ptr->GetEngineProperty(static_cast<asEEngineProp>(14))));
 }
 TEST_METHOD(SuppliedFilesAndOrdinaryGetterExecuteSeven)
 {
  FSourceExecutionInput Input(TArray<TPair<FString,FString>>{{TEXT("First.as"), TEXT("int get_Value(){return 7;}")}, {TEXT("Second.as"), TEXT("int Run(){return get_Value();}")}});
  asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
  ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
  auto Emitted = EmitFromSession(Builder);
  ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
  FNativeEngineOwner Engine;
  ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
  asCExecutableSnapshot* Snapshot = nullptr;
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
  ON_SCOPE_EXIT { Snapshot->Release(); };
  auto* Context = Engine.Ptr->CreateContext(); ON_SCOPE_EXIT { Context->Release(); };
  ASSERT_THAT(AreEqual(0, Context->Prepare(FindSourceFunction(Builder,"Run"))));
  ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
  ASSERT_THAT(AreEqual(7, int32(Context->GetReturnDWord())));
  FSourceExecutionInput Bad(TEXT("int get_Value(){return 7;} int Run(){return Value;}"));
  asCBuilder BadBuilder(Bad.Snapshot, Bad.Diagnostics, Bad.Options);
  ASSERT_THAT(IsFalse(BadBuilder.RunThrough()));
 }
 TEST_METHOD(FrozenProviderAndGenericCallSurviveFailedCandidate)
 {
  SDKSurfaceTests::FHost Host; ASSERT_THAT(IsTrue(Host.Initialize()));
  FSourceExecutionInput Input(TEXT("int Run(){return Host::Add(20,22);}"), Host.Fixture.Context, Host.Fixture.Image);
  asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
  ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
  auto Emitted = EmitFromSession(Builder); ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
  FNativeEngineOwner Engine; ASSERT_THAT(IsTrue(Host.Install(*Engine.Ptr)));
  ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
  asCExecutableSnapshot* Snapshot = nullptr;
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
  ON_SCOPE_EXIT { Snapshot->Release(); };
  FNativeEngineOwner MissingProvider;
  ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::ForeignEngine, MissingProvider.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
  asCExecutableSnapshot* Rejected = nullptr;
  ASSERT_THAT(IsTrue(MissingProvider.Ptr->LinkByteCodeImage(*Emitted.Image, Rejected) != asEByteCodeLinkStatus::Succeeded));
  ASSERT_THAT(IsNull(Rejected));
  // An unresolved replacement in this same engine cannot displace the installed executable.
  Emitted.Image->Functions[0].FunctionKey = asSStableKey{};
  ASSERT_THAT(IsTrue(Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Rejected) != asEByteCodeLinkStatus::Succeeded));
  ASSERT_THAT(IsNull(Rejected));
  auto* Context = Engine.Ptr->CreateContext(); ON_SCOPE_EXIT { Context->Release(); };
  ASSERT_THAT(AreEqual(0, Context->Prepare(FindSourceFunction(Builder,"Run"))));
  ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
  ASSERT_THAT(AreEqual(42, int32(Context->GetReturnDWord())));
 }
 TEST_METHOD(HostContextSuspendResumeAbortAndRuntimeFaultKeepCleanup)
 {
  SDKSurfaceTests::FHost Host; ASSERT_THAT(IsTrue(Host.Initialize()));
  FSourceExecutionInput Input(TEXT("class Box { Box(){} ~Box(){Host::Mark();} } int Run(){Box Hold(); Host::Pause(); return Host::Add(20,22);} int Fail(){Box Hold(); int Zero=0; return 1/Zero;} int Recovery(){return 97;}"), Host.Fixture.Context, Host.Fixture.Image);
  asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
  ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
  auto Emitted = EmitFromSession(Builder); ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
  FNativeEngineOwner Engine; ASSERT_THAT(IsTrue(Host.Install(*Engine.Ptr)));
  ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
  asCExecutableSnapshot* Snapshot = nullptr;
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
  ON_SCOPE_EXIT { Snapshot->Release(); };
  auto* Context = Engine.Ptr->CreateContext(); ON_SCOPE_EXIT { Context->Release(); };
  SDKSurfaceTests::Destructions = 0;
  ASSERT_THAT(AreEqual(0, Context->Prepare(FindSourceFunction(Builder,"Run"))));
  ASSERT_THAT(AreEqual(asEXECUTION_SUSPENDED, Context->Execute()));
  ASSERT_THAT(AreEqual(0, SDKSurfaceTests::Destructions));
  ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
  ASSERT_THAT(AreEqual(42, int32(Context->GetReturnDWord())));
  ASSERT_THAT(AreEqual(1, SDKSurfaceTests::Destructions));
  ASSERT_THAT(AreEqual(0, Context->Prepare(FindSourceFunction(Builder,"Run"))));
  ASSERT_THAT(AreEqual(asEXECUTION_SUSPENDED, Context->Execute()));
  ASSERT_THAT(AreEqual(0, Context->Abort()));
  ASSERT_THAT(AreEqual(2, SDKSurfaceTests::Destructions));
  ASSERT_THAT(AreEqual(0, Context->Prepare(FindSourceFunction(Builder,"Fail"))));
  ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
  ASSERT_THAT(AreEqual(3, SDKSurfaceTests::Destructions));
  ASSERT_THAT(AreEqual(0, Context->Prepare(FindSourceFunction(Builder,"Recovery"))));
  ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
  ASSERT_THAT(AreEqual(97, int32(Context->GetReturnDWord())));
 }
};
#endif

