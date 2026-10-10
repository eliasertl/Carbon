#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace WebApp
{
    /// A run of inline text with one style: plain, strong, emphasized, code, part of a link, or an image.
    struct Span
    {
        /// The text; for an image, its alternative text.
        std::string Text;
        bool IsStrong = false;
        bool IsEmphasis = false;
        bool IsCode = false;
        /// Index into Document::Links when the span is (part of) a link; -1 otherwise.
        int Link = -1;
        /// An image: Target is its path as written in the Markdown.
        bool IsImage = false;
        std::string Target;
    };

    /// The inline content of a paragraph, heading, list item or table cell.
    using Spans = std::vector<Span>;

    /// What a block of a document is.
    enum class BlockKind : uint8_t
    {
        Heading,
        Paragraph,
        ListItem,
        CodeBlock,
        Table,
        Rule
    };

    /// How the cells of a table column are aligned.
    enum class ColumnAlignment : uint8_t
    {
        Leading,
        Center,
        Trailing
    };

    /// One block of a document. Blocks are flat: nesting in lists and quotes is kept as Indent and IsQuote.
    struct Block
    {
        BlockKind Kind = BlockKind::Paragraph;
        /// Heading level, 1 to 6.
        int Level = 0;
        /// List nesting: a list item at depth d has Indent d, a paragraph or code block inside it d + 1.
        int Indent = 0;
        /// Inside a block quote.
        bool IsQuote = false;
        /// A list item's marker: empty for a bullet, else the number with its delimiter ("3.").
        std::string Marker;
        /// Headings, paragraphs and list items.
        Spans Content;
        /// The anchor of a heading, as GitHub makes it: "#building-the-web-app" links to it.
        std::string Anchor;
        /// A code block's text, without the final line break, and the language of its fence ("cpp", "sh").
        std::string Code;
        std::string Language;
        /// A table's rows of cells; the first row is the header.
        std::vector<std::vector<Spans>> Rows;
        std::vector<ColumnAlignment> Alignments;
    };

    /// A parsed Markdown document.
    struct Document
    {
        /// The text of the first level-1 heading; empty when there is none.
        std::string Title;
        std::vector<Block> Blocks;
        /// The targets of the links, as written; Span::Link indexes them.
        std::vector<std::string> Links;
    };

    /// Parses the subset of GitHub Flavored Markdown that Carbon's documentation uses: ATX headings, paragraphs,
    /// nested bullet and ordered lists, fenced code blocks, pipe tables, block quotes, thematic breaks, and inline
    /// code, strong and emphasized text, links, images and backslash escapes. HTML comments are skipped.
    Document ParseMarkdown(std::string_view text);

    /// Parses inline Markdown into spans. Link targets are appended to `links`.
    Spans ParseInline(std::string_view text, std::vector<std::string>& links);

    /// The anchor GitHub gives a heading: lower case, punctuation removed, spaces turned into hyphens.
    std::string MakeAnchor(std::string_view heading);

    /// The text of spans without their formatting.
    std::string GetPlainText(const Spans& spans);
} // namespace WebApp
