#ifndef COLOR_H
#define COLOR_H

// Defines a simple color structure (RGBA).
struct Color {
    unsigned char r, g, b, a;

    // Constructors
    Color() : r(0), g(0), b(0), a(255) {} // Default to opaque black
    Color(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}

    // (Optional) Add comparison operators if needed, e.g., for std::map or std::set
    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    bool operator!=(const Color& other) const {
        return !(*this == other);
    }
};

#endif // COLOR_H