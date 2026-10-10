#include "Markdown.h"

#include <algorithm>
#include <map>
#include <optional>

namespace WebApp
{
    namespace
    {
        bool IsSpace(char c)
        {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r';
        }

        bool IsAsciiPunctuation(char c)
        {
            return (c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`') || (c >= '{' && c <= '~');
        }

        bool IsAlphanumeric(char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        }

        std::string_view Trim(std::string_view text)
        {
            while (!text.empty() && IsSpace(text.front()))
                text.remove_prefix(1);
            while (!text.empty() && IsSpace(text.back()))
                text.remove_suffix(1);
            return text;
        }

        bool StartsWith(std::string_view text, std::string_view prefix)
        {
            return text.substr(0, prefix.size()) == prefix;
        }

        size_t CountRun(std::string_view text, size_t position, char c)
        {
            size_t end = position;
            while (end < text.size() && text[end] == c)
                end++;
            return end - position;
        }

        // The position of the backtick run of exactly `length` that closes a code span whose content starts at
        // `position`; npos when there is none.
        size_t FindCodeEnd(std::string_view text, size_t position, size_t length)
        {
            while (position < text.size())
            {
                const size_t start = text.find('`', position);
                if (start == std::string_view::npos)
                    return std::string_view::npos;
                const size_t run = CountRun(text, start, '`');
                if (run == length)
                    return start;
                position = start + run;
            }
            return std::string_view::npos;
        }

        // Skips the code span or backtick run at `position`; returns the position after it.
        size_t SkipCode(std::string_view text, size_t position)
        {
            const size_t run = CountRun(text, position, '`');
            const size_t end = FindCodeEnd(text, position + run, run);
            return end == std::string_view::npos ? position + run : end + run;
        }

        // The position of the bracket that closes the one at `open`, skipping escapes, code spans and nested
        // brackets; npos when there is none.
        size_t FindClosing(std::string_view text, size_t open, char opening, char closing)
        {
            int depth = 0;
            size_t i = open;
            while (i < text.size())
            {
                const char c = text[i];
                if (c == '\\')
                {
                    i += 2;
                    continue;
                }
                if (c == '`' && opening == '[')
                {
                    i = SkipCode(text, i);
                    continue;
                }
                if (c == opening)
                    depth++;
                else if (c == closing && --depth == 0)
                    return i;
                i++;
            }
            return std::string_view::npos;
        }

        // The target of a link or image from what is between its parentheses: without a title and angle brackets.
        std::string CleanTarget(std::string_view target)
        {
            target = Trim(target);
            const size_t space = target.find_first_of(" \t");
            if (space != std::string_view::npos)
                target = target.substr(0, space);
            if (target.size() >= 2 && target.front() == '<' && target.back() == '>')
                target = target.substr(1, target.size() - 2);
            return std::string(target);
        }

        struct InlineStyle
        {
            bool IsStrong = false;
            bool IsEmphasis = false;
            int Link = -1;
        };

        class InlineParser
        {
        public:
            InlineParser(Spans& spans, std::vector<std::string>& links) : m_Spans(spans), m_Links(links) {}

            void Parse(std::string_view text, InlineStyle style)
            {
                size_t i = 0;
                while (i < text.size())
                {
                    const char c = text[i];
                    const char next = i + 1 < text.size() ? text[i + 1] : '\0';
                    if (c == '\\' && IsAsciiPunctuation(next))
                    {
                        Append(text.substr(i + 1, 1), style);
                        i += 2;
                    }
                    else if (c == '`')
                        i = ParseCode(text, i, style);
                    else if (c == '!' && next == '[' && ParseImage(text, i, style, i))
                    {
                    }
                    else if (c == '[' && ParseLink(text, i, style, i))
                    {
                    }
                    else if ((c == '*' || c == '_') && ParseEmphasis(text, i, style, i))
                    {
                    }
                    else if (c == '<' && StartsWith(text.substr(i), "<!--"))
                    {
                        const size_t end = text.find("-->", i + 4);
                        i = end == std::string_view::npos ? text.size() : end + 3;
                    }
                    else
                    {
                        // A run of a delimiter character that did not open anything stays text as a whole.
                        const size_t run = (c == '*' || c == '_') ? CountRun(text, i, c) : 1;
                        Append(text.substr(i, run), style);
                        i += run;
                    }
                }
            }

