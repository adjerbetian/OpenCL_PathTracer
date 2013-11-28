


#include "C:\Users\Alexandre Djerbetian\Documents\Visual Studio 2012\Projects\OpenCL_PathTracer\src\Kernel\PathTracer_FullKernel_header.cl"


////////////////////////////////////////////////////////////////////////////////////////
///						Utilitaires
////////////////////////////////////////////////////////////////////////////////////////

/*	Somme préfixe parralele sur le tableau local *values
 *	Le tableau *values doit être de la taille du groupe
 *
 *	En sorti, on obtient le tableau dont chaque valeur est 
 *	la somme des valeurs précédentes dans le tableau d'entrée
 */
void PrefixSum_int(int __local volatile *values)
{
	PRINT_DEBUG_INFO("PrefixSum START", value : %u, values[get_local_id(0)]);

	for( uint offset = 1; offset < get_local_size(0); offset *= 2)
	{
		int t;
		if(get_local_id(0) >= offset )
			t = values[get_local_id(0) - offset ];

		barrier(CLK_LOCAL_MEM_FENCE);

		if(get_local_id(0) >= offset )
			values[get_local_id(0)] = t + values[get_local_id(0)];

		barrier(CLK_LOCAL_MEM_FENCE);
	}

	PRINT_DEBUG_INFO("PrefixSum END", value : %u, values[get_local_id(0)]);
}

void PrefixSum_uint(uint __local volatile *values)
{
	PRINT_DEBUG_INFO("PrefixSum START", value : %u, values[get_local_id(0)]);

	for( uint offset = 1; offset < get_local_size(0); offset *= 2)
	{
		uint t;
		if(get_local_id(0) >= offset )
			t = values[get_local_id(0) - offset ];

		barrier(CLK_LOCAL_MEM_FENCE);

		if(get_local_id(0) >= offset )
			values[get_local_id(0)] = t + values[get_local_id(0)];

		barrier(CLK_LOCAL_MEM_FENCE);
	}

	PRINT_DEBUG_INFO("PrefixSum END", value : %u, values[get_local_id(0)]);
}



////////////////////////////////////////////////////////////////////////////////////////
///						BOUNDING BOX
////////////////////////////////////////////////////////////////////////////////////////

bool BoundingBox_Intersects ( BoundingBox const *This, Ray3D const *r, const float squaredDistance)
{
	if(This->isEmpty)
		return false;

	float tMin, tMax;
	float tyMin, tyMax, tzMin, tzMax;

	if(r->positiveDirection[0])
	{
		tMin = ( This->pMin.x - r->origin.x )*r->inverse.x;
		tMax = ( This->pMax.x - r->origin.x )*r->inverse.x;
	}
	else
	{
		tMin = ( This->pMax.x - r->origin.x )*r->inverse.x;
		tMax = ( This->pMin.x - r->origin.x )*r->inverse.x;
	}

	if(tMin<0 && tMax<0)					// Boite dans la mauvaise direction selon x
		return false;

	if(r->positiveDirection[1])
	{
		tyMin = ( This->pMin.y - r->origin.y )*r->inverse.y;
		tyMax = ( This->pMax.y - r->origin.y )*r->inverse.y;
	}
	else
	{
		tyMin = ( This->pMax.y - r->origin.y )*r->inverse.y;
		tyMax = ( This->pMin.y - r->origin.y )*r->inverse.y;
	}

	if(tyMin<0 && tyMax<0)					// Boite dans la mauvaise direction selon y
		return false;

	if( tMin > tyMax || tyMin > tMax )		// la projection selon l'axe z n'intersetcte pas
		return false;

	if(tyMin > tMin)
		tMin = tyMin;
	if(tyMax < tMax)
		tMax = tyMax;

	if(r->positiveDirection[2])
	{
		tzMin = ( This->pMin.z - r->origin.z )*r->inverse.z;
		tzMax = ( This->pMax.z - r->origin.z )*r->inverse.z;
	}
	else
	{
		tzMin = ( This->pMax.z - r->origin.z )*r->inverse.z;
		tzMax = ( This->pMin.z - r->origin.z )*r->inverse.z;
	}

	if(tzMin<0 && tzMax<0)
		return false; // Boite dans la mauvaise direction selon z

	if( tMin > tzMax || tzMin > tMax )
		return false; // Le rayon n'intersecte pas

	if(tzMin > tMin)
		tMin = tzMin;
	if(tzMax < tMax)
		tMax = tzMax;

	if(tMin < 0)
		return true; // L'origine du rayon est dans la boite

	if(tMin > squaredDistance )
		return false; // La boite est trop loin

	return true;
}


////////////////////////////////////////////////////////////////////////////////////////
///						MATERIAL
////////////////////////////////////////////////////////////////////////////////////////

/************************************************************************/
/*                            Constructeurs                             */
/************************************************************************/

void Material_Create( Material *This, MaterialType _type)
{
	This->type = _type;
	This->textureName = NULL;
	This->textureId = 0;
	This->isSimpleColor = true;
	This->hasAlphaMap = false;
	This->opacity = 1;
};


/************************************************************************/
/*                            Methodes                                  */
/************************************************************************/

//	Both ri and rr pointing toward the surface
float Material_BRDF( Material const *This, float4 const *ri, float4 const *N, float4 const *rr)
{
	if(This->type == MAT_STANDART)
		return PATH_PI_INVERSE;

	//if(This->type == MAT_GLASS)
	//	return 1;
	//
	//if(This->type == MAT_WATER)		//	Scattering
	//{
	//	//	Schlick model
	//	float denom = 1 + MATERIAL_KSCHLICK * dot(ri->direction, rr->direction);
	//	return ( 1 - MATERIAL_KSCHLICK*MATERIAL_KSCHLICK ) / ( 4 * PATH_PI * denom * denom );
	//}
	//
	//if(This->type == MAT_VARNHISHED)
	//{
	//	//	La lumière entre entre
	//	float rFresnel1 = Material_FresnelVarnishReflectionFraction(This, ri, N, false, NULL);
	//
	//	//	on cherche la direction de de diffusion originelle de rr
	//	float4 originalRefractionDirection;
	//	Material_FresnelVarnishReflectionFraction(This, rr, N, false, &originalRefractionDirection);
	//
	//	//	Calcul du deuxième coefficient de Fresnel
	//	Ray3D insideRay;
	//	float4 originalRefractionDirectionOpposite = -originalRefractionDirection;
	//	float4 NOpposite = -(*N);
	//
	//	Ray3D_Create(&insideRay, &rr->origin, &originalRefractionDirectionOpposite, rr->isInWater);
	//	float rFresnel2 = Material_FresnelVarnishReflectionFraction(This, &insideRay, &NOpposite, true, NULL);
	//
	//	return (1 - rFresnel1) * (1 - rFresnel2) * PATH_PI_INVERSE;
	//}

	ASSERT(false);
	return 0;

};

