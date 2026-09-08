#pragma once

#include "Host/AngelscriptStandaloneDiagnosticSink.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace AngelscriptStandalone
{
	struct FCallStackFrame
	{
		std::string Declaration;
		std::string Section;
		int Row = 0;
		int Column = 0;
	};

	struct FRunResult
	{
		int ExitCode = 2;
		std::optional<int> ScriptResult;
		std::string ModuleId;
		std::string LogicalEntryPath;
		std::string InputHash;
		std::string ProfileHash;
		std::string Error;
		std::string ExceptionMessage;
		std::string StandardOutput;
		bool bTimedOut = false;
		bool bMemoryLimitReached = false;
		std::uint64_t PeakAllocatedBytes = 0;
		std::uint64_t AllocatedBytesAfterShutdown = 0;
		std::vector<std::uint8_t> ByteCode;
		std::vector<FDiagnostic> Diagnostics;
		std::vector<FCallStackFrame> CallStack;
	};

}
