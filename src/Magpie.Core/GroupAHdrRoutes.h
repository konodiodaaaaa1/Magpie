#pragma once

#include "HdrProtocol.h"

#include <string_view>

namespace Magpie {

// Effect-local protocol description for group A. The auxiliary text is kept
// alongside the structured route so diagnostics can expose intermediate
// resource boundaries without teaching the shared HDR dispatcher effect names.
struct GroupAHdrEffectDescription {
    HdrFormatRoutes routes;
    std::string_view auxiliaryResources;
    std::string_view evidence;
};

GroupAHdrEffectDescription GetGroupAHdrEffectDescription(
    std::string_view effectName,
    int casFormatOption = 0
);

HdrFormatRoutes GetGroupAHdrRoutes(std::string_view effectName, int casFormatOption = 0);

}
