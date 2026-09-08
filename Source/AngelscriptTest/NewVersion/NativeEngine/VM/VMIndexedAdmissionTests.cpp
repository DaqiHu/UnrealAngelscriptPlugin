#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_bytecode_verifier.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(VMIndexedAdmission, "Angelscript.UnitTest.NativeEngine",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
 enum class ECase { Valid, Underflow, LifetimeBypass, JoinMismatch, WrongEntry, Truncated, Oversized, Negative };
 struct FTableCase
 {
  FDetachedDefinitionFixture F;
  asCScriptFunction* Run=nullptr;
  asCScriptFunction* Recovery=nullptr;
  bool Define()
  {
   asSMetadataParameter P; P.Type=F.Image->Primitive(ttInt);
   Run=F.DefineFunction(TEXT("Run"),TEXT(""),asFUNC_SCRIPT,F.Image->Primitive(ttInt),MakeArrayView(&P,1));
   Recovery=F.DefineFunction(TEXT("Recovery"),TEXT(""),asFUNC_SCRIPT,F.Image->Primitive(ttInt),{});
   return Run && Recovery && F.FinalizeAndFreeze();
  }
  TUniquePtr<asCByteCodeImage> Make(ECase Which)
  {
   asCByteCodeImageBuilder B; asUINT Slot=0;
   if (B.AddFunctionRequirement(*Run,Slot)!=asEByteCodeImageStatus::Succeeded ||
    B.BeginFunction(Recovery->GetStableKey(),Recovery->GetStableKey(),4,0)!=asEByteCodeImageStatus::Succeeded) return nullptr;
   auto Emit=[&](asEBCInstr Op,std::initializer_list<asSByteCodeOperand> Args)
   { return B.Emit(Op,MakeArrayView(Args.begin(),static_cast<int32>(Args.size())))==asEByteCodeImageStatus::Succeeded; };
   if (!Emit(asBC_SetV4,{ByteLocal(1),ByteImm32(97)}) || !Emit(asBC_CpyVtoR4,{ByteLocal(1)}) ||
    !Emit(asBC_RET,{ByteImm16(0)}) || B.EndFunction()!=asEByteCodeImageStatus::Succeeded ||
    B.BeginFunction(Run->GetStableKey(),Run->GetStableKey(),8,1)!=asEByteCodeImageStatus::Succeeded) return nullptr;
   const asUINT A=B.NewLabel(), C=B.NewLabel(), Join=B.NewLabel();
   auto Label=[](asUINT Id) { asSByteCodeOperand O; O.Kind=asEByteCodeOperandKind::Label; O.Label=Id; O.Immediate=Id; return O; };
   const int32 Max=Which==ECase::Negative ? -1 : Which==ECase::Truncated ? 20 : Which==ECase::Oversized ? MAX_int32 : 1;
   if (!Emit(asBC_JMPP,{ByteLocal(0),ByteImm32(Max)}) || !Emit(asBC_JMP,{Label(A)})) return nullptr;
   if (Which==ECase::WrongEntry) { if (!Emit(asBC_SUSPEND,{})) return nullptr; }
   else if (!Emit(asBC_JMP,{Label(C)})) return nullptr;
   if (B.BindLabel(A)!=asEByteCodeImageStatus::Succeeded || !Emit(asBC_SetV4,{ByteLocal(1),ByteImm32(11)})) return nullptr;
   if (Which==ECase::JoinMismatch && !Emit(asBC_PshC4,{ByteImm32(1)})) return nullptr;
   if (!Emit(asBC_JMP,{Label(Join)}) || B.BindLabel(C)!=asEByteCodeImageStatus::Succeeded) return nullptr;
   if (Which==ECase::Underflow && !Emit(asBC_PopPtr,{})) return nullptr;
   if (!Emit(asBC_SetV4,{ByteLocal(1),ByteImm32(22)}) || !Emit(asBC_JMP,{Label(Join)}) ||
    B.BindLabel(Join)!=asEByteCodeImageStatus::Succeeded || !Emit(asBC_CpyVtoR4,{ByteLocal(1)}) ||
    !Emit(asBC_RET,{ByteImm16(1)}) || B.EndFunction()!=asEByteCodeImageStatus::Succeeded) return nullptr;
   TUniquePtr<asCByteCodeImage> Image;
   if (B.Finish(Image)!=asEByteCodeImageStatus::Succeeded) return nullptr;
   return Image;
  }
 };
 void Check(ECase Which, asEByteCodeVerifyStatus Expected)
 {
  FTableCase T; ASSERT_THAT(IsTrue(T.Define())); auto I=T.Make(Which);
  ASSERT_THAT(IsNotNull(I.Get()));
  ASSERT_THAT(AreEqual(Expected,asCByteCodeVerifier::Verify(*I)));
  TArray<uint8> Bytes; TUniquePtr<asCByteCodeImage> D;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,asCByteCodeImageCodec::Encode(*I,Bytes)));
  ASSERT_THAT(AreEqual(Expected==asEByteCodeVerifyStatus::Succeeded?asEByteCodeImageStatus::Succeeded:asEByteCodeImageStatus::InvalidOperand,
   asCByteCodeImageCodec::Decode(Bytes,D)));
 }
 void ExecuteIndex(int32 Index, bool bValid)
 {
  FTableCase T; ASSERT_THAT(IsTrue(T.Define()));
  if (!bValid)
  {
   // Old admission must fail this assertion before any invalid index executes.
   auto Malformed=T.Make(ECase::WrongEntry);
   ASSERT_THAT(IsNotNull(Malformed.Get()));
   ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidJump,asCByteCodeVerifier::Verify(*Malformed)));
  }
  auto I=T.Make(ECase::Valid); ASSERT_THAT(IsNotNull(I.Get()));
  TArray<uint8> Bytes; TUniquePtr<asCByteCodeImage> D;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,asCByteCodeImageCodec::Encode(*I,Bytes)));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,asCByteCodeImageCodec::Decode(Bytes,D)));
  FNativeEngineOwner E;
  ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,E.Ptr->RegisterMetadataImage(T.F.Image)));
  asCExecutableSnapshot* S=nullptr;
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,E.Ptr->LinkByteCodeImage(*D,S)));
  auto* C=E.Ptr->CreateContext();
  for (int Repeat=0;Repeat<3;++Repeat)
  {
   ASSERT_THAT(AreEqual(0,C->Prepare(T.Run)));
   ASSERT_THAT(AreEqual(0,C->SetArgDWord(0,static_cast<asDWORD>(Index))));
   ASSERT_THAT(AreEqual(bValid?asEXECUTION_FINISHED:asEXECUTION_EXCEPTION,C->Execute()));
   if (bValid) { ASSERT_THAT(AreEqual(Index==0?11:22,static_cast<int32>(C->GetReturnDWord()))); }
   else { ASSERT_THAT(AreEqual(FString(TEXT("Indexed jump index out of range")),FString(UTF8_TO_TCHAR(C->GetExceptionString())))); }
   ASSERT_THAT(AreEqual(0,C->Prepare(T.Recovery)));
   ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,C->Execute()));
   ASSERT_THAT(AreEqual(97,static_cast<int32>(C->GetReturnDWord())));
  }
  C->Release(); S->Release();
 }
