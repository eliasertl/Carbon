#include "Carbon/Animation/Easing.h"

#include <algorithm>
#include <cmath>

namespace Carbon
{
    namespace
    {
        // One coordinate of a cubic Bezier with end points 0 and 1 and control values a and b.
        float Bezier(float a, float b, float s)
        {
            const float inverse = 1.0f - s;
            return 3.0f * inverse * inverse * s * a + 3.0f * inverse * s * s * b + s * s * s;
        }

        float BezierDerivative(float a, float b, float s)
        {
            const float inverse = 1.0f - s;
            return 3.0f * inverse * inverse * a + 6.0f * inverse * s * (b - a) + 3.0f * s * s * (1.0f - b);
        }
    } // namespace

    float CubicBezier(float x1, float y1, float x2, float y2, float t)
    {
        if (t <= 0.0f)
            return 0.0f;
        if (t >= 1.0f)
            return 1.0f;

        // Find the curve parameter s whose x equals t: a few Newton steps, then bisection as a safety net.
        float s = t;
        for (int i = 0; i < 8; i++)
        {
            const float error = Bezier(x1, x2, s) - t;
            if (std::abs(error) < 1e-6f)
                return Bezier(y1, y2, s);
            const float slope = BezierDerivative(x1, x2, s);
            if (std::abs(slope) < 1e-6f)
                break;
            s = std::clamp(s - error / slope, 0.0f, 1.0f);
        }

        float low = 0.0f;
        float high = 1.0f;
        for (int i = 0; i < 32; i++)
        {
            s = (low + high) * 0.5f;
            if (Bezier(x1, x2, s) < t)
                low = s;
            else
                high = s;
        }
        return Bezier(y1, y2, s);
    }

    float Ease(Easing easing, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        switch (easing)
        {
            case Easing::Linear:
                return t;
            case Easing::EaseIn:
                return CubicBezier(0.42f, 0.0f, 1.0f, 1.0f, t);
            case Easing::EaseOut:
                return CubicBezier(0.0f, 0.0f, 0.58f, 1.0f, t);
            case Easing::EaseInOut:
                return CubicBezier(0.42f, 0.0f, 0.58f, 1.0f, t);
        }
        return t;
    }
} // namespace Carbon
