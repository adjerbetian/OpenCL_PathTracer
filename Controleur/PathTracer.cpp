
#include "PathTracer.h"
#include "PathTracer_FileImporter.h"
#include "PathTracer_OpenCL.h"

#include <ctime>
#include <sstream>

namespace PathTracerNS
{

	////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////   VARIABLES GLOBALES    //////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////

	//	Données d'import et d'affichage, dépendante du projet en Alone ou en LumenRT

	PathTracerDialog	*global__window;				// Pointeur vers une fenêtre qui permet d'affichage du rendu
	PathTracerImporter	*global__importer = NULL;		// Pointeur vers une instance dérivant de PathTracerImporter


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


	//	Données images, qui viennent du rendu

	uint				 global__imageWidth				= 0;		// Largeur de l'image de rendu
	uint				 global__imageHeight			= 0;		// Hauteur de l'image de rendu
	uint				 global__imageSize				= 0;		// Largeur * Hauteur
	RGBAColor			*global__imageColor				= NULL;		// Somme des couleurs rendues
	uint				*global__imageRayNb				= NULL;		// Nombre de rayons ayant contribue aux pixels
	uint				*global__imageRayDepth			= NULL;		// Depth of the ray at each pixel
	uint				*global__rayDepths				= NULL;		// Number of ray for each depth


	////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////   FONCTIONS    ///////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////

	/*	Permet de varier l'importateur :
	 *		- Par exemple charger un fichier pré-enregistré
	 *		- ou bien charger dynamiquement la scène depuis LumenRT
	 */

	void PathTracer_SetImporter(PathTracerImporter* importer)
	{
		global__importer = importer;
	}


	/*	Fonction principale
	 */

	void PathTracer_Main(uint image_width, uint image_height, uint numImagesToRender, bool saveRenderedImages, bool loadSky, bool exportScene)
	{
		CONSOLE << ENDL;

		CONSOLE << "//////////////////////////////////////////////////////////////////////////" << ENDL;
		CONSOLE << "                          PATH TRACER" << ENDL;
		CONSOLE << "//////////////////////////////////////////////////////////////////////////" << ENDL;

		PathTracer_Initialize(image_width, image_height, saveRenderedImages, loadSky);

		BVH_Create(global__triangulationSize, global__triangulation, &global__bvhMaxDepth, &global__bvhSize, &global__bvh);

		RTASSERT(global__bvhMaxDepth < BVH_MAX_DEPTH); // Le programme n'est pas guaranti de fonctionner si cette affirmatiion est fausse.
		RTASSERT(global__lightsSize < MAX_LIGHT_SIZE); // Le programme n'est pas guaranti de fonctionner si cette affirmatiion est fausse.

		if(exportScene)
			PathTracer_Export();

		bool noError = true;

		noError &= OpenCL_SetupContext();

		if(!noError) return ;

		noError &= OpenCL_InitializeMemory	(
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
			global__imageColor			,
			global__imageRayNb			,
			global__imageRayDepth		,
			global__rayDepths			,
			global__bvhMaxDepth			,

			&global__sky
			);

		if(!noError) return ;

		noError &= OpenCL_RunKernel(
			global__imageWidth			,
			global__imageHeight			,
			global__imageSize			,
			global__imageColor			,
			global__imageRayNb			,
			global__imageRayDepth		,
			global__rayDepths			,
			&PathTracer_UpdateWindow	,
			numImagesToRender
			);

		if(!noError) return ;

		PathTracer_Clear();
	}

	/*	Gestion des différents initialiseurs
	*/

	void PathTracer_Initialize(uint image_width, uint image_height, bool saveRenderedImages, bool loadSky)
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

		if(!global__importer->Import(image_width, image_height, loadSky))
		{
			system("pause");
			exit(1);
		}

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

		RTASSERT(global__imageWidth > 0 && global__imageHeight > 0);

		global__imageColor		= new RGBAColor[global__imageWidth*global__imageHeight];
		global__imageRayNb		= new uint[global__imageWidth*global__imageHeight];
		global__imageRayDepth	= new uint[global__imageWidth*global__imageHeight];
		global__rayDepths		= new uint[MAX_REFLECTION_NUMBER+1];

		for(int x=0; x<MAX_REFLECTION_NUMBER+1; x++)
			global__rayDepths[x] = 0;

