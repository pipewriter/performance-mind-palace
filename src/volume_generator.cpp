#include "volume_generator.h"
#include <iostream>

VolumeGenerator::VolumeGenerator() {
    // Configure noise for terrain generation
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetFrequency(0.02f);  // Lower = smoother terrain
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(3);
}

void VolumeGenerator::generateChunk(VolumeChunk& chunk) {
    glm::vec3 chunkWorldPos = chunkToWorldPos(chunk.coord);

    // Set chunk bounds
    chunk.worldMin = chunkWorldPos;
    chunk.worldMax = chunkWorldPos + glm::vec3(CHUNK_WORLD_SIZE);

    // Generate SDF for each voxel
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                // World position of this voxel
                glm::vec3 voxelWorldPos = chunkWorldPos + glm::vec3(
                    x * VOXEL_SIZE,
                    y * VOXEL_SIZE,
                    z * VOXEL_SIZE
                );

                // Generate SDF value
                chunk.sdf[x][y][z] = generateSDF(voxelWorldPos);
            }
        }
    }

    std::cout << "Generated chunk at (" << chunk.coord.x << ", "
              << chunk.coord.y << ", " << chunk.coord.z << ")" << std::endl;
}

float VolumeGenerator::generateSDF(glm::vec3 worldPos) {
    // Simple terrain: ground plane with noise

    // Get height from 2D noise
    float terrainHeight = noise.GetNoise(worldPos.x, worldPos.z) * 10.0f;

    // Base ground level at y = 0
    terrainHeight += 0.0f;

    // SDF: positive = below ground (solid), negative = above ground (air)
    float distToGround = terrainHeight - worldPos.y;

    // Add some 3D caves/overhangs with 3D noise (optional)
    float caveNoise = noise.GetNoise(worldPos.x * 0.1f, worldPos.y * 0.1f, worldPos.z * 0.1f);
    if (caveNoise > 0.6f && worldPos.y < 0) {
        // Carve out caves underground
        distToGround = -5.0f;  // Air
    }

    return distToGround;
}
