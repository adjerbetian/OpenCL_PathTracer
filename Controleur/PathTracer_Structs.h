
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



	ALIGN(16) typedef struct
	{
		Float4		direction;
		RGBAColor		color;
		float			power;
	} SunLight;


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
		float			opacity;				//	Pour le verre
		uint			textureId;
		MaterialType	type;
		char			isSimpleColor;
		char			hasAlphaMap;
		char const		*textureName;
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
		RGBAColor transferFunction;
		cl_uint2 pixel;
		int reflectionId;
		char positiveDirection[3];
		char isInWater;
		char isActive;
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


	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////  METHODES  ////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////


	inline void	BoundingBox_Create ( BoundingBox *This, Float4 const *s1, Float4 const *s2, Float4 const *s3)
	{
		This->isEmpty = false;

		This->pMin = ppmin(ppmin(*s1, *s2), *s3);
		This->pMax = ppmax(ppmax(*s1, *s2), *s3);

		This->centroid = (This->pMin + This->pMax) / 2;
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

	inline float BoundingBox_Area ( BoundingBox const *This )
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

		This->positiveDirection[0] = ( This->inverse.x > 0 );
		This->positiveDirection[1] = ( This->inverse.y > 0 );
		This->positiveDirection[2] = ( This->inverse.z > 0 );
	};

	inline void Ray3D_Create( Ray3D *This, Float4 const *o, Float4 const *d, bool _isInWater)
	{
		This->origin = *o;
		This->isInWater = _isInWater;
		This->reflectionId = 0;
		This->isActive = true;

		Ray3D_SetDirection(This, d);
	};

	inline Ray3D Ray3D_Opposite( Ray3D const *This )
	{
		Ray3D r;

		r.origin = This->origin;
		r.direction = This->direction * (-1);
		r.inverse = This->inverse * (-1);
		r.isInWater = This->isInWater;
		r.reflectionId = This->reflectionId;

		for(int i=0; i<3; i++)
			r.positiveDirection[i] = !This->positiveDirection[i];
		return r;
	};
}

#endif
