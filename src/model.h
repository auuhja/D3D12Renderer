#pragma once

#include "common.h"
#include "math.h"

#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct vertex_3P
{
	vec3 position;
};

struct vertex_3PUN
{
	vec3 position;
	vec2 uv;
	vec3 normal;
};

struct vertex_3PUNT
{
	vec3 position;
	vec2 uv;
	vec3 normal;
	vec3 tangent;
};

struct indexed_triangle32
{
	uint32 a, b, c;
};

struct cpu_render_material_desc
{
	std::wstring albedo;
	float r, g, b;
};

defineHasMember(position);
defineHasMember(normal);
defineHasMember(tangent);
defineHasMember(uv);

template <typename vertex_t>
struct cpu_mesh
{
	std::vector<vertex_t> vertices;
	std::vector<indexed_triangle32> triangles;

	cpu_render_material_desc material;

	static cpu_mesh quad(float radius = 1.f);
	static cpu_mesh cube(float radius = 1.f, bool invertWindingOrder = false);
	static cpu_mesh sphere(uint32 slices, uint32 rows, float radius = 1.f);
	static cpu_mesh capsule(uint32 slices, uint32 rows, float height, float radius = 1.f);

private:
	template <typename vertex_t> friend struct cpu_mesh_group;

	void loadFromAssimp(const std::filesystem::path& parentPath, const aiScene* scene, uint32 meshIndex);

	void setPosition(vertex_t& v, vec3 p)
	{
		if constexpr (hasMember(vertex_t, position))
		{
			v.position = p;
		}
	}

	void setNormal(vertex_t& v, vec3 n)
	{
		if constexpr (hasMember(vertex_t, normal))
		{
			v.normal = n;
		}
	}

	void setTangent(vertex_t& v, vec3 n)
	{
		if constexpr (hasMember(vertex_t, tangent))
		{
			v.tangent = n;
		}
	}

	void setUV(vertex_t& v, vec2 uv)
	{
		if constexpr (hasMember(vertex_t, uv))
		{
			v.uv = uv;
		}
	}
};

template <typename vertex_t>
struct cpu_mesh_group
{
	std::vector<cpu_mesh<vertex_t>> meshes;

	void loadFromFile(const std::string& filename);
};

template<typename vertex_t>
inline void cpu_mesh_group<vertex_t>::loadFromFile(const std::string& filename)
{
	std::filesystem::path path(filename);
	assert(std::filesystem::exists(path));

	std::filesystem::path exportPath = path;
	exportPath.replace_extension("assbin");

	std::filesystem::path parent = path.parent_path();

	Assimp::Importer importer;
	const aiScene* scene;

	// Check if a preprocessed file exists.
	if (std::filesystem::exists(exportPath) && std::filesystem::is_regular_file(exportPath))
	{
		scene = importer.ReadFile(exportPath.string(), 0);
	}
	else
	{
		// File has not been preprocessed yet. Import and processes the file.
		importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.0f);
		importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

		uint32 preprocessFlags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph | aiProcess_CalcTangentSpace;
		scene = importer.ReadFile(path.string(), preprocessFlags);

		if (scene)
		{
			// Export the preprocessed scene file for faster loading next time.
			Assimp::Exporter exporter;
			exporter.Export(scene, "assbin", exportPath.string(), preprocessFlags);
		}
	}

	if (scene)
	{
		meshes.resize(scene->mNumMeshes);
		for (uint32 i = 0; i < scene->mNumMeshes; ++i)
		{
			meshes[i].loadFromAssimp(parent, scene, i);
		}
	}
}

