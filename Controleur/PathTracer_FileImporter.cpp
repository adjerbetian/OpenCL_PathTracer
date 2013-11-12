
#include "PathTracer_FileImporter.h"


namespace PathTracerNS
{
	const std::string PathTracerFileImporter::fileSizesPath			= PATHTRACER_SCENE_FOLDER"ExportedScene\\sizes.pth";
	const std::string PathTracerFileImporter::filePointersPath		= PATHTRACER_SCENE_FOLDER"ExportedScene\\pointers.pth";
	const std::string PathTracerFileImporter::fileTextureDataPath	= PATHTRACER_SCENE_FOLDER"ExportedScene\\textureData.pth";


	PathTracerFileImporter::PathTracerFileImporter():PathTracerImporter()
	{}


	void PathTracerFileImporter::Export()
	{
		size_t nWritte = 0;

		FILE* fichier = NULL;
		fopen_s(&fichier, fileSizesPath.c_str(), "wb");

		if(fichier != NULL)
		{

			nWritte = fwrite(ptr__global__cameraDirection	, sizeof(Double4), 1, fichier);
			nWritte = fwrite(ptr__global__cameraRight		, sizeof(Double4), 1, fichier);
			nWritte = fwrite(ptr__global__cameraUp		, sizeof(Double4), 1, fichier);
			nWritte = fwrite(ptr__global__cameraPosition	, sizeof(Double4), 1, fichier);

			nWritte = fwrite(ptr__global__triangulationSize	, sizeof(uint), 1, fichier);
			nWritte = fwrite(ptr__global__lightsSize		, sizeof(uint), 1, fichier);
			nWritte = fwrite(ptr__global__materiauxSize		, sizeof(uint), 1, fichier);
			nWritte = fwrite(ptr__global__texturesSize		, sizeof(uint), 1, fichier);
			nWritte = fwrite(ptr__global__textureDataSize	, sizeof(uint), 1, fichier);
			nWritte = fwrite(ptr__global__imageWidth		, sizeof(uint), 1, fichier);
			nWritte = fwrite(ptr__global__imageHeight		, sizeof(uint), 1, fichier);

			fclose(fichier);
		}

		FILE* fichierPtr = NULL;
		 fopen_s(&fichierPtr, filePointersPath.c_str(), "wb");

		if(fichierPtr != NULL)
		{
			if((*ptr__global__triangulationSize)>0)	nWritte = fwrite(*ptr__global__triangulation	, sizeof(Triangle)	, (*ptr__global__triangulationSize)	, fichierPtr);
			if((*ptr__global__lightsSize)>0)		nWritte = fwrite(*ptr__global__lights			, sizeof(Light)		, (*ptr__global__lightsSize)		, fichierPtr);
			if((*ptr__global__materiauxSize)>0)		nWritte = fwrite(*ptr__global__materiaux		, sizeof(Material)	, (*ptr__global__materiauxSize)		, fichierPtr);
			if((*ptr__global__texturesSize)>0)		nWritte = fwrite(*ptr__global__textures			, sizeof(Texture)	, (*ptr__global__texturesSize)		, fichierPtr);

			nWritte = fwrite(ptr__global__sun					, sizeof(SunLight)	, 1, fichierPtr);
			nWritte = fwrite(ptr__global__sky					, sizeof(Sky)		, 1, fichierPtr);

			fclose(fichierPtr);
		}

		FILE* fichierTex = NULL;
		fopen_s(&fichierTex, fileTextureDataPath.c_str(), "wb");

		if(fichierTex != NULL)
		{
			nWritte = fwrite(*ptr__global__textureData		, sizeof(Uchar4), *ptr__global__textureDataSize		, fichierTex);
			fclose(fichierTex);
		}

	}



	void PathTracerFileImporter::Import()
	{
		FILE* fichierSizes = NULL;
		fopen_s(&fichierSizes, fileSizesPath.c_str(), "rb");

		size_t nRead = 0;

		if(fichierSizes != NULL)
		{
			nRead = fread(ptr__global__cameraDirection	, sizeof(Double4), 1, fichierSizes);
			nRead = fread(ptr__global__cameraRight	, sizeof(Double4), 1, fichierSizes);
			nRead = fread(ptr__global__cameraUp	, sizeof(Double4), 1, fichierSizes);
			nRead = fread(ptr__global__cameraPosition	, sizeof(Double4), 1, fichierSizes);

			nRead = fread(ptr__global__triangulationSize, sizeof(uint), 1, fichierSizes);
			nRead = fread(ptr__global__lightsSize		, sizeof(uint), 1, fichierSizes);
			nRead = fread(ptr__global__materiauxSize	, sizeof(uint), 1, fichierSizes);
			nRead = fread(ptr__global__texturesSize		, sizeof(uint), 1, fichierSizes);
			nRead = fread(ptr__global__textureDataSize	, sizeof(uint), 1, fichierSizes);
			nRead = fread(ptr__global__imageWidth		, sizeof(uint), 1, fichierSizes);
			nRead = fread(ptr__global__imageHeight		, sizeof(uint), 1, fichierSizes);

			fclose(fichierSizes);
		}

		*ptr__global__imageSize = (*ptr__global__imageWidth) * (*ptr__global__imageHeight);

		*ptr__global__triangulation = new Triangle	[(*ptr__global__triangulationSize)];
		*ptr__global__lights		= new Light		[(*ptr__global__lightsSize)];
		*ptr__global__materiaux		= new Material	[(*ptr__global__materiauxSize)];
		*ptr__global__textures		= new Texture	[(*ptr__global__texturesSize)];

		FILE* fichierPtr = NULL;
		fopen_s(&fichierPtr, filePointersPath.c_str(), "rb");

		if(fichierPtr != NULL)
		{
			nRead = fread(*ptr__global__triangulation		, sizeof(Triangle)	, (*ptr__global__triangulationSize)	, fichierPtr);
			nRead = fread(*ptr__global__lights				, sizeof(Light)		, (*ptr__global__lightsSize)		, fichierPtr);
			nRead = fread(*ptr__global__materiaux			, sizeof(Material)	, (*ptr__global__materiauxSize)		, fichierPtr);
			nRead = fread(*ptr__global__textures			, sizeof(Texture)	, (*ptr__global__texturesSize)		, fichierPtr);

			nRead = fread(ptr__global__sun					, sizeof(SunLight)	, 1, fichierPtr);
			nRead = fread(ptr__global__sky					, sizeof(Sky)		, 1, fichierPtr);

			fclose(fichierPtr);
		}


		FILE* fichierTex = NULL;
		fopen_s(&fichierTex, fileTextureDataPath.c_str(), "rb");

		if(fichierTex != NULL)
		{
			*ptr__global__textureData = new Uchar4[*ptr__global__textureDataSize];
			nRead = fread(*ptr__global__textureData, sizeof(Uchar4), *ptr__global__textureDataSize, fichierTex);

			fclose(fichierTex);
		}


	}

}