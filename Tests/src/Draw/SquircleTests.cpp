#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

#include "Carbon/Draw/Squircle.h"

namespace Carbon
{
    namespace
    {
        // Reference: exact signed distance to a rectangle with circular corners.
        float RoundedRectDistance(Vec2 point, Vec2 halfSize, float radius)
        {
            const float qx = std::abs(point.X) - (halfSize.X - radius);
            const float qy = std::abs(point.Y) - (halfSize.Y - radius);
            const float outside = Vec2(std::max(qx, 0.0f), std::max(qy, 0.0f)).GetLength();
            return outside + std::min(std::max(qx, qy), 0.0f) - radius;
        }

        // Finds where the outline crosses the ray from the center through `direction`, by bisection.
        float FindOutline(Vec2 direction, Vec2 halfSize, float radius, float smoothing)
        {
            float inside = 0.0f;
            float outside = halfSize.GetLength() + 1.0f;
            for (int i = 0; i < 60; i++)
            {
                const float middle = (inside + outside) * 0.5f;
                if (SquircleDistance(direction * middle, halfSize, radius, smoothing) < 0.0f)
                    inside = middle;
                else
                    outside = middle;
            }
            return (inside + outside) * 0.5f;
        }
    } // namespace

    TEST(SquircleTests, SmoothingZeroIsACircularRoundedRect)
    {
        const Vec2 halfSize(60.0f, 30.0f);
        const float radius = 12.0f;
        for (float y = -40.0f; y <= 40.0f; y += 1.7f)
        {
            for (float x = -75.0f; x <= 75.0f; x += 1.3f)
            {
                const Vec2 point(x, y);
                EXPECT_NEAR(SquircleDistance(point, halfSize, radius, 0.0f),
                            RoundedRectDistance(point, halfSize, radius), 1e-4f)
                    << "at (" << x << ", " << y << ")";
            }
        }
        EXPECT_FLOAT_EQ(GetSquircleExponent(radius, GetSquircleExtent(halfSize, radius, 0.0f)), 2.0f);
    }

    TEST(SquircleTests, RadiusZeroIsARectangle)
    {
        const Vec2 halfSize(50.0f, 20.0f);
        EXPECT_FLOAT_EQ(SquircleDistance(Vec2(0.0f, 0.0f), halfSize, 0.0f, 0.6f), -20.0f);
        EXPECT_FLOAT_EQ(SquircleDistance(Vec2(50.0f, 0.0f), halfSize, 0.0f, 0.6f), 0.0f);
        EXPECT_FLOAT_EQ(SquircleDistance(Vec2(49.0f, 19.5f), halfSize, 0.0f, 0.6f), -0.5f);
        EXPECT_GT(SquircleDistance(Vec2(51.0f, 21.0f), halfSize, 0.0f, 0.6f), 0.0f);
    }

    TEST(SquircleTests, OutlineStaysInsideItsBounds)
    {
        const Vec2 halfSize(80.0f, 40.0f);
        for (float smoothing : {0.0f, 0.3f, 0.6f, 1.0f})
        {
            for (float radius : {4.0f, 12.0f, 40.0f})
            {
                for (int step = 0; step < 720; step++)
                {
                    const float angle = float(step) * 3.14159265f / 360.0f;
                    const Vec2 direction(std::cos(angle), std::sin(angle));
                    const Vec2 onOutline = direction * FindOutline(direction, halfSize, radius, smoothing);
                    EXPECT_LE(std::abs(onOutline.X), halfSize.X + 1e-3f);
                    EXPECT_LE(std::abs(onOutline.Y), halfSize.Y + 1e-3f);
                }
                // Anything outside the rectangle is outside the shape.
                EXPECT_GT(SquircleDistance(Vec2(halfSize.X + 0.01f, 0.0f), halfSize, radius, smoothing), 0.0f);
                EXPECT_GT(SquircleDistance(Vec2(0.0f, halfSize.Y + 0.01f), halfSize, radius, smoothing), 0.0f);
                EXPECT_GT(SquircleDistance(halfSize, halfSize, radius, smoothing), 0.0f);
            }
        }
    }