template<typename vertex_t>
inline void cpu_mesh<vertex_t>::loadFromAssimp(const std::filesystem::path& parentPath, const aiScene* scene, uint32 meshIndex)
{
	const aiMesh* mesh = scene->mMeshes[meshIndex];
	const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

	vertices.resize(mesh->mNumVertices);
	triangles.resize(mesh->mNumFaces);

	vec3 position(0.f, 0.f, 0.f);
	vec3 normal(0.f, 0.f, 0.f);
	vec3 tangent(0.f, 0.f, 0.f);
	vec2 uv(0.f, 0.f);

	for (uint32 i = 0; i < mesh->mNumVertices; ++i)
	{
		if (mesh->HasPositions())
		{
			position = vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
		}
		if (mesh->HasNormals())
		{
			normal = vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
		}
		if (mesh->HasTangentsAndBitangents())
		{
			tangent = vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
		}
		if (mesh->HasTextureCoords(0))
		{
			uv = vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		}

		setPosition(vertices[i], position);
		setNormal(vertices[i], normal);
		setTangent(vertices[i], tangent);
		setUV(vertices[i], uv);
	}


	for (uint32 i = 0; i < mesh->mNumFaces; ++i)
	{
		const aiFace& face = mesh->mFaces[i];
		triangles[i].a = face.mIndices[0];
		triangles[i].b = face.mIndices[1];
		triangles[i].c = face.mIndices[2];
	}

	aiColor3D color;
	material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
	this->material.r = color.r;
	this->material.g = color.g;
	this->material.b = color.b;

	aiString diffusePath;
	material->GetTexture(aiTextureType_DIFFUSE, 0, &diffusePath);
	
	std::filesystem::path path = parentPath / diffusePath.C_Str();
	this->material.albedo = path.wstring();
}

template<typename vertex_t>
inline cpu_mesh<vertex_t> cpu_mesh<vertex_t>::quad(float radius)
{
	vertex_3PUN vertices[] = {
		{ { -radius, -radius, 0.f }, { 0.f, 0.f }, { 0.f, 0.f, 1.f } },
		{ { radius, -radius, 0.f }, { 1.f, 0.f }, { 0.f, 0.f, 1.f } },
		{ { -radius, radius, 0.f }, { 0.f, 1.f }, { 0.f, 0.f, 1.f } },
		{ { radius, radius, 0.f }, { 1.f, 1.f }, { 0.f, 0.f, 1.f } },
	};

	indexed_triangle32 triangles[] = {
		{ 0, 1, 2 },
		{ 1, 3, 2 }
	};

	cpu_mesh<vertex_t> result;
	result.vertices.resize(arraysize(vertices));
	result.triangles.resize(arraysize(triangles));

	for (uint32 i = 0; i < arraysize(vertices); ++i)
	{
		result.setPosition(result.vertices[i], vertices[i].position);
		result.setUV(result.vertices[i], vertices[i].uv);
		result.setNormal(result.vertices[i], vertices[i].normal);
	}

	memcpy(result.triangles.data(), triangles, sizeof(indexed_triangle32) * arraysize(triangles));

	return result;
}