float Material_FresnelGlassReflectionFraction( Material const *This, Ray3D const *r , float4 const *N)
{
	ASSERT(This->type == MAT_GLASS);

	float n1, n2;

	if(r->isInWater)
		n1 = MATERIAL_N_WATER;
	else
		n1 = 1;

	n2 = MATERIAL_N_GLASS;

	float cos1 = - dot(r->direction, *N);
	float sin1 = sqrt(1 - cos1 * cos1);
	float sin2 = n1 * sin1 / n2;
	if(sin2 >= 1) // Totalement réfléchi
		return 1;
	float cos2 = sqrt(1 - sin2*sin2);

	float rPara = ( n2 * cos1 - n1 * cos2 ) / ( n2 * cos1 + n1 * cos2 );
	float rPerp = (	n1 * cos1 - n2 * cos2 ) / ( n1 * cos1 + n2 * cos2 );

	float rFresnel = (rPara*rPara + rPerp*rPerp)/2.0f;

	ASSERT(rFresnel <= 1);

	return rFresnel;

}

float Material_FresnelWaterReflectionFraction( Material const *This, Ray3D const *r , float4 const *N, float4 *refractionDirection, float *refractionMultCoeff)
{
	ASSERT(This->type == MAT_WATER);

	float n1, n2;

	if(r->isInWater)
	{
		n1 = MATERIAL_N_WATER;
		n2 = 1;
	}
	else
	{
		n1 = 1;
		n2 = MATERIAL_N_WATER;
	}

	float cos1 = - dot(r->direction, *N);
	float sin1 = sqrt(1 - cos1 * cos1);
	float sin2 = n1 * sin1 / n2;
	if(sin2 >= 1) // Totalement réfléchi
		return 1;
	float cos2 = sqrt(1 - sin2*sin2);

	float rPara = ( n2 * cos1 - n1 * cos2 ) / ( n2 * cos1 + n1 * cos2 );
	float rPerp = (	n1 * cos1 - n2 * cos2 ) / ( n1 * cos1 + n2 * cos2 );

	float rFresnel = (rPara*rPara + rPerp*rPerp)/2.0f;

	ASSERT(rFresnel <= 1);

	if(refractionDirection != NULL)
		*refractionDirection = r->direction * (n1/n2) + (*N) * (n1/n2 * cos1 - cos2);
	if(refractionMultCoeff != NULL)
		*refractionMultCoeff = (n2*n2)/(n1*n1);

	return rFresnel;
}

float Material_FresnelVarnishReflectionFraction( Material const *This, Ray3D const *r , float4 const *N, bool isInVarnish, float4 *refractionDirection)
{
	ASSERT(This->type == MAT_VARNHISHED);
	ASSERT( dot(r->direction, *N) <= 0 );

	float n1, n2;

	n1 = isInVarnish ? MATERIAL_N_VARNISH : (r->isInWater ? MATERIAL_N_WATER : 1.0f);
	n2 = isInVarnish ? (r->isInWater ? MATERIAL_N_WATER : 1.0f) : MATERIAL_N_VARNISH;

	float cos1 = fmin(1.f , - dot(r->direction, *N) );
	float sin1 = sqrt(1 - cos1 * cos1);
	float sin2 = n1 * sin1 / n2;
	if(sin2 >= 1) // Totalement réfléchi
		return 1;
	float cos2 = sqrt(1 - sin2*sin2);

	float rPara = ( n2 * cos1 - n1 * cos2 ) / ( n2 * cos1 + n1 * cos2 );
	float rPerp = (	n1 * cos1 - n2 * cos2 ) / ( n1 * cos1 + n2 * cos2 );

	float rFresnel = (rPara*rPara + rPerp*rPerp)/2.0f;

	ASSERT(rFresnel <= 1);

	if(refractionDirection != NULL)
		*refractionDirection = r->direction * (n1/n2) + (*N) * (n1/n2 * cos1 - cos2);

	return rFresnel;
}

float4 Material_FresnelReflection( Material const *This, float4 const *v, float4 const *N)
{
	ASSERT(This->type != MAT_STANDART);

	float4 reflection = (*v) - ( (*N) * ( 2 * dot(*v, *N) ) );
	return reflection;
}


float4 Material_CosineSampleHemisphere(int *kernel__seed, float4 const *N)
{
	ASSERT(fabs(length(*N) - 1.0f) < 0.0001f);

	//	1 - Calcul d'un rayon aléatoire sur l'hémisphère canonique

	float x,y,z;
	Material_ConcentricSampleDisk(kernel__seed, &x, &y);
	z = 1 - x*x - y*y;
	z = (z<0) ? 0 : sqrt(z);
	const float4 v = FLOAT4(x,y,z,0);

	//	2 - Rotation suivant l'axe *N

	if( (*N).z > 0.9999f ) // angle trop petit, donc rotation negligee
		return v;
	if( (*N).z < - 0.9999f ) // idem, mais dans l'autre sens
		return -v;

	//	Création d'un base directe pour le nouvel hémisphère
	float4 sn = normalize(FLOAT4( - (*N).y , (*N).x , 0 , 0 )); // = float4(0,0,1) ^ N
	float4 tn = normalize(cross(*N, sn));

	//	Rotation
	float4 vWorld = normalize(FLOAT4(
		dot(FLOAT4(sn.x, tn.x, (*N).x, 0), v),
		dot(FLOAT4(sn.y, tn.y, (*N).y, 0), v),
		dot(FLOAT4(sn.z, tn.z, (*N).z, 0), v),
		0));

	ASSERT( dot(vWorld, *N) >= 0);

	return vWorld;
};


