#pragma once

#include <span>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon::Internal
{
    /// The width of the widest of `items` set in `spec`, for controls that size themselves to their longest
    /// item. Measuring means finding every item's shaped line; the result is remembered under `id` with a hash
    /// of the items' text and the font, so the items are measured again only when one of them changes.
    float MeasureWidestItem(ID id, std::span<const std::string_view> items, const TextSpec& spec);
} // namespace Carbon::Internal
