#include "frontend/Basic/as_language_surface.h"

BEGIN_AS_NAMESPACE

const TCHAR* asRemovedLanguageFeatureName(asERemovedLanguageFeature Feature)
{
	switch (Feature)
	{
	case asERemovedLanguageFeature::ModuleSharing: return TEXT("ModuleSharing");
	case asERemovedLanguageFeature::FuncdefDeclaration: return TEXT("FuncdefDeclaration");
	case asERemovedLanguageFeature::AnonymousFunction: return TEXT("AnonymousFunction");
	case asERemovedLanguageFeature::ScriptException: return TEXT("ScriptException");
	case asERemovedLanguageFeature::ScriptCoroutine: return TEXT("ScriptCoroutine");
	case asERemovedLanguageFeature::UserTemplate: return TEXT("UserTemplate");
	case asERemovedLanguageFeature::VirtualProperty: return TEXT("VirtualProperty");
	}
	checkNoEntry();
	return TEXT("UnknownRemovedFeature");
}

END_AS_NAMESPACE
