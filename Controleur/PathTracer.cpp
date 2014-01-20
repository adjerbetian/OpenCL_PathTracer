
#include "PathTracer.h"
#include "PathTracer_FileImporter.h"
#include "PathTracer_OpenCL.h"

#include <ctime>
#include <sstream>

mstream console;

namespace PathTracerNS
{
	GlobalVars globalVars;

	////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////   FONCTIONS    ///////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////

	void PathTracer_SetImporter(PathTracerImporter* importer)
	{
		globalVars.importer = importer;
	}


	bool PathTracer_Main(uint image_width, uint image_height, uint numImagesToRender, bool saveRenderedImages, bool loadSky, bool exportScene, Sampler sampler, uint rayMaxDepth, bool printLogInfos, bool superSampling)
	{
		try
		{
			CONSOLE_LOG << ENDL;
			CONSOLE_LOG << "//////////////////////////////////////////////////////////////////////////" << ENDL;
			CONSOLE_LOG << "                             PATH TRACER" << ENDL;
			CONSOLE_LOG << "//////////////////////////////////////////////////////////////////////////" << ENDL;
			CONSOLE_LOG << ENDL;

			clock_t start;

			double loadingTime;
			double bvhBuildingTime;
			double openclSettingTime;
			double pathTracingTime;
			double memoryTime;
			double displayTime;

			bool noError = true;

			PathTracer_PrintSection("LOADING MAYA SCENE"); start = clock();
			PathTracer_Initialize(image_width, image_height, saveRenderedImages, loadSky, rayMaxDepth, printLogInfos, superSampling);
			loadingTime = clock()-start;

			PathTracer_PrintSection("BUILDING BVH"); start = clock();
			BVH_Create(globalVars);
			bvhBuildingTime = clock()-start;

			if(globalVars.bvhMaxDepth >= BVH_MAX_DEPTH)
			{
				std::ostringstream errorMessage;
				errorMessage << "BVH is too deep : it has a size of " << globalVars.bvhMaxDepth << " and should be under " << BVH_MAX_DEPTH;
				throw std::runtime_error(errorMessage.str());
			}
			if(globalVars.lightsSize >= MAX_LIGHT_SIZE)
			{
				std::ostringstream errorMessage;
				errorMessage << "The scene has too many lights : it has " << globalVars.lightsSize << " and should be under " << MAX_LIGHT_SIZE;
				throw std::runtime_error(errorMessage.str());
			}

			if(exportScene)
			{
				PathTracer_PrintSection("EXPORTING SCENE");
				PathTracer_Export();
			}

			PathTracer_PrintSection("SETTING OPENCL CONTEXT"); start = clock();
			OpenCL_SetupContext(globalVars, sampler);

			OpenCL_InitializeMemory	(globalVars);

			openclSettingTime = clock()-start;


			PathTracer_PrintSection("PATH TRACING");
			OpenCL_RunKernel(
				globalVars					,
				&PathTracer_UpdateWindow	,
				numImagesToRender			,
				&pathTracingTime			,
				&memoryTime					,
				&displayTime
				);

			pathTracingTime	 = clock()-start;

			PathTracer_PrintSection("STATISTICS");
			PathTracer_ComputeStatistics(numImagesToRender, loadingTime, bvhBuildingTime, openclSettingTime, pathTracingTime, memoryTime, displayTime);

			PathTracer_Clear();

		}
		catch(std::exception const& e)
		{
			CONSOLE_LOG << "ERROR : " << e.what() << ENDL;
			try	{ PathTracer_Clear();} catch(std::exception const& e2)
			{
				CONSOLE_LOG << "ERROR 2 : " << e2.what() << ENDL;
			}
			return false;
		}

		return true;
	}

	/*	Gestion des différents initialiseurs
	*/

	void PathTracer_Initialize(uint image_width, uint image_height, bool saveRenderedImages, bool loadSky, uint rayMaxDepth, bool printLogInfos, bool superSampling)
	{
		globalVars.printLogInfos = printLogInfos;
		globalVars.superSampling = superSampling;
		globalVars.importer->Initialize(globalVars);

		globalVars.importer->Import(image_width, image_height, loadSky);
		globalVars.rayMaxDepth = rayMaxDepth;

		PathTracer_InitializeImage();
		PathTracer_InitializeWindow(saveRenderedImages);

		PathTracer_PaintLoadingScreen();
	}

