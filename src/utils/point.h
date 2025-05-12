#ifndef POINT_H
#define POINT_H

#include <functional> // Required for std::hash

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
            // A simple hash combining the x and y coordinates
            // For more robust hashing, consider more complex hash functions
            // or using a library like Boost's hash_combine.
            auto h1 = std::hash<int>{}(p.x);
            auto h2 = std::hash<int>{}(p.y);
            // Simple XOR combination, can be improved
            return h1 ^ (h2 << 1);
        }
    };
}

#endif // POINT_H