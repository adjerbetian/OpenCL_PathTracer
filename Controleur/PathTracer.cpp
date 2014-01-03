
#include "PathTracer.h"
#include "PathTracer_FileImporter.h"
#include "PathTracer_OpenCL.h"

#include <ctime>
#include <sstream>

mstream console;

namespace PathTracerNS
{

	////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////   VARIABLES GLOBALES    //////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////

	//	Données d'import et d'affichage, dépendante du projet en Alone ou en LumenRT

	PathTracerDialog	*global__window;				// Pointeur vers une fenêtre qui permet d'affichage du rendu
	PathTracerImporter	*global__importer = NULL;		// Pointeur vers une instance derivant de PathTracerImporter


	//	Données de la scène :

	Float4				 global__cameraDirection		= Float4(0,0,0,0);	// Vecteur de direction principal de la caméra (passant par le milieu de l'image)
	Float4				 global__cameraRight			= Float4(0,0,0,0);	// Vector orthogonal à la direction, canoniquement parrallèle au sol, qui indique la largeur de l'image
	Float4				 global__cameraUp			= Float4(0,0,0,0);	// Vector orthogonal à la direction, canoniquement parrallèle à la vertical, qui indique la hauteur de l'image
	Float4				 global__cameraPosition			= Float4(0,0,0,0);	// Position 3D du point de focal

	Node				*global__bvh					= NULL;		// tableau de Noeuds représentant le BVH. global__bvh[0] est la racine de l'arbre
	Triangle			*global__triangulation			= NULL;		// tableau de Triangles, NON DUPLIQUES, représentant la géométrie de la scène
	Light				*global__lights					= NULL;		// tableau de Light, à l'exclusion du soleil et du ciel
	Material			*global__materiaux				= NULL;		// tableau de tous les Matériaux de la scène
	Texture				*global__textures				= NULL;		// tableau de Textures de la scène à l'exclusion des textures du ciel. Attention, Texture est une classe qui ne comprends ques les données principales des textures, et pas les textures même
	Uchar4				*global__texturesData			= NULL;		// tableau de toutes les textures images de la scène, y compris celles du ciel. Les textures sont stockées de manière continues : pixel_texture1(0,0) ... pixel_texture1(w1,h1) , pixel_texture2(0,0) ... pixel_texture2(w2,h2) , ...

	uint				 global__bvhSize				= 0;		// Nombre de noeuds du BVH
	uint				 global__triangulationSize		= 0;		// Nombre de triangles de la scène
	uint				 global__lightsSize				= 0;		// Nombre de lumières de la scène, à l'exclusion du soleil et du ciel
	uint				 global__materiauxSize			= 0;		// Nombre de matériaux de la scène
	uint				 global__texturesSize			= 0;		// Nombre de textures de la scène
	uint				 global__texturesDataSize		= 0;		// Somme des tailles de toutes les textures : ( largeur1 * hauteur1 ) + ( largeur2 * hauteur2 ) + ...
	uint				 global__bvhMaxDepth			= 0;		// Profondeur de la plus grande branche du BVH

	Sky					 global__sky;	// Ciel


	//	Donnees images, qui viennent du rendu

	uint				 global__imageWidth				= 0;		// Largeur de l'image de rendu
	uint				 global__imageHeight			= 0;		// Hauteur de l'image de rendu
	uint				 global__imageSize				= 0;		// Largeur * Hauteur
	uint				 global__rayMaxDepth			= 10;		// Profondeur max des rayons
	RGBAColor			*global__imageColor				= NULL;		// Somme des couleurs rendues
	float				*global__imageRayNb				= NULL;		// Nombre de rayons ayant contribue aux pixels
	uint				*global__rayDepths				= NULL;		// Number of ray for each depth
	uint				*global__rayIntersectedBBx		= NULL;		// Number of ray for each number of BBx interstection
	uint				*global__rayIntersectedTri		= NULL;		// Number of ray for each number of Tri interstection


	////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////   FONCTIONS    ///////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////

	void PathTracer_SetImporter(PathTracerImporter* importer)
	{
		global__importer = importer;
	}


	bool PathTracer_Main(uint image_width, uint image_height, uint numImagesToRender, bool saveRenderedImages, bool loadSky, bool exportScene, Sampler sampler, uint rayMaxDepth)
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
			double displayTime;