        private:
            void Append(std::string_view text, InlineStyle style, bool isCode = false)
            {
                if (!m_Spans.empty())
                {
                    Span& last = m_Spans.back();
                    if (!last.IsImage && last.IsStrong == style.IsStrong && last.IsEmphasis == style.IsEmphasis &&
                        last.IsCode == isCode && last.Link == style.Link)
                    {
                        last.Text += text;
                        return;
                    }
                }
                Span span;
                span.Text = std::string(text);
                span.IsStrong = style.IsStrong;
                span.IsEmphasis = style.IsEmphasis;
                span.IsCode = isCode;
                span.Link = style.Link;
                m_Spans.push_back(std::move(span));
            }

            size_t ParseCode(std::string_view text, size_t position, InlineStyle style)
            {
                const size_t run = CountRun(text, position, '`');
                const size_t end = FindCodeEnd(text, position + run, run);
                if (end == std::string_view::npos)
                {
                    Append(text.substr(position, run), style);
                    return position + run;
                }
                std::string code(text.substr(position + run, end - position - run));
                std::replace(code.begin(), code.end(), '\n', ' ');
                // One space on both sides is stripped, so that a span can start or end with a backtick.
                if (code.size() >= 2 && code.front() == ' ' && code.back() == ' ' &&
                    code.find_first_not_of(' ') != std::string::npos)
                    code = code.substr(1, code.size() - 2);
                // Code is never merged with the text before it: two code spans in a row stay two.
                Span span;
                span.Text = std::move(code);
                span.IsStrong = style.IsStrong;
                span.IsEmphasis = style.IsEmphasis;
                span.IsCode = true;
                span.Link = style.Link;
                m_Spans.push_back(std::move(span));
                return end + run;
            }

            // [text](target) at `position`, or ![alt](target) one character later. Sets `next` past it.
            bool ParseBracketed(std::string_view text, size_t open, std::string_view& label, std::string& target,
                                size_t& next)
            {
                const size_t close = FindClosing(text, open, '[', ']');
                if (close == std::string_view::npos || close + 1 >= text.size() || text[close + 1] != '(')
                    return false;
                const size_t end = FindClosing(text, close + 1, '(', ')');
                if (end == std::string_view::npos)
                    return false;
                label = text.substr(open + 1, close - open - 1);
                target = CleanTarget(text.substr(close + 2, end - close - 2));
                next = end + 1;
                return true;
            }

            bool ParseImage(std::string_view text, size_t position, InlineStyle style, size_t& next)
            {
                std::string_view alt;
                std::string target;
                if (!ParseBracketed(text, position + 1, alt, target, next))
                    return false;
                Span span;
                span.Text = std::string(alt);
                span.IsImage = true;
                span.Target = std::move(target);
                span.Link = style.Link;
                m_Spans.push_back(std::move(span));
                return true;
            }

            bool ParseLink(std::string_view text, size_t position, InlineStyle style, size_t& next)
            {
                std::string_view label;
                std::string target;
                if (!ParseBracketed(text, position, label, target, next))
                    return false;
                m_Links.push_back(std::move(target));
                InlineStyle inner = style;
                inner.Link = static_cast<int>(m_Links.size() - 1);
                Parse(label, inner);
                return true;
            }

