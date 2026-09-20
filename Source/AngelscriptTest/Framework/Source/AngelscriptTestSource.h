#pragma once

#include "Framework/Source/AngelscriptTestAnnotations.h"
#include "Framework/Source/AngelscriptTestSourceOriginMap.h"

#include "Containers/ArrayView.h"
#include "Templates/SharedPointer.h"

namespace AngelscriptTest
{
class FAngelscriptTestSourceParser;

class ANGELSCRIPTTEST_API FAngelscriptTestSource
{
public:
	static FAngelscriptTestSource FromBytes(TConstArrayView<uint8> Bytes);

	TConstArrayView<uint8> GetBytes() const &;
	TConstArrayView<uint8> GetBytes() const && = delete;
	const FAngelscriptTestAnnotations& GetAnnotations() const &;
	const FAngelscriptTestAnnotations& GetAnnotations() const && = delete;
	const FAngelscriptTestSourceOriginMap& GetOriginMap() const &;
	const FAngelscriptTestSourceOriginMap& GetOriginMap() const && = delete;

	static FAngelscriptTestSource FromLiteral(
		TConstArrayView<uint8> Bytes,
		const ANSICHAR* HostFile,
		int32 HostLine,
		bool bNormalize);

private:
	struct FData;
	explicit FAngelscriptTestSource(TSharedRef<const FData, ESPMode::ThreadSafe> InData);
	static FAngelscriptTestSource FromParsedData(
		TArray<uint8> Bytes,
		FAngelscriptTestAnnotations Annotations,
		FAngelscriptTestSourceOriginMap OriginMap);

	TSharedRef<const FData, ESPMode::ThreadSafe> Data;

	friend class FAngelscriptTestAnnotations;
	friend class FAngelscriptTestSourceParser;
	friend class FAngelscriptTestCodeBuilder;
};
}

#define AS_TEST_SOURCE(Literal) \
	::AngelscriptTest::FAngelscriptTestSource::FromLiteral( \
		MakeArrayView(reinterpret_cast<const uint8*>(Literal), static_cast<int32>(sizeof(Literal) - 1)), \
		__builtin_FILE(), __builtin_LINE(), true)

#define AS_TEST_SOURCE_EXACT(Literal) \
	::AngelscriptTest::FAngelscriptTestSource::FromLiteral( \
		MakeArrayView(reinterpret_cast<const uint8*>(Literal), static_cast<int32>(sizeof(Literal) - 1)), \
		__builtin_FILE(), __builtin_LINE(), false)
