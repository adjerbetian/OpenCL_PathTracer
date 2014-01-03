
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


	void PathTracerImporter::Initialize(
		Float4*		 global__cameraDirection,
		Float4*		 global__cameraRight,
		Float4*		 global__cameraUp,
		Float4*		 global__cameraPosition,
		Triangle*	*global__triangulation,
		Light*		*global__lights,
		Material*	*global__materiaux,
		Texture*	*global__textures,
		Uchar4*		*global__texturesData,
		uint*		 global__triangulationSize,
		uint*		 global__lightsSize,
		uint*		 global__materiauxSize,
		uint*		 global__texturesSize,
		uint*		 global__texturesDataSize,
		uint*		 global__imageWidth,
		uint*		 global__imageHeight,
		uint*		 global__imageSize,
		Sky*		 global__sky
		)
	{
		ptr__global__cameraDirection	= global__cameraDirection;
		ptr__global__cameraRight		= global__cameraRight;
		ptr__global__cameraUp			= global__cameraUp;
		ptr__global__cameraPosition		= global__cameraPosition;

		ptr__global__triangulation		= global__triangulation;
		ptr__global__lights				= global__lights;
		ptr__global__materiaux			= global__materiaux;
		ptr__global__textures			= global__textures;
		ptr__global__texturesData		= global__texturesData;
		ptr__global__triangulationSize	= global__triangulationSize;
		ptr__global__lightsSize			= global__lightsSize;
		ptr__global__materiauxSize		= global__materiauxSize;
		ptr__global__texturesSize		= global__texturesSize;
		ptr__global__texturesDataSize	= global__texturesDataSize;

		ptr__global__imageWidth			= global__imageWidth;
		ptr__global__imageHeight		= global__imageHeight;
		ptr__global__imageSize			= global__imageSize;

		ptr__global__sky				= global__sky;
	}




}