void Material_ConcentricSampleDisk(int *kernel__seed, float *dx, float *dy)
{
	//	1 - tirage au sort de 2 coordonnées dans le carré 1x1

	float u1 = random(kernel__seed);
	float u2 = random(kernel__seed);

	float r,theta;

	//	2 - Remise dans un carré de [-1 , 1] x [-1 , 1]

	float sx = 2 * u1 - 1;
	float sy = 2 * u2 - 1;

	//	3 - Calcul du rayon et de l'angle dans le cercle selon la zone de tirage :
	//	 _________________
	//	|\               /|
	//	|  \    2      /  |
	//	|    \       /    |
	//	|      \   /      |
	//	|  3     /     1  |
	//	|      /  \       |
	//	|    /      \     |
	//	|  /    4     \   |
	//	|/______________\_|
	//

	// Si on est au centre
	if( fabs(sx) < 0.0001f && fabs(sy) < 0.0001f )
	{
		r = 0;
		theta = 0;
	}
	else if(sx > -sy)
	{
		//	Zone 1
		if(sx > sy)
		{
			r = sx;
			if(sy > 0)	theta = sy/sx;
			else		theta = 8.f + sy/sx;
		}
		//	Zone 2
		else
		{
			r = sy;
			theta = 2.f - sx/sy;
		}
	}
	else
	{
		//	Zone 3
		if(sx < sy)
		{
			r = -sx;
			theta = 4.f + sy/sx;
		}
		//	Zone 4
		else
		{
			r = -sy;
			theta = 6.f - sy/sx;
		}
	}
	theta *= PATH_PI / 4.f;	//	Coefficient commun à tous les cas ci-dessus
	r *= 0.999;				//	Précaution en cas de d'erreur de précision numérique

	//	4 - Calcul des coordonnées dans le cerle

	*dx = r * cos( theta );	
	*dy = r * sin( theta );

	ASSERT((*dx)*(*dx) + (*dy)*(*dy) < 1);
};


RGBAColor Material_WaterAbsorption(float dist)
{
	if(dist == INFINITY)
		return RGBACOLOR(0.f, 0.f, 0.f, 0.f);

	RGBAColor absorption = exp( - MATERIAL_WATER_ABSORPTION_COLOR * dist );
	ASSERT(absorption.x >= 0 && absorption.x <= 1 && absorption.y >= 0 && absorption.y <= 1 && absorption.z >= 0 && absorption.z <= 1);

	return absorption;
}



////////////////////////////////////////////////////////////////////////////////////////
///						SKY
////////////////////////////////////////////////////////////////////////////////////////



RGBAColor Sky_GetColorValue( Sky __global const *This ,uchar4 __global __const *global__texturesData, Ray3D const *r)
{
	/*
	float t;

	// Sol
	if(!r->positiveDirection[2])
	{
		t = - r->origin.z / r->direction.z;
		float xSol = r->origin.x + t * r->direction.x;
		float ySol = r->origin.y + t * r->direction.y;

		return Sky_GetFaceColorValue( This, global__texturesData, 5, xSol*This->groundScale , ySol*This->groundScale );
	}
	*/

	// Ciel

	float x = This->cosRotationAngle * r->direction.x - This->sinRotationAngle * r->direction.y;
	float y = This->sinRotationAngle * r->direction.x + This->cosRotationAngle * r->direction.y;
	float z = r->direction.z;

	int faceId = 0;
	float u = 0, v = 0;

	if( fabs(z) > fabs(x) && fabs(z) > fabs(y) )
	{
		if( z > 0)
		{
			faceId = 5;
			u = (1-x/z)/2;
			v = (1+y/z)/2;
		}
		else
		{
			faceId = 0;
			u = (1-x/z)/2;
			v = (1+y/z)/2;
		}
	}
	else if( fabs(x) > fabs(y) && fabs(x) > fabs(z) )
	{
		if( x > 0)
		{
			faceId = 1;
			u = (1-y/x)/2;
			v = (1+z/x)/2;
		}
		else
		{
			faceId = 3;
			u = (1-y/x)/2;
			v = (1-z/x)/2;
		}
	}
	else if( fabs(y) > fabs(x) && fabs(y) > fabs(z) )
	{
		if( y > 0)
		{
			faceId = 4;
			u = (1+x/y)/2;
			v = (1+z/y)/2;
		}
		else
		{
			faceId = 2;
			u = (1+x/y)/2;
			v = (1-z/y)/2;
		}
	}

	return Sky_GetFaceColorValue( This, global__texturesData, faceId, u , v );
}

RGBAColor Sky_GetFaceColorValue( Sky __global const *This, uchar4 __global const *global__texturesData, int faceId, float u, float v)
{
	RGBAColor rgba = Texture_GetPixelColorValue(&This->skyTextures[faceId], faceId, global__texturesData, u, v);

	/*
	if(faceId != 4)
	{
		int expo = (255.f * (1.f - rgba.w)) - This->exposantFactorX;
		float coeff = pow(This->exposantFactorY, expo);
		rgba *= coeff;
	}
	rgba.w = 0;
	*/

	return rgba;
}


////////////////////////////////////////////////////////////////////////////////////////
///						TRIANGLE
////////////////////////////////////////////////////////////////////////////////////////

