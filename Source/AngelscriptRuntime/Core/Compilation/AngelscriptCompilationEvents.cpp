#include "Compilation/AngelscriptCompilationEvents.h"

#include "HAL/PlatformTLS.h"

namespace AngelscriptCompilationEvents_Private
{
	thread_local int32 SuppressionDepth = 0;

	FAngelscriptCompilationEventDelegate& GetDelegate()
	{
		static FAngelscriptCompilationEventDelegate Delegate;
		return Delegate;
	}
}

bool FAngelscriptCompilationEvents::HasListeners()
{
	return AngelscriptCompilationEvents_Private::SuppressionDepth == 0
		&& AngelscriptCompilationEvents_Private::GetDelegate().IsBound();
}

FDelegateHandle FAngelscriptCompilationEvents::RegisterListener(TFunction<void(const FAngelscriptCompilationEvent&)> Listener)
{
	return AngelscriptCompilationEvents_Private::GetDelegate().AddLambda(MoveTemp(Listener));
}

void FAngelscriptCompilationEvents::UnregisterListener(FDelegateHandle Handle)
{
	if (Handle.IsValid())
	{
		AngelscriptCompilationEvents_Private::GetDelegate().Remove(Handle);
	}
}

void FAngelscriptCompilationEvents::Broadcast(const FAngelscriptCompilationEvent& Event)
{
	if (AngelscriptCompilationEvents_Private::SuppressionDepth != 0)
	{
		return;
	}

	FAngelscriptCompilationEventDelegate& Delegate = AngelscriptCompilationEvents_Private::GetDelegate();
	if (Delegate.IsBound())
	{
		FAngelscriptCompilationEvent EventCopy = Event;
		EventCopy.ThreadId = FPlatformTLS::GetCurrentThreadId();
		EventCopy.bOnGameThread = IsInGameThread();
		Delegate.Broadcast(EventCopy);
	}
}

FAngelscriptCompilationEventSuppressionScope::
	FAngelscriptCompilationEventSuppressionScope(const bool bInSuppress)
	: bSuppress(bInSuppress)
{
	if (bSuppress)
	{
		++AngelscriptCompilationEvents_Private::SuppressionDepth;
	}
}

FAngelscriptCompilationEventSuppressionScope::
	~FAngelscriptCompilationEventSuppressionScope()
{
	if (bSuppress)
	{
		check(AngelscriptCompilationEvents_Private::SuppressionDepth > 0);
		--AngelscriptCompilationEvents_Private::SuppressionDepth;
	}
}
