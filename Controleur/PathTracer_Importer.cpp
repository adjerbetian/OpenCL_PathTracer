
#include "PathTracer_Importer.h"


namespace PathTracerNS
{


	PathTracerImporter::PathTracerImporter()
		:ptr__global__cameraDirection(NULL),
		ptr__global__cameraRight(NULL),
		ptr__global__cameraUp(NULL),
		ptr__global__cameraPosition(NULL),

		ptr__global__triangulation(NULL),
		ptr__global__lights(NULL),
		ptr__global__materiaux(NULL),
		ptr__global__textures(NULL),
		ptr__global__texturesData(NULL),
		ptr__global__triangulationSize(NULL),
		ptr__global__lightsSize(NULL),
		ptr__global__materiauxSize(NULL),
		ptr__global__texturesSize(NULL),

		ptr__global__imageWidth(NULL),
		ptr__global__imageHeight(NULL),
		ptr__global__imageSize(NULL),

		ptr__global__sky(NULL)
	{};


	void PathTracerImporter::Initialize(GlobalVars& globalVars)
	{
		ptr__global__cameraDirection	= &globalVars.cameraDirection;
		ptr__global__cameraRight		= &globalVars.cameraRight;
		ptr__global__cameraUp			= &globalVars.cameraUp;
		ptr__global__cameraPosition		= &globalVars.cameraPosition;

		ptr__global__triangulation		= &globalVars.triangulation;
		ptr__global__lights				= &globalVars.lights;
		ptr__global__materiaux			= &globalVars.materiaux;
		ptr__global__textures			= &globalVars.textures;
		ptr__global__texturesData		= &globalVars.texturesData;
		ptr__global__triangulationSize	= &globalVars.triangulationSize;
		ptr__global__lightsSize			= &globalVars.lightsSize;
		ptr__global__materiauxSize		= &globalVars.materiauxSize;
		ptr__global__texturesSize		= &globalVars.texturesSize;
		ptr__global__texturesDataSize	= &globalVars.texturesDataSize;

		ptr__global__imageWidth			= &globalVars.imageWidth;
		ptr__global__imageHeight		= &globalVars.imageHeight;
		ptr__global__imageSize			= &globalVars.imageSize;

		ptr__global__sky				= &globalVars.sky;
	}




}