#pragma once

#include <cmath>

namespace FloorPlan::Geometry
{
    struct Vec2
    {
        double X = 0.0;
        double Y = 0.0;

        Vec2 operator+(const Vec2& other) const { return Vec2{X + other.X, Y + other.Y}; }
        Vec2 operator-(const Vec2& other) const { return Vec2{X - other.X, Y - other.Y}; }
        Vec2 operator*(double scale) const { return Vec2{X * scale, Y * scale}; }

        double Length() const { return std::hypot(X, Y); }
        double LengthSquared() const { return X * X + Y * Y; }

        Vec2 PerpendicularCcw() const { return Vec2{-Y, X}; }
    };

    inline double Cross(const Vec2& left, const Vec2& right)
    {
        return left.X * right.Y - left.Y * right.X;
    }

    inline double Dot(const Vec2& left, const Vec2& right)
    {
        return left.X * right.X + left.Y * right.Y;
    }
}
