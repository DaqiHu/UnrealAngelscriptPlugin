#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_bytecode_verifier.h"
#include "as_generic.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(VMManualAdmission, "Angelscript.UnitTest.NativeEngine.VM",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
 enum class EMutation { Kind, Immediate, WrongRole, MissingSlot, DeadOpcode, DeadFrame, DeadJump,
  DuplicateBody, DuplicateRequirement, ConflictingRequirement, HugeFrame, ZeroBodyKey, LateReturn };
 inline static int32 Calls=0;
 inline static int32 Cells[3]={91,17,92};
 static void Observe(asIScriptGeneric* G) { ++Calls; Cells[1]=static_cast<int32>(G->GetArgDWord(0)); G->SetReturnDWord(G->GetArgDWord(0)+1); }
 struct FManualCase
 {
  FDetachedDefinitionFixture F;
  asCScriptFunction *Run=nullptr,*Peer=nullptr,*Recovery=nullptr,*Probe=nullptr;
  asCObjectType* Type=nullptr;
  bool Define()
  {
   asSDefinitionParameter P; P.Type=F.Set->Primitive(ttInt);
   Probe=F.DefineFunction(TEXT("Probe"),TEXT(""),asFUNC_SYSTEM,F.Set->Primitive(ttInt),MakeArrayView(&P,1));
   Run=F.DefineFunction(TEXT("Run"),TEXT(""),asFUNC_SCRIPT,F.Set->Primitive(ttInt),{});
   Peer=F.DefineFunction(TEXT("Peer"),TEXT(""),asFUNC_SCRIPT,F.Set->Primitive(ttInt),{});
   Recovery=F.DefineFunction(TEXT("Recovery"),TEXT(""),asFUNC_SCRIPT,F.Set->Primitive(ttInt),{});
   Type=F.DefineType(TEXT("Item"),TEXT(""),asOBJ_REF | asOBJ_SCRIPT_OBJECT);
   return Probe && Run && Peer && Recovery && Type && F.FinalizeAndFreeze();
  }
  static bool Constant(asCByteCodeImageBuilder& B,asCScriptFunction* Fn,int32 Value)
  {
   auto Set=ByteLocal(1),Ret=ByteImm16(0); asSByteCodeOperand Ops[2]={Set,ByteImm32(Value)};
   return B.BeginFunction(Fn->GetStableKey(),Fn->GetStableKey(),4,0)==asEByteCodeImageStatus::Succeeded &&
    B.Emit(asBC_SetV4,MakeArrayView(Ops,2))==asEByteCodeImageStatus::Succeeded &&
    B.Emit(asBC_CpyVtoR4,MakeArrayView(&Set,1))==asEByteCodeImageStatus::Succeeded &&
    B.Emit(asBC_RET,MakeArrayView(&Ret,1))==asEByteCodeImageStatus::Succeeded && B.EndFunction()==asEByteCodeImageStatus::Succeeded;
  }
  TUniquePtr<asCByteCodeImage> Make(bool bRecovery=false)
  {
   asCByteCodeImageBuilder B; asUINT Slot=0;
   if (bRecovery) { if (!Constant(B,Recovery,97)) return nullptr; }
   else
   {
    if (B.AddTypeRequirement(*Type,Slot)!=asEByteCodeImageStatus::Succeeded ||
     B.AddFunctionRequirement(*Peer,Slot)!=asEByteCodeImageStatus::Succeeded ||
     B.AddFunctionRequirement(*Run,Slot)!=asEByteCodeImageStatus::Succeeded ||
     B.AddFunctionRequirement(*Probe,Slot)!=asEByteCodeImageStatus::Succeeded ||
     B.BeginFunction(Run->GetStableKey(),Run->GetStableKey(),4,0)!=asEByteCodeImageStatus::Succeeded) return nullptr;
    auto Push=ByteImm32(41),Call=ByteSlot(asEByteCodeOperandKind::FunctionSlot,Slot),Ret=ByteImm16(0);
    if (B.Emit(asBC_PshC4,MakeArrayView(&Push,1))!=asEByteCodeImageStatus::Succeeded ||
     B.Emit(asBC_CALLSYS,MakeArrayView(&Call,1))!=asEByteCodeImageStatus::Succeeded ||
     B.Emit(asBC_RET,MakeArrayView(&Ret,1))!=asEByteCodeImageStatus::Succeeded ||
     B.EndFunction()!=asEByteCodeImageStatus::Succeeded || !Constant(B,Peer,17)) return nullptr;
   }
   TUniquePtr<asCByteCodeImage> I;
   if (B.Finish(I)!=asEByteCodeImageStatus::Succeeded) return nullptr;
   if (!bRecovery && I->Functions[0].FunctionKey!=Run->GetStableKey()) Swap(I->Functions[0],I->Functions[1]);
   return I;
  }
 };
 void Reject(EMutation Mutation,asEByteCodeVerifyStatus Expected,asUINT Variant=0)
 {
  FManualCase T; ASSERT_THAT(IsTrue(T.Define())); auto I=T.Make();
  ASSERT_THAT(IsNotNull(I.Get())); ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded,asCByteCodeVerifier::Verify(*I)));
  ASSERT_THAT(IsTrue(T.Run->GetStableKey()==I->Functions[0].FunctionKey));
  auto& Peer=I->Functions[1];
  asEByteCodeImageStatus DecodeExpected=asEByteCodeImageStatus::InvalidOperand;
  switch(Mutation)
  {
  case EMutation::Kind: Peer.Instructions[0].Operands[0].Kind=asEByteCodeOperandKind::ImmediateS32; break;
  case EMutation::Immediate: Peer.Instructions[0].Operands[1].Immediate=int64(1)<<40; break;
  case EMutation::WrongRole:
   for (int32 N=0;N<I->Requirements.Num();++N) if (I->Requirements[N].Role==asEByteCodeRequirementRole::Type)
    { I->Functions[0].Instructions[1].Operands[0].Slot=N; break; }
   break;
  case EMutation::MissingSlot: I->Functions[0].Instructions[1].Operands[0].Slot=MAX_uint32; break;
  case EMutation::DeadOpcode:
   { asSByteCodeInstruction Tail; Tail.Opcode=asBC_STR; Peer.Instructions.Add(Tail); DecodeExpected=asEByteCodeImageStatus::UnknownOpcode; } break;
  case EMutation::DeadFrame:
   { auto Tail=Peer.Instructions[0]; Tail.Operands[0].Immediate=0; Peer.Instructions.Add(Tail); } break;
  case EMutation::DeadJump:
   { asSByteCodeInstruction Tail; Tail.Opcode=asBC_JMP; asSByteCodeOperand O; O.Kind=asEByteCodeOperandKind::Label; O.Immediate=999; O.Label=999; Tail.Operands.Add(O); Peer.Instructions.Add(Tail); } break;
  case EMutation::DuplicateBody:
   { auto Duplicate=I->Functions[0]; Duplicate.Instructions[0].Operands[0].Immediate=400;
    I->Functions.Add(Duplicate); if (Variant) Swap(I->Functions[0],I->Functions.Last()); } break;
  case EMutation::DuplicateRequirement:
  case EMutation::ConflictingRequirement:
   for (const auto& Req:I->Requirements) if (Req.Key==T.Probe->GetStableKey())
   { auto Duplicate=Req; if (Mutation==EMutation::ConflictingRequirement) { Duplicate.Parameters[0].StorageDwords=2; asEncodeByteCodeRequirementContract(Duplicate,Duplicate.ContractWitness); }
    I->Requirements.Add(Duplicate); break; }
   DecodeExpected=asEByteCodeImageStatus::Duplicate; break;
  case EMutation::HugeFrame: Peer.FrameSize=Variant; Peer.Frame.LocalDwords=Variant; break;
  case EMutation::ZeroBodyKey: Peer.FunctionKey={}; break;
  case EMutation::LateReturn: Peer.Instructions.Last().Operands[0].Immediate=1; break;
  }
  const auto Fault=asCByteCodeVerifier::VerifyWithFault(*I);
  ASSERT_THAT(AreEqual(Expected,Fault.Status));
  ASSERT_THAT(IsFalse(I->bValidated));
  TArray<uint8> Bytes; TUniquePtr<asCByteCodeImage> D;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,asCByteCodeImageCodec::Encode(*I,Bytes)));
  ASSERT_THAT(AreEqual(DecodeExpected,asCByteCodeImageCodec::Decode(Bytes,D)));
  ASSERT_THAT(IsNull(D.Get()));
  FNativeEngineOwner E;
  ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded,E.Ptr->RegisterExternalDefinitions(T.F.TakeSet())));
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,E.Ptr->BindNativeFunction(T.Probe->GetStableKey(),asFUNCTION(Observe),asCALL_GENERIC)));
  auto Recovery=T.Make(true); asCExecutableSnapshot* Kept=nullptr;
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,asLinkByteCodeImage(*E.Ptr, *Recovery,Kept)));
  auto* KeptExecutable=E.Ptr->FindPublishedExecutable(T.Recovery->GetStableKey()); Kept->Release();
  Cells[0]=91; Cells[1]=17; Cells[2]=92; Calls=0;
  // A caller-controlled validation bit cannot authorize publication.
  I->bValidated=true;
  asCExecutableSnapshot* Rejected=nullptr;
  auto Status=asLinkByteCodeImage(*E.Ptr, *I,Rejected); if (Rejected) Rejected->Release();
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Unverified,Status));
  ASSERT_THAT(IsNull(Rejected));
  ASSERT_THAT(IsFalse(I->bValidated));
  ASSERT_THAT(IsNull(E.Ptr->FindPublishedExecutable(T.Run->GetStableKey())));
  ASSERT_THAT(IsNull(E.Ptr->FindPublishedExecutable(T.Peer->GetStableKey())));
  ASSERT_THAT(IsTrue(KeptExecutable==E.Ptr->FindPublishedExecutable(T.Recovery->GetStableKey())));
  auto* C=E.Ptr->CreateContext();
  ASSERT_THAT(AreEqual(0,C->Prepare(T.Recovery)));
  ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,C->Execute()));
  ASSERT_THAT(AreEqual(97,static_cast<int32>(C->GetReturnDWord())));
  C->Release();
  ASSERT_THAT(AreEqual(0,Calls)); ASSERT_THAT(AreEqual(91,Cells[0])); ASSERT_THAT(AreEqual(17,Cells[1])); ASSERT_THAT(AreEqual(92,Cells[2]));
 }
