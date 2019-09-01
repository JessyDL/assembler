#include "stdafx.h"
#include "generators/models.h"
#include "cli/value.h"
#include "psl/math/math.hpp"
#include "psl/serialization.h"
#include "psl/terminal_utils.h"
#include "psl/meta.h"
#include "psl/library.h"
#include <iostream>
#ifdef DBG_NEW
#undef new
#endif

#include "assimp/Importer.hpp"
#include "assimp\scene.h"
#include "assimp\postprocess.h"
#include "assimp\cimport.h"
#ifdef DBG_NEW
#define new DBG_NEW
#endif

#include "data/geometry.h"
using namespace assembler::generators;
using namespace core::data;
using namespace psl::math;

template <typename T>
using cli_value = psl::cli::value<T>;
using namespace psl;

models::models()
{
	m_Pack = psl::cli::pack{std::bind(&assembler::generators::models::on_invoke, this, std::placeholders::_1),
							cli_value<psl::string>{"input", "location of the input file", {"input", "i"}, "", false},
							cli_value<psl::string>{"output", "location of the output file", {"output", "o"}, "", true},
							cli_value<bool>{"tangents", "generate tangent information", {"tangents", "t"}, true},
							cli_value<bool>{"normals", "generate normal information", {"normals", "n"}, true},
							cli_value<bool>{"snormals", "generate smooth normal information", {"snormals", "s"}, true},
							cli_value<bool>{"uvs", "generate uv information", {"uvs"}, true},
							cli_value<bool>{"optimize", "optimize the mesh", {"optimize", "O"}, true},
							cli_value<bool>{"LH", "left handed coordinate system", {"LH"}, true},
							cli_value<bool>{"fuvs", "flip uv coorinates", {"fuvs"}, false},
							cli_value<bool>{"fwinding", "flip triangle winding", {"fwinding"}, false},
							cli_value<bool>{"flatten", "", {"flatten", "f"}, false},
							cli_value<psl::string>{"axis", "what should be left, up, and forward?", {"axis"}, "xzy"},
							cli_value<bool>{"sparse_skeleton", "compress the skeleton information", {"sparse"}, true},
							cli_value<bool>{"binary", "outputs the file in binary form", {"bin", "b"}, false}};
}

