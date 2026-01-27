#pragma once

#include "chunk.h"
#include <vector>

// Vertex structure matching what Vulkan expects
struct MarchingCubesVertex {
    glm::vec3 pos;
    glm::vec3 color;
};

class MarchingCubes {
public:
    MarchingCubes();

    // Generate mesh from chunk SDF data
    std::vector<MarchingCubesVertex> generateMesh(const VolumeChunk& chunk);

private:
    float isoLevel;  // Surface threshold (0.0 for SDF)

    // Get corner positions for a cube at (x,y,z)
    glm::vec3 getCornerPos(int x, int y, int z, int corner, const VolumeChunk& chunk);

    // Interpolate position between two corners
    glm::vec3 interpolate(float val1, float val2, glm::vec3 pos1, glm::vec3 pos2);

    // Calculate normal from SDF gradient (for smooth shading)
    glm::vec3 calculateNormal(const VolumeChunk& chunk, glm::vec3 localPos);

    // Sample SDF at a local position (with trilinear interpolation)
    float sampleSDF(const VolumeChunk& chunk, glm::vec3 localPos);
};
