#pragma once

/// Umbrella header for the CarbonExtensions component library.

#include "Carbon/Carbon.h"

namespace Carbon
{
    /// Returns the version of the Carbon library that CarbonExtensions was built against.
    Version GetExtensionsVersion();
} // namespace Carbon
