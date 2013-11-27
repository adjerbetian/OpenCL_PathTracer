#ifndef PATHTRACER_PATHTRACERIMPORTER
#define PATHTRACER_PATHTRACERIMPORTER


#include "PathTracer_Utils.h"
#include "PathTracer_Structs.h"


namespace PathTracerNS
{

	class PathTracerImporter
	{

	public:

		//	Méthodes

		PathTracerImporter();

		void Initialize (
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
			);

		virtual void Import() = 0;



		//	Attributs

		Float4*		 ptr__global__cameraDirection;
		Float4*		 ptr__global__cameraRight;
		Float4*		 ptr__global__cameraUp;
		Float4*		 ptr__global__cameraPosition;

		Triangle*	*ptr__global__triangulation;
		Light*		*ptr__global__lights;
		Material*	*ptr__global__materiaux;
		Texture*	*ptr__global__textures;
		Uchar4*		*ptr__global__texturesData;
		uint*		 ptr__global__triangulationSize;
		uint*		 ptr__global__lightsSize;
		uint*		 ptr__global__materiauxSize;
		uint*		 ptr__global__texturesSize;
		uint*		 ptr__global__texturesDataSize;

		uint*		 ptr__global__imageWidth;
		uint*		 ptr__global__imageHeight;
		uint*		 ptr__global__imageSize;

		Sky*		 ptr__global__sky;


	};
}

#endif