		for(uint x=0; x<global__imageSize; x++)
		{
			global__imageColor[x] = RGBAColor(0,0,0,0);
			global__imageRayNb   [x] = 0;
			global__imageRayDepth[x] = 0;
		}
	}

	/*	Création de la fenêtre d'affichage
	 */

	void PathTracer_InitializeWindow(bool saveRenderedImages)
	{
		// On crée la fenêtre

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
		delete[] global__imageRayDepth;
		delete[] global__rayDepths;

		delete[] global__bvh;
		delete[] global__triangulation;
		delete[] global__lights;
		delete[] global__materiaux;
		delete[] global__textures;
		delete[] global__texturesData;
	}

	/*	Redirection de la fonction Update
	 */

	bool PathTracer_UpdateWindow()
	{
		global__window->PaintWindow(global__imageColor, global__imageRayNb);
		return true;
	}



	//////////////////////////////////////////////////////////////////////////
	// Fonctions d'impression
	//////////////////////////////////////////////////////////////////////////

	std::string BoundingBox_ToString( BoundingBox const *This)
	{
		if(This->isEmpty)
			return "AABB : empty";
		return "AABB : " + Vector_ToString(&This->pMin) + " --> " + Vector_ToString(&This->pMax);
	}

	std::string Triangle_ToString(Triangle const *This)
	{
		return "Triangle : [ " + Vector_ToString(&This->S1) + " , " + Vector_ToString(&This->S2) + " , " + Vector_ToString(&This->S3) + " ]  ----  " + BoundingBox_ToString(&This->AABB);
	}


	std::string Vector_ToString(Float4 const *This)
	{
		std::ostringstream oss;
		oss << "["
			<< (This->x >= 0 ? " " : "") << ((float)((int)( This->x*100 + ( This->x >= 0 ? 0.5 : -0.5 ) ) ) ) / 100 << (This->x == 0 ? "   " : "") << ", "
			<< (This->y >= 0 ? " " : "") << ((float)((int)( This->y*100 + ( This->y >= 0 ? 0.5 : -0.5 ) ) ) ) / 100 << (This->y == 0 ? "   " : "") << ", "
			<< (This->z >= 0 ? " " : "") << ((float)((int)( This->z*100 + ( This->z >= 0 ? 0.5 : -0.5 ) ) ) ) / 100 << (This->z == 0 ? "   " : "") << ", "
			<< (This->w >= 0 ? " " : "") << ((float)((int)( This->w*100 + ( This->w >= 0 ? 0.5 : -0.5 ) ) ) ) / 100 << (This->w == 0 ? "   " : "") << "]";
		return oss.str();
	}


	void Node_Print( Node const *This, uint n)
	{
		CONSOLE << " ";
		for(uint i = 0; i < n; i++ )
			CONSOLE << " | ";
		CONSOLE << (This->isLeaf ? " F" : " N") << n << " : ";
		CONSOLE << This->triangleStartIndex << " , " << This->nbTriangles << " CA : " << This->cutAxis << " ";
		CONSOLE << (This->isLeaf ? (This->comments == NODE_BAD_SAH ? "Bad SAH" : (This->comments == NODE_LEAF_MAX_SIZE ? "Leaf max size" : (This->comments == NODE_LEAF_MIN_DIAG ? "Leaf min diag" : "Unknown"))) : "");
		CONSOLE << ENDL;

		if(!This->isLeaf)
		{
			Node_Print(&global__bvh[This->son1Id], n+1);
			Node_Print(&global__bvh[This->son2Id], n+1);
		}
	}

	void PathTracer_PrintBVH()
	{
		CONSOLE << "BVH : " << ENDL << ENDL;
		Node_Print( global__bvh, 0 );
		CONSOLE << ENDL;
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

		CONSOLE << "\tCaracteristiques du BVH : " << ENDL;
		CONSOLE << "\t\tBVHMaxLeafSize : "	<< BVHMaxLeafSize	<< " --> " << BVHMaxLeafSizeComments << ENDL;
		CONSOLE << "\t\tBVHMinLeafSize : "	<< BVHMinLeafSize	<< ENDL;
		CONSOLE << "\t\tBVHMaxDepth : "		<< BVHMaxDepth		<< "  dans le build : " << global__bvhMaxDepth << ENDL;
		CONSOLE << "\t\tBVHMinDepth : "		<< BVHMinDepth		<< ENDL;
		CONSOLE << "\t\tnNodes : "			<< nNodes			<< ENDL;
		CONSOLE << "\t\tnLeafs : "			<< nLeafs			<< ENDL;

		CONSOLE << ENDL;

	}


}