			bool noError = true;

			PathTracer_PrintSection("LOADING MAYA SCENE"); start = clock();
			PathTracer_Initialize(image_width, image_height, saveRenderedImages, loadSky, rayMaxDepth);
			loadingTime = clock()-start;

			PathTracer_PrintSection("BUILDING BVH"); start = clock();
			BVH_Create(global__triangulationSize, global__triangulation, &global__bvhMaxDepth, &global__bvhSize, &global__bvh);
			bvhBuildingTime = clock()-start;

			if(global__bvhMaxDepth < BVH_MAX_DEPTH)
			{
				std::ostringstream errorMessage("BVH is too deep : it has a size of ");
				errorMessage << global__bvhMaxDepth << " and should be under " << BVH_MAX_DEPTH;
				throw std::runtime_error(errorMessage.str());
			}
			if(global__lightsSize < MAX_LIGHT_SIZE)
			{
				std::ostringstream errorMessage("The scene has too many lights : it has ");
				errorMessage << global__lightsSize << " and should be under " << MAX_LIGHT_SIZE;
				throw std::runtime_error(errorMessage.str());
			}

			if(exportScene)
			{
				PathTracer_PrintSection("EXPORTING SCENE");
				PathTracer_Export();
			}

			PathTracer_PrintSection("SETTING OPENCL CONTEXT"); start = clock();
			OpenCL_SetupContext(sampler, global__rayMaxDepth);

			OpenCL_InitializeMemory	(
				global__cameraDirection		,
				global__cameraRight			,
				global__cameraUp			,
				global__cameraPosition		,

				global__bvh					,
				global__triangulation		,
				global__lights				,
				global__materiaux			,
				global__textures			,
				global__texturesData		,
				global__bvhSize				,
				global__triangulationSize	,
				global__lightsSize			,
				global__materiauxSize		,
				global__texturesSize		,
				global__texturesDataSize	,

				global__imageWidth			,
				global__imageHeight			,
				global__imageSize			,
				global__rayMaxDepth			,
				global__imageColor			,
				global__imageRayNb			,
				global__rayDepths			,
				global__rayIntersectedBBx	,
				global__rayIntersectedTri	,
				global__bvhMaxDepth			,

				&global__sky
				);

			openclSettingTime = clock()-start;


			PathTracer_PrintSection("PATH TRACING");
			OpenCL_RunKernel(
				global__imageWidth			,
				global__imageHeight			,
				global__imageSize			,
				global__rayMaxDepth			,
				global__imageColor			,
				global__imageRayNb			,
				global__rayDepths			,
				global__rayIntersectedBBx	,
				global__rayIntersectedTri	,
				&PathTracer_UpdateWindow	,
				numImagesToRender			,
				&pathTracingTime			,
				&displayTime
				);

			pathTracingTime	 = clock()-start;

			PathTracer_PrintSection("STATISTICS");
			PathTracer_ComputeStatistics(numImagesToRender, loadingTime, bvhBuildingTime, openclSettingTime, pathTracingTime, displayTime);

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

	void PathTracer_Initialize(uint image_width, uint image_height, bool saveRenderedImages, bool loadSky, uint rayMaxDepth)
	{
		global__importer->Initialize(
			&global__cameraDirection,
			&global__cameraRight,
			&global__cameraUp,
			&global__cameraPosition,
			&global__triangulation,
			&global__lights,
			&global__materiaux,
			&global__textures,
			&global__texturesData,
			&global__triangulationSize,
			&global__lightsSize,
			&global__materiauxSize,
			&global__texturesSize,
			&global__texturesDataSize,
			&global__imageWidth,
			&global__imageHeight,
			&global__imageSize,
			&global__sky
			);

		global__importer->Import(image_width, image_height, loadSky);
		global__rayMaxDepth = rayMaxDepth;

		PathTracer_InitializeImage();
		PathTracer_InitializeWindow(saveRenderedImages);

		PathTracer_PaintLoadingScreen();
	}

	/*	Crée un écran d'attente pour patientez pendant le traitement de la scène
	 *	jusqu'à l'arrivée de la première image de rendu
	 */

	void PathTracer_PaintLoadingScreen()
	{

		global__window->TestPaintWindow();
	}


