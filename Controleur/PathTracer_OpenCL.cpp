

#include "PathTracer_OpenCL.h"
#include <ctime>
#include <vector>


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
	cl_mem				 kernel__textures3DData			= NULL;
	cl_mem				 kernel__sun					= NULL;
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
		RGBAColor	**global__imageColor,
		uint		**global__imageRayNb,
		bool		(*UpdateWindowFunc)(void)
		)
	{
		cl_int errCode = 0;

		opencl__imageColorFromDevice = new RGBAColor[global__imageSize];
		opencl__imageRayNbFromDevice = new uint[global__imageSize];

		const size_t constGlobalWorkSize[2] = { global__imageWidth , global__imageHeight };
		const size_t constLocalWorkSize[2]  = { 1, 1 };

		cl_uint imageId = 0;

		while(true)
		{
			CONSOLE << "Image : " << imageId << ENDL;

			/////////////////////////////////////////////////////////////////////////////////
			//////////////////// 1 -  MAIN KERNEL  //////////////////////////////////////////
			/////////////////////////////////////////////////////////////////////////////////

			errCode = clSetKernelArg(opencl__Kernel_Main, 0, sizeof(cl_uint), (void*) &imageId);
			if(OpenCL_ErrorHandling(errCode)) return false;

			errCode = clEnqueueNDRangeKernel(opencl__queue, opencl__Kernel_Main, 1, NULL, constGlobalWorkSize, constLocalWorkSize, 0, NULL, NULL);
			if(OpenCL_ErrorHandling(errCode)) return false;
			clFinish(opencl__queue);

			/////////////////////////////////////////////////////////////////////////////////
			////////////// 3 - RECUPERERATION DES DONNEES  //////////////////////////////////
			/////////////////////////////////////////////////////////////////////////////////
			errCode = clEnqueueReadBuffer(opencl__queue, kernel__imageColor,	CL_TRUE, 0, sizeof(RGBAColor)	* global__imageSize	, (void *) opencl__imageColorFromDevice, 1, NULL , NULL); if(OpenCL_ErrorHandling(errCode)) return false;
			errCode = clEnqueueReadBuffer(opencl__queue, kernel__imageRayNb,	CL_TRUE, 0, sizeof(uint)		* global__imageSize	, (void *) opencl__imageRayNbFromDevice, 0, NULL , NULL); if(OpenCL_ErrorHandling(errCode)) return false;
			clFinish(opencl__queue);

			/////////////////////////////////////////////////////////////////////////////////
			/////////////// 4 - TRAITEMENT DES DONNEES ET AFFICHAGE  ////////////////////////
			/////////////////////////////////////////////////////////////////////////////////
			int offset = 0;
			for(uint y = 0; y < global__imageHeight; y++)
			{
				for(uint x = 0; x < global__imageWidth; x++)
				{
					global__imageColor[x][y] = opencl__imageColorFromDevice[offset];
					//global__imageRayNb[x][y] = opencl__imageRayNbFromDevice[offset];
					global__imageRayNb[x][y] = imageId+1;
					offset++;
				}
			}
			(*UpdateWindowFunc)();

			imageId++;

			CONSOLE << ENDL << ENDL;
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
		errCode = clReleaseMemObject(kernel__textures3DData);	if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clReleaseMemObject(kernel__sun);				if(OpenCL_ErrorHandling(errCode)) return false;
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
		Float4		const	global__cameraScreenX		,
		Float4		const	global__cameraScreenY		,
		Float4		const	global__cameraPosition		,

		Node		const	*global__bvh				,
		Triangle	const	*global__triangulation		,
		Light		const	*global__lights				,
		Material	const	*global__materiaux			,
		Texture		const	*global__textures			,
		Uchar4		const	*global__textureData		,
		uint		const	 global__bvhSize			,
		uint		const	 global__triangulationSize	,
		uint		const	 global__lightsSize			,
		uint		const	 global__materiauxSize		,
		uint		const	 global__texturesSize		,
		uint		const	 global__textureDataSize	,

		uint		const	 global__imageWidth			,
		uint		const	 global__imageHeight		,
		uint		const	 global__imageSize			,
		RGBAColor			**global__imageColor		,
		uint				**global__imageRayNb		,
		uint		const	 global__bvhMaxDepth		,

		SunLight	const	*global__sun				,
		Sky			const	*global__sky
		)
	{

		/////////////////////////////////////////////////////////////////////////////////
		///////////  1 - ALLOCATION ET COPIE  ///////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////

		cl_int errCode = 0;
		cl_uint i = 0;

		kernel__imageColor		= clCreateBuffer(opencl__context, CL_MEM_READ_WRITE                        , sizeof(RGBAColor)    * global__imageSize			    , NULL                            , &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__imageRayNb		= clCreateBuffer(opencl__context, CL_MEM_READ_WRITE                        , sizeof(cl_uint)      * global__imageSize			    , NULL                            , &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__bvh				= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , max(sizeof(Node)	  * global__bvhSize             ,1u), (void *) global__bvh            , &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__triangulation	= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , max(sizeof(Triangle) * global__triangulationSize   ,1u), (void *) global__triangulation  , &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__lights			= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , max(sizeof(Light)	  * global__lightsSize          ,1u), (void *) global__lights         , &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__materiaux		= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , max(sizeof(Material) * global__materiauxSize		,1u), (void *) global__materiaux      , &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__textures		= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , max(sizeof(Texture)  * global__texturesSize		,1u), (void *) global__textures       , &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__sun				= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , sizeof(SunLight)                                       , (void *) global__sun            , &errCode); if(OpenCL_ErrorHandling(errCode)) return false;
		kernel__sky				= clCreateBuffer(opencl__context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR , sizeof(Sky)                                            , (void *) global__sky            , &errCode); if(OpenCL_ErrorHandling(errCode)) return false;


		/////////////////////////////////////////////////////////////////////////////////
		///////////  1 - ARGUEMENTS POUR LES NOYAUX  ////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////

		// ARGUMENTS POUR global__kernelSortedMain
		i = 1;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(cl_uint),	(void*) &global__imageWidth);			if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(cl_uint),	(void*) &global__imageHeight);			if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(cl_uint),	(void*) &global__lightsSize);			if(OpenCL_ErrorHandling(errCode)) return false;

		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__imageColor),		(void*) &kernel__imageColor);		if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__imageRayNb),		(void*) &kernel__imageRayNb);		if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__bvh),				(void*) &kernel__bvh);				if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__triangulation),	(void*) &kernel__triangulation);	if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__lights),			(void*) &kernel__lights);			if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__materiaux),		(void*) &kernel__materiaux);		if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__textures),		(void*) &kernel__textures);			if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__textures3DData),	(void*) &kernel__textures3DData);	if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__sun),				(void*) &kernel__sun);				if(OpenCL_ErrorHandling(errCode)) return false;
		errCode = clSetKernelArg(opencl__Kernel_Main, i++, sizeof(kernel__sky),				(void*) &kernel__sky);				if(OpenCL_ErrorHandling(errCode)) return false;

		return true;
	}


	bool OpenCL_InitializeContext()
	{
		const cl_uint nbProgramFiles = 1;
		char *kernelFileName[nbProgramFiles] = {
			PATHTRACER_FOLDER"Kernel\\PathTracer_FullKernel.cl"
		};

		const char *Kernel_Main									= "Kernel_Main";
		const char *Kernel_CreateRays							= "Kernel_CreateRays";
		const char *Kernel_SortRays_1_ComputeHashValues_Part1	= "Kernel_SortRays_1_ComputeHashValues_Part1";
		const char *Kernel_SortRays_2_ComputeHashValues_Part2	= "Kernel_SortRays_2_ComputeHashValues_Part2";
		const char *Kernel_SortRays_3_PrefixSum					= "Kernel_SortRays_3_PrefixSum";
		const char *Kernel_SortRays_4_AdditionBlockOffset		= "Kernel_SortRays_4_AdditionBlockOffset";
		const char *Kernel_SortRays_5_Compress					= "Kernel_SortRays_5_Compress";
		const char *Kernel_SortRays_6_ChunkSize					= "Kernel_SortRays_6_ChunkSize";
		const char *Kernel_SortRays_7_ComputeChunk16BaseInfo	= "Kernel_SortRays_7_ComputeChunk16BaseInfo";
		const char *Kernel_SortRays_9_AddComputedOffsetAndSort	= "Kernel_SortRays_9_AddComputedOffsetAndSort";
		const char *Kernel_SortRays_0_DebugHashValues			= "Kernel_SortRays_0_DebugHashValues";
		const char *Kernel_SortRays_0_DebugRadixSort			= "Kernel_SortRays_0_DebugRadixSort";
		const char *Kernel_CustomDebug							= "Kernel_CustomDebug";

		cl_platform_id *platform_ids;
		cl_platform_id platform_id = 0;
		cl_device_id *device_ids;

		cl_uint ret_num_devices;
		cl_uint ret_num_platforms;

		cl_int errCode = 0;

		FILE *fp = NULL;
		char *source_str[nbProgramFiles];
		size_t source_size[nbProgramFiles];

		/* Load the sources code containing the kernel*/
		for(int i=0; i < nbProgramFiles; i++)
		{
			fopen_s(&fp, kernelFileName[i], "r");
			if (!fp)
				return 1;

			source_str[i] = (char*)malloc(0x100000);
			source_size[i] = fread(source_str[i], 1, 0x100000, fp);
			fclose(fp);
		}

		/************************************************************************/
		/*                            PLATFORM                                  */
		/************************************************************************/
		CONSOLE << "Choix de la plateforme : " << ENDL << ENDL;
		errCode = clGetPlatformIDs(0, NULL, &ret_num_platforms);			if(OpenCL_ErrorHandling(errCode)) return false;
		platform_ids = new cl_platform_id[ret_num_platforms];
		errCode = clGetPlatformIDs(ret_num_platforms, platform_ids, NULL);	if(OpenCL_ErrorHandling(errCode)) return false;

		if(ret_num_platforms == 0)
			return 2;

		char infoString[128];
		uint platformIdx;
		for(platformIdx = 0; platformIdx < ret_num_platforms; platformIdx++)
		{
			CONSOLE << "\tPlatform " << (platformIdx+1) << " / " << ret_num_platforms << " : " << ENDL;
			platform_id = platform_ids[platformIdx];

			errCode = clGetPlatformInfo (platform_id, CL_PLATFORM_NAME, sizeof(infoString), infoString, NULL);		if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\tName : " << infoString << ENDL;

			errCode = clGetPlatformInfo (platform_id, CL_PLATFORM_VENDOR, sizeof(infoString), infoString, NULL);	if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\tVendor : " << infoString << ENDL;

			errCode = clGetPlatformInfo (platform_id, CL_PLATFORM_VERSION, sizeof(infoString), infoString, NULL);	if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\tVersion : " << infoString << ENDL;
		}
		CONSOLE << ENDL;

		if(ret_num_platforms > 1)
		{
			CONSOLE << "Quelle plateforme desirez-vous ? ";

			//std::cin >> platformIdx;
			platformIdx = 1;

			platformIdx = max( 1 , (int) min( platformIdx, (int) ret_num_platforms ) );
			platformIdx -= 1;

			platform_id = platform_ids[platformIdx];
			errCode = clGetPlatformInfo (platform_id, CL_PLATFORM_NAME, sizeof(infoString), infoString, NULL);		if(OpenCL_ErrorHandling(errCode)) return false;

			CONSOLE << "Vous avez choisi la plateforme " << infoString << ENDL;

			CONSOLE << ENDL;
		}


		/************************************************************************/
		/*                            DEVICE                                    */
		/************************************************************************/

		CONSOLE << "Choix du device : " << ENDL << ENDL;

		errCode = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL, 0, NULL, &ret_num_devices);		if(OpenCL_ErrorHandling(errCode)) return false;

		device_ids = new cl_device_id[ret_num_devices];

		errCode = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL, ret_num_devices, device_ids, &ret_num_devices); if(OpenCL_ErrorHandling(errCode)) return false;

		uint deviceIdx;
		for(deviceIdx=0; deviceIdx < ret_num_devices; deviceIdx++)
		{
			CONSOLE << "\tDevice : " << (deviceIdx+1) << " / " << ret_num_devices << ENDL;
			opencl__device = device_ids[deviceIdx];

			size_t param_size_t;
			size_t param_size_t_3[3];
			cl_ulong param_cl_ulong;
			cl_bool param_cl_bool;
			cl_device_type deviceType;

			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_NAME,						sizeof(infoString), infoString, NULL);				if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\t" << "Name : "												<< infoString << ENDL;

			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_TYPE,						sizeof(deviceType), &deviceType, NULL);				if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\t" << "CL_DEVICE_TYPE : "									<< ((deviceType == CL_DEVICE_TYPE_CPU) ? "CL_DEVICE_TYPE_CPU " : "") << ((deviceType == CL_DEVICE_TYPE_GPU) ? "CL_DEVICE_TYPE_GPU " : "") << ((deviceType == CL_DEVICE_TYPE_ACCELERATOR) ? "CL_DEVICE_TYPE_ACCELERATOR " : "") << ((deviceType == CL_DEVICE_TYPE_DEFAULT) ? "CL_DEVICE_TYPE_DEFAULT " : "") << ENDL;

			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_MAX_COMPUTE_UNITS,			sizeof(param_size_t), &param_size_t, NULL);			if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\t" << "CL_DEVICE_MAX_COMPUTE_UNITS : "						<< param_size_t << ENDL;

			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,	sizeof(param_size_t), &param_size_t, NULL);			if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\t" << "CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS : "				<< param_size_t << ENDL;

			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_MAX_WORK_GROUP_SIZE,		sizeof(param_size_t), &param_size_t, NULL);			if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\t" << "DEVICE_MAX_WORK_GROUP_SIZE : "						<< param_size_t << ENDL;

			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_MAX_WORK_ITEM_SIZES,		sizeof(param_size_t_3), param_size_t_3, NULL);		if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\t" << "CL_DEVICE_MAX_WORK_ITEM_SIZES : "						<< param_size_t_3[0] << " , " << param_size_t_3[1] << " , " << param_size_t_3[2] << ENDL;

			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_MAX_MEM_ALLOC_SIZE,			sizeof(param_cl_ulong), &param_cl_ulong, NULL);		if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\t" << "CL_DEVICE_MAX_MEM_ALLOC_SIZE : "						<< param_cl_ulong << ENDL;

			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE,	sizeof(param_size_t), &param_size_t, NULL);			if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\t" << "CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE : "				<< param_size_t << ENDL;

			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_GLOBAL_MEM_SIZE,			sizeof(param_cl_ulong), &param_cl_ulong, NULL);		if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\t" << "CL_DEVICE_GLOBAL_MEM_SIZE : "							<< param_cl_ulong << ENDL;

			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,	sizeof(param_cl_ulong), &param_cl_ulong, NULL);		if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\t" << "CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE : "				<< param_cl_ulong << ENDL;			

			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_LOCAL_MEM_SIZE,				sizeof(param_cl_ulong), &param_cl_ulong, NULL);		if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\t" << "CL_DEVICE_LOCAL_MEM_SIZE : "							<< param_cl_ulong << ENDL;

			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_IMAGE_SUPPORT,				sizeof(param_cl_bool), &param_cl_bool, NULL);		if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\t" << "CL_DEVICE_IMAGE_SUPPORT : "							<< param_cl_bool << ENDL;

			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_IMAGE2D_MAX_WIDTH,			sizeof(param_size_t), &param_size_t, NULL);			if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\t" << "CL_DEVICE_IMAGE2D_MAX_WIDTH : "						<< param_size_t << ENDL;

			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_IMAGE2D_MAX_HEIGHT,			sizeof(param_size_t), &param_size_t, NULL);			if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "\t\t" << "CL_DEVICE_IMAGE2D_MAX_HEIGHT : "						<< param_size_t << ENDL;

		}
		CONSOLE << ENDL;

		if(ret_num_devices > 1)
		{
			CONSOLE << "Quel device desirez-vous ? ";

			//std::cin >> deviceIdx;
			deviceIdx = 1;

			deviceIdx = max( 1 , (int) min( deviceIdx, (int) ret_num_devices ) );
			deviceIdx -= 1;

			opencl__device = device_ids[deviceIdx];
			errCode = clGetDeviceInfo (opencl__device, CL_DEVICE_NAME, sizeof(infoString), infoString, NULL);	if(OpenCL_ErrorHandling(errCode)) return false;
			CONSOLE << "Vous avez choisi le device " << infoString << ENDL;
			CONSOLE << ENDL;
		}

		/* Create OpenCL context */
		opencl__context = clCreateContext(NULL, 1, &opencl__device, NULL, NULL, &errCode);	if(OpenCL_ErrorHandling(errCode)) return false;


		/* Create Command Queue */
		opencl__queue = clCreateCommandQueue(opencl__context, opencl__device, 0, &errCode);	if(OpenCL_ErrorHandling(errCode)) return false;


		/* Create Kernel Program from the source */
		opencl__program = clCreateProgramWithSource(opencl__context, nbProgramFiles, (const char **) source_str, source_size, &errCode);	if(OpenCL_ErrorHandling(errCode)) return false;


		/* Build Kernel Program */

		//	1 - Pour le debugger d'intel :
		//char const * buildOptions = "-I "PATHTRACER_FOLDER"Kernel\\ -g -s C:\\Users\\Alexandre\\Documents\\Visual_Studio_2010\\Projects\\PathTracer\\PathTracer\\src\\Kernel\\PathTracer_FullKernel.cl";
		//	2 - Pour des performnace normalement accrue mais moins de précision
		//char const * buildOptions = "-cl-mad-enable -I "PATHTRACER_FOLDER"Kernel\\";
		//	3 - Normal
		char const * buildOptions = "-I \""PATHTRACER_FOLDER"Kernel\\\"";

		errCode = clBuildProgram(opencl__program, 0, NULL, buildOptions , NULL, NULL); if(OpenCL_ErrorHandling(errCode)) return false;

		/* Create OpenCL Kernel */
		opencl__Kernel_Main = clCreateKernel(opencl__program, Kernel_Main, &errCode); if(OpenCL_ErrorHandling(errCode)) return false;

		// Release pointers
		for(int i=0; i < nbProgramFiles; i++)
			free(source_str[i]);

		return true;
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