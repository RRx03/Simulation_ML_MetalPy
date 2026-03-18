//Shared between C++ and Python
#pragma once
#include <semaphore.h>
#include <stdint.h>

#define MAX_CREATURES 200
#define NUM_INPUTS 38
#define NUM_ACTIONS 5

typedef struct {
  uint32_t num_alive;
  uint32_t tick;

  float perceptions[MAX_CREATURES][NUM_INPUTS];
  float energies[MAX_CREATURES];
  uint32_t creature_ids[MAX_CREATURES];

  float actions[MAX_CREATURES][NUM_ACTIONS];
} FrameBuffer;
