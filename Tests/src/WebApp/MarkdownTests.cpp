#include <gtest/gtest.h>

#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include "DocLibrary.h"
#include "Markdown.h"

// The Markdown parser and the documentation library of the web app (Examples/WebApp). They have no GPU or browser
// code, so they are tested here, and the last test reads the real documentation: every document must parse, and
// every link and image in it must lead somewhere.
namespace WebApp
{
    TEST(MarkdownTests, ParsesInlineStyles)
    {
        std::vector<std::string> links;
        const Spans spans = ParseInline("Use **bold**, *em*, `code` and [a *link*](Other.md#x).", links);
        ASSERT_EQ(spans.size(), 10u);
        EXPECT_EQ(spans[0].Text, "Use ");
        EXPECT_EQ(spans[1].Text, "bold");
        EXPECT_TRUE(spans[1].IsStrong);
        EXPECT_EQ(spans[3].Text, "em");
        EXPECT_TRUE(spans[3].IsEmphasis);
        EXPECT_EQ(spans[5].Text, "code");
        EXPECT_TRUE(spans[5].IsCode);
        EXPECT_EQ(spans[7].Text, "a ");
        EXPECT_EQ(spans[7].Link, 0);
        EXPECT_EQ(spans[8].Text, "link");
        EXPECT_TRUE(spans[8].IsEmphasis);
        EXPECT_EQ(spans[8].Link, 0);
        EXPECT_EQ(spans[9].Text, ".");
        ASSERT_EQ(links.size(), 1u);
        EXPECT_EQ(links[0], "Other.md#x");
    }

    TEST(MarkdownTests, KeepsLiteralDelimiters)
    {
        std::vector<std::string> links;
        Spans spans = ParseInline("snake_case_name, 2 * 3 * 4, \\*not\\* and [no link]", links);
        ASSERT_EQ(spans.size(), 1u);
        EXPECT_EQ(spans[0].Text, "snake_case_name, 2 * 3 * 4, *not* and [no link]");

        spans = ParseInline("``a ` b`` and `` `x` ``", links);
        ASSERT_EQ(spans.size(), 3u);
        EXPECT_EQ(spans[0].Text, "a ` b");
        EXPECT_EQ(spans[2].Text, "`x`");
        EXPECT_TRUE(links.empty());
    }

    TEST(MarkdownTests, ParsesImages)
    {
        std::vector<std::string> links;
        const Spans spans = ParseInline("![The *Gallery*](Images/Gallery-Light.png \"title\")", links);
        ASSERT_EQ(spans.size(), 1u);
        EXPECT_TRUE(spans[0].IsImage);
        EXPECT_EQ(spans[0].Text, "The *Gallery*");
        EXPECT_EQ(spans[0].Target, "Images/Gallery-Light.png");
    }

    TEST(MarkdownTests, ParsesBlocks)
    {
        const Document document = ParseMarkdown(
            "# Title\n"
            "\n"
            "A paragraph\n"
            "on two lines.\n"
            "\n"
            "## Second *part*\n"
            "---\n"
            "> **Note.** Quoted\n"
            "> text.\n");
        ASSERT_EQ(document.Blocks.size(), 5u);
        EXPECT_EQ(document.Title, "Title");
        EXPECT_EQ(document.Blocks[0].Kind, BlockKind::Heading);
        EXPECT_EQ(document.Blocks[0].Anchor, "title");
        EXPECT_EQ(document.Blocks[1].Kind, BlockKind::Paragraph);
        EXPECT_EQ(GetPlainText(document.Blocks[1].Content), "A paragraph on two lines.");
        EXPECT_EQ(document.Blocks[2].Level, 2);
        EXPECT_EQ(document.Blocks[2].Anchor, "second-part");
        EXPECT_EQ(document.Blocks[3].Kind, BlockKind::Rule);
        EXPECT_TRUE(document.Blocks[4].IsQuote);
        EXPECT_EQ(GetPlainText(document.Blocks[4].Content), "Note. Quoted text.");
    }

    TEST(MarkdownTests, ParsesNestedLists)
    {
        const Document document = ParseMarkdown(
            "- One\n"
            "  continued\n"
            "  - Nested\n"
            "- Two\n"
            "\n"
            "  A paragraph of Two.\n"
            "\n"
            "  ```cpp\n"
            "  int x;\n"
            "  ```\n"
            "\n"
            "1. First\n"
            "2. Second\n"
            "\n"
            "After.\n");
        ASSERT_EQ(document.Blocks.size(), 8u);
        EXPECT_EQ(GetPlainText(document.Blocks[0].Content), "One continued");
        EXPECT_EQ(document.Blocks[0].Indent, 0);
        EXPECT_EQ(document.Blocks[1].Indent, 1);
        EXPECT_EQ(document.Blocks[2].Indent, 0);
        EXPECT_EQ(document.Blocks[3].Kind, BlockKind::Paragraph);
        EXPECT_EQ(document.Blocks[3].Indent, 1);
        EXPECT_EQ(document.Blocks[4].Kind, BlockKind::CodeBlock);
        EXPECT_EQ(document.Blocks[4].Indent, 1);
        EXPECT_EQ(document.Blocks[4].Code, "int x;");
        EXPECT_EQ(document.Blocks[4].Language, "cpp");
        EXPECT_EQ(document.Blocks[5].Marker, "1.");
        EXPECT_EQ(document.Blocks[6].Marker, "2.");
        EXPECT_EQ(document.Blocks[7].Kind, BlockKind::Paragraph);
        EXPECT_EQ(document.Blocks[7].Indent, 0);
    }