public:
 TEST_METHOD(WrongOperandKindRejectsEveryAdmission) { Reject(EMutation::Kind,asEByteCodeVerifyStatus::InvalidOperand); }
 TEST_METHOD(ImmediateNarrowingRejectsEveryAdmission) { Reject(EMutation::Immediate,asEByteCodeVerifyStatus::InvalidOperand); }
 TEST_METHOD(WrongRoleFunctionSlotRejectsEveryAdmission) { Reject(EMutation::WrongRole,asEByteCodeVerifyStatus::InvalidOperand); }
 TEST_METHOD(MissingFunctionSlotRejectsEveryAdmission) { Reject(EMutation::MissingSlot,asEByteCodeVerifyStatus::MissingRequirement); }
 TEST_METHOD(UnreachableUnknownOpcodeStillRejects) { Reject(EMutation::DeadOpcode,asEByteCodeVerifyStatus::UnknownOpcode); }
 TEST_METHOD(UnreachableInvalidLocalStillRejects) { Reject(EMutation::DeadFrame,asEByteCodeVerifyStatus::InvalidFrame); }
 TEST_METHOD(UnreachableInvalidBranchStillRejects) { Reject(EMutation::DeadJump,asEByteCodeVerifyStatus::InvalidJump); }
 TEST_METHOD(DuplicateBodyCannotSelectAnOrderDependentWinner) { Reject(EMutation::DuplicateBody,asEByteCodeVerifyStatus::InvalidFrame,0); Reject(EMutation::DuplicateBody,asEByteCodeVerifyStatus::InvalidFrame,1); }
 TEST_METHOD(IdenticalDuplicateRequirementStillRejectsDirectAdmission) { Reject(EMutation::DuplicateRequirement,asEByteCodeVerifyStatus::CorruptClosure); }
 TEST_METHOD(ConflictingDuplicateRequirementRejectsEveryAdmission) { Reject(EMutation::ConflictingRequirement,asEByteCodeVerifyStatus::CorruptClosure); }
 TEST_METHOD(OversizedFrameCannotWrapRuntimeReservation) { for (asUINT Size:{0x7fffffffu,0x80000000u,0xffffffffu}) Reject(EMutation::HugeFrame,asEByteCodeVerifyStatus::InvalidFrame,Size); }
 TEST_METHOD(ZeroFunctionIdentityCannotEnterDirectAdmission) { Reject(EMutation::ZeroBodyKey,asEByteCodeVerifyStatus::InvalidOperand); }
 TEST_METHOD(LateInvalidBodyAndForgedValidationPublishNothing) { Reject(EMutation::LateReturn,asEByteCodeVerifyStatus::InvalidFrame); }
 TEST_METHOD(LegalCounterpartsExecuteThroughDirectAndDecodedAdmission)
 {
  for (bool bDecode:{false,true})
  {
   FManualCase T; ASSERT_THAT(IsTrue(T.Define())); auto I=T.Make(); ASSERT_THAT(IsNotNull(I.Get()));
   if (bDecode)
   {
    TArray<uint8> Bytes; TUniquePtr<asCByteCodeImage> D;
    ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,asCByteCodeImageCodec::Encode(*I,Bytes)));
    ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,asCByteCodeImageCodec::Decode(Bytes,D))); I=MoveTemp(D);
   }
   FNativeEngineOwner E;
   ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded,E.Ptr->RegisterExternalDefinitions(T.F.TakeSet())));
   ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,E.Ptr->BindNativeFunction(T.Probe->GetStableKey(),asFUNCTION(Observe),asCALL_GENERIC)));
   asCExecutableSnapshot* S=nullptr;
   ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,asLinkByteCodeImage(*E.Ptr, *I,S))); S->Release();
   auto* C=E.Ptr->CreateContext(); Calls=0; Cells[0]=91; Cells[1]=17; Cells[2]=92;
   ASSERT_THAT(AreEqual(0,C->Prepare(T.Run))); ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,C->Execute()));
   ASSERT_THAT(AreEqual(42,static_cast<int32>(C->GetReturnDWord())));
   ASSERT_THAT(AreEqual(1,Calls)); ASSERT_THAT(AreEqual(91,Cells[0])); ASSERT_THAT(AreEqual(41,Cells[1])); ASSERT_THAT(AreEqual(92,Cells[2]));
   ASSERT_THAT(AreEqual(0,C->Prepare(T.Peer))); ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,C->Execute()));
   ASSERT_THAT(AreEqual(17,static_cast<int32>(C->GetReturnDWord()))); C->Release();
  }
 }
 TEST_METHOD(TemporaryStackPeakIsReservedInAdditionToLocals)
 {
  FManualCase T; ASSERT_THAT(IsTrue(T.Define()));
  asCByteCodeImageBuilder B;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.BeginFunction(T.Run->GetStableKey(),T.Run->GetStableKey(),4,0)));
  auto Push=ByteImm64(21);
  for (int32 N=0;N<32;++N) ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.Emit(asBC_PshC8,MakeArrayView(&Push,1))));
  for (int32 N=0;N<32;++N) ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.Emit(asBC_PopPtr,{})));
  asSByteCodeOperand Set[2]={ByteLocal(1),ByteImm32(42)}; auto Ret=ByteImm16(0);
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.Emit(asBC_SetV4,MakeArrayView(Set,2))));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.Emit(asBC_CpyVtoR4,MakeArrayView(Set,1))));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.Emit(asBC_RET,MakeArrayView(&Ret,1))));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.EndFunction()));
  TUniquePtr<asCByteCodeImage> I;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.Finish(I)));
  FNativeEngineOwner E;
  ASSERT_THAT(AreEqual(0,E.Ptr->SetEngineProperty(asEP_INIT_STACK_SIZE,64)));
  ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded,E.Ptr->RegisterExternalDefinitions(T.F.TakeSet())));
  asCExecutableSnapshot* S=nullptr;
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,asLinkByteCodeImage(*E.Ptr, *I,S))); S->Release();
  auto* Executable=E.Ptr->FindPublishedExecutable(T.Run->GetStableKey());
  ASSERT_THAT(IsNotNull(Executable));
  // Four local DWORDs plus thirty-two literal QWORD pushes require at least 68 DWORDs.
  ASSERT_THAT(IsTrue(Executable->scriptData->stackNeeded>=68));
  auto* C=E.Ptr->CreateContext();
  for (int32 Repeat=0;Repeat<3;++Repeat)
  {
   ASSERT_THAT(AreEqual(0,C->Prepare(T.Run))); ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,C->Execute()));
   ASSERT_THAT(AreEqual(42,static_cast<int32>(C->GetReturnDWord())));
  }
  C->Release();
 }
};
#endif