template<typename vertex_t>
inline cpu_mesh<vertex_t> cpu_mesh<vertex_t>::cube(float radius, bool invertWindingOrder)
{
	if constexpr (hasMember(vertex_t, position) && !hasMember(vertex_t, uv) && !hasMember(vertex_t, normal))
	{
		vertex_3P vertices[8];
		indexed_triangle32 triangles[12];

		vertices[0] = { vec3(-radius, -radius, radius) };  // 0
		vertices[1] = { vec3(radius, -radius, radius) };   // x
		vertices[2] = { vec3(-radius, radius, radius) };   // y
		vertices[3] = { vec3(radius, radius, radius) };	   // xy
		vertices[4] = { vec3(-radius, -radius, -radius) }; // z
		vertices[5] = { vec3(radius, -radius, -radius) };  // xz
		vertices[6] = { vec3(-radius, radius, -radius) };  // yz
		vertices[7] = { vec3(radius, radius, -radius) };   // xyz

		triangles[0] = { 0, 1, 2 };
		triangles[1] = { 1, 3, 2 };

		triangles[2] = { 1, 5, 3 };
		triangles[3] = { 5, 7, 3 };

		triangles[4] = { 5, 4, 7 };
		triangles[5] = { 4, 6, 7 };

		triangles[6] = { 4, 0, 6 };
		triangles[7] = { 0, 2, 6 };

		triangles[8] = { 2, 3, 6 };
		triangles[9] = { 3, 7, 6 };

		triangles[10] = { 4, 5, 0 };
		triangles[11] = { 5, 1, 0 };

		if (invertWindingOrder)
		{
			for (uint32 i = 0; i < arraysize(triangles); ++i)
			{
				uint32 tmp = triangles[i].b;
				triangles[i].b = triangles[i].c;
				triangles[i].c = tmp;
			}
		}

		cpu_mesh<vertex_t> result;
		result.vertices.resize(arraysize(vertices));
		result.triangles.resize(arraysize(triangles));

		for (uint32 i = 0; i < arraysize(vertices); ++i)
		{
			result.vertices[i].position = vertices[i].position;
		}

		memcpy(result.triangles.data(), triangles, sizeof(indexed_triangle32) * arraysize(triangles));

		return result;
	}
	else
	{
		vertex_3PUN vertices[24];
		indexed_triangle32 triangles[12];

		vertices[0] = { vec3(-radius, -radius, radius), vec2(0.f, 0.f), vec3(0.f, 0.f, 1.f) };
		vertices[1] = { vec3(radius, -radius, radius), vec2(1.f, 0.f), vec3(0.f, 0.f, 1.f) };
		vertices[2] = { vec3(-radius, radius, radius), vec2(0.f, 1.f), vec3(0.f, 0.f, 1.f) };
		vertices[3] = { vec3(radius, radius, radius), vec2(1.f, 1.f), vec3(0.f, 0.f, 1.f) };
		triangles[0] = { 0, 1, 2 };
		triangles[1] = { 1, 3, 2 };

		vertices[4] = { vec3(radius, -radius, radius), vec2(0.f, 0.f), vec3(1.f, 0.f, 0.f) };
		vertices[5] = { vec3(radius, -radius, -radius), vec2(1.f, 0.f), vec3(1.f, 0.f, 0.f) };
		vertices[6] = { vec3(radius, radius, radius), vec2(0.f, 1.f), vec3(1.f, 0.f, 0.f) };
		vertices[7] = { vec3(radius, radius, -radius), vec2(1.f, 1.f), vec3(1.f, 0.f, 0.f) };
		triangles[2] = { 4, 5, 6 };
		triangles[3] = { 5, 7, 6 };

		vertices[8] = { vec3(radius, -radius, -radius), vec2(0.f, 0.f), vec3(0.f, 0.f, -1.f) };
		vertices[9] = { vec3(-radius, -radius, -radius), vec2(1.f, 0.f), vec3(0.f, 0.f, -1.f) };
		vertices[10] = { vec3(radius, radius, -radius), vec2(0.f, 1.f), vec3(0.f, 0.f, -1.f) };
		vertices[11] = { vec3(-radius, radius, -radius), vec2(1.f, 1.f), vec3(0.f, 0.f, -1.f) };
		triangles[4] = { 8, 9, 10 };
		triangles[5] = { 9, 11, 10 };

		vertices[12] = { vec3(-radius, -radius, -radius), vec2(0.f, 0.f), vec3(-1.f, 0.f, 0.f) };
		vertices[13] = { vec3(-radius, -radius, radius), vec2(1.f, 0.f), vec3(-1.f, 0.f, 0.f) };
		vertices[14] = { vec3(-radius, radius, -radius), vec2(0.f, 1.f), vec3(-1.f, 0.f, 0.f) };
		vertices[15] = { vec3(-radius, radius, radius), vec2(1.f, 1.f), vec3(-1.f, 0.f, 0.f) };
		triangles[6] = { 12, 13, 14 };
		triangles[7] = { 13, 15, 14 };

		vertices[16] = { vec3(-radius, radius, radius), vec2(0.f, 0.f), vec3(0.f, 1.f, 0.f) };
		vertices[17] = { vec3(radius, radius, radius), vec2(1.f, 0.f), vec3(0.f, 1.f, 0.f) };
		vertices[18] = { vec3(-radius, radius, -radius), vec2(0.f, 1.f), vec3(0.f, 1.f, 0.f) };
		vertices[19] = { vec3(radius, radius, -radius), vec2(1.f, 1.f), vec3(0.f, 1.f, 0.f) };
		triangles[8] = { 16, 17, 18 };
		triangles[9] = { 17, 19, 18 };

		vertices[20] = { vec3(-radius, -radius, -radius), vec2(0.f, 0.f), vec3(0.f, -1.f, 0.f) };
		vertices[21] = { vec3(radius, -radius, -radius), vec2(1.f, 0.f), vec3(0.f, -1.f, 0.f) };
		vertices[22] = { vec3(-radius, -radius, radius), vec2(0.f, 1.f), vec3(0.f, -1.f, 0.f) };
		vertices[23] = { vec3(radius, -radius, radius), vec2(1.f, 1.f), vec3(0.f, -1.f, 0.f) };
		triangles[10] = { 20, 21, 22 };
		triangles[11] = { 21, 23, 22 };

		if (invertWindingOrder)
		{
			for (uint32 i = 0; i < arraysize(triangles); ++i)
			{
				uint32 tmp = triangles[i].b;
				triangles[i].b = triangles[i].c;
				triangles[i].c = tmp;
			}
		}

		cpu_mesh<vertex_t> result;
		result.vertices.resize(arraysize(vertices));
		result.triangles.resize(arraysize(triangles));

		for (uint32 i = 0; i < arraysize(vertices); ++i)
		{
			result.setPosition(result.vertices[i], vertices[i].position);
			result.setUV(result.vertices[i], vertices[i].uv);
			result.setNormal(result.vertices[i], vertices[i].normal);
		}

		memcpy(result.triangles.data(), triangles, sizeof(indexed_triangle32) * arraysize(triangles));

		return result;
	}
}

