#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_bytecode_verifier.h"
#include "as_generic.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(VMReturnAdmission, "Angelscript.UnitTest.NativeEngine",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
 struct FReturnCase
 {
  FDetachedDefinitionFixture Definitions;
  asCScriptFunction* Function = nullptr;
  bool Define(eTokenType Token = ttInt, asDWORD Mode = asTM_NONE, bool bMethod = false, eTokenType Return = ttInt, bool bStackReturn = false)
  {
   asCObjectType* Owner = bMethod ? Definitions.DefineType(TEXT("Item"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT) : nullptr;
   asSMetadataParameter Param;
   Param.Type = Definitions.Set->Primitive(Token);
   Param.Flags = Mode;
   if (Mode != asTM_NONE) Param.Type.MakeReference(true);
   asCDataType ReturnType = Definitions.Set->Primitive(Return);
   if (bStackReturn)
   {
    auto* Value = Definitions.DefineType(TEXT("ValueResult"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
    if (!Value || !Definitions.DefineProperty(*Value, "x", Definitions.Set->Primitive(ttInt))) return false;
    ReturnType = asCDataType::CreateType(Value, false);
   }
   Function = Definitions.DefineFunction(TEXT("Value"), TEXT(""), asFUNC_SCRIPT, ReturnType, MakeArrayView(&Param, 1), Owner);
   return Function && Definitions.FinalizeAndFreeze();
  }
  TUniquePtr<asCByteCodeImage> Make(asUINT Parameters, int16 Pop, bool bRequirement = true, bool bShorthand = false)
  {
   asCByteCodeImageBuilder Builder;
   asUINT Slot = 0;
   if (bRequirement && Builder.AddFunctionRequirement(*Function, Slot) != asEByteCodeImageStatus::Succeeded) return nullptr;
   if (Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, Parameters) != asEByteCodeImageStatus::Succeeded) return nullptr;
   if (bShorthand)
   {
    auto Value = ByteImm32(42);
    if (Builder.Emit(asBC_PshC4, MakeArrayView(&Value, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
   }
   auto Ret = ByteImm16(Pop);
   if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded || Builder.EndFunction() != asEByteCodeImageStatus::Succeeded) return nullptr;
   TUniquePtr<asCByteCodeImage> Image;
   if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded) return nullptr;
   return Image;
  }
 };
 void RejectCount(int16 Pop)
 {
  FReturnCase Case;
  ASSERT_THAT(IsTrue(Case.Define()));
  auto Image = Case.Make(1, Pop);
  ASSERT_THAT(IsNotNull(Image.Get()));
  const auto Fault = asCByteCodeVerifier::VerifyWithFault(*Image);
  ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, Fault.Status));
  ASSERT_THAT(AreEqual(0, Fault.InstructionIndex));
  TArray<uint8> Bytes;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
  TUniquePtr<asCByteCodeImage> Decoded;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::InvalidOperand, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
  ASSERT_THAT(IsNull(Decoded.Get()));
 }
 inline static TArray<int32> Observed;
 inline static TArray<asDWORD*> StackPositions;
 static void Observe(asIScriptGeneric* Generic)
 {
  Observed.Add(static_cast<int32>(Generic->GetArgDWord(0)));
  StackPositions.Add(static_cast<asCContext*>(asGetActiveContext())->m_regs.stackPointer);
 }
public:
 TEST_METHOD(ReturnCannotPopFewerArguments) { RejectCount(0); }
 TEST_METHOD(ReturnCannotPopMoreArguments) { RejectCount(2); }
 TEST_METHOD(ReturnCannotHaveNegativePop) { RejectCount(-1); }
 TEST_METHOD(BodyParameterSizeMustMatchRealWideRequirement)
 {
  FReturnCase Case;
  ASSERT_THAT(IsTrue(Case.Define(ttInt64)));
  for (asUINT Count : {1u, 3u})
  {
   auto Image = Case.Make(Count, static_cast<int16>(Count));
   ASSERT_THAT(IsNotNull(Image.Get()));
   ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
  }
  auto Valid = Case.Make(2, 2);
  ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Valid)));
 }
 TEST_METHOD(BodyIncludesReferenceStorage)
 {
  FReturnCase Case;
  ASSERT_THAT(IsTrue(Case.Define(ttInt, asTM_INREF)));
  auto Invalid = Case.Make(1, 1);
  ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Invalid)));
  auto Valid = Case.Make(AS_PTR_SIZE, AS_PTR_SIZE);
  ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Valid)));
 }
 TEST_METHOD(MethodBodyIncludesReceiverStorage)
 {
  FReturnCase Case;
  ASSERT_THAT(IsTrue(Case.Define(ttInt, asTM_NONE, true)));
  auto Invalid = Case.Make(1, 1);
  ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Invalid)));
  auto Valid = Case.Make(1 + AS_PTR_SIZE, 1 + AS_PTR_SIZE);
  ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Valid)));
 }
 TEST_METHOD(BodyIncludesHiddenReturnDestination)
 {
  FReturnCase Case;
  ASSERT_THAT(IsTrue(Case.Define(ttInt, asTM_NONE, false, ttInt, true)));
  ASSERT_THAT(IsTrue(Case.Function->DoesReturnOnStack()));
  auto Invalid = Case.Make(1, 1);
  ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Invalid)));
  auto Valid = Case.Make(1 + AS_PTR_SIZE, 1 + AS_PTR_SIZE);
  ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Valid)));
 }
 TEST_METHOD(TargetChecksBodyWithoutInImageRequirement)
 {
  FReturnCase Case;
  ASSERT_THAT(IsTrue(Case.Define(ttInt64)));
  auto Image = Case.Make(1, 1, false);
  ASSERT_THAT(IsNotNull(Image.Get()));
  ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
  FNativeEngineOwner Engine;
  ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->InstallDefinitionSet(Case.Definitions.TakeSet())));
  asCExecutableSnapshot* Snapshot = nullptr;
  const auto Status = asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot);
  if (Snapshot) Snapshot->Release();
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, Status));
  ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Case.Function->GetStableKey())));
 }
 TEST_METHOD(ScalarShorthandRetainsMethodArgumentPop)
 {
  FReturnCase Case;
  ASSERT_THAT(IsTrue(Case.Define(ttInt, asTM_NONE, true)));
  auto Image = Case.Make(1 + AS_PTR_SIZE, 1 + AS_PTR_SIZE, true, true);
  FNativeEngineOwner Engine;
  ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->InstallDefinitionSet(Case.Definitions.TakeSet())));
  asCExecutableSnapshot* Snapshot = nullptr;
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
  const auto* Executable = Engine.Ptr->FindPublishedExecutable(Case.Function->GetStableKey());
  ASSERT_THAT(IsNotNull(Executable));
  const auto& Code = Executable->scriptData->byteCode;
  const asDWORD* Ret = &Code[Code.GetLength() - 1];
  const asWORD Pop = asBC_WORDARG0(Ret);
  Snapshot->Release();
  ASSERT_THAT(AreEqual(static_cast<asWORD>(1 + AS_PTR_SIZE), Pop));
 }
 TEST_METHOD(ScalarShorthandRejectsWideReturn)
 {
  FReturnCase Case;
  ASSERT_THAT(IsTrue(Case.Define(ttInt, asTM_NONE, false, ttInt64)));
  auto Image = Case.Make(1, 1, true, true);
  FNativeEngineOwner Engine;
  ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->InstallDefinitionSet(Case.Definitions.TakeSet())));
  asCExecutableSnapshot* Snapshot = nullptr;
  auto Status = asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot);
  if (Snapshot) Snapshot->Release();
  ASSERT_THAT(AreNotEqual(asEByteCodeLinkStatus::Succeeded, Status));
  ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Case.Function->GetStableKey())));
 }
 TEST_METHOD(RepeatedNestedCallsPreserveResultsArgumentsAndStackPosition)
 {
  FDetachedDefinitionFixture Definitions;
  asSMetadataParameter Param; Param.Type = Definitions.Set->Primitive(ttInt);
  auto* Probe = Definitions.DefineFunction(TEXT("Probe"), TEXT(""), asFUNC_SYSTEM, Definitions.Set->Primitive(ttVoid), MakeArrayView(&Param, 1));
  auto* Inner = Definitions.DefineFunction(TEXT("Inner"), TEXT(""), asFUNC_SCRIPT, Definitions.Set->Primitive(ttInt), MakeArrayView(&Param, 1));
  auto* Run = Definitions.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Definitions.Set->Primitive(ttInt), {});
  ASSERT_THAT(IsTrue(Definitions.FinalizeAndFreeze()));
  FNativeEngineOwner Engine;
  ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->InstallDefinitionSet(Definitions.TakeSet())));
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindNativeFunction(Probe->GetStableKey(), asFUNCTION(Observe), asCALL_GENERIC)));
  asCByteCodeImageBuilder Builder;
  asUINT ProbeSlot=0, InnerSlot=0, RunSlot=0;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Probe, ProbeSlot)));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Inner, InnerSlot)));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Inner->GetStableKey(), Inner->GetStableKey(), 4, 1)));
  auto Arg=ByteLocal(0), Call=ByteSlot(asEByteCodeOperandKind::FunctionSlot, ProbeSlot), Ret=ByteImm16(1);
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshV4, MakeArrayView(&Arg,1))));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call,1))));
  asSByteCodeOperand One[2]={ByteLocal(1),ByteImm32(1)}, Add[3]={ByteLocal(2),Arg,ByteLocal(1)};
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV4, MakeArrayView(One,2))));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDi, MakeArrayView(Add,3))));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(Add,1))));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret,1))));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 4, 0)));
  for (int32 Index=0; Index<2; ++Index)
  {
   auto Value=ByteImm32(Index==0 ? 10 : 20), Invoke=ByteSlot(asEByteCodeOperandKind::FunctionSlot, InnerSlot), Save=ByteLocal(Index+1);
   ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Value,1))));
   ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALL, MakeArrayView(&Invoke,1))));
   ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyRtoV4, MakeArrayView(&Save,1))));
  }
  asSByteCodeOperand Sum[3]={ByteLocal(3),ByteLocal(1),ByteLocal(2)};
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDi, MakeArrayView(Sum,3))));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(Sum,1))));
  Ret=ByteImm16(0);
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret,1))));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
  TUniquePtr<asCByteCodeImage> Image;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
  asCExecutableSnapshot* Snapshot=nullptr;
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image,Snapshot)));
  auto* Context=Engine.Ptr->CreateContext();
  Observed.Reset(); StackPositions.Reset();
  for (int32 Repeat=0;Repeat<3;++Repeat)
  {
   ASSERT_THAT(AreEqual(0,Context->Prepare(Run)));
   ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,Context->Execute()));
   ASSERT_THAT(AreEqual(32,static_cast<int32>(Context->GetReturnDWord())));
  }
  ASSERT_THAT(IsTrue(Observed==TArray<int32>({10,20,10,20,10,20})));
  ASSERT_THAT(AreEqual(6,StackPositions.Num()));
  for (auto* Position : StackPositions) ASSERT_THAT(IsTrue(Position==StackPositions[0]));
  Context->Release(); Snapshot->Release();
 }
};
#endif
