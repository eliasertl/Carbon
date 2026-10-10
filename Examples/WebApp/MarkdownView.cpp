#include "MarkdownView.h"

#include <algorithm>
#include <cmath>

#include "DocLibrary.h"

namespace WebApp
{
    using namespace Carbon;

    namespace
    {
        // The reading column: at most this wide, centered in the view; code lines of 120 characters fit in it.
        constexpr float MaxContentWidth = 900.0f;
        // Body text is a little larger than the macOS default of 13 points, for long reading.
        constexpr float BodySize = 14.0f;
        constexpr float BodyLineHeight = 22.0f;
        // Code in running text, relative to the text around it.
        constexpr float InlineCodeScale = 0.88f;
        constexpr float InlineCodePadding = 3.0f;
        constexpr float InlineCodeRadius = 4.0f;
        constexpr float CodeSize = 12.0f;
        constexpr float CodeLineHeight = 19.0f;
        constexpr float CodePadding = 14.0f;
        constexpr float CodeRadius = 8.0f;
        constexpr float TableSize = 13.0f;
        constexpr float TableLineHeight = 19.0f;
        constexpr float CellPadding = 10.0f;
        constexpr float CellVerticalPadding = 6.0f;
        constexpr float RowRadius = 6.0f;
        constexpr float ListIndent = 26.0f;
        constexpr float QuoteIndent = 18.0f;
        constexpr float QuoteBarWidth = 3.0f;
        // The documentation's images are rendered at content scale 2: two pixels per point.
        constexpr float ImageScale = 0.5f;
        // The narrowest a table column with an image gets.
        constexpr float MinImageWidth = 120.0f;
        constexpr float ParagraphSpacing = 14.0f;
        constexpr float ListItemSpacing = 6.0f;

        struct HeadingStyle
        {
            float Size;
            float LineHeight;
            FontWeight Weight;
            float SpaceAbove;
            float SpaceBelow;
            /// A hairline under the heading, as on GitHub, for the two top levels.
            bool HasRule;
        };

        constexpr HeadingStyle HeadingStyles[] = {
            {30.0f, 38.0f, FontWeight::Bold, 36.0f, 16.0f, true},
            {22.0f, 30.0f, FontWeight::Bold, 34.0f, 14.0f, true},
            {17.0f, 24.0f, FontWeight::Semibold, 26.0f, 8.0f, false},
            {15.0f, 22.0f, FontWeight::Semibold, 22.0f, 6.0f, false},
            {14.0f, 22.0f, FontWeight::Semibold, 20.0f, 6.0f, false},
            {14.0f, 22.0f, FontWeight::Semibold, 20.0f, 6.0f, false},
        };

        const HeadingStyle& GetHeadingStyle(int level)
        {
            return HeadingStyles[std::clamp(level, 1, 6) - 1];
        }

        bool IsSameSpec(const TextSpec& a, const TextSpec& b)
        {
            return a.Font == b.Font && a.Size == b.Size && a.Weight == b.Weight && a.Italic == b.Italic &&
                   a.LineHeight == b.LineHeight;
        }

        TextSpec GetBodySpec()
        {
            TextSpec spec = GetTextSpec(TextStyle::Body);
            spec.Size = BodySize;
            spec.LineHeight = BodyLineHeight;
            spec.MaxWidth = 0.0f;
            return spec;
        }

        // Where the comment of a code line starts, for the languages of the documentation's code blocks.
        size_t FindComment(std::string_view line, std::string_view language)
        {
            const bool isCpp = language == "cpp" || language == "c++" || language == "c" || language.empty();
            const bool isHash = language == "sh" || language == "cmake" || language == "bash" ||
                                language == "powershell" || language == "ps1" || language == "python";
            if (!isCpp && !isHash)
                return std::string_view::npos;
            char quote = '\0';
            for (size_t i = 0; i < line.size(); i++)
            {
                const char c = line[i];
                if (quote != '\0')
                {
                    if (c == '\\')
                        i++;
                    else if (c == quote)
                        quote = '\0';
                    continue;
                }
                if (c == '"' || (c == '\'' && isHash))
                    quote = c;
                else if (isCpp && c == '/' && i + 1 < line.size() && line[i + 1] == '/' &&
                         (i == 0 || line[i - 1] != ':'))
                    return i;
                else if (isHash && c == '#' && (i == 0 || line[i - 1] == ' '))
                    return i;
            }
            return std::string_view::npos;
        }
    } // namespace

