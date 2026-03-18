// SimRenderer.hpp — Rendu 2D des créatures et nourriture
// Séparé du Renderer principal pour garder les responsabilités claires.
#pragma once

#include "Common.h"
#include "CreatureRenderData.h"
#include "metal-cpp/Metal/Metal.hpp"

class SimRenderer {
public:
    // Initialise les pipelines et buffers de rendu.
    // Appeler une fois après avoir le device Metal.
    bool init(MTL::Device* device, const char* shaderLibPath);

    // Libère les ressources Metal.
    void cleanup();

    // Met à jour les buffers d'instances depuis le SharedState.
    // Appeler chaque frame AVANT encode().
    // Pour l'instant on lit directement le FrameBuffer.
    // Plus tard tu pourras ajouter les données génétiques ici.
    void updateFromState(FrameBuffer* fb, int numCreatures, int numFood);

    // Encode les draw calls dans un render encoder existant.
    // Tu appelles ça depuis Renderer::renderFrame() entre
    // encoder->begin() et encoder->endEncoding().
    void encode(MTL::RenderCommandEncoder* encoder, float time);

    // Met à jour les uniforms de la caméra
    void setCamera(float worldW, float worldH, float screenW, float screenH,
                   float offsetX = 0, float offsetY = 0, float zoom = 1.0f);

    // Sélection
    void selectCreature(int index) { _selectedIndex = index; }
    void clearSelection() { _selectedIndex = -1; }

private:
    MTL::Device* _device = nullptr;

    // Pipelines
    MTL::RenderPipelineState* _creaturePSO = nullptr;
    MTL::RenderPipelineState* _foodPSO     = nullptr;

    // Buffers d'instances (remplis chaque frame)
    MTL::Buffer* _creatureBuffer = nullptr;
    MTL::Buffer* _foodBuffer     = nullptr;
    MTL::Buffer* _uniformBuffer  = nullptr;

    // Compteurs
    int _numCreatures = 0;
    int _numFood      = 0;
    int _selectedIndex = -1;

    // Uniforms
    SimUniforms _uniforms;
};
