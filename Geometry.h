#pragma once

#include <cmath>
#include <algorithm>

#undef min
#undef max

/**
 * @class Point
 * @brief Represents a 2D point in the interface
 */
class Point {
public:
    float h = 0.0f; ///< Horizontal coordinate
    float v = 0.0f; ///< Vertical coordinate

    /**
     * @brief Default constructor
     */
    Point() = default;

    /**
     * @brief Constructor with coordinates
     * @param horizontal Horizontal coordinate
     * @param vertical Vertical coordinate
     */
    Point(float horizontal, float vertical) : h(horizontal), v(vertical) {}

    /**
     * @brief Calculates distance to another point
     * @param other The other point
     * @return The Euclidean distance
     */
    float DistanceTo(const Point &other) const {
        float dx = h - other.h;
        float dy = v - other.v;
        return std::sqrt(dx * dx + dy * dy);
    }

    /**
     * @brief Calculates squared distance to another point
     * @param other The other point
     * @return The squared Euclidean distance
     */
    float DistanceSquaredTo(const Point &other) const {
        float dx = h - other.h;
        float dy = v - other.v;
        return dx * dx + dy * dy;
    }

    // Mathematical operators
    Point operator+(const Point &other) const { return {h + other.h, v + other.v}; }
    Point operator-(const Point &other) const { return {h - other.h, v - other.v}; }
    Point operator*(float factor) const { return {h * factor, v * factor}; }
    Point operator/(float factor) const { return {h / factor, v / factor}; }

    Point &operator+=(const Point &other) {
        h += other.h;
        v += other.v;
        return *this;
    }

    Point &operator-=(const Point &other) {
        h -= other.h;
        v -= other.v;
        return *this;
    }

    Point &operator*=(float factor) {
        h *= factor;
        v *= factor;
        return *this;
    }

    Point &operator/=(float factor) {
        h /= factor;
        v /= factor;
        return *this;
    }

    bool operator==(const Point &other) const { return h == other.h && v == other.v; }
    bool operator!=(const Point &other) const { return !(*this == other); }

    /**
     * @brief Calculates the dot product with another point
     * @param other The other point
     * @return The dot product
     */
    float Dot(const Point &other) const {
        return h * other.h + v * other.v;
    }

    /**
     * @brief Calculates the cross product with another point
     * @param other The other point
     * @return The cross product
     */
    float Cross(const Point &other) const {
        return h * other.v - v * other.h;
    }

    /**
     * @brief Returns the length of the vector
     * @return The Euclidean length
     */
    float Length() const {
        return std::sqrt(h * h + v * v);
    }

    /**
     * @brief Returns the squared length of the vector
     * @return The squared length
     */
    float LengthSquared() const {
        return h * h + v * v;
    }

    /**
     * @brief Returns a normalized version of this point
     * @return A unit vector in the same direction
     */
    Point Normalized() const {
        float len = Length();
        if (len > 0) {
            return {h / len, v / len};
        }
        return *this;
    }

    /**
     * @brief Rotates the point around the origin
     * @param angle Angle in radians
     * @return The rotated point
     */
    Point Rotated(float angle) const {
        float s = std::sin(angle);
        float c = std::cos(angle);
        return {h * c - v * s, h * s + v * c};
    }

    /**
     * @brief Calculates the angle between this point and another
     * @param other The other point
     * @return The angle in radians
     */
    float AngleTo(const Point &other) const {
        return std::atan2(Cross(other), Dot(other));
    }

    /**
     * @brief Reflects the point across a normal vector
     * @param normal The normal to reflect across
     * @return The reflected point
     */
    Point Reflected(const Point &normal) const {
        Point n = normal.Normalized();
        return *this - n * (2.0f * Dot(n));
    }

    /**
     * @brief Linearly interpolates between this point and another
     * @param other The target point
     * @param t The interpolation factor (0-1)
     * @return The interpolated point
     */
    Point Lerp(const Point &other, float t) const {
        return {h + (other.h - h) * t, v + (other.v - v) * t};
    }

    /**
     * @brief Checks if the point is approximately equal to another
     * @param other The other point
     * @param epsilon The tolerance
     * @return True if approximately equal
     */
    bool ApproximatelyEqual(const Point &other, float epsilon = 0.001f) const {
        return std::abs(h - other.h) < epsilon && std::abs(v - other.v) < epsilon;
    }
};

/**
 * @class Rect
 * @brief Represents a rectangular area in the interface
 */
class Rect {
public:
    float hPos = 0.0f;  ///< Horizontal position (left)
    float vPos = 0.0f;  ///< Vertical position (top)
    float hSize = 0.0f; ///< Width
    float vSize = 0.0f; ///< Height

    /**
     * @brief Default constructor
     */
    Rect() = default;

