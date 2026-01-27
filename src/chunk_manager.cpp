#include "chunk_manager.h"
#include <iostream>

ChunkManager::ChunkManager() {
}

VolumeChunk* ChunkManager::getOrCreateChunk(ChunkCoord coord) {
    // Check if chunk already exists
    auto it = chunks.find(coord);
    if (it != chunks.end()) {
        return it->second.get();
    }

    // Create new chunk
    auto chunk = std::make_unique<VolumeChunk>();
    chunk->coord = coord;

    // Generate volume data
    generator.generateChunk(*chunk);

    // Store and return
    VolumeChunk* ptr = chunk.get();
    chunks[coord] = std::move(chunk);

    return ptr;
}

VolumeChunk* ChunkManager::getChunk(ChunkCoord coord) {
    auto it = chunks.find(coord);
    if (it != chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

void ChunkManager::updateChunks(glm::vec3 cameraPos, int loadRadius) {
    ChunkCoord cameraChunk = worldToChunkCoord(cameraPos);

    // Load chunks in a cube around camera
    for (int dx = -loadRadius; dx <= loadRadius; dx++) {
        for (int dy = -loadRadius; dy <= loadRadius; dy++) {
            for (int dz = -loadRadius; dz <= loadRadius; dz++) {
                ChunkCoord coord = {
                    cameraChunk.x + dx,
                    cameraChunk.y + dy,
                    cameraChunk.z + dz
                };

                // Create chunk if it doesn't exist
                getOrCreateChunk(coord);
            }
        }
    }

    // TODO: Unload distant chunks (Phase 5)
}

void ChunkManager::clear() {
    chunks.clear();
}
