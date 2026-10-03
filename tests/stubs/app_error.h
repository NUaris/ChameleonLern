#include "fds.h"
#include <assert.h>
#define APP_ERROR_HANDLER(error) assert(!(error))
#define APP_ERROR_CHECK(error) assert(!(error))
#define APP_ERROR_CHECK_BOOL(value) assert(value)
