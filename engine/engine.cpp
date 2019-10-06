#include "stdafx.h"
#include "engine.h"
#include "../renderer/render_thread.h"

#include <iostream>
#include <codecvt>
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

SceneEngine::SceneEngine() : BaseApp() {}
SceneEngine::~SceneEngine() {}

uint32_t SceneEngine::LoadMeshesFromFile(char* path)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	
	auto wstr_to_str = [](const std::wstring& wstr) -> std::string
	{
		std::string str;
		str.reserve(wstr.size());
		for (wchar_t x : wstr) str += static_cast<char>(x);
		return str;
	};

	{
		std::string warn;
		std::string err;
		
		const std::string model_path = wstr_to_str(assets_path_) + path;
		const bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str());
		if (!warn.empty()) { std::cout << warn << std::endl; }
		if (!err.empty()) { std::cerr << err << std::endl; }
		if (!ret) { std::cerr << "Error: cannot load '" << path << "'\n"; return 0; }
		//if (1 != shapes.size()) { std::cerr << "Error: unsupported shape num: " << shapes.size() << " in: " << path << "\n"; return 0; }
	}

	const auto vertices_num = attrib.vertices.size() / 3;
	assert(0 == (attrib.vertices.size() % 3));
	assert(0 == (attrib.normals.size() % 3));
	assert(0 == (attrib.texcoords.size() % 2));
	assert(vertices_num == (attrib.normals.size() / 3)		|| !attrib.normals.size());
	assert(vertices_num == (attrib.texcoords.size() / 2)	|| !attrib.texcoords.size());

	Mesh mesh;
	mesh.name = path;
	mesh.data = std::make_shared<RawMeshData>();

	//INDICES
	for (size_t s = 0; s < shapes.size(); s++) 
	{
		size_t index_offset = 0;
		for (size_t face_idx = 0; face_idx < shapes[s].mesh.num_face_vertices.size(); face_idx++)
		{
			const int vertices_in_face = shapes[s].mesh.num_face_vertices[face_idx];
			for (size_t vertex_idx = 0; vertex_idx < vertices_in_face; vertex_idx++)
			{
				const tinyobj::index_t idx	= shapes[s].mesh.indices[index_offset + vertex_idx];
				assert(idx.normal_index		== idx.vertex_index || idx.normal_index		== -1);
				assert(idx.texcoord_index	== idx.vertex_index || idx.texcoord_index	== -1);
				mesh.data->indices.push_back(idx.vertex_index);
			}
			index_offset += vertices_in_face;
		}
	}

	//VERTEXES
	mesh.data->vertexes.reserve(vertices_num);
	for (uint32_t idx = 0; idx < vertices_num; idx++)
	{
		RawVertexData vertex;
		vertex.position = XMFLOAT3(&attrib.vertices	[3 * idx]);
		if (attrib.normals.size())	{ vertex.normal		= XMFLOAT3(&attrib.normals	[3 * idx]); }
		if (attrib.texcoords.size()){ vertex.texcoord	= XMFLOAT2(&attrib.texcoords[2 * idx]); }
		mesh.data->vertexes.push_back(vertex);
	}

	meshes_.push_back(std::move(mesh));
	return 1;
}

void SceneEngine::OnInit(HWND hWnd, UINT width, UINT height)
{ 
	LoadMeshesFromFile("models/cornell_box.obj");

	renderer_.Init(hWnd, width, height, assets_path_);
	renderer_.Start();
}

void SceneEngine::OnDestroy() 
{ 
	renderer_.Stop();
	renderer_.Destroy();
}