    int MarkdownView::Show(const Document& document, std::string_view path, DocImageCache& images)
    {
        if (&document != m_Document || path != m_Path)
            Prepare(document, path, images);

        const float available = ResolveItemSize(Vec2(), {.Width = Size::Fill()}).X;
        const float width = std::floor(std::min(available, MaxContentWidth));
        if (width != m_Width)
            Layout(width);

        const Rect rect = AllocateItem(Vec2(available, m_Height), {.Width = Size::Fill()});
        m_Top = rect.Y;
        const Vec2 origin = rect.GetMin() + Vec2(std::floor((available - width) * 0.5f), 0.0f);

        DrawList& drawList = GetDrawList();
        const Rect clip = drawList.GetClipRect();
        const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);

        for (const Shape& shape : m_Shapes)
        {
            const Rect bounds(origin + shape.Bounds.GetMin(), shape.Bounds.GetSize());
            if (!bounds.Intersects(clip))
                continue;
            const Color color = Resolve(shape.Color);
            if (shape.Kind == ShapeKind::Disc)
                drawList.AddCircle(bounds.GetCenter(), shape.Radius, color);
            else if (shape.Kind == ShapeKind::Ring)
                drawList.AddCircleStroke(bounds.GetCenter(), shape.Radius, color, 1.2f);
            else if (shape.Radius > 0.0f)
                drawList.AddSquircle(bounds, color, shape.Radius, smoothing);
            else
                drawList.AddRect(bounds, color);
        }

        for (const Picture& picture : m_Pictures)
        {
            const Rect bounds(origin + picture.Bounds.GetMin(), picture.Bounds.GetSize());
            if (!bounds.Intersects(clip))
                continue;
            const TextureID texture = images.GetTexture(picture.Path);
            if (texture.Value != 0)
                drawList.AddImage(texture, bounds);
            else
                drawList.AddSquircle(bounds, Resolve(Ink::Fill), CodeRadius, smoothing);
        }

        // Links: every visible piece of one is a button; hovering any piece underlines all of them.
        const ID id = GetID("##markdown");
        int hoveredLink = -1;
        int clickedLink = -1;
        for (size_t i = 0; i < m_Runs.size(); i++)
        {
            const TextRun& run = m_Runs[i];
            const Rect bounds(origin + run.Position, Vec2(run.Width, run.Height));
            if (run.Link < 0 || !bounds.Intersects(clip))
                continue;
            const Interaction interaction =
                ButtonBehavior(HashID(static_cast<int64_t>(i), id), bounds, {.Focusable = false});
            if (interaction.Hovered)
                hoveredLink = run.Link;
            if (interaction.Clicked)
                clickedLink = run.Link;
        }
        if (hoveredLink >= 0)
            SetCursor(Cursor::PointingHand);

