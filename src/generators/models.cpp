#include "stdafx.h"
#include "generators/models.h"
#include "cli_pack.h"
#include "bash_terminal.h"
#include "bash_utils.h"
#include "terminal_utils.h"
#include <glm/glm.hpp>
#include "serialization.h"
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
models::models()
{
	m_Pack << std::move(cli::value<psl::string>{}
	.name(("input file"))
		.command(("input"))
		.short_command(('i'))
		.hint(("target file to process"))
		.required(true))
		<< std::move(cli::value<psl::string>{}
	.name(("output file"))
		.command(("output"))
		.short_command(('o'))
		.hint(("output file to process, will put it next to the input if not set"))
		.bind_default_value(m_Pack.get_as_a<psl::string>(("input file"))->get())) 
		<< std::move(cli::value<bool>{}
	.name(("generate tangents"))
		.command(("tangents"))
		.short_command(('t'))
		.hint(("generate tangent information")))
		<< std::move(cli::value<bool>{}
	.name(("generate normals"))
		.command(("normals"))
		.short_command(('n'))
		.hint(("generate normal information"))) 
		<< std::move(cli::value<bool>{}
	.name(("generate smooth normals"))
		.command(("snormals"))
		.short_command(('s'))
		.hint(("generate smooth normals information, this overrides the 'generate normals' flag, so no need to set both"))) 
		<< std::move(cli::value<bool>{}
	.name(("generate uvs"))
		.command(("uvs"))
		.hint(("generate UV information"))) 
		<< std::move(cli::value<bool>{}
	.name(("optimize"))
		.command(("optimize"))
		.hint(("optimize the mesh data, applies compression etc..")))
		<< std::move(cli::value<bool>{}
	.name(("LH coordinates"))
		.command(("LH"))
		.hint(("use a left handed coordinate system"))) 
		<< std::move(cli::value<bool>{}
	.name(("flip uv coordinates"))
		.command(("fuvs"))
		.hint(("flips the uv coordinates"))) 
		<< std::move(cli::value<bool>{}
	.name(("flip winding"))
		.command(("fwinding"))
		.hint(("flip the model winding")))
		<< std::move(cli::value<bool>{}
	.name(("flatten"))
		.command(("flatten"))
		.short_command(('f'))
		.hint(("generate tangent information"))) 
		<< std::move(cli::value<psl::string>{}
	.name(("axis"))
		.command(("axis"))
		.default_value(("xzy"))
		.hint(("generate tangent information"))) 
		<< std::move(cli::value<bool>{}
	.name(("sparse skeleton"))
		.command(("sparse"))
		.hint(("compress the skeleton information")))
		<< std::move(cli::value<bool>{}
	.name(("binary"))
		.command(("bin"))
		.short_command(('b'))
		.hint(("outputs the file in the binary form")));

	m_Pack.callback(std::bind(&assembler::generators::models::on_invoke, this, m_Pack));

}

