#include "rgb.h"
const int GROWTH_THRESHOLD = 3;
const int CONSUMPTION_THRESHOLD = 3;
const int SUPPORT_THRESHOLD = 2;
extern "C" RGB_PLUGIN_API int update(const int* neighborStates) {
    int currentState = neighborStates[4];
    int nextState = currentState;
    int countR = 0;
    int countG = 0;
    int countB = 0;

    for (int i = 0; i < 9; ++i) {
        if (i == 4) {
            continue;
        }
        if (neighborStates[i] == 1) {
            countR++;
        } else if (neighborStates[i] == 2) {
            countG++;
        } else if (neighborStates[i] == 3) {
            countB++;
        } else if (neighborStates[i] == 4) {
            if(currentState!=4) return 0;
        }
    }
    if (currentState == 0) {
        if (countR >= GROWTH_THRESHOLD && countR > countG && countR > countB) {
            nextState = 1;
        } else if (countG >= GROWTH_THRESHOLD && countG > countR && countG > countB) {
            nextState = 2;
        } else if (countB >= GROWTH_THRESHOLD && countB > countR && countB > countG) {
            nextState = 3;
        }
    } else if (currentState == 1) {
        if (countG >= CONSUMPTION_THRESHOLD) {
            nextState = 2;
        }
        else if (countR < SUPPORT_THRESHOLD) {
            nextState = 0;
        }
    } else if (currentState == 2) {
        if (countB >= CONSUMPTION_THRESHOLD) {
            nextState = 3;
        }
        else if (countG < SUPPORT_THRESHOLD) {
            nextState = 0;
        }
    } else if (currentState == 3) {
        if (countR >= CONSUMPTION_THRESHOLD) {
            nextState = 1;
        }
        else if (countB < SUPPORT_THRESHOLD) {
            nextState = 0;
        }
    }
    return nextState;
}