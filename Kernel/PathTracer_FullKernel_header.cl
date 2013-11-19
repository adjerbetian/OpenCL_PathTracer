
#ifndef PATHTRACER_KERNEL
#define PATHTRACER_KERNEL


////////////////////////////////////////////////////////////////////////////////////////
///						PRE PROCESSEUR
////////////////////////////////////////////////////////////////////////////////////////


#define PATH_PI 3.14159265f
#define PATH_PI_INVERSE 0.31830988618f
#define RUSSIAN_ROULETTE false
#define MIN_REFLECTION_NUMBER 3
#define MAX_REFLECTION_NUMBER 10
#define MIN_CONTRIBUTION_VALUE 0.001f
#define BVH_MAX_DEPTH 30
#define MAX_LIGHT_SIZE 30

// Debug log
//#define PRINT_DEBUG_INFO(X, Y, Z) printf(X" : Group x : %i : \tGroup y : %i : \t item x : %i : \t item y : %i : \t local id : %i : \t global id : %i : \t "#Y" \n", get_group_id(0),  get_group_id(1), get_local_id(0),  get_local_id(1), get_local_id(1) * get_local_size(0) + get_local_id(0), get_global_id(1)*get_global_size(0) + get_global_id(0) , Z)
#define PRINT_DEBUG_INFO(X, Y, Z)

// Pour des debug locaux
#define PRINT_DEBUG_INFO2(X, Y, Z) printf(X" : local id : ( %v2u ) : \t global id : ( %v2u ) : \t "Y" \n", (uint2) (get_local_id(0),  get_local_id(1)) , (uint2) (get_global_id(0), get_global_id(1)) , Z)

// Assert
//#define ASSERT(X) if(!(X)) { printf("***************  ERROR ******************* : Group x : %i : \tGroup y : %i : \t item x : %i : \t item y : %i : \t local id : %i : \t global id : %i : \t error : "#X"\n", get_group_id(0),  get_group_id(1), get_local_id(0),  get_local_id(1), get_local_id(1) * get_local_size(0) + get_local_id(0), get_global_id(1)*get_global_size(0) + get_global_id(0)); }
#define ASSERT(X) 


#define INT4 (int4)
#define FLOAT2 (float2)
#define FLOAT4 (float4)
#define DOUBLE4 (double4)
#define RGBACOLOR (RGBAColor)
#define NULL 0
#define IMAGE3D __read_only image3d_t

typedef double4 RGBAColor;

#define KERNEL_GLOBAL_VAR		 \
	seed						,\
	global__bvh					,\
	global__triangulation		,\
	global__lights				,\
	global__materiaux			,\
	global__textures			,\
	global__texturesData		,\
	global__lightsSize			,\
	global__sun					,\
	global__sky					



#define KERNEL_GLOBAL_VAR_DECLARATION							 \
	int								*seed						,\
	Node		__global	const	*global__bvh				,\
	Triangle	__global	const	*global__triangulation		,\
	Light		__global	const	*global__lights				,\
	Material	__global	const	*global__materiaux			,\
	Texture		__global	const	*global__textures			,\
	uchar4		__global	const	*global__texturesData		,\
	uint							 global__lightsSize			,\
	SunLight	__global const		*global__sun				,\
	Sky			__global const		*global__sky				



////////////////////////////////////////////////////////////////////////////////////////
///						STRUCTURES
////////////////////////////////////////////////////////////////////////////////////////

typedef struct
{
	double4 origin;
	double4 direction;
	double4 inverse;
	char	positiveDirection[3];
	char	isInWater;
} Ray3D;

typedef struct
{
	double4	pMin;
	double4	pMax;
	double4	centroid;
	char	isEmpty;
} BoundingBox;

typedef enum
{
	NODE_BAD_SAH,
	NODE_LEAF_MAX_SIZE,
	NODE_LEAF_MIN_DIAG
} NodeStopType;


typedef struct Node
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


typedef enum
{
	LIGHT_POINT,
	LIGHT_SPOT,
	LIGHT_UNKNOWN
} LightType;


typedef struct
{
	double4		position;
	double4		direction;
	RGBAColor	color;
	float		power;
	float		cosOfInnerFallOffAngle;
	float		cosOfOuterFallOffAngle;
	LightType	type;
} Light;

typedef struct
{
	double4		direction;
	RGBAColor		color;
	float			power;
} SunLight;

typedef struct
{
	uint width;
	uint height;
	uint offset;
} Texture;

