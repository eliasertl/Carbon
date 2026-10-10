#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Carbon/Extension.h>

#include "DocImageCache.h"
#include "Markdown.h"

namespace WebApp
{
    /// Draws a Markdown document with Carbon: headings, paragraphs with strong, emphasized and code text, links,
    /// lists, code blocks, tables, quotes, rules and images. The document is one item that fills the width of
    /// its container, normally a scroll view; text is measured once per document and broken into lines again
    /// only when the width changes, and only what is inside the clip rectangle is drawn.
    class MarkdownView
    {
    public:
        /// Lays out and draws `document`, whose file is `path` (relative to the image cache's root; images are
        /// relative to it). Returns the index of the link (into Document::Links) the user clicked, or -1.
        int Show(const Document& document, std::string_view path, DocImageCache& images);

        /// The top of the heading with `anchor`, in points from the top of the item, after Show; empty when the
        /// document has no such heading.
        std::optional<float> FindAnchor(std::string_view anchor) const;
        /// The top of the item in its scroll view's content, as of the last Show.
        float GetTop() const { return m_Top; }

    private:
        /// The colors of the view, resolved from the theme when drawing, so that a theme switch animates.
        enum class Ink : uint8_t
        {
            Label,
            Secondary,
            Link,
            Fill,
            Stripe,
            Separator,
            Comment
        };

        /// A word, or a piece of text between spaces and style changes, measured once per document.
        struct Atom
        {
            std::string_view Text;
            Carbon::TextSpec Spec;
            Ink Color = Ink::Label;
            int Link = -1;
            float Width = 0.0f;
            float SpaceWidth = 0.0f;
            float Baseline = 0.0f;
            float LineHeight = 0.0f;
            /// A line may break after this atom.
            bool HasSpaceAfter = false;
            /// The first and last atom of an inline code span, which get a background.
            bool StartsCode = false;
            bool EndsCode = false;
            /// An image: its path relative to the image cache's root and its size in points.
            bool IsImage = false;
            std::string ImagePath;
        };

        /// The atoms of one paragraph, heading, list item or table cell.
        struct Inline
        {
            std::vector<Atom> Atoms;
            /// Its widest unbreakable piece and its width on one line, for the columns of tables.
            float MinWidth = 0.0f;
            float MaxWidth = 0.0f;
        };

        struct TextRun
        {
            Carbon::Vec2 Position;
            std::string Text;
            Carbon::TextSpec Spec;
            Ink Color = Ink::Label;
            int Link = -1;
            float Width = 0.0f;
            float Height = 0.0f;
            float Baseline = 0.0f;
        };

        enum class ShapeKind : uint8_t
        {
            Fill,
            Disc,
            Ring
        };

        struct Shape
        {
            Carbon::Rect Bounds;
            Ink Color = Ink::Fill;
            ShapeKind Kind = ShapeKind::Fill;
            float Radius = 0.0f;
        };

        struct Picture
        {
            Carbon::Rect Bounds;
            std::string Path;
        };

        /// Inline atoms laid out into lines.
        struct InlineLayout
        {
            float Height = 0.0f;
            /// The baseline of the first line, from its top.
            float FirstBaseline = 0.0f;
        };

    private:
        void Prepare(const Document& document, std::string_view path, DocImageCache& images);
        Inline MakeInline(const Spans& spans, const Carbon::TextSpec& base, Ink color, std::string_view path,
                          DocImageCache& images);
        void Layout(float width);
        InlineLayout LayoutInline(const Inline& content, float x, float y, float width,
                                  Carbon::TextAlignment alignment = Carbon::TextAlignment::Leading);
        float LayoutCodeBlock(const Block& block, float x, float y, float width);
        float LayoutTable(const Block& block, size_t firstInline, float x, float y, float width);
        float GetSpaceWidth(const Carbon::TextSpec& spec);
        Carbon::Color Resolve(Ink ink) const;

    private:
        const Document* m_Document = nullptr;
        std::string m_Path;
        /// Per block: the index of its first Inline (tables have one per cell).
        std::vector<size_t> m_BlockInlines;
        std::vector<Inline> m_Inlines;
        std::vector<std::pair<Carbon::TextSpec, float>> m_SpaceWidths;

        float m_Width = -1.0f;
        float m_Height = 0.0f;
        float m_Top = 0.0f;
        std::vector<TextRun> m_Runs;
        std::vector<Shape> m_Shapes;
        std::vector<Picture> m_Pictures;
        std::vector<std::pair<std::string_view, float>> m_Anchors;
    };
} // namespace WebApp
