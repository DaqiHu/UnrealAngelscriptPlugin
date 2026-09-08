#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_generic.h"
#include <atomic>
#include <thread>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(VMNativeBindingLifetime, "Angelscript.UnitTest.NativeEngine",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
 static void First(asIScriptGeneric* G) { G->SetReturnDWord(41); }
 static void Second(asIScriptGeneric* G) { G->SetReturnDWord(42); }
 inline static std::weak_ptr<asSSystemFunctionInterface> ActiveGeneration;
 inline static bool ActiveSurvived = false;
 inline static asEByteCodeLinkStatus RebindStatus = asEByteCodeLinkStatus::InvalidArgument;
 static void Reentrant(asIScriptGeneric* G)
 {
  auto* E = static_cast<asCScriptEngine*>(G->GetEngine());
  auto* F = static_cast<asCScriptFunction*>(G->GetFunction());
  RebindStatus = E->BindNativeFunction(F->GetStableKey(), asFUNCTION(Second), asCALL_GENERIC);
  ActiveSurvived = !ActiveGeneration.expired();
  G->SetReturnDWord(41);
 }
 inline static int32 ObservedValue = 0;
 static void ReentrantValue(asIScriptGeneric* G)
 {
  ObservedValue = *static_cast<int32*>(G->GetArgObject(0));
  Reentrant(G);
 }
 struct FCase
 {
  FDetachedDefinitionFixture Definitions;
  asCScriptFunction* Function = nullptr;
  FNativeEngineOwner Engine;
  bool Start()
  {
   Function = Definitions.DefineFunction(TEXT("Native"), TEXT(""), asFUNC_SYSTEM, Definitions.Image->Primitive(ttInt), {});
   return Function && Definitions.FinalizeAndFreeze()
    && Engine.Ptr->RegisterMetadataImage(Definitions.Image) == asEMetadataRegistrationResult::Succeeded;
  }
 };
 int32 InvokeLease(FCase& T, const std::shared_ptr<asSSystemFunctionInterface>& Lease)
 {
  asCGeneric Generic(T.Engine.Ptr, T.Function, nullptr, nullptr);
  reinterpret_cast<void (*)(asIScriptGeneric*)>(Lease->func)(&Generic);
  return static_cast<int32>(Generic.returnVal);
 }
 void Execute(FCase& T, int32 Expected)
 {
  auto* C = T.Engine.Ptr->CreateContext();
  ASSERT_THAT(AreEqual(0, C->Prepare(T.Function)));
  ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, C->Execute()));
  ASSERT_THAT(AreEqual(Expected, static_cast<int32>(C->GetReturnDWord())));
  C->Release();
 }
