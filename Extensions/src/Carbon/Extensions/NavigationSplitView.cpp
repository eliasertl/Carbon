#include "Carbon/Extensions/NavigationSplitView.h"

#include "Carbon/Extensions/Internal/NavigationStack.h"

namespace Carbon
{
    namespace
    {
        constexpr int MaxDepth = 4;

        // The navigation split views being built, and whether each one is collapsed.
        struct NavigationSplitStack
        {
            uint64_t Frame;
            int Depth;
            bool IsCollapsed[MaxDepth];
            std::string_view DetailTitles[MaxDepth];
        };

        NavigationSplitStack& GetStack()
        {
            NavigationSplitStack& stack =
                *GetState<NavigationSplitStack>(HashID("Carbon.NavigationSplitView.Stack"), StateLifetime::Persistent);
            if (stack.Frame != GetFrameCount())
            {
                stack.Frame = GetFrameCount();
                stack.Depth = 0;
            }
            return stack;
        }
    } // namespace

    void BeginNavigationSplitView(std::string_view id, const NavigationSplitViewOptions& options)
    {
        NavigationSplitStack& stack = GetStack();
        const bool hasRoom = stack.Depth < MaxDepth;
        CB_VERIFY(hasRoom, "Navigation split views can be nested at most {} deep", MaxDepth);
        const bool isCollapsed = IsCompactWidth();
        if (hasRoom)
        {
            stack.IsCollapsed[stack.Depth] = isCollapsed;
            stack.DetailTitles[stack.Depth] = options.DetailTitle;
        }
        stack.Depth++;

        if (isCollapsed)
        {
            Internal::BeginCollapsedNavigation(GetID(id), options.Title, options.Width, options.Height,
                                               options.RootBackTitle);
            return;
        }
        BeginHStack({.Spacing = 0.0f,
                     .Alignment = VerticalAlignment::Top,
                     .Width = options.Width,
                     .Height = options.Height,
                     .ID = id});
    }

    void NavigationSplitViewDetail()
    {
        NavigationSplitStack& stack = GetStack();
        const bool isOpen = stack.Depth > 0 && stack.Depth <= MaxDepth;
        CB_VERIFY(isOpen,
                  "NavigationSplitViewDetail must be called between BeginNavigationSplitView and "
                  "EndNavigationSplitView");
        if (!isOpen)
            return;
        if (stack.IsCollapsed[stack.Depth - 1])
            Internal::CollapsedNavigationDetail(stack.DetailTitles[stack.Depth - 1]);
        else
            BeginVStack({.Spacing = 0.0f, .Width = Size::Fill(), .Height = Size::Fill()});
    }

    bool EndNavigationSplitView()
    {
        NavigationSplitStack& stack = GetStack();
        const bool isOpen = stack.Depth > 0;
        CB_VERIFY(isOpen, "EndNavigationSplitView called without BeginNavigationSplitView");
        if (!isOpen)
            return false;
        stack.Depth--;
        if (stack.Depth < MaxDepth && stack.IsCollapsed[stack.Depth])
            return Internal::EndCollapsedNavigation();
        EndVStack();
        EndHStack();
        return false;
    }

    void ShowNavigationDetail(std::string_view id, bool isShown, bool animated)
    {
        Internal::SetNavigationDetailShown(GetID(id), isShown, animated);
    }

    bool IsNavigationDetailShown(std::string_view id)
    {
        return Internal::IsNavigationDetailShown(GetID(id));
    }
} // namespace Carbon