            // *emphasis*, **strong** and ***both***, with * or _. An underscore inside a word is text.
            bool ParseEmphasis(std::string_view text, size_t position, InlineStyle style, size_t& next)
            {
                const char c = text[position];
                const size_t run = CountRun(text, position, c);
                const size_t start = position + run;
                if (run > 3 || start >= text.size() || IsSpace(text[start]))
                    return false;
                if (c == '_' && position > 0 && IsAlphanumeric(text[position - 1]))
                    return false;

                size_t i = start;
                while (i < text.size())
                {
                    if (text[i] == '\\')
                    {
                        i += 2;
                        continue;
                    }
                    if (text[i] == '`')
                    {
                        i = SkipCode(text, i);
                        continue;
                    }
                    if (text[i] != c)
                    {
                        i++;
                        continue;
                    }
                    const size_t closing = CountRun(text, i, c);
                    const bool isAfterText = i > start && !IsSpace(text[i - 1]);
                    const bool isWordEnd = c != '_' || i + closing >= text.size() || !IsAlphanumeric(text[i + closing]);
                    if (closing == run && isAfterText && isWordEnd)
                    {
                        InlineStyle inner = style;
                        inner.IsStrong = style.IsStrong || run >= 2;
                        inner.IsEmphasis = style.IsEmphasis || run != 2;
                        Parse(text.substr(start, i - start), inner);
                        next = i + closing;
                        return true;
                    }
                    i += closing;
                }
                return false;
            }

        private:
            Spans& m_Spans;
            std::vector<std::string>& m_Links;
        };

        // Columns of leading whitespace, a tab counting as four.
        int CountIndent(std::string_view line)
        {
            int indent = 0;
            for (char c : line)
            {
                if (c == ' ')
                    indent++;
                else if (c == '\t')
                    indent += 4;
                else
                    break;
            }
            return indent;
        }

        // The line without up to `columns` columns of leading whitespace.
        std::string_view RemoveIndent(std::string_view line, int columns)
        {
            int removed = 0;
            size_t i = 0;
            while (i < line.size() && removed < columns && (line[i] == ' ' || line[i] == '\t'))
            {
                removed += line[i] == '\t' ? 4 : 1;
                i++;
            }
            return line.substr(i);
        }

        bool IsRule(std::string_view content)
        {
            const char c = content.empty() ? '\0' : content[0];
            if (c != '-' && c != '*' && c != '_')
                return false;
            int count = 0;
            for (char other : content)
            {
                if (other == c)
                    count++;
                else if (!IsSpace(other))
                    return false;
            }
            return count >= 3;
        }

        struct ListMarker
        {
            /// Empty for a bullet.
            std::string Text;
            /// Columns from the marker to the item's text.
            int Width = 0;
            std::string_view Rest;
        };

        std::optional<ListMarker> MatchListItem(std::string_view content)
        {
            ListMarker marker;
            size_t length = 0;
            if (!content.empty() && (content[0] == '-' || content[0] == '*' || content[0] == '+'))
                length = 1;
            else
            {
                while (length < content.size() && length < 9 && content[length] >= '0' && content[length] <= '9')
                    length++;
                if (length == 0 || length >= content.size() || (content[length] != '.' && content[length] != ')'))
                    return std::nullopt;
                length++;
                marker.Text = std::string(content.substr(0, length));
            }
            if (length < content.size() && content[length] != ' ' && content[length] != '\t')
                return std::nullopt;
            const size_t text = content.find_first_not_of(" \t", length);
            marker.Rest = text == std::string_view::npos ? std::string_view() : content.substr(text);
            marker.Width = static_cast<int>(text == std::string_view::npos ? length + 1 : text);
            return marker;
        }

        bool IsTableSeparator(std::string_view line)
        {
            line = Trim(line);
            if (line.empty() || line.find('-') == std::string_view::npos || line.find('|') == std::string_view::npos)
                return false;
            return line.find_first_not_of("|:- \t") == std::string_view::npos;
        }

