

#include "PathTracer_OpenCL.h"
#include <ctime>
#include <vector>
#include <algorithm>
#include <sstream>

namespace PathTracerNS
{

	//	Variables générales OpenCL 

	cl_context			 opencl__context				= NULL;
	cl_command_queue	 opencl__queue					= NULL;
	cl_program			 opencl__program				= NULL;
	cl_device_id		 opencl__device					= NULL;

	//	Ensemble des noyaux appelable depuis l'hôte

	cl_kernel			 opencl__Kernel_Main								= NULL;	

	//	Objets mémoires aloué sur le device OpenCL

	cl_mem				 kernel__imageColor				= NULL;
	cl_mem				 kernel__imageRayNb				= NULL;
	cl_mem				 kernel__bvh					= NULL;
	cl_mem				 kernel__triangulation			= NULL;
	cl_mem				 kernel__lights					= NULL;
	cl_mem				 kernel__materiaux				= NULL;
	cl_mem				 kernel__textures				= NULL;
	cl_mem				 kernel__texturesData			= NULL;
	cl_mem				 kernel__sky					= NULL;

	// tableaux utiilisés pour la récupération des données depuis le device

	RGBAColor			*opencl__imageColorFromDevice	= NULL;
	uint				*opencl__imageRayNbFromDevice	= NULL;


	/*	Fonction de lancement des noyaux
	*		1 - Lancement du noyeux principal								
	*		2 - Récupération des données de rendu et affichage				
	*		3 - Libération de la mémoire sur le device et des variables OpenCL
	*/

	bool OpenCL_RunKernel(
		uint		global__imageWidth,
		uint		global__imageHeight,
		uint		global__imageSize,
		RGBAColor	*global__imageColor,
		uint		*global__imageRayNb,
		bool		(*UpdateWindowFunc)(void),
		uint		numImagesToRender
		)
	{
		cl_int errCode = 0;

		const size_t constGlobalWorkSize[2] = { global__imageWidth , global__imageHeight };
		//const size_t constGlobalWorkSize[2] = { 1 , 1 };
		const size_t constLocalWorkSize[2]  = { 1, 1 };
		//const size_t constLocalWorkSize[2]  = { 32, 1 };

		cl_uint imageId = 0;

		while(true && imageId < numImagesToRender)
		{
			CONSOLE << "Computing sample number " << imageId << ENDL;

			/////////////////////////////////////////////////////////////////////////////////
			//////////////////// 1 -  MAIN KERNEL  //////////////////////////////////////////
			/////////////////////////////////////////////////////////////////////////////////

			errCode = clSetKernelArg(opencl__Kernel_Main, 0, sizeof(cl_uint), (void*) &imageId);
			if(OpenCL_ErrorHandling(errCode)) return false;

			errCode = clEnqueueNDRangeKernel(opencl__queue, opencl__Kernel_Main, 2, NULL, constGlobalWorkSize, constLocalWorkSize, 0, NULL, NULL);
			if(OpenCL_ErrorHandling(errCode)) return false;

			clFinish(opencl__queue);

			/////////////////////////////////////////////////////////////////////////////////
			////////////// 3 - RECUPERERATION DES DONNEES  //////////////////////////////////
			/////////////////////////////////////////////////////////////////////////////////
			errCode = clEnqueueReadBuffer(opencl__queue, kernel__imageColor,	CL_TRUE, 0, sizeof(RGBAColor)	* global__imageSize	, (void *) global__imageColor, 0, NULL , NULL); if(OpenCL_ErrorHandling(errCode)) return false;
			errCode = clEnqueueReadBuffer(opencl__queue, kernel__imageRayNb,	CL_TRUE, 0, sizeof(uint)		* global__imageSize	, (void *) global__imageRayNb, 0, NULL , NULL); if(OpenCL_ErrorHandling(errCode)) return false;
			clFinish(opencl__queue);

			(*UpdateWindowFunc)();

			imageId++;
		}


		/////////////////////////////////////////////////////////////////////////////////
		///////////  5 -   LIBERATION DES DONNEES MEMOIRE OPENCL  ///////////////////////
		/////////////////////////////////////////////////////////////////////////////////

		errCode = clReleaseMemObject(kernel__imageColor);		if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clReleaseMemObject(kernel__imageRayNb);		if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clReleaseMemObject(kernel__bvh);				if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clReleaseMemObject(kernel__triangulation);	if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clReleaseMemObject(kernel__lights);			if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clReleaseMemObject(kernel__materiaux);		if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clReleaseMemObject(kernel__textures);			if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clReleaseMemObject(kernel__texturesData);		if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clReleaseMemObject(kernel__sky);				if(OpenCL_ErrorHandling(errCode)) return false;

		errCode = clFlush				(opencl__queue		 ); if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clFinish				(opencl__queue		 ); if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clReleaseKernel		(opencl__Kernel_Main ); if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clReleaseProgram		(opencl__program	 ); if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clReleaseCommandQueue	(opencl__queue		 ); if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clReleaseContext		(opencl__context	 ); if(OpenCL_ErrorHandling(errCode)) return false;

		return true;
	}


