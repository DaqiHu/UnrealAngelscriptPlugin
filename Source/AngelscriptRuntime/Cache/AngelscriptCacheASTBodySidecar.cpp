#include "Cache/AngelscriptCacheASTBodySidecar.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_sidecar.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	EAngelscriptCacheASTBodySidecarError MapError(const int Status)
	{
		switch (Status)
		{
		case asAST_SIDECAR_OK:
			return EAngelscriptCacheASTBodySidecarError::Ok;
		case asAST_SIDECAR_EMPTY_NOT_ABSENCE:
			return EAngelscriptCacheASTBodySidecarError::EmptyPayloadNotAbsence;
		case asAST_SIDECAR_UNKNOWN_KIND:
			return EAngelscriptCacheASTBodySidecarError::UnknownNodeOrTypeKind;
		case asAST_SIDECAR_FUNCTION_KEY_MISMATCH:
			return EAngelscriptCacheASTBodySidecarError::FunctionKeyMismatch;
		case asAST_SIDECAR_PROFILE_MISMATCH:
			return EAngelscriptCacheASTBodySidecarError::ProfileMismatch;
		case asAST_SIDECAR_BUDGET:
			return EAngelscriptCacheASTBodySidecarError::Budget;
		case asAST_SIDECAR_POINTER:
			return EAngelscriptCacheASTBodySidecarError::PointerOrNumericId;
		default:
			return EAngelscriptCacheASTBodySidecarError::Malformed;
		}
	}

	void CopyToForkString(asCString& Out, const FString& In)
	{
		const auto Utf8 = StringCast<UTF8CHAR>(*In);
		Out = reinterpret_cast<const char*>(Utf8.Get());
	}
}

FAngelscriptCacheASTBodySidecarResult AngelscriptEncodeASTBodySidecar(
	const FAngelscriptCacheASTBodySidecar& Sidecar,
	TArray<uint8>& OutBytes)
{
	OutBytes.Reset();
	if (Sidecar.CanonicalAstBytes.Num() == 0)
	{
		return {EAngelscriptCacheASTBodySidecarError::EmptyPayloadNotAbsence};
	}

	// CanonicalAstBytes is already the complete, pointer-free Canonical AST DTO. This
	// wrapper is deliberately a validation/persistence boundary, not a second
	// serializer: rebuilding an empty translation unit here used to silently
	// discard the caller's function body and accept arbitrary placeholder bytes.
	asCASTContext Context;
	asSASTSidecarHeader Header;
	CopyToForkString(Header.functionKey, Sidecar.FunctionKey);
	CopyToForkString(Header.profile, Sidecar.Profile);
	Header.version = FAngelscriptCacheASTBodySidecar::SchemaVersion;
	const int Status = asCASTDecodeSidecar(
		Context,
		Header,
		Sidecar.CanonicalAstBytes.GetData(),
		static_cast<asUINT>(Sidecar.CanonicalAstBytes.Num()));
	if (Status != asAST_SIDECAR_OK)
	{
		return {MapError(Status)};
	}

	OutBytes = Sidecar.CanonicalAstBytes;
	return {};
}

FAngelscriptCacheASTBodySidecarResult AngelscriptDecodeASTBodySidecar(
	const TConstArrayView<uint8> Bytes,
	const FString& ExpectedFunctionKey,
	const FString& ExpectedProfile,
	FAngelscriptCacheASTBodySidecar& OutSidecar)
{
	OutSidecar = FAngelscriptCacheASTBodySidecar{};
	if (Bytes.Num() == 0)
	{
		return {EAngelscriptCacheASTBodySidecarError::EmptyPayloadNotAbsence};
	}

	asCASTContext Context;
	asSASTSidecarHeader Expected;
	CopyToForkString(Expected.functionKey, ExpectedFunctionKey);
	CopyToForkString(Expected.profile, ExpectedProfile);
	Expected.version = FAngelscriptCacheASTBodySidecar::SchemaVersion;
	const int Status = asCASTDecodeSidecar(Context, Expected, Bytes.GetData(), static_cast<asUINT>(Bytes.Num()));
	if (Status != asAST_SIDECAR_OK)
	{
		return {MapError(Status)};
	}

	OutSidecar.FunctionKey = ExpectedFunctionKey;
	OutSidecar.Profile = ExpectedProfile;
	OutSidecar.CanonicalAstBytes.Append(Bytes.GetData(), Bytes.Num());
	return {};
}

FAngelscriptCacheASTBodySidecarResult AngelscriptInspectASTBodySidecar(
	const TConstArrayView<uint8> Bytes,
	FAngelscriptCacheASTBodySidecar& OutSidecar)
{
	OutSidecar = FAngelscriptCacheASTBodySidecar{};
	if (Bytes.Num() == 0)
	{
		return {EAngelscriptCacheASTBodySidecarError::EmptyPayloadNotAbsence};
	}

	asSASTSidecarHeader Header;
	const int HeaderStatus = asCASTReadSidecarHeader(
		Bytes.GetData(), static_cast<asUINT>(Bytes.Num()), Header);
	if (HeaderStatus != asAST_SIDECAR_OK)
	{
		return {MapError(HeaderStatus)};
	}

	// This is intentionally not a wildcard decode. The record hash, kind,
	// bounded payload length, and self-describing header have already been
	// validated for Cache V2 graph admission. The full pointer-free AST DTO is
	// decoded only when a consumer explicitly asks to retain a snapshot, where
	// Restore supplies the selected module/profile as exact expectations.
	OutSidecar.FunctionKey = UTF8_TO_TCHAR(Header.functionKey.AddressOf());
	OutSidecar.Profile = UTF8_TO_TCHAR(Header.profile.AddressOf());
	return {};
}