#define MATERIAL_N_WATER 1.333f
#define MATERIAL_N_GLASS 1.55f
#define MATERIAL_N_VARNISH 1.5f
#define MATERIAL_KSCHLICK 0.8f
#define MATERIAL_WATER_SCATTERING_PART 1.f
#define MATERIAL_WATER_ABSORPTION_COLOR RGBACOLOR(0.28f, 0.14f, 0.10f, 0.f)
//#define MATERIAL_WATER_ABSORPTION_COLOR RGBACOLOR(0.f, 0.f, 0.f, 0.f)
//#define MATERIAL_WATER_ABSORPTION_COLOR RGBACOLOR(0.4f,	0.0511f,	0.0053f,	0.f)



typedef enum
{
	MAT_STANDART,
	MAT_WATER,
	MAT_GLASS,
	MAT_VARNHISHED,
	MAT_METAL,
	MAT_UNKNOWN
} MaterialType;


typedef struct
{
	RGBAColor		simpleColor;
	float			opacity;				//	Pour le verre
	uint			textureId;
	MaterialType	type;
	char			isSimpleColor;
	char			hasAlphaMap;
	char const		*textureName;
} Material;


typedef struct
{
	Texture skyTextures[6];
	float groundScale;
	float exposantFactorX;
	float exposantFactorY;
	float cosRotationAngle;
	float sinRotationAngle;
} Sky;

typedef struct
{
	double4	S1, S2, S3;			// Sommets 3D dans la scène
	double4	N1, N2, N3;			// Normales
	double4	T1, T2, T3;			// Tangentes
	double4	BT1, BT2, BT3;		// Bitangentes
	double4	N;					// Vecteur normal
	float2	UVP1, UVP2, UVP3;	// Position des sommets sur la texture sur la face positive
	float2	UVN1, UVN2, UVN3;	// Position des sommets sur la texture sur la face positive
	BoundingBox AABB;
	uint materialWithPositiveNormalIndex;
	uint materialWithNegativeNormalIndex;
} Triangle;


////////////////////////////////////////////////////////////////////////////////////////
///						UTILITAIRES
////////////////////////////////////////////////////////////////////////////////////////

inline float Vector_SquaredNorm			(double4 const	*This)					{ return dot(*This, *This);};
inline float Vector_SquaredDistanceTo	(double4 const	*This, double4 const *v)	{ double4 temp = (*v) - ((*This)); return Vector_SquaredNorm(&temp);};
inline bool	 Vector_LexLessThan			(double4 const	*This, double4 const *v)	{ return ( (*This).x < (*v).x ) || ( ((*This).x == (*v).x) && ((*This).y < (*v).y) ) || ( ((*This).x == (*v).x) && ((*This).y == (*v).y) && ((*This).z < (*v).z) );};
inline float Vector_Max					(double4 const	*This)					{ return max((*This).x, max((*This).y, (*This).z));};
inline float Vector_Mean				(double4 const	*This)					{ return ( (*This).x + (*This).y + (*This).z ) / 3;};
inline bool	 RGBAColor_IsTransparent	(RGBAColor const *This)						{ return (*This).w > 0.5f;};

inline void Vector_PutInSameHemisphereAs(double4 *This, double4 const *N)
{
	float dotProd = dot(*This, *N);
	if( dotProd < 0.001f )
		(*This) += (*N) * (0.01f - dotProd);

	ASSERT(dot(*This, *N) > 0.0f);
};

inline float random	(int *seed)
{
	const ulong a = 16807;
	const ulong m = 0x7FFFFFFF;//2147483647;
	*seed = (a * (*seed)) & m;
	float res = ((float) *seed) / ((float) m);
	return res;
};

inline int InitializeRandomSeed(uint x, uint y, uint width, uint height, uint iterationNum)
{
	int seed = 0;
	seed = x + ( y * width ) + ( iterationNum * width * height );
	seed *= 2011;	//	Nombre premier, pour plus répartir les nombres
	seed *= seed;
	if(seed == 0)
		seed = 1;
	return seed;
};


void PrefixSum_uint (uint __local volatile *values);
void PrefixSum_int (int __local volatile *values);

////////////////////////////////////////////////////////////////////////////////////////
///						RAY 3D
////////////////////////////////////////////////////////////////////////////////////////



inline void Ray3D_SetDirection( Ray3D *This, double4 const *d)
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

inline void Ray3D_Create( Ray3D *This, double4 const *o, double4 const *d, bool _isInWater)
{
	This->origin = *o;
	This->isInWater = _isInWater;

	Ray3D_SetDirection(This, d);
};

