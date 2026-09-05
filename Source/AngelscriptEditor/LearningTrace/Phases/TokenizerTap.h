// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Phases/ILearningTracePhaseTap.h"

class asCScriptEngine;

namespace AngelscriptEditor::LearningTrace
{
	class FTokenizerTap : public ILearningTracePhaseTap
	{
	public:
		explicit FTokenizerTap(const asCScriptEngine* InEngine);

		virtual FString GetPhaseName() const override { return TEXT("tokenizer"); }

		virtual bool Run(
			const FLearningTraceExample& Example,
			FLearningTraceEventStream& OutStream,
			FString& OutErrorMessage) override;
	};
}
