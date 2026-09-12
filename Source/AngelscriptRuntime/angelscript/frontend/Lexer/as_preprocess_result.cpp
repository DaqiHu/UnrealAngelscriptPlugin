#include "frontend/Lexer/as_preprocess_result.h"

BEGIN_AS_NAMESPACE

void FAngelscriptPreprocessResult::RejectPublishableOutput()
	{
		State = EAngelscriptPreprocessResultState::Failed;
		Modules.Reset();
		DeclarationDependencies = {};
		BodyInvalidation = {};
	}

END_AS_NAMESPACE