        // The cells of a table row: split at pipes that are not escaped, with "\|" turned into "|".
        std::vector<std::string> SplitRow(std::string_view line)
        {
            line = Trim(line);
            if (!line.empty() && line.front() == '|')
                line.remove_prefix(1);
            if (line.size() >= 2 && line.back() == '|' && line[line.size() - 2] != '\\')
                line.remove_suffix(1);
            else if (line.size() == 1 && line.back() == '|')
                line.remove_suffix(1);

            std::vector<std::string> cells;
            std::string cell;
            for (size_t i = 0; i < line.size(); i++)
            {
                if (line[i] == '\\' && i + 1 < line.size() && line[i + 1] == '|')
                {
                    cell += '|';
                    i++;
                }
                else if (line[i] == '|')
                {
                    cells.emplace_back(Trim(cell));
                    cell.clear();
                }
                else
                    cell += line[i];
            }
            cells.emplace_back(Trim(cell));
            return cells;
        }

        class BlockParser
        {
        public:
            explicit BlockParser(Document& document) : m_Document(document) {}

            void Parse(const std::vector<std::string_view>& lines, bool isQuote)
            {
                m_IsQuote = isQuote;
                size_t i = 0;
                while (i < lines.size())
                    i = ParseLine(lines, i);
                Flush();
            }

        private:
            struct ListLevel
            {
                int MarkerIndent = 0;
                int ContentColumn = 0;
            };

            enum class Pending : uint8_t
            {
                None,
                Paragraph,
                ListItem
            };

            // Parses the block that starts at line `i`; returns the line after it.
            size_t ParseLine(const std::vector<std::string_view>& lines, size_t i)
            {
                const std::string_view line = lines[i];
                const int indent = CountIndent(line);
                const std::string_view content = Trim(line);

                if (content.empty())
                {
                    Flush();
                    m_SawBlank = true;
                    return i + 1;
                }
                if (StartsWith(content, "<!--"))
                {
                    Flush();
                    while (i < lines.size() && lines[i].find("-->") == std::string_view::npos)
                        i++;
                    return i + 1;
                }
                if (StartsWith(content, "```") || StartsWith(content, "~~~"))
                    return ParseCodeBlock(lines, i, indent, content);
                if (content[0] == '#')
                {
                    const size_t level = CountRun(content, 0, '#');
                    if (level <= 6 && (level == content.size() || content[level] == ' '))
                    {
                        Flush();
                        m_Lists.clear();
                        std::string_view title = Trim(content.substr(level));
                        while (!title.empty() && title.back() == '#')
                            title.remove_suffix(1);
                        Block block = MakeBlock(BlockKind::Heading, 0);
                        block.Level = static_cast<int>(level);
                        block.Content = ParseInline(Trim(title), m_Document.Links);
                        m_Document.Blocks.push_back(std::move(block));
                        m_SawBlank = false;
                        return i + 1;
                    }
                }
                if (IsRule(content))
                {
                    Flush();
                    m_Lists.clear();
                    m_Document.Blocks.push_back(MakeBlock(BlockKind::Rule, 0));
                    return i + 1;
                }
                if (content[0] == '>')
                    return ParseQuote(lines, i);
                if (content[0] == '|' && i + 1 < lines.size() && IsTableSeparator(lines[i + 1]))
                    return ParseTable(lines, i);
                if (const std::optional<ListMarker> marker = MatchListItem(content))
                {
                    StartListItem(indent, *marker);
                    return i + 1;
                }

                // Text continues the paragraph or list item before it, unless a blank line ended that.
                if (m_Pending != Pending::None && !m_SawBlank)
                {
                    m_PendingText += ' ';
                    m_PendingText += content;
                    return i + 1;
                }
                Flush();
                m_Pending = Pending::Paragraph;
                m_PendingBlock = MakeBlock(BlockKind::Paragraph, GetNestedIndent(indent));
                m_PendingText = std::string(content);
                m_SawBlank = false;
                return i + 1;
            }

