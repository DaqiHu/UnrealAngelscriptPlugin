#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/StaticJITHeader.h"

#include <array>
#include <type_traits>

/**
 * Immutable generated metadata for one provider-private Bind call.
 *
 * ReferenceSlotIndex is the only executable identity. The strings and stable
 * hashes make generated code and diagnostics understandable, while reference
 * table adoption has already validated the stable key and ABI against the
 * current Engine.
 */
struct FAngelscriptTypedASTJITBoundCallSite
{
	const TCHAR* CanonicalDeclaration = TEXT("");
	const TCHAR* RegisteredTarget = TEXT("");
	FAngelscriptStableFunctionKey StableTargetKey;
	FAngelscriptHash256 ExpectedABI;
	int32 ReferenceSlotIndex = INDEX_NONE;
};

enum class EAngelscriptTypedASTJITScalarKind : uint8
{
	Invalid = 0,
	Void,
	Bool,
	Int8,
	UInt8,
	Int16,
	UInt16,
	Int32,
	UInt32,
	Int64,
	UInt64,
	Float,
	Double,
};

/** Fixed cross-DLL scalar carrier. Payload owns the exact source bits. */
struct FAngelscriptTypedASTJITScalarValue
{
	EAngelscriptTypedASTJITScalarKind Kind =
		EAngelscriptTypedASTJITScalarKind::Invalid;
	uint64 Payload = 0;
};

enum class EAngelscriptTypedASTJITBridgeResult : uint8
{
	Success = 0,
	ExistingException,
	MissingExecutionEngine,
	MissingReferenceTable,
	MissingReferenceSlot,
	WrongReferenceKind,
	MissingFunction,
	WrongEngine,
	UnsupportedFunctionKind,
	UnsupportedNativeCallingForm,
	MissingNativeAddress,
	InvalidScalarCarrier,
	SignatureMismatch,
	ContextUnavailable,
	PrepareFailed,
	SetArgumentFailed,
	ExecutionFailed,
};

namespace AngelscriptTypedASTJIT
{
	/** Stable diagnostic spelling for bridge failures and debug tooling. */
	ANGELSCRIPTRUNTIME_API const TCHAR* LexToString(
		EAngelscriptTypedASTJITBridgeResult Result);

	/**
	 * Fixed Runtime DLL boundary used by generated provider modules.
	 *
	 * It resolves only CallSite.ReferenceSlotIndex from Execution's immutable
	 * current-Engine reference table, validates the scalar signature, and then
	 * executes the registered system function through an AngelScript context.
	 */
	ANGELSCRIPTRUNTIME_API EAngelscriptTypedASTJITBridgeResult InvokeBoundViaVM(
		FScriptExecution& Execution,
		const FAngelscriptTypedASTJITBoundCallSite& CallSite,
		const FAngelscriptTypedASTJITScalarValue* Arguments,
		uint32 ArgumentCount,
		EAngelscriptTypedASTJITScalarKind ReturnKind,
		FAngelscriptTypedASTJITScalarValue* OutReturnValue);

	/**
	 * Resolves one reviewed scalar system Bind to the actual native function
	 * address registered in the current Engine slot. No name lookup or
	 * AngelScript context execution occurs on this route.
	 */
	ANGELSCRIPTRUNTIME_API EAngelscriptTypedASTJITBridgeResult ResolveBoundNative(
		FScriptExecution& Execution,
		const FAngelscriptTypedASTJITBoundCallSite& CallSite,
		const EAngelscriptTypedASTJITScalarKind* ArgumentKinds,
		uint32 ArgumentCount,
		EAngelscriptTypedASTJITScalarKind ReturnKind,
		asFUNCTION_t& OutFunction);

	namespace Private
	{
		template <typename ValueType>
		inline constexpr bool TIsSupportedScalar =
			std::is_same_v<ValueType, bool>
			|| std::is_same_v<ValueType, int8>
			|| std::is_same_v<ValueType, uint8>
			|| std::is_same_v<ValueType, int16>
			|| std::is_same_v<ValueType, uint16>
			|| std::is_same_v<ValueType, int32>
			|| std::is_same_v<ValueType, uint32>
			|| std::is_same_v<ValueType, int64>
			|| std::is_same_v<ValueType, uint64>
			|| std::is_same_v<ValueType, float>
			|| std::is_same_v<ValueType, double>;

		template <typename ValueType>
		constexpr EAngelscriptTypedASTJITScalarKind GetScalarKind()
		{
			using T = std::remove_cv_t<std::remove_reference_t<ValueType>>;
			static_assert(TIsSupportedScalar<T>,
				"TypedASTJIT VM bridge supports only reviewed scalar ABI types");
			if constexpr (std::is_same_v<T, bool>)
				return EAngelscriptTypedASTJITScalarKind::Bool;
			else if constexpr (std::is_same_v<T, int8>)
				return EAngelscriptTypedASTJITScalarKind::Int8;
			else if constexpr (std::is_same_v<T, uint8>)
				return EAngelscriptTypedASTJITScalarKind::UInt8;
			else if constexpr (std::is_same_v<T, int16>)
				return EAngelscriptTypedASTJITScalarKind::Int16;
			else if constexpr (std::is_same_v<T, uint16>)
				return EAngelscriptTypedASTJITScalarKind::UInt16;
			else if constexpr (std::is_same_v<T, int32>)
				return EAngelscriptTypedASTJITScalarKind::Int32;
			else if constexpr (std::is_same_v<T, uint32>)
				return EAngelscriptTypedASTJITScalarKind::UInt32;
			else if constexpr (std::is_same_v<T, int64>)
				return EAngelscriptTypedASTJITScalarKind::Int64;
			else if constexpr (std::is_same_v<T, uint64>)
				return EAngelscriptTypedASTJITScalarKind::UInt64;
			else if constexpr (std::is_same_v<T, float>)
				return EAngelscriptTypedASTJITScalarKind::Float;
			else
				return EAngelscriptTypedASTJITScalarKind::Double;
		}

