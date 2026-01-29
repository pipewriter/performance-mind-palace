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

    // Load chunks around a position and unload distant ones
    // Returns newly loaded chunks (for mesh generation)
    std::vector<VolumeChunk*> updateChunks(glm::vec3 cameraPos, int loadRadius, int unloadRadius);

    void generateChunkSdf(VolumeChunk& chunk) { generator.generateChunk(chunk); }

    // Get all loaded chunks
    const std::unordered_map<ChunkCoord, std::unique_ptr<VolumeChunk>>& getChunks() const {
        return chunks;
    }

    // Non-const version for modifications
    std::unordered_map<ChunkCoord, std::unique_ptr<VolumeChunk>>& getChunks() {
        return chunks;
    }

    // Clear all chunks
    void clear();

private:
    std::unordered_map<ChunkCoord, std::unique_ptr<VolumeChunk>> chunks;
    VolumeGenerator generator;
};
