#include "Carbon/Extensions/ScrubField.h"

#include "Carbon/Extensions/Internal/NumberEditing.h"

namespace Carbon
{
    namespace
    {
        Internal::NumberEditOptions ToEditOptions(const ScrubFieldOptions& options)
        {
            Internal::NumberEditOptions edit;
            edit.Min = options.Min;
            edit.Max = options.Max;
            edit.Step = options.Step;
            edit.Format = options.Format;
            edit.Width = options.Width;
            edit.ControlSize = options.ControlSize;
            edit.Disabled = options.Disabled;
            edit.Scrubs = true;
            return edit;
        }
    } // namespace

    bool ScrubField(std::string_view label, int* value, const ScrubFieldOptions& options)
    {
        return Internal::EditNumberValue(label, value, Internal::NumberKind::Int, ToEditOptions(options));
    }

    bool ScrubField(std::string_view label, float* value, const ScrubFieldOptions& options)
    {
        return Internal::EditNumberValue(label, value, Internal::NumberKind::Float, ToEditOptions(options));
    }

    bool ScrubField(std::string_view label, double* value, const ScrubFieldOptions& options)
    {
        return Internal::EditNumberValue(label, value, Internal::NumberKind::Double, ToEditOptions(options));
    }
} // namespace Carbon