	/*	Initialise les double pointeurs des images 2D.
	 */

	void PathTracer_InitializeImage()
	{
		// Les données OpenCL sont prévues pour une image de taille 1280 x 720

		if(global__imageWidth > 0 && global__imageHeight > 0)
		{
			std::ostringstream errorStream("Bad inage dimension : width x height : ");
			errorStream << global__imageWidth << " x " << global__imageHeight ;
			throw std::runtime_error(errorStream.str());
		}

		global__imageColor			= new RGBAColor[global__imageWidth*global__imageHeight];
		global__imageRayNb			= new float[global__imageWidth*global__imageHeight];
		global__rayDepths			= new uint[global__rayMaxDepth+1];
		global__rayIntersectedBBx	= new uint[MAX_INTERSETCION_NUMBER];
		global__rayIntersectedTri	= new uint[MAX_INTERSETCION_NUMBER];

		for(uint x=0; x<global__rayMaxDepth+1; x++)
		{
			global__rayDepths[x] = 0;
		}
		for(uint x=0; x<MAX_INTERSETCION_NUMBER; x++)
		{
			global__rayIntersectedBBx[x] = 0;
			global__rayIntersectedTri[x] = 0;
		}

		for(uint x=0; x<global__imageSize; x++)
		{
			global__imageColor[x] = RGBAColor(0,0,0,0);
			global__imageRayNb[x] = 0;
		}
	}

	/*	Création de la fenêtre d'affichage
	 */

	void PathTracer_InitializeWindow(bool saveRenderedImages)
	{
		// On cree la fenêtre

		global__window = new PathTracerDialog();
		global__window->SetWidth(global__imageWidth);
		global__window->SetHeight(global__imageHeight);
		global__window->Create();


		//On affiche quelques informations
		global__window->saveRenderedImages = saveRenderedImages;
		global__window->SetNumberOfTriangles( global__triangulationSize );
		global__window->SetMaxDepthOfBVH(global__bvhMaxDepth);
		global__window->SetSizeOfBVH(global__bvhSize);
	}


	/*	Fonction d'exportation de la scène pour un chargement postérieur plus rapide
	 */

	void PathTracer_Export()
	{
		PathTracerFileImporter fileExporter;
		fileExporter.Initialize(
			&global__cameraDirection,
			&global__cameraRight,
			&global__cameraUp,
			&global__cameraPosition,
			&global__triangulation,
			&global__lights,
			&global__materiaux,
			&global__textures,
			&global__texturesData,
			&global__triangulationSize,
			&global__lightsSize,
			&global__materiauxSize,
			&global__texturesSize,
			&global__texturesDataSize,
			&global__imageWidth,
			&global__imageHeight,
			&global__imageSize,
			&global__sky
			);
		fileExporter.Export();
	}

	/*	Suppression de tous les pointeurs crées
	 */

	void PathTracer_Clear()
	{
		delete global__importer;

		delete[] global__imageColor;
		delete[] global__imageRayNb;
		delete[] global__rayDepths;
		delete[] global__rayIntersectedBBx;
		delete[] global__rayIntersectedTri;

		delete[] global__bvh;
		delete[] global__triangulation;
		delete[] global__lights;
		delete[] global__materiaux;
		delete[] global__textures;
		delete[] global__texturesData;
	}

