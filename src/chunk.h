#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <functional>

// Chunk size constants
constexpr int CHUNK_CUBES = 32;     // 32x32x32 marching cubes grid
constexpr int CHUNK_SIZE = CHUNK_CUBES + 1;  // 33x33x33 voxel grid (need +1 for cube corners)
constexpr float CHUNK_WORLD_SIZE = 16.0f;  // 16 meters per chunk
constexpr float VOXEL_SIZE = CHUNK_WORLD_SIZE / CHUNK_CUBES;  // Size of each cube (0.5m)

// Chunk coordinate in chunk space (not world space)
struct ChunkCoord {
    int x, y, z;

    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const ChunkCoord& other) const {
        return !(*this == other);
    }
};

// Hash function for ChunkCoord (for unordered_map)
namespace std {
    template<>
    struct hash<ChunkCoord> {
        size_t operator()(const ChunkCoord& c) const {
            // Simple hash combining
            size_t h1 = hash<int>()(c.x);
            size_t h2 = hash<int>()(c.y);
            size_t h3 = hash<int>()(c.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

// Chunk containing SDF volume data
struct VolumeChunk {
    ChunkCoord coord;

    // SDF volume data: positive = inside solid, negative = air, 0 = surface
    float sdf[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];

    // Vulkan mesh data (generated from SDF)
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    uint32_t vertexCount = 0;

    // State flags
    bool meshGenerated = false;
    bool meshUploaded = false;

    // Bounding box in world space
    glm::vec3 worldMin;
    glm::vec3 worldMax;

    VolumeChunk() {
        // Initialize SDF to "air" everywhere
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int y = 0; y < CHUNK_SIZE; y++) {
                for (int z = 0; z < CHUNK_SIZE; z++) {
                    sdf[x][y][z] = -1.0f;  // Air
                }
            }
        }
    }
};

// Helper functions
inline glm::vec3 chunkToWorldPos(ChunkCoord coord) {
    return glm::vec3(
        coord.x * CHUNK_WORLD_SIZE,
        coord.y * CHUNK_WORLD_SIZE,
        coord.z * CHUNK_WORLD_SIZE
    );
}

inline ChunkCoord worldToChunkCoord(glm::vec3 worldPos) {
    return ChunkCoord{
        static_cast<int>(floor(worldPos.x / CHUNK_WORLD_SIZE)),
        static_cast<int>(floor(worldPos.y / CHUNK_WORLD_SIZE)),
        static_cast<int>(floor(worldPos.z / CHUNK_WORLD_SIZE))
    };
}
