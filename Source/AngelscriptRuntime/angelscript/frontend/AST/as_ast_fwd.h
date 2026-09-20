/* Maintained Unreal AngelScript fork — typed frontend AST forward declarations. */
#pragma once

#include "as_config.h"

BEGIN_AS_NAMESPACE

enum class asEASTNodeFamily : uint8
	{
		Decl,
		Stmt,
		Type,
		Attr,
	};

	enum class asEDeclKind : uint16
	{
		Invalid,
		TranslationUnit,
		Namespace,
		Record,
		EnumConstant,
		Function,
		Var,
		ParmVar,
		Recovery,
		Delegate,
		Event,
		TypeAlias,
		AccessSpecifier,
	};

	enum class asECallableFlavor : uint8 { Ordinary, Dynamic };
	enum class asEAccessSpecifier : uint8 { Public, Protected, Private };
	enum class asEAccessPermissionSubject : uint8 { Named, Wildcard };
	enum class asEAccessPermissionModifier : uint8 { ReadOnly, EditDefaults, Inherited };
	enum class asEFunctionKind : uint8 { Ordinary, Constructor, Destructor, Getter, Setter, ReservedAnonymousFunction, ClassDefault, Conversion };
	enum class asEFunctionModifiers : uint16
	{
		None = 0, Final = 1, Override = 2, Property = 4, Explicit = 8, Local = 16, Mixin = 32,
		AcceptTemporary = 64, ExternalImplicitThis = 128, NoDiscard = 256, AllowDiscard = 512,
		Generated = 1024, Deprecated = 2048, Defaults = 4096, UnsafeDuringConstruction = 8192,
	};
	enum class asERecordModifiers : uint8 { None = 0, Final = 1, Abstract = 2, Mixin = 4, Local = 8 };

	enum class asEStmtKind : uint16
	{
		Invalid,
		Compound,
		If,
		Return,
		While,
		Break,
		Continue,
		DeclStmt,
		RecoveryStmt,
		NullStmt,
		For,
		Do,
		IntegerLiteral = 100,
		DeclRefExpr,
		CallExpr,
		ImplicitCastExpr,
		RecoveryExpr,
		BooleanLiteral,
		FloatingLiteral,
		ParenExpr,
		UnaryOperator,
		BinaryOperator,
		ConditionalOperator,
		MemberExpr,
		ArraySubscriptExpr,
		ExplicitCastExpr,
		ConstructExpr,
		StringLiteral,
		NullPtrLiteral,
		DefaultArgumentExpr,
		ThisExpr,
		ReservedAnonymousFunction, // Retired wire ordinal; never construct.
		InitListExpr,
		ValueInitExpr,
		DeclGroupStmt,
		ClassDefaultStmt,
		Switch,
		Case,
		Default,
		Fallthrough,
		Foreach,
		UserConversionExpr,
	};

	enum class asEUserConversionMode : uint8 { Implicit, Explicit };

	enum class asEUnaryOperator : uint8
	{
		Plus, Minus, LogicalNot, BitwiseNot,
		PreIncrement, PreDecrement, PostIncrement, PostDecrement,
	};

	enum class asEBinaryOperator : uint8
	{
		Add, Subtract, Multiply, Divide, Modulo, Power,
		ShiftLeft, ShiftRight, ShiftRightArithmetic, BitwiseAnd, BitwiseXor, BitwiseOr,
		Less, Greater, LessEqual, GreaterEqual, Equal, NotEqual,
		LogicalAnd, LogicalOr, LogicalXor,
		Assign, AddAssign, SubtractAssign, MultiplyAssign, DivideAssign, ModuloAssign, PowerAssign,
		ShiftLeftAssign, ShiftRightAssign, ShiftRightArithmeticAssign, BitwiseAndAssign, BitwiseXorAssign, BitwiseOrAssign,
	};

	enum class asEValueCategory : uint8
	{
		Invalid,
		LValue,
		PRValue,
	};

	enum class asEImplicitConversionKind : uint8
	{
		None,
		Numeric,
		LValueToRValue,
		NullToHandle,
		ReferenceBinding,
	};

	enum class asETypeKind : uint16
	{
		Invalid,
		Builtin,
		Nominal,
		Generic,
		Function,
		Error,
	};

	enum class asEAttrKind : uint16
	{
		Invalid,
		Flag,
		String,
		Annotation,
	};

	class asCASTNode;
	class asCASTContext;
	struct asSASTVerificationResult;
	class asCDecl;
	class asCNamedDecl;
	class asCDeclContext;
	class asCCallableTypeDecl;
	struct asSAccessPermissionModifier;
	struct asSAuthoredAccessPermission;
	struct asSAccessSpecifierSourceInfo;
	struct asSAuthoredAccessUse;
#define AS_DECL_NODE(Class, Base, Kind) class Class;
#include "frontend/AST/as_decl_nodes.def"
#undef AS_DECL_NODE
	class asCStmt;
	class asCValueStmt;
	class asCExpr;
	class asCSwitchClause;
#define AS_STMT_NODE(Class, Base, Kind) class Class;
#include "frontend/AST/as_stmt_nodes.def"
#undef AS_STMT_NODE
	class asCType;
	class asCQualType;
#define AS_TYPE_NODE(Class, Base, Kind) class Class;
#include "frontend/AST/as_type_nodes.def"
#undef AS_TYPE_NODE
	class asCTypeLoc;
	class asCTypeSourceInfo;
	class asCAttr;
#define AS_ATTR_NODE(Class, Base, Kind) class Class;
#include "frontend/AST/as_attr_nodes.def"
#undef AS_ATTR_NODE

END_AS_NAMESPACE