template<typename vertex_t>
inline cpu_mesh<vertex_t> cpu_mesh<vertex_t>::sphere(uint32 slices, uint32 rows, float radius)
{
	assert(slices > 2);
	assert(rows > 0);

	float vertDeltaAngle = DirectX::XM_PI / (rows + 1);
	float horzDeltaAngle = 2.f * DirectX::XM_PI / slices;

	vertex_3PUN* vertices = new vertex_3PUN[slices * rows + 2];
	indexed_triangle32* triangles = new indexed_triangle32[2 * rows * slices];

	uint32 vertIndex = 0;

	// Vertices.
	vertices[vertIndex].position = vec3(0.f, -radius, 0.f);
	vertices[vertIndex].normal = vec3(0.f, -1.f, 0.f);
	vertices[vertIndex].uv = vec2(0.5f, 0.f);
	++vertIndex;

	for (uint32 y = 0; y < rows; ++y)
	{
		float vertAngle = (y + 1) * vertDeltaAngle - DirectX::XM_PI;
		float vertexY = cosf(vertAngle);
		float currentCircleRadius = sinf(vertAngle);
		for (uint32 x = 0; x < slices; ++x)
		{
			float horzAngle = x * horzDeltaAngle;
			float vertexX = cosf(horzAngle) * currentCircleRadius;
			float vertexZ = sinf(horzAngle) * currentCircleRadius;
			vec3 pos(vertexX * radius, vertexY * radius, vertexZ * radius);
			vec3 nor(vertexX, vertexY, vertexZ);
			vertices[vertIndex].position = pos;
			vertices[vertIndex].normal = nor;

			float u = asinf(vertexX) / DirectX::XM_PI + 0.5f;
			float v = asinf(vertexY) / DirectX::XM_PI + 0.5f;
			vertices[vertIndex].uv = vec2(u, v);
			++vertIndex;
		}
	}
	vertices[vertIndex].position = vec3(0.f, radius, 0.f);
	vertices[vertIndex].normal = vec3(0.f, 1.f, 0.f);
	vertices[vertIndex].uv = vec2(0.5f, 1.f);
	++vertIndex;

	assert(vertIndex == slices * rows + 2);

	uint32 triIndex = 0;

	// Indices.
	for (uint32 x = 0; x < slices - 1; ++x)
	{
		triangles[triIndex++] = indexed_triangle32{ 0, x + 1, x + 2 };
	}
	triangles[triIndex++] = indexed_triangle32{ 0, slices, 1 };
	for (uint32 y = 0; y < rows - 1; ++y)
	{
		for (uint32 x = 0; x < slices - 1; ++x)
		{
			triangles[triIndex++] = indexed_triangle32{ y * slices + 1 + x, (y + 1) * slices + 2 + x, y * slices + 2 + x };
			triangles[triIndex++] = indexed_triangle32{ y * slices + 1 + x, (y + 1) * slices + 1 + x, (y + 1) * slices + 2 + x };
		}
		triangles[triIndex++] = indexed_triangle32{ y * slices + slices, (y + 1) * slices + 1, y * slices + 1 };
		triangles[triIndex++] = indexed_triangle32{ y * slices + slices, (y + 1) * slices + slices, (y + 1) * slices + 1 };
	}
	for (uint32 x = 0; x < slices - 1; ++x)
	{
		triangles[triIndex++] = indexed_triangle32{ vertIndex - 2 - x, vertIndex - 3 - x, vertIndex - 1 };
	}
	triangles[triIndex++] = indexed_triangle32{ vertIndex - 1 - slices, vertIndex - 2, vertIndex - 1 };

	assert(triIndex == 2 * rows * slices);


	cpu_mesh<vertex_t> result;
	result.vertices.resize(vertIndex);
	result.triangles.resize(triIndex);

	for (uint32 i = 0; i < vertIndex; ++i)
	{
		result.setPosition(result.vertices[i], vertices[i].position);
		result.setUV(result.vertices[i], vertices[i].uv);
		result.setNormal(result.vertices[i], vertices[i].normal);
	}

	memcpy(result.triangles.data(), triangles, sizeof(indexed_triangle32) * triIndex);

	delete[] vertices;
	delete[] triangles;

	return result;
}

