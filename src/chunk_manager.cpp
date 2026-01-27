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

std::vector<VolumeChunk*> ChunkManager::updateChunks(glm::vec3 cameraPos, int loadRadius, int unloadRadius) {
    ChunkCoord cameraChunk = worldToChunkCoord(cameraPos);
    std::vector<VolumeChunk*> newChunks;

    // Load chunks in a cube around camera
    for (int dx = -loadRadius; dx <= loadRadius; dx++) {
        for (int dy = -loadRadius; dy <= loadRadius; dy++) {
            for (int dz = -loadRadius; dz <= loadRadius; dz++) {
                ChunkCoord coord = {
                    cameraChunk.x + dx,
                    cameraChunk.y + dy,
                    cameraChunk.z + dz
                };

                // Check if chunk already exists
                auto it = chunks.find(coord);
                if (it == chunks.end()) {
                    // New chunk - create and add to list
                    VolumeChunk* chunk = getOrCreateChunk(coord);
                    newChunks.push_back(chunk);
                }
            }
        }
    }

    // Unload distant chunks
    std::vector<ChunkCoord> toRemove;
    for (const auto& [coord, chunk] : chunks) {
        int dx = coord.x - cameraChunk.x;
        int dy = coord.y - cameraChunk.y;
        int dz = coord.z - cameraChunk.z;

        // Check if outside unload radius
        if (abs(dx) > unloadRadius || abs(dy) > unloadRadius || abs(dz) > unloadRadius) {
            toRemove.push_back(coord);
        }
    }

    // Remove chunks marked for removal
    for (const auto& coord : toRemove) {
        chunks.erase(coord);
    }

    // Log summary if anything changed
    if (!newChunks.empty() || !toRemove.empty()) {
        std::cout << "Chunk update: +" << newChunks.size() << " loaded, -" << toRemove.size()
                  << " unloaded (total: " << chunks.size() << ")" << std::endl;
    }

    return newChunks;
}

void ChunkManager::clear() {
    chunks.clear();
}