    TEST(MarkdownTests, ParsesTables)
    {
        const Document document = ParseMarkdown(
            "| Name | Value | Note |\n"
            "| :--- | ---: | :-: |\n"
            "| `a \\| b` | 1 |\n");
        ASSERT_EQ(document.Blocks.size(), 1u);
        const Block& table = document.Blocks[0];
        ASSERT_EQ(table.Rows.size(), 2u);
        ASSERT_EQ(table.Alignments.size(), 3u);
        EXPECT_EQ(table.Alignments[0], ColumnAlignment::Leading);
        EXPECT_EQ(table.Alignments[1], ColumnAlignment::Trailing);
        EXPECT_EQ(table.Alignments[2], ColumnAlignment::Center);
        ASSERT_EQ(table.Rows[1].size(), 3u);
        ASSERT_EQ(table.Rows[1][0].size(), 1u);
        EXPECT_TRUE(table.Rows[1][0][0].IsCode);
        EXPECT_EQ(table.Rows[1][0][0].Text, "a | b");
        EXPECT_TRUE(table.Rows[1][2].empty());
    }

    TEST(MarkdownTests, MakesAnchorsLikeGitHub)
    {
        EXPECT_EQ(MakeAnchor("Emscripten (web browsers)"), "emscripten-web-browsers");
        EXPECT_EQ(MakeAnchor("Decision \xE2\x80\x94 naming."), "decision--naming");
        EXPECT_EQ(MakeAnchor("CARBON_DEPS_<NAME>_BUILD"), "carbon_deps_name_build");
        const Document document = ParseMarkdown("## Options\n## Options\n");
        ASSERT_EQ(document.Blocks.size(), 2u);
        EXPECT_EQ(document.Blocks[1].Anchor, "options-1");
    }

    TEST(MarkdownTests, ResolvesLinks)
    {
        const std::string source = "https://example.com/blob/main";
        LinkTarget link = ResolveLink("Components/Button.md", "../Layout.md#size", source);
        EXPECT_EQ(link.Kind, LinkKind::Document);
        EXPECT_EQ(link.Path, "Layout.md");
        EXPECT_EQ(link.Anchor, "size");

        link = ResolveLink("Components/Button.md", "#options", source);
        EXPECT_EQ(link.Kind, LinkKind::Document);
        EXPECT_EQ(link.Path, "Components/Button.md");

        link = ResolveLink("Building.md", "../Examples/Minimal/OpenGLESMinimal.cpp", source);
        EXPECT_EQ(link.Kind, LinkKind::External);
        EXPECT_EQ(link.Url, source + "/Examples/Minimal/OpenGLESMinimal.cpp");

        link = ResolveLink("Building.md", "https://emscripten.org", source);
        EXPECT_EQ(link.Kind, LinkKind::External);
        EXPECT_EQ(link.Url, "https://emscripten.org");

        EXPECT_EQ(ResolvePath("Components/Button.md", "../Images/Components/Button.png"),
                  "Images/Components/Button.png");
    }

    // Every document of Docs/ parses into blocks, and its links and images lead to documents, headings and files
    // that exist, so that the web app's reader has no dead ends.
    TEST(MarkdownTests, EveryDocumentResolves)
    {
        const std::filesystem::path root = CARBON_TESTS_DOCS_DIR;
        DocLibrary library;
        ASSERT_TRUE(library.Load(root));
        ASSERT_NE(library.Find("Components/README.md"), nullptr);

        for (const DocEntry& entry : library.GetEntries())
        {
            const Document& document = library.GetDocument(entry.Path);
            EXPECT_FALSE(document.Title.empty()) << entry.Path;
            EXPECT_FALSE(document.Blocks.empty()) << entry.Path;

            for (const std::string& target : document.Links)
            {
                const LinkTarget link = ResolveLink(entry.Path, target, "https://example.com");
                if (link.Kind != LinkKind::Document)
                {
                    if (link.Url.starts_with("https://example.com/"))
                    {
                        const std::string file = link.Url.substr(std::string("https://example.com/").size());
                        EXPECT_TRUE(std::filesystem::exists(root.parent_path() / file.substr(0, file.find('#'))))
                            << std::format("{}: {}", entry.Path, target);
                    }
                    continue;
                }
                ASSERT_NE(library.Find(link.Path), nullptr) << std::format("{}: {}", entry.Path, target);
                if (link.Anchor.empty())
                    continue;
                bool found = false;
                for (const Block& block : library.GetDocument(link.Path).Blocks)
                    found = found || block.Anchor == link.Anchor;
                EXPECT_TRUE(found) << std::format("{}: {}", entry.Path, target);
            }

            auto checkImages = [&](const Spans& spans)
            {
                for (const Span& span : spans)
                {
                    if (span.IsImage)
                        EXPECT_TRUE(std::filesystem::exists(root / ResolvePath(entry.Path, span.Target)))
                            << std::format("{}: {}", entry.Path, span.Target);
                }
            };
            for (const Block& block : document.Blocks)
            {
                checkImages(block.Content);
                for (const std::vector<Spans>& row : block.Rows)
                {
                    for (const Spans& cell : row)
                        checkImages(cell);
                }
            }
        }
    }
} // namespace WebApp
