#include <string>
#include <vector>

#include "Support/WidgetTest.h"

namespace Carbon
{
    namespace
    {
        const Rect Card(10.0f, 10.0f, 100.0f, 40.0f);
        const Rect Bin(300.0f, 10.0f, 200.0f, 200.0f);
        // A target inside the bin.
        const Rect Slot(320.0f, 30.0f, 50.0f, 50.0f);
    } // namespace

    class DragDropTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                // The card's content comes first; the card is known to be a source once its rectangle is.
                if (m_HasButton)
                {
                    SetCursorPos(Card.GetMin());
                    m_ButtonClicked = Button("Inside") || m_ButtonClicked;
                }
                m_CardID = GetID("card");
                if (m_HasSource && BeginDragSource(m_CardID, Card))
                {
                    SetDragPayload(m_PayloadType, m_PayloadValue);
                    m_IsPreviewBuilt = true;
                    m_Preview = AllocateItem(Vec2(60.0f, 20.0f));
                    EndDragSource();
                }
                Record(m_BinDrop, AcceptDrop(GetID("bin"), Bin, "Task"), m_BinDeliveries);
                if (m_HasSlot)
                    Record(m_SlotDrop, AcceptDrop(GetID("slot"), Slot, "Task"), m_SlotDeliveries);
                Record(m_OtherDrop, AcceptDrop(GetID("other"), Bin, "Photo"), m_OtherDeliveries);
                Record(m_FileDrop, AcceptDrop(GetID("files"), Bin, FilesPayloadType), m_FileDeliveries);
            };
        }

        void Record(Drop& last, const Drop& drop, int& deliveries)
        {
            last = drop;
            if (drop.IsDelivered)
            {
                deliveries++;
                m_Delivered = drop.Payload.As<int>();
                m_DeliveredFiles.clear();
                for (const std::string_view file : drop.Payload.Files)
                    m_DeliveredFiles.emplace_back(file);
            }
        }

        /// Presses on the card and moves the pointer to `to`, keeping the button down.
        void StartDrag(Vec2 to)
        {
            MoveMouse(Card.GetCenter(), Interface());
            PressMouse(Interface());
            MoveMouse(to, Interface());
        }

        ID m_CardID;
        bool m_HasSource = true;
        bool m_HasButton = false;
        bool m_HasSlot = false;
        std::string_view m_PayloadType = "Task";
        int m_PayloadValue = 42;
        bool m_IsPreviewBuilt = false;
        Rect m_Preview;
        bool m_ButtonClicked = false;
        Drop m_BinDrop;
        Drop m_SlotDrop;
        Drop m_OtherDrop;
        Drop m_FileDrop;
        int m_BinDeliveries = 0;
        int m_SlotDeliveries = 0;
        int m_OtherDeliveries = 0;
        int m_FileDeliveries = 0;
        int m_Delivered = 0;
        std::vector<std::string> m_DeliveredFiles;
    };

    TEST_F(DragDropTests, ADragBeginsPastTheThreshold)
    {
        Settle(Interface(), 2);
        MoveMouse(Card.GetCenter(), Interface());
        PressMouse(Interface());
        MoveMouse(Card.GetCenter() + Vec2(3.0f, 0.0f), Interface());
        EXPECT_FALSE(IsDragging()) << "three points are less than the threshold";
        MoveMouse(Card.GetCenter() + Vec2(6.0f, 0.0f), Interface());
        EXPECT_TRUE(IsDragging());
        EXPECT_TRUE(m_IsPreviewBuilt);
        EXPECT_EQ(GetDragSourceID(), m_CardID);
        EXPECT_EQ(GetDragPayload().Type, "Task");
        // The preview keeps the distance from the pointer at which the card was grabbed: here, its center.
        Frame(Interface());
        EXPECT_NEAR(m_Preview.X, Card.X + 6.0f + 8.0f, 0.5f);
        ReleaseMouse(Interface());
        EXPECT_FALSE(IsDragging());
        EXPECT_EQ(m_BinDeliveries, 0) << "released outside every target";
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(DragDropTests, TheTargetOfTheTypeReceivesTheDrop)
    {
        Settle(Interface(), 2);
        StartDrag(Bin.GetCenter());
        EXPECT_TRUE(m_BinDrop.IsHovered);
        EXPECT_FALSE(m_BinDrop.IsDelivered);
        EXPECT_EQ(m_BinDrop.Payload.As<int>(), 42);
        EXPECT_FALSE(m_OtherDrop.IsHovered) << "a target for another type ignores the drag";
        EXPECT_FALSE(m_FileDrop.IsHovered);

        ReleaseMouse(Interface());
        EXPECT_EQ(m_BinDeliveries, 1);
        EXPECT_EQ(m_Delivered, 42);
        EXPECT_EQ(m_OtherDeliveries, 0);
        EXPECT_FALSE(IsDragging());
        Frame(Interface());
        EXPECT_EQ(m_BinDeliveries, 1) << "a drop is delivered once";
    }

    TEST_F(DragDropTests, TheSmallestOfOverlappingTargetsWins)
    {
        m_HasSlot = true;
        Settle(Interface(), 2);
        StartDrag(Slot.GetCenter());
        EXPECT_TRUE(m_SlotDrop.IsHovered);
        EXPECT_FALSE(m_BinDrop.IsHovered);
        MoveMouse(Vec2(450.0f, 150.0f), Interface());
        EXPECT_FALSE(m_SlotDrop.IsHovered);
        EXPECT_TRUE(m_BinDrop.IsHovered);
        MoveMouse(Slot.GetCenter(), Interface());
        ReleaseMouse(Interface());
        EXPECT_EQ(m_SlotDeliveries, 1);
        EXPECT_EQ(m_BinDeliveries, 0);
    }

    TEST_F(DragDropTests, EscapeCancelsADrag)
    {
        Settle(Interface(), 2);
        StartDrag(Bin.GetCenter());
        ASSERT_TRUE(IsDragging());
        TapKey(Key::Escape, Interface());
        EXPECT_FALSE(IsDragging());
        EXPECT_FALSE(m_BinDrop.IsHovered);
        ReleaseMouse(Interface());
        EXPECT_EQ(m_BinDeliveries, 0);
        // The pointer is free again: the next press can start a drag.
        StartDrag(Bin.GetCenter());
        EXPECT_TRUE(IsDragging());
    }

    TEST_F(DragDropTests, CancelDragEndsItFromCode)
    {
        Settle(Interface(), 2);
        StartDrag(Bin.GetCenter());
        CancelDrag();
        Frame(Interface());
        ReleaseMouse(Interface());
        EXPECT_FALSE(IsDragging());
        EXPECT_EQ(m_BinDeliveries, 0);
    }

    TEST_F(DragDropTests, TheDragOutlivesItsSource)
    {
        Settle(Interface(), 2);
        StartDrag(Vec2(200.0f, 100.0f));
        m_HasSource = false;
        MoveMouse(Bin.GetCenter(), Interface());
        EXPECT_TRUE(IsDragging());
        EXPECT_TRUE(m_BinDrop.IsHovered);
        ReleaseMouse(Interface());
        EXPECT_EQ(m_BinDeliveries, 1);
        EXPECT_EQ(m_Delivered, 42) << "the payload was copied when it was attached";
    }

    TEST_F(DragDropTests, AnItemInsideASourceKeepsItsClicks)
    {
        m_HasButton = true;
        Settle(Interface(), 2);
        Click(Card.GetMin() + Vec2(10.0f, 10.0f), Interface());
        EXPECT_TRUE(m_ButtonClicked);
        // A drag that starts on the button is the button's: nothing is dragged.
        MoveMouse(Card.GetMin() + Vec2(10.0f, 10.0f), Interface());
        PressMouse(Interface());
        MoveMouse(Bin.GetCenter(), Interface());
        EXPECT_FALSE(IsDragging());
        ReleaseMouse(Interface());
    }

    TEST_F(DragDropTests, DroppedFilesReachTheTargetUnderThem)
    {
        Settle(Interface(), 2);
        const std::string_view paths[] = {"C:/Photos/Beach.jpg", "C:/Photos/Snow.png"};
        GetIO().AddFileDropEvent(Bin.GetCenter().X, Bin.GetCenter().Y, paths);
        Frame(Interface());
        EXPECT_TRUE(IsDragging());
        EXPECT_EQ(GetDragPayload().Type, FilesPayloadType);
        EXPECT_EQ(m_FileDeliveries, 0) << "the target is found during the frame of the drop";
        EXPECT_TRUE(IsAnimating()) << "the drop needs another frame";
        Frame(Interface());
        EXPECT_EQ(m_FileDeliveries, 1);
        ASSERT_EQ(m_DeliveredFiles.size(), 2u);
        EXPECT_EQ(m_DeliveredFiles[0], "C:/Photos/Beach.jpg");
        EXPECT_EQ(m_DeliveredFiles[1], "C:/Photos/Snow.png");
        EXPECT_EQ(m_BinDeliveries, 0) << "files are no task";
        EXPECT_FALSE(IsDragging());
    }

    TEST_F(DragDropTests, FilesDraggedOverTheWindowHighlightTargets)
    {
        Settle(Interface(), 2);
        const std::string_view paths[] = {"/home/ada/notes.txt"};
        GetIO().AddFileDragEvent(Bin.GetCenter().X, Bin.GetCenter().Y, paths);
        Frame(Interface());
        Frame(Interface());
        EXPECT_TRUE(m_FileDrop.IsHovered);
        ASSERT_EQ(m_FileDrop.Payload.Files.size(), 1u);
        EXPECT_EQ(m_FileDrop.Payload.Files[0], "/home/ada/notes.txt");
        GetIO().AddFileDragLeaveEvent();
        Frame(Interface());
        EXPECT_FALSE(IsDragging());
        EXPECT_FALSE(m_FileDrop.IsHovered);
        EXPECT_EQ(m_FileDeliveries, 0);
    }

    TEST_F(DragDropTests, ADragNearTheEdgeOfAScrollViewScrollsIt)
    {
        float offset = 0.0f;
        const Builder build = [&]
        {
            BeginScrollView("scroll", {.Width = 200.0f, .Height = 200.0f, .Spacing = 0.0f});
            for (int i = 0; i < 40; i++)
            {
                PushID(i);
                const Rect row = AllocateItem(Vec2(200.0f, 20.0f));
                if (i == 0 && BeginDragSource(GetID("row"), row))
                {
                    SetDragPayload("Row", i);
                    EndDragSource();
                }
                PopID();
            }
            EndScrollView();
            offset = GetScrollOffset("scroll").Y;
        };
        Settle(build, 2);
        MoveMouse(Vec2(100.0f, 10.0f), build);
        PressMouse(build);
        MoveMouse(Vec2(100.0f, 100.0f), build);
        ASSERT_TRUE(IsDragging());
        Settle(build, 10);
        EXPECT_EQ(offset, 0.0f) << "the middle of the view does not scroll";
        MoveMouse(Vec2(100.0f, 195.0f), build);
        Settle(build, 30);
        EXPECT_GT(offset, 100.0f);
        const float scrolled = offset;
        MoveMouse(Vec2(100.0f, 5.0f), build);
        Settle(build, 10);
        EXPECT_LT(offset, scrolled);
        ReleaseMouse(build);
    }
} // namespace Carbon