	/*	Crée un écran d'attente pour patientez pendant le traitement de la scène
	 *	jusqu'à l'arrivée de la première image de rendu
	 */

	void PathTracer_PaintLoadingScreen()
	{
		globalVars.window->TestPaintWindow();
	}


	/*	Initialise les double pointeurs des images 2D.
	 */

	void PathTracer_InitializeImage()
	{
		// Les données OpenCL sont prévues pour une image de taille 1280 x 720

		if(!(globalVars.imageWidth > 0 && globalVars.imageHeight > 0))
		{
			std::ostringstream errorStream;
			errorStream << "Bad image dimension : width x height : ";
			errorStream << globalVars.imageWidth << " x " << globalVars.imageHeight ;
			throw std::runtime_error(errorStream.str());
		}

		globalVars.imageColor			= new RGBAColor[globalVars.imageWidth*globalVars.imageHeight];
		globalVars.imageRayNb			= new float[globalVars.imageWidth*globalVars.imageHeight];
		globalVars.rayDepths			= new uint[globalVars.rayMaxDepth+1];
		globalVars.rayIntersectedBBx	= new uint[MAX_INTERSETCION_NUMBER];
		globalVars.rayIntersectedTri	= new uint[MAX_INTERSETCION_NUMBER];

		for(uint x=0; x<globalVars.rayMaxDepth+1; x++)
		{
			globalVars.rayDepths[x] = 0;
		}
		for(uint x=0; x<MAX_INTERSETCION_NUMBER; x++)
		{
			globalVars.rayIntersectedBBx[x] = 0;
			globalVars.rayIntersectedTri[x] = 0;
		}

		for(uint x=0; x<globalVars.imageSize; x++)
		{
			globalVars.imageColor[x] = RGBAColor(0,0,0,0);
			globalVars.imageRayNb[x] = 0;
		}
	}

	/*	Création de la fenêtre d'affichage
	 */

	void PathTracer_InitializeWindow(bool saveRenderedImages)
	{
		// On cree la fenêtre

		globalVars.window = new PathTracerDialog();
		globalVars.window->SetWidth(globalVars.imageWidth);
		globalVars.window->SetHeight(globalVars.imageHeight);
		globalVars.window->Create();


		//On affiche quelques informations
		globalVars.window->saveRenderedImages = saveRenderedImages;
		globalVars.window->SetNumberOfTriangles( globalVars.triangulationSize );
		globalVars.window->SetMaxDepthOfBVH(globalVars.bvhMaxDepth);
		globalVars.window->SetSizeOfBVH(globalVars.bvhSize);
	}


	/*	Fonction d'exportation de la scène pour un chargement postérieur plus rapide
	 */

	void PathTracer_Export()
	{
		PathTracerFileImporter fileExporter;
		fileExporter.Initialize(globalVars);
		fileExporter.Export();
	}

	/*	Suppression de tous les pointeurs crées
	 */

	void PathTracer_Clear()
	{
		delete globalVars.importer;

		delete[] globalVars.imageColor;
		delete[] globalVars.imageRayNb;
		delete[] globalVars.rayDepths;
		delete[] globalVars.rayIntersectedBBx;
		delete[] globalVars.rayIntersectedTri;

		delete[] globalVars.bvh;
		delete[] globalVars.triangulation;
		delete[] globalVars.lights;
		delete[] globalVars.materiaux;
		delete[] globalVars.textures;
		delete[] globalVars.texturesData;
	}

