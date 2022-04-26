#include "version.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

std::string_view schengine_version()
{
    return TOSTRING(SCHENGINE_VERSION);
}