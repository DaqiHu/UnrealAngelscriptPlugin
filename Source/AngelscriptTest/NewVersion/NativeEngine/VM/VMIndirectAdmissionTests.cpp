#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_bytecode_verifier.h"
#include "as_generic.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
TEST_CLASS_WITH_FLAGS(VMIndirectAdmission, "Angelscript.UnitTest.NativeEngine",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
 static asCScriptFunction* Define(FDetachedDefinitionFixture& F, const TCHAR* Name, eTokenType Token,
  asDWORD Mode = asTM_NONE, eTokenType Return = ttInt, bool bNoParameter = false, bool bScript = false, asCObjectType* Owner = nullptr)
 {
  asSMetadataParameter P; P.Type = F.Image->Primitive(Token); P.Flags = Mode;
  if (Mode != asTM_NONE) P.Type.MakeReference(true);
  return F.DefineFunction(Name, TEXT(""), bScript ? asFUNC_SCRIPT : asFUNC_SYSTEM, F.Image->Primitive(Return),
   bNoParameter ? TConstArrayView<asSMetadataParameter>() : MakeArrayView(&P,1), Owner);
 }
 // Build a valid direct-call instruction first, then express its expected contract as CallPtr.
 // This lets RED exercise admission without executing an unguarded malformed call.
 static TUniquePtr<asCByteCodeImage> Make(asCScriptFunction& Shape, asCScriptFunction& Run,
  asEBCInstr Push, asSByteCodeOperand Value, bool bLoop = false, asCScriptFunction* Target = nullptr,
  asCScriptFunction* Recovery = nullptr)
 {
  asCByteCodeImageBuilder B; asUINT ShapeSlot=0, RunSlot=0, TargetSlot=0;
  if (B.AddFunctionRequirement(Shape,ShapeSlot)!=asEByteCodeImageStatus::Succeeded ||
   B.AddFunctionRequirement(Run,RunSlot)!=asEByteCodeImageStatus::Succeeded) return nullptr;
  if (Target && B.AddFunctionRequirement(*Target,TargetSlot)!=asEByteCodeImageStatus::Succeeded) return nullptr;
  if (Target && Target->funcType == asFUNC_SCRIPT)
  {
   if (B.BeginFunction(Target->GetStableKey(),Target->GetStableKey(),4,1)!=asEByteCodeImageStatus::Succeeded) return nullptr;
   asSByteCodeOperand Set[2]={ByteLocal(1),ByteImm32(42)}, Ret=ByteImm16(1);
   if (B.Emit(asBC_SetV4,MakeArrayView(Set,2))!=asEByteCodeImageStatus::Succeeded ||
    B.Emit(asBC_CpyVtoR4,MakeArrayView(Set,1))!=asEByteCodeImageStatus::Succeeded ||
    B.Emit(asBC_RET,MakeArrayView(&Ret,1))!=asEByteCodeImageStatus::Succeeded || B.EndFunction()!=asEByteCodeImageStatus::Succeeded) return nullptr;
  }
  if (Recovery)
  {
   if (B.BeginFunction(Recovery->GetStableKey(),Recovery->GetStableKey(),4,0)!=asEByteCodeImageStatus::Succeeded) return nullptr;
   asSByteCodeOperand Set[2]={ByteLocal(1),ByteImm32(97)}, R=ByteImm16(0);
   if (B.Emit(asBC_SetV4,MakeArrayView(Set,2))!=asEByteCodeImageStatus::Succeeded ||
    B.Emit(asBC_CpyVtoR4,MakeArrayView(Set,1))!=asEByteCodeImageStatus::Succeeded ||
    B.Emit(asBC_RET,MakeArrayView(&R,1))!=asEByteCodeImageStatus::Succeeded || B.EndFunction()!=asEByteCodeImageStatus::Succeeded) return nullptr;
  }
  if (B.BeginFunction(Run.GetStableKey(),Run.GetStableKey(),8,0)!=asEByteCodeImageStatus::Succeeded) return nullptr;
  auto Local=ByteLocal(2);
  if (Target)
  {
   auto T=ByteSlot(asEByteCodeOperandKind::FunctionSlot,TargetSlot);
   if (B.Emit(asBC_FuncPtr,MakeArrayView(&T,1))!=asEByteCodeImageStatus::Succeeded ||
    B.Emit(asBC_PopRPtr,{})!=asEByteCodeImageStatus::Succeeded ||
    B.Emit(asBC_CpyRtoV8,MakeArrayView(&Local,1))!=asEByteCodeImageStatus::Succeeded) return nullptr;
  }
  else if (B.Emit(asBC_ClrVPtr,MakeArrayView(&Local,1))!=asEByteCodeImageStatus::Succeeded) return nullptr;
  const asUINT Start=B.NewLabel();
  if (B.BindLabel(Start)!=asEByteCodeImageStatus::Succeeded || B.Emit(Push,MakeArrayView(&Value,1))!=asEByteCodeImageStatus::Succeeded) return nullptr;
  auto Call=ByteSlot(asEByteCodeOperandKind::FunctionSlot,ShapeSlot), End=ByteImm16(0);
  if (B.Emit(asBC_CALLSYS,MakeArrayView(&Call,1))!=asEByteCodeImageStatus::Succeeded) return nullptr;
  if (bLoop) { End.Kind=asEByteCodeOperandKind::Label; End.Label=Start; End.Immediate=Start; }
  if (B.Emit(bLoop?asBC_JMP:asBC_RET,MakeArrayView(&End,1))!=asEByteCodeImageStatus::Succeeded || B.EndFunction()!=asEByteCodeImageStatus::Succeeded) return nullptr;
  TUniquePtr<asCByteCodeImage> Image;
  if (B.Finish(Image)!=asEByteCodeImageStatus::Succeeded) return nullptr;
  for (auto& Fn:Image->Functions) for (auto& I:Fn.Instructions) if (I.Opcode==asBC_CALLSYS)
  { I.Opcode=asBC_CallPtr; I.Operands.Insert(ByteLocal(2),0); }
  return Image;
 }
 void CheckBoth(asCByteCodeImage& Image, asEByteCodeVerifyStatus Status)
 {
  ASSERT_THAT(AreEqual(Status,asCByteCodeVerifier::Verify(Image)));
  TArray<uint8> Bytes;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,asCByteCodeImageCodec::Encode(Image,Bytes)));
  TUniquePtr<asCByteCodeImage> Decoded;
  ASSERT_THAT(AreEqual(Status==asEByteCodeVerifyStatus::Succeeded ? asEByteCodeImageStatus::Succeeded : asEByteCodeImageStatus::InvalidOperand,
   asCByteCodeImageCodec::Decode(Bytes,Decoded)));
 }
 inline static int32 Calls=0;
 static void Observe(asIScriptGeneric* G) { ++Calls; G->SetReturnDWord(42); }
 void ExecuteCase(eTokenType ActualToken, asDWORD ExpectedMode=asTM_NONE, asDWORD ActualMode=asTM_NONE,
  eTokenType ActualReturn=ttInt, bool bNoParameter=false, bool bNull=false, bool bScript=false, bool bMethod=false)
 {
  FDetachedDefinitionFixture F;
  auto* Shape=Define(F,TEXT("Expected"),ttInt,ExpectedMode);
  auto* Owner=bMethod ? F.DefineType(TEXT("Receiver"),TEXT(""),asOBJ_REF | asOBJ_SCRIPT_OBJECT) : nullptr;
  auto* Target=Define(F,TEXT("DistinctActual"),ActualToken,ActualMode,ActualReturn,bNoParameter,bScript,Owner);
  auto* Run=F.DefineFunction(TEXT("Run"),TEXT(""),asFUNC_SCRIPT,F.Image->Primitive(ttInt),{});
  auto* Recovery=F.DefineFunction(TEXT("Recovery"),TEXT(""),asFUNC_SCRIPT,F.Image->Primitive(ttInt),{});
  ASSERT_THAT(IsTrue(F.FinalizeAndFreeze()));
  auto Image=Make(*Shape,*Run,ExpectedMode==asTM_NONE?asBC_PshC4:asBC_PSF,
   ExpectedMode==asTM_NONE?ByteImm32(21):ByteLocal(4),false,bNull?nullptr:Target,Recovery);
  ASSERT_THAT(IsNotNull(Image.Get()));
  FNativeEngineOwner Engine;
  ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,Engine.Ptr->RegisterMetadataImage(F.Image)));
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,Engine.Ptr->BindNativeFunction(Shape->GetStableKey(),asFUNCTION(Observe),asCALL_GENERIC)));
  if (!bScript) { ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,Engine.Ptr->BindNativeFunction(Target->GetStableKey(),asFUNCTION(Observe),asCALL_GENERIC))); }
  TArray<uint8> Bytes;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,asCByteCodeImageCodec::Encode(*Image,Bytes)));
  TUniquePtr<asCByteCodeImage> Decoded;
  ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,asCByteCodeImageCodec::Decode(Bytes,Decoded)));
  asCExecutableSnapshot* Snapshot=nullptr;
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,Engine.Ptr->LinkByteCodeImage(*Decoded,Snapshot)));
  auto* C=Engine.Ptr->CreateContext(); Calls=0;
  const bool bMatch=ActualToken==ttInt && ActualMode==ExpectedMode && ActualReturn==ttInt && !bNoParameter && !bNull && !bMethod;
  for (int Repeat=0; Repeat<3; ++Repeat)
  {
   ASSERT_THAT(AreEqual(0,C->Prepare(Run)));
   ASSERT_THAT(AreEqual(bMatch?asEXECUTION_FINISHED:asEXECUTION_EXCEPTION,C->Execute()));
   if (bMatch) { ASSERT_THAT(AreEqual(42,static_cast<int32>(C->GetReturnDWord()))); }
   else if (!bNull) { ASSERT_THAT(AreEqual(FString(TEXT("Indirect call signature mismatch")),FString(UTF8_TO_TCHAR(C->GetExceptionString())))); }
   else { ASSERT_THAT(IsTrue(FString(UTF8_TO_TCHAR(C->GetExceptionString())).Contains(TEXT("Unbound")))); }
   ASSERT_THAT(AreEqual(bMatch && !bScript ? Repeat+1 : 0,Calls));
   ASSERT_THAT(AreEqual(0,C->Prepare(Recovery)));
   ASSERT_THAT(AreEqual(asEXECUTION_FINISHED,C->Execute()));
   ASSERT_THAT(AreEqual(97,static_cast<int32>(C->GetReturnDWord())));
  }
  C->Release(); Snapshot->Release();
 }
