

#include "PathTracer_OpenCL.h"
#include <ctime>
#include <vector>
#include <algorithm>
#include <sstream>

#include "shlobj.h"
#include <string>



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
	cl_mem				 kernel__rayDepths				= NULL;
	cl_mem				 kernel__rayIntersectedBBx		= NULL;
	cl_mem				 kernel__rayIntersectedTri		= NULL;
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

	void OpenCL_RunKernel(GlobalVars& globalVars,
		bool		(*UpdateWindowFunc)(void),
		uint		numImagesToRender,
		double*		pathTracingTime,
		double*		displayTime
		)
	{
		cl_int errCode = 0;

		*pathTracingTime = 0;
		*displayTime = 0;
		clock_t startTime;

		const size_t constGlobalWorkSize[2] = { globalVars.imageWidth , globalVars.imageHeight };
		const size_t constLocalWorkSize[2]  = { 1, 1 };

		cl_uint imageId = 0;

		while(true && imageId < numImagesToRender)
		{
			startTime = clock();
			CONSOLE_LOG << "Computing sample number " << imageId << ENDL;

			/////////////////////////////////////////////////////////////////////////////////
			//////////////////// 1 -  MAIN KERNEL  //////////////////////////////////////////
			/////////////////////////////////////////////////////////////////////////////////

			errCode = clSetKernelArg(opencl__Kernel_Main, 0, sizeof(cl_uint), (void*) &imageId);
			OpenCL_ErrorHandling(errCode);

			errCode = clEnqueueNDRangeKernel(opencl__queue, opencl__Kernel_Main, 2, NULL, constGlobalWorkSize, constLocalWorkSize, 0, NULL, NULL);
			OpenCL_ErrorHandling(errCode);

			/////////////////////////////////////////////////////////////////////////////////
			////////////// 3 - RECUPERERATION DES DONNEES  //////////////////////////////////
			/////////////////////////////////////////////////////////////////////////////////
			errCode = clEnqueueReadBuffer(opencl__queue, kernel__imageColor   ,	CL_TRUE, 0, sizeof(RGBAColor)	* globalVars.imageSize			, (void *) globalVars.imageColor   , 0, NULL , NULL); OpenCL_ErrorHandling(errCode);
			errCode = clEnqueueReadBuffer(opencl__queue, kernel__imageRayNb   ,	CL_TRUE, 0, sizeof(float)		* globalVars.imageSize			, (void *) globalVars.imageRayNb   , 0, NULL , NULL); OpenCL_ErrorHandling(errCode);
			clFinish(opencl__queue);

			*pathTracingTime += clock() - startTime;

			startTime = clock();
			(*UpdateWindowFunc)();
			*displayTime += clock() - startTime;

			imageId++;
		}

		// Retrieving statistic information
		errCode = clEnqueueReadBuffer(opencl__queue, kernel__rayDepths			,	CL_TRUE, 0, sizeof(uint)  * (globalVars.rayMaxDepth+1) , (void *) globalVars.rayDepths			, 0, NULL , NULL); OpenCL_ErrorHandling(errCode);
		errCode = clEnqueueReadBuffer(opencl__queue, kernel__rayIntersectedBBx	,	CL_TRUE, 0, sizeof(uint)  * MAX_INTERSETCION_NUMBER	, (void *) globalVars.rayIntersectedBBx	, 0, NULL , NULL); OpenCL_ErrorHandling(errCode);
		errCode = clEnqueueReadBuffer(opencl__queue, kernel__rayIntersectedTri	,	CL_TRUE, 0, sizeof(uint)  * MAX_INTERSETCION_NUMBER , (void *) globalVars.rayIntersectedTri	, 0, NULL , NULL); OpenCL_ErrorHandling(errCode);
		clFinish(opencl__queue);


		/////////////////////////////////////////////////////////////////////////////////
		///////////  5 -   LIBERATION DES DONNEES MEMOIRE OPENCL  ///////////////////////
		/////////////////////////////////////////////////////////////////////////////////

		errCode = clReleaseMemObject(kernel__imageColor);			OpenCL_ErrorHandling(errCode);
		errCode = clReleaseMemObject(kernel__imageRayNb);			OpenCL_ErrorHandling(errCode);
		errCode = clReleaseMemObject(kernel__rayDepths);			OpenCL_ErrorHandling(errCode);
		errCode = clReleaseMemObject(kernel__rayIntersectedBBx);	OpenCL_ErrorHandling(errCode);
		errCode = clReleaseMemObject(kernel__rayIntersectedTri);	OpenCL_ErrorHandling(errCode);
		errCode = clReleaseMemObject(kernel__bvh);					OpenCL_ErrorHandling(errCode);
		errCode = clReleaseMemObject(kernel__triangulation);		OpenCL_ErrorHandling(errCode);
		errCode = clReleaseMemObject(kernel__lights);				OpenCL_ErrorHandling(errCode);
		errCode = clReleaseMemObject(kernel__materiaux);			OpenCL_ErrorHandling(errCode);
		errCode = clReleaseMemObject(kernel__textures);				OpenCL_ErrorHandling(errCode);
		errCode = clReleaseMemObject(kernel__texturesData);			OpenCL_ErrorHandling(errCode);
		errCode = clReleaseMemObject(kernel__sky);					OpenCL_ErrorHandling(errCode);

		errCode = clFlush				(opencl__queue		 );		OpenCL_ErrorHandling(errCode);
		errCode = clFinish				(opencl__queue		 );		OpenCL_ErrorHandling(errCode);
		errCode = clReleaseKernel		(opencl__Kernel_Main );		OpenCL_ErrorHandling(errCode);
		errCode = clReleaseProgram		(opencl__program	 );		OpenCL_ErrorHandling(errCode);
		errCode = clReleaseCommandQueue	(opencl__queue		 );		OpenCL_ErrorHandling(errCode);
		errCode = clReleaseContext		(opencl__context	 );		OpenCL_ErrorHandling(errCode);
	}


	/*	Fonction d'intilisation des objets mémoire OpenCL
	*
	*		1 - Allocation des objets mémoire sur le device et copie des données
	*		2 - Assignation des arguments des noyaux
	*/

	void OpenCL_InitializeMemory(GlobalVars& globalVars)
	{

		/////////////////////////////////////////////////////////////////////////////////
		///////////  1 - ALLOCATION ET COPIE  ///////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////

		cl_int errCode = 0;
		cl_uint i = 0;

		kernel__imageColor			= clCreateBuffer(opencl__context, CL_MEM_READ_WRITE						   , sizeof(RGBAColor)				 * globalVars.imageSize			   , NULL								, &errCode); OpenCL_ErrorHandling(errCode);
		kernel__imageRayNb			= clCreateBuffer(opencl__context, CL_MEM_READ_WRITE						   , sizeof(cl_float)				 * globalVars.imageSize			   , NULL								, &errCode); OpenCL_ErrorHandling(errCode);
		kernel__rayDepths			= clCreateBuffer(opencl__context, CL_MEM_READ_WRITE						   , sizeof(cl_uint)				 * (globalVars.rayMaxDepth+1)	   , NULL								, &errCode); OpenCL_ErrorHandling(errCode);
		kernel__rayIntersectedBBx	= clCreateBuffer(opencl__context, CL_MEM_READ_WRITE						   , sizeof(cl_uint)				 * MAX_INTERSETCION_NUMBER	       , NULL								, &errCode); OpenCL_ErrorHandling(errCode);
		kernel__rayIntersectedTri	= clCreateBuffer(opencl__context, CL_MEM_READ_WRITE						   , sizeof(cl_uint)				 * MAX_INTERSETCION_NUMBER	       , NULL								, &errCode); OpenCL_ErrorHandling(errCode);
		kernel__bvh					= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , std::max<uint>(sizeof(Node)	 * globalVars.bvhSize			,1), (void *) globalVars.bvh			, &errCode); OpenCL_ErrorHandling(errCode);
		kernel__triangulation		= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , std::max<uint>(sizeof(Triangle) * globalVars.triangulationSize	,1), (void *) globalVars.triangulation	, &errCode); OpenCL_ErrorHandling(errCode);
		kernel__lights				= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , std::max<uint>(sizeof(Light)	 * globalVars.lightsSize		,1), (void *) globalVars.lights			, &errCode); OpenCL_ErrorHandling(errCode);
		kernel__materiaux			= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , std::max<uint>(sizeof(Material) * globalVars.materiauxSize		,1), (void *) globalVars.materiaux		, &errCode); OpenCL_ErrorHandling(errCode);
		kernel__texturesData		= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , std::max<uint>(sizeof(Uchar4)	 * globalVars.texturesDataSize	,1), (void *) globalVars.texturesData	, &errCode); OpenCL_ErrorHandling(errCode);
		kernel__textures			= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , std::max<uint>(sizeof(Texture)  * globalVars.texturesSize		,1), (void *) globalVars.textures		, &errCode); OpenCL_ErrorHandling(errCode);
		kernel__sky					= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , sizeof(Sky)													   , (void *) &globalVars.sky			, &errCode); OpenCL_ErrorHandling(errCode);


		/////////////////////////////////////////////////////////////////////////////////
		///////////  1 - ARGUEMENTS POUR LES NOYAUX  ////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////

		// ARGUMENTS POUR globalVars.kernelSortedMain
		i = 1;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(cl_float4),	(void*) &globalVars.cameraPosition);	OpenCL_ErrorHandling(errCode);
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(cl_float4),	(void*) &globalVars.cameraDirection);	OpenCL_ErrorHandling(errCode);
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(cl_float4),	(void*) &globalVars.cameraRight);		OpenCL_ErrorHandling(errCode);
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(cl_float4),	(void*) &globalVars.cameraUp);			OpenCL_ErrorHandling(errCode);

		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__imageColor),			(void*) &kernel__imageColor);			OpenCL_ErrorHandling(errCode);
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__imageRayNb),			(void*) &kernel__imageRayNb);			OpenCL_ErrorHandling(errCode);
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__rayDepths),			(void*) &kernel__rayDepths);			OpenCL_ErrorHandling(errCode);
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__rayIntersectedBBx),	(void*) &kernel__rayIntersectedBBx);	OpenCL_ErrorHandling(errCode);
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__rayIntersectedTri),	(void*) &kernel__rayIntersectedTri);	OpenCL_ErrorHandling(errCode);
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__bvh),					(void*) &kernel__bvh);					OpenCL_ErrorHandling(errCode);
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__triangulation),		(void*) &kernel__triangulation);		OpenCL_ErrorHandling(errCode);
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__lights),				(void*) &kernel__lights);				OpenCL_ErrorHandling(errCode);
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__materiaux),			(void*) &kernel__materiaux);			OpenCL_ErrorHandling(errCode);
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__textures),			(void*) &kernel__textures);				OpenCL_ErrorHandling(errCode);
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__texturesData),		(void*) &kernel__texturesData);			OpenCL_ErrorHandling(errCode);
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__sky),					(void*) &kernel__sky);					OpenCL_ErrorHandling(errCode);
	}

	char* OpenCL_ReadSources(char const* fileName)
	{
		FILE *file = fopen(fileName, "rb");
		if (!file)
		{
			CONSOLE << "ERROR: Failed to open file " << fileName << ENDL;
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

	void OpenCL_SetupContext(GlobalVars& globalVars, Sampler const sampler)
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
		char *sources = NULL;

		std::string const platformName1 = "Intel(R) OpenCL";
		std::string const platformName2 = "AMD Accelerated Parallel Processing";


		try
		{

			//if(runOnGPU) printf("Trying to run on a Processor Graphics \n");
			//else		 printf("Trying to run on a CPU \n");

			cl_platform_id platform_id = OpenCL_GetPlatform(platformName1.c_str());
			if( platform_id == NULL )
			{
				CONSOLE_LOG << "WARNING : the program didn't find any " << platformName1 << " platform. Trying with " << platformName2 << ENDL;
				platform_id = OpenCL_GetPlatform(platformName2.c_str());
				if( platform_id == NULL )
					throw std::runtime_error("ERROR : the program didn't find find the platform "+platformName2+". Aborting.");
			}

			cl_context_properties context_properties[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platform_id, NULL };

			// create the OpenCL context on a CPU/PG 
			if(runOnGPU) opencl__context = clCreateContextFromType(context_properties, CL_DEVICE_TYPE_GPU, NULL, NULL, NULL);
			else		 opencl__context = clCreateContextFromType(context_properties, CL_DEVICE_TYPE_CPU, NULL, NULL, NULL);

			if (opencl__context == (cl_context)0)
				throw std::runtime_error("Wrong context.");

			// get the list of CPU devices associated with context
			err = clGetContextInfo(opencl__context, CL_CONTEXT_DEVICES, 0, NULL, &cb);		OpenCL_ErrorHandling(err);
			err = clGetContextInfo(opencl__context, CL_CONTEXT_DEVICES, cb, devices, NULL); OpenCL_ErrorHandling(err);

			opencl__queue = clCreateCommandQueue(opencl__context, devices[0], 0, &err);		OpenCL_ErrorHandling(err);

			sources = OpenCL_ReadSources(program_source);	//read program .cl source file
			if(sources == NULL)
				throw std::runtime_error("Failed to read OpenCL sources.");

			opencl__program = clCreateProgramWithSource(opencl__context, 1, (const char**)&sources, NULL, &err); OpenCL_ErrorHandling(err);
			if (opencl__program == (cl_program)0)
				throw std::runtime_error("Failed to create Program with sources");

			std::ostringstream buildOptionsStream;
			buildOptionsStream << "-I \""PATHTRACER_FOLDER"Kernel\"";
			switch (sampler)
			{
			case JITTERED:	 buildOptionsStream << " -D SAMPLE_JITTERED";	 break;
			case RANDOM:	 buildOptionsStream << " -D SAMPLE_RANDOM";		 break;
			case UNIFORM:	 buildOptionsStream << " -D SAMPLE_UNIFORM";		 break;
			default: break;
			}
			buildOptionsStream << " -D MAX_REFLECTION_NUMBER=" << globalVars.rayMaxDepth;
			buildOptionsStream << " -D IMAGE_WIDTH=" << globalVars.imageWidth;
			buildOptionsStream << " -D IMAGE_HEIGHT=" << globalVars.imageHeight;
			buildOptionsStream << " -D LIGHTS_SIZE=" << globalVars.lightsSize;

			if(false) // if we want to use the intel OpenCL debugger (which doesn't work...)
				buildOptionsStream << "-g -s \"" << program_source << "\"";

			std::string buildOptionsString = buildOptionsStream.str();

			err = clBuildProgram(opencl__program, 0, NULL, buildOptionsString.c_str(), NULL, NULL);
			if (err != CL_SUCCESS)
			{
				OpenCL_BuildFailLog(opencl__program, devices[0]);
				throw std::runtime_error("Failed to build program.");
			}

			opencl__Kernel_Main = clCreateKernel(opencl__program, "Kernel_Main", &err); OpenCL_ErrorHandling(err);
			if (opencl__Kernel_Main == (cl_kernel)0)
				throw std::runtime_error("Failed to create kernel");

			device_ID = devices[0];

			err = clGetDeviceInfo(device_ID, CL_DEVICE_NAME				, 128			 , device_name	, NULL); OpenCL_ErrorHandling(err);
			err = clGetDeviceInfo(device_ID, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &num_cores	, NULL); OpenCL_ErrorHandling(err);

			CONSOLE << "Using device "<< device_name << " and using " << num_cores << " compute units." << ENDL;

		}
		catch(std::exception const& e)
		{
			free(sources);
			throw e;
		}
	}

	/*	Fonction qui permet de déléguer la gestion des erreurs OpenCL comme par exemple les erreurs de compilations
	*/

	void OpenCL_ErrorHandling(cl_int errCode)
	{
		if(errCode==0)
			return;
		std::ostringstream errorMessage;
		errorMessage << ENDL;
		errorMessage << "//////////////////////////////////////////////////////////////////////////" << ENDL;

		switch (errCode)
		{
		case -1 : errorMessage << "OPENCL ERROR : CL_DEVICE_NOT_FOUND"; break;
		case -2 : errorMessage << "OPENCL ERROR : CL_DEVICE_NOT_AVAILABLE";	break;
		case -3 : errorMessage << "OPENCL ERROR : CL_COMPILER_NOT_AVAILABLE"; break;
		case -4 : errorMessage << "OPENCL ERROR : CL_MEM_OBJECT_ALLOCATION_FAILURE"; break;
		case -5 : errorMessage << "OPENCL ERROR : CL_OUT_OF_RESOURCES"; break;
		case -6 : errorMessage << "OPENCL ERROR : CL_OUT_OF_HOST_MEMORY"; break;
		case -7 : errorMessage << "OPENCL ERROR : CL_PROFILING_INFO_NOT_AVAILABLE"; break;
		case -8 : errorMessage << "OPENCL ERROR : CL_MEM_COPY_OVERLAP"; break;
		case -9 : errorMessage << "OPENCL ERROR : CL_IMAGE_FORMAT_MISMATCH"; break;
		case -10: errorMessage << "OPENCL ERROR : CL_IMAGE_FORMAT_NOT_SUPPORTED"; break;
		case -11: errorMessage << "OPENCL ERROR : CL_BUILD_PROGRAM_FAILURE"; break;
		case -12: errorMessage << "OPENCL ERROR : CL_MAP_FAILURE"; break;
		case -13: errorMessage << "OPENCL ERROR : CL_MISALIGNED_SUB_BUFFER_OFFSET"; break;
		case -14: errorMessage << "OPENCL ERROR : CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST"; break;
		case -30: errorMessage << "OPENCL ERROR : CL_INVALID_VALUE"; break;
		case -31: errorMessage << "OPENCL ERROR : CL_INVALID_DEVICE_TYPE"; break;
		case -32: errorMessage << "OPENCL ERROR : CL_INVALID_PLATFORM"; break;
		case -33: errorMessage << "OPENCL ERROR : CL_INVALID_DEVICE"; break;
		case -34: errorMessage << "OPENCL ERROR : CL_INVALID_CONTEXT"; break;
		case -35: errorMessage << "OPENCL ERROR : CL_INVALID_QUEUE_PROPERTIES"; break;
		case -36: errorMessage << "OPENCL ERROR : CL_INVALID_COMMAND_QUEUE"; break;
		case -37: errorMessage << "OPENCL ERROR : CL_INVALID_HOST_PTR"; break;
		case -38: errorMessage << "OPENCL ERROR : CL_INVALID_MEM_OBJECT"; break;
		case -39: errorMessage << "OPENCL ERROR : CL_INVALID_IMAGE_FORMAT_DESCRIPTOR"; break;
		case -40: errorMessage << "OPENCL ERROR : CL_INVALID_IMAGE_SIZE"; break;
		case -41: errorMessage << "OPENCL ERROR : CL_INVALID_SAMPLER"; break;
		case -42: errorMessage << "OPENCL ERROR : CL_INVALID_BINARY"; break;
		case -43: errorMessage << "OPENCL ERROR : CL_INVALID_BUILD_OPTIONS"; break;
		case -44: errorMessage << "OPENCL ERROR : CL_INVALID_PROGRAM"; break;
		case -45: errorMessage << "OPENCL ERROR : CL_INVALID_PROGRAM_EXECUTABLE"; break;
		case -46: errorMessage << "OPENCL ERROR : CL_INVALID_KERNEL_NAME"; break;
		case -47: errorMessage << "OPENCL ERROR : CL_INVALID_KERNEL_DEFINITION"; break;
		case -48: errorMessage << "OPENCL ERROR : CL_INVALID_KERNEL"; break;
		case -49: errorMessage << "OPENCL ERROR : CL_INVALID_ARG_INDEX"; break;
		case -50: errorMessage << "OPENCL ERROR : CL_INVALID_ARG_VALUE"; break;
		case -51: errorMessage << "OPENCL ERROR : CL_INVALID_ARG_SIZE"; break;
		case -52: errorMessage << "OPENCL ERROR : CL_INVALID_KERNEL_ARGS"; break;
		case -53: errorMessage << "OPENCL ERROR : CL_INVALID_WORK_DIMENSION"; break;
		case -54: errorMessage << "OPENCL ERROR : CL_INVALID_WORK_GROUP_SIZE"; break;
		case -55: errorMessage << "OPENCL ERROR : CL_INVALID_WORK_ITEM_SIZE"; break;
		case -56: errorMessage << "OPENCL ERROR : CL_INVALID_GLOBAL_OFFSET"; break;
		case -57: errorMessage << "OPENCL ERROR : CL_INVALID_EVENT_WAIT_LIST"; break;
		case -58: errorMessage << "OPENCL ERROR : CL_INVALID_EVENT"; break;
		case -59: errorMessage << "OPENCL ERROR : CL_INVALID_OPERATION"; break;
		case -60: errorMessage << "OPENCL ERROR : CL_INVALID_GL_OBJECT"; break;
		case -61: errorMessage << "OPENCL ERROR : CL_INVALID_BUFFER_SIZE"; break;
		case -62: errorMessage << "OPENCL ERROR : CL_INVALID_MIP_LEVEL"; break;
		case -63: errorMessage << "OPENCL ERROR : CL_INVALID_GLOBAL_WORK_SIZE"; break;
		}

		errorMessage << ENDL;
		errorMessage << "//////////////////////////////////////////////////////////////////////////" << ENDL;
		errorMessage << ENDL;


		if(errCode == -11) // Build fail
		{
			size_t log_size;
			clGetProgramBuildInfo(opencl__program, opencl__device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
			char *log = (char *) malloc(log_size);
			clGetProgramBuildInfo(opencl__program, opencl__device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
			errorMessage << log << ENDL;

			errorMessage << ENDL;
			errorMessage << "//////////////////////////////////////////////////////////////////////////" << ENDL;

		}

		throw(std::runtime_error(errorMessage.str()));
	}



}