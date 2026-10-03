#include "Support/ContextTest.h"

#include <unordered_set>

namespace Carbon
{
    TEST(IDHashTests, IsDeterministicAndNeverZero)
    {
        EXPECT_EQ(HashID("Save"), HashID("Save"));
        EXPECT_NE(HashID("Save"), HashID("Cancel"));
        EXPECT_TRUE(HashID("").IsValid());
        EXPECT_TRUE(HashID(int64_t(0)).IsValid());
        EXPECT_FALSE(ID().IsValid());
    }

    TEST(IDHashTests, SeedChangesTheResult)
    {
        const ID parentA = HashID("WindowA");
        const ID parentB = HashID("WindowB");
        EXPECT_NE(HashID("OK", parentA), HashID("OK", parentB));
        EXPECT_NE(HashID("OK", parentA), HashID("OK"));
    }

    TEST(IDHashTests, TripleHashFixesTheID)
    {
        // Everything before "###" is display text only.
        EXPECT_EQ(HashID("Frame 1###counter"), HashID("Frame 2###counter"));
        EXPECT_EQ(HashID("###counter"), HashID("Anything###counter"));
        EXPECT_NE(HashID("Label###a"), HashID("Label###b"));
    }

    TEST(IDHashTests, DoubleHashDisambiguates)
    {
        EXPECT_NE(HashID("Delete##row1"), HashID("Delete##row2"));
        EXPECT_EQ(GetDisplayLabel("Delete##row1"), "Delete");
        EXPECT_EQ(GetDisplayLabel("Frame 1###counter"), "Frame 1");
        EXPECT_EQ(GetDisplayLabel("Plain"), "Plain");
        EXPECT_EQ(GetDisplayLabel("##hidden"), "");
    }

    TEST(IDHashTests, IntegersAndStringsDoNotCollide)
    {
        EXPECT_NE(HashID(int64_t(1)), HashID("1"));
        EXPECT_NE(HashID(int64_t(1)), HashID(int64_t(2)));
        EXPECT_NE(HashID(int64_t(-1)), HashID(int64_t(1)));
    }

    TEST(IDHashTests, NoCollisionsAcrossManyLabels)
    {
        std::unordered_set<ID> seen;
        for (int64_t scope = 0; scope < 50; scope++)
        {
            const ID parent = HashID(scope);
            for (int64_t item = 0; item < 200; item++)
                EXPECT_TRUE(seen.insert(HashID(item, parent)).second);
        }
    }

    using IDStackTests = ContextTest;

    TEST_F(IDStackTests, PushedScopesChangeIDs)
    {
        NewFrame();
        const ID root = GetID("Button");

        PushID("Sidebar");
        const ID inSidebar = GetID("Button");
        PushID(3);
        const ID inRow = GetID("Button");
        PopID();
        EXPECT_EQ(GetID("Button"), inSidebar);
        PopID();

        EXPECT_EQ(GetID("Button"), root);
        EXPECT_NE(root, inSidebar);
        EXPECT_NE(inSidebar, inRow);
        EndFrame();
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(IDStackTests, IDsAreStableAcrossFrames)
    {
        NewFrame();
        PushID("List");
        const ID first = GetID(7);
        PopID();
        EndFrame();

        NewFrame();
        PushID("List");
        EXPECT_EQ(GetID(7), first);
        PopID();
        EndFrame();
    }

    TEST_F(IDStackTests, UnbalancedPushIsReportedAtEndFrame)
    {
        NewFrame();
        PushID("Leaked");
        EndFrame();
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("Unbalanced ID stack"), std::string::npos);

        // The stack recovers, so the next frame is clean.
        m_AssertMessages.clear();
        NewFrame();
        EndFrame();
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(IDStackTests, PopWithoutPushIsReportedAndIgnored)
    {
        NewFrame();
        const ID before = GetID("Item");
        PopID();
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("PopID"), std::string::npos);
        EXPECT_EQ(GetID("Item"), before);
        EndFrame();
    }
} // namespace Carbon
