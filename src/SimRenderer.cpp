// SimRenderer.cpp
#include "SimRenderer.hpp"
#include <iostream>
#include <cstring>
#include <cmath>

// ============================================================
// INIT
// ============================================================
bool SimRenderer::init(MTL::Device* device, const char* shaderLibPath) {
    _device = device;
    NS::Error* error = nullptr;

    // -------------------------------------------------------
    // Charger la bibliothèque de shaders de simulation
    // -------------------------------------------------------
    MTL::Library* lib = device->newLibrary(
        NS::String::string(shaderLibPath, NS::UTF8StringEncoding), &error);
    if (!lib) {
        std::cerr << "[SimRenderer] Shader load failed: "
                  << error->localizedDescription()->utf8String() << std::endl;
        return false;
    }

    // -------------------------------------------------------
    // Pipeline créatures
    // -------------------------------------------------------
    MTL::Function* creatureVert = lib->newFunction(
        NS::String::string("creature_vertex", NS::UTF8StringEncoding));
    MTL::Function* creatureFrag = lib->newFunction(
        NS::String::string("creature_fragment", NS::UTF8StringEncoding));

    if (!creatureVert || !creatureFrag) {
        std::cerr << "[SimRenderer] Creature shader functions not found" << std::endl;
        lib->release();
        return false;
    }

    MTL::RenderPipelineDescriptor* pipeDesc =
        MTL::RenderPipelineDescriptor::alloc()->init();
    pipeDesc->setVertexFunction(creatureVert);
    pipeDesc->setFragmentFunction(creatureFrag);
    pipeDesc->colorAttachments()->object(0)->setPixelFormat(
        MTL::PixelFormatBGRA8Unorm);
    // Alpha blending pour les cercles antialiasés
    pipeDesc->colorAttachments()->object(0)->setBlendingEnabled(true);
    pipeDesc->colorAttachments()->object(0)->setSourceRGBBlendFactor(
        MTL::BlendFactorSourceAlpha);
    pipeDesc->colorAttachments()->object(0)->setDestinationRGBBlendFactor(
        MTL::BlendFactorOneMinusSourceAlpha);
    pipeDesc->colorAttachments()->object(0)->setSourceAlphaBlendFactor(
        MTL::BlendFactorOne);
    pipeDesc->colorAttachments()->object(0)->setDestinationAlphaBlendFactor(
        MTL::BlendFactorOneMinusSourceAlpha);

    _creaturePSO = device->newRenderPipelineState(pipeDesc, &error);
    if (!_creaturePSO) {
        std::cerr << "[SimRenderer] Creature PSO failed: "
                  << error->localizedDescription()->utf8String() << std::endl;
        pipeDesc->release();
        creatureVert->release();
        creatureFrag->release();
        lib->release();
        return false;
    }

    // -------------------------------------------------------
    // Pipeline nourriture
    // -------------------------------------------------------
    MTL::Function* foodVert = lib->newFunction(
        NS::String::string("food_vertex", NS::UTF8StringEncoding));
    MTL::Function* foodFrag = lib->newFunction(
        NS::String::string("food_fragment", NS::UTF8StringEncoding));

    pipeDesc->setVertexFunction(foodVert);
    pipeDesc->setFragmentFunction(foodFrag);

    _foodPSO = device->newRenderPipelineState(pipeDesc, &error);
    if (!_foodPSO) {
        std::cerr << "[SimRenderer] Food PSO failed: "
                  << error->localizedDescription()->utf8String() << std::endl;
    }

    // Cleanup des objets temporaires
    pipeDesc->release();
    creatureVert->release();
    creatureFrag->release();
    foodVert->release();
    foodFrag->release();
    lib->release();

    // -------------------------------------------------------
    // Allouer les buffers d'instances
    // -------------------------------------------------------
    // On alloue pour MAX_CREATURES — on n'utilisera que les N premières
    _creatureBuffer = device->newBuffer(
        sizeof(CreatureInstance) * MAX_CREATURES,
        MTL::ResourceStorageModeShared);

    _foodBuffer = device->newBuffer(
        sizeof(FoodInstance) * 500,  // max food items
        MTL::ResourceStorageModeShared);

    _uniformBuffer = device->newBuffer(
        sizeof(SimUniforms),
        MTL::ResourceStorageModeShared);

    std::cout << "[SimRenderer] Initialized successfully." << std::endl;
    std::cout << "  CreatureInstance size: " << sizeof(CreatureInstance) << " bytes" << std::endl;
    std::cout << "  Max creatures: " << MAX_CREATURES << std::endl;

    return true;
}

