#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_generic.h"
#include "as_vm_object.h"
#include "HAL/PlatformTime.h"
#include <atomic>
#include <thread>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(VMRuntimeDrain, "Angelscript.UnitTest.NativeEngine",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
 inline static int32 Destructors = 0;
 inline static asCObjectType* CycleType = nullptr;
 inline static int32 CycleOffset = 0;
 inline static bool CycleCreated = false;
 inline static std::atomic<bool> Entered{false};
 inline static std::atomic<bool> Continue{false};
 inline static std::atomic<bool> AbortContinuation{false};
 inline static asCObjectType* CleanupTemporaryType = nullptr;
 inline static int32 CleanupTemporaryCount = 0;
 static void Count(asIScriptGeneric*)
 {
  ++Destructors;
  if (CleanupTemporaryType)
  {
   if (void* Temporary = asVmAllocateObject(CleanupTemporaryType))
   {
    ++CleanupTemporaryCount;
    asVmRelease(Temporary);
   }
  }
 }
 static void Yield(asIScriptGeneric*) { if (auto* C = asGetActiveContext()) C->Suspend(); }
 static void Block(asIScriptGeneric*)
 {
  Entered = true;
  while (!Continue.load()) std::this_thread::yield();
  if (AbortContinuation.load())
   if (auto* C = asGetActiveContext()) C->SetException("missing-retained-continuation");
 }
 static void ShutdownWithCycle(asIScriptGeneric* G)
 {
  void* Object = asVmAllocateObject(CycleType);
  CycleCreated = Object != nullptr;
  if (Object)
  {
   *reinterpret_cast<void**>(static_cast<char*>(Object) + CycleOffset) = Object;
   asVmAddRef(Object);
   asVmRelease(Object);
  }
  G->GetEngine()->ShutDownAndRelease();
 }
 struct FCase
 {
  FDetachedDefinitionFixture F;
  FNativeEngineOwner E;
  asCObjectType* Type = nullptr;
  asCObjectType* TemporaryType = nullptr;
  asCScriptFunction* Dtor = nullptr;
  asCScriptFunction* Counter = nullptr;
  asCScriptFunction* Helper = nullptr;
  asCScriptFunction* Run = nullptr;
  asCScriptFunction* Suspend = nullptr;
  asCScriptFunction* Shutdown = nullptr;
  asCScriptFunction* Wait = nullptr;
  asCObjectProperty* Next = nullptr;
  bool Link(asCByteCodeImageBuilder& B)
  {
   TUniquePtr<asCByteCodeImage> Image;
   if (B.Finish(Image) != asEByteCodeImageStatus::Succeeded) return false;
   asCExecutableSnapshot* Snapshot = nullptr;
   const auto Status = E.Ptr->LinkByteCodeImage(*Image, Snapshot);
   if (Snapshot) Snapshot->Release(); // Only the Engine retains published code.
   return Status == asEByteCodeLinkStatus::Succeeded;
  }
  bool EmitCallBody(asCScriptFunction* Function, asCScriptFunction* Target)
  {
   asCByteCodeImageBuilder B; asUINT FunctionSlot = 0, TargetSlot = 0;
   if (B.AddFunctionRequirement(*Function, FunctionSlot) != asEByteCodeImageStatus::Succeeded
    || B.AddFunctionRequirement(*Target, TargetSlot) != asEByteCodeImageStatus::Succeeded
    || B.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, ByteArgumentDwords(*Function)) != asEByteCodeImageStatus::Succeeded) return false;
   const auto Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, TargetSlot);
   const auto Ret = ByteImm16(ByteArgumentDwords(*Function));
   return B.Emit(Target->funcType == asFUNC_SYSTEM ? asBC_CALLSYS : asBC_CALL, MakeArrayView(&Call, 1)) == asEByteCodeImageStatus::Succeeded
    && B.Emit(asBC_RET, MakeArrayView(&Ret, 1)) == asEByteCodeImageStatus::Succeeded
    && B.EndFunction() == asEByteCodeImageStatus::Succeeded && Link(B);
  }
  bool Start(bool ScriptDestructor, bool Gc, bool CrossBody = false, bool Cycle = false)
  {
   Destructors = 0;
   CleanupTemporaryType = nullptr; CleanupTemporaryCount = 0;
   TemporaryType = F.DefineType(TEXT("Temporary"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
   Type = F.DefineType(TEXT("Owned"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT | (Gc ? asOBJ_GC : 0));
   if (!Type) return false;
   if (Cycle) { Next = F.DefineProperty(*Type, "Next", asCDataType::CreateObjectHandle(Type, false)); if (!Next) return false; }
   Dtor = F.DefineFunction(TEXT("Destruct"), TEXT(""), ScriptDestructor ? asFUNC_SCRIPT : asFUNC_SYSTEM, F.Image->Primitive(ttVoid), {}, Type);
   Counter = F.DefineFunction(TEXT("Count"), TEXT(""), asFUNC_SYSTEM, F.Image->Primitive(ttVoid), {});
   Helper = F.DefineFunction(TEXT("Helper"), TEXT(""), asFUNC_SCRIPT, F.Image->Primitive(ttVoid), {});
   Run = F.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, F.Image->Primitive(ttInt), {});
   Suspend = F.DefineFunction(TEXT("Suspend"), TEXT(""), asFUNC_SYSTEM, F.Image->Primitive(ttVoid), {});
   Shutdown = F.DefineFunction(TEXT("Shutdown"), TEXT(""), asFUNC_SYSTEM, F.Image->Primitive(ttVoid), {});
   Wait = F.DefineFunction(TEXT("Wait"), TEXT(""), asFUNC_SYSTEM, F.Image->Primitive(ttVoid), {});
   if (!Dtor || !Counter || !Helper || !Run || !Suspend || !Shutdown || !Wait
    || F.Image->AddBehaviour(*Type, asBEHAVE_DESTRUCT, *Dtor) != asEMetadataResult::Succeeded || !F.FinalizeAndFreeze()
    || E.Ptr->RegisterMetadataImage(MoveTemp(F.Image)) != asEMetadataRegistrationResult::Succeeded) return false;
   E.Ptr->SetEngineProperty(asEP_AUTO_GARBAGE_COLLECT, 0);
   if (E.Ptr->BindNativeFunction(Counter->GetStableKey(), asFUNCTION(Count), asCALL_GENERIC) != asEByteCodeLinkStatus::Succeeded
    || E.Ptr->BindNativeFunction(Suspend->GetStableKey(), asFUNCTION(Yield), asCALL_GENERIC) != asEByteCodeLinkStatus::Succeeded
    || E.Ptr->BindNativeFunction(Shutdown->GetStableKey(), asFUNCTION(ShutdownWithCycle), asCALL_GENERIC) != asEByteCodeLinkStatus::Succeeded
    || E.Ptr->BindNativeFunction(Wait->GetStableKey(), asFUNCTION(Block), asCALL_GENERIC) != asEByteCodeLinkStatus::Succeeded) return false;
   if (CrossBody && !EmitCallBody(Helper, Counter)) return false;
   if (ScriptDestructor) return EmitCallBody(Dtor, CrossBody ? Helper : Counter);
   return E.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(Count), asCALL_GENERIC) == asEByteCodeLinkStatus::Succeeded;
  }
  bool EmitRun(bool SuspendedObject)
  {
   asCByteCodeImageBuilder B; asUINT RunSlot=0, TypeSlot=0, CallSlot=0, HelperSlot=0;
   if (B.AddFunctionRequirement(*Run, RunSlot) != asEByteCodeImageStatus::Succeeded
    || B.AddTypeRequirement(*Type, TypeSlot) != asEByteCodeImageStatus::Succeeded
    || B.AddFunctionRequirement(*(SuspendedObject ? Suspend : Wait), CallSlot) != asEByteCodeImageStatus::Succeeded
    || (!SuspendedObject && B.AddFunctionRequirement(*Helper, HelperSlot) != asEByteCodeImageStatus::Succeeded)
    || B.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, 0) != asEByteCodeImageStatus::Succeeded) return false;
   const auto Object = ByteLocal(2);
   if (SuspendedObject)
   {
    const asSByteCodeOperand Alloc[] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
    if (B.Emit(asBC_PSF, MakeArrayView(&Object,1)) != asEByteCodeImageStatus::Succeeded || B.Emit(asBC_ALLOC, MakeArrayView(Alloc)) != asEByteCodeImageStatus::Succeeded) return false;
    asSByteCodeFrameDescriptor Frame; Frame.LocalDwords=16; Frame.InitializedObjectSlots.Add(2);
    if (B.SetFrameDescriptor(Frame) != asEByteCodeImageStatus::Succeeded) return false;
    asSByteCodeCleanupAction Cleanup; Cleanup.Kind=asEByteCodeCleanupKind::DestroyObject; Cleanup.Path=asEByteCodeCleanupPath::Exception; Cleanup.InstructionIndex=1; Cleanup.FrameOffset=2; Cleanup.TypeSlot=TypeSlot;
    if (B.AddCleanupAction(Cleanup) != asEByteCodeImageStatus::Succeeded) return false;
    Cleanup.Path=asEByteCodeCleanupPath::Normal; Cleanup.InstructionIndex=3;
    if (B.AddCleanupAction(Cleanup) != asEByteCodeImageStatus::Succeeded) return false;
   }
   const auto Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot,CallSlot);
   if (B.Emit(asBC_CALLSYS,MakeArrayView(&Call,1)) != asEByteCodeImageStatus::Succeeded) return false;
   if (SuspendedObject)
   {
    const asSByteCodeOperand Free[] = {Object,ByteSlot(asEByteCodeOperandKind::TypeSlot,TypeSlot)};
    if (B.Emit(asBC_FREE,MakeArrayView(Free)) != asEByteCodeImageStatus::Succeeded) return false;
   }
   else
   {
    const auto Invoke = ByteSlot(asEByteCodeOperandKind::FunctionSlot,HelperSlot);
    if (B.Emit(asBC_CALL,MakeArrayView(&Invoke,1)) != asEByteCodeImageStatus::Succeeded) return false;
   }
   const auto Acc = ByteLocal(1); const asSByteCodeOperand Set[] = {Acc,ByteImm32(42)}; const auto Ret=ByteImm16(0);
   return B.Emit(asBC_SetV4,MakeArrayView(Set)) == asEByteCodeImageStatus::Succeeded
    && B.Emit(asBC_CpyVtoR4,MakeArrayView(&Acc,1)) == asEByteCodeImageStatus::Succeeded
    && B.Emit(asBC_RET,MakeArrayView(&Ret,1)) == asEByteCodeImageStatus::Succeeded
    && B.EndFunction() == asEByteCodeImageStatus::Succeeded && Link(B);
  }
 };
 bool GuardFinalOwnerRelease(FCase& T, void*& Object)
 {
  const int CountWithProbe = T.E.Ptr->AddRef(); T.E.Ptr->Release();
  if (CountWithProbe < 3)
  {
   asVmRelease(Object); Object = nullptr; T.E.Ptr->GarbageCollect();
   return false; // Clean before assertion RED; never dereference a dead Engine.
  }
  return true;
 }
