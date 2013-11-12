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

	void	PathTracer_Main						();
	void	PathTracer_Initialize				();
	void	PathTracer_InitializeImage			();
	void	PathTracer_InitializeWindow			();
	void	PathTracer_Export					();
	void	PathTracer_Clear					();
	void	PathTracer_PaintLoadingScreen		();
	bool	PathTracer_UpdateWindow				();


	//	Fonction d'impression

	std::string		BoundingBox_ToString				(BoundingBox const *This);
	std::string		Triangle_ToString					(Triangle const *This);
	std::string		Vector_ToString						(Double4 const *This);
	void			Node_Print							(Node const *This, uint n);
	void			PathTracer_PrintBVH					();
	void			PathTracer_PrintBVHCharacteristics	();

}

#endif