template<typename vertex_t>
inline cpu_mesh<vertex_t> cpu_mesh<vertex_t>::capsule(uint32 slices, uint32 rows, float height, float radius)
{
	assert(slices > 2);
	assert(rows > 0);
	assert(rows % 2 == 1);

	float vertDeltaAngle = DirectX::XM_PI / (rows + 1);
	float horzDeltaAngle = 2.f * DirectX::XM_PI / slices;
	float halfHeight = 0.5f * height;
	float texStretch = radius / (radius + halfHeight);

	vertex_3PUN* vertices = new vertex_3PUN[slices * (rows + 1) + 2];
	indexed_triangle32* triangles = new indexed_triangle32[2 * (rows + 1) * slices];

	uint32 vertIndex = 0;

	// Vertices.
	vertices[vertIndex].position = vec3(0.f, -radius - halfHeight, 0.f);
	vertices[vertIndex].normal = vec3(0.f, -1.f, 0.f);
	vertices[vertIndex].uv = vec2(0.5f, 0.f);
	++vertIndex;
	for (uint32 y = 0; y < rows / 2 + 1; ++y)
	{
		float vertAngle = (y + 1) * vertDeltaAngle - DirectX::XM_PI;
		float vertexY = cosf(vertAngle);
		float currentCircleRadius = sinf(vertAngle);
		for (uint32 x = 0; x < slices; ++x)
		{
			float horzAngle = x * horzDeltaAngle;
			float vertexX = cosf(horzAngle) * currentCircleRadius;
			float vertexZ = sinf(horzAngle) * currentCircleRadius;
			vec3 pos(vertexX * radius, vertexY * radius - halfHeight, vertexZ * radius);
			vec3 nor(vertexX, vertexY, vertexZ);
			vertices[vertIndex].position = pos;
			vertices[vertIndex].normal = nor;

			float u = asinf(vertexX) / DirectX::XM_PI + 0.5f;
			float v = asinf(vertexY) / DirectX::XM_PI + 0.5f;
			vertices[vertIndex].uv = vec2(u, v * texStretch);
			++vertIndex;
		}
	}
	for (uint32 y = 0; y < rows / 2 + 1; ++y)
	{
		float vertAngle = (y + rows / 2 + 1) * vertDeltaAngle - DirectX::XM_PI;
		float vertexY = cosf(vertAngle);
		float currentCircleRadius = sinf(vertAngle);
		for (uint32 x = 0; x < slices; ++x)
		{
			float horzAngle = x * horzDeltaAngle;
			float vertexX = cosf(horzAngle) * currentCircleRadius;
			float vertexZ = sinf(horzAngle) * currentCircleRadius;
			vec3 pos(vertexX * radius, vertexY * radius + halfHeight, vertexZ * radius);
			vec3 nor(vertexX, vertexY, vertexZ);
			vertices[vertIndex].position = pos;
			vertices[vertIndex].normal = nor;

			float u = asinf(vertexX) / DirectX::XM_PI + 0.5f;
			float v = asinf(vertexY) / DirectX::XM_PI + 0.5f;
			vertices[vertIndex].uv = vec2(u, v * texStretch + 1.f - texStretch);
			++vertIndex;
		}
	}
	vertices[vertIndex].position = vec3(0.f, radius + halfHeight, 0.f);
	vertices[vertIndex].normal = vec3(0.f, 1.f, 0.f);
	vertices[vertIndex].uv = vec2(0.5f, 1.f);
	++vertIndex;

	uint32 triIndex = 0;

	// Indices.
	for (uint32 x = 0; x < slices - 1; ++x)
	{
		triangles[triIndex++] = indexed_triangle32{ 0, x + 1, x + 2 };
	}
	triangles[triIndex++] = indexed_triangle32{ 0, slices, 1 };
	for (uint32 y = 0; y < rows; ++y)
	{
		for (uint32 x = 0; x < slices - 1; ++x)
		{
			triangles[triIndex++] = indexed_triangle32{ y * slices + 1 + x, (y + 1) * slices + 2 + x, y * slices + 2 + x };
			triangles[triIndex++] = indexed_triangle32{ y * slices + 1 + x, (y + 1) * slices + 1 + x, (y + 1) * slices + 2 + x };
		}
		triangles[triIndex++] = indexed_triangle32{ y * slices + slices, (y + 1) * slices + 1, y * slices + 1 };
		triangles[triIndex++] = indexed_triangle32{ y * slices + slices, (y + 1) * slices + slices, (y + 1) * slices + 1 };
	}
	for (uint32 x = 0; x < slices - 1; ++x)
	{
		triangles[triIndex++] = indexed_triangle32{ vertIndex - 2 - x, vertIndex - 3 - x, vertIndex - 1 };
	}
	triangles[triIndex++] = indexed_triangle32{ vertIndex - 1 - slices, vertIndex - 2, vertIndex - 1 };


	cpu_mesh<vertex_t> result;
	result.vertices.resize(vertIndex);
	result.triangles.resize(triIndex);

	for (uint32 i = 0; i < vertIndex; ++i)
	{
		result.setPosition(result.vertices[i], vertices[i].position);
		result.setUV(result.vertices[i], vertices[i].uv);
		result.setNormal(result.vertices[i], vertices[i].normal);
	}

	memcpy(result.triangles.data(), triangles, sizeof(indexed_triangle32) * triIndex);

	delete[] vertices;
	delete[] triangles;

	return result;
}
