#include "platform_api.h"

#ifdef Q_OS_WIN
#include "win/win_platform_api.h"
#elif defined(Q_OS_LINUX)
#include "linux/linux_platform_api.h"
#endif

// Note: PlatformApi::create() is defined in the platform-specific .cpp files
// (win_platform_api.cpp or linux_platform_api.cpp) to avoid linking issues
// with conditional compilation.