public:
 TEST_METHOD(OldGenerationInvokesOriginalAfterReplacementAndExpiresOnRelease)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start()));
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, T.Engine.Ptr->BindNativeFunction(T.Function->GetStableKey(), asFUNCTION(First), asCALL_GENERIC)));
  auto Old = T.Engine.Ptr->AcquireSystemInterface(T.Function);
  ASSERT_THAT(IsNotNull(Old.get()));
  std::weak_ptr<asSSystemFunctionInterface> Weak = Old;
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, T.Engine.Ptr->BindNativeFunction(T.Function->GetStableKey(), asFUNCTION(Second), asCALL_GENERIC)));
  auto Current = T.Engine.Ptr->AcquireSystemInterface(T.Function);
  ASSERT_THAT(IsNotNull(Current.get()));
  ASSERT_THAT(IsTrue(Old != Current));
  ASSERT_THAT(AreEqual(41, InvokeLease(T, Old)));
  ASSERT_THAT(AreEqual(42, InvokeLease(T, Current)));
  Execute(T, 42);
  ASSERT_THAT(IsFalse(Weak.expired()));
  Old.reset();
  ASSERT_THAT(IsTrue(Weak.expired()));
 }
 TEST_METHOD(InvalidReplacementPreservesInstalledCallback)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start()));
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, T.Engine.Ptr->BindNativeFunction(T.Function->GetStableKey(), asFUNCTION(First), asCALL_GENERIC)));
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::InvalidArgument,
   T.Engine.Ptr->BindNativeFunction(T.Function->GetStableKey(), asFUNCTION(Second), asCALL_CDECL)));
  Execute(T, 41);
  ASSERT_THAT(IsNull(T.Function->sysFuncIntf));
 }
 TEST_METHOD(RetiredEngineRejectsReplacementBeforeBindingMutation)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start()));
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, T.Engine.Ptr->BindNativeFunction(T.Function->GetStableKey(), asFUNCTION(First), asCALL_GENERIC)));
  T.Engine.Ptr->RetireMetadataImages();
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Retired,
   T.Engine.Ptr->BindNativeFunction(T.Function->GetStableKey(), asFUNCTION(Second), asCALL_GENERIC)));
 }
 TEST_METHOD(ReentrantReplacementRetainsActiveDescriptorThroughReturn)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start()));
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, T.Engine.Ptr->BindNativeFunction(T.Function->GetStableKey(), asFUNCTION(Reentrant), asCALL_GENERIC)));
  auto Acquired = T.Engine.Ptr->AcquireSystemInterface(T.Function);
  ASSERT_THAT(IsNotNull(Acquired.get())); // Stop RED before any unowned self-replacement executes.
  ActiveGeneration = Acquired; Acquired.reset(); ActiveSurvived = false;
  Execute(T, 41);
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, RebindStatus));
  ASSERT_THAT(IsTrue(ActiveSurvived));
  ASSERT_THAT(IsTrue(ActiveGeneration.expired()));
  Execute(T, 42);
 }
 TEST_METHOD(ReentrantReplacementCleansTransferredValueExactlyOnce)
 {
  FDetachedDefinitionFixture F;
  auto* Value = F.DefineType(TEXT("Payload"), TEXT(""), asOBJ_VALUE | asOBJ_APP_CLASS);
  ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, F.Image->SetNativeLayout(*Value, sizeof(int32), alignof(int32))));
  asSMetadataParameter P; P.Type = asCDataType::CreateType(Value, false);
  auto* Function = F.DefineFunction(TEXT("Consume"), TEXT(""), asFUNC_SYSTEM, F.Image->Primitive(ttInt), MakeArrayView(&P, 1));
  ASSERT_THAT(IsTrue(F.FinalizeAndFreeze()));
  FNativeEngineOwner E;
  ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, E.Ptr->RegisterMetadataImage(F.Image)));
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, E.Ptr->BindNativeFunction(Function->GetStableKey(), asFUNCTION(ReentrantValue), asCALL_GENERIC)));
  ASSERT_THAT(AreEqual(0u, Function->parameterOffsets.GetLength()));
  ASSERT_THAT(AreEqual(0, Function->spaceNeededForArguments));
  auto Lease = E.Ptr->AcquireSystemInterface(Function);
  ASSERT_THAT(IsNotNull(Lease.get()));
  ActiveGeneration = Lease; Lease.reset(); ActiveSurvived = false; ObservedValue = 0;
  auto* C = E.Ptr->CreateContext();
  ASSERT_THAT(AreEqual(0, C->Prepare(Function)));
  auto* Payload = static_cast<int32*>(E.Ptr->CallAlloc(Value));
  ASSERT_THAT(IsNotNull(Payload)); *Payload = 41;
  ASSERT_THAT(AreEqual(0, C->SetArgObject(0, Payload)));
  void** Argument = static_cast<void**>(C->GetAddressOfArg(0));
  ASSERT_THAT(IsNotNull(Argument));
  ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, C->Execute()));
  ASSERT_THAT(AreEqual(41, ObservedValue));
  ASSERT_THAT(AreEqual(41, static_cast<int32>(C->GetReturnDWord())));
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, RebindStatus));
  ASSERT_THAT(IsTrue(ActiveSurvived));
  ASSERT_THAT(IsTrue(ActiveGeneration.expired()));
  ASSERT_THAT(AreEqual(0u, Function->parameterOffsets.GetLength()));
  ASSERT_THAT(AreEqual(0, Function->spaceNeededForArguments));
  ASSERT_THAT(IsNull(*Argument)); // The moved value was freed and the slot retired, including later Unprepare.
  ASSERT_THAT(AreEqual(0, C->Unprepare()));
  C->Release();
 }
 TEST_METHOD(ConcurrentReadersSeeCompleteRetainedGenerations)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start()));
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, T.Engine.Ptr->BindNativeFunction(T.Function->GetStableKey(), asFUNCTION(First), asCALL_GENERIC)));
  auto Guard = T.Engine.Ptr->AcquireSystemInterface(T.Function);
  ASSERT_THAT(IsNotNull(Guard.get()));
  std::atomic<bool> Start = false;
  std::atomic<int32> Errors = 0;
  auto Reader = [&]
  {
   while (!Start.load()) std::this_thread::yield();
   for (int32 N = 0; N < 200; ++N)
   {
    auto Lease = T.Engine.Ptr->AcquireSystemInterface(T.Function);
    if (!Lease) { ++Errors; continue; }
    const int32 Result = InvokeLease(T, Lease);
    if (Result != 41 && Result != 42) ++Errors;
   }
  };
  std::thread A(Reader), B(Reader);
  Start = true;
  for (int32 N = 0; N < 100; ++N)
   if (T.Engine.Ptr->BindNativeFunction(T.Function->GetStableKey(), N % 2 ? asFUNCTION(First) : asFUNCTION(Second), asCALL_GENERIC) != asEByteCodeLinkStatus::Succeeded) ++Errors;
  A.join(); B.join();
  ASSERT_THAT(AreEqual(0, Errors.load()));
  ASSERT_THAT(AreEqual(41, InvokeLease(T, Guard)));
 }
 TEST_METHOD(DescriptorLeaseKeepsDeclarationReadableAfterEngineAndProducerRelease)
 {
  std::shared_ptr<asSSystemFunctionInterface> Lease;
  asCScriptFunction* Function = nullptr;
  {
   FCase T; ASSERT_THAT(IsTrue(T.Start())); Function = T.Function;
   ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, T.Engine.Ptr->BindNativeFunction(Function->GetStableKey(), asFUNCTION(First), asCALL_GENERIC)));
   Lease = T.Engine.Ptr->AcquireSystemInterface(Function);
   ASSERT_THAT(IsNotNull(Lease.get()));
  }
  ASSERT_THAT(AreEqual(FString(TEXT("Native")), FString(UTF8_TO_TCHAR(Function->GetName()))));
  ASSERT_THAT(AreEqual(0u, Function->GetParamCount()));
  std::weak_ptr<asSSystemFunctionInterface> Weak = Lease;
  Lease.reset();
  ASSERT_THAT(IsTrue(Weak.expired()));
 }
};
#endif
