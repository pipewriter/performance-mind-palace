#pragma once

#include "chunk.h"
#include "volume_generator.h"
#include <unordered_map>
#include <memory>

class ChunkManager {
public:
    ChunkManager();

    // Get a chunk at the given coordinate (creates if doesn't exist)
    VolumeChunk* getOrCreateChunk(ChunkCoord coord);

    // Get chunk without creating
    VolumeChunk* getChunk(ChunkCoord coord);

    // Load chunks around a position
    void updateChunks(glm::vec3 cameraPos, int loadRadius);

    // Get all loaded chunks
    const std::unordered_map<ChunkCoord, std::unique_ptr<VolumeChunk>>& getChunks() const {
        return chunks;
    }

    // Clear all chunks
    void clear();

private:
    std::unordered_map<ChunkCoord, std::unique_ptr<VolumeChunk>> chunks;
    VolumeGenerator generator;
};
