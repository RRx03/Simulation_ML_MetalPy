"""
main.py — Côté Python de la simulation
Se connecte au segment de mémoire partagée créé par C++.
"""
import mmap
import struct
import ctypes
import numpy as np
import time
from multiprocessing import resource_tracker

def _remove_shm_tracking(name):
    """Supprime le tracking d'un segment spécifique."""
    try:
        resource_tracker._resource_tracker.unregister(f"/{name}", "shared_memory")
    except Exception:
        pass
# -------------------------------------------------------
# Ces constantes DOIVENT correspondre exactement à Common.h
# Si tu changes Common.h, tu dois changer ici aussi.
# -------------------------------------------------------
SHM_NAME       = "/evo_sim"
MAX_CREATURES  = 200
NUM_INPUTS     = 38
NUM_ACTIONS    = 5
NUM_BUFFERS    = 2

# Taille d'un FrameBuffer (doit matcher le sizeof en C++)
# uint32 num_alive + uint32 tick
# + float[200][38] perceptions
# + float[200] energies
# + uint32[200] creature_ids
# + float[200][5] actions
FRAMEBUFFER_SIZE = (
    4 +                           # num_alive (uint32)
    4 +                           # tick (uint32)
    MAX_CREATURES * NUM_INPUTS * 4 +  # perceptions (float32)
    MAX_CREATURES * 4 +               # energies (float32)
    MAX_CREATURES * 4 +               # creature_ids (uint32)
    MAX_CREATURES * NUM_ACTIONS * 4   # actions (float32)
)

# Taille du header SharedState (write_idx + padding)
HEADER_SIZE = 4 + 15 * 4  # write_idx (uint32) + _pad[15] (uint32 * 15) = 64 bytes

# Taille totale
SHM_SIZE = HEADER_SIZE + FRAMEBUFFER_SIZE * NUM_BUFFERS

# Offsets à l'intérieur d'un FrameBuffer
OFF_NUM_ALIVE    = 0
OFF_TICK         = 4
OFF_PERCEPTIONS  = 8
OFF_ENERGIES     = OFF_PERCEPTIONS + MAX_CREATURES * NUM_INPUTS * 4
OFF_CREATURE_IDS = OFF_ENERGIES + MAX_CREATURES * 4
OFF_ACTIONS      = OFF_CREATURE_IDS + MAX_CREATURES * 4


