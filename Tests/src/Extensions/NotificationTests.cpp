#include <Carbon/Extensions/Extensions.h>

#include <string>
#include <vector>

#include "Support/WidgetTest.h"

namespace Carbon
{
    // Notifications are 340 points wide and keep 16 points from the edges: at the top right they span x 444 to 784
    // and start at y 16.
    class NotificationTests : public WidgetTest
    {
    protected:
        void TearDown() override
        {
            DismissAllNotifications();
            WidgetTest::TearDown();
        }

        Builder Interface()
        {
            return [this]
            {
                for (const NotificationEvent& event : ShowNotifications(m_Options))
                    m_Events.push_back(event);
                m_OverlayIndices = GetDrawList().GetIndices(DrawLayer::Overlay, MaxOverlayDepth - 1).size();
            };
        }

        // Runs frames of `seconds` with the pointer where it is.
        void Wait(float seconds)
        {
            for (float time = 0.0f; time < seconds; time += 0.1f)
                Frame(Interface(), 0.1f);
        }

        NotificationCenterOptions m_Options;
        std::vector<NotificationEvent> m_Events;
        size_t m_OverlayIndices = 0;
    };

    TEST_F(NotificationTests, PostedNotificationsAreShown)
    {
        Settle(Interface());
        EXPECT_EQ(m_OverlayIndices, 0u);
        const ID id = PostNotification("Hello", {.Body = "A message.", .Style = NotificationStyle::Info});
        EXPECT_TRUE(id.IsValid());
        EXPECT_EQ(GetNotificationCount(), 1);
        Settle(Interface());
        EXPECT_GT(m_OverlayIndices, 0u);
        EXPECT_TRUE(IsAnimating()) << "a counting notification keeps frames coming";
    }

    TEST_F(NotificationTests, ExpiresAfterItsDuration)
    {
        PostNotification("Hello", {.Duration = 1.0f, .Tag = 7});
        Wait(0.5f);
        EXPECT_EQ(GetNotificationCount(), 1);
        Wait(0.8f);
        ASSERT_EQ(m_Events.size(), 1u);
        EXPECT_EQ(m_Events[0].Kind, NotificationEventKind::Expired);
        EXPECT_EQ(m_Events[0].Tag, 7u);
        Wait(0.5f);
        EXPECT_EQ(GetNotificationCount(), 0) << "gone once it has faded";
    }

    TEST_F(NotificationTests, StaysWithoutADuration)
    {
        PostNotification("Hello", {.Duration = 0.0f});
        Wait(30.0f);
        EXPECT_EQ(GetNotificationCount(), 1);
        EXPECT_TRUE(m_Events.empty());
    }

    TEST_F(NotificationTests, TheTimeStopsUnderThePointer)
    {
        PostNotification("Hello", {.Duration = 1.0f});
        Settle(Interface());
        MoveMouse(Vec2(600.0f, 30.0f), Interface());
        Wait(3.0f);
        EXPECT_EQ(GetNotificationCount(), 1);
        MoveMouse(Vec2(100.0f, 500.0f), Interface());
        Wait(2.0f);
        EXPECT_EQ(GetNotificationCount(), 0);
    }

    TEST_F(NotificationTests, ClickReportsAndDismisses)
    {
        const ID id = PostNotification("Hello", {.Duration = 0.0f, .Tag = 3});
        Settle(Interface());
        Click(Vec2(600.0f, 30.0f), Interface());
        ASSERT_EQ(m_Events.size(), 1u);
        EXPECT_EQ(m_Events[0].Kind, NotificationEventKind::Clicked);
        EXPECT_EQ(m_Events[0].Notification, id);
        Settle(Interface());
        EXPECT_EQ(GetNotificationCount(), 0);
    }

