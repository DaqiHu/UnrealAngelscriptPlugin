#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestForLoopGenerator.h"
#include "Framework/Generate/AngelscriptTestBranchConditionGenerator.h"
#include "Framework/Generate/AngelscriptTestConditionGenerator.h"
#include "Framework/Generate/AngelscriptTestLiveLocalCleanupGenerator.h"
#include "Framework/Generate/AngelscriptTestLoopCondTransferGenerator.h"
#include "Framework/Generate/AngelscriptTestLoopDepthGenerator.h"
#include "Framework/Generate/AngelscriptTestNestedTargetGenerator.h"
#include "Framework/Generate/AngelscriptTestStatementTransferGenerator.h"
#include "Framework/Generate/AngelscriptTestSwitchGenerator.h"
#include "Framework/Generate/AngelscriptTestSwitchPlacementGenerator.h"
#include "Framework/Generate/AngelscriptTestTransferValidityGenerator.h"
#include "Framework/Generate/AngelscriptTestOpAssignmentGenerator.h"
#include "Framework/Generate/AngelscriptTestOpAssignmentTargetRejectionGenerator.h"
#include "Framework/Generate/AngelscriptTestOpAssignmentTypeRejectionGenerator.h"
#include "Framework/Generate/AngelscriptTestOpComparisonEnumAliasGenerator.h"
#include "Framework/Generate/AngelscriptTestOpComparisonFloatGenerator.h"
#include "Framework/Generate/AngelscriptTestOpComparisonOverloadGenerator.h"
#include "Framework/Generate/AngelscriptTestOpComparisonReferenceGenerator.h"
#include "Framework/Generate/AngelscriptTestOpFailureGenerator.h"
#include "Framework/Generate/AngelscriptTestOpIncrementGenerator.h"
#include "Framework/Generate/AngelscriptTestOpIncrementTargetRejectionGenerator.h"
#include "Framework/Generate/AngelscriptTestOpIncrementTypeRejectionGenerator.h"
#include "Framework/Generate/AngelscriptTestOpIntegralBitwiseGenerator.h"
#include "Framework/Generate/AngelscriptTestOpLogicalGenerator.h"
#include "Framework/Generate/AngelscriptTestOpLogicalNotGenerator.h"
#include "Framework/Generate/AngelscriptTestOpNumericBinaryGenerator.h"
#include "Framework/Generate/AngelscriptTestOpOverloadAssignmentConsumerGenerator.h"
#include "Framework/Generate/AngelscriptTestOpOverloadBooleanConsumerGenerator.h"
#include "Framework/Generate/AngelscriptTestOpOverloadDuplicateDeclarationGenerator.h"
#include "Framework/Generate/AngelscriptTestOpOverloadIntegerConsumerGenerator.h"
#include "Framework/Generate/AngelscriptTestOpPowerFractionalExponentGenerator.h"
#include "Framework/Generate/AngelscriptTestOpPowerNegativeExponentGenerator.h"
#include "Framework/Generate/AngelscriptTestOpPowerUniversalGenerator.h"
#include "Framework/Generate/AngelscriptTestOpResultContextGenerator.h"
#include "Framework/Generate/AngelscriptTestOpUnaryGenerator.h"
#include "Framework/Generate/AngelscriptTestOpUnaryRejectionGenerator.h"
#include "Framework/Generate/AngelscriptTestExprAssociativityGenerator.h"
#include "Framework/Generate/AngelscriptTestExprChainGenerator.h"
#include "Framework/Generate/AngelscriptTestExprEvalOrderGenerator.h"
#include "Framework/Generate/AngelscriptTestExprFailureGenerator.h"
#include "Framework/Generate/AngelscriptTestExprLazyEvaluationGenerator.h"
#include "Framework/Generate/AngelscriptTestExprPrecedenceGenerator.h"
#include "Framework/Generate/AngelscriptTestExprPrimaryContextGenerator.h"
#include "Framework/Generate/AngelscriptTestExprResolutionGenerator.h"
#include "Framework/Generate/AngelscriptTestExprSourceBoundaryGenerator.h"
#include "Framework/Generate/AngelscriptTestExprValueMutationGenerator.h"
#include "Framework/Generate/AngelscriptTestConvAbiGenerator.h"
#include "Framework/Generate/AngelscriptTestConvBoolContextGenerator.h"
#include "Framework/Generate/AngelscriptTestConvEnumAliasGenerator.h"
#include "Framework/Generate/AngelscriptTestConvFailureGenerator.h"
#include "Framework/Generate/AngelscriptTestConvFloatFiniteSpecialGenerator.h"
#include "Framework/Generate/AngelscriptTestConvFloat64ToFloat32RangeGenerator.h"
#include "Framework/Generate/AngelscriptTestConvNonfinitePreconversionGenerator.h"
#include "Framework/Generate/AngelscriptTestConvNumericGenerator.h"
#include "Framework/Generate/AngelscriptTestConvObjectCastGenerator.h"
#include "Framework/Generate/AngelscriptTestConvOverloadGenerator.h"
#include "Framework/Generate/AngelscriptTestConvValueObjectGenerator.h"
#include "Framework/Generate/AngelscriptTestFnArgSourceGenerator.h"
#include "Framework/Generate/AngelscriptTestFnArityTargetGenerator.h"
#include "Framework/Generate/AngelscriptTestFnArityTypeStressGenerator.h"
#include "Framework/Generate/AngelscriptTestFnDefaultsGenerator.h"
#include "Framework/Generate/AngelscriptTestFnDirectionDefaultGenerator.h"
#include "Framework/Generate/AngelscriptTestFnIndirectImportedGenerator.h"
#include "Framework/Generate/AngelscriptTestFnIndirectMixinGenerator.h"
#include "Framework/Generate/AngelscriptTestFnIndirectRegisteredFuncdefGenerator.h"
#include "Framework/Generate/AngelscriptTestFnIndirectScriptFuncdefGenerator.h"
#include "Framework/Generate/AngelscriptTestFnMixinDirectDispatchGenerator.h"
#include "Framework/Generate/AngelscriptTestFnMixinFreeCallRejectionGenerator.h"
#include "Framework/Generate/AngelscriptTestFnOverloadGenerator.h"
#include "Framework/Generate/AngelscriptTestFnParamDirectionGenerator.h"
#include "Framework/Generate/AngelscriptTestFnParamPositionGenerator.h"
#include "Framework/Generate/AngelscriptTestFnRecursionGenerator.h"
#include "Framework/Generate/AngelscriptTestFnReturnGenerator.h"
#include "Framework/Generate/AngelscriptTestFnSignatureShapeGenerator.h"
#include "Framework/Generate/AngelscriptTestFnTypedDefaultsGenerator.h"
#include "Framework/Generate/AngelscriptTestFnValueLifecycleGenerator.h"
#include "Framework/Generate/AngelscriptTestVarAssignTargetGenerator.h"
#include "Framework/Generate/AngelscriptTestVarCountedReferenceAssignmentGenerator.h"
#include "Framework/Generate/AngelscriptTestVarFailureBoundaryGenerator.h"
#include "Framework/Generate/AngelscriptTestVarInitStorageGenerator.h"
#include "Framework/Generate/AngelscriptTestVarLifetimeGenerator.h"
#include "Framework/Generate/AngelscriptTestVarLoopDeclLifetimeGenerator.h"
#include "Framework/Generate/AngelscriptTestVarReferenceInitGenerator.h"
#include "Framework/Generate/AngelscriptTestVarShadowGenerator.h"
#include "Framework/Generate/AngelscriptTestRefDirectionGenerator.h"
#include "Framework/Generate/AngelscriptTestRefDirectionForkGlobalGenerator.h"
#include "Framework/Generate/AngelscriptTestRefFailureGenerator.h"
#include "Framework/Generate/AngelscriptTestRefForkDerivedInrefGenerator.h"
#include "Framework/Generate/AngelscriptTestRefLifetimeGenerator.h"
#include "Framework/Generate/AngelscriptTestRefResolutionGenerator.h"
#include "Framework/Generate/AngelscriptTestRefSourceOpGenerator.h"
#include "Framework/Generate/AngelscriptTestPropAccessorGenerator.h"
#include "Framework/Generate/AngelscriptTestPropCopyIndependenceGenerator.h"
#include "Framework/Generate/AngelscriptTestPropFailureGenerator.h"
#include "Framework/Generate/AngelscriptTestPropForkSemanticsGenerator.h"
#include "Framework/Generate/AngelscriptTestPropIndexedGenerator.h"
#include "Framework/Generate/AngelscriptTestPropInitOrderGenerator.h"
#include "Framework/Generate/AngelscriptTestPropRebuildGenerator.h"
#include "Framework/Generate/AngelscriptTestPropValueOpGenerator.h"
#include "Framework/Generate/AngelscriptTestPropVisibilityGenerator.h"
#include "Framework/Generate/AngelscriptTestInhAccessGenerator.h"
#include "Framework/Generate/AngelscriptTestInhCastGenerator.h"
#include "Framework/Generate/AngelscriptTestInhClassRuleGenerator.h"
#include "Framework/Generate/AngelscriptTestInhDispatchGenerator.h"
#include "Framework/Generate/AngelscriptTestInhOverrideSignatureGenerator.h"
#include "Framework/Generate/AngelscriptTestDeclCollisionGenerator.h"
#include "Framework/Generate/AngelscriptTestDeclFailureRecoveryGenerator.h"
#include "Framework/Generate/AngelscriptTestDeclFamilyScopeOrderGenerator.h"
#include "Framework/Generate/AngelscriptTestCtorBoundaryGenerator.h"
#include "Framework/Generate/AngelscriptTestCtorKindCallGenerator.h"
#include "Framework/Generate/AngelscriptTestCtorOrderFailureGenerator.h"
#include "Framework/Generate/AngelscriptTestCtorParamSelectGenerator.h"
#include "Framework/Generate/AngelscriptTestCtorSpecialPolicyGenerator.h"
#include "Framework/Generate/AngelscriptTestCtorTransferGenerator.h"
#include "Framework/Generate/AngelscriptTestCtorVisibilityGenerator.h"
#include "Framework/Generate/AngelscriptTestDtorDeclarationGenerator.h"
#include "Framework/Generate/AngelscriptTestDtorOwnerExitGenerator.h"
#include "Framework/Generate/AngelscriptTestDtorPartialGenerator.h"
#include "Framework/Generate/AngelscriptTestForeachProtocolGenerator.h"
#include "Framework/Generate/AngelscriptTestForeachSizeVariableGenerator.h"
#include "Framework/Generate/AngelscriptTestForeachStructuralMutationGenerator.h"
#include "Framework/Generate/AngelscriptTestForeachTransferLifetimeGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	LanguageGeneratorCorpus,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FInventoryRow
	{
		const TCHAR* ProductId = nullptr;
		const TCHAR* ClassWithoutF = nullptr;
		int32 Cells = 0;
		int32 Normal = 0;
		int32 Reject = 0;
		int32 Fault = 0;
	};

	struct FBoundProduct
	{
		const FInventoryRow* Row = nullptr;
		FString ObservedProductId;
		TFunction<TArray<AngelscriptTest::Generate::FGeneratedCaseInfo>()> ListCases;
		TFunction<FString(FStringView)> BuildCaseSource;
		TFunction<FString(int32&)> BuildAllSource;
		TFunction<int32(FStringView)> GetExpected;
		TFunction<TArray<FString>()> ListRejectCaseIds;
		bool bHasRejectApi = false;
	};

	struct FDirectGenerators
	{
		AngelscriptTest::Generate::FForLoopGenerator ForLoopGenerator;
		AngelscriptTest::Generate::FBranchConditionGenerator BranchConditionGenerator;
		AngelscriptTest::Generate::FConditionGenerator ConditionGenerator;
		AngelscriptTest::Generate::FLiveLocalCleanupGenerator LiveLocalCleanupGenerator;
		AngelscriptTest::Generate::FLoopCondTransferGenerator LoopCondTransferGenerator;
		AngelscriptTest::Generate::FLoopDepthGenerator LoopDepthGenerator;
		AngelscriptTest::Generate::FNestedTargetGenerator NestedTargetGenerator;
		AngelscriptTest::Generate::FStatementTransferGenerator StatementTransferGenerator;
		AngelscriptTest::Generate::FSwitchGenerator SwitchGenerator;
		AngelscriptTest::Generate::FSwitchPlacementGenerator SwitchPlacementGenerator;
		AngelscriptTest::Generate::FTransferValidityGenerator TransferValidityGenerator;
		AngelscriptTest::Generate::FOpAssignmentGenerator OpAssignmentGenerator;
		AngelscriptTest::Generate::FOpAssignmentTargetRejectionGenerator OpAssignmentTargetRejectionGenerator;
		AngelscriptTest::Generate::FOpAssignmentTypeRejectionGenerator OpAssignmentTypeRejectionGenerator;
		AngelscriptTest::Generate::FOpComparisonEnumAliasGenerator OpComparisonEnumAliasGenerator;
		AngelscriptTest::Generate::FOpComparisonFloatGenerator OpComparisonFloatGenerator;
		AngelscriptTest::Generate::FOpComparisonOverloadGenerator OpComparisonOverloadGenerator;
		AngelscriptTest::Generate::FOpComparisonReferenceGenerator OpComparisonReferenceGenerator;
		AngelscriptTest::Generate::FOpFailureGenerator OpFailureGenerator;
		AngelscriptTest::Generate::FOpIncrementGenerator OpIncrementGenerator;
		AngelscriptTest::Generate::FOpIncrementTargetRejectionGenerator OpIncrementTargetRejectionGenerator;
		AngelscriptTest::Generate::FOpIncrementTypeRejectionGenerator OpIncrementTypeRejectionGenerator;
		AngelscriptTest::Generate::FOpIntegralBitwiseGenerator OpIntegralBitwiseGenerator;
		AngelscriptTest::Generate::FOpLogicalGenerator OpLogicalGenerator;
		AngelscriptTest::Generate::FOpLogicalNotGenerator OpLogicalNotGenerator;
		AngelscriptTest::Generate::FOpNumericBinaryGenerator OpNumericBinaryGenerator;
		AngelscriptTest::Generate::FOpOverloadAssignmentConsumerGenerator OpOverloadAssignmentConsumerGenerator;
		AngelscriptTest::Generate::FOpOverloadBooleanConsumerGenerator OpOverloadBooleanConsumerGenerator;
		AngelscriptTest::Generate::FOpOverloadDuplicateDeclarationGenerator OpOverloadDuplicateDeclarationGenerator;
		AngelscriptTest::Generate::FOpOverloadIntegerConsumerGenerator OpOverloadIntegerConsumerGenerator;
		AngelscriptTest::Generate::FOpPowerFractionalExponentGenerator OpPowerFractionalExponentGenerator;
		AngelscriptTest::Generate::FOpPowerNegativeExponentGenerator OpPowerNegativeExponentGenerator;
		AngelscriptTest::Generate::FOpPowerUniversalGenerator OpPowerUniversalGenerator;
		AngelscriptTest::Generate::FOpResultContextGenerator OpResultContextGenerator;
		AngelscriptTest::Generate::FOpUnaryGenerator OpUnaryGenerator;
		AngelscriptTest::Generate::FOpUnaryRejectionGenerator OpUnaryRejectionGenerator;
		AngelscriptTest::Generate::FExprAssociativityGenerator ExprAssociativityGenerator;
		AngelscriptTest::Generate::FExprChainGenerator ExprChainGenerator;
		AngelscriptTest::Generate::FExprEvalOrderGenerator ExprEvalOrderGenerator;
		AngelscriptTest::Generate::FExprFailureGenerator ExprFailureGenerator;
		AngelscriptTest::Generate::FExprLazyEvaluationGenerator ExprLazyEvaluationGenerator;
		AngelscriptTest::Generate::FExprPrecedenceGenerator ExprPrecedenceGenerator;
		AngelscriptTest::Generate::FExprPrimaryContextGenerator ExprPrimaryContextGenerator;
		AngelscriptTest::Generate::FExprResolutionGenerator ExprResolutionGenerator;
		AngelscriptTest::Generate::FExprSourceBoundaryGenerator ExprSourceBoundaryGenerator;
		AngelscriptTest::Generate::FExprValueMutationGenerator ExprValueMutationGenerator;
		AngelscriptTest::Generate::FConvAbiGenerator ConvAbiGenerator;
		AngelscriptTest::Generate::FConvBoolContextGenerator ConvBoolContextGenerator;
		AngelscriptTest::Generate::FConvEnumAliasGenerator ConvEnumAliasGenerator;
		AngelscriptTest::Generate::FConvFailureGenerator ConvFailureGenerator;
		AngelscriptTest::Generate::FConvFloatFiniteSpecialGenerator ConvFloatFiniteSpecialGenerator;
		AngelscriptTest::Generate::FConvFloat64ToFloat32RangeGenerator ConvFloat64ToFloat32RangeGenerator;
		AngelscriptTest::Generate::FConvNonfinitePreconversionGenerator ConvNonfinitePreconversionGenerator;
		AngelscriptTest::Generate::FConvNumericGenerator ConvNumericGenerator;
		AngelscriptTest::Generate::FConvObjectCastGenerator ConvObjectCastGenerator;
		AngelscriptTest::Generate::FConvOverloadGenerator ConvOverloadGenerator;
		AngelscriptTest::Generate::FConvValueObjectGenerator ConvValueObjectGenerator;
		AngelscriptTest::Generate::FFnArgSourceGenerator FnArgSourceGenerator;
		AngelscriptTest::Generate::FFnArityTargetGenerator FnArityTargetGenerator;
		AngelscriptTest::Generate::FFnArityTypeStressGenerator FnArityTypeStressGenerator;
		AngelscriptTest::Generate::FFnDefaultsGenerator FnDefaultsGenerator;
		AngelscriptTest::Generate::FFnDirectionDefaultGenerator FnDirectionDefaultGenerator;
		AngelscriptTest::Generate::FFnIndirectImportedGenerator FnIndirectImportedGenerator;
		AngelscriptTest::Generate::FFnIndirectMixinGenerator FnIndirectMixinGenerator;
		AngelscriptTest::Generate::FFnIndirectRegisteredFuncdefGenerator FnIndirectRegisteredFuncdefGenerator;
		AngelscriptTest::Generate::FFnIndirectScriptFuncdefGenerator FnIndirectScriptFuncdefGenerator;
		AngelscriptTest::Generate::FFnMixinDirectDispatchGenerator FnMixinDirectDispatchGenerator;
		AngelscriptTest::Generate::FFnMixinFreeCallRejectionGenerator FnMixinFreeCallRejectionGenerator;
		AngelscriptTest::Generate::FFnOverloadGenerator FnOverloadGenerator;
		AngelscriptTest::Generate::FFnParamDirectionGenerator FnParamDirectionGenerator;
		AngelscriptTest::Generate::FFnParamPositionGenerator FnParamPositionGenerator;
		AngelscriptTest::Generate::FFnRecursionGenerator FnRecursionGenerator;
		AngelscriptTest::Generate::FFnReturnGenerator FnReturnGenerator;
		AngelscriptTest::Generate::FFnSignatureShapeGenerator FnSignatureShapeGenerator;
		AngelscriptTest::Generate::FFnTypedDefaultsGenerator FnTypedDefaultsGenerator;
		AngelscriptTest::Generate::FFnValueLifecycleGenerator FnValueLifecycleGenerator;
		AngelscriptTest::Generate::FVarAssignTargetGenerator VarAssignTargetGenerator;
		AngelscriptTest::Generate::FVarCountedReferenceAssignmentGenerator VarCountedReferenceAssignmentGenerator;
		AngelscriptTest::Generate::FVarFailureBoundaryGenerator VarFailureBoundaryGenerator;
		AngelscriptTest::Generate::FVarInitStorageGenerator VarInitStorageGenerator;
		AngelscriptTest::Generate::FVarLifetimeGenerator VarLifetimeGenerator;
		AngelscriptTest::Generate::FVarLoopDeclLifetimeGenerator VarLoopDeclLifetimeGenerator;
		AngelscriptTest::Generate::FVarReferenceInitGenerator VarReferenceInitGenerator;
		AngelscriptTest::Generate::FVarShadowGenerator VarShadowGenerator;
		AngelscriptTest::Generate::FRefDirectionGenerator RefDirectionGenerator;
		AngelscriptTest::Generate::FRefDirectionForkGlobalGenerator RefDirectionForkGlobalGenerator;
		AngelscriptTest::Generate::FRefFailureGenerator RefFailureGenerator;
		AngelscriptTest::Generate::FRefForkDerivedInrefGenerator RefForkDerivedInrefGenerator;
		AngelscriptTest::Generate::FRefLifetimeGenerator RefLifetimeGenerator;
		AngelscriptTest::Generate::FRefResolutionGenerator RefResolutionGenerator;
		AngelscriptTest::Generate::FRefSourceOpGenerator RefSourceOpGenerator;
		AngelscriptTest::Generate::FPropAccessorGenerator PropAccessorGenerator;
		AngelscriptTest::Generate::FPropCopyIndependenceGenerator PropCopyIndependenceGenerator;
		AngelscriptTest::Generate::FPropFailureGenerator PropFailureGenerator;
		AngelscriptTest::Generate::FPropForkSemanticsGenerator PropForkSemanticsGenerator;
		AngelscriptTest::Generate::FPropIndexedGenerator PropIndexedGenerator;
		AngelscriptTest::Generate::FPropInitOrderGenerator PropInitOrderGenerator;
		AngelscriptTest::Generate::FPropRebuildGenerator PropRebuildGenerator;
		AngelscriptTest::Generate::FPropValueOpGenerator PropValueOpGenerator;
		AngelscriptTest::Generate::FPropVisibilityGenerator PropVisibilityGenerator;
		AngelscriptTest::Generate::FInhAccessGenerator InhAccessGenerator;
		AngelscriptTest::Generate::FInhCastGenerator InhCastGenerator;
		AngelscriptTest::Generate::FInhClassRuleGenerator InhClassRuleGenerator;
		AngelscriptTest::Generate::FInhDispatchGenerator InhDispatchGenerator;
		AngelscriptTest::Generate::FInhOverrideSignatureGenerator InhOverrideSignatureGenerator;
		AngelscriptTest::Generate::FDeclCollisionGenerator DeclCollisionGenerator;
		AngelscriptTest::Generate::FDeclFailureRecoveryGenerator DeclFailureRecoveryGenerator;
		AngelscriptTest::Generate::FDeclFamilyScopeOrderGenerator DeclFamilyScopeOrderGenerator;
		AngelscriptTest::Generate::FCtorBoundaryGenerator CtorBoundaryGenerator;
		AngelscriptTest::Generate::FCtorKindCallGenerator CtorKindCallGenerator;
		AngelscriptTest::Generate::FCtorOrderFailureGenerator CtorOrderFailureGenerator;
		AngelscriptTest::Generate::FCtorParamSelectGenerator CtorParamSelectGenerator;
		AngelscriptTest::Generate::FCtorSpecialPolicyGenerator CtorSpecialPolicyGenerator;
		AngelscriptTest::Generate::FCtorTransferGenerator CtorTransferGenerator;
		AngelscriptTest::Generate::FCtorVisibilityGenerator CtorVisibilityGenerator;
		AngelscriptTest::Generate::FDtorDeclarationGenerator DtorDeclarationGenerator;
		AngelscriptTest::Generate::FDtorOwnerExitGenerator DtorOwnerExitGenerator;
		AngelscriptTest::Generate::FDtorPartialGenerator DtorPartialGenerator;
		AngelscriptTest::Generate::FForeachProtocolGenerator ForeachProtocolGenerator;
		AngelscriptTest::Generate::FForeachSizeVariableGenerator ForeachSizeVariableGenerator;
		AngelscriptTest::Generate::FForeachStructuralMutationGenerator ForeachStructuralMutationGenerator;
		AngelscriptTest::Generate::FForeachTransferLifetimeGenerator ForeachTransferLifetimeGenerator;
	};

	struct FFakeConsumerTotals
	{
		int32 Passed = 0;
		int32 Skipped = 0;
		int32 Unsupported = 0;
		int32 ExceptionChecked = 0;
		int32 CompileRejectRouted = 0;
	};

	enum class EFakeDisposition : uint8
	{
		Passed,
		Skipped,
		Unsupported,
		ExceptionChecked,
		CompileRejectRouted,
	};

	template<typename T>
	struct TDetectListReject
	{
		template<typename U>
		static uint8 Test(decltype(&U::ListRejectCaseIds));
		template<typename U>
		static uint16 Test(...);
		static constexpr bool Value = sizeof(Test<T>(nullptr)) == sizeof(uint8);
	};

	static TArray<FInventoryRow> BuildInventory()
	{
		const FInventoryRow Rows[] = {
		{TEXT("LANG-CF-FOR-CLAUSES"), TEXT("ForLoopGenerator"), 24, 24, 0, 0},
		{TEXT("LANG-CF-BRANCH-CONDITION-DEPTH"), TEXT("BranchConditionGenerator"), 45, 45, 0, 0},
		{TEXT("LANG-CF-CONDITION"), TEXT("ConditionGenerator"), 56, 48, 8, 0},
		{TEXT("LANG-CF-LIVE-LOCAL-CLEANUP"), TEXT("LiveLocalCleanupGenerator"), 120, 96, 0, 24},
		{TEXT("LANG-CF-LOOP-COND-TRANSFER-DEPTH"), TEXT("LoopCondTransferGenerator"), 180, 180, 0, 0},
		{TEXT("LANG-CF-LOOP-DEPTH"), TEXT("LoopDepthGenerator"), 48, 48, 0, 0},
		{TEXT("LANG-CF-NESTED-TARGETS"), TEXT("NestedTargetGenerator"), 18, 18, 0, 0},
		{TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER"), TEXT("StatementTransferGenerator"), 1536, 1344, 0, 192},
		{TEXT("LANG-CF-SWITCH"), TEXT("SwitchGenerator"), 432, 209, 146, 77},
		{TEXT("LANG-CF-SWITCH-PLACEMENT"), TEXT("SwitchPlacementGenerator"), 16, 0, 16, 0},
		{TEXT("LANG-CF-TRANSFER-VALIDITY"), TEXT("TransferValidityGenerator"), 8, 3, 5, 0},
		{TEXT("LANG-OP-ASSIGNMENT"), TEXT("OpAssignmentGenerator"), 444, 444, 0, 0},
		{TEXT("LANG-OP-ASSIGNMENT-TARGET-REJECTION"), TEXT("OpAssignmentTargetRejectionGenerator"), 222, 0, 222, 0},
		{TEXT("LANG-OP-ASSIGNMENT-TYPE-REJECTION"), TEXT("OpAssignmentTypeRejectionGenerator"), 128, 0, 128, 0},
		{TEXT("LANG-OP-COMPARISON-ENUM-ALIAS"), TEXT("OpComparisonEnumAliasGenerator"), 144, 144, 0, 0},
		{TEXT("LANG-OP-COMPARISON-FLOAT"), TEXT("OpComparisonFloatGenerator"), 192, 192, 0, 0},
		{TEXT("LANG-OP-COMPARISON-OVERLOAD"), TEXT("OpComparisonOverloadGenerator"), 96, 96, 0, 0},
		{TEXT("LANG-OP-COMPARISON-REFERENCE"), TEXT("OpComparisonReferenceGenerator"), 96, 32, 64, 0},
		{TEXT("LANG-OP-FAILURE"), TEXT("OpFailureGenerator"), 102, 6, 42, 54},
		{TEXT("LANG-OP-INCREMENT"), TEXT("OpIncrementGenerator"), 480, 480, 0, 0},
		{TEXT("LANG-OP-INCREMENT-TARGET-REJECTION"), TEXT("OpIncrementTargetRejectionGenerator"), 80, 0, 80, 0},
		{TEXT("LANG-OP-INCREMENT-TYPE-REJECTION"), TEXT("OpIncrementTypeRejectionGenerator"), 16, 0, 16, 0},
		{TEXT("LANG-OP-INTEGRAL-BITWISE"), TEXT("OpIntegralBitwiseGenerator"), 1680, 1680, 0, 0},
		{TEXT("LANG-OP-LOGICAL"), TEXT("OpLogicalGenerator"), 192, 192, 0, 0},
		{TEXT("LANG-OP-LOGICAL-NOT"), TEXT("OpLogicalNotGenerator"), 10, 10, 0, 0},
		{TEXT("LANG-OP-NUMERIC-BINARY"), TEXT("OpNumericBinaryGenerator"), 5500, 5500, 0, 0},
		{TEXT("LANG-OP-OVERLOAD-ASSIGNMENT-CONSUMER"), TEXT("OpOverloadAssignmentConsumerGenerator"), 24, 12, 12, 0},
		{TEXT("LANG-OP-OVERLOAD-BOOLEAN-CONSUMER"), TEXT("OpOverloadBooleanConsumerGenerator"), 20, 10, 10, 0},
		{TEXT("LANG-OP-OVERLOAD-DUPLICATE-DECLARATION"), TEXT("OpOverloadDuplicateDeclarationGenerator"), 7, 0, 7, 0},
		{TEXT("LANG-OP-OVERLOAD-INTEGER-CONSUMER"), TEXT("OpOverloadIntegerConsumerGenerator"), 120, 66, 54, 0},
		{TEXT("LANG-OP-POWER-FRACTIONAL-EXPONENT"), TEXT("OpPowerFractionalExponentGenerator"), 80, 80, 0, 0},
		{TEXT("LANG-OP-POWER-NEGATIVE-EXPONENT"), TEXT("OpPowerNegativeExponentGenerator"), 240, 144, 96, 0},
		{TEXT("LANG-OP-POWER-UNIVERSAL"), TEXT("OpPowerUniversalGenerator"), 1600, 848, 680, 72},
		{TEXT("LANG-OP-RESULT-CONTEXT"), TEXT("OpResultContextGenerator"), 240, 180, 60, 0},
		{TEXT("LANG-OP-UNARY"), TEXT("OpUnaryGenerator"), 700, 700, 0, 0},
		{TEXT("LANG-OP-UNARY-REJECTION"), TEXT("OpUnaryRejectionGenerator"), 40, 0, 40, 0},
		{TEXT("LANG-EXPR-ASSOCIATIVITY"), TEXT("ExprAssociativityGenerator"), 72, 60, 12, 0},
		{TEXT("LANG-EXPR-CHAIN"), TEXT("ExprChainGenerator"), 800, 160, 320, 320},
		{TEXT("LANG-EXPR-EVAL-ORDER"), TEXT("ExprEvalOrderGenerator"), 540, 135, 0, 405},
		{TEXT("LANG-EXPR-FAILURE"), TEXT("ExprFailureGenerator"), 192, 0, 112, 80},
		{TEXT("LANG-EXPR-LAZY-EVALUATION"), TEXT("ExprLazyEvaluationGenerator"), 72, 60, 0, 12},
		{TEXT("LANG-EXPR-PRECEDENCE"), TEXT("ExprPrecedenceGenerator"), 432, 231, 201, 0},
		{TEXT("LANG-EXPR-PRIMARY-CONTEXT"), TEXT("ExprPrimaryContextGenerator"), 220, 174, 46, 0},
		{TEXT("LANG-EXPR-RESOLUTION"), TEXT("ExprResolutionGenerator"), 224, 112, 112, 0},
		{TEXT("LANG-EXPR-SOURCE-BOUNDARY"), TEXT("ExprSourceBoundaryGenerator"), 210, 210, 0, 0},
		{TEXT("LANG-EXPR-VALUE-MUTATION"), TEXT("ExprValueMutationGenerator"), 140, 60, 80, 0},
		{TEXT("LANG-CONV-ABI"), TEXT("ConvAbiGenerator"), 12, 12, 0, 0},
		{TEXT("LANG-CONV-BOOL-CONTEXT"), TEXT("ConvBoolContextGenerator"), 234, 18, 216, 0},
		{TEXT("LANG-CONV-ENUM-ALIAS"), TEXT("ConvEnumAliasGenerator"), 1260, 1130, 130, 0},
		{TEXT("LANG-CONV-FAILURE"), TEXT("ConvFailureGenerator"), 24, 4, 14, 6},
		{TEXT("LANG-CONV-FLOAT-FINITE-SPECIAL"), TEXT("ConvFloatFiniteSpecialGenerator"), 240, 240, 0, 0},
		{TEXT("LANG-CONV-FLOAT64-TO-FLOAT32-RANGE"), TEXT("ConvFloat64ToFloat32RangeGenerator"), 8, 8, 0, 0},
		{TEXT("LANG-CONV-NONFINITE-PRECONVERSION"), TEXT("ConvNonfinitePreconversionGenerator"), 240, 0, 0, 240},
		{TEXT("LANG-CONV-NUMERIC"), TEXT("ConvNumericGenerator"), 4200, 4200, 0, 0},
		{TEXT("LANG-CONV-OBJECT-CAST"), TEXT("ConvObjectCastGenerator"), 180, 108, 72, 0},
		{TEXT("LANG-CONV-OVERLOAD"), TEXT("ConvOverloadGenerator"), 144, 72, 72, 0},
		{TEXT("LANG-CONV-VALUE-OBJECT"), TEXT("ConvValueObjectGenerator"), 720, 200, 520, 0},
		{TEXT("LANG-FN-ARG-SOURCE"), TEXT("FnArgSourceGenerator"), 44, 22, 17, 5},
		{TEXT("LANG-FN-ARITY-TARGET"), TEXT("FnArityTargetGenerator"), 21, 21, 0, 0},
		{TEXT("LANG-FN-ARITY-TYPE-STRESS"), TEXT("FnArityTypeStressGenerator"), 96, 96, 0, 0},
		{TEXT("LANG-FN-DEFAULTS"), TEXT("FnDefaultsGenerator"), 72, 57, 15, 0},
		{TEXT("LANG-FN-DIRECTION-DEFAULT"), TEXT("FnDirectionDefaultGenerator"), 528, 309, 219, 0},
		{TEXT("LANG-FN-INDIRECT-IMPORTED"), TEXT("FnIndirectImportedGenerator"), 6, 0, 6, 0},
		{TEXT("LANG-FN-INDIRECT-MIXIN"), TEXT("FnIndirectMixinGenerator"), 6, 4, 2, 0},
		{TEXT("LANG-FN-INDIRECT-REGISTERED-FUNCDEF"), TEXT("FnIndirectRegisteredFuncdefGenerator"), 6, 0, 6, 0},
		{TEXT("LANG-FN-INDIRECT-SCRIPT-FUNCDEF"), TEXT("FnIndirectScriptFuncdefGenerator"), 6, 0, 6, 0},
		{TEXT("LANG-FN-MIXIN-DIRECT-DISPATCH"), TEXT("FnMixinDirectDispatchGenerator"), 2, 2, 0, 0},
		{TEXT("LANG-FN-MIXIN-FREE-CALL-REJECTION"), TEXT("FnMixinFreeCallRejectionGenerator"), 2, 0, 2, 0},
		{TEXT("LANG-FN-OVERLOAD"), TEXT("FnOverloadGenerator"), 28, 14, 14, 0},
		{TEXT("LANG-FN-PARAM-DIRECTION"), TEXT("FnParamDirectionGenerator"), 60, 60, 0, 0},
		{TEXT("LANG-FN-PARAM-POSITION"), TEXT("FnParamPositionGenerator"), 180, 180, 0, 0},
		{TEXT("LANG-FN-RECURSION"), TEXT("FnRecursionGenerator"), 24, 6, 12, 6},
		{TEXT("LANG-FN-RETURN"), TEXT("FnReturnGenerator"), 136, 86, 33, 17},
		{TEXT("LANG-FN-SIGNATURE-SHAPE"), TEXT("FnSignatureShapeGenerator"), 192, 192, 0, 0},
		{TEXT("LANG-FN-TYPED-DEFAULTS"), TEXT("FnTypedDefaultsGenerator"), 168, 168, 0, 0},
		{TEXT("LANG-FN-VALUE-LIFECYCLE"), TEXT("FnValueLifecycleGenerator"), 24, 6, 0, 18},
		{TEXT("LANG-VAR-ASSIGN-TARGET"), TEXT("VarAssignTargetGenerator"), 595, 212, 383, 0},
		{TEXT("LANG-VAR-COUNTED-REFERENCE-ASSIGNMENT"), TEXT("VarCountedReferenceAssignmentGenerator"), 7, 6, 0, 1},
		{TEXT("LANG-VAR-FAILURE-BOUNDARY"), TEXT("VarFailureBoundaryGenerator"), 28, 12, 12, 4},
		{TEXT("LANG-VAR-INIT-STORAGE"), TEXT("VarInitStorageGenerator"), 735, 713, 22, 0},
		{TEXT("LANG-VAR-LIFETIME"), TEXT("VarLifetimeGenerator"), 100, 80, 0, 20},
		{TEXT("LANG-VAR-LOOP-DECL-LIFETIME"), TEXT("VarLoopDeclLifetimeGenerator"), 200, 168, 0, 32},
		{TEXT("LANG-VAR-REFERENCE-INIT"), TEXT("VarReferenceInitGenerator"), 150, 128, 20, 2},
		{TEXT("LANG-VAR-SHADOW"), TEXT("VarShadowGenerator"), 110, 89, 21, 0},
		{TEXT("LANG-REF-DIRECTION"), TEXT("RefDirectionGenerator"), 96, 96, 0, 0},
		{TEXT("LANG-REF-DIRECTION-FORK-GLOBAL"), TEXT("RefDirectionForkGlobalGenerator"), 1, 0, 1, 0},
		{TEXT("LANG-REF-FAILURE"), TEXT("RefFailureGenerator"), 20, 0, 18, 2},
		{TEXT("LANG-REF-FORK-DERIVED-INREF"), TEXT("RefForkDerivedInrefGenerator"), 1, 0, 1, 0},
		{TEXT("LANG-REF-LIFETIME"), TEXT("RefLifetimeGenerator"), 200, 150, 50, 0},
		{TEXT("LANG-REF-RESOLUTION"), TEXT("RefResolutionGenerator"), 160, 111, 49, 0},
		{TEXT("LANG-REF-SOURCE-OP"), TEXT("RefSourceOpGenerator"), 288, 193, 88, 7},
		{TEXT("LANG-PROP-ACCESSOR"), TEXT("PropAccessorGenerator"), 72, 36, 28, 8},
		{TEXT("LANG-PROP-COPY-INDEPENDENCE"), TEXT("PropCopyIndependenceGenerator"), 459, 459, 0, 0},
		{TEXT("LANG-PROP-FAILURE"), TEXT("PropFailureGenerator"), 56, 0, 36, 20},
		{TEXT("LANG-PROP-FORK-SEMANTICS"), TEXT("PropForkSemanticsGenerator"), 13, 1, 12, 0},
		{TEXT("LANG-PROP-INDEXED"), TEXT("PropIndexedGenerator"), 468, 129, 339, 0},
		{TEXT("LANG-PROP-INIT-ORDER"), TEXT("PropInitOrderGenerator"), 1500, 1500, 0, 0},
		{TEXT("LANG-PROP-REBUILD"), TEXT("PropRebuildGenerator"), 90, 90, 0, 0},
		{TEXT("LANG-PROP-VALUE-OP"), TEXT("PropValueOpGenerator"), 300, 243, 57, 0},
		{TEXT("LANG-PROP-VISIBILITY"), TEXT("PropVisibilityGenerator"), 60, 42, 18, 0},
		{TEXT("LANG-INH-ACCESS"), TEXT("InhAccessGenerator"), 60, 37, 23, 0},
		{TEXT("LANG-INH-CAST"), TEXT("InhCastGenerator"), 60, 60, 0, 0},
		{TEXT("LANG-INH-CLASS-RULE"), TEXT("InhClassRuleGenerator"), 64, 24, 40, 0},
		{TEXT("LANG-INH-DISPATCH"), TEXT("InhDispatchGenerator"), 360, 360, 0, 0},
		{TEXT("LANG-INH-OVERRIDE-SIGNATURE"), TEXT("InhOverrideSignatureGenerator"), 54, 36, 18, 0},
		{TEXT("LANG-DECL-COLLISION"), TEXT("DeclCollisionGenerator"), 96, 48, 48, 0},
		{TEXT("LANG-DECL-FAILURE-RECOVERY"), TEXT("DeclFailureRecoveryGenerator"), 18, 0, 18, 0},
		{TEXT("LANG-DECL-FAMILY-SCOPE-ORDER"), TEXT("DeclFamilyScopeOrderGenerator"), 300, 300, 0, 0},
		{TEXT("LANG-CTOR-BOUNDARY"), TEXT("CtorBoundaryGenerator"), 68, 20, 40, 8},
		{TEXT("LANG-CTOR-KIND-CALL"), TEXT("CtorKindCallGenerator"), 288, 257, 31, 0},
		{TEXT("LANG-CTOR-ORDER-FAILURE"), TEXT("CtorOrderFailureGenerator"), 128, 16, 0, 112},
		{TEXT("LANG-CTOR-PARAM-SELECT"), TEXT("CtorParamSelectGenerator"), 300, 152, 148, 0},
		{TEXT("LANG-CTOR-SPECIAL-POLICY"), TEXT("CtorSpecialPolicyGenerator"), 64, 44, 20, 0},
		{TEXT("LANG-CTOR-TRANSFER"), TEXT("CtorTransferGenerator"), 448, 448, 0, 0},
		{TEXT("LANG-CTOR-VISIBILITY"), TEXT("CtorVisibilityGenerator"), 72, 21, 51, 0},
		{TEXT("LANG-DTOR-DECLARATION"), TEXT("DtorDeclarationGenerator"), 52, 40, 4, 8},
		{TEXT("LANG-DTOR-OWNER-EXIT"), TEXT("DtorOwnerExitGenerator"), 666, 522, 0, 144},
		{TEXT("LANG-DTOR-PARTIAL"), TEXT("DtorPartialGenerator"), 168, 126, 0, 42},
		{TEXT("LANG-FE-PROTOCOL"), TEXT("ForeachProtocolGenerator"), 250, 45, 190, 15},
		{TEXT("LANG-FE-SIZE-VARIABLE"), TEXT("ForeachSizeVariableGenerator"), 640, 319, 288, 33},
		{TEXT("LANG-FE-STRUCTURAL-MUTATION"), TEXT("ForeachStructuralMutationGenerator"), 36, 36, 0, 0},
		{TEXT("LANG-FE-TRANSFER-LIFETIME"), TEXT("ForeachTransferLifetimeGenerator"), 84, 72, 0, 12},
		};
		TArray<FInventoryRow> Inventory;
		Inventory.Append(Rows, UE_ARRAY_COUNT(Rows));
		return Inventory;
	}

	template<typename TGenerator>
	static FBoundProduct BindProduct(const FInventoryRow& Row, TGenerator& Generator)
	{
		FBoundProduct Bound;
		Bound.Row = &Row;
		Bound.ObservedProductId = FString(Generator.GetProductId());
		Bound.ListCases = [&Generator]()
		{
			return Generator.ListCases();
		};
		Bound.BuildCaseSource = [&Generator](const FStringView CaseId)
		{
			return Generator.BuildCaseSource(CaseId);
		};
		Bound.BuildAllSource = [&Generator](int32& OutCaseCount)
		{
			return Generator.BuildAllSource(OutCaseCount);
		};
		Bound.GetExpected = [&Generator](const FStringView CaseId)
		{
			return Generator.GetExpected(CaseId);
		};
		if constexpr (TDetectListReject<TGenerator>::Value)
		{
			Bound.ListRejectCaseIds = [&Generator]()
			{
				return Generator.ListRejectCaseIds();
			};
			Bound.bHasRejectApi = true;
		}
		return Bound;
	}

	static void BindAll(
		TArray<FBoundProduct>& Out,
		const TArray<FInventoryRow>& Inventory,
		FDirectGenerators& Generators)
	{
		Out.Reserve(122);
		Out.Add(BindProduct(Inventory[0], Generators.ForLoopGenerator));
		Out.Add(BindProduct(Inventory[1], Generators.BranchConditionGenerator));
		Out.Add(BindProduct(Inventory[2], Generators.ConditionGenerator));
		Out.Add(BindProduct(Inventory[3], Generators.LiveLocalCleanupGenerator));
		Out.Add(BindProduct(Inventory[4], Generators.LoopCondTransferGenerator));
		Out.Add(BindProduct(Inventory[5], Generators.LoopDepthGenerator));
		Out.Add(BindProduct(Inventory[6], Generators.NestedTargetGenerator));
		Out.Add(BindProduct(Inventory[7], Generators.StatementTransferGenerator));
		Out.Add(BindProduct(Inventory[8], Generators.SwitchGenerator));
		Out.Add(BindProduct(Inventory[9], Generators.SwitchPlacementGenerator));
		Out.Add(BindProduct(Inventory[10], Generators.TransferValidityGenerator));
		Out.Add(BindProduct(Inventory[11], Generators.OpAssignmentGenerator));
		Out.Add(BindProduct(Inventory[12], Generators.OpAssignmentTargetRejectionGenerator));
		Out.Add(BindProduct(Inventory[13], Generators.OpAssignmentTypeRejectionGenerator));
		Out.Add(BindProduct(Inventory[14], Generators.OpComparisonEnumAliasGenerator));
		Out.Add(BindProduct(Inventory[15], Generators.OpComparisonFloatGenerator));
		Out.Add(BindProduct(Inventory[16], Generators.OpComparisonOverloadGenerator));
		Out.Add(BindProduct(Inventory[17], Generators.OpComparisonReferenceGenerator));
		Out.Add(BindProduct(Inventory[18], Generators.OpFailureGenerator));
		Out.Add(BindProduct(Inventory[19], Generators.OpIncrementGenerator));
		Out.Add(BindProduct(Inventory[20], Generators.OpIncrementTargetRejectionGenerator));
		Out.Add(BindProduct(Inventory[21], Generators.OpIncrementTypeRejectionGenerator));
		Out.Add(BindProduct(Inventory[22], Generators.OpIntegralBitwiseGenerator));
		Out.Add(BindProduct(Inventory[23], Generators.OpLogicalGenerator));
		Out.Add(BindProduct(Inventory[24], Generators.OpLogicalNotGenerator));
		Out.Add(BindProduct(Inventory[25], Generators.OpNumericBinaryGenerator));
		Out.Add(BindProduct(Inventory[26], Generators.OpOverloadAssignmentConsumerGenerator));
		Out.Add(BindProduct(Inventory[27], Generators.OpOverloadBooleanConsumerGenerator));
		Out.Add(BindProduct(Inventory[28], Generators.OpOverloadDuplicateDeclarationGenerator));
		Out.Add(BindProduct(Inventory[29], Generators.OpOverloadIntegerConsumerGenerator));
		Out.Add(BindProduct(Inventory[30], Generators.OpPowerFractionalExponentGenerator));
		Out.Add(BindProduct(Inventory[31], Generators.OpPowerNegativeExponentGenerator));
		Out.Add(BindProduct(Inventory[32], Generators.OpPowerUniversalGenerator));
		Out.Add(BindProduct(Inventory[33], Generators.OpResultContextGenerator));
		Out.Add(BindProduct(Inventory[34], Generators.OpUnaryGenerator));
		Out.Add(BindProduct(Inventory[35], Generators.OpUnaryRejectionGenerator));
		Out.Add(BindProduct(Inventory[36], Generators.ExprAssociativityGenerator));
		Out.Add(BindProduct(Inventory[37], Generators.ExprChainGenerator));
		Out.Add(BindProduct(Inventory[38], Generators.ExprEvalOrderGenerator));
		Out.Add(BindProduct(Inventory[39], Generators.ExprFailureGenerator));
		Out.Add(BindProduct(Inventory[40], Generators.ExprLazyEvaluationGenerator));
		Out.Add(BindProduct(Inventory[41], Generators.ExprPrecedenceGenerator));
		Out.Add(BindProduct(Inventory[42], Generators.ExprPrimaryContextGenerator));
		Out.Add(BindProduct(Inventory[43], Generators.ExprResolutionGenerator));
		Out.Add(BindProduct(Inventory[44], Generators.ExprSourceBoundaryGenerator));
		Out.Add(BindProduct(Inventory[45], Generators.ExprValueMutationGenerator));
		Out.Add(BindProduct(Inventory[46], Generators.ConvAbiGenerator));
		Out.Add(BindProduct(Inventory[47], Generators.ConvBoolContextGenerator));
		Out.Add(BindProduct(Inventory[48], Generators.ConvEnumAliasGenerator));
		Out.Add(BindProduct(Inventory[49], Generators.ConvFailureGenerator));
		Out.Add(BindProduct(Inventory[50], Generators.ConvFloatFiniteSpecialGenerator));
		Out.Add(BindProduct(Inventory[51], Generators.ConvFloat64ToFloat32RangeGenerator));
		Out.Add(BindProduct(Inventory[52], Generators.ConvNonfinitePreconversionGenerator));
		Out.Add(BindProduct(Inventory[53], Generators.ConvNumericGenerator));
		Out.Add(BindProduct(Inventory[54], Generators.ConvObjectCastGenerator));
		Out.Add(BindProduct(Inventory[55], Generators.ConvOverloadGenerator));
		Out.Add(BindProduct(Inventory[56], Generators.ConvValueObjectGenerator));
		Out.Add(BindProduct(Inventory[57], Generators.FnArgSourceGenerator));
		Out.Add(BindProduct(Inventory[58], Generators.FnArityTargetGenerator));
		Out.Add(BindProduct(Inventory[59], Generators.FnArityTypeStressGenerator));
		Out.Add(BindProduct(Inventory[60], Generators.FnDefaultsGenerator));
		Out.Add(BindProduct(Inventory[61], Generators.FnDirectionDefaultGenerator));
		Out.Add(BindProduct(Inventory[62], Generators.FnIndirectImportedGenerator));
		Out.Add(BindProduct(Inventory[63], Generators.FnIndirectMixinGenerator));
		Out.Add(BindProduct(Inventory[64], Generators.FnIndirectRegisteredFuncdefGenerator));
		Out.Add(BindProduct(Inventory[65], Generators.FnIndirectScriptFuncdefGenerator));
		Out.Add(BindProduct(Inventory[66], Generators.FnMixinDirectDispatchGenerator));
		Out.Add(BindProduct(Inventory[67], Generators.FnMixinFreeCallRejectionGenerator));
		Out.Add(BindProduct(Inventory[68], Generators.FnOverloadGenerator));
		Out.Add(BindProduct(Inventory[69], Generators.FnParamDirectionGenerator));
		Out.Add(BindProduct(Inventory[70], Generators.FnParamPositionGenerator));
		Out.Add(BindProduct(Inventory[71], Generators.FnRecursionGenerator));
		Out.Add(BindProduct(Inventory[72], Generators.FnReturnGenerator));
		Out.Add(BindProduct(Inventory[73], Generators.FnSignatureShapeGenerator));
		Out.Add(BindProduct(Inventory[74], Generators.FnTypedDefaultsGenerator));
		Out.Add(BindProduct(Inventory[75], Generators.FnValueLifecycleGenerator));
		Out.Add(BindProduct(Inventory[76], Generators.VarAssignTargetGenerator));
		Out.Add(BindProduct(Inventory[77], Generators.VarCountedReferenceAssignmentGenerator));
		Out.Add(BindProduct(Inventory[78], Generators.VarFailureBoundaryGenerator));
		Out.Add(BindProduct(Inventory[79], Generators.VarInitStorageGenerator));
		Out.Add(BindProduct(Inventory[80], Generators.VarLifetimeGenerator));
		Out.Add(BindProduct(Inventory[81], Generators.VarLoopDeclLifetimeGenerator));
		Out.Add(BindProduct(Inventory[82], Generators.VarReferenceInitGenerator));
		Out.Add(BindProduct(Inventory[83], Generators.VarShadowGenerator));
		Out.Add(BindProduct(Inventory[84], Generators.RefDirectionGenerator));
		Out.Add(BindProduct(Inventory[85], Generators.RefDirectionForkGlobalGenerator));
		Out.Add(BindProduct(Inventory[86], Generators.RefFailureGenerator));
		Out.Add(BindProduct(Inventory[87], Generators.RefForkDerivedInrefGenerator));
		Out.Add(BindProduct(Inventory[88], Generators.RefLifetimeGenerator));
		Out.Add(BindProduct(Inventory[89], Generators.RefResolutionGenerator));
		Out.Add(BindProduct(Inventory[90], Generators.RefSourceOpGenerator));
		Out.Add(BindProduct(Inventory[91], Generators.PropAccessorGenerator));
		Out.Add(BindProduct(Inventory[92], Generators.PropCopyIndependenceGenerator));
		Out.Add(BindProduct(Inventory[93], Generators.PropFailureGenerator));
		Out.Add(BindProduct(Inventory[94], Generators.PropForkSemanticsGenerator));
		Out.Add(BindProduct(Inventory[95], Generators.PropIndexedGenerator));
		Out.Add(BindProduct(Inventory[96], Generators.PropInitOrderGenerator));
		Out.Add(BindProduct(Inventory[97], Generators.PropRebuildGenerator));
		Out.Add(BindProduct(Inventory[98], Generators.PropValueOpGenerator));
		Out.Add(BindProduct(Inventory[99], Generators.PropVisibilityGenerator));
		Out.Add(BindProduct(Inventory[100], Generators.InhAccessGenerator));
		Out.Add(BindProduct(Inventory[101], Generators.InhCastGenerator));
		Out.Add(BindProduct(Inventory[102], Generators.InhClassRuleGenerator));
		Out.Add(BindProduct(Inventory[103], Generators.InhDispatchGenerator));
		Out.Add(BindProduct(Inventory[104], Generators.InhOverrideSignatureGenerator));
		Out.Add(BindProduct(Inventory[105], Generators.DeclCollisionGenerator));
		Out.Add(BindProduct(Inventory[106], Generators.DeclFailureRecoveryGenerator));
		Out.Add(BindProduct(Inventory[107], Generators.DeclFamilyScopeOrderGenerator));
		Out.Add(BindProduct(Inventory[108], Generators.CtorBoundaryGenerator));
		Out.Add(BindProduct(Inventory[109], Generators.CtorKindCallGenerator));
		Out.Add(BindProduct(Inventory[110], Generators.CtorOrderFailureGenerator));
		Out.Add(BindProduct(Inventory[111], Generators.CtorParamSelectGenerator));
		Out.Add(BindProduct(Inventory[112], Generators.CtorSpecialPolicyGenerator));
		Out.Add(BindProduct(Inventory[113], Generators.CtorTransferGenerator));
		Out.Add(BindProduct(Inventory[114], Generators.CtorVisibilityGenerator));
		Out.Add(BindProduct(Inventory[115], Generators.DtorDeclarationGenerator));
		Out.Add(BindProduct(Inventory[116], Generators.DtorOwnerExitGenerator));
		Out.Add(BindProduct(Inventory[117], Generators.DtorPartialGenerator));
		Out.Add(BindProduct(Inventory[118], Generators.ForeachProtocolGenerator));
		Out.Add(BindProduct(Inventory[119], Generators.ForeachSizeVariableGenerator));
		Out.Add(BindProduct(Inventory[120], Generators.ForeachStructuralMutationGenerator));
		Out.Add(BindProduct(Inventory[121], Generators.ForeachTransferLifetimeGenerator));
	}

	static FString ProductDetail(const TCHAR* ProductId, const TCHAR* Detail)
	{
		return FString::Printf(TEXT("%s: %s"), ProductId, Detail);
	}

	static EFakeDisposition DispatchFakeConsumer(const AngelscriptTest::Generate::FGeneratedCaseInfo& Info)
	{
		if (Info.Outcome == AngelscriptTest::Generate::EGeneratedCaseOutcome::CompileReject)
		{
			return EFakeDisposition::CompileRejectRouted;
		}
		if (Info.Execution == AngelscriptTest::Generate::EGeneratedCaseExecution::RequiresHostSetup)
		{
			return EFakeDisposition::Skipped;
		}
		if (Info.Execution == AngelscriptTest::Generate::EGeneratedCaseExecution::SourceOnly)
		{
			return EFakeDisposition::Unsupported;
		}
		if (Info.Outcome == AngelscriptTest::Generate::EGeneratedCaseOutcome::RuntimeException)
		{
			return EFakeDisposition::ExceptionChecked;
		}
		return EFakeDisposition::Passed;
	}

	static FString ExpectedExportPath(const FInventoryRow& Row)
	{
		if (FCString::Strcmp(Row.ProductId, TEXT("LANG-CF-FOR-CLAUSES")) == 0)
		{
			return FString::Printf(TEXT("Angelscript.UnitTest.Framework.%s.GeneratesAndExportsAllCases"), Row.ClassWithoutF);
		}
		return FString::Printf(
			TEXT("Angelscript.UnitTest.Framework.Generate.%s.GeneratesAndExportsAllCases"),
			Row.ClassWithoutF);
	}

public:
	TEST_METHOD(VerifiesCompleteCorpus)
	{
		using AngelscriptTest::FGeneratedCaseExport;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;

		const TArray<FInventoryRow> Inventory = BuildInventory();
		ASSERT_THAT(AreEqual(122, Inventory.Num(), TEXT("oracle row count")));

		int32 OracleCells = 0;
		int32 OracleNormal = 0;
		int32 OracleReject = 0;
		int32 OracleFault = 0;
		int32 ForLoopRows = 0;
		for (const FInventoryRow& Row : Inventory)
		{
			ASSERT_THAT(AreEqual(
				Row.Cells,
				Row.Normal + Row.Reject + Row.Fault,
				*ProductDetail(Row.ProductId, TEXT("Normal+Reject+Fault must equal Cells"))));
			OracleCells += Row.Cells;
			OracleNormal += Row.Normal;
			OracleReject += Row.Reject;
			OracleFault += Row.Fault;
			if (FCString::Strcmp(Row.ProductId, TEXT("LANG-CF-FOR-CLAUSES")) == 0)
			{
				++ForLoopRows;
				ASSERT_THAT(AreEqual(24, Row.Cells, TEXT("LANG-CF-FOR-CLAUSES cells")));
			}
		}
		ASSERT_THAT(AreEqual(1, ForLoopRows, TEXT("ForLoop must appear once in the oracle")));
		ASSERT_THAT(AreEqual(36686, OracleCells, TEXT("oracle cell total")));
		ASSERT_THAT(AreEqual(28689, OracleNormal, TEXT("oracle normal total")));
		ASSERT_THAT(AreEqual(5999, OracleReject, TEXT("oracle reject total")));
		ASSERT_THAT(AreEqual(1998, OracleFault, TEXT("oracle fault total")));

		FDirectGenerators Generators;
		TArray<FBoundProduct> Products;
		BindAll(Products, Inventory, Generators);
		ASSERT_THAT(AreEqual(122, Products.Num(), TEXT("direct-constructed products")));

		TSet<FString> DistinctProductIds;
		TSet<FString> DistinctCaseIds;
		TSet<FString> DumpLeaves;
		FFakeConsumerTotals Consumer;
		bool bSawNormalZero = false;
		bool bSawDivideByZero = false;
		FString NormalZeroCaseId;
		FString DivideByZeroCaseId;
		int32 TransferValidityAggregate = 0;
		int32 TransferValidityReject = 0;
		int32 RegisteredFuncdefAggregate = 0;
		int32 RegisteredFuncdefReject = 0;
		int32 ConvAbiHostRows = 0;
		int32 PropRebuildLifecycleRows = 0;

		for (int32 ProductIndex = 0; ProductIndex < Products.Num(); ++ProductIndex)
		{
			FBoundProduct& Product = Products[ProductIndex];
			const FInventoryRow& Row = *Product.Row;
			const TCHAR* const ProductId = Row.ProductId;
			ASSERT_THAT(AreEqual(
				FString(ProductId),
				Product.ObservedProductId,
				*ProductDetail(ProductId, TEXT("GetProductId"))));
			ASSERT_THAT(IsTrue(
				!DistinctProductIds.Contains(Product.ObservedProductId),
				*ProductDetail(ProductId, TEXT("duplicate product id"))));
			DistinctProductIds.Add(Product.ObservedProductId);

			TArray<FGeneratedCaseInfo> Cases = Product.ListCases();
			int32 OutCaseCount = 0;
			const FString Aggregate = Product.BuildAllSource(OutCaseCount);
			const int32 NeighborIndex = (ProductIndex + 1) % Products.Num();
			int32 NeighborCount = 0;
			Products[NeighborIndex].BuildAllSource(NeighborCount);
			int32 RepeatCount = 0;
			const FString RepeatAggregate = Product.BuildAllSource(RepeatCount);
			TArray<FGeneratedCaseInfo> NeighborCases = Products[NeighborIndex].ListCases();
			TArray<FGeneratedCaseInfo> RepeatCases = Product.ListCases();
			ASSERT_THAT(AreEqual(
				Products[NeighborIndex].Row->Normal + Products[NeighborIndex].Row->Fault,
				NeighborCount,
				*ProductDetail(Products[NeighborIndex].Row->ProductId, TEXT("neighbor OutCaseCount"))));
			ASSERT_THAT(AreNotEqual(
				FString(ProductId),
				FString(Products[NeighborIndex].Row->ProductId),
				*ProductDetail(ProductId, TEXT("interleaved product must be distinct"))));
			if (Cases.Num() > 0 && NeighborCases.Num() > 0)
			{
				ASSERT_THAT(AreNotEqual(
					Cases[0].CaseId,
					NeighborCases[0].CaseId,
					*ProductDetail(ProductId, TEXT("independent neighbor case id"))));
			}

			ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate, *ProductDetail(ProductId, TEXT("determinism"))));
			ASSERT_THAT(AreEqual(OutCaseCount, RepeatCount, *ProductDetail(ProductId, TEXT("repeat OutCaseCount"))));
			ASSERT_THAT(AreEqual(Cases.Num(), RepeatCases.Num(), *ProductDetail(ProductId, TEXT("repeat ListCases count"))));
			for (int32 CaseIndex = 0; CaseIndex < Cases.Num(); ++CaseIndex)
			{
				ASSERT_THAT(AreEqual(
					Cases[CaseIndex].CaseId,
					RepeatCases[CaseIndex].CaseId,
					*ProductDetail(ProductId, TEXT("repeat ListCases order"))));
			}

			TArray<FGeneratedCaseInfo> Owned = Cases;
			if (Owned.Num() > 0)
			{
				Owned[0].CaseId = TEXT("MUTATED");
				Owned[0].ExpectedReturn.Reset();
			}
			int32 OwnershipNeighborCount = 0;
			Products[NeighborIndex].BuildAllSource(OwnershipNeighborCount);
			const TArray<FGeneratedCaseInfo> Relisted = Product.ListCases();
			ASSERT_THAT(AreEqual(Cases.Num(), Relisted.Num(), *ProductDetail(ProductId, TEXT("owned ListCases count"))));
			if (Cases.Num() > 0)
			{
				ASSERT_THAT(AreEqual(
					Cases[0].CaseId,
					Relisted[0].CaseId,
					*ProductDetail(ProductId, TEXT("descriptor ownership"))));
			}

			int32 RejectCount = 0;
			TArray<FString> ProjectedRejects;
			TSet<FString> CanonicalNames;
			for (const FGeneratedCaseInfo& Info : Cases)
			{
				ASSERT_THAT(IsTrue(
					!DistinctCaseIds.Contains(Info.CaseId),
					*FString::Printf(TEXT("%s duplicate cell %s"), ProductId, *Info.CaseId)));
				DistinctCaseIds.Add(Info.CaseId);

				const FString Canonical = MakeCanonicalEntryName(Info.CaseId);
				ASSERT_THAT(IsTrue(
					!CanonicalNames.Contains(Canonical),
					*FString::Printf(TEXT("%s collapsed PascalCase %s"), ProductId, *Info.CaseId)));
				CanonicalNames.Add(Canonical);
				ASSERT_THAT(IsFalse(
					Canonical.Contains(TEXT("_")),
					*FString::Printf(TEXT("%s underscore in %s"), ProductId, *Canonical)));

				const FString CaseSource = Product.BuildCaseSource(Info.CaseId);
				ASSERT_THAT(IsTrue(
					CaseSource.Len() > 0,
					*FString::Printf(TEXT("%s empty BuildCaseSource %s"), ProductId, *Info.CaseId)));

				if (Info.Outcome == EGeneratedCaseOutcome::CompileReject)
				{
					++RejectCount;
					ProjectedRejects.Add(Info.CaseId);
					ASSERT_THAT(IsTrue(
						Info.EntryDeclaration.IsEmpty(),
						*FString::Printf(TEXT("%s reject EntryDeclaration %s"), ProductId, *Info.CaseId)));
				}
				else
				{
					const FString ExpectedDeclaration = FString::Printf(TEXT("int %s()"), *Canonical);
					ASSERT_THAT(AreEqual(
						ExpectedDeclaration,
						Info.EntryDeclaration,
						*FString::Printf(TEXT("%s PascalCase declaration %s"), ProductId, *Info.CaseId)));
					ASSERT_THAT(IsTrue(
						CaseSource.Contains(Info.EntryDeclaration),
						*FString::Printf(TEXT("%s single-source declaration %s"), ProductId, *Info.CaseId)));
					ASSERT_THAT(IsTrue(
						Aggregate.Contains(Info.EntryDeclaration),
						*FString::Printf(TEXT("%s aggregate declaration %s"), ProductId, *Info.CaseId)));
				}

				const EFakeDisposition Disposition = DispatchFakeConsumer(Info);
				if (Disposition == EFakeDisposition::Passed)
				{
					++Consumer.Passed;
					ASSERT_THAT(IsTrue(
						Info.ExpectedReturn.IsSet(),
						*FString::Printf(TEXT("%s passed row missing ExpectedReturn %s"), ProductId, *Info.CaseId)));
					if (Info.ExpectedReturn.Get(MIN_int32) == 0)
					{
						bSawNormalZero = true;
						NormalZeroCaseId = Info.CaseId;
					}
				}
				else if (Disposition == EFakeDisposition::Skipped)
				{
					++Consumer.Skipped;
				}
				else if (Disposition == EFakeDisposition::Unsupported)
				{
					++Consumer.Unsupported;
				}
				else if (Disposition == EFakeDisposition::ExceptionChecked)
				{
					++Consumer.ExceptionChecked;
					ASSERT_THAT(IsFalse(
						Info.ExpectedReturn.IsSet(),
						*FString::Printf(TEXT("%s exception row has integer %s"), ProductId, *Info.CaseId)));
				}
				else
				{
					++Consumer.CompileRejectRouted;
				}

				if (Info.ExpectedException == TEXT("Divide by zero"))
				{
					bSawDivideByZero = true;
					DivideByZeroCaseId = Info.CaseId;
					ASSERT_THAT(IsFalse(
						Info.ExpectedReturn.IsSet(),
						*FString::Printf(TEXT("%s divide-by-zero integer %s"), ProductId, *Info.CaseId)));
					ASSERT_THAT(IsTrue(
						Disposition == EFakeDisposition::ExceptionChecked
							|| Disposition == EFakeDisposition::Skipped
							|| Disposition == EFakeDisposition::Unsupported,
						*FString::Printf(TEXT("%s divide-by-zero routed as integer %s"), ProductId, *Info.CaseId)));
				}

				if (FCString::Strcmp(ProductId, TEXT("LANG-CONV-ABI")) == 0)
				{
					ASSERT_THAT(IsTrue(
						Info.Execution == EGeneratedCaseExecution::RequiresHostSetup,
						*FString::Printf(TEXT("LANG-CONV-ABI RequiresHostSetup %s"), *Info.CaseId)));
					++ConvAbiHostRows;
				}
				if (FCString::Strcmp(ProductId, TEXT("LANG-PROP-REBUILD")) == 0
					&& Info.Execution == EGeneratedCaseExecution::SourceOnly)
				{
					++PropRebuildLifecycleRows;
				}
			}

			const int32 NonRejectCount = Cases.Num() - RejectCount;
			ASSERT_THAT(AreEqual(Row.Cells, Cases.Num(), *ProductDetail(ProductId, TEXT("ListCases cells"))));
			ASSERT_THAT(AreEqual(Row.Reject, RejectCount, *ProductDetail(ProductId, TEXT("reject partition"))));
			ASSERT_THAT(AreEqual(
				Row.Normal + Row.Fault,
				NonRejectCount,
				*ProductDetail(ProductId, TEXT("non-reject = Normal+Fault"))));
			ASSERT_THAT(AreEqual(
				Row.Normal + Row.Fault,
				OutCaseCount,
				*ProductDetail(ProductId, TEXT("OutCaseCount = Normal+Fault"))));
			if (Row.Normal + Row.Fault == 0)
			{
				ASSERT_THAT(AreEqual(0, OutCaseCount, *ProductDetail(ProductId, TEXT("reject-only OutCaseCount"))));
			}

			if (Product.bHasRejectApi)
			{
				const TArray<FString> RejectIds = Product.ListRejectCaseIds();
				ASSERT_THAT(AreEqual(
					ProjectedRejects.Num(),
					RejectIds.Num(),
					*ProductDetail(ProductId, TEXT("ListRejectCaseIds count"))));
				for (int32 RejectIndex = 0; RejectIndex < RejectIds.Num(); ++RejectIndex)
				{
					ASSERT_THAT(AreEqual(
						ProjectedRejects[RejectIndex],
						RejectIds[RejectIndex],
						*ProductDetail(ProductId, TEXT("ListRejectCaseIds order"))));
				}
			}

			if (FCString::Strcmp(ProductId, TEXT("LANG-CF-TRANSFER-VALIDITY")) == 0)
			{
				TransferValidityAggregate = NonRejectCount;
				TransferValidityReject = RejectCount;
			}
			if (FCString::Strcmp(ProductId, TEXT("LANG-FN-INDIRECT-REGISTERED-FUNCDEF")) == 0)
			{
				RegisteredFuncdefAggregate = NonRejectCount;
				RegisteredFuncdefReject = RejectCount;
			}

			const FString DumpLeaf = FString(Row.ClassWithoutF) + TEXT(".as");
			ASSERT_THAT(IsFalse(DumpLeaf.Contains(TEXT("_")), *ProductDetail(ProductId, TEXT("dump underscore"))));
			ASSERT_THAT(IsTrue(
				!DumpLeaves.Contains(DumpLeaf),
				*ProductDetail(ProductId, TEXT("duplicate dump filename"))));
			DumpLeaves.Add(DumpLeaf);
			const FString ResolvedDump = FGeneratedCaseExport::ResolveOutputFile(Row.ClassWithoutF);
			if (ResolvedDump.Len() > 0)
			{
				ASSERT_THAT(IsTrue(
					ResolvedDump.EndsWith(DumpLeaf) || ResolvedDump.EndsWith(FString(Row.ClassWithoutF) + TEXT(".as")),
					*ProductDetail(ProductId, TEXT("resolved dump leaf"))));
			}
		}

		ASSERT_THAT(AreEqual(122, DistinctProductIds.Num(), TEXT("distinct products")));
		ASSERT_THAT(AreEqual(36686, DistinctCaseIds.Num(), TEXT("distinct cells")));
		ASSERT_THAT(AreEqual(122, DumpLeaves.Num(), TEXT("distinct dump filenames")));
		ASSERT_THAT(AreEqual(3, TransferValidityAggregate, TEXT("TransferValidity aggregate")));
		ASSERT_THAT(AreEqual(5, TransferValidityReject, TEXT("TransferValidity reject")));
		ASSERT_THAT(AreEqual(0, RegisteredFuncdefAggregate, TEXT("RegisteredFuncdef aggregate")));
		ASSERT_THAT(AreEqual(6, RegisteredFuncdefReject, TEXT("RegisteredFuncdef reject")));
		ASSERT_THAT(AreEqual(12, ConvAbiHostRows, TEXT("ConvAbi RequiresHostSetup rows")));
		ASSERT_THAT(IsTrue(PropRebuildLifecycleRows > 0, TEXT("PropRebuild lifecycle SourceOnly rows")));
		ASSERT_THAT(IsTrue(bSawNormalZero, TEXT("a normal zero ExpectedReturn must exist")));
		ASSERT_THAT(IsTrue(bSawDivideByZero, TEXT("a Divide by zero row must exist")));

		const FString UnknownSource = Products[0].BuildCaseSource(TEXT("UNKNOWN-CASE"));
		ASSERT_THAT(IsTrue(UnknownSource.IsEmpty(), TEXT("unknown IDs produce empty source")));
		ASSERT_THAT(AreEqual(0, Products[0].GetExpected(TEXT("UNKNOWN-CASE")), TEXT("unknown GetExpected fallback")));
		ASSERT_THAT(IsFalse(
			DistinctCaseIds.Contains(TEXT("UNKNOWN-CASE")),
			TEXT("unknown GetExpected == 0 is not membership")));

		ASSERT_THAT(IsTrue(
			DistinctCaseIds.Contains(NormalZeroCaseId),
			TEXT("normal zero is a listed member")));
		ASSERT_THAT(IsTrue(
			DistinctCaseIds.Contains(DivideByZeroCaseId),
			TEXT("divide-by-zero is a listed member")));

		const FString LoopDepthBreak = MakeCanonicalEntryName(TEXT("LANG-CF-LOOP-DEPTH-WHILE-ZERO-BREAK"));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCfLoopDepthWhileZeroBreak")),
			LoopDepthBreak,
			TEXT("LoopDepth WHILE-ZERO-BREAK PascalCase")));

		FBoundProduct* TransferValidity = nullptr;
		FBoundProduct* RegisteredFuncdef = nullptr;
		for (FBoundProduct& Product : Products)
		{
			if (FCString::Strcmp(Product.Row->ProductId, TEXT("LANG-CF-TRANSFER-VALIDITY")) == 0)
			{
				TransferValidity = &Product;
			}
			if (FCString::Strcmp(Product.Row->ProductId, TEXT("LANG-FN-INDIRECT-REGISTERED-FUNCDEF")) == 0)
			{
				RegisteredFuncdef = &Product;
			}
		}
		ASSERT_THAT(IsTrue(TransferValidity != nullptr, TEXT("TransferValidity constructed")));
		ASSERT_THAT(IsTrue(RegisteredFuncdef != nullptr, TEXT("RegisteredFuncdef constructed")));
		const TArray<FGeneratedCaseInfo> TransferCases = TransferValidity->ListCases();
		const TArray<FString> TransferRejectIds = TransferValidity->ListRejectCaseIds();
		ASSERT_THAT(AreEqual(5, TransferRejectIds.Num(), TEXT("TransferValidity ListRejectCaseIds")));
		int32 TransferIndependentRejects = 0;
		for (const FGeneratedCaseInfo& Info : TransferCases)
		{
			if (Info.Outcome != EGeneratedCaseOutcome::CompileReject)
			{
				continue;
			}
			++TransferIndependentRejects;
			ASSERT_THAT(IsTrue(Info.EntryDeclaration.IsEmpty(), TEXT("TransferValidity reject EntryDeclaration")));
			ASSERT_THAT(IsTrue(TransferRejectIds.Contains(Info.CaseId), TEXT("TransferValidity reject source id")));
			ASSERT_THAT(IsTrue(
				TransferValidity->BuildCaseSource(Info.CaseId).Len() > 0,
				TEXT("TransferValidity independent reject source")));
		}
		ASSERT_THAT(AreEqual(5, TransferIndependentRejects, TEXT("TransferValidity five independent rejects")));
		ASSERT_THAT(AreEqual(6, RegisteredFuncdef->ListRejectCaseIds().Num(), TEXT("RegisteredFuncdef ListRejectCaseIds")));

		TArray<FAutomationTestInfo> RegisteredTests;
		FAutomationTestFramework::Get().GetValidTestNames(RegisteredTests);
		TSet<FString> ExportMethods;
		int32 CorpusMethods = 0;
		for (const FAutomationTestInfo& Info : RegisteredTests)
		{
			const FString Path = Info.GetFullTestPath();
			if (Path.EndsWith(TEXT(".GeneratesAndExportsAllCases")))
			{
				ExportMethods.Add(Path);
			}
			if (Path.Contains(TEXT("LanguageGeneratorCorpus.VerifiesCompleteCorpus")))
			{
				++CorpusMethods;
			}
		}
		ASSERT_THAT(AreEqual(
			122,
			ExportMethods.Num(),
			TEXT("exactly one GeneratesAndExportsAllCases per generator")));
		ASSERT_THAT(AreEqual(1, CorpusMethods, TEXT("exactly one VerifiesCompleteCorpus")));
		for (const FInventoryRow& Row : Inventory)
		{
			const FString ExpectedPath = ExpectedExportPath(Row);
			ASSERT_THAT(IsTrue(
				ExportMethods.Contains(ExpectedPath),
				*ProductDetail(Row.ProductId, TEXT("registered GeneratesAndExportsAllCases"))));
		}

		ASSERT_THAT(IsTrue(Consumer.Passed > 0, TEXT("fake consumer recorded passed")));
		ASSERT_THAT(IsTrue(Consumer.Skipped > 0, TEXT("fake consumer recorded skipped")));
		ASSERT_THAT(IsTrue(Consumer.Unsupported > 0, TEXT("fake consumer recorded unsupported")));
		ASSERT_THAT(IsTrue(Consumer.CompileRejectRouted > 0, TEXT("fake consumer recorded compile rejects")));
		ASSERT_THAT(IsTrue(Consumer.ExceptionChecked > 0, TEXT("fake consumer recorded exceptions")));
		ASSERT_THAT(AreEqual(
			36686,
			Consumer.Passed + Consumer.Skipped + Consumer.Unsupported
				+ Consumer.ExceptionChecked + Consumer.CompileRejectRouted,
			TEXT("fake consumer dispositions cover every listed cell")));

		UE_LOG(
			LogTemp,
			Display,
			TEXT("LanguageGeneratorCorpus products=%d cells=%d passed=%d skipped=%d unsupported=%d exceptions=%d rejects=%d zero=%s div0=%s"),
			DistinctProductIds.Num(),
			DistinctCaseIds.Num(),
			Consumer.Passed,
			Consumer.Skipped,
			Consumer.Unsupported,
			Consumer.ExceptionChecked,
			Consumer.CompileRejectRouted,
			*NormalZeroCaseId,
			*DivideByZeroCaseId);
	}
};

#endif