            size_t ParseCodeBlock(const std::vector<std::string_view>& lines, size_t i, int indent,
                                  std::string_view content)
            {
                Flush();
                const char fence = content[0];
                const size_t length = CountRun(content, 0, fence);
                Block block = MakeBlock(BlockKind::CodeBlock, GetNestedIndent(indent));
                block.Language = std::string(Trim(content.substr(length)));
                i++;
                bool isFirst = true;
                while (i < lines.size())
                {
                    const std::string_view inner = Trim(lines[i]);
                    if (CountRun(inner, 0, fence) >= length && Trim(inner.substr(CountRun(inner, 0, fence))).empty())
                    {
                        i++;
                        break;
                    }
                    if (!isFirst)
                        block.Code += '\n';
                    block.Code += RemoveIndent(lines[i], indent);
                    isFirst = false;
                    i++;
                }
                m_Document.Blocks.push_back(std::move(block));
                m_SawBlank = false;
                return i;
            }

            size_t ParseQuote(const std::vector<std::string_view>& lines, size_t i)
            {
                Flush();
                m_Lists.clear();
                std::vector<std::string_view> inner;
                while (i < lines.size())
                {
                    std::string_view content = Trim(lines[i]);
                    if (content.empty() || content[0] != '>')
                        break;
                    content.remove_prefix(1);
                    if (!content.empty() && content[0] == ' ')
                        content.remove_prefix(1);
                    inner.push_back(content);
                    i++;
                }
                BlockParser quote(m_Document);
                quote.Parse(inner, true);
                m_SawBlank = false;
                return i;
            }

            size_t ParseTable(const std::vector<std::string_view>& lines, size_t i)
            {
                Flush();
                m_Lists.clear();
                Block block = MakeBlock(BlockKind::Table, 0);
                const std::vector<std::string> header = SplitRow(lines[i]);
                for (const std::string& cell : SplitRow(lines[i + 1]))
                {
                    const bool isLeft = !cell.empty() && cell.front() == ':';
                    const bool isRight = !cell.empty() && cell.back() == ':';
                    block.Alignments.push_back(isLeft && isRight ? ColumnAlignment::Center
                                               : isRight         ? ColumnAlignment::Trailing
                                                                 : ColumnAlignment::Leading);
                }
                block.Alignments.resize(header.size(), ColumnAlignment::Leading);
                i += 2;

                auto addRow = [&](const std::vector<std::string>& cells)
                {
                    std::vector<Spans> row;
                    for (size_t column = 0; column < header.size(); column++)
                        row.push_back(column < cells.size() ? ParseInline(cells[column], m_Document.Links) : Spans());
                    block.Rows.push_back(std::move(row));
                };
                addRow(header);
                while (i < lines.size())
                {
                    const std::string_view content = Trim(lines[i]);
                    if (content.empty() || content[0] != '|')
                        break;
                    addRow(SplitRow(content));
                    i++;
                }
                m_Document.Blocks.push_back(std::move(block));
                m_SawBlank = false;
                return i;
            }

            void StartListItem(int indent, const ListMarker& marker)
            {
                Flush();
                // Leave the levels this item is not nested in. An item whose marker is between a level's marker
                // and its text is a sibling at that level.
                while (!m_Lists.empty() && indent < m_Lists.back().MarkerIndent)
                    m_Lists.pop_back();
                const ListLevel level = {indent, indent + marker.Width};
                if (m_Lists.empty() || indent >= m_Lists.back().ContentColumn)
                    m_Lists.push_back(level);
                else
                    m_Lists.back() = level;

                m_Pending = Pending::ListItem;
                m_PendingBlock = MakeBlock(BlockKind::ListItem, static_cast<int>(m_Lists.size()) - 1);
                m_PendingBlock.Marker = marker.Text;
                m_PendingText = std::string(marker.Rest);
                m_SawBlank = false;
            }

