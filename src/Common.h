// Common.h — Shared between C++ and Python
// This file defines the memory layout that BOTH sides must agree on.
#pragma once
#include <stdint.h>

#define MAX_CREATURES 200
#define NUM_INPUTS 38
#define NUM_ACTIONS 5
#define NUM_BUFFERS 2

// Le nom du segment de mémoire partagée POSIX.
// Python ouvrira exactement ce nom.
#define SHM_NAME "/evo_sim"

typedef struct {
  uint32_t num_alive;
  uint32_t tick;

  // C++ écrit, Python lit
  float perceptions[MAX_CREATURES][NUM_INPUTS]; // 30 400 octets
  float energies[MAX_CREATURES];                //    800 octets
  uint32_t creature_ids[MAX_CREATURES];         //    800 octets

  // Python écrit, C++ lit
  float actions[MAX_CREATURES][NUM_ACTIONS]; //  4 000 octets
} FrameBuffer;

// Structure globale en tête du segment partagé.
// C'est la PREMIÈRE chose dans le segment mmap.
// Python la lit pour savoir quel buffer est actif.
typedef struct {
  // Index du buffer que C++ est en train d'ÉCRIRE (0 ou 1).
  // Python doit lire/écrire dans le MÊME buffer (après le sémaphore).
  // L'autre buffer est libre pour C++ (il lit les anciennes actions).
  volatile uint32_t write_idx;

  // Padding pour aligner les FrameBuffers sur 64 octets (cache line)
  uint32_t _pad[15];

  // Les deux frame buffers, côte à côte en mémoire
  FrameBuffer buf[NUM_BUFFERS];
} SharedState;

// Taille totale du segment de mémoire partagée
#define SHM_SIZE sizeof(SharedState)