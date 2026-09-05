#include "frontend/as_declaration_fragment.h"

BEGIN_AS_NAMESPACE

// Declaration fragments intentionally remain a small value type. Keeping an
	// implementation unit makes the collection boundary independently linkable as
	// later fragment adoption and parallel collection are introduced.

END_AS_NAMESPACE