	void PathTracer_ComputeStatistics(uint numImageToRender, double loadingTime, double bvhBuildingTime, double openclSettingTime, double pathTracingTime, double memoryTime, double displayTime )
	{
		unsigned long int numberOfShotRays = 0;
		unsigned long int numberOfProcessedRays = 0;
		unsigned long int numberOfTestedBBx = 0;
		unsigned long int numberOfTestedTri = 0;
		unsigned long int numberOfIntersectedTri = 0;
		
		for(uint i=0; i<globalVars.rayMaxDepth; i++)
		{
			numberOfShotRays += globalVars.rayDepths[i];
			numberOfProcessedRays += (1 + globalVars.lightsSize) * (i+1) * globalVars.rayDepths[i];
			numberOfIntersectedTri += (1 + globalVars.lightsSize) * i * globalVars.rayDepths[i];
		}

		for(uint i=0; i<MAX_INTERSETCION_NUMBER; i++)
		{
			numberOfTestedBBx += i * globalVars.rayIntersectedBBx[i];
			numberOfTestedTri += i * globalVars.rayIntersectedTri[i];
		}


		CONSOLE_LOG << "Scene information" << ENDL;
		CONSOLE_LOG << "\t" << "image size          : " << globalVars.imageWidth << " x " << globalVars.imageHeight << ENDL;
		CONSOLE_LOG << "\t" << "number of triangles : " << globalVars.triangulationSize << ENDL;
		CONSOLE_LOG << "\t" << "number of lights    : " << globalVars.lightsSize << ENDL;
		CONSOLE_LOG << "\t" << "size of the bvh     : " << globalVars.bvhSize << ENDL;
		CONSOLE_LOG << "\t" << "depth of bvh        : " << globalVars.bvhMaxDepth << ENDL;

		CONSOLE_LOG << ENDL;
		
		CONSOLE_LOG << "Path tracing information" << ENDL;
		CONSOLE_LOG << "\t" << "Number of shot rays                    : " << numberOfShotRays << ENDL;
		CONSOLE_LOG << "\t" << "Number of processed rays               : " << numberOfProcessedRays << ENDL;
		CONSOLE_LOG << "\t" << "Average number of tested BBx per ray   : " << ((float) numberOfTestedBBx) / ((float) numberOfProcessedRays) << ENDL;
		CONSOLE_LOG << "\t" << "Average number of tested Tri per ray   : " << ((float) numberOfTestedTri) / ((float) numberOfProcessedRays) << ENDL;
		CONSOLE_LOG << "\t" << "Percentage of successful intersection : " << ((float) numberOfIntersectedTri * 100.) / ((float) numberOfTestedTri) << " % " << ENDL;
		CONSOLE_LOG << "\t" << "Depth histogram in %" << ENDL;
		for(uint i=0; i<globalVars.rayMaxDepth; i++)
			CONSOLE_LOG << "\t\t" << i << " : " << globalVars.rayDepths[i] * 100.0 / numberOfShotRays << " %" << ENDL;

		CONSOLE_LOG << ENDL;

		CONSOLE_LOG << "Time information" << ENDL;
		CONSOLE_LOG << "\t" << "Loading time        : " << loadingTime       << " ms" << ENDL;
		CONSOLE_LOG << "\t" << "BVH building time   : " << bvhBuildingTime   << " ms" << ENDL;
		CONSOLE_LOG << "\t" << "OpenCL setting time : " << openclSettingTime << " ms" << ENDL;
		CONSOLE_LOG << "\t" << "Path Tracing time   : " << pathTracingTime   << " ms" << ENDL;
		CONSOLE_LOG << "\t" << "Memory time         : " << memoryTime        << " ms" << ENDL;
		CONSOLE_LOG << "\t" << "Display time        : " << displayTime       << " ms" << ENDL;

		CONSOLE_LOG << ENDL;

		CONSOLE_LOG << "Time statistics" << ENDL;
		CONSOLE_LOG << "\t" << "Average time per picture                  : " << pathTracingTime / numImageToRender      << " ms" << ENDL;
		CONSOLE_LOG << "\t" << "Average time per 1000 full rays           : " << 1000. * pathTracingTime / numberOfShotRays      << " ms" << ENDL;
		CONSOLE_LOG << "\t" << "Average time per 1000 (ray + shadow rays) : " << 1000. * pathTracingTime / numberOfProcessedRays << " ms" << ENDL;

		CONSOLE_LOG << ENDL;

	}