bool Triangle_Intersects(Texture __global const *global__textures, Material __global const *global__materiaux, uchar4 __global const *global__texturesData, Triangle const *This, Ray3D const *r, float *squaredDistance, float4 *intersectionPoint, Material *intersectedMaterial, RGBAColor *intersectionColor, float *sBestTriangle, float *tBestTriangle)
{
	// REMARQUE : Les triangles sont orientés avec la normale
	//		--> Pas d'intersection si le rayon vient de derrière...

	const float4 u = This->S2 - This->S1;	// Cotes des triangles
	const float4 v = This->S3 - This->S1;	// Cotes des triangles

	const float d = dot(This->N, This->S1);					// Coordonnée d du plan : n.x = d

	const float nd = dot(This->N, r->direction);

	if( (nd > -0.00001f) && (nd < 0.00001f) )			// Rayon parallele au triangle
		return false;

	const float4 q = r->origin + (r->direction*((d-dot(This->N, r->origin))/nd)); // Point d'intersection du rayon dans le plan du triangle

	float4 fullRay = q - r->origin;
	const float newSquaredDistance = Vector_SquaredNorm( &fullRay );

	if(newSquaredDistance > *squaredDistance) // Trop loin
		return false;
	if(newSquaredDistance < 0.00001f) //Trop près
		return false;

	const float4 w = q - This->S1;

	float uv = dot(u,v),
		wv = dot(w,v),
		wu = dot(w,u),
		uu = dot(u,u),
		vv = dot(v,v);

	float denom = 1/(uv*uv-uu*vv);		//Dénominateur commun aux deux coordonnées s,t

	float s = (uv*wv - vv*wu)*denom;
	float t = (uv*wu - uu*wv)*denom;

	if( s < 0 || t < 0 || s+t > 1)		// le point est à l'extérieur du triangle
		return false;

	if( dot(fullRay, r->direction) < 0)
		return false;					// Le triangle est derriere nous (cas particulier à traiter si l'origine du rayon est dans la feuille du BVH)... Peut-être à enlever.

	Material const mat				= nd < 0  ? global__materiaux[This->materialWithPositiveNormalIndex] : global__materiaux[This->materialWithNegativeNormalIndex];
	RGBAColor const materialColor	= Triangle_GetColorValueAt(global__textures, global__materiaux, global__texturesData, This, nd < 0 ,s,t);

	//if(mat.hasAlphaMap && RGBAColor_IsTransparent(&materialColor)) // Transparent
	//	return false;

	*squaredDistance = newSquaredDistance;

	if(intersectedMaterial	!= NULL) *intersectedMaterial = mat;
	if(intersectionColor	!= NULL) *intersectionColor = materialColor;
	//if(intersectionColor	!= NULL) *intersectionColor = RGBACOLOR(1,1,1,1)*dot(r->direction,This->N)*2./3. + 1./3.;
	if(sBestTriangle		!= NULL) *sBestTriangle = s;
	if(tBestTriangle		!= NULL) *tBestTriangle = t;
	if(intersectionPoint	!= NULL) *intersectionPoint = q;

	return true;
}

RGBAColor Triangle_GetColorValueAt(Texture __global const *global__textures, Material __global const *global__materiaux, uchar4 __global const *global__texturesData, Triangle const *This, bool positiveNormal, float s, float t)
{
	return RGBACOLOR(0.8,0.8,0.8,1);
	//Material const mat = positiveNormal ? global__materiaux[This->materialWithPositiveNormalIndex] : global__materiaux[This->materialWithNegativeNormalIndex];

	//if(mat.isSimpleColor)
	//	return mat.simpleColor;

	//float2 uvText = positiveNormal ? (This->UVP1*(1-s-t)) + (This->UVP2*s) + (This->UVP3*t) : (This->UVN1*(1-s-t)) + (This->UVN2*s) + (This->UVN3*t);
	//return Texture_GetPixelColorValue( &global__textures[mat.textureId], mat.textureId + 6, global__texturesData, uvText.x, uvText.y );
}

float4 Triangle_GetSmoothNormal(Triangle const *This, bool positiveNormal, float s, float t)
{
	return This->N;
	/*
	float4 smoothNormal = normalize( (This->N2 * s) + (This->N3 * t) + (This->N1 * (1 - s - t)) );
	if(positiveNormal)
		return smoothNormal;
	return -smoothNormal;
	*/
}

////////////////////////////////////////////////////////////////////////////////////////
///						BVH
////////////////////////////////////////////////////////////////////////////////////////

/*	Fonction qui teste si un rayon intersecte le BVH
 *	La traversée de l'arbre se fait par paquet avec le groupe entier
 */

bool BVH_IntersectRay(KERNEL_GLOBAL_VAR_DECLARATION, const Ray3D *r, float4 *intersectionPoint, float *s, float *t, Triangle *intersetedTriangle, Material *intersectedMaterial, RGBAColor *intersectionColor)
{
	float squaredDistance = INFINITY;

	bool hasIntersection = false;	
	int currentReadStackIdx = -1;
	Node const __global *currentNode = &global__bvh[0];
	Node const __global *stack[BVH_MAX_DEPTH];

	Node const __global *son1;
	Node const __global *son2;

	while(true)	// On sort de la boucle lorsque l'on dépile le stack et que ce dernier est vide
	{
		if(currentNode->isLeaf)
		{
			// Si on entre dans une feuille, on teste notre rayon contre tous les trirangles de la feuille

			for(uint i=currentNode->triangleStartIndex; i < currentNode->triangleStartIndex+currentNode->nbTriangles; i++)
			{
				Triangle triangle = global__triangulation[i];
				if(Triangle_Intersects(global__textures, global__materiaux, global__texturesData, &triangle, r, &squaredDistance, intersectionPoint, intersectedMaterial, intersectionColor, s, t))
				{
					*intersetedTriangle = triangle;
					hasIntersection = true;
				}
			}

			//	On a finit de tester le rayon contre la feuille, il faut dépiler, sauf si la pile est vide

			if(currentReadStackIdx < 0) // La pile est vide
			{
				break;
			}

			currentNode = stack[currentReadStackIdx];
			currentReadStackIdx--;
		}
		else
		{
			if(r->positiveDirection[currentNode->cutAxis])
			{
				son1 = &global__bvh[currentNode->son1Id];
				son2 = &global__bvh[currentNode->son2Id];
			}
			else
			{
				son2 = &global__bvh[currentNode->son1Id];
				son1 = &global__bvh[currentNode->son2Id];
			}

			BoundingBox bb1 = son1->trianglesAABB;
			BoundingBox bb2 = son2->trianglesAABB;
			bool b1 = BoundingBox_Intersects( &bb1, r, squaredDistance);
			bool b2 = BoundingBox_Intersects( &bb2, r, squaredDistance);

			if(b1)
			{
				if(b2)
				{
					currentNode = son1;
					stack[++currentReadStackIdx] = son2;
				}
				else
				{
					currentNode = son1;
				}
			}
			else if(b2)
			{
				currentNode = son2;
			}
			else
			{
				if(currentReadStackIdx < 0)
					break;
				currentNode = stack[currentReadStackIdx--];
			}
		}
	}

	return hasIntersection;
}


