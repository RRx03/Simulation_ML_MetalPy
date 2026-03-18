// SharedMemory.hpp — Gère le segment mmap partagé avec Python
#pragma once
#include "Common.h"
#include "metal-cpp/Metal/Metal.hpp"
#include "metal-cpp/QuartzCore/QuartzCore.hpp"

class SharedMemory {
public:
    // Crée le segment partagé et le lie à un MTLBuffer.
    // Appeler une seule fois au démarrage.
    // device : le MTLDevice pour créer le buffer Metal
    // Retourne false si l'init échoue.
    bool init(MTL::Device* device);

    // Libère le segment partagé. Appeler à la fermeture.
    void cleanup();

    // Accès direct à la structure partagée
    SharedState* state() { return _state; }

    // Accès au buffer Metal (pour l'envoyer aux shaders si besoin)
    MTL::Buffer* metalBuffer() { return _metalBuffer; }

    // Raccourcis vers les frame buffers
    FrameBuffer* writeBuffer()  { return &_state->buf[_state->write_idx]; }
    FrameBuffer* readBuffer()   { return &_state->buf[1 - _state->write_idx]; }

    // Swap les buffers (à appeler après que les deux côtés ont fini)
    void swap() { _state->write_idx = 1 - _state->write_idx; }

private:
    SharedState*  _state       = nullptr;
    MTL::Buffer*  _metalBuffer = nullptr;
    int           _fd          = -1;      // file descriptor du shm
    void*         _ptr         = nullptr; // pointeur mmap brut
};