    TEST(SquircleTests, SmoothedCornerLiesInsideTheCircularOne)
    {
        // The smooth corner shares the apex of the circular corner and removes a little more material next to
        // it, so every point inside the squircle is also inside the circular rounded rectangle.
        const Vec2 halfSize(60.0f, 40.0f);
        const float radius = 16.0f;
        for (float y = 20.0f; y <= 40.0f; y += 0.25f)
        {
            for (float x = 30.0f; x <= 60.0f; x += 0.25f)
            {
                const Vec2 point(x, y);
                if (SquircleDistance(point, halfSize, radius, 0.6f) < -1e-3f)
                {
                    EXPECT_LT(RoundedRectDistance(point, halfSize, radius), 1e-3f) << "at (" << x << ", " << y << ")";
                }
            }
        }

        // Shared apex: on the diagonal of the corner both outlines cross at the same point.
        const float diagonal = radius * (1.0f - 0.70710678f);
        const Vec2 apex(halfSize.X - diagonal, halfSize.Y - diagonal);
        EXPECT_NEAR(SquircleDistance(apex, halfSize, radius, 0.6f), 0.0f, 1e-3f);
        EXPECT_NEAR(SquircleDistance(apex, halfSize, radius, 1.0f), 0.0f, 1e-3f);
        EXPECT_NEAR(RoundedRectDistance(apex, halfSize, radius), 0.0f, 1e-3f);
    }

    TEST(SquircleTests, SmoothingLeavesTheEdgeEarlier)
    {
        // With smoothing the corner starts radius * (1 + smoothing) from the corner instead of radius.
        const Vec2 halfSize(100.0f, 50.0f);
        const float radius = 10.0f;
        EXPECT_FLOAT_EQ(GetSquircleExtent(halfSize, radius, 0.0f), 10.0f);
        EXPECT_FLOAT_EQ(GetSquircleExtent(halfSize, radius, 0.6f), 16.0f);
        EXPECT_FLOAT_EQ(GetSquircleExtent(halfSize, radius, 1.0f), 20.0f);

        // A point on the top edge, 13 points from the corner: on the outline of the circular version, but
        // already cut away by the smooth one.
        const Vec2 onEdge(halfSize.X - 13.0f, halfSize.Y);
        EXPECT_NEAR(SquircleDistance(onEdge, halfSize, radius, 0.0f), 0.0f, 1e-4f);
        EXPECT_GT(SquircleDistance(onEdge, halfSize, radius, 0.6f), 0.0f);

        EXPECT_NEAR(GetSquircleExponent(radius, 16.0f), 3.43f, 0.02f);
        EXPECT_NEAR(GetSquircleExponent(radius, 20.0f), 4.38f, 0.02f);
    }

    TEST(SquircleTests, RadiusClampsToHalfTheShortestSide)
    {
        const Vec2 halfSize(50.0f, 12.0f);
        for (float y = -14.0f; y <= 14.0f; y += 0.9f)
        {
            for (float x = -54.0f; x <= 54.0f; x += 1.1f)
            {
                const Vec2 point(x, y);
                EXPECT_FLOAT_EQ(SquircleDistance(point, halfSize, 500.0f, 0.6f),
                                SquircleDistance(point, halfSize, 12.0f, 0.6f));
            }
        }
        // Negative radii clamp to zero.
        EXPECT_FLOAT_EQ(SquircleDistance(Vec2(49.0f, 11.0f), halfSize, -5.0f, 0.6f),
                        SquircleDistance(Vec2(49.0f, 11.0f), halfSize, 0.0f, 0.6f));
    }

    TEST(SquircleTests, PillsAndCirclesHaveCircularEnds)
    {
        // With the radius at its maximum there is no room left to smooth: the ends are exact semicircles.
        const Vec2 pill(40.0f, 10.0f);
        EXPECT_FLOAT_EQ(GetSquircleExtent(pill, 10.0f, 0.6f), 10.0f);
        for (float x = 25.0f; x <= 42.0f; x += 0.5f)
        {
            for (float y = -11.0f; y <= 11.0f; y += 0.5f)
            {
                EXPECT_NEAR(SquircleDistance(Vec2(x, y), pill, 10.0f, 0.6f),
                            RoundedRectDistance(Vec2(x, y), pill, 10.0f), 1e-4f);
            }
        }

        const Vec2 circle(8.0f, 8.0f);
        EXPECT_NEAR(SquircleDistance(Vec2(8.0f, 0.0f), circle, 8.0f, 0.6f), 0.0f, 1e-5f);
        EXPECT_NEAR(SquircleDistance(Vec2(3.0f, 4.0f), circle, 8.0f, 0.6f), -3.0f, 1e-4f);
    }