bool BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR_DECLARATION, const Ray3D *r, float squaredDistance, RGBAColor *tint)
{
	float4 *intersectionPoint = NULL;
	Material *intersectedMaterial = NULL;
	RGBAColor *intersectionColor = NULL;
	float *s = NULL;
	float *t = NULL;

	bool hasIntersection = false;	
	int currentReadStackIdx = -1;
	Node const __global *currentNode = &global__bvh[0];
	Node const __global *stack[BVH_MAX_DEPTH];

	Node const __global *son1;
	Node const __global *son2;

	while(true)	// On sort de la boucle lorsque l'on dépile le stack et que ce dernier est vide
	{
		if(currentNode->isLeaf)
		{
			// Si on entre dans une feuille, on teste notre rayon contre tous les trirangles de la feuille

			for(uint i=currentNode->triangleStartIndex; i < currentNode->triangleStartIndex+currentNode->nbTriangles; i++)
			{
				Triangle triangle = global__triangulation[i];
				if(Triangle_Intersects(global__textures, global__materiaux, global__texturesData, &triangle, r, &squaredDistance, intersectionPoint, intersectedMaterial, intersectionColor, s, t))
				{
					return true;
				}
			}

			//	On a finit de tester le rayon contre la feuille, il faut dépiler, sauf si la pile est vide

			if(currentReadStackIdx < 0) // La pile est vide
				break;

			currentNode = stack[currentReadStackIdx];
			currentReadStackIdx--;
		}
		else
		{

			if(r->positiveDirection[currentNode->cutAxis])
			{
				son1 = &global__bvh[currentNode->son1Id];
				son2 = &global__bvh[currentNode->son2Id];
			}
			else
			{
				son2 = &global__bvh[currentNode->son1Id];
				son1 = &global__bvh[currentNode->son2Id];
			}

			BoundingBox bb1 = son1->trianglesAABB;
			BoundingBox bb2 = son2->trianglesAABB;
			bool b1 = BoundingBox_Intersects( &bb1, r, squaredDistance);
			bool b2 = BoundingBox_Intersects( &bb2, r, squaredDistance);

			if(b1)
			{
				if(b2)
				{
					currentNode = son1;
					stack[++currentReadStackIdx] = son2;
				}
				else
				{
					currentNode = son1;
				}
			}
			else if(b2)
			{
				currentNode = son2;
			}
			else
			{
				if(currentReadStackIdx < 0)
					break;
				currentNode = stack[currentReadStackIdx--];
			}
		}
	}

	return hasIntersection;
}



////////////////////////////////////////////////////////////////////////////////////////
///						SCENE
////////////////////////////////////////////////////////////////////////////////////////

RGBAColor Scene_ComputeRadiance(KERNEL_GLOBAL_VAR_DECLARATION, const float4 *p, Ray3D *r, Triangle const *triangle, Material const * mat, RGBAColor const * materialColor, float s, float t, RGBAColor const *directIlluminationRadiance, RGBAColor* transferFunction, float4 const *Ng,  float4 const *Ns)
{
	float4 N = r->direction;

	RGBAColor radianceToCompute = RGBACOLOR(0,0,0,0);

	float4 outDirection = r->direction;
	/*
	if(mat->type == MAT_STANDART)
	{
		*transferFunction *= (*materialColor);
		radianceToCompute = (*directIlluminationRadiance) * (*transferFunction);
		outDirection = Material_CosineSampleHemisphere(seed, Ns);
		N = *Ns;
	}
	else if(mat->type == MAT_GLASS)
	{
		float rFresnel = Material_FresnelGlassReflectionFraction(mat, r, Ns);

		//	Reflection
		if( random(seed) < rFresnel )
		{
			outDirection = Material_FresnelReflection(mat, &r->direction, Ns);
			N = *Ng;
		}
		else
		{
			*transferFunction *= (*materialColor) * (1 - mat->opacity);
			N = r->direction;
		}
	}

	else if(mat->type == MAT_WATER)
	{
		float4 refractedDirection;
		float refractionMultCoeff;
		float rFresnel = Material_FresnelWaterReflectionFraction(mat, r, Ns, &refractedDirection, &refractionMultCoeff);

		//	Reflection
		if(random(seed) < rFresnel)
		{
			outDirection = Material_FresnelReflection(mat, &r->direction, Ns);
			N = *Ng;
		}
		else //	Refraction
		{
			r->isInWater = !r->isInWater;
			outDirection = refractedDirection;
			N = -(*Ng);
			*transferFunction *= refractionMultCoeff;
		}
	}
	*/

	*transferFunction *= (*materialColor);
	radianceToCompute = (*directIlluminationRadiance) * (*transferFunction);
	outDirection = Material_CosineSampleHemisphere(seed, Ns);
	N = *Ns;

	Vector_PutInSameHemisphereAs(&outDirection, &N);
	Ray3D_SetDirection(r, &outDirection);
	r->origin = *p + 0.01f * outDirection;

	return radianceToCompute;



	//if(mat.type == MAT_VARNHISHED)
	//{

	//	//	Illumination directe
	//	*radianceToCompute += directIlluminationRadiance * materialColor * (*transferFunction);

	//	float4 refractedDirection;
	//	float rFresnel1 = Material_FresnelVarnishReflectionFraction(&mat, r, &Ns, false, &refractedDirection);

	//	//	Reflection
	//	if(random(seed) < rFresnel1)
	//	{
	//		*transferFunction *= 0.8f;

	//		r->origin = *p;
	//		outDirection = Material_FresnelReflection(&mat, &r->direction, &Ns);
	//		Vector_PutInSameHemisphereAs(&outDirection, &Ng);
	//		Ray3D_SetDirection(r, &outDirection);
	//		return;
	//	}

	//	//	Refraction (ignorée) + rebond diffus + refraction
	//	r->origin = *p;

	//	float rFresnel2;
	//	float4 NsOposite = -Ns;

	//	outDirection = Material_CosineSampleHemisphere(seed, &Ns);
	//	Ray3D_SetDirection(r, &outDirection);

	//	rFresnel2 = Material_FresnelVarnishReflectionFraction(&mat, r, &NsOposite, true, &outDirection);
	//	Vector_PutInSameHemisphereAs(&outDirection, &Ng);
	//	Ray3D_SetDirection(r, &outDirection);

	//	*transferFunction *= materialColor * (1-rFresnel2);

	//	return;
	//}

	//if(mat.type == MAT_METAL)
	//{
	//	ASSERT(false);
	//}

	//ASSERT(false);

}

