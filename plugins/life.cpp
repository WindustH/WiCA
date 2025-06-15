#include "life.h"
// No other project-specific headers should be needed here if the function is self-contained.
// If you need Point or other utilities, you'd have to reconsider the DLL's dependencies
// or pass all necessary data through the function arguments.
// For simplicity, this example is self-contained.

// Define LIFE_PLUGIN_EXPORTS when building this DLL
// This is usually done in the CMakeLists.txt for the DLL target.

extern "C" LIFE_PLUGIN_API int update(const int* neighborStates) {
    // Count live neighbors
    int liveNeighbors = 0;
    if (neighborStates != nullptr) { // Ensure neighborStates is not null before iterating
        for (int i = 0; i < 8; ++i) {
            if (neighborStates[i] == 1) { // Assuming state 1 is "alive"
                liveNeighbors++;
            }
        }
    }

    // Apply Conway's Game of Life rules
    if (neighborStates[8] == 1) { // Cell is currently alive
        if (liveNeighbors < 2) {
            return 0; // Dies from underpopulation
        } else if (liveNeighbors == 2 || liveNeighbors == 3) {
            return 1; // Lives on
        } else { // liveNeighbors > 3
            return 0; // Dies from overpopulation
        }
    } else { // Cell is currently dead (state 0)
        if (liveNeighbors == 3) {
            return 1; // Becomes alive (reproduction)
        } else {
            return 0; // Stays dead
        }
    }
}
