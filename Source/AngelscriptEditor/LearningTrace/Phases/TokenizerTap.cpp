// Copyright Epic Games, Inc. All Rights Reserved.

#include "Phases/TokenizerTap.h"

#include "Core/LearningTraceEventStream.h"
#include "Core/LearningTraceExample.h"

namespace AngelscriptEditor::LearningTrace
{
	FTokenizerTap::FTokenizerTap(const asCScriptEngine* InEngine)
	{
		(void)InEngine;
	}

	bool FTokenizerTap::Run(
		const FLearningTraceExample& Example,
		FLearningTraceEventStream& OutStream,
		FString& OutErrorMessage)
	{
		(void)Example;
		(void)OutStream;
		OutErrorMessage = TEXT("Legacy tokenizer tap is isolated from the active compile graph.");
		return false;
	}
}
