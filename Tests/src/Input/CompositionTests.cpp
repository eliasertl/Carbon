#include "Support/ContextTest.h"

#include <string>
#include <vector>

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/UTF8.h"

namespace Carbon
{
    // The input method composition as the host drives it through IO: the state Carbon keeps after each frame,
    // and how composition events take their place in the input queue. TextCompositionTests covers what the text
    // controls do with it.
    class CompositionTests : public ContextTest
    {
    protected:
        const Internal::CompositionState& GetComposition() { return Internal::GetContext().Input.Composition; }

        // The characters typed or committed in the current frame, as UTF-8.
        std::string GetText()
        {
            std::string text;
            for (const char32_t character : GetInputCharacters())
                AppendUTF8(text, character);
            return text;
        }
    };

    // "か" (ka), "かん" (kan) and "漢" (kan, converted), in UTF-8.
    constexpr std::string_view Ka = "\xE3\x81\x8B";
    constexpr std::string_view KaN = "\xE3\x81\x8B\xE3\x82\x93";
    constexpr std::string_view Kan = "\xE6\xBC\xA2";

    TEST_F(CompositionTests, AnUpdateStartsTheCompositionAndReplacesItsText)
    {
        IO& io = GetIO();
        io.AddCompositionUpdateEvent(Ka, Ka.size());
        NewFrame();
        EXPECT_TRUE(GetComposition().IsActive);
        EXPECT_EQ(GetComposition().Text, Ka);
        EXPECT_EQ(GetComposition().Caret, 3u);
        EXPECT_TRUE(GetInputCharacters().empty()) << "pre-edit text is not typed text";
        EndFrame();

        io.AddCompositionUpdateEvent(KaN, 3);
        NewFrame();
        EXPECT_EQ(GetComposition().Text, KaN);
        EXPECT_EQ(GetComposition().Caret, 3u);
        EndFrame();
    }

    TEST_F(CompositionTests, ACommitIsTypedTextAndEndsTheComposition)
    {
        IO& io = GetIO();
        io.AddCompositionStartEvent();
        io.AddCompositionUpdateEvent(KaN, KaN.size());
        NewFrame();
        EXPECT_TRUE(GetComposition().IsActive);
        EndFrame();

        io.AddCompositionCommitEvent(Kan);
        NewFrame();
        EXPECT_FALSE(GetComposition().IsActive);
        EXPECT_TRUE(GetComposition().Text.empty());
        EXPECT_EQ(GetText(), Kan);
        EndFrame();

        NewFrame();
        EXPECT_TRUE(GetInputCharacters().empty());
        EndFrame();
    }

    TEST_F(CompositionTests, ACancelEndsTheCompositionWithoutText)
    {
        IO& io = GetIO();
        io.AddCompositionUpdateEvent(Ka, Ka.size());
        NewFrame();
        EndFrame();

        io.AddCompositionCancelEvent();
        NewFrame();
        EXPECT_FALSE(GetComposition().IsActive);
        EXPECT_TRUE(GetInputCharacters().empty());
        EndFrame();
    }

    TEST_F(CompositionTests, AStartBeginsAnEmptyComposition)
    {
        GetIO().AddCompositionStartEvent();
        NewFrame();
        EXPECT_TRUE(GetComposition().IsActive);
        EXPECT_TRUE(GetComposition().Text.empty());
        EndFrame();
    }