class SharedMemoryClient:
    """
    Se connecte au segment de mémoire partagée créé par le moteur C++.
    Permet de lire les perceptions et d'écrire les actions.
    """

    def __init__(self):
        self.fd = None
        self.mm = None

    def connect(self):
        """
        Ouvre le segment de mémoire partagée.
        Le moteur C++ DOIT l'avoir créé avant.
        """
        # Ouvrir le segment POSIX par son nom.
        # C'est le même nom que dans shm_open() côté C++.
        # On n'utilise pas O_CREAT : si le segment n'existe pas, ça échoue.
        import posixshmem
        self.fd = posixshmem.shm_open(SHM_NAME)

        # Alternative sans posixshmem (plus portable) :
        # Sur macOS, les segments shm POSIX apparaissent comme des fichiers
        # dans /dev/shm/ (Linux) ou sont accessibles via ctypes.
        # Mais la méthode la plus simple sur macOS est via multiprocessing :
        import multiprocessing.shared_memory as sm
        self.shm = sm.SharedMemory(name="evo_sim", create=False)
        self.mm = self.shm.buf

        print(f"[Python] Connected to shared memory '{SHM_NAME}'")
        print(f"[Python] Size: {len(self.mm)} bytes (expected {SHM_SIZE})")

    def connect_simple(self):
        """
        Méthode de connexion simplifiée avec multiprocessing.shared_memory.
        C'est la méthode recommandée sur macOS.

        Note : multiprocessing.shared_memory ajoute un '/' devant le nom
        automatiquement sur certains OS, et le retire sur d'autres.
        Le nom passé ici ne doit PAS contenir le '/' initial.
        """
        import multiprocessing.shared_memory as sm
        # "evo_sim" sans le '/' — shared_memory le gère
        self.shm = sm.SharedMemory(name="evo_sim", create=False)
        self.mm = self.shm.buf
        print(f"[Python] Connected. Size: {len(self.mm)} bytes")

    def get_write_idx(self) -> int:
        """Lit l'index du buffer actif (celui que C++ vient d'écrire)."""
        return struct.unpack_from('I', self.mm, 0)[0]

    def _buf_offset(self, buf_idx: int) -> int:
        """Calcule l'offset en octets du début du FrameBuffer[buf_idx]."""
        return HEADER_SIZE + buf_idx * FRAMEBUFFER_SIZE

    def read_num_alive(self, buf_idx: int) -> int:
        """Combien de créatures sont vivantes dans ce buffer."""
        offset = self._buf_offset(buf_idx) + OFF_NUM_ALIVE
        return struct.unpack_from('I', self.mm, offset)[0]

    def read_tick(self, buf_idx: int) -> int:
        offset = self._buf_offset(buf_idx) + OFF_TICK
        return struct.unpack_from('I', self.mm, offset)[0]

    def read_perceptions(self, buf_idx: int) -> np.ndarray:
        """
        Lit les perceptions comme un numpy array (num_alive, 38).
        C'est un VIEW sur la mémoire partagée — zéro copie.
        """
        n = self.read_num_alive(buf_idx)
        offset = self._buf_offset(buf_idx) + OFF_PERCEPTIONS

        # np.frombuffer crée un array qui POINTE sur la mémoire partagée.
        # Pas de copie. Si C++ modifie la mémoire, numpy voit le changement.
        # C'est pourquoi les sémaphores sont essentiels.
        all_perceptions = np.frombuffer(
            self.mm,
            dtype=np.float32,
            count=MAX_CREATURES * NUM_INPUTS,
            offset=offset
        ).reshape(MAX_CREATURES, NUM_INPUTS)

        # On ne retourne que les lignes des créatures vivantes
        return all_perceptions[:n]

    def read_energies(self, buf_idx: int) -> np.ndarray:
        n = self.read_num_alive(buf_idx)
        offset = self._buf_offset(buf_idx) + OFF_ENERGIES
        all_energies = np.frombuffer(
            self.mm,
            dtype=np.float32,
            count=MAX_CREATURES,
            offset=offset
        )
        return all_energies[:n]

    def write_actions(self, buf_idx: int, actions: np.ndarray):
        """
        Écrit les actions (numpy float32, shape (n, 5)) dans le buffer.
        """
        offset = self._buf_offset(buf_idx) + OFF_ACTIONS
        # Écrire directement dans la mémoire partagée
        data = actions.astype(np.float32).tobytes()
        self.mm[offset:offset + len(data)] = data

    def disconnect(self):
        if self.shm:
            self.shm.close()
            # NE PAS appeler shm.unlink() ici — c'est C++ qui gère le lifecycle
            self.shm = None
        print("[Python] Disconnected from shared memory")


# -------------------------------------------------------
# Exemple d'utilisation
# -------------------------------------------------------
if __name__ == "__main__":
    import torch
    # from brain import Brain, Population  # ton code de cerveau

    client = SharedMemoryClient()

    print("Waiting for C++ engine to create shared memory...")
    while True:
        try:
            client.connect_simple()
            _remove_shm_tracking("evo_sim")
            break
        except FileNotFoundError:
            time.sleep(0.1)

    print("Connected! Starting brain loop.")

    try:
        while True:
            # TODO: remplacer par sem_wait quand tu auras les sémaphores
            time.sleep(0.016)  # ~60 fps placeholder

            buf_idx = client.get_write_idx()
            n = client.read_num_alive(buf_idx)

            if n == 0:
                continue

            # Lire les perceptions (numpy view, zero-copy)
            perceptions = client.read_perceptions(buf_idx)
            print(f"Tick {client.read_tick(buf_idx)} | "
                  f"Alive: {n} | "
                  f"Perception[0][:3] = {perceptions[0][:3]}")

            # TODO: forward pass PyTorch ici
            # states = torch.from_numpy(perceptions)
            # actions = population.think(states)

            # Pour l'instant, actions aléatoires
            actions = np.random.randn(n, NUM_ACTIONS).astype(np.float32)
            client.write_actions(buf_idx, actions)

            # TODO: sem_post pour signaler à C++ que les actions sont prêtes

    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        client.disconnect()