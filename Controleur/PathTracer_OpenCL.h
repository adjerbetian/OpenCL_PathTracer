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

	void OpenCL_RunKernel(
		uint		 global__imageWidth,
		uint		 global__imageHeight,
		uint		 global__imageSize,
		uint		 global__rayMaxDepth,
		RGBAColor	*global__imageColor,
		float		*global__imageRayNb,
		uint		*global__rayDepths,
		uint		*global__rayIntersectedBBx,
		uint		*global__rayIntersectedTri,
		bool		(*UpdateWindowFunc)(void),
		uint		 numImagesToRender,
		double*		 pathTracingTime,
		double*		 displayTime
		);


	void	OpenCL_InitializeMemory		(
		Float4		const&	global__cameraDirection		,
		Float4		const&	global__cameraRight			,
		Float4		const&	global__cameraUp			,
		Float4		const&	global__cameraPosition		,

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
		uint		const	 global__rayMaxDepth		,
		RGBAColor			*global__imageColor			,
		float				*global__imageRayNb			,
		uint				*global__rayDepths			,
		uint				*global__rayIntersectedBBx	,
		uint				*global__rayIntersectedTri	,
		uint		const	 global__bvhMaxDepth		,

		Sky			const	*global__sky				
		);

	char*			OpenCL_ReadSources			(const char *fileName);
	cl_platform_id	OpenCL_GetIntelOCLPlatform	();
	void			OpenCL_BuildFailLog( cl_program program, cl_device_id device_id );
	void			OpenCL_SetupContext			(Sampler sampler, uint const global__rayMaxDepth);
	void			OpenCL_ErrorHandling		(cl_int errCode);
}

#endif