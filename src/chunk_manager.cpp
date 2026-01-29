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

    // Load chunks - use smaller vertical radius (don't need chunks far above/below)
    int verticalRadius = 1;  // Only load 1 chunk above/below
    for (int dx = -loadRadius; dx <= loadRadius; dx++) {
        for (int dy = -verticalRadius; dy <= verticalRadius; dy++) {
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

    // Unload distant chunks (use larger vertical unload radius)
    std::vector<ChunkCoord> toRemove;
    int verticalUnloadRadius = unloadRadius * 2;  // Keep more chunks vertically to prevent thrashing
    for (const auto& [coord, chunk] : chunks) {
        int dx = coord.x - cameraChunk.x;
        int dy = coord.y - cameraChunk.y;
        int dz = coord.z - cameraChunk.z;

        // Check if outside unload radius (different for vertical)
        if (abs(dx) > unloadRadius || abs(dy) > verticalUnloadRadius || abs(dz) > unloadRadius) {
            if (!chunk->generationQueued.load() && !chunk->generationInProgress.load() &&
                !chunk->uploadInProgress.load()) {
                toRemove.push_back(coord);
            }
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