bool proccess_flags(cli::pack& pack, unsigned int& flags)
{
	flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType;

	if(pack["tangents"]->as<bool>().get()) flags |= aiProcess_CalcTangentSpace;
	if(pack["normals"]->as<bool>().get()) flags |= aiProcess_GenNormals;
	if(pack["snormals"]->as<bool>().get()) flags |= aiProcess_GenSmoothNormals;
	if(pack["uvs"]->as<bool>().get()) flags |= aiProcess_GenUVCoords;
	if(pack["optimize"]->as<bool>().get()) flags |= aiProcess_OptimizeMeshes;
	if(pack["LH"]->as<bool>().get()) flags |= aiProcess_MakeLeftHanded;
	if(pack["fuvs"]->as<bool>().get()) flags |= aiProcess_FlipUVs;
	if(pack["fwinding"]->as<bool>().get()) flags |= aiProcess_FlipWindingOrder;
	if(pack["flatten"]->as<bool>().get()) flags |= aiProcess_JoinIdenticalVertices;

	return true;
}
void models::on_invoke(cli::pack& pack)
{
	// --generate --geometry -i "C:\Projects\data_old\Models\Cerberus.FBX"
	// --generate --geometry -i "C:\Projects\data_old\Models\Translate.FBX"
	std::array<uint8_t, 3> axis_setup;
	{
		auto axis = pack["axis"]->as<psl::string>().get();
		if(axis.size() != 3)
		{
			utility::terminal::set_color(utility::terminal::color::RED);
			std::cerr << psl::to_string8_t(
				"you provided an invalid axis, you shjould enter three letters, and the only accepted values are 'x', "
				"'y', and 'z': " +
				axis + "\n");
			utility::terminal::set_color(utility::terminal::color::WHITE);
			return;
		}

		bool xSet = false;
		bool ySet = false;
		bool zSet = false;
		for(uint8_t i = 0; i < 3; ++i)
		{
			if(axis[i] == ('x') && !xSet)
			{
				xSet		  = true;
				axis_setup[i] = 0;
			}
			else if(axis[i] == ('y') && !ySet)
			{
				ySet		  = true;
				axis_setup[i] = 1;
			}
			else if(axis[i] == ('z') && !zSet)
			{
				zSet		  = true;
				axis_setup[i] = 2;
			}
			else
			{
				utility::terminal::set_color(utility::terminal::color::RED);
				std::cerr << psl::to_string8_t(
					"you provided an invalid axis, the accepted values are 'x', 'y', and 'z', and no duplicates you "
					"entered: " +
					axis + "\n");
				utility::terminal::set_color(utility::terminal::color::WHITE);
				return;
			}
		}
	}

	auto input_file		  = pack["input"]->as<psl::string>().get();
	auto output_file	  = pack["output"]->as<psl::string>().get();
	bool encode_to_binary = pack["flatten"]->as<bool>().get();
	Assimp::Importer importer;


	unsigned int flags = 0u;
	if(!proccess_flags(pack, flags)) return;

	if(!utility::platform::file::exists(input_file))
	{
		utility::terminal::set_color(utility::terminal::color::RED);
		std::cerr << psl::to_string8_t("the file '" + input_file + "' does not exist\n");
		utility::terminal::set_color(utility::terminal::color::WHITE);
		return;
	}

	if(input_file == output_file)
	{
		output_file = output_file.substr(0, output_file.find_last_of(('.')));
		output_file += (".pgf");
	}
	input_file  = utility::platform::directory::to_platform(input_file);
	output_file = utility::platform::directory::to_unix(output_file);

	const aiScene* pScene = importer.ReadFile(psl::to_string8_t(input_file), flags);
	psl::string errorMessage;
	if(!pScene)
	{
		utility::terminal::set_color(utility::terminal::color::RED);
		std::cerr << psl::to_string8_t("the scene of the file '" + input_file + "' could not be loaded by assimp\n");
		utility::terminal::set_color(utility::terminal::color::WHITE);
		return;
	}

	const int iVertexTotalSize = sizeof(aiVector3D) * 2 + sizeof(aiVector2D);
	int iTotalVertices		   = 0;

	geometry result;
	typename std::decay<decltype(result.indices())>::type indices;
	format::container cont{};
	format::settings settings{};
	if(encode_to_binary)
	{
		settings.compact_string = true;
		settings.binary_value   = true;
	}
	cont.set_settings(settings);
	serialization::serializer s;

	// todo handle more
	if(pScene->mNumMeshes > 1)
	{
		errorMessage = ("the scene is too complex, currently the importer only supports one model per file.\n");
		goto error;
	}

	for(unsigned int m = 0; m < pScene->mNumMeshes; ++m)
	{
		aiMesh* pAIMesh = pScene->mMeshes[m];
		unsigned int nVertices{pAIMesh->mNumVertices};

		if(pAIMesh->HasFaces())
		{
			aiFace* pAIFaces;
			pAIFaces			  = pAIMesh->mFaces;
			unsigned int nIndices = pAIMesh->mNumFaces * 3;
			indices.resize(nIndices);

			for(unsigned int iface = 0; iface < pAIMesh->mNumFaces; iface++)
			{
				if(pAIFaces[iface].mNumIndices != 3)
				{
					errorMessage =
						("the model has an incorrect number vertices per face. only 3 vertices per face allowed.\n");
					goto error;
				}

				for(DWORD j = 0; j < 3; j++)
				{
					indices[iface * 3 + j] = pAIFaces[iface].mIndices[j];
				}
			}

			result.indices(indices);
		}
		core::stream vec4_stream{core::stream::type::vec4};
		core::stream vec3_stream{core::stream::type::vec3};
		core::stream vec2_stream{core::stream::type::vec2};
		std::vector<psl::vec4>& datav4 = vec4_stream.as_vec4().value();
		std::vector<psl::vec3>& datav3 = vec3_stream.as_vec3().value();
		std::vector<psl::vec2>& datav2 = vec2_stream.as_vec2().value();
		datav4.resize(nVertices);
		datav3.resize(nVertices);
		datav2.resize(nVertices);

		if(pAIMesh->HasPositions())
		{
			for(unsigned int ivert = 0; ivert < nVertices; ivert++)
			{
				datav3[ivert] =
					psl::vec3(pAIMesh->mVertices[ivert][axis_setup[0]], pAIMesh->mVertices[ivert][axis_setup[1]],
							  pAIMesh->mVertices[ivert][axis_setup[2]]);
			}

			result.vertices(geometry::constants::POSITION, vec3_stream);
		}
		else
		{
			errorMessage = ("the model has no position data.\n");
			goto error;
		}

		if(pAIMesh->HasNormals())
		{
			for(unsigned int i = 0; i < nVertices; i++)
			{
				datav3[i] =
					normalize(psl::vec3(pAIMesh->mNormals[i][axis_setup[0]], pAIMesh->mNormals[i][axis_setup[1]],
											 pAIMesh->mNormals[i][axis_setup[2]]));
			}

			result.vertices(geometry::constants::NORMAL, vec3_stream);
		}

		if(pAIMesh->HasTangentsAndBitangents())
		{
			{
				for(unsigned int i = 0; i < nVertices; i++)
				{
					datav3[i] = normalize(psl::vec3(pAIMesh->mTangents[i][axis_setup[0]],
														 pAIMesh->mTangents[i][axis_setup[1]],
														 pAIMesh->mTangents[i][axis_setup[2]]));
				}

				result.vertices(geometry::constants::TANGENT, vec3_stream);
			}
			{
				for(unsigned int i = 0; i < nVertices; i++)
				{
					datav3[i] = normalize(psl::vec3(pAIMesh->mBitangents[i][axis_setup[0]],
														 pAIMesh->mBitangents[i][axis_setup[1]],
														 pAIMesh->mBitangents[i][axis_setup[2]]));
				}

				result.vertices(geometry::constants::BITANGENT, vec3_stream);
			}
		}

		for(uint32_t uvChannel = 0; uvChannel < pAIMesh->GetNumUVChannels(); ++uvChannel)
		{
			if(pAIMesh->HasTextureCoords(uvChannel))
			{
				for(unsigned int i = 0; i < nVertices; i++)
				{
					datav2[i] =
						psl::vec2(pAIMesh->mTextureCoords[uvChannel][i].x, pAIMesh->mTextureCoords[uvChannel][i].y);
				}

				if(uvChannel > 1)
					result.vertices(psl::string(geometry::constants::TEX) +
										psl::from_string8_t(utility::to_string((uvChannel))),
									vec2_stream);
				else
					result.vertices(geometry::constants::TEX, vec2_stream);
			}
		}

		for(unsigned int c = 0; c < pAIMesh->GetNumColorChannels(); ++c)
		{
			if(pAIMesh->HasVertexColors(c))
			{
				for(unsigned int i = 0; i < nVertices; i++)
				{
					datav4[i] = psl::vec4(pAIMesh->mColors[c][i].r, pAIMesh->mColors[c][i].g, pAIMesh->mColors[c][i].b,
										  pAIMesh->mColors[c][i].a);
				}

				if(c > 1)
					result.vertices(psl::string(geometry::constants::COLOR) +
										psl::from_string8_t(utility::to_string((c))),
									vec4_stream);
				else
					result.vertices(geometry::constants::COLOR, vec4_stream);
			}
		}

		if(pAIMesh->HasBones())
		{
			// todo: write bone support
		}
	}

	s.serialize<serialization::encode_to_format>(result, cont);

	if(!utility::platform::file::write(output_file, cont.to_string()))
	{
		errorMessage = ("could not write the output file.\n");
		goto error;
	}

	{
		UID uid = UID::generate();
		if(utility::platform::file::exists(output_file + (".") + psl::from_string8_t(meta::META_EXTENSION)))
		{
			::meta::file* original = nullptr;
			serialization::serializer temp_s;
			temp_s.deserialize<serialization::decode_from_format>(
				original, output_file + (".") + psl::from_string8_t(meta::META_EXTENSION));
			uid = original->ID();
		}

		meta::file metaFile{uid};
		cont = format::container{};

		s.serialize<serialization::encode_to_format>(metaFile, cont);
		if(!utility::platform::file::write(output_file + (".") + psl::from_string8_t(meta::META_EXTENSION),
										   cont.to_string()))
		{
			errorMessage = ("could not write the output file.\n");
			goto error;
		}
	}

	importer.FreeScene();
	return;
error:
	importer.FreeScene();
	utility::terminal::set_color(utility::terminal::color::RED);
	std::cerr << psl::to_string8_t(errorMessage);
	utility::terminal::set_color(utility::terminal::color::WHITE);
	return;
}
