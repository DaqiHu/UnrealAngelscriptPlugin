#pragma once
#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptTypeBindInfoApply.h"
#include "as_scriptengine.h"
#include "as_context.h"

namespace RuntimeBindingTestSupport
{
	struct FPair
	{
		int32 X = 3, Y = 4;
		int32 Sum() const { return X + Y; }
	};
	inline int32 Add(int32 A, int32 B) { return A + B; }
	inline TSharedRef<FAngelscriptTypeBindInfoStore> RecordFixture(bool bMissingTarget = false, bool bSeal = true)
	{
		auto Store = MakeShared<FAngelscriptTypeBindInfoStore>();
		Store->SetSource({TEXT("FixtureModule"), TEXT("FixtureProvider"), TEXT("Fixture/BindingEngine.cpp"), 42});
		FAngelscriptBinds Binds(*Store); FBindFlags Flags; Flags.bPOD = true;
		auto Pair = Binds.ValueClassForTarget<FPair>("Pair", Flags); Pair.Method("int Sum() const", &FPair::Sum);
		Binds.BindGlobalFunctionForTarget("int Add(int A, int B)", &Add);
		if (bMissingTarget) Binds.BindGlobalFunctionDirectForTarget("void Missing()", asSFuncPtr(), asCALL_CDECL, ASAutoCaller::FunctionCaller::Make());
		FString Diagnostic; if (bSeal) check(Store->Seal(Diagnostic)); return Store;
	}
	inline AS_NAMESPACE_QUALIFIER asCScriptFunction* Function(const FAngelscriptEngine& Engine, const TCHAR* Declaration)
	{
		const auto* Installed = Engine.GetBindingInstallation();
		if (!Installed) return nullptr;
		for (const auto& Member : Installed->GetMembers())
			if (Installed->GetStore().GetMember(Member.Record)->Declaration.Equals(Declaration, ESearchCase::CaseSensitive)) return Member.Function;
		return nullptr;
	}
	constexpr asPWORD CleanupKey = 0x42544E47;
	struct FCleanupCounts { int32 Contexts = 0; int32 Engines = 0; };
	inline void ContextDestroyed(asIScriptContext* Context)
	{
		++static_cast<FCleanupCounts*>(Context->GetUserData(CleanupKey))->Contexts;
	}
	inline void EngineDestroyed(asIScriptEngine* Engine)
	{
		++static_cast<FCleanupCounts*>(Engine->GetUserData(CleanupKey))->Engines;
	}
	inline void ObserveCleanup(FAngelscriptEngine& Owner, FCleanupCounts& Counts)
	{
		Owner.Engine->SetContextUserDataCleanupCallback(&ContextDestroyed, CleanupKey);
		Owner.Engine->SetEngineUserDataCleanupCallback(&EngineDestroyed, CleanupKey);
		Owner.Engine->SetUserData(&Counts, CleanupKey);
	}
}