bool proccess_flags(cli::parameter_pack& pack, unsigned int& flags)
{
	flags = aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_SortByPType;

	if(pack.get_as_a<bool>(("generate tangents")))
		flags |= aiProcess_CalcTangentSpace;
	if(pack.get_as_a<bool>(("generate normals")))
		flags |= aiProcess_GenNormals;
	if(pack.get_as_a<bool>(("generate smooth normals")))
		flags |= aiProcess_GenSmoothNormals;
	if(pack.get_as_a<bool>(("generate uvs")))
		flags |= aiProcess_GenUVCoords;
	if(pack.get_as_a<bool>(("optimize")))
		flags |= aiProcess_OptimizeMeshes;
	if(pack.get_as_a<bool>(("LH coordinates")))
		flags |= aiProcess_MakeLeftHanded;
	if(pack.get_as_a<bool>(("flip uv coordinates")))
		flags |= aiProcess_FlipUVs;
	if(pack.get_as_a<bool>(("flip winding")))
		flags |= aiProcess_FlipWindingOrder;
	if(pack.get_as_a<bool>(("flatten")))
		flags |= aiProcess_JoinIdenticalVertices;

	return true;
}
void models::on_invoke(cli::parameter_pack& pack)
{
	// --generate --geometry -i "C:\Projects\data_old\Models\Cerberus.FBX"
	// --generate --geometry -i "C:\Projects\data_old\Models\Translate.FBX"
	std::array<uint8_t, 3> axis_setup;
	{
		auto axis = pack.get_as_a<psl::string>(("axis"))->get();
		if(axis.size() != 3)
		{
			utility::terminal::set_color(utility::terminal::color::RED);
			psl::cerr << psl::to_platform_string("you provided an invalid axis, you shjould enter three letters, and the only accepted values are 'x', 'y', and 'z': " + axis + "\n");
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
				xSet = true;
				axis_setup[i] = 0;
			}
			else if(axis[i] == ('y') && !ySet)
			{
				ySet = true;
				axis_setup[i] = 1;
			}
			else if(axis[i] == ('z') && !zSet)
			{
				zSet = true;
				axis_setup[i] = 2;
			}
			else
			{
				utility::terminal::set_color(utility::terminal::color::RED);
				psl::cerr << psl::to_platform_string("you provided an invalid axis, the accepted values are 'x', 'y', and 'z', and no duplicates you entered: " + axis + "\n");
				utility::terminal::set_color(utility::terminal::color::WHITE);
				return;
			}
		}		
	}

	auto input_file = pack.get_as_a<psl::string>(("input file"))->get();
	auto output_file = pack.get_as_a<psl::string>(("output file"))->get();
	bool encode_to_binary = pack.get_as_a<bool>("flatten")->get();
	Assimp::Importer importer;

	unsigned int flags = 0u;
	if(!proccess_flags(pack, flags))
		return;

	tools::bash_terminal bt;
	if(!utility::bash::file::exists(bt, input_file))
	{
		utility::terminal::set_color(utility::terminal::color::RED);
		psl::cerr << psl::to_platform_string("the file '" + input_file + "' does not exist\n");
		utility::terminal::set_color(utility::terminal::color::WHITE);
		return;
	}

	if(input_file == output_file)
	{
		output_file = output_file.substr(0, output_file.find_last_of(('.')));
		output_file += (".pgf");
	}
	input_file = utility::platform::directory::to_platform(input_file);
	output_file = utility::platform::directory::to_unix(output_file);

	const aiScene* pScene = importer.ReadFile(psl::to_string8_t(input_file), flags);
	psl::string errorMessage;
	if(!pScene)
	{
		utility::terminal::set_color(utility::terminal::color::RED);
		psl::cerr << psl::to_platform_string("the scene of the file '" + input_file + "' could not be loaded by assimp\n");
		utility::terminal::set_color(utility::terminal::color::WHITE);
		return;
	}

	const int iVertexTotalSize = sizeof(aiVector3D) * 2 + sizeof(aiVector2D);
	int iTotalVertices = 0;

	geometry result;
	typename std::decay<decltype(result.indices())>::type indices;
	format::container cont{}; 
	format::settings settings{};
	if(encode_to_binary)
	{
		settings.compact_string = true;
		settings.binary_value = true;
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
			aiFace * pAIFaces;
			pAIFaces = pAIMesh->mFaces;
			unsigned int nIndices = pAIMesh->mNumFaces * 3;
			indices.resize(nIndices);

			for(unsigned int iface = 0; iface < pAIMesh->mNumFaces; iface++)
			{
				if(pAIFaces[iface].mNumIndices != 3)
				{
					errorMessage = ("the model has an incorrect number vertices per face. only 3 vertices per face allowed.\n");
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
		std::vector<glm::vec4>& datav4 = vec4_stream.as_vec4().value();
		std::vector<glm::vec3>& datav3 = vec3_stream.as_vec3().value();
		std::vector<glm::vec2>& datav2 = vec2_stream.as_vec2().value();
		datav4.resize(nVertices);
		datav3.resize(nVertices);
		datav2.resize(nVertices);

		if(pAIMesh->HasPositions())
		{
			for(unsigned int ivert = 0; ivert < nVertices; ivert++)
			{
				datav3[ivert] = glm::vec3(pAIMesh->mVertices[ivert][axis_setup[0]], pAIMesh->mVertices[ivert][axis_setup[1]], pAIMesh->mVertices[ivert][axis_setup[2]]);
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
				datav3[i] = glm::normalize(glm::vec3(pAIMesh->mNormals[i][axis_setup[0]], pAIMesh->mNormals[i][axis_setup[1]], pAIMesh->mNormals[i][axis_setup[2]]));
			}

			result.vertices(geometry::constants::NORMAL, vec3_stream);
		}

		if(pAIMesh->HasTangentsAndBitangents())
		{
			{
				for(unsigned int i = 0; i < nVertices; i++)
				{
					datav3[i] = glm::normalize(glm::vec3(pAIMesh->mTangents[i][axis_setup[0]], pAIMesh->mTangents[i][axis_setup[1]], pAIMesh->mTangents[i][axis_setup[2]]));
				}

				result.vertices(geometry::constants::TANGENT, vec3_stream);
			}
			{
				for(unsigned int i = 0; i < nVertices; i++)
				{
					datav3[i] = glm::normalize(glm::vec3(pAIMesh->mBitangents[i][axis_setup[0]], pAIMesh->mBitangents[i][axis_setup[1]], pAIMesh->mBitangents[i][axis_setup[2]]));
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
					datav2[i] = glm::vec2(pAIMesh->mTextureCoords[uvChannel][i].x, pAIMesh->mTextureCoords[uvChannel][i].y);
				}

				if(uvChannel > 1)
					result.vertices(psl::string(geometry::constants::TEX) + psl::from_string8_t(utility::to_string((uvChannel))), vec2_stream);
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
					datav4[i] = glm::vec4(pAIMesh->mColors[c][i].r, pAIMesh->mColors[c][i].g, pAIMesh->mColors[c][i].b, pAIMesh->mColors[c][i].a);
				}

				if(c > 1)
					result.vertices(psl::string(geometry::constants::COLOR) + psl::from_string8_t(utility::to_string((c))), vec4_stream);
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
			temp_s.deserialize<serialization::decode_from_format>(original, output_file + (".") + psl::from_string8_t(meta::META_EXTENSION));
			uid = original->ID();
		}

		meta::file metaFile{uid};
		cont = format::container{};

		s.serialize<serialization::encode_to_format>(metaFile, cont);
		if(!utility::platform::file::write(output_file + (".") + psl::from_string8_t(meta::META_EXTENSION), cont.to_string()))
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
	psl::cerr << psl::to_platform_string(errorMessage);
	utility::terminal::set_color(utility::terminal::color::WHITE);
	return;
}
