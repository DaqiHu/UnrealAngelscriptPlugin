#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#if WITH_ANGELSCRIPT_TESTS
#include "Framework/Catalog/AngelscriptTestCodeRegistration.h"
#endif

class FAngelscriptTestModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
#if WITH_ANGELSCRIPT_TESTS
	void ActivateTestCode();
	TUniquePtr<AngelscriptTest::FAngelscriptTestCodeActivationBarrier> TestCodeActivationBarrier;
#endif
};
