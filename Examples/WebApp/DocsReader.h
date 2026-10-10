#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "DocImageCache.h"
#include "DocLibrary.h"
#include "MarkdownView.h"

namespace WebApp
{
    /// The documentation reader: a sidebar with every document, grouped by folder, and the current document drawn
    /// by MarkdownView. Links to other documents open them, links to headings scroll there, and everything else
    /// opens in a new browser tab.
    /// The ID of the reader's navigation split view.
    inline constexpr std::string_view DocsNavigationID = "docs";

    class DocsReader
    {
    public:
        /// Loads the documentation below `root`. `sourceUrl` is the web address of the repository's files
        /// (".../blob/<ref>"), where links to files outside the documentation go.
        DocsReader(const std::filesystem::path& root, std::string sourceUrl);

        /// True when the folder had documents.
        bool IsLoaded() const { return !m_Library.GetEntries().empty(); }

        /// Shows the document at `path` (relative to the root; the first document when there is none), scrolled
        /// to the heading with `anchor`, or to the top when `scrollsToTop` is set. Otherwise the document keeps
        /// where it was scrolled to last.
        void Open(std::string_view path, std::string_view anchor = {}, bool scrollsToTop = false);
        const std::string& GetCurrentPath() const { return m_CurrentPath; }
        /// The title of the current document.
        std::string_view GetCurrentTitle() const;

        /// Builds the reader for one frame, filling the display. `isDark` is the appearance, which the header's
        /// toggle changes. Returns true when the back button was clicked.
        bool Build(bool& isDark);

        /// On a phone the reader is a navigation stack: the list of documents, from which a document slides in.
        /// Whether the document is shown rather than the list; ShowDocument switches without sliding (an
        /// address). In regular width both are always shown.
        bool IsDocumentShown() const;
        void ShowDocument(bool isShown);

    private:
        void FollowLink(std::string_view target);

    private:
        DocLibrary m_Library;
        DocImageCache m_Images;
        MarkdownView m_View;
        std::string m_SourceUrl;
        /// The sidebar's labels, one per entry, made once: "Title##path".
        std::vector<std::string> m_Labels;
        std::string m_CurrentPath;
        std::string m_PendingAnchor;
        bool m_IsScrollPending = false;
        /// The textures of the previous document are deleted at the start of the next frame, once the frame that
        /// drew them has been rendered.
        bool m_IsImageClearPending = false;
    };
} // namespace WebApp
