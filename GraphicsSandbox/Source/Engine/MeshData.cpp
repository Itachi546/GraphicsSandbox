#include "MeshData.h"
#include "Components.h"

static uint8_t encodeNormal(float p) {
	return (uint8_t)(p * 127.0f + 127.0f);
}

Vertex createVertex(const glm::vec3& p, const glm::vec3& n, const glm::vec2& uv) {
	Vertex vertex;
	vertex.px = p.x;
	vertex.py = p.y;
	vertex.pz = p.z;
	
	vertex.nx = encodeNormal(n.x);
	vertex.ny = encodeNormal(n.y);
	vertex.nz = encodeNormal(n.z);

	vertex.ux = uv.x;
	vertex.uy = uv.y;

	vertex.tx = encodeNormal(0.0f);
	vertex.ty = encodeNormal(0.0f);
	vertex.tz = encodeNormal(0.0f);

	vertex.bx = encodeNormal(0.0f);
	vertex.by = encodeNormal(0.0f);
	vertex.bz = encodeNormal(0.0f);
	return vertex;
}

void InitializeCubeMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
	vertices = {
		createVertex(glm::vec3(-1.0f, +1.0f, +1.0f), glm::vec3(+0.0f, +1.0f, +0.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(+1.0f, +1.0f, +1.0f), glm::vec3(+0.0f, +1.0f, +0.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(+1.0f, +1.0f, -1.0f), glm::vec3(+0.0f, +1.0f, +0.0f), glm::vec2(0.0f)),

		createVertex(glm::vec3(-1.0f, +1.0f, -1.0f), glm::vec3(+0.0f, +1.0f, +0.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(-1.0f, +1.0f, -1.0f), glm::vec3(+0.0f, +0.0f, -1.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(+1.0f, +1.0f, -1.0f), glm::vec3(+0.0f, +0.0f, -1.0f), glm::vec2(0.0f)),

		createVertex(glm::vec3(+1.0f, -1.0f, -1.0f), glm::vec3(+0.0f, +0.0f, -1.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(+0.0f, +0.0f, -1.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(+1.0f, +1.0f, -1.0f), glm::vec3(+1.0f, +0.0f, +0.0f), glm::vec2(0.0f)),

		createVertex(glm::vec3(+1.0f, +1.0f, +1.0f), glm::vec3(+1.0f, +0.0f, +0.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(+1.0f, -1.0f, +1.0f), glm::vec3(+1.0f, +0.0f, +0.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(+1.0f, -1.0f, -1.0f), glm::vec3(+1.0f, +0.0f, +0.0f), glm::vec2(0.0f)),

		createVertex(glm::vec3(-1.0f, +1.0f, +1.0f), glm::vec3(-1.0f, +0.0f, +0.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(-1.0f, +1.0f, -1.0f), glm::vec3(-1.0f, +0.0f, +0.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(-1.0f, +0.0f, +0.0f), glm::vec2(0.0f)),

		createVertex(glm::vec3(-1.0f, -1.0f, +1.0f), glm::vec3(-1.0f, +0.0f, +0.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(+1.0f, +1.0f, +1.0f), glm::vec3(+0.0f, +0.0f, +1.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(-1.0f, +1.0f, +1.0f), glm::vec3(+0.0f, +0.0f, +1.0f), glm::vec2(0.0f)),

		createVertex(glm::vec3(-1.0f, -1.0f, +1.0f), glm::vec3(+0.0f, +0.0f, +1.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(+1.0f, -1.0f, +1.0f), glm::vec3(+0.0f, +0.0f, +1.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(+1.0f, -1.0f, -1.0f), glm::vec3(+0.0f, -1.0f, +0.0f), glm::vec2(0.0f)),

		createVertex(glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(+0.0f, -1.0f, +0.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(-1.0f, -1.0f, +1.0f), glm::vec3(+0.0f, -1.0f, +0.0f), glm::vec2(0.0f)),
		createVertex(glm::vec3(+1.0f, -1.0f, +1.0f), glm::vec3(+0.0f, -1.0f, +0.0f), glm::vec2(0.0f)),
	};

	indices = {
		0,   1,  2,  0,  2,  3, // Top
		4,   5,  6,  4,  6,  7, // Front
		8,   9, 10,  8, 10, 11, // Right
		12, 13, 14, 12, 14, 15, // Left
		16, 17, 18, 16, 18, 19, // Back
		20, 22, 21, 20, 23, 22, // Bottom
	};
}

void InitializePlaneMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, uint32_t subdivide)
{
	float dims = (float)subdivide;
	float stepSize = 2.0f / float(subdivide);

	for (uint32_t y = 0; y <= subdivide; ++y)
	{
		float yPos = y * stepSize - 1.0f;
		for (uint32_t x = 0; x <= subdivide; ++x)
		{
			Vertex vertex = createVertex(glm::vec3(x * stepSize - 1.0f, 0.0f, yPos),
				glm::vec3(0.0f, 1.0f, 0.0f),
				glm::vec2(x * stepSize, y * stepSize));
			vertices.push_back(vertex);
			/*
			vertices.push_back(createVertex(glm::vec3(x * stepSize - 1.0f, 0.0f, yPos),
				glm::vec3(0.0f, 1.0f, 0.0f),
				glm::vec2(x * stepSize, y * stepSize)));
				*/
		}
	}

	for (uint32_t y = 0; y < subdivide; ++y)
	{
		for (uint32_t x = 0; x < subdivide; ++x)
		{
			uint32_t i0 = y * (subdivide + 1) + x;
			uint32_t i1 = i0 + 1;
			uint32_t i2 = i0 + subdivide + 1;
			uint32_t i3 = i2 + 1;

			indices.push_back(i0);  indices.push_back(i2); indices.push_back(i1);
			indices.push_back(i2);  indices.push_back(i3); indices.push_back(i1);
		}
	}
}

void InitializeSphereMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
	const int nSector = 32;
	const int nRing = 32;

	// x = r * sinTheta * cosPhi
	// y = r * sinTheta * sinPhi
	// z = r * cosTheta

	constexpr float pi = glm::pi<float>();
	constexpr float kdTheta = pi / float(nRing);
	constexpr float kdPhi = (pi * 2.0f) / float(nSector);

	for (int s = 0; s <= nRing; ++s)
	{
		float theta = pi * 0.5f - s * kdTheta;
		float cosTheta = static_cast<float>(cos(theta));
		float sinTheta = static_cast<float>(sin(theta));
		for (int r = 0; r <= nSector; ++r)
		{
			float phi = r * kdPhi;
			float x = cosTheta * static_cast<float>(cos(phi));
			float y = sinTheta;
			float z = cosTheta * static_cast<float>(sin(phi));
			vertices.push_back(createVertex(glm::vec3(x, y, z), glm::vec3(x, y, z), glm::vec2(0.0f, 0.0f)));
		}
	}

	uint32_t numIndices = (nSector + 1) * (nRing + 1) * 6;

	for (unsigned int r = 0; r < nRing; ++r)
	{
		for (unsigned int s = 0; s < nSector; ++s)
		{
			unsigned int i0 = r * (nSector + 1) + s;
			unsigned int i1 = i0 + (nSector + 1);

			indices.push_back(i0);
			indices.push_back(i0 + 1);
			indices.push_back(i1);

			indices.push_back(i1);
			indices.push_back(i0 + 1);
			indices.push_back(i1 + 1);
		}
	}
}