/*	Cette fonction calcule l'illumination directe au point d'intersection *p
 *	Il y a 2 étapes
 *		1 - la contribution des lampes de la scène
 *		2 - la conttibution du soleil qui est traitée à part
 *			- Pour des scènes en intérieur, il est mieux de ne traiter que rarement la contribution du soleil
 *			- Pour des scène en exterieur, il est préférable de tout le temps tester le soleil
 */

RGBAColor Scene_ComputeDirectIllumination(KERNEL_GLOBAL_VAR_DECLARATION, const float4 *p, const Ray3D *cameraRay, Material const *mat, const float4 *N)
{
	RGBAColor L		= RGBACOLOR(0,0,0,0);				//	Radiance à calculer
	RGBAColor tint	= RGBACOLOR(1,1,1,1);				//	Teinte lors d'un shadow ray
	Light light;
	float lightContribution;

	const bool sampleLights = false;
	// Lampes
	if(sampleLights)
	{
		if(global__lightsSize > 0 && Scene_PickLight(KERNEL_GLOBAL_VAR, p, cameraRay, mat, N, &light, &lightContribution))
		{
			L = RGBACOLOR(0.5,0.5,0.5,1);

			//Ray3D lightRay;
			//float4 fullRay = light.position - (*p);
			//Ray3D_Create(&lightRay, p, &fullRay, cameraRay->isInWater);
			//
			//if(!BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR, &lightRay, Vector_SquaredDistanceTo(&light.position, p), &tint))		//Lumière visible
			//	L += light.color * tint * lightContribution;
		}
	}
	else
	{
		Light light;
		float BRDF;
		if(global__lightsSize > 0)
		{
			printf("Test");
			for(uint i = 0; i < global__lightsSize; i++)
			{
				Ray3D lightRay;
				float4 fullRay =  LIGHT_DIRECTIONNAL ? -light.direction : light.position - (*p);
				Ray3D_Create(&lightRay, p, &fullRay, cameraRay->isInWater);

				light = global__lights[i];

				float lightDistance = light.type == LIGHT_DIRECTIONNAL ? INFINITY : length(fullRay);
				float4 oppositeDirection = - lightRay.direction;
				BRDF = Material_BRDF(mat, &oppositeDirection, N, &cameraRay->direction);

				if( !BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR, &lightRay, 0, &tint) )
					L += Light_PowerToward(&light, p, N) * BRDF * tint;
			}
		}
	}

	ASSERT(L.x >= 0 && L.y >= 0 && L.z >= 0);

	return L;

	////	Soleil

	//tint = RGBACOLOR(1,1,1,1);
	//Ray3D sunRay;
	//float4 sunOpositeDirection = - global__sun->direction;
	//Ray3D_Create(&sunRay, p, &sunOpositeDirection, cameraRay->isInWater);
	//float G = dot(sunRay.direction, *N);
	//float BRDF = Material_BRDF( mat, &sunRay, N, cameraRay);

	//ASSERT( BRDF >= 0 );


	//if(false)	// Si on veut smapler le soleil selon sa contribution potentielle, mettre ce test à true
	//{
	//	if( (random(seed) < (G * BRDF)) && !BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR, &sunRay, INFINITY, &tint))
	//	{
	//		RGBAColor LSun = global__sun->color * global__sun->power * tint;
	//		L += LSun;
	//	}
	//}
	//else	// Le soleil est sampler tout le temps
	//{
	//	if( (G * BRDF) > 0 && !BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR, &sunRay, INFINITY, &tint))
	//	{
	//		RGBAColor LSun = global__sun->color * global__sun->power * tint;
	//		L += LSun * (G * BRDF);
	//	}
	//}

	//ASSERT(L.x >= 0 && L.y >= 0 && L.z >= 0);

	//return L;
}

/*	Calcule une contribution suplémentaire dans l'eau par in-scaterring
 *	
 *
 */