        const float hairline = GetContentScale().GetPixelSize();
        for (const TextRun& run : m_Runs)
        {
            const Vec2 position = origin + run.Position;
            if (!Rect(position, Vec2(run.Width, run.Height)).Expand(run.Spec.Size).Intersects(clip))
                continue;
            const Color color = Resolve(run.Color);
            drawList.AddText(position, run.Text, run.Spec, color);
            if (run.Link >= 0 && run.Link == hoveredLink)
                drawList.AddRect(
                    Rect(position.X, position.Y + run.Baseline + 2.0f, run.Width, std::max(hairline, 1.0f)), color);
        }
        return clickedLink;
    }

    std::optional<float> MarkdownView::FindAnchor(std::string_view anchor) const
    {
        for (const auto& [name, top] : m_Anchors)
        {
            if (name == anchor)
                return top;
        }
        return std::nullopt;
    }

    void MarkdownView::Prepare(const Document& document, std::string_view path, DocImageCache& images)
    {
        m_Document = &document;
        m_Path = std::string(path);
        m_Width = -1.0f;
        m_Inlines.clear();
        m_BlockInlines.clear();

        const TextSpec body = GetBodySpec();
        for (const Block& block : document.Blocks)
        {
            m_BlockInlines.push_back(m_Inlines.size());
            const Ink color = block.IsQuote ? Ink::Secondary : Ink::Label;
            switch (block.Kind)
            {
                case BlockKind::Heading:
                {
                    const HeadingStyle& style = GetHeadingStyle(block.Level);
                    TextSpec spec = body;
                    spec.Size = style.Size;
                    spec.LineHeight = style.LineHeight;
                    spec.Weight = style.Weight;
                    m_Inlines.push_back(MakeInline(block.Content, spec, Ink::Label, path, images));
                    break;
                }
                case BlockKind::Paragraph:
                case BlockKind::ListItem:
                    m_Inlines.push_back(MakeInline(block.Content, body, color, path, images));
                    break;
                case BlockKind::Table:
                {
                    TextSpec spec = body;
                    spec.Size = TableSize;
                    spec.LineHeight = TableLineHeight;
                    for (size_t row = 0; row < block.Rows.size(); row++)
                    {
                        spec.Weight = row == 0 ? FontWeight::Semibold : body.Weight;
                        for (const Spans& cell : block.Rows[row])
                            m_Inlines.push_back(MakeInline(cell, spec, color, path, images));
                    }
                    break;
                }
                case BlockKind::CodeBlock:
                case BlockKind::Rule:
                    break;
            }
        }
    }

    MarkdownView::Inline MarkdownView::MakeInline(const Spans& spans, const TextSpec& base, Ink color,
                                                  std::string_view path, DocImageCache& images)
    {
        Inline result;
        for (const Span& span : spans)
        {
            if (span.IsImage)
            {
                Atom atom;
                atom.IsImage = true;
                atom.ImagePath = ResolvePath(path, span.Target);
                const Vec2 size = images.GetSize(atom.ImagePath) * ImageScale;
                atom.Width = size.X;
                atom.LineHeight = size.Y;
                atom.Baseline = size.Y;
                atom.Link = span.Link;
                atom.HasSpaceAfter = true;
                result.Atoms.push_back(std::move(atom));
                continue;
            }

            TextSpec spec = base;
            if (span.IsCode)
            {
                spec.Font = GetMonospacedFont();
                spec.Size = std::round(base.Size * InlineCodeScale * 2.0f) * 0.5f;
            }
            if (span.IsStrong)
                spec.Weight = std::max(spec.Weight, FontWeight::Semibold);
            spec.Italic = span.IsEmphasis;
            const FontMetrics metrics = GetFontMetrics(spec);

            const std::string_view text = span.Text;
            if (!text.empty() && text.front() == ' ' && !result.Atoms.empty() && !result.Atoms.back().HasSpaceAfter)
            {
                Atom& previous = result.Atoms.back();
                previous.HasSpaceAfter = true;
                // The space is the next span's: after code, it is as wide as a space of the text that follows.
                previous.SpaceWidth = GetSpaceWidth(spec);
            }
            const size_t firstAtom = result.Atoms.size();
            size_t start = 0;
            while (start < text.size())
            {
                start = text.find_first_not_of(' ', start);
                if (start == std::string_view::npos)
                    break;
                const size_t end = std::min(text.find(' ', start), text.size());
                Atom atom;
                atom.Text = text.substr(start, end - start);
                atom.Spec = spec;
                atom.Color = span.Link >= 0 ? Ink::Link : color;
                atom.Link = span.Link;
                atom.Width = MeasureText(atom.Text, spec).X;
                atom.Baseline = metrics.Baseline;
                atom.LineHeight = metrics.LineHeight;
                atom.HasSpaceAfter = end < text.size();
                atom.SpaceWidth = atom.HasSpaceAfter ? GetSpaceWidth(spec) : 0.0f;
                result.Atoms.push_back(std::move(atom));
                start = end;
            }
            if (span.IsCode && result.Atoms.size() > firstAtom)
            {
                result.Atoms[firstAtom].StartsCode = true;
                result.Atoms.back().EndsCode = true;
            }
        }

        // The widest piece that cannot break, and the width on one line.
        float piece = 0.0f;
        for (const Atom& atom : result.Atoms)
        {
            const float padding =
                (atom.StartsCode ? InlineCodePadding : 0.0f) + (atom.EndsCode ? InlineCodePadding : 0.0f);
            // Images shrink with their column, down to a minimum.
            piece += (atom.IsImage ? std::min(atom.Width, MinImageWidth) : atom.Width) + padding;
            result.MaxWidth += atom.Width + padding + atom.SpaceWidth;
            if (atom.HasSpaceAfter)
            {
                result.MinWidth = std::max(result.MinWidth, piece);
                piece = 0.0f;
            }
        }
        result.MinWidth = std::max(result.MinWidth, piece);
        return result;
    }

    float MarkdownView::GetSpaceWidth(const TextSpec& spec)
    {
        for (const auto& [measured, width] : m_SpaceWidths)
        {
            if (IsSameSpec(measured, spec))
                return width;
        }
        // A space alone measures as nothing on some fonts' line ends; measure it between two letters.
        const float width = MeasureText("x x", spec).X - MeasureText("xx", spec).X;
        m_SpaceWidths.emplace_back(spec, width);
        return width;
    }

    void MarkdownView::Layout(float width)
    {
        m_Width = width;
        m_Runs.clear();
        m_Shapes.clear();
        m_Pictures.clear();
        m_Anchors.clear();

        const TextSpec body = GetBodySpec();
        const FontMetrics bodyMetrics = GetFontMetrics(body);
        const std::vector<Block>& blocks = m_Document->Blocks;
        float y = 0.0f;
        float quoteTop = 0.0f;
        for (size_t i = 0; i < blocks.size(); i++)
        {
            const Block& block = blocks[i];
            if (i > 0)
            {
                const Block& previous = blocks[i - 1];
                if (block.Kind == BlockKind::Heading)
                    y += GetHeadingStyle(block.Level).SpaceAbove;
                else if (previous.Kind == BlockKind::Heading)
                    y += GetHeadingStyle(previous.Level).SpaceBelow;
                else if (block.Kind == BlockKind::ListItem && previous.Kind == BlockKind::ListItem)
                    y += ListItemSpacing;
                else if (block.Indent > 0 && previous.Kind == BlockKind::ListItem)
                    y += ListItemSpacing + 2.0f;
                else
                    y += ParagraphSpacing;
            }
            if (block.IsQuote && (i == 0 || !blocks[i - 1].IsQuote))
                quoteTop = y;

            const float x = float(block.Indent) * ListIndent + (block.IsQuote ? QuoteIndent : 0.0f);
            const float available = width - x;
            const Inline* content = m_BlockInlines[i] < m_Inlines.size() ? &m_Inlines[m_BlockInlines[i]] : nullptr;
            switch (block.Kind)
            {
                case BlockKind::Heading:
                {
                    m_Anchors.emplace_back(block.Anchor, y);
                    y += LayoutInline(*content, x, y, available).Height;
                    if (GetHeadingStyle(block.Level).HasRule)
                    {
                        y += 8.0f;
                        m_Shapes.push_back({Rect(x, y, available, 1.0f), Ink::Separator});
                        y += 1.0f;
                    }
                    break;
                }
                case BlockKind::Paragraph:
                    y += LayoutInline(*content, x, y, available).Height;
                    break;
                case BlockKind::ListItem:
                {
                    const float textX = x + ListIndent;
                    const InlineLayout layout = LayoutInline(*content, textX, y, available - ListIndent);
                    const float baseline = y + (layout.Height > 0.0f ? layout.FirstBaseline : bodyMetrics.Baseline);
                    if (block.Marker.empty())
                    {
                        // Bullets alternate between discs and rings with the depth, centered on the x-height.
                        const Vec2 center(textX - 12.0f, baseline - BodySize * 0.28f);
                        const bool isRing = block.Indent % 2 == 1;
                        const float radius = isRing ? 2.6f : 2.5f;
                        m_Shapes.push_back({Rect::FromCenter(center, Vec2(radius * 2.0f)), Ink::Secondary,
                                            isRing ? ShapeKind::Ring : ShapeKind::Disc, radius});
                    }
                    else
                    {
                        TextRun run;
                        run.Text = block.Marker;
                        run.Spec = body;
                        run.Color = Ink::Secondary;
                        run.Width = MeasureText(run.Text, body).X;
                        run.Height = bodyMetrics.LineHeight;
                        run.Baseline = bodyMetrics.Baseline;
                        run.Position = Vec2(textX - 8.0f - run.Width, baseline - bodyMetrics.Baseline);
                        m_Runs.push_back(std::move(run));
                    }
                    y += std::max(layout.Height, bodyMetrics.LineHeight);
                    break;
                }
                case BlockKind::CodeBlock:
                    y += LayoutCodeBlock(block, x, y, available);
                    break;
                case BlockKind::Table:
                    y += LayoutTable(block, m_BlockInlines[i], x, y, available);
                    break;
                case BlockKind::Rule:
                    y += 10.0f;
                    m_Shapes.push_back({Rect(x, y, available, 1.0f), Ink::Separator});
                    y += 11.0f;
                    break;
            }

            // A quote's bar runs along all of its blocks.
            if (block.IsQuote && (i + 1 == blocks.size() || !blocks[i + 1].IsQuote))
                m_Shapes.push_back(
                    {Rect(0.0f, quoteTop, QuoteBarWidth, y - quoteTop), Ink::Separator, ShapeKind::Fill, 1.5f});
        }
        m_Height = y + 8.0f;
    }

    MarkdownView::InlineLayout MarkdownView::LayoutInline(const Inline& content, float x, float y, float width,
                                                          TextAlignment alignment)
    {
        struct Placed
        {
            const Atom* Item;
            float X;
            Vec2 Size;
        };
        std::vector<Placed> line;
        InlineLayout result;
        float lineTop = y;
        float cursor = 0.0f;

        auto finishLine = [&]()
        {
            if (line.empty())
                return;
            float baseline = 0.0f;
            float descent = 0.0f;
            for (const Placed& placed : line)
            {
                const float atomBaseline = placed.Item->IsImage ? placed.Size.Y : placed.Item->Baseline;
                baseline = std::max(baseline, atomBaseline);
                descent = std::max(descent, placed.Size.Y - atomBaseline);
            }
            const Placed& last = line.back();
            const float lineWidth = last.X + last.Size.X + (last.Item->EndsCode ? InlineCodePadding : 0.0f);
            const float shift = alignment == TextAlignment::Center     ? std::floor((width - lineWidth) * 0.5f)
                                : alignment == TextAlignment::Trailing ? width - lineWidth
                                                                       : 0.0f;
            const float left = x + shift;

            float codeStart = -1.0f;
            const Atom* previous = nullptr;
            for (const Placed& placed : line)
            {
                const Atom& atom = *placed.Item;
                const float top = lineTop + baseline - (atom.IsImage ? placed.Size.Y : atom.Baseline);
                if (atom.IsImage)
                {
                    m_Pictures.push_back({Rect(left + placed.X, top, placed.Size.X, placed.Size.Y), atom.ImagePath});
                    previous = nullptr;
                    continue;
                }

                // Inline code gets a rounded background around the words of its span on this line.
                if (atom.StartsCode || (codeStart < 0.0f && atom.Spec.Font == GetMonospacedFont()))
                    codeStart = placed.X - (atom.StartsCode ? InlineCodePadding : 0.0f);
                const bool isLastOnLine = &placed == &line.back();
                if (codeStart >= 0.0f && (atom.EndsCode || isLastOnLine))
                {
                    const float end = placed.X + placed.Size.X + (atom.EndsCode ? InlineCodePadding : 0.0f);
                    const float height = std::round(atom.Spec.Size + 7.0f);
                    const float center = top + atom.LineHeight * 0.5f;
                    m_Shapes.push_back(
                        {Rect(left + codeStart, std::round(center - height * 0.5f), end - codeStart, height), Ink::Fill,
                         ShapeKind::Fill, InlineCodeRadius});
                    codeStart = -1.0f;
                }

                const bool continues = previous != nullptr && IsSameSpec(previous->Spec, atom.Spec) &&
                                       previous->Color == atom.Color && previous->Link == atom.Link &&
                                       !previous->EndsCode && !atom.StartsCode;
                if (continues)
                {
                    TextRun& run = m_Runs.back();
                    if (previous->HasSpaceAfter)
                        run.Text += ' ';
                    run.Text += atom.Text;
                    run.Width = left + placed.X + placed.Size.X - run.Position.X;
                }
                else
                {
                    TextRun run;
                    run.Position = Vec2(left + placed.X, top);
                    run.Text = std::string(atom.Text);
                    run.Spec = atom.Spec;
                    run.Color = atom.Color;
                    run.Link = atom.Link;
                    run.Width = placed.Size.X;
                    run.Height = atom.LineHeight;
                    run.Baseline = atom.Baseline;
                    m_Runs.push_back(std::move(run));
                }
                previous = &atom;
            }

            if (lineTop == y)
                result.FirstBaseline = baseline;
            lineTop += baseline + descent;
            line.clear();
            cursor = 0.0f;
        };

        // Atoms are placed in pieces that cannot break: up to and including the next atom followed by a space.
        const std::vector<Atom>& atoms = content.Atoms;
        size_t i = 0;
        while (i < atoms.size())
        {
            size_t end = i;
            float pieceWidth = 0.0f;
            while (true)
            {
                const Atom& atom = atoms[end];
                const float atomWidth = atom.IsImage ? std::min(atom.Width, width) : atom.Width;
                pieceWidth += atomWidth + (atom.StartsCode ? InlineCodePadding : 0.0f) +
                              (atom.EndsCode ? InlineCodePadding : 0.0f);
                if (atom.HasSpaceAfter || end + 1 == atoms.size())
                    break;
                end++;
            }
            if (cursor > 0.0f && cursor + pieceWidth > width)
                finishLine();
            for (; i <= end; i++)
            {
                const Atom& atom = atoms[i];
                Vec2 size(atom.Width, atom.LineHeight);
                // An image wider than the column is scaled down to fit it.
                if (atom.IsImage && atom.Width > width && atom.Width > 0.0f)
                    size = Vec2(width, std::round(atom.LineHeight * width / atom.Width));
                const float placedX = cursor + (atom.StartsCode ? InlineCodePadding : 0.0f);
                line.push_back({&atom, placedX, size});
                cursor = placedX + size.X + (atom.EndsCode ? InlineCodePadding : 0.0f) + atom.SpaceWidth;
            }
        }
        finishLine();
        result.Height = lineTop - y;
        return result;
    }

    float MarkdownView::LayoutCodeBlock(const Block& block, float x, float y, float width)
    {
        TextSpec spec = GetBodySpec();
        spec.Font = GetMonospacedFont();
        spec.Size = CodeSize;
        spec.LineHeight = CodeLineHeight;
        TextSpec wrapped = spec;
        wrapped.MaxWidth = width - CodePadding * 2.0f;
        wrapped.Wraps = true;

        m_Shapes.push_back({Rect(), Ink::Fill, ShapeKind::Fill, CodeRadius});
        const size_t background = m_Shapes.size() - 1;
        const float left = x + CodePadding;
        float top = y + CodePadding - 2.0f;
        std::string_view code = block.Code;
        while (true)
        {
            const size_t end = std::min(code.find('\n'), code.size());
            const std::string_view line = code.substr(0, end);
            const float height = line.empty() ? CodeLineHeight : MeasureText(line, wrapped).Y;
            if (!line.empty())
            {
                // A line that fits shows its comment in the secondary color.
                const size_t comment = height <= CodeLineHeight ? FindComment(line, block.Language) : line.npos;
                const std::string_view text = line.substr(0, comment);
                float commentX = 0.0f;
                if (!text.empty())
                {
                    m_Runs.push_back(
                        {Vec2(left, top), std::string(text), wrapped, Ink::Label, -1, wrapped.MaxWidth, height, 0.0f});
                    // Measured up to a letter after it, so that the spaces before the comment count.
                    if (comment != line.npos)
                        commentX = MeasureText(std::string(text) + "x", spec).X - MeasureText("x", spec).X;
                }
                if (comment != line.npos)
                    m_Runs.push_back({Vec2(left + commentX, top), std::string(line.substr(comment)), spec, Ink::Comment,
                                      -1, wrapped.MaxWidth - commentX, height, 0.0f});
            }
            top += height;
            if (end >= code.size())
                break;
            code = code.substr(end + 1);
        }
        const float height = top - y + CodePadding - 2.0f;
        m_Shapes[background].Bounds = Rect(x, y, width, height);
        return height;
    }

    float MarkdownView::LayoutTable(const Block& block, size_t firstInline, float x, float y, float width)
    {
        const size_t columns = block.Alignments.size();
        if (columns == 0)
            return 0.0f;
        std::vector<float> minWidths(columns, 0.0f);
        std::vector<float> maxWidths(columns, 0.0f);
        for (size_t row = 0; row < block.Rows.size(); row++)
        {
            for (size_t column = 0; column < columns; column++)
            {
                const Inline& cell = m_Inlines[firstInline + row * columns + column];
                minWidths[column] = std::max(minWidths[column], std::ceil(cell.MinWidth) + CellPadding * 2.0f);
                maxWidths[column] = std::max(maxWidths[column], std::ceil(cell.MaxWidth) + CellPadding * 2.0f);
            }
        }

        // Columns get their width on one line when the table fits, else what they need at least and a share of
        // the rest by how much more they would like.
        float minTotal = 0.0f;
        float maxTotal = 0.0f;
        for (size_t column = 0; column < columns; column++)
        {
            minTotal += minWidths[column];
            maxTotal += maxWidths[column];
        }
        std::vector<float> widths = maxWidths;
        if (maxTotal > width)
        {
            const float extra = std::max(width - minTotal, 0.0f);
            for (size_t column = 0; column < columns; column++)
            {
                const float wish = maxWidths[column] - minWidths[column];
                widths[column] =
                    std::floor(minWidths[column] + (maxTotal > minTotal ? wish / (maxTotal - minTotal) : 0.0f) * extra);
            }
        }
        float tableWidth = 0.0f;
        for (float columnWidth : widths)
            tableWidth += columnWidth;

        float top = y;
        for (size_t row = 0; row < block.Rows.size(); row++)
        {
            // Every other body row is striped, like a macOS table; the background goes in before the cells.
            const size_t stripe = m_Shapes.size();
            if (row > 0 && row % 2 == 0)
                m_Shapes.push_back({Rect(), Ink::Stripe, ShapeKind::Fill, RowRadius});

            float cellX = x;
            float rowHeight = TableLineHeight;
            for (size_t column = 0; column < columns; column++)
            {
                const Inline& cell = m_Inlines[firstInline + row * columns + column];
                const TextAlignment alignment =
                    block.Alignments[column] == ColumnAlignment::Center     ? TextAlignment::Center
                    : block.Alignments[column] == ColumnAlignment::Trailing ? TextAlignment::Trailing
                                                                            : TextAlignment::Leading;
                const InlineLayout layout = LayoutInline(cell, cellX + CellPadding, top + CellVerticalPadding,
                                                         widths[column] - CellPadding * 2.0f, alignment);
                rowHeight = std::max(rowHeight, layout.Height);
                cellX += widths[column];
            }
            rowHeight += CellVerticalPadding * 2.0f;
            if (stripe < m_Shapes.size())
                m_Shapes[stripe].Bounds = Rect(x, top, tableWidth, rowHeight);
            top += rowHeight;
            if (row == 0)
            {
                m_Shapes.push_back({Rect(x, top - 1.0f, tableWidth, 1.0f), Ink::Separator});
                top += 2.0f;
            }
        }
        return top - y;
    }

    Color MarkdownView::Resolve(Ink ink) const
    {
        switch (ink)
        {
            case Ink::Label:
                return GetStyleColor(StyleColor::Label);
            case Ink::Secondary:
                return GetStyleColor(StyleColor::SecondaryLabel);
            case Ink::Link:
                return GetStyleColor(StyleColor::Accent);
            case Ink::Fill:
            {
                const Color label = GetStyleColor(StyleColor::Label);
                return label.WithAlpha(label.A * 0.06f);
            }
            case Ink::Stripe:
            {
                const Color label = GetStyleColor(StyleColor::Label);
                return label.WithAlpha(label.A * 0.035f);
            }
            case Ink::Separator:
                return GetStyleColor(StyleColor::Separator);
            case Ink::Comment:
                return GetStyleColor(StyleColor::SecondaryLabel);
        }
        return GetStyleColor(StyleColor::Label);
    }
} // namespace WebApp