public:
 TEST_METHOD(GlobalStorageCannotBeReboundDuringRetainedObjectDrain)
 {
  FCase T;
  ASSERT_THAT(IsTrue(T.F.BindContext()));
  AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Descriptor;
  Descriptor.StableScope=T.F.Scope; Descriptor.Name=TEXT("RetainedCell");
  Descriptor.Kind=AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::GlobalVariable;
  asSStableKey Key, TypeKey;
  ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
   T.F.Context->GetIdentityRegistry().InternDeclaration(Descriptor,Key)));
  AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor TypeDescriptor;
  TypeDescriptor.Kind=AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
  TypeDescriptor.Primitive=AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32;
  TypeDescriptor.Qualifiers=AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const;
  ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
   T.F.Context->GetIdentityRegistry().InternTypeUse(TypeDescriptor,TypeKey)));
  asCGlobalProperty* Global=nullptr;
  ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
   T.F.Image->CreateGlobalProperty(Key,TypeKey,"RetainedCell","",T.F.Image->Primitive(ttInt,true),Global)));
  ASSERT_THAT(IsTrue(T.Start(false,false)));
  int32 Original=41, Replacement=99;
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,T.E.Ptr->BindGlobalStorage(Key,&Original)));
  void* Object=asVmAllocateObject(T.Type); ASSERT_THAT(IsNotNull(Object));
  T.E.Ptr->AddRef(); T.E.Ptr->ShutDownAndRelease();
  const auto Status=T.E.Ptr->BindGlobalStorage(Key,&Replacement);
  const bool Preserved=T.E.Ptr->FindBoundGlobalStorage(Key)==&Original;
  asVmRelease(Object);
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Retired,Status));
  ASSERT_THAT(IsTrue(Preserved));
  ASSERT_THAT(AreEqual(1,Destructors));
 }
 TEST_METHOD(NativeCleanupCanAllocateAndReleaseTemporaryAfterShutdown)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start(false,false)));
  void* Object=asVmAllocateObject(T.Type); ASSERT_THAT(IsNotNull(Object));
  CleanupTemporaryType=T.TemporaryType;
  T.E.Ptr->AddRef(); T.E.Ptr->ShutDownAndRelease();
  void* Rejected=asVmAllocateObject(T.TemporaryType);
  const bool PublicAllocationClosed=Rejected==nullptr;
  if (Rejected) asVmRelease(Rejected);
  asVmRelease(Object); CleanupTemporaryType=nullptr;
  ASSERT_THAT(IsTrue(PublicAllocationClosed));
  ASSERT_THAT(AreEqual(1,Destructors));
  ASSERT_THAT(AreEqual(1,CleanupTemporaryCount));
 }
 TEST_METHOD(ScriptCleanupCanAllocateAndReleaseTemporaryAfterShutdown)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start(true,true,true)));
  void* Object=asVmAllocateObject(T.Type); ASSERT_THAT(IsNotNull(Object));
  CleanupTemporaryType=T.TemporaryType;
  T.E.Ptr->AddRef(); T.E.Ptr->ShutDownAndRelease();
  asVmRelease(Object); T.E.Ptr->GarbageCollect(); CleanupTemporaryType=nullptr;
  ASSERT_THAT(AreEqual(1,Destructors));
  ASSERT_THAT(AreEqual(1,CleanupTemporaryCount));
 }
 TEST_METHOD(CallerSnapshotDetachesWhenLastRuntimeObjectDrains)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start(true,false,true)));
  auto* Body=T.E.Ptr->FindPublishedExecutable(T.Dtor->GetStableKey());
  ASSERT_THAT(IsNotNull(Body));
  auto* Snapshot=Body->Owner; Snapshot->AddRef();
  void* Object=asVmAllocateObject(T.Type); ASSERT_THAT(IsNotNull(Object));
  T.F.Reset(); T.E.Reset(); asVmRelease(Object);
  const bool Detached=Snapshot->Engine==nullptr;
  if (!Detached) Snapshot->Engine=nullptr;
  Snapshot->Release();
  ASSERT_THAT(IsTrue(Detached));
  ASSERT_THAT(AreEqual(1,Destructors));
 }
 TEST_METHOD(PayloadAddsAndReleasesItsOwnRuntimeLease)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start(false,false)));
  const int Before=T.E.Ptr->AddRef(); T.E.Ptr->Release();
  void* Object=asVmAllocateObject(T.Type); ASSERT_THAT(IsNotNull(Object));
  const int During=T.E.Ptr->AddRef(); T.E.Ptr->Release();
  asVmRelease(Object);
  const int After=T.E.Ptr->AddRef(); T.E.Ptr->Release();
  ASSERT_THAT(AreEqual(Before+1,During));
  ASSERT_THAT(AreEqual(Before,After));
  ASSERT_THAT(AreEqual(1,Destructors));
 }
 TEST_METHOD(RetainedScriptDestructorRunsAfterRetirementRequest)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start(true,true)));
  void* Object=asVmAllocateObject(T.Type); ASSERT_THAT(IsNotNull(Object));
  T.E.Ptr->AddRef(); T.E.Ptr->ShutDownAndRelease(); // Safe baseline keeps one explicit fixture owner.
  ASSERT_THAT(AreEqual(0,Destructors));
  asVmRelease(Object); T.E.Ptr->GarbageCollect();
  ASSERT_THAT(AreEqual(1,Destructors));
  ASSERT_THAT(IsNull(T.E.Ptr->FindPublishedExecutable(T.Dtor->GetStableKey())));
 }
 TEST_METHOD(OnlyObjectRetainsNativeCleanupAfterHostAndProducerRelease)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start(false,false)));
  void* Object=asVmAllocateObject(T.Type); ASSERT_THAT(IsNotNull(Object));
  ASSERT_THAT(IsTrue(GuardFinalOwnerRelease(T,Object)));
  T.F.Reset(); T.E.Reset();
  asVmRelease(Object);
  ASSERT_THAT(AreEqual(1,Destructors));
 }
 TEST_METHOD(OnlyObjectRetainsScriptAndCrossSnapshotCleanupThenExpires)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start(true,true,true)));
  void* Object=asVmAllocateObject(T.Type); ASSERT_THAT(IsNotNull(Object));
  ASSERT_THAT(IsTrue(GuardFinalOwnerRelease(T,Object)));
  T.F.Reset(); T.E.Reset();
  asVmRelease(Object); // Shutdown drain must not require a hidden extra Engine owner or explicit GC call.
  ASSERT_THAT(AreEqual(1,Destructors));
 }
 TEST_METHOD(PublicAdmissionsStayClosedWhileNativeCleanupRemainsValid)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start(false,true)));
  auto* Prepared=T.E.Ptr->CreateContext(); ASSERT_THAT(AreEqual(0,Prepared->Prepare(T.Counter)));
  void* Object=asVmAllocateObject(T.Type); ASSERT_THAT(IsNotNull(Object));
  T.E.Ptr->AddRef(); T.E.Ptr->ShutDownAndRelease();
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Retired,T.E.Ptr->BindNativeFunction(T.Counter->GetStableKey(),asFUNCTION(Count),asCALL_GENERIC)));
  ASSERT_THAT(AreNotEqual(asEXECUTION_FINISHED,Prepared->Execute()));
  auto* New=T.E.Ptr->CreateContext(); ASSERT_THAT(AreNotEqual(0,New->Prepare(T.Counter))); New->Release();
  asVmRelease(Object); T.E.Ptr->GarbageCollect();
  ASSERT_THAT(AreEqual(1,Destructors));
  Prepared->Release();
 }
 TEST_METHOD(CallbackShutdownCollectsUnreachableCycleAfterOuterReturn)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start(false,true,false,true)));
  CycleType=T.Type; CycleOffset=T.Next->byteOffset; CycleCreated=false;
  auto* C=T.E.Ptr->CreateContext(); ASSERT_THAT(AreEqual(0,C->Prepare(T.Shutdown)));
  T.E.Ptr->AddRef(); // Safe RED owner; actual only-object ownership is covered separately.
  const int Result=C->Execute(); const int CountAtReturn=Destructors;
  T.E.Ptr->GarbageCollect(); C->Release(); CycleType=nullptr;
  ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,Result));
  ASSERT_THAT(IsTrue(CycleCreated));
  ASSERT_THAT(AreEqual(1,CountAtReturn));
  ASSERT_THAT(AreEqual(1,Destructors));
 }
 TEST_METHOD(SuspendedScriptRootUnwindsWithItsCleanupBinding)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start(true,true)));
  ASSERT_THAT(IsTrue(T.EmitRun(true)));
  auto* C=T.E.Ptr->CreateContext(); ASSERT_THAT(AreEqual(0,C->Prepare(T.Run)));
  ASSERT_THAT(AreEqual(asEXECUTION_SUSPENDED,C->Execute()));
  T.E.Ptr->AddRef(); T.E.Ptr->ShutDownAndRelease();
  C->Abort(); C->Unprepare(); T.E.Ptr->GarbageCollect();
  C->Release();
  ASSERT_THAT(AreEqual(1,Destructors));
 }
 TEST_METHOD(ConcurrentShutdownRetainsActiveCrossSnapshotContinuation)
 {
  FCase T; ASSERT_THAT(IsTrue(T.Start(false,false,true)));
  ASSERT_THAT(IsTrue(T.EmitRun(false)));
  Entered=false; Continue=false; AbortContinuation=false; std::atomic<int> Prepared{-999},Result{-999},Value{0};
  T.E.Ptr->AddRef(); auto* Engine=T.E.Ptr;
  std::thread Worker([&]
  {
   auto* C=Engine->CreateContext(); Prepared=C->Prepare(T.Run);
   if (Prepared==0) { Result=C->Execute(); Value=static_cast<int>(C->GetReturnDWord()); }
   C->Release();
  });
  const double Deadline=FPlatformTime::Seconds()+5.0;
  while (!Entered.load() && FPlatformTime::Seconds()<Deadline) std::this_thread::yield();
  const bool DidEnter=Entered.load();
  Engine->ShutDownAndRelease();
  const bool RetainedContinuation = Engine->FindPublishedExecutable(T.Helper->GetStableKey()) != nullptr;
  AbortContinuation = !RetainedContinuation; // Stop the old runtime before it dispatches through removed IDs.
  Continue=true; Worker.join();
  ASSERT_THAT(IsTrue(DidEnter));
  ASSERT_THAT(IsTrue(RetainedContinuation));
  ASSERT_THAT(AreEqual(0,Prepared.load()));
  ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,Result.load()));
  ASSERT_THAT(AreEqual(42,Value.load()));
  ASSERT_THAT(AreEqual(1,Destructors));
 }
};
#endif