public:
 TEST_METHOD(WideArgumentsRejectOneDwordDirectAndDecoded)
 {
  for (eTokenType Token:{ttDouble,ttInt64})
  {
   FDetachedDefinitionFixture F; auto* S=Define(F,TEXT("Shape"),Token);
   auto* R=F.DefineFunction(TEXT("Run"),TEXT(""),asFUNC_SCRIPT,F.Image->Primitive(ttInt),{});
   ASSERT_THAT(IsTrue(F.FinalizeAndFreeze())); auto I=Make(*S,*R,asBC_PshC4,ByteImm32(1));
   ASSERT_THAT(IsNotNull(I.Get())); CheckBoth(*I,asEByteCodeVerifyStatus::StackUnderflow);
  }
 }
 TEST_METHOD(WideCallLoopsBalanceDirectAndDecoded)
 {
  for (eTokenType Token:{ttDouble,ttInt64})
  {
   FDetachedDefinitionFixture F; auto* S=Define(F,TEXT("Shape"),Token);
   auto* R=F.DefineFunction(TEXT("Run"),TEXT(""),asFUNC_SCRIPT,F.Image->Primitive(ttInt),{});
   ASSERT_THAT(IsTrue(F.FinalizeAndFreeze())); auto I=Make(*S,*R,asBC_PshC8,ByteImm64(0),true);
   ASSERT_THAT(IsNotNull(I.Get())); CheckBoth(*I,asEByteCodeVerifyStatus::Succeeded);
  }
 }
 TEST_METHOD(ReferenceRejectsValueBitsAndAcceptsAddressLoops)
 {
  for (asDWORD Mode:{asDWORD(asTM_INREF),asDWORD(asTM_OUTREF),asDWORD(asTM_INOUTREF)})
  {
   FDetachedDefinitionFixture F; auto* S=Define(F,TEXT("Shape"),ttInt,Mode);
   auto* R=F.DefineFunction(TEXT("Run"),TEXT(""),asFUNC_SCRIPT,F.Image->Primitive(ttInt),{});
   ASSERT_THAT(IsTrue(F.FinalizeAndFreeze())); auto Bad=Make(*S,*R,asBC_PshC8,ByteImm64(0));
   ASSERT_THAT(IsNotNull(Bad.Get())); CheckBoth(*Bad,asEByteCodeVerifyStatus::InvalidOperand);
   auto Good=Make(*S,*R,asBC_PSF,ByteLocal(4),true); CheckBoth(*Good,asEByteCodeVerifyStatus::Succeeded);
  }
 }
 TEST_METHOD(MissingCallSiteSignatureRejectsLegacyEncoding)
 {
  FDetachedDefinitionFixture F; auto* S=Define(F,TEXT("Shape"),ttInt);
  auto* R=F.DefineFunction(TEXT("Run"),TEXT(""),asFUNC_SCRIPT,F.Image->Primitive(ttInt),{});
  ASSERT_THAT(IsTrue(F.FinalizeAndFreeze())); auto I=Make(*S,*R,asBC_PshC4,ByteImm32(1));
  ASSERT_THAT(IsNotNull(I.Get()));
  for (auto& Fn:I->Functions) for (auto& Ins:Fn.Instructions) if (Ins.Opcode==asBC_CallPtr) Ins.Operands.SetNum(1);
  CheckBoth(*I,asEByteCodeVerifyStatus::InvalidOperand);
 }
 TEST_METHOD(ForgedExpectedWidthRejectsAtTarget)
 {
  FDetachedDefinitionFixture F; auto* S=Define(F,TEXT("Shape"),ttDouble);
  auto* R=F.DefineFunction(TEXT("Run"),TEXT(""),asFUNC_SCRIPT,F.Image->Primitive(ttInt),{});
  ASSERT_THAT(IsTrue(F.FinalizeAndFreeze())); auto I=Make(*S,*R,asBC_PshC4,ByteImm32(1));
  ASSERT_THAT(IsNotNull(I.Get()));
  for (auto& Req:I->Requirements) if (Req.Key==S->GetStableKey())
  { Req.Parameters[0].StorageDwords=1; asEncodeByteCodeRequirementContract(Req,Req.ContractWitness); }
  ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded,asCByteCodeVerifier::Verify(*I)));
  FNativeEngineOwner E;
  ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded,E.Ptr->RegisterMetadataImage(F.Image)));
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,E.Ptr->BindNativeFunction(S->GetStableKey(),asFUNCTION(Observe),asCALL_GENERIC)));
  asCExecutableSnapshot* Snapshot=nullptr;
  auto Status=E.Ptr->LinkByteCodeImage(*I,Snapshot); if (Snapshot) Snapshot->Release();
  ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch,Status));
  ASSERT_THAT(IsNull(E.Ptr->FindPublishedExecutable(R->GetStableKey())));
 }
 TEST_METHOD(DistinctNativeNameWithSameShapeReturnsFortyTwo) { ExecuteCase(ttInt); }
 TEST_METHOD(DistinctNativeReferenceShapeReturnsFortyTwo) { ExecuteCase(ttInt,asTM_INOUTREF,asTM_INOUTREF); }
 TEST_METHOD(DistinctScriptNameWithSameShapeReturnsFortyTwo) { ExecuteCase(ttInt,asTM_NONE,asTM_NONE,ttInt,false,false,true); }
 TEST_METHOD(ActualSameWidthDifferentTypeRejectsBeforeCallbackAndRecovers) { ExecuteCase(ttFloat); }
 TEST_METHOD(UnboundReceiverCannotUseReceiverFreeShape) { ExecuteCase(ttInt,asTM_NONE,asTM_NONE,ttInt,false,false,false,true); }
 TEST_METHOD(ActualWideParameterRejectsBeforeCallbackAndRecovers) { ExecuteCase(ttInt64); }
 TEST_METHOD(ActualParameterModeRejectsBeforeCallbackAndRecovers) { ExecuteCase(ttInt,asTM_INREF,asTM_OUTREF); }
 TEST_METHOD(ActualReturnTypeRejectsBeforeCallbackAndRecovers) { ExecuteCase(ttInt,asTM_NONE,asTM_NONE,ttInt64); }
 TEST_METHOD(ActualParameterCountRejectsBeforeCallbackAndRecovers) { ExecuteCase(ttInt,asTM_NONE,asTM_NONE,ttInt,true); }
 TEST_METHOD(NullTargetPreservesExceptionAndRecovers) { ExecuteCase(ttInt,asTM_NONE,asTM_NONE,ttInt,false,true); }
};
#endif