	/*	Redirection de la fonction Update
	 */

	bool PathTracer_UpdateWindow()
	{
		return globalVars.window->PaintWindow(globalVars.imageColor, globalVars.imageRayNb);
	}



	//////////////////////////////////////////////////////////////////////////
	// Fonctions d'impression
	//////////////////////////////////////////////////////////////////////////

	void PathTracer_PrintSection(char const* section)
	{
		CONSOLE_LOG << ENDL << ENDL;
		CONSOLE_LOG << "*** " << section << " ";
		for(int i=0; i<69 - strlen(section); i++)
			CONSOLE_LOG << "*";
		CONSOLE_LOG << ENDL;
		CONSOLE_LOG << ENDL;
	}

	void Node_Print( Node const *This, uint n)
	{
		LOG << " ";
		for(uint i = 0; i < n; i++ )
			LOG << " | ";
		LOG << (This->isLeaf ? " F" : " N") << n << " : ";
		LOG << This->triangleStartIndex << " , " << This->nbTriangles << " CA : " << This->cutAxis << " ";
		LOG << (This->isLeaf ? (This->comments == NODE_BAD_SAH ? "Bad SAH" : (This->comments == NODE_LEAF_MAX_SIZE ? "Leaf max size" : (This->comments == NODE_LEAF_MIN_DIAG ? "Leaf min diag" : "Unknown"))) : "");
		LOG << ENDL;

		if(!This->isLeaf)
		{
			Node_Print(&globalVars.bvh[This->son1Id], n+1);
			Node_Print(&globalVars.bvh[This->son2Id], n+1);
		}
	}

	void PathTracer_PrintBVH()
	{
		LOG << "BVH : " << ENDL << ENDL;
		Node_Print( globalVars.bvh, 0 );
		LOG << ENDL;
	}

	void PathTracer_PrintBVHCharacteristics()
	{
		uint BVHMaxLeafSize	=	0;
		uint BVHMinLeafSize	=	INT_MAX;
		uint BVHMaxDepth	=	0;
		uint BVHMinDepth	=	INT_MAX;
		uint nNodes			=	0;
		uint nLeafs			=	0;
		std::string BVHMaxLeafSizeComments;

		BVH_GetCharacteristics(globalVars.bvh, 0, 0, BVHMaxLeafSize, BVHMinLeafSize, BVHMaxDepth, BVHMinDepth, nNodes, nLeafs, BVHMaxLeafSizeComments);

		CONSOLE_LOG << "\tCaracteristiques du BVH : " << ENDL;
		CONSOLE_LOG << "\t\tBVHMaxLeafSize : "	<< BVHMaxLeafSize	<< " --> " << BVHMaxLeafSizeComments << ENDL;
		CONSOLE_LOG << "\t\tBVHMinLeafSize : "	<< BVHMinLeafSize	<< ENDL;
		CONSOLE_LOG << "\t\tBVHMaxDepth : "		<< BVHMaxDepth		<< "  dans le build : " << globalVars.bvhMaxDepth << ENDL;
		CONSOLE_LOG << "\t\tBVHMinDepth : "		<< BVHMinDepth		<< ENDL;
		CONSOLE_LOG << "\t\tnNodes : "			<< nNodes			<< ENDL;
		CONSOLE_LOG << "\t\tnLeafs : "			<< nLeafs			<< ENDL;

		CONSOLE_LOG << ENDL;

	}

	void PathTracer_PrintTriangulation()
	{
		LOG << "TRIANGULATION : " << ENDL;
		for(uint i=0; i<globalVars.triangulationSize; i++)
			Triangle_Print(&globalVars.triangulation[i], i);
		LOG << ENDL;
	}

	void Triangle_Print(Triangle const *This, uint id)
	{
		LOG << "\t" << "Triangle : " << id << ENDL;
		LOG << "\t\t" << This->S1.toString() << ENDL;
		LOG << "\t\t" << This->S2.toString() << ENDL;
		LOG << "\t\t" << This->S3.toString() << ENDL;
	}

}