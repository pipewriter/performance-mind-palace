#include "marching_cubes.h"
#include "marching_cubes_tables.h"
#include <cmath>

MarchingCubes::MarchingCubes() : isoLevel(0.0f) {
}

std::vector<MarchingCubesVertex> MarchingCubes::generateMesh(const VolumeChunk& chunk) {
    std::vector<MarchingCubesVertex> vertices;

    // Process each cube in the volume
    for (int x = 0; x < CHUNK_SIZE - 1; x++) {
        for (int y = 0; y < CHUNK_SIZE - 1; y++) {
            for (int z = 0; z < CHUNK_SIZE - 1; z++) {
                // Get the 8 corner values of this cube
                float cubeValues[8];
                cubeValues[0] = chunk.sdf[x][y][z];
                cubeValues[1] = chunk.sdf[x+1][y][z];
                cubeValues[2] = chunk.sdf[x+1][y][z+1];
                cubeValues[3] = chunk.sdf[x][y][z+1];
                cubeValues[4] = chunk.sdf[x][y+1][z];
                cubeValues[5] = chunk.sdf[x+1][y+1][z];
                cubeValues[6] = chunk.sdf[x+1][y+1][z+1];
                cubeValues[7] = chunk.sdf[x][y+1][z+1];

                // Determine the index into the edge table
                int cubeIndex = 0;
                for (int i = 0; i < 8; i++) {
                    if (cubeValues[i] < isoLevel) {
                        cubeIndex |= (1 << i);
                    }
                }

                // Cube is entirely inside or outside the surface
                if (edgeTable[cubeIndex] == 0) {
                    continue;
                }

                // Get corner positions
                glm::vec3 cornerPos[8];
                for (int i = 0; i < 8; i++) {
                    cornerPos[i] = getCornerPos(x, y, z, i, chunk);
                }

                // Find the vertices on each edge
                glm::vec3 vertList[12];

                if (edgeTable[cubeIndex] & 1)
                    vertList[0] = interpolate(cubeValues[0], cubeValues[1], cornerPos[0], cornerPos[1]);
                if (edgeTable[cubeIndex] & 2)
                    vertList[1] = interpolate(cubeValues[1], cubeValues[2], cornerPos[1], cornerPos[2]);
                if (edgeTable[cubeIndex] & 4)
                    vertList[2] = interpolate(cubeValues[2], cubeValues[3], cornerPos[2], cornerPos[3]);
                if (edgeTable[cubeIndex] & 8)
                    vertList[3] = interpolate(cubeValues[3], cubeValues[0], cornerPos[3], cornerPos[0]);
                if (edgeTable[cubeIndex] & 16)
                    vertList[4] = interpolate(cubeValues[4], cubeValues[5], cornerPos[4], cornerPos[5]);
                if (edgeTable[cubeIndex] & 32)
                    vertList[5] = interpolate(cubeValues[5], cubeValues[6], cornerPos[5], cornerPos[6]);
                if (edgeTable[cubeIndex] & 64)
                    vertList[6] = interpolate(cubeValues[6], cubeValues[7], cornerPos[6], cornerPos[7]);
                if (edgeTable[cubeIndex] & 128)
                    vertList[7] = interpolate(cubeValues[7], cubeValues[4], cornerPos[7], cornerPos[4]);
                if (edgeTable[cubeIndex] & 256)
                    vertList[8] = interpolate(cubeValues[0], cubeValues[4], cornerPos[0], cornerPos[4]);
                if (edgeTable[cubeIndex] & 512)
                    vertList[9] = interpolate(cubeValues[1], cubeValues[5], cornerPos[1], cornerPos[5]);
                if (edgeTable[cubeIndex] & 1024)
                    vertList[10] = interpolate(cubeValues[2], cubeValues[6], cornerPos[2], cornerPos[6]);
                if (edgeTable[cubeIndex] & 2048)
                    vertList[11] = interpolate(cubeValues[3], cubeValues[7], cornerPos[3], cornerPos[7]);

                // Create the triangles
                for (int i = 0; triTable[cubeIndex][i] != -1; i += 3) {
                    MarchingCubesVertex v1, v2, v3;

                    v1.pos = vertList[triTable[cubeIndex][i]];
                    v2.pos = vertList[triTable[cubeIndex][i+1]];
                    v3.pos = vertList[triTable[cubeIndex][i+2]];

                    // Calculate normals from SDF gradient
                    glm::vec3 localPos1 = v1.pos - chunkToWorldPos(chunk.coord);
                    glm::vec3 localPos2 = v2.pos - chunkToWorldPos(chunk.coord);
                    glm::vec3 localPos3 = v3.pos - chunkToWorldPos(chunk.coord);

                    glm::vec3 normal1 = calculateNormal(chunk, localPos1);
                    glm::vec3 normal2 = calculateNormal(chunk, localPos2);
                    glm::vec3 normal3 = calculateNormal(chunk, localPos3);

                    // Use normals as colors for now (visualize shading)
                    v1.color = normal1 * 0.5f + 0.5f;  // Map [-1,1] to [0,1]
                    v2.color = normal2 * 0.5f + 0.5f;
                    v3.color = normal3 * 0.5f + 0.5f;

                    vertices.push_back(v1);
                    vertices.push_back(v2);
                    vertices.push_back(v3);
                }
            }
        }
    }

    return vertices;
}