    TEST_F(CompositionTests, EventsWithoutACompositionAreHarmless)
    {
        // What an input method sends after it was told to cancel: an empty update and the end.
        IO& io = GetIO();
        io.AddCompositionUpdateEvent("", 0);
        io.AddCompositionCancelEvent();
        NewFrame();
        EXPECT_FALSE(GetComposition().IsActive);
        EndFrame();

        // A commit without a composition still inserts its text; some input methods only ever commit.
        io.AddCompositionCommitEvent("abc");
        NewFrame();
        EXPECT_EQ(GetText(), "abc");
        EndFrame();
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(CompositionTests, ACommitDropsControlCharacters)
    {
        GetIO().AddCompositionCommitEvent("a\nb\x7F");
        NewFrame();
        EXPECT_EQ(GetText(), "ab");
        EndFrame();
    }

    TEST_F(CompositionTests, ClausesAreKeptInOrderOnCharacterBoundaries)
    {
        // "漢字に" with the clauses "漢字" (active) and "に".
        const std::string text = "\xE6\xBC\xA2\xE5\xAD\x97\xE3\x81\xAB";
        const CompositionClause clauses[] = {{0, 6, true}, {6, 9, false}};
        GetIO().AddCompositionUpdateEvent(text, 6, clauses);
        NewFrame();
        const Internal::CompositionState& composition = GetComposition();
        ASSERT_EQ(composition.Clauses.size(), 2u);
        EXPECT_EQ(composition.Clauses[0].Start, 0u);
        EXPECT_EQ(composition.Clauses[0].End, 6u);
        EXPECT_TRUE(composition.Clauses[0].IsActive);
        EXPECT_EQ(composition.Clauses[1].Start, 6u);
        EXPECT_EQ(composition.Clauses[1].End, 9u);
        EXPECT_FALSE(composition.Clauses[1].IsActive);
        EndFrame();
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(CompositionTests, OffsetsInsideCharactersMoveBackAndOutsideTheTextAreReported)
    {
        // Offsets in the middle of a character, as a host that counts UTF-16 units might send by mistake.
        const std::string text = "\xE6\xBC\xA2\xE5\xAD\x97"; // 漢字
        const CompositionClause clauses[] = {{1, 4, false}, {4, 4, false}};
        GetIO().AddCompositionUpdateEvent(text, 2, clauses);
        NewFrame();
        EXPECT_EQ(GetComposition().Caret, 0u);
        ASSERT_EQ(GetComposition().Clauses.size(), 1u) << "the empty clause is dropped";
        EXPECT_EQ(GetComposition().Clauses[0].Start, 0u);
        EXPECT_EQ(GetComposition().Clauses[0].End, 3u);
        EndFrame();
        EXPECT_TRUE(m_AssertMessages.empty());

        // Past the end is a mistake the host should hear about; Carbon carries on with what fits.
        const CompositionClause outside[] = {{0, 9, true}};
        GetIO().AddCompositionUpdateEvent(text, 10, outside);
        NewFrame();
        EXPECT_EQ(GetComposition().Caret, text.size());
        ASSERT_EQ(GetComposition().Clauses.size(), 1u);
        EXPECT_EQ(GetComposition().Clauses[0].End, text.size());
        EndFrame();
        EXPECT_EQ(m_AssertMessages.size(), 2u);
    }

    TEST_F(CompositionTests, ManyUpdatesInOneFrameLeaveTheLast)
    {
        IO& io = GetIO();
        io.AddCompositionUpdateEvent("k", 1);
        io.AddCompositionUpdateEvent(Ka, Ka.size());
        io.AddCompositionUpdateEvent(KaN, KaN.size());
        NewFrame();
        EXPECT_EQ(GetComposition().Text, KaN);
        EndFrame();
    }

    TEST_F(CompositionTests, ACommitAndTheNextCompositionShareAFrame)
    {
        // A Korean input method commits a syllable and starts the next one with the same key.
        IO& io = GetIO();
        io.AddCompositionUpdateEvent("\xED\x95\x9C", 3); // 한
        io.AddCompositionCommitEvent("\xED\x95\x9C");
        io.AddCompositionUpdateEvent("\xE3\x84\xB1", 3); // ㄱ
        NewFrame();
        EXPECT_EQ(GetText(), "\xED\x95\x9C");
        EXPECT_TRUE(GetComposition().IsActive);
        EXPECT_EQ(GetComposition().Text, "\xE3\x84\xB1");
        EndFrame();
    }

    TEST_F(CompositionTests, KeepsItsOrderRelativeToEditingKeysAndCharacters)
    {
        // Backspace, a composition, Enter: each in the order it came, one frame apart where order matters.
        IO& io = GetIO();
        io.AddKeyEvent(Key::Backspace, true);
        io.AddKeyEvent(Key::Backspace, false);
        io.AddCompositionUpdateEvent(Ka, Ka.size());
        io.AddCompositionCommitEvent(Ka);
        io.AddInputCharacter(U'x');
        io.AddKeyEvent(Key::Enter, true);
        io.AddKeyEvent(Key::Enter, false);

        std::vector<std::string> frames;
        for (int i = 0; i < 6; i++)
        {
            NewFrame();
            std::string frame;
            if (IsKeyPressed(Key::Backspace))
                frame += "<";
            if (GetComposition().IsActive)
                frame += "[" + GetComposition().Text + "]";
            frame += GetText();
            if (IsKeyPressed(Key::Enter))
                frame += "!";
            frames.push_back(frame);
            EndFrame();
        }
        const std::string committed = std::string(Ka) + "x";
        EXPECT_EQ(frames[0], "<");
        EXPECT_EQ(frames[1], committed) << "the update and the commit share the frame after Backspace";
        EXPECT_EQ(frames[2], "!");
    }

    TEST_F(CompositionTests, TheQueueKeepsTheTextOfDeferredEvents)
    {
        // The update waits a frame behind Backspace; its text must survive until then.
        IO& io = GetIO();
        io.AddKeyEvent(Key::Backspace, true);
        io.AddCompositionUpdateEvent(KaN, 3);
        io.AddKeyEvent(Key::Backspace, false);
        NewFrame();
        EXPECT_FALSE(GetComposition().IsActive);
        EndFrame();
        NewFrame();
        EXPECT_TRUE(GetComposition().IsActive);
        EXPECT_EQ(GetComposition().Text, KaN);
        EndFrame();
    }

    TEST_F(CompositionTests, ACompositionNoTextFieldShowsIsDroppedAndTheHostAskedToCancel)
    {
        // No text field is being edited: nothing can show the pre-edit text or take the commit.
        GetIO().AddCompositionUpdateEvent(Ka, Ka.size());
        NewFrame();
        EXPECT_TRUE(GetComposition().IsActive);
        EndFrame();
        EXPECT_FALSE(GetComposition().IsActive);
        EXPECT_TRUE(GetIO().WantsCompositionCancel());
        EXPECT_FALSE(GetIO().WantsTextInput());
        EXPECT_TRUE(GetIO().GetCaretRect().IsEmpty());

        // The request lasts one frame.
        RunFrame();
        EXPECT_FALSE(GetIO().WantsCompositionCancel());
    }
} // namespace Carbon
