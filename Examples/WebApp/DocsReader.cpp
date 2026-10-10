#include "DocsReader.h"

#include <format>

#include <Carbon/Extensions/Extensions.h>

#include "WebPlatform.h"

namespace WebApp
{
    using namespace Carbon;

    namespace
    {
        // Points between the top of the view and a heading that a link scrolled to.
        constexpr float AnchorMargin = 16.0f;
        constexpr std::string_view FolderIndex = "README.md";

        std::string_view GetFolder(std::string_view path)
        {
            const size_t slash = path.rfind('/');
            return slash == std::string_view::npos ? std::string_view() : path.substr(0, slash);
        }
    } // namespace

    DocsReader::DocsReader(const std::filesystem::path& root, std::string sourceUrl)
        : m_Images(root), m_SourceUrl(std::move(sourceUrl))
    {
        m_Library.Load(root);
        for (const DocEntry& entry : m_Library.GetEntries())
        {
            // A folder's README is its overview.
            const bool isIndex = entry.IsInSubfolder && entry.Path.ends_with(FolderIndex);
            m_Labels.push_back(std::format("{}##{}", isIndex ? "Overview" : entry.Title, entry.Path));
        }
        if (IsLoaded())
            m_CurrentPath = m_Library.GetEntries().front().Path;
    }

    void DocsReader::Open(std::string_view path, std::string_view anchor, bool scrollsToTop)
    {
        const DocEntry* entry = m_Library.Find(path);
        if (entry == nullptr && IsLoaded())
            entry = &m_Library.GetEntries().front();
        if (entry == nullptr)
            return;
        if (entry->Path != m_CurrentPath)
            m_IsImageClearPending = true;
        m_CurrentPath = entry->Path;
        m_PendingAnchor = std::string(anchor);
        m_IsScrollPending = scrollsToTop || !anchor.empty();
    }

    std::string_view DocsReader::GetCurrentTitle() const
    {
        const DocEntry* entry = m_Library.Find(m_CurrentPath);
        return entry != nullptr ? std::string_view(entry->Title) : std::string_view("Documentation");
    }

    bool DocsReader::Build(bool& isDark)
    {
        if (m_IsImageClearPending)
        {
            m_Images.Clear();
            m_IsImageClearPending = false;
        }
        bool isBackClicked = false;

        BeginHStack(
            {.Spacing = 0.0f, .Alignment = VerticalAlignment::Top, .Width = Size::Fill(), .Height = Size::Fill()});

        // The sidebar: top-level documents are the guides, each folder a group of its own.
        BeginSidebar("documents", {.Width = 250.0f});
        std::string_view group = "\n";
        const std::span<const DocEntry> entries = m_Library.GetEntries();
        for (size_t i = 0; i < entries.size(); i++)
        {
            const DocEntry& entry = entries[i];
            const std::string_view folder = GetFolder(entry.Path);
            if (folder != group)
            {
                SidebarHeader(folder.empty() ? std::string_view("Guides") : folder);
                group = folder;
            }
            const bool isIndex = entry.IsInSubfolder && entry.Path.ends_with(FolderIndex);
            const char* icon = !entry.IsInSubfolder ? Icons::BookOpenText : isIndex ? Icons::SquaresFour : Icons::Cube;
            if (SidebarItem(m_Labels[i], entry.Path == m_CurrentPath, {.Icon = icon}))
                Open(entry.Path, {}, false);
        }
        EndSidebar();

        BeginVStack({.Spacing = 0.0f, .Width = Size::Fill(), .Height = Size::Fill()});
        BeginHStack({.Spacing = 16.0f, .Padding = EdgeInsets(24.0f, 14.0f), .Width = Size::Fill()});
        isBackClicked = Button("##back", {.Role = ButtonRole::Plain, .Icon = Icons::CaretLeft});
        Tooltip("Back to the start");
        Text("Documentation", {.Style = TextStyle::Title2, .Emphasized = true});
        Spacer();
        if (Button("View Source",
                   {.Role = ButtonRole::Plain, .ControlSize = ControlSize::Small, .Icon = Icons::ArrowSquareOut}))
            OpenUrl(std::format("{}/Docs/{}", m_SourceUrl, m_CurrentPath));
        Tooltip("Open this document's Markdown on GitHub");
        if (Toggle("Dark", &isDark, {.ControlSize = ControlSize::Small}))
            SetTheme(isDark ? Theme::Dark() : Theme::Light());
        EndHStack();
        Separator();

        // Each document has its own scroll view, so each one remembers how far it was scrolled.
        int clickedLink = -1;
        const Document& document = m_Library.GetDocument(m_CurrentPath);
        BeginScrollView(m_CurrentPath, {.Padding = EdgeInsets(40.0f, 32.0f)});
        clickedLink = m_View.Show(document, m_CurrentPath, m_Images);
        EndScrollView();
        if (m_IsScrollPending)
        {
            // The document has been laid out in this frame, so its headings' positions are known.
            const Rect view = GetLastItemRect();
            const float offset = GetScrollOffset(m_CurrentPath).Y;
            float target = 0.0f;
            if (const std::optional<float> anchor = m_View.FindAnchor(m_PendingAnchor);
                anchor && !m_PendingAnchor.empty())
                target = std::max(0.0f, offset + m_View.GetTop() + *anchor - view.Y - AnchorMargin);
            SetScrollOffset(m_CurrentPath, Vec2(0.0f, target));
            m_IsScrollPending = false;
        }

        EndVStack();
        EndHStack();

        if (clickedLink >= 0 && static_cast<size_t>(clickedLink) < document.Links.size())
            FollowLink(document.Links[static_cast<size_t>(clickedLink)]);
        return isBackClicked;
    }

    void DocsReader::FollowLink(std::string_view target)
    {
        const LinkTarget link = ResolveLink(m_CurrentPath, target, m_SourceUrl);
        if (link.Kind == LinkKind::Document && m_Library.Find(link.Path) != nullptr)
            Open(link.Path, link.Anchor, true);
        else if (link.Kind == LinkKind::Document)
            OpenUrl(std::format("{}/Docs/{}", m_SourceUrl, link.Path));
        else
            OpenUrl(link.Url);
    }
} // namespace WebApp
