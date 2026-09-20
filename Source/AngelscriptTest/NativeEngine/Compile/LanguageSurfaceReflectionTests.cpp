#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"
#include "frontend/Compile/as_descriptor_consumer.h"
#include "Core/AngelscriptEngine.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace ReflectionSurfaceTests
{
 using FSession = AngelscriptNativeEngineTest::FFrontendSessionRun;
 struct FProjection
 {
  FSession Frontend;
  FAngelscriptPreprocessResult Result;
  bool Resolved = false, Projected = false;
  explicit FProjection(const FString& Source, TMap<FString,bool> Flags = {})
   : Frontend({{TEXT("Reflection.as"),Source}}, MoveTemp(Flags))
  {
   Resolved = Frontend.ResolveDeclarations();
   Projected = FAngelscriptDescriptorConsumer().Project(*Frontend.Session, Frontend.SourceManager, Result);
  }
 };
 bool RejectsMarker(const FString& Source, const FString& Marker)
 {
  FProjection Run(Source + TEXT(" int Good(){return 7;}"));
  if (Run.Resolved || Run.Projected || Run.Frontend.Session->IsPublishable() || !Run.Result.GetModules().IsEmpty()
   || Run.Frontend.Session->Lookup(TEXT("Good")).Num() != 1) return false;
  asCCollectingDiagnosticConsumer Capture;
  Run.Frontend.Diagnostics.EmitTo(Capture);
  for (const auto& D : Capture.GetRecords())
  {
   if (D.DiagnosticID == 3002 && D.Severity == asEDiagnosticSeverity::Error && D.Arguments.Num() == 2
    && D.Arguments[0].TextValue == TEXT("blueprint-accessor-metadata-removed") && D.Arguments[1].TextValue == Marker)
   {
    TConstArrayView<uint8> Bytes;
    if (!Run.Frontend.SourceManager.TryGetBytes(D.PrimaryRange, Bytes)) return false;
    FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
    return FString(Text.Length(),Text.Get()) == Marker && int32(D.PrimaryRange.Begin.ByteOffset) == Source.Find(Marker,ESearchCase::CaseSensitive);
   }
  }
  return false;
 }
 template<class T> constexpr bool HasAccessorFields = requires(T& V) { V.BlueprintGetter; } || requires(T& V) { V.BlueprintSetter; }
  || requires(T& V) { V.GetterFunctionName; } || requires(T& V) { V.SetterFunctionName; }
  || requires(T& V) { V.bBlueprintGetter; } || requires(T& V) { V.bBlueprintSetter; };
 template<class T> constexpr bool HasAccessorConsumer = requires(T& V) { &T::VerifyBlueprintGetFunc; } || requires(T& V) { &T::VerifyBlueprintSetFunc; };
 static_assert(!HasAccessorFields<FAngelscriptPropertyDesc> && !HasAccessorFields<FAngelscriptFunctionDesc>);
 static_assert(!HasAccessorConsumer<FAngelscriptEngine>);
}
TEST_CLASS_WITH_FLAGS(Reflection, "Angelscript.UnitTest.NativeEngine.Compile", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
 TEST_METHOD(PropertyAccessorRequestsRejectAtTheirMarker)
 {
  for (const FString Marker : {TEXT("BlueprintGetter"),TEXT("BlueprintSetter"),TEXT("blueprintgetter"),TEXT("BLUEPRINTSETTER")})
   ASSERT_THAT(IsTrue(ReflectionSurfaceTests::RejectsMarker(FString::Printf(TEXT("UCLASS() class Item { UPROPERTY(%s=Read) int Value; UFUNCTION() int Read(){return Value;} }"), *Marker), Marker), Marker));
 }
 TEST_METHOD(FunctionAccessorRequestsRejectAtTheirMarker)
 {
  for (const FString Marker : {TEXT("BlueprintGetter"),TEXT("BlueprintSetter")})
   ASSERT_THAT(IsTrue(ReflectionSurfaceTests::RejectsMarker(FString::Printf(TEXT("UCLASS() class Item { UFUNCTION(%s) int Read(){return 1;} }"), *Marker), Marker), Marker));
 }
 TEST_METHOD(NestedMetadataCannotHideAccessorRequests)
 {
  for (const FString Marker : {TEXT("BlueprintGetter"),TEXT("BlueprintSetter")})
  {
   ASSERT_THAT(IsTrue(ReflectionSurfaceTests::RejectsMarker(FString::Printf(TEXT("UCLASS() class Item { UPROPERTY(Meta=(%s=Read)) int Value; }"),*Marker), Marker), Marker));
   ASSERT_THAT(IsTrue(ReflectionSurfaceTests::RejectsMarker(FString::Printf(TEXT("UCLASS() class Item { UFUNCTION(Meta=(Extra=(%s=Read))) int Read(){return 1;} }"),*Marker), Marker), Marker));
  }
 }
 TEST_METHOD(EnumUMetaAccessorRequestsAreRejected)
 {
  for (const FString Marker : {TEXT("BlueprintGetter"),TEXT("BlueprintSetter")})
   ASSERT_THAT(IsTrue(ReflectionSurfaceTests::RejectsMarker(FString::Printf(TEXT("UENUM() enum Mode { Idle UMETA(Meta=(%s=Read)) };"),*Marker), Marker), Marker));
 }
 TEST_METHOD(InactiveAccessorMetadataDoesNotAffectDescriptors)
 {
  ReflectionSurfaceTests::FProjection Run(TEXT("#if ACCESSORS\nUCLASS() class Bad { UPROPERTY(BlueprintGetter=Read) int Value; };\n#endif\nUCLASS() class Good { UPROPERTY(BlueprintReadWrite) int Value; };"), {{TEXT("ACCESSORS"),false}});
  ASSERT_THAT(IsTrue(Run.Resolved && Run.Projected));
  ASSERT_THAT(AreEqual(1, Run.Result.GetModules().Num()));
  ASSERT_THAT(AreEqual(1, Run.Result.GetModules()[0]->Classes.Num()));
  ASSERT_THAT(AreEqual(FString(TEXT("Good")),Run.Result.GetModules()[0]->Classes[0]->ClassName));
  asCCollectingDiagnosticConsumer Capture; Run.Frontend.Diagnostics.EmitTo(Capture);
  ASSERT_THAT(AreEqual(0,Capture.GetRecords().Num()));
 }
 TEST_METHOD(OrdinaryFieldAndExplicitMethodsKeepStableDescriptors)
 {
  const FString Source = TEXT("UCLASS() class Item { UPROPERTY(BlueprintReadWrite) int Value; UFUNCTION() int Read(){return Value;} UFUNCTION() void Write(int NewValue){Value=NewValue;} int get_Extra(){return 7;} }");
  ReflectionSurfaceTests::FProjection First(Source), Second(Source);
  ASSERT_THAT(IsTrue(First.Resolved && First.Projected && Second.Resolved && Second.Projected));
  const auto& A = First.Result.GetModules()[0]->Classes[0].Get();
  const auto& B = Second.Result.GetModules()[0]->Classes[0].Get();
  ASSERT_THAT(AreEqual(1,A.Properties.Num())); ASSERT_THAT(AreEqual(2,A.Methods.Num()));
  ASSERT_THAT(AreEqual(FString(TEXT("Value")),A.Properties[0]->PropertyName));
  ASSERT_THAT(IsTrue(A.Properties[0]->Meta.Contains(TEXT("BlueprintReadWrite"))));
  ASSERT_THAT(IsFalse(A.Properties[0]->Semantic.StableDeclarationKey.IsZero()));
  ASSERT_THAT(IsTrue(A.Properties[0]->Semantic.StableDeclarationKey == B.Properties[0]->Semantic.StableDeclarationKey));
  ASSERT_THAT(IsTrue(A.Properties[0]->Semantic.StableTypeUseKey == B.Properties[0]->Semantic.StableTypeUseKey));
  for (int32 I=0; I<2; ++I)
  {
   ASSERT_THAT(IsTrue(A.Methods[I]->Semantic.StableDeclarationKey == B.Methods[I]->Semantic.StableDeclarationKey));
   ASSERT_THAT(IsFalse(A.Methods[I]->Semantic.StableDeclarationKey.IsZero()));
   ASSERT_THAT(IsNull(A.Methods[I]->ScriptFunction));
  }
  ASSERT_THAT(IsTrue(First.Frontend.Session->AnalyzeBodies(1)));
  ASSERT_THAT(IsTrue(First.Frontend.Session->VerifyAST()));
 }
 TEST_METHOD(UnrelatedMetadataAndAccessorWordsAsValuesRemainOrdinary)
 {
  ReflectionSurfaceTests::FProjection Run(TEXT("UCLASS() class Item { UPROPERTY(FutureFlag, Meta=(ToolTip=\"BlueprintGetter\",Custom=\"BlueprintSetter\")) int Value; UFUNCTION(Meta=(DisplayName=\"BlueprintGetter\")) int Read(){return Value;} }"));
  ASSERT_THAT(IsTrue(Run.Resolved && Run.Projected));
  const auto& C = Run.Result.GetModules()[0]->Classes[0].Get();
  ASSERT_THAT(IsTrue(C.Properties[0]->Meta.Contains(TEXT("FutureFlag"))));
  ASSERT_THAT(AreEqual(FString(TEXT("BlueprintGetter")),C.Properties[0]->Meta.FindChecked(TEXT("ToolTip"))));
  ASSERT_THAT(AreEqual(FString(TEXT("BlueprintSetter")),C.Properties[0]->Meta.FindChecked(TEXT("Custom"))));
  ASSERT_THAT(AreEqual(FString(TEXT("BlueprintGetter")),C.Methods[0]->Meta.FindChecked(TEXT("DisplayName"))));
 }
 TEST_METHOD(DedicatedAccessorDescriptorFieldsAndAssociationConsumersAreAbsent)
 {
  ASSERT_THAT(IsFalse(ReflectionSurfaceTests::HasAccessorFields<FAngelscriptPropertyDesc>));
  ASSERT_THAT(IsFalse(ReflectionSurfaceTests::HasAccessorFields<FAngelscriptFunctionDesc>));
  ASSERT_THAT(IsFalse(ReflectionSurfaceTests::HasAccessorConsumer<FAngelscriptEngine>));
 }
};
#endif