public:
 TEST_METHOD(NonFirstEntryUnderflowRejectsDirectAndDecoded) { Check(ECase::Underflow,asEByteCodeVerifyStatus::StackUnderflow); }
 TEST_METHOD(NonFirstEntryStackJoinRejectsDirectAndDecoded) { Check(ECase::JoinMismatch,asEByteCodeVerifyStatus::InvalidJump); }
 TEST_METHOD(EveryEntryMustBeFixedWidthJump) { Check(ECase::WrongEntry,asEByteCodeVerifyStatus::InvalidJump); }
 TEST_METHOD(TruncatedTableRejectsDirectAndDecoded) { Check(ECase::Truncated,asEByteCodeVerifyStatus::InvalidJump); }
 TEST_METHOD(OversizedTableRejectsWithoutArithmeticOverflow) { Check(ECase::Oversized,asEByteCodeVerifyStatus::InvalidJump); }
 TEST_METHOD(NegativeMaximumRejectsDirectAndDecoded) { Check(ECase::Negative,asEByteCodeVerifyStatus::InvalidJump); }
 TEST_METHOD(CompleteTablePassesDirectAndDecoded) { Check(ECase::Valid,asEByteCodeVerifyStatus::Succeeded); }
 TEST_METHOD(EveryEntryExecutesItsDistinctResult) { ExecuteIndex(0,true); ExecuteIndex(1,true); }
 TEST_METHOD(NegativeRuntimeIndexThrowsAndRecovers) { ExecuteIndex(-1,false); }
 TEST_METHOD(MaximumPlusOneThrowsAndRecovers) { ExecuteIndex(2,false); }
 TEST_METHOD(LargeRuntimeIndexThrowsAndRecovers) { ExecuteIndex(MAX_int32,false); }
 TEST_METHOD(NonFirstEntryCannotBypassObjectConstruction)
 {
  FDetachedDefinitionFixture F;
  auto* Type=F.DefineType(TEXT("Item"),TEXT(""),asOBJ_REF | asOBJ_SCRIPT_OBJECT);
  auto* Run=F.DefineFunction(TEXT("Run"),TEXT(""),asFUNC_SCRIPT,F.Image->Primitive(ttVoid),{});
  ASSERT_THAT(IsTrue(F.FinalizeAndFreeze()));
  asCByteCodeImageBuilder B; asUINT TypeSlot=0,RunSlot=0;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.AddTypeRequirement(*Type,TypeSlot)));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.AddFunctionRequirement(*Run,RunSlot)));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.BeginFunction(Run->GetStableKey(),Run->GetStableKey(),8,0)));
  asSByteCodeFrameDescriptor Frame; Frame.LocalDwords=8; Frame.InitializedObjectSlots.Add(2);
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.SetFrameDescriptor(Frame)));
  auto Emit=[&](asEBCInstr Op,std::initializer_list<asSByteCodeOperand> Args)
  { return B.Emit(Op,MakeArrayView(Args.begin(),static_cast<int32>(Args.size()))); };
  auto Label=[](asUINT Id) { asSByteCodeOperand O; O.Kind=asEByteCodeOperandKind::Label; O.Label=Id; O.Immediate=Id; return O; };
  auto Action=[&](bool bInit)
  {
   asSByteCodeCleanupAction A; A.Kind=asEByteCodeCleanupKind::DestroyObject;
   A.Path=bInit?asEByteCodeCleanupPath::Exception:asEByteCodeCleanupPath::Normal;
   A.FrameOffset=2; A.TypeSlot=TypeSlot; A.InstructionIndex=B.GetInstructionCount()-1;
   return B.AddCleanupAction(A);
  };
  const auto Construct=B.NewLabel(), Free=B.NewLabel();
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,Emit(asBC_JMPP,{ByteLocal(4),ByteImm32(1)})));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,Emit(asBC_JMP,{Label(Construct)})));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,Emit(asBC_JMP,{Label(Free)})));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.BindLabel(Construct)));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,Emit(asBC_PSF,{ByteLocal(2)})));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,Emit(asBC_ALLOC,{ByteSlot(asEByteCodeOperandKind::TypeSlot,TypeSlot),ByteImm32(0)})));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,Action(true)));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.BindLabel(Free)));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,Emit(asBC_FREE,{ByteLocal(2),ByteSlot(asEByteCodeOperandKind::TypeSlot,TypeSlot)})));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,Action(false)));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,Emit(asBC_RET,{ByteImm16(0)})));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.EndFunction()));
  TUniquePtr<asCByteCodeImage> I;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,B.Finish(I)));
  ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame,asCByteCodeVerifier::Verify(*I)));
  TArray<uint8> Bytes; TUniquePtr<asCByteCodeImage> D;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,asCByteCodeImageCodec::Encode(*I,Bytes)));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::InvalidOperand,asCByteCodeImageCodec::Decode(Bytes,D)));
  // The same declared storage and cleanup is valid when both entries construct first.
  I->Functions[0].Instructions[2].Operands[0].Immediate=3;
  ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded,asCByteCodeVerifier::Verify(*I)));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,asCByteCodeImageCodec::Encode(*I,Bytes)));
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,asCByteCodeImageCodec::Decode(Bytes,D)));
 }
};
#endif
