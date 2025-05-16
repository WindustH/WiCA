#ifndef POINT_H
#define POINT_H

#include <functional> // Required for std::hash
#include <cstddef>    // Required for std::size_t
#include <fmt/core.h>


// Defines a simple 2D point/vector structure for coordinates.
struct Point {
    int x, y;

    // Constructors
    Point() : x(0), y(0) {}
    Point(int x_val, int y_val) : x(x_val), y(y_val) {}

    // Operator overloads for comparison
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Point& other) const {
        return !(*this == other);
    }

    // Operator overload for std::map key comparison
    bool operator<(const Point& other) const {
        if (x < other.x) return true;
        if (x > other.x) return false;
        return y < other.y;
    }

    // Operator overloads for basic vector math
    Point operator+(const Point& other) const {
        return Point(x + other.x, y + other.y);
    }

    Point operator-(const Point& other) const {
        return Point(x - other.x, y - other.y);
    }
};

// Custom hash function for Point to be used with std::unordered_map
namespace std {
    template <>
    struct hash<Point> {
        std::size_t operator()(const Point& p) const {
            std::size_t seed = 0;
            // Combine hash of p.x
            // The magic number 0x9e3779b9 is derived from the golden ratio (phi).
            // (sqrt(5)-1)/2 * 2^32. It's often used in hash functions.
            seed ^= std::hash<int>{}(p.x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            // Combine hash of p.y
            seed ^= std::hash<int>{}(p.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
}

template<>
struct fmt::formatter<Point> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const Point& obj, FormatContext& ctx) -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "Point(x={}, y={})", obj.x, obj.y);
    }
};

#endif // POINT_H
