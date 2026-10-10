#include "DocLibrary.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

namespace WebApp
{
    namespace
    {
        bool StartsWith(std::string_view text, std::string_view prefix)
        {
            return text.substr(0, prefix.size()) == prefix;
        }

        bool EndsWith(std::string_view text, std::string_view suffix)
        {
            return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
        }

        // The documentation folder's path in the repository, for links that leave it.
        constexpr std::string_view DocsFolder = "Docs";
        constexpr std::string_view FirstGuide = "GettingStarted.md";
        constexpr std::string_view FolderIndex = "README.md";

        std::string GetFileName(std::string_view path)
        {
            const size_t slash = path.rfind('/');
            return std::string(slash == std::string_view::npos ? path : path.substr(slash + 1));
        }

        // Where an entry goes in the sidebar: top-level documents first, the first guide and folder indexes
        // leading their group.
        int GetRank(const DocEntry& entry)
        {
            if (!entry.IsInSubfolder)
                return GetFileName(entry.Path) == FirstGuide ? 0 : 1;
            return GetFileName(entry.Path) == FolderIndex ? 2 : 3;
        }

        std::string GetFolder(std::string_view path)
        {
            const size_t slash = path.rfind('/');
            return slash == std::string_view::npos ? std::string() : std::string(path.substr(0, slash));
        }
    } // namespace

    std::string ResolvePath(std::string_view from, std::string_view relative)
    {
        std::string combined = GetFolder(from);
        if (!combined.empty())
            combined += '/';
        combined += relative;

        std::vector<std::string_view> segments;
        std::string_view rest = combined;
        while (!rest.empty())
        {
            const size_t slash = rest.find('/');
            const std::string_view segment = rest.substr(0, slash);
            rest = slash == std::string_view::npos ? std::string_view() : rest.substr(slash + 1);
            if (segment.empty() || segment == ".")
                continue;
            if (segment == ".." && !segments.empty() && segments.back() != "..")
                segments.pop_back();
            else
                segments.push_back(segment);
        }

        std::string result;
        for (const std::string_view segment : segments)
        {
            if (!result.empty())
                result += '/';
            result += segment;
        }
        return result;
    }

    LinkTarget ResolveLink(std::string_view from, std::string_view target, std::string_view sourceUrl)
    {
        LinkTarget link;
        if (target.find("://") != std::string_view::npos || StartsWith(target, "mailto:"))
        {
            link.Url = std::string(target);
            return link;
        }

        const size_t hash = target.find('#');
        const std::string_view path = target.substr(0, hash);
        if (hash != std::string_view::npos)
            link.Anchor = std::string(target.substr(hash + 1));
        if (path.empty())
        {
            link.Kind = LinkKind::Document;
            link.Path = std::string(from);
            return link;
        }

        const std::string resolved = ResolvePath(from, path);
        if (!StartsWith(resolved, "..") && EndsWith(resolved, ".md"))
        {
            link.Kind = LinkKind::Document;
            link.Path = resolved;
            return link;
        }
        // A file of the repository: its page on the repository's site.
        const std::string inRepository = ResolvePath(std::string(DocsFolder) + "/", resolved);
        link.Url = std::string(sourceUrl) + "/" + inRepository;
        if (!link.Anchor.empty())
            link.Url += "#" + link.Anchor;
        link.Anchor.clear();
        return link;
    }

    bool DocLibrary::Load(const std::filesystem::path& root)
    {
        m_Root = root;
        m_Entries.clear();
        m_Documents.clear();

        std::error_code error;
        for (std::filesystem::recursive_directory_iterator it(root, error), end; !error && it != end;
             it.increment(error))
        {
            if (!it->is_regular_file(error) || it->path().extension() != ".md")
                continue;
            DocEntry entry;
            entry.Path = it->path().lexically_relative(root).generic_string();
            entry.IsInSubfolder = entry.Path.find('/') != std::string::npos;

            // The title is the first level-1 heading; reading up to it is enough.
            std::ifstream file(it->path());
            std::string line;
            while (std::getline(file, line))
            {
                if (StartsWith(line, "# "))
                {
                    std::vector<std::string> links;
                    entry.Title = GetPlainText(ParseInline(line.substr(2), links));
                    break;
                }
            }
            if (entry.Title.empty())
                entry.Title = it->path().stem().string();
            m_Entries.push_back(std::move(entry));
        }

        std::sort(m_Entries.begin(), m_Entries.end(),
                  [](const DocEntry& a, const DocEntry& b)
                  {
                      const std::string folderA = GetFolder(a.Path);
                      const std::string folderB = GetFolder(b.Path);
                      if (a.IsInSubfolder != b.IsInSubfolder)
                          return !a.IsInSubfolder;
                      if (folderA != folderB)
                          return folderA < folderB;
                      if (GetRank(a) != GetRank(b))
                          return GetRank(a) < GetRank(b);
                      return a.Title < b.Title;
                  });
        m_Documents.resize(m_Entries.size());
        return !m_Entries.empty();
    }

    const DocEntry* DocLibrary::Find(std::string_view path) const
    {
        for (const DocEntry& entry : m_Entries)
        {
            if (entry.Path == path)
                return &entry;
        }
        return nullptr;
    }

    const Document& DocLibrary::GetDocument(std::string_view path)
    {
        static const Document empty;
        const DocEntry* entry = Find(path);
        if (entry == nullptr)
            return empty;
        std::unique_ptr<Document>& document = m_Documents[static_cast<size_t>(entry - m_Entries.data())];
        if (!document)
            document = std::make_unique<Document>(ParseMarkdown(ReadFile(m_Root / entry->Path)));
        return *document;
    }

    std::string ReadFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return {};
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
} // namespace WebApp
