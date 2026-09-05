#include "Cache/AngelscriptCacheASTBodySidecar.h"

FAngelscriptCacheASTBodySidecarResult AngelscriptEncodeASTBodySidecar(
	const FAngelscriptCacheASTBodySidecar& Sidecar,
	TArray<uint8>& OutBytes)
{
	(void)Sidecar;
	OutBytes.Reset();
	FAngelscriptCacheASTBodySidecarResult Result;
	Result.Error = EAngelscriptCacheASTBodySidecarError::Malformed;
	return Result;
}

FAngelscriptCacheASTBodySidecarResult AngelscriptDecodeASTBodySidecar(
	const TConstArrayView<uint8> Bytes,
	const FString& ExpectedFunctionKey,
	const FString& ExpectedProfile,
	FAngelscriptCacheASTBodySidecar& OutSidecar)
{
	(void)Bytes;
	(void)ExpectedFunctionKey;
	(void)ExpectedProfile;
	OutSidecar = {};
	FAngelscriptCacheASTBodySidecarResult Result;
	Result.Error = EAngelscriptCacheASTBodySidecarError::Malformed;
	return Result;
}

FAngelscriptCacheASTBodySidecarResult AngelscriptInspectASTBodySidecar(
	const TConstArrayView<uint8> Bytes,
	FAngelscriptCacheASTBodySidecar& OutSidecar)
{
	(void)Bytes;
	OutSidecar = {};
	FAngelscriptCacheASTBodySidecarResult Result;
	Result.Error = EAngelscriptCacheASTBodySidecarError::Malformed;
	return Result;
}