	void PathTracer_ComputeStatistics(uint numImageToRender, double loadingTime, double bvhBuildingTime, double openclSettingTime, double pathTracingTime, double displayTime)
	{
		unsigned long int numberOfShotRays = 0;
		unsigned long int numberOfProcessedRays = 0;
		unsigned long int numberOfTestedBBx = 0;
		unsigned long int numberOfTestedTri = 0;
		unsigned long int numberOfIntersectedTri = 0;
		
		for(uint i=0; i<global__rayMaxDepth; i++)
		{
			numberOfShotRays += global__rayDepths[i];
			numberOfProcessedRays += (1 + global__lightsSize) * (i+1) * global__rayDepths[i];
			numberOfIntersectedTri += (1 + global__lightsSize) * i * global__rayDepths[i];
		}

		for(uint i=0; i<MAX_INTERSETCION_NUMBER; i++)
		{
			numberOfTestedBBx += i * global__rayIntersectedBBx[i];
			numberOfTestedTri += i * global__rayIntersectedTri[i];
		}


		CONSOLE_LOG << "Scene information" << ENDL;
		CONSOLE_LOG << "\t" << "image size          : " << global__imageWidth << " x " << global__imageHeight << ENDL;
		CONSOLE_LOG << "\t" << "number of triangles : " << global__triangulationSize << ENDL;
		CONSOLE_LOG << "\t" << "number of lights    : " << global__lightsSize << ENDL;
		CONSOLE_LOG << "\t" << "size of the bvh     : " << global__bvhSize << ENDL;
		CONSOLE_LOG << "\t" << "depth of bvh        : " << global__bvhMaxDepth << ENDL;

		CONSOLE_LOG << ENDL;
		
		CONSOLE_LOG << "Path tracing information" << ENDL;
		CONSOLE_LOG << "\t" << "Number of shot rays                    : " << numberOfShotRays << ENDL;
		CONSOLE_LOG << "\t" << "Number of processed rays               : " << numberOfProcessedRays << ENDL;
		CONSOLE_LOG << "\t" << "Average number of tested BBx per ray   : " << ((float) numberOfTestedBBx) / ((float) numberOfProcessedRays) << ENDL;
		CONSOLE_LOG << "\t" << "Average number of tested Tri per ray   : " << ((float) numberOfTestedTri) / ((float) numberOfProcessedRays) << ENDL;
		CONSOLE_LOG << "\t" << "Percentage of successful intersection : " << ((float) numberOfIntersectedTri * 100.) / ((float) numberOfTestedTri) << " % " << ENDL;
		CONSOLE_LOG << "\t" << "Depth histogram in %" << ENDL;
		for(uint i=0; i<global__rayMaxDepth; i++)
			CONSOLE_LOG << "\t\t" << i << " : " << global__rayDepths[i] * 100.0 / numberOfShotRays << " %" << ENDL;

		CONSOLE_LOG << ENDL;

		CONSOLE_LOG << "Time information" << ENDL;
		CONSOLE_LOG << "\t" << "Loading time        : " << loadingTime       << " ms" << ENDL;
		CONSOLE_LOG << "\t" << "BVH building time   : " << bvhBuildingTime   << " ms" << ENDL;
		CONSOLE_LOG << "\t" << "OpenCL setting time : " << openclSettingTime << " ms" << ENDL;
		CONSOLE_LOG << "\t" << "Path Tracing time   : " << pathTracingTime   << " ms" << ENDL;
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
		return global__window->PaintWindow(global__imageColor, global__imageRayNb);
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
			Node_Print(&global__bvh[This->son1Id], n+1);
			Node_Print(&global__bvh[This->son2Id], n+1);
		}
	}

	void PathTracer_PrintBVH()
	{
		LOG << "BVH : " << ENDL << ENDL;
		Node_Print( global__bvh, 0 );
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

		BVH_GetCharacteristics(global__bvh, 0, 0, BVHMaxLeafSize, BVHMinLeafSize, BVHMaxDepth, BVHMinDepth, nNodes, nLeafs, BVHMaxLeafSizeComments);

		CONSOLE_LOG << "\tCaracteristiques du BVH : " << ENDL;
		CONSOLE_LOG << "\t\tBVHMaxLeafSize : "	<< BVHMaxLeafSize	<< " --> " << BVHMaxLeafSizeComments << ENDL;
		CONSOLE_LOG << "\t\tBVHMinLeafSize : "	<< BVHMinLeafSize	<< ENDL;
		CONSOLE_LOG << "\t\tBVHMaxDepth : "		<< BVHMaxDepth		<< "  dans le build : " << global__bvhMaxDepth << ENDL;
		CONSOLE_LOG << "\t\tBVHMinDepth : "		<< BVHMinDepth		<< ENDL;
		CONSOLE_LOG << "\t\tnNodes : "			<< nNodes			<< ENDL;
		CONSOLE_LOG << "\t\tnLeafs : "			<< nLeafs			<< ENDL;

		CONSOLE_LOG << ENDL;

	}

	void PathTracer_PrintTriangulation()
	{
		LOG << "TRIANGULATION : " << ENDL;
		for(uint i=0; i<global__triangulationSize; i++)
			Triangle_Print(&global__triangulation[i], i);
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