		template <typename ValueType>
		FAngelscriptTypedASTJITScalarValue PackScalar(const ValueType Value)
		{
			using T = std::remove_cv_t<std::remove_reference_t<ValueType>>;
			static_assert(TIsSupportedScalar<T>,
				"TypedASTJIT VM bridge supports only reviewed scalar ABI types");
			static_assert(sizeof(T) <= sizeof(uint64));
			FAngelscriptTypedASTJITScalarValue Packed;
			Packed.Kind = GetScalarKind<T>();
			const T Copy = static_cast<T>(Value);
			FMemory::Memcpy(&Packed.Payload, &Copy, sizeof(T));
			return Packed;
		}

		template <typename ValueType>
		ValueType UnpackScalar(
			const FAngelscriptTypedASTJITScalarValue& Packed)
		{
			using T = std::remove_cv_t<std::remove_reference_t<ValueType>>;
			static_assert(TIsSupportedScalar<T>,
				"TypedASTJIT VM bridge supports only reviewed scalar ABI types");
			T Value{};
			FMemory::Memcpy(&Value, &Packed.Payload, sizeof(T));
			return static_cast<ValueType>(Value);
		}
	}

	/**
	 * Header-only typed facade. Templates describe marshalling shape only; they
	 * never resolve names, stable keys, Engine functions, or private pointers.
	 */
	template <typename ReturnType, typename... ArgumentTypes>
	ReturnType InvokeBound(
		FScriptExecution& Execution,
		const FAngelscriptTypedASTJITBoundCallSite& CallSite,
		ArgumentTypes... Arguments)
	{
		const std::array<FAngelscriptTypedASTJITScalarValue,
			sizeof...(ArgumentTypes)> PackedArguments = {
			Private::PackScalar<ArgumentTypes>(Arguments)...
		};

		if constexpr (std::is_void_v<ReturnType>)
		{
			InvokeBoundViaVM(
				Execution,
				CallSite,
				PackedArguments.data(),
				static_cast<uint32>(PackedArguments.size()),
				EAngelscriptTypedASTJITScalarKind::Void,
				nullptr);
			return;
		}
		else
		{
			using T = std::remove_cv_t<std::remove_reference_t<ReturnType>>;
			static_assert(Private::TIsSupportedScalar<T>,
				"TypedASTJIT VM bridge supports only reviewed scalar ABI returns");
			FAngelscriptTypedASTJITScalarValue PackedReturn;
			const EAngelscriptTypedASTJITBridgeResult Result = InvokeBoundViaVM(
				Execution,
				CallSite,
				PackedArguments.data(),
				static_cast<uint32>(PackedArguments.size()),
				Private::GetScalarKind<T>(),
				&PackedReturn);
			return Result == EAngelscriptTypedASTJITBridgeResult::Success
				? Private::UnpackScalar<ReturnType>(PackedReturn)
				: ReturnType{};
		}
	}

	/**
	 * Direct native C++ call through the current Engine's adopted Bind slot.
	 * The only extra operation versus a literal symbol call is loading the
	 * current function address; arguments and return value use the native ABI.
	 */
	template <typename ReturnType, typename... ArgumentTypes>
	ReturnType InvokeBoundNative(
		FScriptExecution& Execution,
		const FAngelscriptTypedASTJITBoundCallSite& CallSite,
		ArgumentTypes... Arguments)
	{
		static_assert((Private::TIsSupportedScalar<
			std::remove_cv_t<std::remove_reference_t<ArgumentTypes>>> && ...),
			"TypedASTJIT native calls support only reviewed scalar ABI arguments");
		if constexpr (!std::is_void_v<ReturnType>)
		{
			using T = std::remove_cv_t<std::remove_reference_t<ReturnType>>;
			static_assert(Private::TIsSupportedScalar<T>,
				"TypedASTJIT native calls support only reviewed scalar ABI returns");
		}

		const std::array<EAngelscriptTypedASTJITScalarKind,
			sizeof...(ArgumentTypes)> ArgumentKinds = {
			Private::GetScalarKind<ArgumentTypes>()...
		};
		constexpr EAngelscriptTypedASTJITScalarKind ReturnKind = []
		{
			if constexpr (std::is_void_v<ReturnType>)
			{
				return EAngelscriptTypedASTJITScalarKind::Void;
			}
			else
			{
				return Private::GetScalarKind<ReturnType>();
			}
		}();
		asFUNCTION_t RawFunction = nullptr;
		const EAngelscriptTypedASTJITBridgeResult Result = ResolveBoundNative(
			Execution,
			CallSite,
			ArgumentKinds.data(),
			static_cast<uint32>(ArgumentKinds.size()),
			ReturnKind,
			RawFunction);
		if (Result != EAngelscriptTypedASTJITBridgeResult::Success)
		{
			if constexpr (std::is_void_v<ReturnType>)
			{
				return;
			}
			else
			{
				return ReturnType{};
			}
		}

		using FNativeFunction = ReturnType(*)(ArgumentTypes...);
		FNativeFunction NativeFunction =
			reinterpret_cast<FNativeFunction>(RawFunction);
		if constexpr (std::is_void_v<ReturnType>)
		{
			NativeFunction(Arguments...);
			return;
		}
		else
		{
			return NativeFunction(Arguments...);
		}
	}
}