    /**
     * @brief Constructor with all parameters
     * @param hPosition Horizontal position
     * @param vPosition Vertical position
     * @param width Width
     * @param height Height
     */
    Rect(float hPosition, float vPosition, float width, float height)
        : hPos(hPosition), vPos(vPosition), hSize(width), vSize(height) {}

    /**
     * @brief Sets the position of the rectangle
     * @param h Horizontal position
     * @param v Vertical position
     */
    void SetPosition(float h, float v) {
        hPos = h;
        vPos = v;
    }

    /**
     * @brief Sets the size of the rectangle
     * @param width Width
     * @param height Height
     */
    void SetSize(float width, float height) {
        hSize = width;
        vSize = height;
    }

    /**
     * @brief Gets the right edge position
     * @return The right edge position
     */
    float Right() const {
        return hPos + hSize;
    }

    /**
     * @brief Gets the bottom edge position
     * @return The bottom edge position
     */
    float Bottom() const {
        return vPos + vSize;
    }

    /**
     * @brief Gets the center X coordinate
     * @return The center X coordinate
     */
    float CenterX() const {
        return hPos + hSize / 2.0f;
    }

    /**
     * @brief Gets the center Y coordinate
     * @return The center Y coordinate
     */
    float CenterY() const {
        return vPos + vSize / 2.0f;
    }

    /**
     * @brief Checks if the rectangle contains a point
     * @param x X coordinate
     * @param y Y coordinate
     * @return True if the point is inside the rectangle
     */
    bool Contains(float x, float y) const {
        return x >= hPos && x <= Right() && y >= vPos && y <= Bottom();
    }

    /**
     * @brief Checks if the rectangle contains a point
     * @param point The point to check
     * @return True if the point is inside the rectangle
     */
    bool Contains(const Point &point) const {
        return Contains(point.h, point.v);
    }

    /**
     * @brief Checks if this rectangle intersects with another
     * @param other The other rectangle
     * @return True if the rectangles intersect
     */
    bool Intersects(const Rect &other) const {
        return !(other.hPos > Right() || other.Right() < hPos ||
            other.vPos > Bottom() || other.Bottom() < vPos);
    }

    /**
     * @brief Calculates the area of intersection with another rectangle
     * @param other The other rectangle
     * @return The area of intersection (0 if no intersection)
     */
    float IntersectionArea(const Rect &other) const {
        if (!Intersects(other)) return 0.0f;

        float xOverlap = std::min(Right(), other.Right()) - std::max(hPos, other.hPos);
        float yOverlap = std::min(Bottom(), other.Bottom()) - std::max(vPos, other.vPos);

        return xOverlap * yOverlap;
    }

    /**
     * @brief Expands the rectangle to include a point
     * @param x X coordinate
     * @param y Y coordinate
     */
    void ExpandToInclude(float x, float y) {
        float newRight = std::max(Right(), x);
        float newBottom = std::max(Bottom(), y);

        hPos = std::min(hPos, x);
        vPos = std::min(vPos, y);

        hSize = newRight - hPos;
        vSize = newBottom - vPos;
    }

    /**
     * @brief Expands the rectangle to include another rectangle
     * @param other The other rectangle
     */
    void ExpandToInclude(const Rect &other) {
        float newRight = std::max(Right(), other.Right());
        float newBottom = std::max(Bottom(), other.Bottom());

        hPos = std::min(hPos, other.hPos);
        vPos = std::min(vPos, other.vPos);

        hSize = newRight - hPos;
        vSize = newBottom - vPos;
    }

    /**
     * @brief Creates a rectangle that is the union of this and another
     * @param other The other rectangle
     * @return The union rectangle
     */
    Rect Union(const Rect &other) const {
        Rect result = *this;
        result.ExpandToInclude(other);
        return result;
    }

    /**
     * @brief Creates a rectangle that is the intersection of this and another
     * @param other The other rectangle
     * @return The intersection rectangle (empty if no intersection)
     */
    Rect Intersection(const Rect &other) const {
        if (!Intersects(other)) return {0, 0, 0, 0};

        float left = std::max(hPos, other.hPos);
        float top = std::max(vPos, other.vPos);
        float right = std::min(Right(), other.Right());
        float bottom = std::min(Bottom(), other.Bottom());

        return {left, top, right - left, bottom - top};
    }

    /**
     * @brief Insets the rectangle by the specified amounts
     * @param horizontal Horizontal inset
     * @param vertical Vertical inset
     */
    void Inset(float horizontal, float vertical) {
        hPos += horizontal;
        vPos += vertical;
        hSize -= horizontal * 2;
        vSize -= vertical * 2;

        // Ensure dimensions don't go negative
        hSize = std::max(0.0f, hSize);
        vSize = std::max(0.0f, vSize);
    }

    /**
     * @brief Offsets the rectangle by the specified amounts
     * @param horizontal Horizontal offset
     * @param vertical Vertical offset
     */
    void Offset(float horizontal, float vertical) {
        hPos += horizontal;
        vPos += vertical;
    }
};
