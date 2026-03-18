#include <metal_stdlib>
using namespace metal;
#include "../src/CreatureRenderData.h"

// ============================================================
// VERTEX OUTPUT
// ============================================================
struct VertexOut {
    float4 position [[position]];
    float4 color;
    float2 uv;           // coordonnées locales [-1,1] dans le quad
    float  radius;
    float  attackFlash;
    uint   isSelected;
    float  energy;
};

struct FoodVertexOut {
    float4 position [[position]];
    float2 uv;
    float  energy;
    float  radius;
};

// ============================================================
// HELPER : convertir coordonnées monde -> NDC (clip space)
// ============================================================
float2 worldToNDC(float2 worldPos, constant SimUniforms& u) {
    // Appliquer le zoom et l'offset caméra
    float2 pos = (worldPos - u.cameraOffset) * u.cameraZoom;

    // Convertir en NDC [-1, 1]
    // x: 0..worldSize.x -> -1..1
    // y: 0..worldSize.y -> 1..-1 (inversé car Metal y pointe vers le bas)
    float2 ndc;
    ndc.x = (pos.x / u.worldSize.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (pos.y / u.worldSize.y) * 2.0;
    return ndc;
}

float worldToNDCScale(float worldSize, float worldDim, float zoom) {
    return (worldSize * zoom / worldDim) * 2.0;
}

// ============================================================
// CREATURE VERTEX SHADER
// ============================================================
// On dessine chaque créature comme un quad (2 triangles, 6 vertices).
// Le vertexID [0..5] définit quel coin du quad.
// L'instanceID sélectionne quelle créature.

vertex VertexOut creature_vertex(
    uint vertexID    [[vertex_id]],
    uint instanceID  [[instance_id]],
    constant CreatureInstance* creatures [[buffer(0)]],
    constant SimUniforms&      uniforms  [[buffer(1)]]
) {
    // Les 6 vertices d'un quad (2 triangles)
    // En coordonnées locales [-1, 1]
    const float2 quadVerts[6] = {
        {-1, -1}, { 1, -1}, { 1,  1},   // triangle 1
        {-1, -1}, { 1,  1}, {-1,  1}    // triangle 2
    };

    CreatureInstance c = creatures[instanceID];
    float2 local = quadVerts[vertexID];

    // Agrandir le quad un peu pour la sélection et les effets
    float extraRadius = c.isSelected ? 4.0 : 1.0;
    float totalRadius = c.radius + extraRadius;

    // Position monde du vertex
    float2 worldPos = c.position + local * totalRadius;

    // Convertir en clip space
    float2 ndc = worldToNDC(worldPos, uniforms);

    VertexOut out;
    out.position    = float4(ndc, 0.0, 1.0);
    out.color       = c.bodyColor;
    out.uv          = local;
    out.radius      = c.radius;
    out.attackFlash = c.attackFlash;
    out.isSelected  = c.isSelected;
    out.energy      = c.energy;
    return out;
}

// ============================================================
// CREATURE FRAGMENT SHADER
// ============================================================
fragment float4 creature_fragment(VertexOut in [[stage_in]]) {
    // Distance au centre du quad (cercle)
    float dist = length(in.uv);

    // Le corps est un disque
    // Antialiasing avec smoothstep sur le bord
    float bodyAlpha = 1.0 - smoothstep(0.75, 0.85, dist);

    if (bodyAlpha < 0.01) discard_fragment();

    float4 color = in.color;

    // Flash blanc quand attaque
    color = mix(color, float4(1.0), in.attackFlash * 0.6);

    // Cercle de sélection (anneau jaune)
    if (in.isSelected) {
        float ring = smoothstep(0.85, 0.88, dist) * (1.0 - smoothstep(0.92, 0.95, dist));
        color = mix(color, float4(1.0, 1.0, 0.3, 1.0), ring);
        bodyAlpha = max(bodyAlpha, ring);
    }

    // Point de direction (petit point blanc vers l'avant)
    // L'avant est en uv.x > 0, uv.y ~ 0 (car le quad est aligné et
    // la rotation est gérée par le vertex shader ou le CPU)
    float dirDot = 1.0 - smoothstep(0.0, 0.2, length(in.uv - float2(0.55, 0.0)));
    color = mix(color, float4(1.0), dirDot * 0.8);

    // Barre d'énergie (au-dessus de la créature)
    // On la dessine dans l'espace UV du quad
    float barY = in.uv.y;
    float barX = in.uv.x;
    if (barY < -0.85 && barY > -0.95 && abs(barX) < 0.7) {
        // Fond de la barre
        float barBg = 0.15;
        // Remplissage
        float fillX = (barX + 0.7) / 1.4; // normaliser [0,1]
        float barFill = fillX < in.energy ? 1.0 : 0.0;

        float3 barColor = mix(float3(0.8, 0.1, 0.1), float3(0.1, 0.8, 0.2), in.energy);
        color = float4(mix(float3(barBg), barColor, barFill), 1.0);
        bodyAlpha = 1.0;
    }

    color.a = bodyAlpha;
    return color;
}

// ============================================================
// FOOD VERTEX SHADER
// ============================================================
vertex FoodVertexOut food_vertex(
    uint vertexID    [[vertex_id]],
    uint instanceID  [[instance_id]],
    constant FoodInstance* food     [[buffer(0)]],
    constant SimUniforms&  uniforms [[buffer(1)]]
) {
    const float2 quadVerts[6] = {
        {-1, -1}, { 1, -1}, { 1,  1},
        {-1, -1}, { 1,  1}, {-1,  1}
    };

    FoodInstance f = food[instanceID];
    float2 local = quadVerts[vertexID];
    float2 worldPos = f.position + local * (f.radius + 1.0);
    float2 ndc = worldToNDC(worldPos, uniforms);

    FoodVertexOut out;
    out.position = float4(ndc, 0.0, 1.0);
    out.uv       = local;
    out.energy   = f.energy;
    out.radius   = f.radius;
    return out;
}

// ============================================================
// FOOD FRAGMENT SHADER
// ============================================================
fragment float4 food_fragment(FoodVertexOut in [[stage_in]]) {
    float dist = length(in.uv);
    float alpha = 1.0 - smoothstep(0.7, 0.9, dist);

    if (alpha < 0.01) discard_fragment();

    // Vert avec variation selon l'énergie
    float3 color = mix(float3(0.2, 0.5, 0.2), float3(0.3, 0.9, 0.4), in.energy);

    // Petit highlight
    float highlight = 1.0 - smoothstep(0.0, 0.4, length(in.uv - float2(-0.2, -0.2)));
    color += highlight * 0.2;

    return float4(color, alpha);
}

// ============================================================
// BACKGROUND GRID (optionnel, pour le debug)
// ============================================================
vertex float4 grid_vertex(
    uint vertexID [[vertex_id]],
    constant SimUniforms& uniforms [[buffer(0)]]
) {
    // Fullscreen quad
    const float2 verts[6] = {
        {-1,-1}, {1,-1}, {1,1},
        {-1,-1}, {1,1},  {-1,1}
    };
    return float4(verts[vertexID], 0.0, 1.0);
}
