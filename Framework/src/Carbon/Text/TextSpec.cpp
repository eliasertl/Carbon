#include "Carbon/Text/TextSpec.h"

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Draw/DrawList.h"
#include "Carbon/Text/Internal/TextSystem.h"

namespace Carbon
{
    FontMetrics GetFontMetrics(const TextSpec& spec)
    {
        return Internal::GetContext().Text->GetMetrics(spec);
    }

    Vec2 MeasureText(std::string_view text, const TextSpec& spec)
    {
        return Internal::GetContext().Text->Measure(text, spec);
    }

    void GetCaretPositions(std::string_view line, const TextSpec& spec, std::vector<float>& positions)
    {
        Internal::GetContext().Text->GetCaretPositions(line, spec, positions);
    }

    // Declared in Draw/DrawList.h; implemented here because drawing text needs the text system, which sits above
    // the draw list in Carbon's layering.
    void DrawList::AddText(Vec2 position, std::string_view text, const TextSpec& spec, Color color)
    {
        Internal::GetContext().Text->Draw(*this, position, text, spec, color);
    }
} // namespace Carbon
