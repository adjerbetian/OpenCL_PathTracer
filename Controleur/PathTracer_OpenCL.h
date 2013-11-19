#ifndef PATHTRACER_OPENCL
#define PATHTRACER_OPENCL

#ifdef __APPLE__
#include <CL/opencl.h>
#else
#include <CL/cl.h>
#endif

#include "PathTracer_Utils.h"
#include "PathTracer_Structs.h"


namespace PathTracerNS
{

	bool OpenCL_RunKernel(
		uint		global__imageWidth,
		uint		global__imageHeight,
		uint		global__imageSize,
		RGBAColor	*global__imageColor,
		uint		*global__imageRayNb,
		bool		(*UpdateWindowFunc)(void)
		);


	bool	OpenCL_InitializeMemory		(
		Double4		const	global__cameraDirection		,
		Double4		const	global__cameraRight		,
		Double4		const	global__cameraUp		,
		Double4		const	global__cameraPosition		,

		Node		const	*global__bvh				,
		Triangle	const	*global__triangulation		,
		Light		const	*global__lights				,
		Material	const	*global__materiaux			,
		Texture		const	*global__textures			,
		Uchar4		const	*global__texturesData		,
		uint		const	 global__bvhSize			,
		uint		const	 global__triangulationSize	,
		uint		const	 global__lightsSize			,
		uint		const	 global__materiauxSize		,
		uint		const	 global__texturesSize		,
		uint		const	 global__texturesDataSize	,

		uint		const	 global__imageWidth			,
		uint		const	 global__imageHeight		,
		uint		const	 global__imageSize			,
		RGBAColor			*global__imageColor		,
		uint				*global__imageRayNb		,
		uint		const	 global__bvhMaxDepth		,

		SunLight	const	*global__sun				,
		Sky			const	*global__sky				
		);

	bool	OpenCL_InitializeContext	();
	bool	OpenCL_ErrorHandling		(cl_int errCode);
}

#endif