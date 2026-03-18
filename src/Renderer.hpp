// Renderer.hpp
#pragma once

#include "SharedMemory.hpp"
#include "SimRenderer.hpp"
#include "metal-cpp/Metal/Metal.hpp"
#include "metal-cpp/QuartzCore/QuartzCore.hpp"
#include <SDL.h>

class Renderer {
public:
  Renderer(SDL_Window *window);
  ~Renderer();
  void renderFrame();
  void resize(int width, int height);

  // Accès au shared memory pour la boucle principale
  SharedMemory &sharedMemory() { return _shm; }
  SimRenderer &simRenderer() { return _simRenderer; }

private:
  void buildShaders();
  void buildBuffers();

  MTL::Device *_device = nullptr;
  MTL::CommandQueue *_commandQueue = nullptr;
  CA::MetalLayer *_layer = nullptr;

  // Simulation rendering
  SharedMemory _shm;
  SimRenderer _simRenderer;

  // Textures
  MTL::Texture *_depthTexture = nullptr;
  MTL::Texture *_msaaTexture = nullptr;
  const int _sampleCount = 4;

  // Dimensions
  int _width = 800;
  int _height = 600;
  float _time = 0.0f;
};