    TEST_F(NotificationTests, ActionButtonsReportTheirAction)
    {
        PostNotification("Hello", {.PrimaryAction = "Reply", .Duration = 0.0f});
        Settle(Interface());
        // The card is 12 + 16 (title) + 8 + 24 (actions) + 12 = 72 points high, from y 16 to 88. The primary action
        // is in the bottom row at the trailing edge: from y 52 to 76, ending 12 points inside the card's right edge.
        Click(Vec2(784.0f - 20.0f, 64.0f), Interface());
        ASSERT_EQ(m_Events.size(), 1u);
        EXPECT_EQ(m_Events[0].Kind, NotificationEventKind::PrimaryAction);
    }

    TEST_F(NotificationTests, CloseButtonAppearsUnderThePointer)
    {
        PostNotification("Hello", {.Duration = 0.0f});
        Settle(Interface());
        MoveMouse(Vec2(600.0f, 30.0f), Interface());
        Settle(Interface());
        // At the top-left corner of the card.
        Click(Vec2(444.0f + 5.4f, 16.0f + 5.4f), Interface());
        ASSERT_EQ(m_Events.size(), 1u);
        EXPECT_EQ(m_Events[0].Kind, NotificationEventKind::Dismissed);
    }

    TEST_F(NotificationTests, DismissFromCode)
    {
        const ID first = PostNotification("First");
        PostNotification("Second");
        DismissNotification(first);
        Settle(Interface(), 40);
        EXPECT_EQ(GetNotificationCount(), 1);
        DismissAllNotifications();
        Settle(Interface(), 40);
        EXPECT_EQ(GetNotificationCount(), 0);
        EXPECT_TRUE(m_Events.empty()) << "dismissing from code reports nothing";
    }

    TEST_F(NotificationTests, PositionsAndStacking)
    {
        // Bottom left: the newest sits at the bottom edge, the older one above it.
        m_Options.Position = NotificationPosition::BottomLeft;
        PostNotification("Older", {.Duration = 0.0f, .Tag = 1});
        PostNotification("Newer", {.Duration = 0.0f, .Tag = 2});
        Settle(Interface(), 60);
        Click(Vec2(100.0f, 600.0f - 16.0f - 10.0f), Interface());
        ASSERT_EQ(m_Events.size(), 1u);
        EXPECT_EQ(m_Events[0].Tag, 2u);
        Settle(Interface(), 60);
        // The older one has moved down into its place.
        Click(Vec2(100.0f, 600.0f - 16.0f - 10.0f), Interface());
        ASSERT_EQ(m_Events.size(), 2u);
        EXPECT_EQ(m_Events[1].Tag, 1u);

        // A notification can ask for its own position.
        PostNotification("Centered", {.Duration = 0.0f, .Position = NotificationPosition::TopCenter, .Tag = 3});
        Settle(Interface(), 60);
        Click(Vec2(400.0f, 26.0f), Interface());
        ASSERT_EQ(m_Events.size(), 3u);
        EXPECT_EQ(m_Events[2].Tag, 3u);
    }

    TEST_F(NotificationTests, StacksShowAtMostMaxVisible)
    {
        m_Options.MaxVisible = 2;
        PostNotification("One", {.Duration = 0.0f});
        Settle(Interface());
        const size_t one = m_OverlayIndices;
        PostNotification("Two", {.Duration = 0.0f});
        PostNotification("Three", {.Duration = 0.0f});
        Settle(Interface(), 60);
        EXPECT_EQ(GetNotificationCount(), 3);
        EXPECT_LT(m_OverlayIndices, one * 3) << "the third waits";
    }

    TEST_F(NotificationTests, LongTextIsCutAtACharacterBoundary)
    {
        std::string title;
        for (int i = 0; i < 200; i++)
            title += "\xC3\xA4"; // two bytes per character
        PostNotification(title, {.Body = std::string(2000, 'x'), .Duration = 0.0f});
        Settle(Interface());
        EXPECT_TRUE(m_AssertMessages.empty());
        EXPECT_GT(m_OverlayIndices, 0u);
    }

    TEST_F(NotificationTests, TheOldestMakesRoom)
    {
        for (int i = 0; i < 40; i++)
            PostNotification("Many", {.Duration = 0.0f});
        EXPECT_EQ(GetNotificationCount(), 32);
    }
} // namespace Carbon
