#include "Core/AngelscriptRuntimeModule.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
#include "StaticJIT/AngelscriptJITProvider.h"

#if __has_include("Generated/EditorDevelopment/Provider.generated.h")
#include "Generated/EditorDevelopment/Provider.generated.h"
#define ANGELSCRIPT_TESTJIT_HAS_GENERATED_PROVIDER 1
#else
#define ANGELSCRIPT_TESTJIT_HAS_GENERATED_PROVIDER 0
#endif

namespace
{
	const FAngelscriptJITProviderView*
	GetCurrentGeneratedAngelscriptTestJITProviderView()
	{
#if ANGELSCRIPT_TESTJIT_HAS_GENERATED_PROVIDER
		static thread_local FAngelscriptJITProviderView View;
		View = GetGeneratedAngelscriptJITProviderView();
		return &View;
#else
		return nullptr;
#endif
	}
}

class FAngelscriptTestJITModule final
	: public IModuleInterface
	, public IAngelscriptJITArtifactProvider
{
public:
	virtual void StartupModule() override
	{
		if (!FAngelscriptRuntimeModule::IsLegacyRuntimeEnabled())
		{
			return;
		}

		IModularFeatures::Get().RegisterModularFeature(
			IAngelscriptJITArtifactProvider::FeatureName(), this);
		bRegistered = true;
	}

	virtual void ShutdownModule() override
	{
		if (bRegistered)
		{
			IModularFeatures::Get().UnregisterModularFeature(
				IAngelscriptJITArtifactProvider::FeatureName(), this);
			bRegistered = false;
		}
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	virtual const FAngelscriptJITProviderView*
	GetAngelscriptJITProviderView() const override
	{
		return GetCurrentGeneratedAngelscriptTestJITProviderView();
	}

private:
	bool bRegistered = false;
};

IMPLEMENT_MODULE(FAngelscriptTestJITModule, AngelscriptTestJIT)
