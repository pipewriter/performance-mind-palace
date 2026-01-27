#pragma once

#include "chunk.h"
#include <FastNoiseLite.h>

class VolumeGenerator {
public:
    VolumeGenerator();

    // Generate SDF volume data for a chunk
    void generateChunk(VolumeChunk& chunk);

private:
    FastNoiseLite terrainNoise;    // Large-scale terrain shapes
    FastNoiseLite caveNoise;       // Cave systems
    FastNoiseLite detailNoise;     // Small-scale detail

    // Generate SDF value at a world position
    float generateSDF(glm::vec3 worldPos);
};