RGBAColor Scene_ComputeScatteringIllumination(KERNEL_GLOBAL_VAR_DECLARATION, const Ray3D *cameraRay, const float4 *intersectionPoint)
{
	RGBAColor L = RGBACOLOR(0,0,0,0);
	return L;

	//if(MATERIAL_WATER_SCATTERING_PART == 0)
	//	return L;

	//Material water;
	//Material_Create(&water, MAT_WATER);

	//const uint nSample = 1;	//	Nombre de points d'entrée de la lumière extérieure
	//RGBAColor water_absorption = MATERIAL_WATER_ABSORPTION_COLOR;
	//float sigmaTotalMean = Vector_Mean(&water_absorption);

	//for(uint i=0; i < nSample; i++)
	//{

	//	RGBAColor transferFunction = RGBACOLOR(1,1,1,1);

	//	float4 scatteringPoint = cameraRay->origin;
	//	float fullRayDist = distance(*intersectionPoint, cameraRay->origin);
	//	float xsi = random(seed);
	//	xsi = (0.98f * xsi) + 0.01f; // On met xsi entre 0.01 et 0.99

	//	//	On met xsi à l'échelle de l'abosption en exponentielle de l'eau pour obtenir le point où nous calculerons une contribution supplémentaire
	//	float scatteringRayDist = - ( 1.0f / sigmaTotalMean ) * log( 1 - (1 - exp( - sigmaTotalMean * fullRayDist ) ) * xsi );

	//	//	pdfScatteringPointInverse est l'inverse de la probabilité d'avoir tiré ce point
	//	float pdfScatteringPointInverse = ( 1 - exp( - sigmaTotalMean * fullRayDist ) ) / ( sigmaTotalMean * exp( - sigmaTotalMean * scatteringRayDist ) );

	//	ASSERT(scatteringRayDist/fullRayDist >= 0 && scatteringRayDist/fullRayDist <= 1);

	//	// Calcul du point de in-scaterring
	//	scatteringPoint += ((*intersectionPoint) - cameraRay->origin) * (scatteringRayDist / fullRayDist);

	//	//	Absorption da la contribution jusqu'au point d'intersection
	//	transferFunction *= Material_WaterAbsorption(scatteringRayDist) * (MATERIAL_WATER_SCATTERING_PART * pdfScatteringPointInverse);

	//	// Calcul de l'Illumination directe du Soleil au point scatteringPoint

	//	RGBAColor tint = RGBACOLOR(1,1,1,1);
	//	Ray3D sunRay;			//Pointing toward the sun
	//	Ray3D sunOpositeRay;	//Pointing away from the sun
	//	float4 sunOpositeDirection = -global__sun->direction;
	//	Ray3D_Create( &sunRay, &scatteringPoint, &sunOpositeDirection, cameraRay->isInWater);
	//	float BRDF;

	//	if( !BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR, &sunRay, INFINITY, &tint) )
	//	{
	//		sunOpositeRay = Ray3D_Opposite(&sunRay);
	//		BRDF = Material_BRDF(&water, &sunOpositeRay, NULL, cameraRay);
	//		RGBAColor LSun = global__sun->color * tint * global__sun->power;
	//		L += LSun * transferFunction * BRDF;
	//	}


	//	//	Calcul de l'Illumination directe des lumières au point scatteringPoint
	//	//	Cette illumination est faite de manière brutale pour toutes les lampes.
	//	//	La fonctionnalité ne compilant pas, je n'ai pas fait les amélioration nécessaire...

	//	Ray3D lightRay;
	//	Ray3D lightOpositeRay;
	//	Light light;

	//	for(uint il=0; il<global__lightsSize; il++)
	//	{
	//		light = global__lights[il];
	//		tint = RGBACOLOR(1,1,1,1);
	//		float4 fullRay = light.position - scatteringPoint;
	//		Ray3D_Create(&lightRay, &scatteringPoint, &fullRay, cameraRay->isInWater);

	//		if( !BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR, &lightRay, INFINITY, &tint) )
	//		{
	//			lightOpositeRay = Ray3D_Opposite(&lightRay);
	//			BRDF = Material_BRDF(&water, &lightOpositeRay, NULL, cameraRay);
	//			float lightPower = Light_PowerToward( &light, &lightOpositeRay.direction) / Vector_SquaredDistanceTo(&light.position, &scatteringPoint);

	//			RGBAColor LLight = light.color * tint * lightPower;
	//			L += LLight * transferFunction * BRDF;
	//		}
	//	}
	//}

	//ASSERT(L.x >= 0 && L.y >= 0 && L.z >= 0);

	//return L / ((float) nSample);

}

/*	Cette fonction tire une lampe au hasard selon sa ccontribution potentielle
 */

bool Scene_PickLight(KERNEL_GLOBAL_VAR_DECLARATION, const float4 *p, const Ray3D *cameraRay, Material const *mat, const float4 *N, Light *lightToChoose, float *lightContribution)
{
	
	float lightContributions[MAX_LIGHT_SIZE+1];
	float distribution[MAX_LIGHT_SIZE+1];
	for(uint i=0; i < MAX_LIGHT_SIZE; i++)
	{
		lightContributions[i] = 0.0f;
		distribution[i] = 0.0f;
	}

	Light light;

	// Construction de la distribution de probabilité

	distribution[0] = 0;
	float G;
	float BRDF;
	for(uint i = 0; i < global__lightsSize; i++)
	{
		light = global__lights[i];
		Ray3D lightRay;
		float4 fullRay = (*p) - light.position;
		Ray3D_Create(&lightRay, p, &fullRay, cameraRay->isInWater);
		Ray3D lightOpositeRay = Ray3D_Opposite(&lightRay);

		G = - dot(lightRay.direction, *N ) * Light_PowerToward(&light, p, N); 	// = cos(angle) * power / d²
		if(G < 0)
			G = 0;

		BRDF = Material_BRDF(mat, &lightOpositeRay.direction, N, &cameraRay->direction);

		lightContributions[i] = G * BRDF;

		distribution[i+1] = distribution[i] + lightContributions[i];
	}

	if( distribution[global__lightsSize] <= 0.0f ) // Pas d'éclairage directe
		return false;

	// Tirage au sort
	float x = random(seed) * distribution[global__lightsSize];

	// Recherche de la lampe tirée par dichotomie
	int lightIndex = global__lightsSize/2;
	uint step = (uint) (global__lightsSize/4);
	while( (lightIndex >= 0 && lightIndex < MAX_LIGHT_SIZE+1) && (distribution[lightIndex] > x || distribution[(lightIndex)+1] <= x )) 
	{
		if(distribution[lightIndex] > x)
			lightIndex -= step;
		else
			lightIndex += step;
		step /= 2;
		if(step==0)
			step = 1;
	}

	*lightToChoose = global__lights[lightIndex];
	*lightContribution = lightContributions[lightIndex];
	return true;
}


////////////////////////////////////////////////////////////////////////////////////////
///						KERNEL
////////////////////////////////////////////////////////////////////////////////////////