	/*	Fonction d'intilisation des objets mémoire OpenCL
	*
	*		1 - Allocation des objets mémoire sur le device et copie des données
	*		2 - Assignation des arguments des noyaux
	*/

	bool OpenCL_InitializeMemory(
		Float4		const	global__cameraDirection		,
		Float4		const	global__cameraRight		,
		Float4		const	global__cameraUp		,
		Float4		const	global__cameraPosition		,

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

		Sky			const	*global__sky
		)
	{

		/////////////////////////////////////////////////////////////////////////////////
		///////////  1 - ALLOCATION ET COPIE  ///////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////

		cl_int errCode = 0;
		cl_uint i = 0;

		kernel__imageColor		= clCreateBuffer(opencl__context, CL_MEM_READ_WRITE                        , sizeof(RGBAColor)    * global__imageSize						   , NULL							, &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__imageRayNb		= clCreateBuffer(opencl__context, CL_MEM_READ_WRITE                        , sizeof(cl_uint)      * global__imageSize						   , NULL							, &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__bvh				= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , std::max<uint>(sizeof(Node)	  * global__bvhSize				,1), (void *) global__bvh			, &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__triangulation	= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , std::max<uint>(sizeof(Triangle) * global__triangulationSize	,1), (void *) global__triangulation	, &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__lights			= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , std::max<uint>(sizeof(Light)	  * global__lightsSize			,1), (void *) global__lights		, &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__materiaux		= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , std::max<uint>(sizeof(Material) * global__materiauxSize		,1), (void *) global__materiaux		, &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__texturesData	= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , std::max<uint>(sizeof(Uchar4)	  * global__texturesDataSize	,1), (void *) global__texturesData	, &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__textures		= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , std::max<uint>(sizeof(Texture)  * global__texturesSize			,1), (void *) global__textures		, &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__sky				= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , sizeof(Sky)													   , (void *) global__sky			, &errCode); if(OpenCL_ErrorHandling(errCode)) return false;


		/////////////////////////////////////////////////////////////////////////////////
		///////////  1 - ARGUEMENTS POUR LES NOYAUX  ////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////

		// ARGUMENTS POUR global__kernelSortedMain
		i = 1;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(cl_uint),		(void*) &global__imageWidth);		if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(cl_uint),		(void*) &global__imageHeight);		if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(cl_float4),	(void*) &global__cameraPosition);	if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(cl_float4),	(void*) &global__cameraDirection);	if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(cl_float4),	(void*) &global__cameraRight);		if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(cl_float4),	(void*) &global__cameraUp);			if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(cl_uint),		(void*) &global__lightsSize);		if(OpenCL_ErrorHandling(errCode)) return false;

		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__imageColor),		(void*) &kernel__imageColor);		if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__imageRayNb),		(void*) &kernel__imageRayNb);		if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__bvh),				(void*) &kernel__bvh);				if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__triangulation),	(void*) &kernel__triangulation);	if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__lights),			(void*) &kernel__lights);			if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__materiaux),		(void*) &kernel__materiaux);		if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__textures),		(void*) &kernel__textures);			if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__texturesData),	(void*) &kernel__texturesData);		if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__sky),				(void*) &kernel__sky);				if(OpenCL_ErrorHandling(errCode)) return false;

		return true;
	}

	char *OpenCL_ReadSources(const char *fileName)
	{
		FILE *file = fopen(fileName, "rb");
		if (!file)
		{
			printf("ERROR: Failed to open file '%s'\n", fileName);
			return NULL;
		}

		if (fseek(file, 0, SEEK_END))
		{
			printf("ERROR: Failed to seek file '%s'\n", fileName);
			fclose(file);
			return NULL;
		}

		long size = ftell(file);
		if (size == 0)
		{
			printf("ERROR: Failed to check position on file '%s'\n", fileName);
			fclose(file);
			return NULL;
		}

		rewind(file);

		char *src = (char *)malloc(sizeof(char) * size + 1);
		if (!src)
		{
			printf("ERROR: Failed to allocate memory for file '%s'\n", fileName);
			fclose(file);
			return NULL;
		}

		size_t res = fread(src, 1, sizeof(char) * size, file);
		if (res != sizeof(char) * size)
		{
			printf("ERROR: Failed to read file '%s'\n", fileName);
			fclose(file);
			free(src);
			return NULL;
		}

		src[size] = '\0'; /* NULL terminated */
		fclose(file);

		return src;
	}

	cl_platform_id OpenCL_GetPlatform(char const *platformName)
	{
		cl_platform_id pPlatforms[10] = { 0 };
		char pPlatformName[128] = { 0 };

		cl_uint uiPlatformsCount = 0;
		cl_int err = clGetPlatformIDs(10, pPlatforms, &uiPlatformsCount);
		for (cl_uint ui = 0; ui < uiPlatformsCount; ++ui)
		{
			err = clGetPlatformInfo(pPlatforms[ui], CL_PLATFORM_NAME, 128 * sizeof(char), pPlatformName, NULL);
			if ( err != CL_SUCCESS )
			{
				printf("ERROR: Failed to retreive platform vendor name.\n", ui);
				return NULL;
			}

			if (!strcmp(pPlatformName, platformName))
				return pPlatforms[ui];
		}

		return NULL;
	}

	void OpenCL_BuildFailLog( cl_program program,
		cl_device_id device_id )
	{
		size_t paramValueSizeRet = 0;
		clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &paramValueSizeRet);

		char* buildLogMsgBuf = (char *)malloc(sizeof(char) * paramValueSizeRet + 1);
		if( buildLogMsgBuf )
		{
			clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, paramValueSizeRet, buildLogMsgBuf, &paramValueSizeRet);
			buildLogMsgBuf[paramValueSizeRet] = '\0';	//mark end of message string

			printf("Build Log:\n");
			puts(buildLogMsgBuf);
			fflush(stdout);

			free(buildLogMsgBuf);
		}
	}

	bool OpenCL_SetupContext()
	{
		const char* program_source = PATHTRACER_FOLDER"Kernel\\PathTracer_FullKernel.cl";

		cl_device_id devices[16];
		size_t cb;
		cl_uint size_ret = 0;
		cl_int err;
		int num_cores;
		cl_device_id device_ID;
		char device_name[128] = {0};
		const bool runOnGPU = false;

		//if(runOnGPU) printf("Trying to run on a Processor Graphics \n");
		//else		 printf("Trying to run on a CPU \n");

		char const* platformName1 = "Intel(R) OpenCL";
		char const* platformName2 = "AMD Accelerated Parallel Processing";

		cl_platform_id platform_id = OpenCL_GetPlatform(platformName1);
		if( platform_id == NULL )
		{
			platform_id = OpenCL_GetPlatform(platformName2);
			if( platform_id == NULL )
			{
				printf("ERROR: Failed to find requested platforms. Stopping.\n");
				return false;
			}
		}

		cl_context_properties context_properties[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platform_id, NULL };

		// create the OpenCL context on a CPU/PG 
		if(runOnGPU) opencl__context = clCreateContextFromType(context_properties, CL_DEVICE_TYPE_GPU, NULL, NULL, NULL);
		else		 opencl__context = clCreateContextFromType(context_properties, CL_DEVICE_TYPE_CPU, NULL, NULL, NULL);

		if (opencl__context == (cl_context)0)
			return false;

		// get the list of CPU devices associated with context
		err = clGetContextInfo(opencl__context, CL_CONTEXT_DEVICES, 0, NULL, &cb);
		clGetContextInfo(opencl__context, CL_CONTEXT_DEVICES, cb, devices, NULL);

		opencl__queue = clCreateCommandQueue(opencl__context, devices[0], 0, NULL);
		if (opencl__queue == (cl_command_queue)0)
			return false;

		char *sources = OpenCL_ReadSources(program_source);	//read program .cl source file
		if(sources == NULL)
			return false;

		opencl__program = clCreateProgramWithSource(opencl__context, 1, (const char**)&sources, NULL, NULL);
		if (opencl__program == (cl_program)0)
		{
			printf("ERROR: Failed to create Program with source...\n");
			free(sources);
			return false;
		}

		std::string buildOptionsString;
		if(false)
		{
			buildOptionsString += "-g -s \"";
			buildOptionsString += program_source;
			buildOptionsString += "\"";
		}


		err = clBuildProgram(opencl__program, 0, NULL, buildOptionsString.c_str(), NULL, NULL);
		if (err != CL_SUCCESS)
		{
			printf("ERROR: Failed to build program...\n");
			OpenCL_BuildFailLog(opencl__program, devices[0]);
			free(sources);
			return false;
		}

		opencl__Kernel_Main = clCreateKernel(opencl__program, "Kernel_Main", NULL);
		if (opencl__Kernel_Main == (cl_kernel)0)
		{
			printf("ERROR: Failed to create kernel...\n");
			free(sources);
			return false;
		}
		free(sources);

		// retrieve platform information

		// use first device ID
		device_ID = devices[0];

		err = clGetDeviceInfo(device_ID, CL_DEVICE_NAME, 128, device_name, NULL);
		if (err!=CL_SUCCESS)
		{
			printf("ERROR: Failed to get device information (device name)...\n");
			return false;
		}
		err = clGetDeviceInfo(device_ID, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &num_cores, NULL);
		if (err!=CL_SUCCESS)
		{
			printf("ERROR: Failed to get device information (max compute units)...\n");
			return false;
		}
		printf("Using device %s and using %d compute units.\n", device_name, num_cores);

		return true; // success...
	}

	/*	Fonction qui permet de déléguer la gestion des erreurs OpenCL comme par exemple les erreurs de compilations
	*/

	bool OpenCL_ErrorHandling(cl_int errCode)
	{
		if(errCode==0)
			return false;
		CONSOLE << ENDL;
		CONSOLE << "//////////////////////////////////////////////////////////////////////////" << ENDL;

		switch (errCode)
		{
		case  2 : CONSOLE << "GLOBAL ERROR : NO OPENCL PLATFORM " << ENDL; break;
		case  1 : CONSOLE << "FILE ERROR : CANNOT OPEN FILE" << ENDL; break;

		case -1 : CONSOLE << "OPENCL ERROR : CL_DEVICE_NOT_FOUND"; break;
		case -2 : CONSOLE << "OPENCL ERROR : CL_DEVICE_NOT_AVAILABLE";	break;
		case -3 : CONSOLE << "OPENCL ERROR : CL_COMPILER_NOT_AVAILABLE"; break;
		case -4 : CONSOLE << "OPENCL ERROR : CL_MEM_OBJECT_ALLOCATION_FAILURE"; break;
		case -5 : CONSOLE << "OPENCL ERROR : CL_OUT_OF_RESOURCES"; break;
		case -6 : CONSOLE << "OPENCL ERROR : CL_OUT_OF_HOST_MEMORY"; break;
		case -7 : CONSOLE << "OPENCL ERROR : CL_PROFILING_INFO_NOT_AVAILABLE"; break;
		case -8 : CONSOLE << "OPENCL ERROR : CL_MEM_COPY_OVERLAP"; break;
		case -9 : CONSOLE << "OPENCL ERROR : CL_IMAGE_FORMAT_MISMATCH"; break;
		case -10: CONSOLE << "OPENCL ERROR : CL_IMAGE_FORMAT_NOT_SUPPORTED"; break;
		case -11: CONSOLE << "OPENCL ERROR : CL_BUILD_PROGRAM_FAILURE"; break;
		case -12: CONSOLE << "OPENCL ERROR : CL_MAP_FAILURE"; break;
		case -13: CONSOLE << "OPENCL ERROR : CL_MISALIGNED_SUB_BUFFER_OFFSET"; break;
		case -14: CONSOLE << "OPENCL ERROR : CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST"; break;
		case -30: CONSOLE << "OPENCL ERROR : CL_INVALID_VALUE"; break;
		case -31: CONSOLE << "OPENCL ERROR : CL_INVALID_DEVICE_TYPE"; break;
		case -32: CONSOLE << "OPENCL ERROR : CL_INVALID_PLATFORM"; break;
		case -33: CONSOLE << "OPENCL ERROR : CL_INVALID_DEVICE"; break;
		case -34: CONSOLE << "OPENCL ERROR : CL_INVALID_CONTEXT"; break;
		case -35: CONSOLE << "OPENCL ERROR : CL_INVALID_QUEUE_PROPERTIES"; break;
		case -36: CONSOLE << "OPENCL ERROR : CL_INVALID_COMMAND_QUEUE"; break;
		case -37: CONSOLE << "OPENCL ERROR : CL_INVALID_HOST_PTR"; break;
		case -38: CONSOLE << "OPENCL ERROR : CL_INVALID_MEM_OBJECT"; break;
		case -39: CONSOLE << "OPENCL ERROR : CL_INVALID_IMAGE_FORMAT_DESCRIPTOR"; break;
		case -40: CONSOLE << "OPENCL ERROR : CL_INVALID_IMAGE_SIZE"; break;
		case -41: CONSOLE << "OPENCL ERROR : CL_INVALID_SAMPLER"; break;
		case -42: CONSOLE << "OPENCL ERROR : CL_INVALID_BINARY"; break;
		case -43: CONSOLE << "OPENCL ERROR : CL_INVALID_BUILD_OPTIONS"; break;
		case -44: CONSOLE << "OPENCL ERROR : CL_INVALID_PROGRAM"; break;
		case -45: CONSOLE << "OPENCL ERROR : CL_INVALID_PROGRAM_EXECUTABLE"; break;
		case -46: CONSOLE << "OPENCL ERROR : CL_INVALID_KERNEL_NAME"; break;
		case -47: CONSOLE << "OPENCL ERROR : CL_INVALID_KERNEL_DEFINITION"; break;
		case -48: CONSOLE << "OPENCL ERROR : CL_INVALID_KERNEL"; break;
		case -49: CONSOLE << "OPENCL ERROR : CL_INVALID_ARG_INDEX"; break;
		case -50: CONSOLE << "OPENCL ERROR : CL_INVALID_ARG_VALUE"; break;
		case -51: CONSOLE << "OPENCL ERROR : CL_INVALID_ARG_SIZE"; break;
		case -52: CONSOLE << "OPENCL ERROR : CL_INVALID_KERNEL_ARGS"; break;
		case -53: CONSOLE << "OPENCL ERROR : CL_INVALID_WORK_DIMENSION"; break;
		case -54: CONSOLE << "OPENCL ERROR : CL_INVALID_WORK_GROUP_SIZE"; break;
		case -55: CONSOLE << "OPENCL ERROR : CL_INVALID_WORK_ITEM_SIZE"; break;
		case -56: CONSOLE << "OPENCL ERROR : CL_INVALID_GLOBAL_OFFSET"; break;
		case -57: CONSOLE << "OPENCL ERROR : CL_INVALID_EVENT_WAIT_LIST"; break;
		case -58: CONSOLE << "OPENCL ERROR : CL_INVALID_EVENT"; break;
		case -59: CONSOLE << "OPENCL ERROR : CL_INVALID_OPERATION"; break;
		case -60: CONSOLE << "OPENCL ERROR : CL_INVALID_GL_OBJECT"; break;
		case -61: CONSOLE << "OPENCL ERROR : CL_INVALID_BUFFER_SIZE"; break;
		case -62: CONSOLE << "OPENCL ERROR : CL_INVALID_MIP_LEVEL"; break;
		case -63: CONSOLE << "OPENCL ERROR : CL_INVALID_GLOBAL_WORK_SIZE"; break;
		}

		CONSOLE << ENDL;
		CONSOLE << "//////////////////////////////////////////////////////////////////////////" << ENDL;
		CONSOLE << ENDL;


		if(errCode == -11) // Build fail
		{
			size_t log_size;
			clGetProgramBuildInfo(opencl__program, opencl__device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
			char *log = (char *) malloc(log_size);
			clGetProgramBuildInfo(opencl__program, opencl__device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
			CONSOLE << log << ENDL;

			CONSOLE << ENDL;
			CONSOLE << "//////////////////////////////////////////////////////////////////////////" << ENDL;
			return true;
		}

		return true;
	}



}