inline Ray3D Ray3D_Opposite( Ray3D const *This )
{
	Ray3D r;

	r.origin = This->origin;
	r.direction = - This->direction;
	r.inverse = - This->inverse;
	r.isInWater = This->isInWater;

	for(int i=0; i<3; i++)
		r.positiveDirection[i] = !This->positiveDirection[i];
	return r;
};


////////////////////////////////////////////////////////////////////////////////////////
///						BOUNDING BOX
////////////////////////////////////////////////////////////////////////////////////////


inline void	BoundingBox_Create ( BoundingBox *This, double4 const *s1, double4 const *s2, double4 const *s3)
{
	This->isEmpty = false;

	This->pMin = min(min(*s1, *s2), *s3);
	This->pMax = max(max(*s1, *s2), *s3);

	This->centroid = (This->pMin + This->pMax) / 2;
}

inline int BoundingBox_WidestDim ( BoundingBox const *This )
{
	double4 p1p2 = This->pMax - This->pMin;
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

	This->pMin = min(This->pMin, bb->pMin);
	This->pMax = max(This->pMax, bb->pMax);

	This->centroid = (This->pMin + This->pMax) / 2;
}

inline BoundingBox BoundingBox_UnionWith ( BoundingBox const *This, BoundingBox const *bb)
{
	BoundingBox result = *This;
	BoundingBox_UniteWith( &result, bb);
	return result;
}

inline void BoundingBox_AddPoint ( BoundingBox *This, double4 const *v)
{
	if(This->isEmpty)
	{
		This->isEmpty = false;
		This->pMin = *v;
		This->pMax = *v;
		This->centroid = *v;
		return;
	}

	This->pMin = min(This->pMin, *v);
	This->pMax = max(This->pMax, *v);

	This->centroid = (This->pMin + This->pMax) / 2;
}

inline float BoundingBox_Area ( BoundingBox const *This )
{
	if(This->isEmpty)
		return 0;

	double4 p1p2 = DOUBLE4(This->pMax - This->pMin);
	return p1p2.x*p1p2.y + p1p2.y*p1p2.z + p1p2.z*p1p2.x;
}

inline void BoundingBox_Reset ( BoundingBox *This )
{
	This->isEmpty = true;
}

bool BoundingBox_Intersects ( BoundingBox const *This, Ray3D const *r, const float squaredDistance);


////////////////////////////////////////////////////////////////////////////////////////
///						LIGHT
////////////////////////////////////////////////////////////////////////////////////////

inline float Light_PowerToward( Light const *This, double4 const *v)
{
	if(This->type == LIGHT_POINT)
		return This->power;

	//SPOT
	float cosAngle = dot(*v, This->direction);
	if(cosAngle > This->cosOfInnerFallOffAngle)
		return This->power;
	if(cosAngle < This->cosOfOuterFallOffAngle)
		return 0.0f;
	return This->power * (cosAngle - This->cosOfOuterFallOffAngle) / ( This->cosOfInnerFallOffAngle - This->cosOfOuterFallOffAngle );
}



////////////////////////////////////////////////////////////////////////////////////////
///						Texture
////////////////////////////////////////////////////////////////////////////////////////


inline RGBAColor Texture_GetPixelColorValue( Texture __global const *This, uint textureId, uchar4 __global const *global__texturesData, float u, float v)
{

	Texture tex = *This;

	double4 bgra;
	uint x,y;

	u = u - ((int) u) + ( u<0 ? 1 : 0);
	v = v - ((int) v) + ( v<0 ? 1 : 0);

	x = (uint) (u*(tex.width  - 1));
	y = (uint) (v*(tex.height - 1));

	const uint index = This->offset + y * This->width + x;

	bgra = convert_double4(global__texturesData[index])/255.0;
	bgra.w = 1.f - bgra.w;
//	return bgra.zyxw;
	return bgra;
};




////////////////////////////////////////////////////////////////////////////////////////
///						MATERIAL
////////////////////////////////////////////////////////////////////////////////////////

void		Material_Create								( Material *This, MaterialType _type);
float		Material_BRDF								( Material const *This, Ray3D const *ri, double4 const *N, Ray3D const *rr);
float		Material_FresnelGlassReflectionFraction		( Material const *This, Ray3D const *r , double4 const *N);
float		Material_FresnelWaterReflectionFraction		( Material const *This, Ray3D const *r , double4 const *N, double4 *refractionDirection, float *refractionMultCoeff);
float		Material_FresnelVarnishReflectionFraction	( Material const *This, Ray3D const *r , double4 const *N, bool isInVarnish, double4 *refractionDirection);
double4		Material_FresnelReflection					( Material const *This, double4 const *v, double4 const *N);

