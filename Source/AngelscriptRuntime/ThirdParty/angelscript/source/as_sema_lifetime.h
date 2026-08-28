#ifndef AS_SEMA_LIFETIME_H
#define AS_SEMA_LIFETIME_H

#include "as_ast_lifetime.h"
#include "as_sema.h"

BEGIN_AS_NAMESPACE

// Dedicated Sema authoring gate. Context keeps the storage API available for
// DTO reconstruction and forged-protocol tests, but semantic selection stays
// in Sema and is never delegated to a backend.
ANGELSCRIPTRUNTIME_API int asSemaAuthorLifetimeRecord(
	asCASTContext& context,
	const asSASTLifetimeRecord& record);

END_AS_NAMESPACE

#endif
