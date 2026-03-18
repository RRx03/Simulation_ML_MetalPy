// CreatureRenderData.h — Données de rendu par créature
// Partagé entre C++ et les shaders Metal
#pragma once
#include <simd/simd.h>

// Données d'instance pour chaque créature vivante.
// Le CPU remplit un buffer de ces structs chaque frame,
// et le GPU dessine une instance par créature.
struct CreatureInstance {
    // Position et orientation dans le monde 2D
    vector_float2 position;     // centre dans l'espace monde
    float         angle;        // direction (radians)

    // Propriétés visuelles (issues des gènes, évoluent avec les générations)
    float         radius;       // taille du corps
    vector_float4 bodyColor;    // couleur RGBA du corps
    float         energy;       // [0..1] normalisée, pour la barre de vie
    float         attackFlash;  // [0..1] flash blanc quand attaque

    // Flags
    uint32_t      isSelected;   // 1 si sélectionné par le joueur

    // Padding pour alignement 16 bytes (Metal aime ça)
    float         _pad;
};

// Uniforms pour la caméra 2D
struct SimUniforms {
    vector_float2 worldSize;      // taille du monde (ex: 1200, 800)
    vector_float2 screenSize;     // taille de la fenêtre en pixels
    vector_float2 cameraOffset;   // pour le pan
    float         cameraZoom;     // pour le zoom
    float         time;           // temps pour les animations shader
};

// Données pour la nourriture (plus simple)
struct FoodInstance {
    vector_float2 position;
    float         radius;
    float         energy;       // [0..1] pour varier la couleur/taille
};