__kernel void Kernel_Main(
	uint     kernel__iterationNum,
	uint     kernel__imageWidth,
	uint     kernel__imageHeight,

	float4  kernel__cameraPosition,
	float4  kernel__cameraDirection,
	float4  kernel__cameraRight,
	float4  kernel__cameraUp,

	uint     global__lightsSize,

	float4	__global			*global__imageColor,
	uint	__global			*global__imageRayNb,
	void	__global	const	*global__void__bvh,
	void	__global	const	*global__void__triangulation,
	void	__global	const	*global__void__lights,
	void	__global	const	*global__void__materiaux,
	void	__global	const	*global__void__textures,
	uchar4	__global	const	*global__texturesData,
	void	__global	const	*global__void__sky
	)
{

	///////////////////////////////////////////////////////////////////////////////////////
	///					INITIALISATION
	///////////////////////////////////////////////////////////////////////////////////////


	PRINT_DEBUG_INFO2("KERNEL MAIN START", , 0);

	Ray3D r;
	int globalImageOffset;
	int seedValue;
	int *seed = &seedValue;

	{
		uint xPixel = get_global_id(0);
		uint yPixel = get_global_id(1);

		globalImageOffset = yPixel * kernel__imageWidth + xPixel;

		if(xPixel != 0 || yPixel != 0)
			return;

		//PRINT_DEBUG_INFO2("KERNEL MAIN END", (uint2) (xPixel, yPixel) , (uint2) (xPixel, yPixel));

		*seed = InitializeRandomSeed(xPixel, yPixel, kernel__imageWidth, kernel__imageHeight, kernel__iterationNum);

		float xScreen = (xPixel + random(seed) - 0.5) / (float) kernel__imageWidth  - 0.5;
		float yScreen = (yPixel + random(seed) - 0.5) / (float) kernel__imageHeight - 0.5;

		float4 shotDirection = kernel__cameraDirection + ( kernel__cameraRight * xScreen ) + ( kernel__cameraUp * yScreen );
		Ray3D_Create( &r, &kernel__cameraPosition, &shotDirection, false);
	}


	///////////////////////////////////////////////////////////////////////////////////////
	///					INITIALISATION DES VARIABLES DE PARCOURS
	///////////////////////////////////////////////////////////////////////////////////////

	//	On convertit les pointeurs vers les pointeurs de bonne dimension.
	//	La raison est qu'on ne peut passer à un noyau que des pointeurs vides
	Node		__global	const *global__bvh				= (Node		__global	const *) global__void__bvh;
	Triangle	__global	const *global__triangulation	= (Triangle __global	const *) global__void__triangulation;
	Light		__global	const *global__lights			= (Light	__global	const *) global__void__lights;
	Material	__global	const *global__materiaux		= (Material	__global	const *) global__void__materiaux;
	Texture		__global	const *global__textures			= (Texture	__global	const *) global__void__textures;
	Sky			__global	const *global__sky				= (Sky		__global	const *) global__void__sky;


	//	Variable utiles lors du parcours de l'arbre
	RGBAColor radianceToCompute = RGBACOLOR(0,0,0,0);
	RGBAColor transferFunction  = RGBACOLOR(1,1,1,1);
	float4 intersectionPoint   = FLOAT4  (0,0,0,0);
	RGBAColor intersectionColor = RGBACOLOR(0,0,0,0);
	float s = 0.0f, t = 0.0f;
	Triangle intersectedTriangle;
	Material intersectedMaterial;

	bool activeRay = true;
	int reflectionId = 0;

	while(activeRay && reflectionId < MAX_REFLECTION_NUMBER)
	{
		PRINT_DEBUG_INFO2("reflection Id", "reflectionId = %i", reflectionId);
		printf("radianceToCompute : %v4f \n", radianceToCompute);
		return;
		if(BVH_IntersectRay(KERNEL_GLOBAL_VAR, &r, &intersectionPoint, &s, &t, &intersectedTriangle, &intersectedMaterial, &intersectionColor))
		{
			//radianceToCompute += intersectionColor * transferFunction;
			//transferFunction *= intersectionColor;
			//radianceToCompute = RGBACOLOR(1,1,1,1)*fabs(dot(r.direction,intersectedTriangle.N));

			//if(r.isInWater)
			//{
			//	*radianceToCompute += Scene_ComputeScatteringIllumination(KERNEL_GLOBAL_VAR, r, &intersectionPoint) * (*transferFunction);

			//	float dist = distance(intersectionPoint, r->origin);
			//	*transferFunction *= Material_WaterAbsorption(dist);
			//}

			bool areRayAndNormalInSameDirection		= ( dot(r.direction, intersectedTriangle.N) > 0 );

			float4 const Ng = Triangle_GetNormal(&intersectedTriangle, !areRayAndNormalInSameDirection);

			float4 Ns = Triangle_GetSmoothNormal(&intersectedTriangle, !areRayAndNormalInSameDirection,s,t);
			float4 rOpositeDirection = - r.direction;
			Vector_PutInSameHemisphereAs(&Ns, &rOpositeDirection);
			Ns = normalize(Ns);
			ASSERT(dot(r.direction, Ns) < 0 && dot(r.direction, Ng) < 0);
			
			RGBAColor directIlluminationRadiance = Scene_ComputeDirectIllumination(KERNEL_GLOBAL_VAR, &intersectionPoint, &r, NULL, &Ng);
			//RGBAColor directIlluminationRadiance = RGBACOLOR(0,0,0,0);
			
			radianceToCompute += Scene_ComputeRadiance(KERNEL_GLOBAL_VAR, &intersectionPoint, &r, &intersectedTriangle, NULL, &intersectionColor, s, t, &directIlluminationRadiance, &transferFunction, &Ng, &Ns);
			//radianceToCompute += RGBACOLOR(0.05,0.1,0.05,1);
			reflectionId++;
		}
		else // Sky
		{
			activeRay = false;

			//RGBAColor skyColor = Sky_GetColorValue( global__sky, global__texturesData, &r );
			//radianceToCompute += (skyColor * transferFunction);
		}

		///////////////////////////////////////////////////////////////////////////////////////
		///					MISE A JOUR DE L'ACTIVITE DU RAYON
		///////////////////////////////////////////////////////////////////////////////////////

		float maxContribution;

		if(activeRay)
		{ // Max contribution
			maxContribution = Vector_Max(&transferFunction);
			if(maxContribution <= MIN_CONTRIBUTION_VALUE)
			{
				activeRay = false;
			}
		}

		//if( RUSSIAN_ROULETTE && activeRay && reflectionId > MIN_REFLECTION_NUMBER )
		//{
		//	float russianRouletteCoeff = maxContribution/(reflectionId - MIN_REFLECTION_NUMBER);
		//	if( russianRouletteCoeff < 1)
		//	{
		//		activeRay &= random(seed) > russianRouletteCoeff;
		//		transferFunction /= russianRouletteCoeff;
		//	}
		//}
	}

	// Si le rayon est termine

	global__imageRayNb[globalImageOffset]++;
	//global__imageColor[globalImageOffset] += radianceToCompute;
	global__imageColor[globalImageOffset] += RGBACOLOR(1,1,1,1);

}
