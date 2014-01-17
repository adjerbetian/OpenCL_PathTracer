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

	void	OpenCL_RunKernel		(GlobalVars& globalVars, bool (*UpdateWindowFunc)(void), uint numImagesToRender, double* pathTracingTime, double* displayTime);
	void	OpenCL_InitializeMemory	(GlobalVars& globalVars);
	void	OpenCL_SetupContext		(GlobalVars& globalVars, Sampler sampler);

	char*			OpenCL_ReadSources			(const char *fileName);
	cl_platform_id	OpenCL_GetIntelOCLPlatform	();
	void			OpenCL_BuildFailLog			(cl_program program, cl_device_id device_id );
	void			OpenCL_ErrorHandling		(cl_int errCode);
}

#endif