    TEST(SquircleTests, IsScaleInvariant)
    {
        const Vec2 halfSize(45.0f, 14.0f);
        const float radius = 6.0f;
        for (float scale : {0.5f, 2.0f, 3.5f})
        {
            for (float y = -16.0f; y <= 16.0f; y += 1.3f)
            {
                for (float x = -48.0f; x <= 48.0f; x += 1.9f)
                {
                    const Vec2 point(x, y);
                    const float base = SquircleDistance(point, halfSize, radius, 0.6f);
                    const float scaled = SquircleDistance(point * scale, halfSize * scale, radius * scale, 0.6f);
                    EXPECT_NEAR(scaled, base * scale, 2e-3f * scale);
                }
            }
        }
    }

    TEST(SquircleTests, IsSymmetricAndContinuous)
    {
        const Vec2 halfSize(30.0f, 20.0f);
        const float radius = 8.0f;
        EXPECT_FLOAT_EQ(SquircleDistance(Vec2(25.0f, 17.0f), halfSize, radius, 0.6f),
                        SquircleDistance(Vec2(-25.0f, -17.0f), halfSize, radius, 0.6f));
        EXPECT_FLOAT_EQ(SquircleDistance(Vec2(25.0f, 17.0f), halfSize, radius, 0.6f),
                        SquircleDistance(Vec2(-25.0f, 17.0f), halfSize, radius, 0.6f));

        // No jumps anywhere, in particular not where the corner patch meets the straight edges.
        const float step = 0.01f;
        for (float y = 0.0f; y <= 24.0f; y += 0.37f)
        {
            float previous = SquircleDistance(Vec2(0.0f, y), halfSize, radius, 0.6f);
            for (float x = step; x <= 34.0f; x += step)
            {
                const float current = SquircleDistance(Vec2(x, y), halfSize, radius, 0.6f);
                EXPECT_LT(std::abs(current - previous), 0.05f) << "at (" << x << ", " << y << ")";
                previous = current;
            }
        }
    }

    TEST(SquircleTests, DistanceEstimateIsAccurateNearTheOutline)
    {
        // Step a known distance along the outline's normal and compare with the estimate.
        const Vec2 halfSize(40.0f, 40.0f);
        const float radius = 20.0f;
        for (int step = 1; step < 90; step += 4)
        {
            const float angle = float(step) * 3.14159265f / 180.0f;
            const Vec2 direction(std::cos(angle), std::sin(angle));
            const Vec2 onOutline = direction * FindOutline(direction, halfSize, radius, 0.6f);

            const float epsilon = 0.01f;
            const float dx = SquircleDistance(onOutline + Vec2(epsilon, 0.0f), halfSize, radius, 0.6f) -
                             SquircleDistance(onOutline - Vec2(epsilon, 0.0f), halfSize, radius, 0.6f);
            const float dy = SquircleDistance(onOutline + Vec2(0.0f, epsilon), halfSize, radius, 0.6f) -
                             SquircleDistance(onOutline - Vec2(0.0f, epsilon), halfSize, radius, 0.6f);
            const Vec2 normal = Vec2(dx, dy) / Vec2(dx, dy).GetLength();

            EXPECT_NEAR(SquircleDistance(onOutline + normal * 0.5f, halfSize, radius, 0.6f), 0.5f, 0.03f);
            EXPECT_NEAR(SquircleDistance(onOutline - normal * 0.5f, halfSize, radius, 0.6f), -0.5f, 0.03f);
        }
    }

    TEST(SquircleTests, CoverageIsHalfOnTheOutlineAndOnePixelWide)
    {
        EXPECT_FLOAT_EQ(SquircleCoverage(0.0f, 1.0f), 0.5f);
        EXPECT_FLOAT_EQ(SquircleCoverage(-0.5f, 1.0f), 1.0f);
        EXPECT_FLOAT_EQ(SquircleCoverage(0.5f, 1.0f), 0.0f);
        // At 2x the transition is half a point wide: still exactly one pixel.
        EXPECT_FLOAT_EQ(SquircleCoverage(-0.25f, 2.0f), 1.0f);
        EXPECT_FLOAT_EQ(SquircleCoverage(0.125f, 2.0f), 0.25f);
    }
} // namespace Carbon
