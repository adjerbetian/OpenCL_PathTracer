
#ifndef PATHTRACER_OPENCL_HEADER
#define PATHTRACER_OPENCL_HEADER

#ifdef __APPLE__
#include <CL/opencl.h>
#else
#include <CL/cl.h>
#endif

#include "PathTracer_Utils.h"

namespace PathTracerNS
{

	ALIGN(16) typedef struct
	{
		Float4	pMin;
		Float4	pMax;
		Float4	centroid;
		char	isEmpty;
	} BoundingBox;

	typedef enum
	{
		LIGHT_DIRECTIONNAL,
		LIGHT_POINT,
		LIGHT_SPOT,
		LIGHT_UNKNOWN
	} LightType;

	ALIGN(16) typedef struct
	{
		Float4		position;
		Float4		direction;
		RGBAColor	color;
		float		power;
		float		cosOfInnerFallOffAngle;
		float		cosOfOuterFallOffAngle;
		LightType	type;
	} Light;

	typedef enum
	{
		MAT_STANDART,
		MAT_WATER,
		MAT_GLASS,
		MAT_VARNHISHED,
		MAT_METAL,
		MAT_UNKNOWN
	} MaterialType;


	ALIGN(16) typedef struct
	{
		RGBAColor		simpleColor;
		char const		*textureName;
		float			opacity;				//	Pour le verre
		int				textureId;
		MaterialType	type;
		bool			isSimpleColor;
		bool			hasAlphaMap;
	} Material;


	typedef enum
	{
		NODE_BAD_SAH,
		NODE_LEAF_MAX_SIZE,
		NODE_LEAF_MIN_DIAG
	} NodeStopType;


	ALIGN(16) typedef struct Node
	{
		BoundingBox trianglesAABB;
		BoundingBox centroidsAABB;
		uint cutAxis;
		uint triangleStartIndex;
		uint nbTriangles;
		uint son1Id;
		uint son2Id;
		NodeStopType comments;
		char isLeaf;
	} Node;


	ALIGN(16) typedef struct
	{
		Float4 origin;
		Float4 direction;
		Float4 inverse;
		char isInWater;
	} Ray3D;

	ALIGN(4) typedef struct
	{
		uint width;
		uint height;
		uint offset;
	} Texture;

	ALIGN(16) typedef struct
	{
		Float4	S1, S2, S3;			// Sommets 3D dans la scène
		Float4	N1, N2, N3;			// Normales
		Float4	T1, T2, T3;			// Tangentes
		Float4	BT1, BT2, BT3;		// Bitangentes
		Float4	N;					// Vecteur normal
		Float2	UVP1, UVP2, UVP3;	// Position des sommets sur la texture sur la face positive
		Float2	UVN1, UVN2, UVN3;	// Position des sommets sur la texture sur la face positive
		BoundingBox AABB;
		uint materialWithPositiveNormalIndex;
		uint materialWithNegativeNormalIndex;
		uint id;

	} Triangle;


	ALIGN(4) typedef struct
	{
		Texture skyTextures[6];
		float groundScale;
		float exposantFactorX;
		float exposantFactorY;
		float cosRotationAngle;
		float sinRotationAngle;
	} Sky;

	typedef enum
	{
		JITTERED,
		RANDOM,
		UNIFORM
	} Sampler;


	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////  GLOBAL VARS  /////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	class PathTracerDialog;
	class PathTracerImporter;

	typedef struct
	{
		PathTracerDialog	*global__window;		// Pointeur vers une fenêtre qui permet d'affichage du rendu
		PathTracerImporter	*global__importer;		// Pointeur vers une instance derivant de PathTracerImporter

		//	Donnees de la scene :

		Float4				 global__cameraDirection;		// Vecteur de direction principal de la caméra (passant par le milieu de l'image)
		Float4				 global__cameraRight;			// Vector orthogonal à la direction, canoniquement parrallèle au sol, qui indique la largeur de l'image
		Float4				 global__cameraUp;				// Vector orthogonal à la direction, canoniquement parrallèle à la vertical, qui indique la hauteur de l'image
		Float4				 global__cameraPosition;		// Position 3D du point de focal

		Node				*global__bvh;					// tableau de Noeuds représentant le BVH. global__bvh[0] est la racine de l'arbre
		Triangle			*global__triangulation;			// tableau de Triangles, NON DUPLIQUES, représentant la géométrie de la scène
		Light				*global__lights;				// tableau de Light, à l'exclusion du soleil et du ciel
		Material			*global__materiaux;				// tableau de tous les Matériaux de la scène
		Texture				*global__textures;				// tableau de Textures de la scène à l'exclusion des textures du ciel. Attention, Texture est une classe qui ne comprends ques les données principales des textures, et pas les textures même
		Uchar4				*global__texturesData;			// tableau de toutes les textures images de la scène, y compris celles du ciel. Les textures sont stockées de manière continues : pixel_texture1(0,0) ... pixel_texture1(w1,h1) , pixel_texture2(0,0) ... pixel_texture2(w2,h2) , ...

		uint				 global__bvhSize;				// Nombre de noeuds du BVH
		uint				 global__triangulationSize;		// Nombre de triangles de la scène
		uint				 global__lightsSize;			// Nombre de lumières de la scène, à l'exclusion du soleil et du ciel
		uint				 global__materiauxSize;			// Nombre de matériaux de la scène
		uint				 global__texturesSize;			// Nombre de textures de la scène
		uint				 global__texturesDataSize;		// Somme des tailles de toutes les textures : ( largeur1 * hauteur1 ) + ( largeur2 * hauteur2 ) + ...
		uint				 global__bvhMaxDepth;			// Profondeur de la plus grande branche du BVH

		Sky					 global__sky;	// Ciel


		//	Donnees images, qui viennent du rendu

		uint				 global__imageWidth;			// Largeur de l'image de rendu
		uint				 global__imageHeight;			// Hauteur de l'image de rendu
		uint				 global__imageSize;				// Largeur * Hauteur
		uint				 global__rayMaxDepth;			// Profondeur max des rayons
		RGBAColor			*global__imageColor;			// Somme des couleurs rendues
		float				*global__imageRayNb;			// Nombre de rayons ayant contribue aux pixels
		uint				*global__rayDepths;				// Number of ray for each depth
		uint				*global__rayIntersectedBBx;		// Number of ray for each number of BBx interstection
		uint				*global__rayIntersectedTri;		// Number of ray for each number of Tri interstection
	} GlobalVars;


	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////  METHODES  ////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////


