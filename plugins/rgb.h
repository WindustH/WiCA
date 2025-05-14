#ifndef RGB_PLUGIN_H
#define RGB_PLUGIN_H

// Standard C/C++ headers can be included if needed
// For example, <vector>, <numeric>, etc.

// Define the export macro for DLLs
// This ensures the function is correctly exported on Windows
// and has default visibility on POSIX systems.
#ifdef _WIN32
    #ifdef RGB_PLUGIN_EXPORTS // This should be defined by the DLL project
        #define RGB_PLUGIN_API __declspec(dllexport)
    #else
        #define RGB_PLUGIN_API __declspec(dllimport) // For executables linking against it (not used here)
    #endif
#else // GCC/Clang on Linux/macOS
    #define RGB_PLUGIN_API __attribute__((visibility("default")))
#endif

/**
 * @brief Conway's Game of Life update rule function.
 * * This function implements the rules for Conway's Game of Life:
 * 1. Any live cell with fewer than two live neighbours dies, as if by underpopulation.
 * 2. Any live cell with two or three live neighbours lives on to the next generation.
 * 3. Any live cell with more than three live neighbours dies, as if by overpopulation.
 * 4. Any dead cell with exactly three live neighbours becomes a live cell, as if by reproduction.
 * * States: 0 for dead, 1 for alive.
 *
 * @param currentCellState The current state of the cell being evaluated (0 or 1).
 * @param neighborStates An array of integers representing the states of the neighbors.
 * @param neighborCount The number of neighbors (typically 8 for Moore neighborhood).
 * @return The new state for the cell (0 or 1).
 */
extern "C" RGB_PLUGIN_API int update(const int* neighborStates);

#endif // RGB_PLUGIN_H