// Methodes statiques

double4		Material_CosineSampleHemisphere	(int *kernel_itemSeed, double4 const *surfaceNormal);
void		Material_ConcentricSampleDisk	(int *kernel_itemSeed, float *dx, float *dy);
RGBAColor	Material_WaterAbsorption		(float dist);



////////////////////////////////////////////////////////////////////////////////////////
///						SKY
////////////////////////////////////////////////////////////////////////////////////////

RGBAColor	Sky_GetColorValue		( Sky __global const *This , uchar4 __global const *global__texturesData, Ray3D const *r);
RGBAColor	Sky_GetFaceColorValue	( Sky __global const *This , uchar4 __global const *global__texturesData, int faceId, float u, float v);


////////////////////////////////////////////////////////////////////////////////////////
///						Triangle
////////////////////////////////////////////////////////////////////////////////////////

bool				Triangle_Intersects			(Texture __global const *global__textures, Material __global const *global__materiaux, uchar4 __global const *global__texturesData, Triangle const *This, Ray3D const *r, float *squaredDistance, double4 *intersectionPoint, Material *intersectedMaterial, RGBAColor *intersectionColor, float *sBestTriangle, float *tBestTriangle);
RGBAColor			Triangle_GetColorValueAt	(__global Texture const *global__textures, __global Material const *global__materiaux, uchar4 __global const *global__texturesData, Triangle const *This, bool positiveNormal, float s, float t);
double4				Triangle_GetSmoothNormal	(Triangle const	*This, bool positiveNormal, float s, float t);
double4				Triangle_GetNormal			(Triangle const	*This, bool positiveNormal);

inline double4		Triangle_GetNormal			(Triangle const *This, bool positiveNormal) { return positiveNormal ? This->N : - This->N; }

////////////////////////////////////////////////////////////////////////////////////////
///						BVH
////////////////////////////////////////////////////////////////////////////////////////


bool		BVH_IntersectRay					(KERNEL_GLOBAL_VAR_DECLARATION, const Ray3D *r, double4 *intersectionPoint, float *s, float *t, Triangle *intersetedTriangle, Material *intersectedMaterial, RGBAColor *intersectionColor);
bool		BVH_IntersectShadowRay				(KERNEL_GLOBAL_VAR_DECLARATION, const Ray3D *r, float squaredDistance, RGBAColor *tint);

////////////////////////////////////////////////////////////////////////////////////////
///						SCENE
////////////////////////////////////////////////////////////////////////////////////////

RGBAColor	Scene_ComputeRadiance				(KERNEL_GLOBAL_VAR_DECLARATION, const double4 *p, Ray3D *r, Triangle const *triangle, Material const * mat, RGBAColor const * materialColor, float s, float t, RGBAColor const *directIlluminationRadiance, double4 const *Ng,  double4 const *Ns);
RGBAColor	Scene_ComputeDirectIllumination		(KERNEL_GLOBAL_VAR_DECLARATION, const double4 *p, const Ray3D *cameraRay, Material const *mat, const double4 *N);
RGBAColor	Scene_ComputeScatteringIllumination	(KERNEL_GLOBAL_VAR_DECLARATION, const Ray3D *cameraRay, const double4 *intersectionPoint);
bool		Scene_PickLight						(KERNEL_GLOBAL_VAR_DECLARATION, const double4 *p, const Ray3D *cameraRay, Material const *mat, const double4 *N, Light *light, float *lightContribution);


////////////////////////////////////////////////////////////////////////////////////////
///						Kernel
////////////////////////////////////////////////////////////////////////////////////////

__kernel void Kernel_Main(
	uint     kernel__iterationNum,
	uint     kernel__imageWidth,
	uint     kernel__imageHeight,

	double4  kernel__cameraPosition,
	double4  kernel__cameraDirection,
	double4  kernel__cameraRight,
	double4  kernel__cameraUp,

	uint     global__lightsSize,

	double4	__global			*global__imageColor,
	uint	__global			*global__imageRayNb,
	void	__global	const	*global__void__bvh,
	void	__global	const	*global__void__triangulation,
	void	__global	const	*global__void__lights,
	void	__global	const	*global__void__materiaux,
	void	__global	const	*global__void__textures,
	uchar4	__global	const	*global__texturesData,
	void	__global	const	*global__void__sun,
	void	__global	const	*global__void__sky
	);

#endif