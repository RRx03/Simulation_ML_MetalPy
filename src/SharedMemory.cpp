// SharedMemory.cpp
#include "SharedMemory.hpp"
#include <iostream>
#include <fcntl.h>      // O_CREAT, O_RDWR
#include <sys/mman.h>   // mmap, shm_open, shm_unlink
#include <sys/stat.h>   // mode constants
#include <unistd.h>     // ftruncate, close
#include <cstring>      // memset

bool SharedMemory::init(MTL::Device* device) {

    // -------------------------------------------------------
    // Étape 1 : Créer le segment de mémoire partagée POSIX
    // -------------------------------------------------------
    // shm_open crée un "fichier" en mémoire visible par tous les process
    // qui connaissent son nom. C'est PAS un fichier sur disque.
    // O_CREAT : le créer s'il n'existe pas
    // O_RDWR  : lecture + écriture
    // 0666    : permissions (lecture/écriture pour tout le monde)
    _fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (_fd == -1) {
        std::cerr << "shm_open failed" << std::endl;
        return false;
    }

    // -------------------------------------------------------
    // Étape 2 : Définir la taille du segment
    // -------------------------------------------------------
    // Le segment vient d'être créé avec une taille de 0.
    // ftruncate le redimensionne à la taille de notre SharedState.
    if (ftruncate(_fd, SHM_SIZE) == -1) {
        std::cerr << "ftruncate failed" << std::endl;
        close(_fd);
        shm_unlink(SHM_NAME);
        return false;
    }

    // -------------------------------------------------------
    // Étape 3 : Mapper le segment dans notre espace d'adressage
    // -------------------------------------------------------
    // mmap retourne un pointeur vers la mémoire partagée.
    // NULL    : laisser l'OS choisir l'adresse virtuelle
    // SHM_SIZE: taille à mapper
    // PROT_READ | PROT_WRITE : on veut lire ET écrire
    // MAP_SHARED : les modifications sont visibles par les autres process
    // _fd     : le file descriptor du segment
    // 0       : offset dans le segment (on mappe tout depuis le début)
    _ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
    if (_ptr == MAP_FAILED) {
        std::cerr << "mmap failed" << std::endl;
        close(_fd);
        shm_unlink(SHM_NAME);
        return false;
    }

    // -------------------------------------------------------
    // Étape 4 : Initialiser la mémoire à zéro
    // -------------------------------------------------------
    memset(_ptr, 0, SHM_SIZE);

    // On peut maintenant caster le pointeur brut en notre structure.
    // À partir d'ici, _state->buf[0] et _state->buf[1] sont les deux
    // frame buffers, et _state->write_idx dit lequel est actif.
    _state = static_cast<SharedState*>(_ptr);
    _state->write_idx = 0;

    // -------------------------------------------------------
    // Étape 5 : Créer un MTLBuffer qui pointe sur CETTE mémoire
    // -------------------------------------------------------
    // newBufferWithBytesNoCopy dit à Metal : "utilise ce pointeur
    // comme buffer GPU, ne copie rien, ne libère rien".
    // StorageModeShared : CPU et GPU peuvent lire/écrire.
    // Sur Apple Silicon, c'est la MÊME mémoire physique — zéro copie.
    // Le deallocator est nil car on gère la mémoire nous-mêmes (munmap).
    _metalBuffer = device->newBuffer(
        _ptr,
        SHM_SIZE,
        MTL::ResourceStorageModeShared,
        nullptr  // pas de deallocator — on cleanup nous-mêmes
    );

    if (!_metalBuffer) {
        std::cerr << "newBufferWithBytesNoCopy failed" << std::endl;
        munmap(_ptr, SHM_SIZE);
        close(_fd);
        shm_unlink(SHM_NAME);
        return false;
    }

    std::cout << "[SharedMemory] Initialized:" << std::endl;
    std::cout << "  Segment  : " << SHM_NAME << std::endl;
    std::cout << "  Size     : " << SHM_SIZE << " bytes" << std::endl;
    std::cout << "  Address  : " << _ptr << std::endl;
    std::cout << "  Buffer[0]: " << &_state->buf[0] << std::endl;
    std::cout << "  Buffer[1]: " << &_state->buf[1] << std::endl;

    return true;
}

void SharedMemory::cleanup() {
    // L'ordre est important : d'abord relâcher Metal, puis unmap, puis fermer.
    if (_metalBuffer) {
        _metalBuffer->release();
        _metalBuffer = nullptr;
    }
    if (_ptr && _ptr != MAP_FAILED) {
        munmap(_ptr, SHM_SIZE);
        _ptr = nullptr;
    }
    if (_fd != -1) {
        close(_fd);
        _fd = -1;
    }
    // shm_unlink supprime le segment du système.
    // Si Python tourne encore, il gardera son mapping (c'est safe),
    // mais ne pourra plus faire shm_open dessus.
    shm_unlink(SHM_NAME);
    _state = nullptr;

    std::cout << "[SharedMemory] Cleaned up." << std::endl;
}