            // The indent of a paragraph or code block that starts at column `indent`: inside the deepest list
            // item whose text it lines up with, or outside all lists.
            int GetNestedIndent(int indent)
            {
                while (!m_Lists.empty() && indent < m_Lists.back().ContentColumn)
                    m_Lists.pop_back();
                return static_cast<int>(m_Lists.size());
            }

            Block MakeBlock(BlockKind kind, int indent) const
            {
                Block block;
                block.Kind = kind;
                block.Indent = indent;
                block.IsQuote = m_IsQuote;
                return block;
            }

            void Flush()
            {
                if (m_Pending == Pending::None)
                    return;
                m_PendingBlock.Content = ParseInline(m_PendingText, m_Document.Links);
                m_Document.Blocks.push_back(std::move(m_PendingBlock));
                m_PendingBlock = Block();
                m_PendingText.clear();
                m_Pending = Pending::None;
            }

        private:
            Document& m_Document;
            bool m_IsQuote = false;
            bool m_SawBlank = false;
            std::vector<ListLevel> m_Lists;
            Pending m_Pending = Pending::None;
            Block m_PendingBlock;
            std::string m_PendingText;
        };
    } // namespace

    Document ParseMarkdown(std::string_view text)
    {
        std::vector<std::string_view> lines;
        size_t start = 0;
        while (start <= text.size())
        {
            size_t end = text.find('\n', start);
            if (end == std::string_view::npos)
                end = text.size();
            std::string_view line = text.substr(start, end - start);
            if (!line.empty() && line.back() == '\r')
                line.remove_suffix(1);
            lines.push_back(line);
            start = end + 1;
        }

        Document document;
        BlockParser parser(document);
        parser.Parse(lines, false);

        // Anchors, numbered like GitHub's when a heading repeats.
        std::map<std::string, int> counts;
        for (Block& block : document.Blocks)
        {
            if (block.Kind != BlockKind::Heading)
                continue;
            std::string anchor = MakeAnchor(GetPlainText(block.Content));
            const int count = counts[anchor]++;
            block.Anchor = count == 0 ? anchor : anchor + "-" + std::to_string(count);
            if (block.Level == 1 && document.Title.empty())
                document.Title = GetPlainText(block.Content);
        }
        return document;
    }

    Spans ParseInline(std::string_view text, std::vector<std::string>& links)
    {
        Spans spans;
        InlineParser parser(spans, links);
        parser.Parse(text, {});
        return spans;
    }

    std::string MakeAnchor(std::string_view heading)
    {
        std::string anchor;
        size_t i = 0;
        while (i < heading.size())
        {
            const unsigned char c = static_cast<unsigned char>(heading[i]);
            if (c < 0x80)
            {
                if (IsAlphanumeric(static_cast<char>(c)))
                    anchor += static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
                else if (c == ' ')
                    anchor += '-';
                else if (c == '-' || c == '_')
                    anchor += static_cast<char>(c);
                i++;
                continue;
            }
            // Characters beyond ASCII: letters stay, General Punctuation (dashes, quotes, ellipsis) and arrows
            // go, as on GitHub.
            size_t length = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
            length = std::min(length, heading.size() - i);
            const std::string_view character = heading.substr(i, length);
            bool isPunctuation = false;
            if (length == 3 && static_cast<unsigned char>(character[0]) == 0xE2)
            {
                const unsigned char second = static_cast<unsigned char>(character[1]);
                // U+2000-U+206F is E2 80 80-E2 81 AF; U+2190-U+21FF is E2 86 90-E2 87 BF.
                isPunctuation = second == 0x80 || second == 0x81 || second == 0x86 || second == 0x87;
            }
            if (!isPunctuation)
                anchor += character;
            i += length;
        }
        return anchor;
    }

    std::string GetPlainText(const Spans& spans)
    {
        std::string text;
        for (const Span& span : spans)
        {
            if (!span.IsImage)
                text += span.Text;
        }
        return text;
    }
} // namespace WebApp
