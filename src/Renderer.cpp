// Renderer.cpp
#include "Renderer.hpp"
#include "Common.h"
#include <iostream>

Renderer::Renderer(SDL_Window *window) {
  _device = MTL::CreateSystemDefaultDevice();
  if (!_device) {
    throw std::runtime_error("No Metal GPU found");
  }
  _commandQueue = _device->newCommandQueue();

  SDL_MetalView view = SDL_Metal_CreateView(window);
  void *layer_ptr = SDL_Metal_GetLayer(view);
  _layer = reinterpret_cast<CA::MetalLayer *>(layer_ptr);
  _layer->setDevice(_device);
  _layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

  buildShaders();
  buildBuffers();
}

Renderer::~Renderer() {
  _simRenderer.cleanup();
  _shm.cleanup();

  if (_depthTexture)
    _depthTexture->release();
  if (_msaaTexture)
    _msaaTexture->release();
  if (_commandQueue)
    _commandQueue->release();
  if (_device)
    _device->release();
}

void Renderer::buildShaders() {
  // Le SimRenderer charge ses propres shaders
  // On compile les shaders de simulation dans un metallib séparé
  // (ou le même, selon ton Makefile)
}

void Renderer::buildBuffers() {
  // Shared memory (crée le segment mmap + MTLBuffer)
  if (!_shm.init(_device)) {
    throw std::runtime_error("Failed to initialize shared memory");
  }

    // SimRenderer (charge les shaders créatures/food, crée les pipelines)
    if (!_simRenderer.init(_device, "./build/sim.metallib")) {
      throw std::runtime_error("Failed to initialize SimRenderer");
    }

    // Configurer la caméra pour le monde 2D
    _simRenderer.setCamera(1200.0f, 800.0f, (float)_width, (float)_height);
}

void Renderer::resize(int width, int height) {
  _width = width;
  _height = height;
  _layer->setDrawableSize(CGSizeMake(width, height));

  // Mettre à jour la caméra
  _simRenderer.setCamera(1200.0f, 800.0f, (float)width, (float)height);

  // Recréer les textures MSAA et depth
  if (_msaaTexture)
    _msaaTexture->release();
  if (_depthTexture)
    _depthTexture->release();

  MTL::TextureDescriptor *msaaDesc = MTL::TextureDescriptor::alloc()->init();
  msaaDesc->setTextureType(MTL::TextureType2DMultisample);
  msaaDesc->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  msaaDesc->setWidth(width);
  msaaDesc->setHeight(height);
  msaaDesc->setSampleCount(_sampleCount);
  msaaDesc->setUsage(MTL::TextureUsageRenderTarget);
  msaaDesc->setStorageMode(MTL::StorageModePrivate);
  _msaaTexture = _device->newTexture(msaaDesc);
  msaaDesc->release();

  MTL::TextureDescriptor *depthDesc = MTL::TextureDescriptor::alloc()->init();
  depthDesc->setTextureType(MTL::TextureType2DMultisample);
  depthDesc->setPixelFormat(MTL::PixelFormatDepth32Float);
  depthDesc->setWidth(width);
  depthDesc->setHeight(height);
  depthDesc->setSampleCount(_sampleCount);
  depthDesc->setUsage(MTL::TextureUsageRenderTarget);
  depthDesc->setStorageMode(MTL::StorageModePrivate);
  _depthTexture = _device->newTexture(depthDesc);
  depthDesc->release();
}

void Renderer::renderFrame() {
  _time += 1.0f / 60.0f;

  // -------------------------------------------------------
  // Mettre à jour les données de rendu depuis le shared buffer
  // -------------------------------------------------------
  FrameBuffer *fb = _shm.readBuffer(); // buffer que Python a fini d'écrire
  int alive = fb->num_alive;
  _simRenderer.updateFromState(fb, alive, 0 /* food count */);

  // -------------------------------------------------------
  // Encoder le rendu Metal
  // -------------------------------------------------------
  NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
  CA::MetalDrawable *drawable = _layer->nextDrawable();

  if (drawable) {
    MTL::CommandBuffer *cmdBuf = _commandQueue->commandBuffer();

    // Render pass
    MTL::RenderPassDescriptor *pass =
        MTL::RenderPassDescriptor::renderPassDescriptor();

    MTL::RenderPassColorAttachmentDescriptor *colorAtt =
        pass->colorAttachments()->object(0);
    colorAtt->setTexture(_msaaTexture);
    colorAtt->setResolveTexture(drawable->texture());
    colorAtt->setLoadAction(MTL::LoadActionClear);
    colorAtt->setClearColor(MTL::ClearColor::Make(0.06, 0.07, 0.09, 1.0));
    colorAtt->setStoreAction(MTL::StoreActionMultisampleResolve);

    MTL::RenderPassDepthAttachmentDescriptor *depthAtt =
        pass->depthAttachment();
    depthAtt->setTexture(_depthTexture);
    depthAtt->setLoadAction(MTL::LoadActionClear);
    depthAtt->setClearDepth(1.0);
    depthAtt->setStoreAction(MTL::StoreActionDontCare);

    MTL::RenderCommandEncoder *encoder = cmdBuf->renderCommandEncoder(pass);

    // --- Dessiner les créatures et la nourriture ---
    _simRenderer.encode(encoder, _time);

    encoder->endEncoding();
    cmdBuf->presentDrawable(drawable);
    cmdBuf->commit();
  }

    pool->release();
}