#include "volume_generator.h"
#include <iostream>
#include <glm/glm.hpp>

VolumeGenerator::VolumeGenerator() {
    // Large-scale terrain shapes (continents, mountains)
    terrainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    terrainNoise.SetFrequency(0.005f);  // Very large features
    terrainNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    terrainNoise.SetFractalOctaves(4);
    terrainNoise.SetFractalLacunarity(2.0f);
    terrainNoise.SetFractalGain(0.5f);

    // Cave systems (worm-like tunnels)
    caveNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    caveNoise.SetFrequency(0.02f);  // Medium-sized caves
    caveNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    caveNoise.SetFractalOctaves(2);

    // Small-scale detail (bumps, cracks)
    detailNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    detailNoise.SetFrequency(0.05f);  // Fine detail
    detailNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    detailNoise.SetFractalOctaves(2);
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

}

float VolumeGenerator::generateSDF(glm::vec3 worldPos) {
    // True volumetric terrain generation using 3D density functions

    // 1. Start with STRONG 3D base terrain density
    float baseDensity = terrainNoise.GetNoise(worldPos.x, worldPos.y, worldPos.z) * 3.0f;  // Amplified!

    // 2. WEAK vertical gradient so overhangs can form
    float verticalGradient = -worldPos.y * 0.04f;  // Reduced from 0.08

    // 3. Combine base density with gradient
    float density = baseDensity + verticalGradient;

    // 4. Add dramatic ground variation
    float groundVariation = terrainNoise.GetNoise(worldPos.x * 0.2f, 0.0f, worldPos.z * 0.2f) * 15.0f;  // More dramatic!
    density += (groundVariation - worldPos.y) * 0.03f;

    // 5. AGGRESSIVE cave carving
    float caves = caveNoise.GetNoise(worldPos.x, worldPos.y, worldPos.z);
    float caveThreshold = 0.3f;  // Lower threshold = more caves

    // Caves everywhere (not just underground)
    if (caves > caveThreshold) {
        float caveCarve = (caves - caveThreshold) * 8.0f;  // Much stronger carving!
        density -= caveCarve;
    }

    // 6. STRONG floating islands in sky (y > 15)
    if (worldPos.y > 15.0f && worldPos.y < 40.0f) {
        float islandNoise = terrainNoise.GetNoise(worldPos.x * 0.4f, worldPos.y * 0.4f, worldPos.z * 0.4f);
        float islandDensity = islandNoise * 4.0f;  // Much stronger!
        density += islandDensity;
    }

    // 7. Add fine detail
    float detail = detailNoise.GetNoise(worldPos.x, worldPos.y, worldPos.z) * 0.5f;
    density += detail;

    // 8. Convert density to SDF
    // Positive = solid, Negative = air, 0 = surface
    return density * 3.0f;  // Reduced scaling for more gradual transitions
}