	inline void	BoundingBox_Create ( BoundingBox& This, Float4 const& s1, Float4 const& s2, Float4 const& s3)
	{
		This.isEmpty = false;

		This.pMin = ppmin(ppmin(s1, s2), s3);
		This.pMax = ppmax(ppmax(s1, s2), s3);

		This.centroid = (This.pMin + This.pMax) / 2;
	}

	inline int BoundingBox_WidestDim ( BoundingBox const *This )
	{
		Float4 p1p2 = This->pMax - This->pMin;
		if(p1p2.x > p1p2.y)
			if(p1p2.x > p1p2.z)
				return 0;
			else
				return 2;
		if(p1p2.y > p1p2.z)
			return 1;
		return 2;
	}

	inline void BoundingBox_UniteWith ( BoundingBox *This, BoundingBox const *bb)
	{
		if(bb->isEmpty)
			return;

		if(This->isEmpty)
		{
			This->pMin = bb->pMin;
			This->pMax = bb->pMax;
			This->centroid = bb->centroid;
			This->isEmpty = false;
			return;
		}

		This->pMin = ppmin(This->pMin, bb->pMin);
		This->pMax = ppmax(This->pMax, bb->pMax);

		This->centroid = (This->pMin + This->pMax) / 2;
	}

	inline BoundingBox BoundingBox_UnionWith ( BoundingBox const *This, BoundingBox const *bb)
	{
		BoundingBox result = *This;
		BoundingBox_UniteWith( &result, bb);
		return result;
	}

	inline void BoundingBox_AddPoint ( BoundingBox *This, Float4 const *v)
	{
		if(This->isEmpty)
		{
			This->isEmpty = false;
			This->pMin = *v;
			This->pMax = *v;
			This->centroid = *v;
			return;
		}

		This->pMin = ppmin(This->pMin, *v);
		This->pMax = ppmax(This->pMax, *v);

		This->centroid = (This->pMin + This->pMax) / 2;
	}

	inline double BoundingBox_Area ( BoundingBox const *This )
	{
		if(This->isEmpty)
			return 0;

		Float4 p1p2 = Float4(This->pMax - This->pMin);
		return p1p2.x*p1p2.y + p1p2.y*p1p2.z + p1p2.z*p1p2.x;
	}

	inline void BoundingBox_Reset ( BoundingBox *This )
	{
		This->isEmpty = true;
	}


	inline void Material_Create( Material *This, MaterialType _type)
	{
		This->type = _type;
		This->textureName = NULL;
		This->textureId = 0;
		This->isSimpleColor = true;
		This->hasAlphaMap = false;
		This->opacity = 1;
	};

	inline void Ray3D_SetDirection( Ray3D *This, Float4 const *d)
	{
		This->direction = normalize(*d);

		This->inverse.x = 1.0f / This->direction.x;
		This->inverse.y = 1.0f / This->direction.y;
		This->inverse.z = 1.0f / This->direction.z;
		This->inverse.w = 0;
	};

	inline void Ray3D_Create( Ray3D *This, Float4 const *o, Float4 const *d, bool _isInWater)
	{
		This->origin = *o;
		This->isInWater = _isInWater;

		Ray3D_SetDirection(This, d);
	};

	inline Ray3D Ray3D_Opposite( Ray3D const *This )
	{
		Ray3D r;

		r.origin = This->origin;
		r.direction = This->direction * (-1);
		r.inverse = This->inverse * (-1);
		r.isInWater = This->isInWater;

		return r;
	};

	inline std::string BoundingBox_ToString (BoundingBox const *This)
	{
		if(This->isEmpty)
			return "AABB : empty";
		return "AABB : " + This->pMin.toString() + " --> " + This->pMax.toString();
	};
	
	inline std::string Triangle_ToString (Triangle const *This)
	{
		return "Triangle : [ " + This->S1.toString() + " , " + This->S2.toString() + " , " + This->S3.toString() + " ]  ----  " + BoundingBox_ToString(&This->AABB);
	};

}

#endif