glm::vec3 MarchingCubes::getCornerPos(int x, int y, int z, int corner, const VolumeChunk& chunk) {
    // Corner offsets (matches lookup table convention)
    static const int cornerOffsets[8][3] = {
        {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1},
        {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}
    };

    glm::vec3 chunkWorldPos = chunkToWorldPos(chunk.coord);
    glm::vec3 localPos = glm::vec3(
        (x + cornerOffsets[corner][0]) * VOXEL_SIZE,
        (y + cornerOffsets[corner][1]) * VOXEL_SIZE,
        (z + cornerOffsets[corner][2]) * VOXEL_SIZE
    );

    return chunkWorldPos + localPos;
}

glm::vec3 MarchingCubes::interpolate(float val1, float val2, glm::vec3 pos1, glm::vec3 pos2) {
    // Linear interpolation to find zero crossing
    if (std::abs(isoLevel - val1) < 0.00001f)
        return pos1;
    if (std::abs(isoLevel - val2) < 0.00001f)
        return pos2;
    if (std::abs(val1 - val2) < 0.00001f)
        return pos1;

    float t = (isoLevel - val1) / (val2 - val1);
    return pos1 + t * (pos2 - pos1);
}

glm::vec3 MarchingCubes::calculateNormal(const VolumeChunk& chunk, glm::vec3 localPos) {
    // Calculate gradient using central differences
    const float h = VOXEL_SIZE * 0.5f;

    float dx = sampleSDF(chunk, localPos + glm::vec3(h, 0, 0)) -
               sampleSDF(chunk, localPos - glm::vec3(h, 0, 0));
    float dy = sampleSDF(chunk, localPos + glm::vec3(0, h, 0)) -
               sampleSDF(chunk, localPos - glm::vec3(0, h, 0));
    float dz = sampleSDF(chunk, localPos + glm::vec3(0, 0, h)) -
               sampleSDF(chunk, localPos - glm::vec3(0, 0, h));

    glm::vec3 normal = glm::vec3(dx, dy, dz);
    float len = glm::length(normal);
    if (len > 0.00001f) {
        normal /= len;
    } else {
        normal = glm::vec3(0, 1, 0);  // Default up
    }

    return normal;
}

float MarchingCubes::sampleSDF(const VolumeChunk& chunk, glm::vec3 localPos) {
    // Convert local position to voxel coordinates
    glm::vec3 voxelPos = localPos / VOXEL_SIZE;

    // Clamp to valid range
    voxelPos = glm::clamp(voxelPos, glm::vec3(0.0f), glm::vec3(CHUNK_SIZE - 1.001f));

    // Trilinear interpolation
    int x0 = (int)voxelPos.x;
    int y0 = (int)voxelPos.y;
    int z0 = (int)voxelPos.z;
    int x1 = std::min(x0 + 1, CHUNK_SIZE - 1);
    int y1 = std::min(y0 + 1, CHUNK_SIZE - 1);
    int z1 = std::min(z0 + 1, CHUNK_SIZE - 1);

    float fx = voxelPos.x - x0;
    float fy = voxelPos.y - y0;
    float fz = voxelPos.z - z0;

    // Interpolate along x
    float c00 = chunk.sdf[x0][y0][z0] * (1 - fx) + chunk.sdf[x1][y0][z0] * fx;
    float c01 = chunk.sdf[x0][y0][z1] * (1 - fx) + chunk.sdf[x1][y0][z1] * fx;
    float c10 = chunk.sdf[x0][y1][z0] * (1 - fx) + chunk.sdf[x1][y1][z0] * fx;
    float c11 = chunk.sdf[x0][y1][z1] * (1 - fx) + chunk.sdf[x1][y1][z1] * fx;

    // Interpolate along y
    float c0 = c00 * (1 - fy) + c10 * fy;
    float c1 = c01 * (1 - fy) + c11 * fy;

    // Interpolate along z
    return c0 * (1 - fz) + c1 * fz;
}
