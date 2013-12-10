#ifndef PATHTRACER_PATHTRACER
#define PATHTRACER_PATHTRACER

#ifdef __APPLE__
#include <CL/opencl.h>
#else
#include <CL/cl.h>
#endif

#include "PathTracer_Utils.h"
#include "PathTracer_Importer.h"
#include "PathTracer_BVH.h"

#ifndef LRT_PRODUCT
#include "../Alone/PathTracer_Dialog.h"
#include <iostream>
#else
#include "../LumenRT/PathTracer_Dialog.h"
#include "toolsLog.h"
#endif


namespace PathTracerNS
{

	void	PathTracer_SetImporter				(PathTracerImporter* importer);

	void	PathTracer_Main						(uint image_width, uint image_height, uint numImagesToRender, bool saveRenderedImages, bool loadSky, bool exportScene);
	void	PathTracer_Initialize				(uint image_width, uint image_height, bool saveRenderedImages, bool loadSky);
	void	PathTracer_InitializeImage			();
	void	PathTracer_InitializeWindow			(bool saveRenderedImages);
	void	PathTracer_Export					();
	void	PathTracer_Clear					();
	void	PathTracer_PaintLoadingScreen		();
	bool	PathTracer_UpdateWindow				();
	void	PathTracer_ComputeStatistics		(uint numImageToRender, double loadingTime, double bvhBuildingTime, double openclSettingTime, double pathTracingTime, double displayTime);

	//	Fonction d'impression

	std::string		BoundingBox_ToString				(BoundingBox const *This);
	std::string		Triangle_ToString					(Triangle const *This);
	std::string		Vector_ToString						(Float4 const *This);
	void			Node_Print							(Node const *This, uint n);
	void			PathTracer_PrintBVH					();
	void			PathTracer_PrintBVHCharacteristics	();
	void			PathTracer_PrintSection				(const char* section);

}

#endif