// ============================================================
// UPDATE FROM SHARED STATE
// ============================================================
void SimRenderer::updateFromState(FrameBuffer* fb, int numCreatures, int numFood) {
    _numCreatures = numCreatures;

    // Remplir le buffer d'instances créatures
    CreatureInstance* instances =
        static_cast<CreatureInstance*>(_creatureBuffer->contents());

    for (int i = 0; i < numCreatures; i++) {
        CreatureInstance& ci = instances[i];

        // Position et angle — pour l'instant on prend des valeurs du FrameBuffer.
        // Les perceptions contiennent les données des rayons, pas la position.
        // Tu devras ajouter position/angle dans le FrameBuffer (ou dans une
        // struct séparée). Pour démarrer, on utilise les creature_ids comme seed.
        //
        // TODO: Ajouter float position_x, position_y, angle dans FrameBuffer
        //       quand ta simulation C++ sera prête.
        //
        // Pour l'instant, placeholder : positions depuis les IDs
        // (remplacer dès que tu as les vraies positions dans le shared buffer)
        uint32_t id = fb->creature_ids[i];
        ci.position = { 
            fb->positions_x[i],  // TODO: remplacer par position_x depuis le shared buffer
            fb->positions_y[i]   // TODO: remplacer par position_y depuis le shared buffer
        };
        ci.angle = fb->angles[i];  // TODO: remplacer par angle depuis le shared buffer

        // Énergie normalisée
        ci.energy = fb->energies[i] / 200.0f;  // max_energy = 200
        ci.energy = fminf(1.0f, fmaxf(0.0f, ci.energy));

        // Propriétés visuelles (pour l'instant constantes, à terme from genes)
        ci.radius = 8.0f;  // TODO: varier selon les gènes

        // Couleur basée sur l'ID (chaque lignée a sa couleur)
        float hue = fmodf(id * 0.618f, 1.0f);  // golden ratio pour distribution
        // HSV to RGB simplifié
        float h = hue * 6.0f;
        float x = 1.0f - fabsf(fmodf(h, 2.0f) - 1.0f);
        float r, g, b;
        if      (h < 1) { r=1; g=x; b=0; }
        else if (h < 2) { r=x; g=1; b=0; }
        else if (h < 3) { r=0; g=1; b=x; }
        else if (h < 4) { r=0; g=x; b=1; }
        else if (h < 5) { r=x; g=0; b=1; }
        else            { r=1; g=0; b=x; }
        // Désaturer un peu pour que ce soit joli
        r = 0.3f + r * 0.7f;
        g = 0.3f + g * 0.7f;
        b = 0.3f + b * 0.7f;
        ci.bodyColor = { r, g, b, 1.0f };

        // Flash d'attaque — on check si l'action "attaque" est > 0.5
        ci.attackFlash = fb->actions[i][3] > 0.5f ? 0.8f : 0.0f;

        // Sélection
        ci.isSelected = (_selectedIndex == i) ? 1 : 0;

        ci._pad = 0;
    }

    // TODO: remplir le buffer food quand tu auras les positions de nourriture
    // dans le shared buffer. Pour l'instant, quelques items de test.
    _numFood = 0;  // Désactivé tant qu'il n'y a pas de données food
}

// ============================================================
// ENCODE DRAW CALLS
// ============================================================
void SimRenderer::encode(MTL::RenderCommandEncoder* encoder, float time) {
    // Mettre à jour le temps pour les animations shader
    _uniforms.time = time;
    memcpy(_uniformBuffer->contents(), &_uniforms, sizeof(SimUniforms));

    // -------------------------------------------------------
    // Dessiner la nourriture (en dessous des créatures)
    // -------------------------------------------------------
    if (_numFood > 0 && _foodPSO) {
        encoder->setRenderPipelineState(_foodPSO);
        encoder->setVertexBuffer(_foodBuffer, 0, 0);
        encoder->setVertexBuffer(_uniformBuffer, 0, 1);
        // 6 vertices par quad (2 triangles), _numFood instances
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle,
                                NS::UInteger(0),
                                NS::UInteger(6),
                                NS::UInteger(_numFood));
    }

    // -------------------------------------------------------
    // Dessiner les créatures
    // -------------------------------------------------------
    if (_numCreatures > 0 && _creaturePSO) {
        encoder->setRenderPipelineState(_creaturePSO);
        encoder->setVertexBuffer(_creatureBuffer, 0, 0);
        encoder->setVertexBuffer(_uniformBuffer, 0, 1);
        // 6 vertices par quad, _numCreatures instances
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle,
                                NS::UInteger(0),
                                NS::UInteger(6),
                                NS::UInteger(_numCreatures));
    }
}

// ============================================================
// CAMERA
// ============================================================
void SimRenderer::setCamera(float worldW, float worldH,
                            float screenW, float screenH,
                            float offsetX, float offsetY, float zoom) {
    _uniforms.worldSize   = { worldW, worldH };
    _uniforms.screenSize  = { screenW, screenH };
    _uniforms.cameraOffset = { offsetX, offsetY };
    _uniforms.cameraZoom  = zoom;
}

// ============================================================
// CLEANUP
// ============================================================
void SimRenderer::cleanup() {
    if (_creaturePSO) { _creaturePSO->release(); _creaturePSO = nullptr; }
    if (_foodPSO)     { _foodPSO->release();     _foodPSO = nullptr; }
    if (_creatureBuffer) { _creatureBuffer->release(); _creatureBuffer = nullptr; }
    if (_foodBuffer)     { _foodBuffer->release();     _foodBuffer = nullptr; }
    if (_uniformBuffer)  { _uniformBuffer->release();  _uniformBuffer = nullptr; }
    std::cout << "[SimRenderer] Cleaned up." << std::endl;
}
