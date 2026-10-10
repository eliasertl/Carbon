#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Markdown.h"

namespace WebApp
{
    /// One Markdown file of the documentation.
    struct DocEntry
    {
        /// Its path relative to the documentation folder, with forward slashes: "Components/Button.md".
        std::string Path;
        /// Its first level-1 heading, or the file name without extension.
        std::string Title;
        /// It lives in a subfolder (Components/), not at the top level.
        bool IsInSubfolder = false;
    };

    /// What a link in a document points to.
    enum class LinkKind : uint8_t
    {
        /// Another document, or a heading in one: Path and Anchor.
        Document,
        /// Anything else, opened in a new browser tab: Url. Files of the repository outside the documentation
        /// (source files, the README) link to the repository's web page.
        External
    };

    /// A resolved link.
    struct LinkTarget
    {
        LinkKind Kind = LinkKind::External;
        std::string Path;
        std::string Anchor;
        std::string Url;
    };

    /// Joins `relative` to the folder of the document `from` and removes "." and ".." segments. Both are paths
    /// relative to the documentation folder; the result may start with "../" when it leaves the folder.
    std::string ResolvePath(std::string_view from, std::string_view relative);

    /// Resolves the link target `target` written in the document `from`. `sourceUrl` is the web address of the
    /// repository's files ("https://github.com/<owner>/<repo>/blob/<ref>"), which the documentation folder,
    /// "Docs", is in; repository files outside the documentation link there.
    LinkTarget ResolveLink(std::string_view from, std::string_view target, std::string_view sourceUrl);

    /// The Markdown files of a documentation folder, found when it is loaded, so that a new file shows up
    /// without code changes. Documents are parsed on first use and kept.
    class DocLibrary
    {
    public:
        /// Finds every .md file below `root`. Top-level documents come first, the "Getting started" guide
        /// (GettingStarted.md) leading and the others ordered by title; then those of subfolders, each folder's
        /// README.md first. Returns false when the folder has no Markdown file.
        bool Load(const std::filesystem::path& root);

        std::span<const DocEntry> GetEntries() const { return m_Entries; }
        /// The entry of a path relative to the folder; null when there is none.
        const DocEntry* Find(std::string_view path) const;
        /// The parsed document of an entry's path; an empty document when it cannot be read.
        const Document& GetDocument(std::string_view path);
        const std::filesystem::path& GetRoot() const { return m_Root; }

    private:
        std::filesystem::path m_Root;
        std::vector<DocEntry> m_Entries;
        /// Parsed documents, by the index of their entry.
        std::vector<std::unique_ptr<Document>> m_Documents;
    };

    /// Reads a whole file; empty when it cannot be read.
    std::string ReadFile(const std::filesystem::path& path);
} // namespace WebApp
