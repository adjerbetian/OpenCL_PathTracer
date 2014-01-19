

//#include "PathTracer_FullKernel_header.cl"
#include "C:\Users\djerbeti\Documents\Visual Studio 2012\Projects\OpenCL_PathTracer\src\Kernel\PathTracer_FullKernel_header.cl"

////////////////////////////////////////////////////////////////////////////////////////
///						Utilitaires
////////////////////////////////////////////////////////////////////////////////////////

/*	Somme prefixe parallele sur le tableau local *values
 *	Le tableau *values doit être de la taille du groupe
 *
 *	En sorti, on obtient le tableau dont chaque valeur est 
 *	la somme des valeurs précédentes dans le tableau d'entrée
 */
void PrefixSum_int(int __local volatile *values)
{
	PRINT_DEBUG_INFO_1("PrefixSum START", "value : %u", values[get_local_id(0)]);

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

	PRINT_DEBUG_INFO_1("PrefixSum END", "value : %u", values[get_local_id(0)]);
}

void PrefixSum_uint(uint __local volatile *values)
{
	PRINT_DEBUG_INFO_1("PrefixSum START", "value : %u", values[get_local_id(0)]);

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

	PRINT_DEBUG_INFO_1("PrefixSum END", "value : %u", values[get_local_id(0)]);
}



////////////////////////////////////////////////////////////////////////////////////////
///						BOUNDING BOX
////////////////////////////////////////////////////////////////////////////////////////

bool BoundingBox_Intersects ( BoundingBox const *This, Ray3D *r, const float squaredDistance)
{
	r->numIntersectedBBx++;

	if(This->isEmpty)
		return false;

	float tMin, tMax;
	float tyMin, tyMax, tzMin, tzMax;

	if(r->direction.x>0)
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

	if(r->direction.y>0)
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

	if(r->direction.z>0)
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
float Material_BRDF( Material const *This, float4 const *incidentDirection, float4 const *N, float4 const *reflectedDirection)
{
	if(This->type == MAT_STANDART)
		return PATH_PI_INVERSE;

	if(This->type == MAT_GLASS)
		return 1;
	
	if(This->type == MAT_WATER)		//	Scattering
	{
		//	Schlick model
		float denom = 1 + MATERIAL_KSCHLICK * dot(*incidentDirection, *reflectedDirection);
		return ( 1 - MATERIAL_KSCHLICK*MATERIAL_KSCHLICK ) / ( 4 * PATH_PI * denom * denom );
	}
	
	if(This->type == MAT_VARNHISHED)
	{
		float rFresnel1 = Material_FresnelVarnishReflectionFraction(This, incidentDirection, N, false, NULL);
		return (1 - rFresnel1) * PATH_PI_INVERSE;
	}

	ASSERT("Material_BRDF - Non standart material", false);
	return 1;

};

float Material_FresnelGlassReflectionFraction( Material const *This, float4 const* incidentDirection, float4 const *N)
{
	ASSERT("Material_FresnelGlassReflectionFraction not glass", This->type == MAT_GLASS);

	float n1, n2;

	n1 = 1;						// We suppose we are not in water
	n2 = MATERIAL_N_GLASS;

	float cos1 = - dot(*incidentDirection, *N);
	float sin1 = sqrt(1 - cos1 * cos1);
	float sin2 = n1 * sin1 / n2;
	if(sin2 >= 1) // Totalement réfléchi
		return 1;
	float cos2 = sqrt(1 - sin2*sin2);

	float rPara = ( n2 * cos1 - n1 * cos2 ) / ( n2 * cos1 + n1 * cos2 );
	float rPerp = (	n1 * cos1 - n2 * cos2 ) / ( n1 * cos1 + n2 * cos2 );

	float rFresnel = (rPara*rPara + rPerp*rPerp)/2.0f;

	ASSERT("Material_FresnelGlassReflectionFraction incorrect rFresnel", rFresnel <= 1);

	return rFresnel;

}

float Material_FresnelWaterReflectionFraction( Material const *This, float4 const* incidentDirection, float4 const *N, bool alreadyInWater, float4 *refractionDirection, float *refractionMultCoeff)
{
	float n1, n2;

	if(alreadyInWater)
	{
		n1 = MATERIAL_N_WATER;
		n2 = 1;
	}
	else
	{
		n1 = 1;
		n2 = MATERIAL_N_WATER;
	}

	float cos1 = - dot(*incidentDirection, *N);
	float sin1 = sqrt(1 - cos1 * cos1);
	float sin2 = n1 * sin1 / n2;
	if(sin2 >= 1) // Totalement réfléchi
		return 1;
	float cos2 = sqrt(1 - sin2*sin2);

	float rPara = ( n2 * cos1 - n1 * cos2 ) / ( n2 * cos1 + n1 * cos2 );
	float rPerp = (	n1 * cos1 - n2 * cos2 ) / ( n1 * cos1 + n2 * cos2 );

	float rFresnel = (rPara*rPara + rPerp*rPerp)/2.0f;

	ASSERT("Material_FresnelWaterReflectionFraction incorrect rFresnel", rFresnel <= 1);

	if(refractionDirection != NULL)
		*refractionDirection = *incidentDirection * (n1/n2) + (*N) * (n1/n2 * cos1 - cos2);
	if(refractionMultCoeff != NULL)
		*refractionMultCoeff = (n2*n2)/(n1*n1);

	return rFresnel;
}

float Material_FresnelVarnishReflectionFraction( Material const *This, float4 const* incidentDirection , float4 const *N, bool isInVarnish, float4 *refractionDirection)
{
	ASSERT("VARNISH REFLECTION FRACTION - material not varnished", This->type == MAT_VARNHISHED);
	WARNING_AND_INFO("VARNISH REFLECTION FRACTION - incorrect incident direction", dot(*incidentDirection, *N) <= 0, "dot(*incidentDirection, *N) = %f", dot(*incidentDirection, *N) );

	float n1, n2;

	if(isInVarnish)
	{
		n1 = MATERIAL_N_VARNISH;
		n2 = 1.0f; //We supppose we are not in water
	}
	else
	{
		n1 = 1.0f; //We supppose we are not in water
		n2 = MATERIAL_N_VARNISH;
	}

	float cos1 = fmax(0.f, fmin(1.f , - dot(*incidentDirection, *N) ));
	float sin1 = sqrt(1 - cos1 * cos1);
	float sin2 = n1 * sin1 / n2;
	if(sin2 >= 1) // Totalement réfléchi
		return 1;
	float cos2 = sqrt(1 - sin2*sin2);

	float rPara = ( n2 * cos1 - n1 * cos2 ) / ( n2 * cos1 + n1 * cos2 );
	float rPerp = (	n1 * cos1 - n2 * cos2 ) / ( n1 * cos1 + n2 * cos2 );

	float rFresnel = (rPara*rPara + rPerp*rPerp)/2.0f;

	ASSERT_AND_INFO("Material_FresnelVarnishReflectionFraction incorrect rFresnel", rFresnel <= 1, "rFresnel : %f", rFresnel);

	if(refractionDirection != NULL)
		*refractionDirection = *incidentDirection * (n1/n2) + (*N) * (n1/n2 * cos1 - cos2);

	return rFresnel;
}

float4 Material_FresnelReflection( Material const *This, float4 const *v, float4 const *N)
{
	ASSERT("Material_FresnelReflection incorrect material type", This->type != MAT_STANDART);

	float4 reflection = (*v) - ( (*N) * ( 2 * dot(*v, *N) ) );
	return reflection;
}


float4 Material_CosineSampleHemisphere(int *kernel__seed, float4 const *N)
{
	ASSERT("Material_CosineSampleHemisphere normal length not 1", fabs(length(*N) - 1.0f) < 0.0001f);

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

	ASSERT("Material_CosineSampleHemisphere sampled direction incorrect", dot(vWorld, *N) >= 0);

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

	if( fabs(sx) < 0.0001f)
	{
			r = sy;
			theta = 0;
	}
	else if( fabs(sy) < 0.0001f )
	{
			r = sx;
			theta = 2;
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
	theta *= PATH_PI / 4.f;	//	Coefficient commun a tous les cas ci-dessus
	r *= 0.999;				//	Precaution en cas de d'erreur de précision numérique

	//	4 - Calcul des coordonnees dans le cerle

	*dx = r * cos( theta );	
	*dy = r * sin( theta );

	ASSERT_AND_INFO("Material_ConcentricSampleDisk incorrect sample dx dy", (*dx)*(*dx) + (*dy)*(*dy) < 1, "( dx , dy , theta , r, sx, sy, u1, u2 ) : %v8f", (float8)(*dx,*dy, theta, r, sx, sy, u1, u2) );
};


RGBAColor Material_WaterAbsorption(float dist)
{
	if(dist == INFINITY)
		return RGBACOLOR(0.f, 0.f, 0.f, 0.f);

	RGBAColor absorption = exp( - MATERIAL_WATER_ABSORPTION_COLOR * dist );
	ASSERT("Material_WaterAbsorption incorrect absorption", absorption.x >= 0 && absorption.x <= 1 && absorption.y >= 0 && absorption.y <= 1 && absorption.z >= 0 && absorption.z <= 1);

	return absorption;
}



////////////////////////////////////////////////////////////////////////////////////////
///						SKY
////////////////////////////////////////////////////////////////////////////////////////



RGBAColor Sky_GetColorValue( Sky __global const *This ,uchar4 __global __const *global__texturesData, float4 const* direction)
{
	//First, we rotate the sky
	float x = This->cosRotationAngle * (*direction).x - This->sinRotationAngle * (*direction).y;
	float y = This->sinRotationAngle * (*direction).x + This->cosRotationAngle * (*direction).y;
	float z = (*direction).z;

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
			u = (1+x/z)/2;
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
	RGBAColor rgba = Texture_GetPixelColorValue(&This->skyTextures[faceId], global__texturesData, u, v);

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

bool Triangle_Intersects(Texture __global const *global__textures, Material __global const *global__materiaux, uchar4 __global const *global__texturesData, Triangle const *This, Ray3D *r, float *squaredDistance)
{
	PRINT_DEBUG_INFO_2("TRIANGLE_INTERSECTS - START \t\t", "Triangle id : %u", This->id, "r.numIntersectedTri : %u", r->numIntersectedTri);

	r->numIntersectedTri++;

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


	uint matIdx					= nd < 0  ? This->materialWithPositiveNormalIndex : This->materialWithNegativeNormalIndex;
	Material mat				= global__materiaux[matIdx];
	RGBAColor materialColor		= Triangle_GetColorValueAt(global__textures, global__materiaux, global__texturesData, This, nd < 0 ,s,t);

	//Material mat;
	//Material_Create(&mat, MAT_STANDART);
	//RGBAColor materialColor	= Triangle_GetColorValueAt(global__textures, global__materiaux, global__texturesData, This, nd < 0 ,s,t);

	//if(mat.hasAlphaMap && RGBAColor_IsTransparent(&materialColor)) // Transparent
	//	return false;

	*squaredDistance = newSquaredDistance;

	r->intersectedMaterialId = matIdx;
	r->intersectionColor = materialColor;
	//r->intersectionColor = RGBACOLOR(1,1,1,1)*dot(r->direction,This->N)*2./3. + 1./3.;
	r->s = s;
	r->t = t;
	r->intersectionPoint = q;

	return true;
}

RGBAColor Triangle_GetColorValueAt(Texture __global const *global__textures, Material __global const *global__materiaux, uchar4 __global const *global__texturesData, Triangle const *This, bool positiveNormal, float s, float t)
{
	Material const mat = positiveNormal ? global__materiaux[This->materialWithPositiveNormalIndex] : global__materiaux[This->materialWithNegativeNormalIndex];

	if(mat.isSimpleColor)
		return mat.simpleColor;

	ASSERT("Triangle_GetColorValueAt material has invalid texture index.", mat.textureId >= 0);

	float2 uvText = positiveNormal ? (This->UVP1*(1-s-t)) + (This->UVP2*s) + (This->UVP3*t) : (This->UVN1*(1-s-t)) + (This->UVN2*s) + (This->UVN3*t);
	return Texture_GetPixelColorValue( &global__textures[mat.textureId], global__texturesData, uvText.x, uvText.y );
}

float4 Triangle_GetSmoothNormal(Triangle const *This, bool positiveNormal, float s, float t)
{
	float4 smoothNormal = normalize( (This->N2 * s) + (This->N3 * t) + (This->N1 * (1 - s - t)) );
	if(positiveNormal)
		return smoothNormal;
	return -smoothNormal;
}

////////////////////////////////////////////////////////////////////////////////////////
///						BVH
////////////////////////////////////////////////////////////////////////////////////////

/*	Fonction qui teste si un rayon intersecte le BVH
 *	La traversée de l'arbre se fait par paquet avec le groupe entier
 */

bool BVH_IntersectRay(KERNEL_GLOBAL_VAR_DECLARATION, Ray3D *r)
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
				if(Triangle_Intersects(global__textures, global__materiaux, global__texturesData, &triangle, r, &squaredDistance))
				{
					r->intersectedTriangleId = i;
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
			if(((float*)(&r->direction))[currentNode->cutAxis]>0)
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


bool BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR_DECLARATION, Ray3D *r, float squaredDistance, RGBAColor *tint)
{
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
				if(Triangle_Intersects(global__textures, global__materiaux, global__texturesData, &triangle, r, &squaredDistance))
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

			if(((float*)(&r->direction))[currentNode->cutAxis]>0)
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

RGBAColor Scene_ComputeRadiance(KERNEL_GLOBAL_VAR_DECLARATION, Ray3D *r, Triangle const *triangle, Material const * mat, RGBAColor const *directIlluminationRadiance, RGBAColor* transferFunction, float4 const *Ng,  float4 const *Ns)
{
	float4 N = r->direction;
	RGBAColor radianceToCompute = RGBACOLOR(0,0,0,0);
	float4 outDirection = r->direction;

	if(mat->type == MAT_STANDART)
	{
		PRINT_DEBUG_INFO_0("COMPUTE RADIANCE - MATERIAL STANDART");
		*transferFunction *= r->intersectionColor;
		radianceToCompute = (*directIlluminationRadiance) * (*transferFunction);
		outDirection = Material_CosineSampleHemisphere(seed, Ns);
		N = *Ns;
	}
	else if(mat->type == MAT_GLASS)
	{
		PRINT_DEBUG_INFO_0("COMPUTE RADIANCE - MATERIAL GLASS");
		float rFresnel = Material_FresnelGlassReflectionFraction(mat, &r->direction, Ns);

		//	Reflection
		if( random(seed) < rFresnel )
		{
			outDirection = Material_FresnelReflection(mat, &r->direction, Ns);
			N = *Ng;
		}
		else
		{
			*transferFunction *= r->intersectionColor * (1 - mat->opacity);
			N = r->direction;
		}
	}

	else if(mat->type == MAT_WATER)
	{
		PRINT_DEBUG_INFO_0("COMPUTE RADIANCE - MATERIAL WATER");
		float4 refractedDirection;
		float refractionMultCoeff;
		float rFresnel = Material_FresnelWaterReflectionFraction(mat, &r->direction, Ns, r->isInWater, &refractedDirection, &refractionMultCoeff);

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

	else if(mat->type == MAT_VARNHISHED)
	{
		PRINT_DEBUG_INFO_1("COMPUTE RADIANCE - MATERIAL VARNHISHED \t", "transferFunction : %v4f", *transferFunction);
		//	Illumination directe
		radianceToCompute += *directIlluminationRadiance * r->intersectionColor * (*transferFunction);

		float4 refractedDirection;
		float rFresnel1 = Material_FresnelVarnishReflectionFraction(mat, &r->direction, Ns, false, &refractedDirection);

		//	Reflection
		if(random(seed) < rFresnel1)
		{
			PRINT_DEBUG_INFO_1("COMPUTE RADIANCE - VARNHISHED REFLECTION", "rFresnel1 : %f", rFresnel1);
			outDirection = Material_FresnelReflection(mat, &r->direction, Ns);
		}

		//	Refraction (ignoree) + rebond diffus + refraction
		else
		{
			outDirection = Material_CosineSampleHemisphere(seed, Ns);
			*transferFunction *= r->intersectionColor;

			/*
			float rFresnel2;
			float4 NsOposite = -*Ns;
			float4 intermetdiateOutDirection = Material_CosineSampleHemisphere(seed, Ns);
			rFresnel2 = Material_FresnelVarnishReflectionFraction(mat, &intermetdiateOutDirection, &NsOposite, true, &outDirection);
			PRINT_DEBUG_INFO_2("COMPUTE RADIANCE - VARNHISHED REFRACTION", "rFresnel2 : %f", rFresnel2, "r->intersectionColor : %v4f", r->intersectionColor);
			*transferFunction *= r->intersectionColor * (1-rFresnel2);
			*/
		}
	}

	Vector_PutInSameHemisphereAs(&outDirection, &N);
	Ray3D_SetDirection(r, &outDirection);
	r->origin = r->intersectionPoint + 0.001f * outDirection;

	return radianceToCompute;

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

RGBAColor Scene_ComputeDirectIllumination(KERNEL_GLOBAL_VAR_DECLARATION, Ray3D *cameraRay, Material const *mat, const float4 *N)
{
	RGBAColor L		= RGBACOLOR(0,0,0,0);				//	Radiance à calculer
	RGBAColor tint	= RGBACOLOR(1,1,1,1);				//	Teinte lors d'un shadow ray
	Light light;
	float lightContribution;

	const bool sampleLights = false;
	// Lampes
	//if(sampleLights)
	//{
	//	if(LIGHTS_SIZE > 0 && Scene_PickLight(KERNEL_GLOBAL_VAR, p, cameraRay, mat, N, &light, &lightContribution))
	//	{
	//		L = RGBACOLOR(0.5,0.5,0.5,1);

	//		//Ray3D lightRay;
	//		//float4 fullRay = light.position - (*p);
	//		//Ray3D_Create(&lightRay, p, &fullRay, cameraRay->isInWater);
	//		//
	//		//if(!BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR, &lightRay, Vector_SquaredDistanceTo(&light.position, p), &tint))		//Lumière visible
	//		//	L += light.color * tint * lightContribution;
	//	}
	//}
	//else
	//{
						
	float BRDF;
	if(LIGHTS_SIZE > 0)
	{
		for(uint i = 0; i < LIGHTS_SIZE; i++)
		{
			light = global__lights[i];
			Ray3D lightRay;
			float4 fullRay =  light.type == LIGHT_DIRECTIONNAL ? -light.direction : light.position - cameraRay->intersectionPoint;

			Ray3D_Create(&lightRay, &cameraRay->intersectionPoint, &fullRay, cameraRay->isInWater);

			float lightDistance = light.type == LIGHT_DIRECTIONNAL ? INFINITY : length(fullRay);
			float4 oppositeDirection = - lightRay.direction;
			PRINT_DEBUG_INFO_0("DIRECT ILLUMINATION 1 \t\t\t");
			BRDF = Material_BRDF(mat, &oppositeDirection, N, &cameraRay->direction);
			PRINT_DEBUG_INFO_0("DIRECT ILLUMINATION 2 \t\t\t");

			if(!BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR, &lightRay, lightDistance, &tint) )
				L += Light_PowerToward(&light, &cameraRay->intersectionPoint, N) * BRDF * tint * light.color;
			cameraRay->numIntersectedBBx += lightRay.numIntersectedBBx;
			cameraRay->numIntersectedTri += lightRay.numIntersectedTri;
		}
	}

	ASSERT_AND_INFO("Scene_ComputeDirectIllumination incorrect radiance L", L.x >= 0 && L.y >= 0 && L.z >= 0, "L : %v4f", L);

	return L;
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

	//	for(uint il=0; il<LIGHTS_SIZE; il++)
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
	
	float lightContributions[LIGHTS_SIZE+1];
	float distribution[LIGHTS_SIZE+1];
	for(uint i=0; i <= LIGHTS_SIZE; i++)
	{
		lightContributions[i] = 0.0f;
		distribution[i] = 0.0f;
	}

	Light light;

	// Construction de la distribution de probabilité

	distribution[0] = 0;
	float G;
	float BRDF;
	for(uint i = 0; i < LIGHTS_SIZE; i++)
	{
		light = global__lights[i];

		float4 lightRayDirection = normalize((*p) - light.position);
		float4 lightOpositeRay = -lightRayDirection;

		G = - dot(lightRayDirection, *N ) * Light_PowerToward(&light, p, N); 	// = cos(angle) * power / d²
		if(G < 0)
			G = 0;

		BRDF = Material_BRDF(mat, &lightOpositeRay, N, &cameraRay->direction);

		lightContributions[i] = G * BRDF;

		distribution[i+1] = distribution[i] + lightContributions[i];
	}

	if( distribution[LIGHTS_SIZE] <= 0.0f ) // Pas d'éclairage directe
		return false;

	// Tirage au sort
	float x = random(seed) * distribution[LIGHTS_SIZE];

	// Recherche de la lampe tirée par dichotomie
	int lightIndex = LIGHTS_SIZE/2;
	uint step = (uint) (LIGHTS_SIZE/4);
	while( (lightIndex >= 0 && lightIndex < LIGHTS_SIZE+1) && (distribution[lightIndex] > x || distribution[(lightIndex)+1] <= x )) 
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
///						SAMPLERS
////////////////////////////////////////////////////////////////////////////////////////

float2 sampler(Ray3D *r, int *seed, uint kernel__iterationNum)
{
	float2 sample;

#if defined(SAMPLE_UNIFORM)
	int sampleId = kernel__iterationNum % 9; // grid of 3x3
	
	sample = (float2) (get_global_id(0), get_global_id(1));

	float2 offset = ((float2) (sampleId % 3, sampleId / 3));
	offset += 0.5f;
	offset /= 3.f;

	sample += offset;

	sample /= (float2) (IMAGE_WIDTH, IMAGE_HEIGHT);
	sample -= 0.5f;

#elif defined(SAMPLE_RANDOM)
	sample = (float2) (random(seed), random(seed));
	sample -= 0.5f;

#else // SAMPLE_JITTERED

	sample.x = (get_global_id(0) + random(seed)) / ((float) IMAGE_WIDTH ) - 0.5f;
	sample.y = (get_global_id(1) + random(seed)) / ((float) IMAGE_HEIGHT) - 0.5f;
#endif

	return sample;
}

bool superSamplingStopCriteria(
	float  __global const *global__imageRayNb,
	float4 __global const *global__imageV,
	Ray3D const *r,
	int *seed)
{
	int2 pixel = (int2) ( (int) ((r->sample.x+0.5) * IMAGE_WIDTH), ( (int) ((r->sample.y+0.5) * IMAGE_HEIGHT) ) );
	pixel = min(pixel, (int2) (IMAGE_WIDTH-1, IMAGE_HEIGHT-1) );
	int globalImageOffset = pixel.y * IMAGE_WIDTH + pixel.x;

	float n = global__imageRayNb[globalImageOffset];
	if(n < 0.5f)
		return false;

	float4 sigma4_n = global__imageV[globalImageOffset] / n;
	float sigma_n = ( sigma4_n.x + sigma4_n.y + sigma4_n.z ) / n;

	bool stop = random(seed) > sigma_n;
	WARNING_AND_INFO("SUPERSAMPLING - STOP ", (get_global_id(0) != 0) || (get_global_id(1) != 0), "n : %f" , n);
	WARNING_AND_INFO("SUPERSAMPLING - STOP ", (get_global_id(0) != 0) || (get_global_id(1) != 0), "sigma4_n : %v4f" , sigma4_n);
	WARNING_AND_INFO("SUPERSAMPLING - STOP ", (get_global_id(0) != 0) || (get_global_id(1) != 0), "sigma_n : %f" , sigma_n);
	WARNING_AND_INFO("SUPERSAMPLING - STOP ", (get_global_id(0) != 0) || (get_global_id(1) != 0), "stop : %u" , stop);
	if(stop)
	{
		PRINT_DEBUG_INFO_1("SUPERSAMPLING - STOP \t\t", "sigma_n : %f" , sigma_n);
	}
	return stop;
}


////////////////////////////////////////////////////////////////////////////////////////
///						KERNEL
////////////////////////////////////////////////////////////////////////////////////////


__kernel void Kernel_Main(
	uint     kernel__iterationNum,

	float4  kernel__cameraPosition,
	float4  kernel__cameraDirection,
	float4  kernel__cameraRight,
	float4  kernel__cameraUp,

	volatile float4	__global *global__imageColor,
	volatile float	__global *global__imageRayNb,
	volatile float4	__global *global__imageV,
	volatile uint	__global *global__rayDepths,
	volatile uint	__global *global__rayIntersectionBBx,
	volatile uint	__global *global__rayIntersectionTri,

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

	Ray3D r;
	int seedValue = InitializeRandomSeed(kernel__iterationNum);
	int *seed = &seedValue;

	float2 sample = sampler(&r, seed, kernel__iterationNum);
	float4 shotDirection = kernel__cameraDirection + ( kernel__cameraRight * sample.x ) + ( kernel__cameraUp * sample.y );
	Ray3D_Create( &r, &kernel__cameraPosition, &shotDirection, false);
	r.sample = sample;

	ASSERT_AND_INFO("SAMPLER - invalid pixel", r.sample.x >= -0.5 && r.sample.y >= -0.5 && r.sample.x <= 0.5 && r.sample.y <= 0.5, "sample : %v2f", r.sample);

	if(superSamplingStopCriteria(global__imageRayNb, global__imageV, &r, seed))
		return;

	PRINT_DEBUG_INFO_1("KERNEL MAIN - START \t\t\t", "seedValue : %i" , *seed);

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
	Triangle intersectedTriangle;
	Material intersectedMaterial;

	bool activeRay = true;

	while(activeRay && r.reflectionId < MAX_REFLECTION_NUMBER)
	{
		PRINT_DEBUG_INFO_3("KERNEL MAIN - ACTIVE LOOP \t\t", "r.reflectionId : %u" , r.reflectionId, "r.numIntersectedTri : %u", r.numIntersectedTri, "transferFunction : %v4f", transferFunction);

		if(BVH_IntersectRay(KERNEL_GLOBAL_VAR, &r))
		{
			intersectedTriangle = global__triangulation[r.intersectedTriangleId];
			intersectedMaterial = global__materiaux[r.intersectedMaterialId];

			PRINT_DEBUG_INFO_1("KERNEL MAIN - INTERSETCT BVH \t\t", "r.numIntersectedTri : %u", r.numIntersectedTri);

			//if(r.isInWater)
			//{
			//	*radianceToCompute += Scene_ComputeScatteringIllumination(KERNEL_GLOBAL_VAR, r, &intersectionPoint) * (*transferFunction);

			//	float dist = distance(intersectionPoint, r->origin);
			//	*transferFunction *= Material_WaterAbsorption(dist);
			//}

			bool areRayAndNormalInSameDirection		= ( dot(r.direction, intersectedTriangle.N) > 0 );

			float4 const Ng = Triangle_GetNormal(&intersectedTriangle, !areRayAndNormalInSameDirection);

			float4 Ns = Triangle_GetSmoothNormal(&intersectedTriangle, !areRayAndNormalInSameDirection,r.s,r.t);
			float4 rOpositeDirection = - r.direction;
			Vector_PutInSameHemisphereAs(&Ns, &rOpositeDirection);
			Ns = normalize(Ns);
			ASSERT("Kernel_Main incorrect normals", dot(r.direction, Ns) < 0 && dot(r.direction, Ng) < 0);

			RGBAColor directIlluminationRadiance = Scene_ComputeDirectIllumination(KERNEL_GLOBAL_VAR, &r, &intersectedMaterial, &Ns);
			radianceToCompute += Scene_ComputeRadiance(KERNEL_GLOBAL_VAR, &r, &intersectedTriangle, &intersectedMaterial, &directIlluminationRadiance, &transferFunction, &Ng, &Ns);
			r.reflectionId++;
		}
		else // Sky
		{
			PRINT_DEBUG_INFO_0("KERNEL MAIN - INTERSETCT SKY");

			activeRay = false;
			RGBAColor skyColor = Sky_GetColorValue( global__sky, global__texturesData, &r.direction );
			radianceToCompute += (skyColor * transferFunction);
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
				PRINT_DEBUG_INFO_1("KERNEL MAIN - MAX CONTRIBUTION STOP \t", "maxContribution : %f", maxContribution);
				activeRay = false;
			}
		}

		//if( RUSSIAN_ROULETTE && activeRay && r.reflectionId > MIN_REFLECTION_NUMBER )
		//{
		//	float russianRouletteCoeff = maxContribution/(r.reflectionId - MIN_REFLECTION_NUMBER);
		//	if( russianRouletteCoeff < 1)
		//	{
		//		activeRay &= random(seed) > russianRouletteCoeff;
		//		transferFunction /= russianRouletteCoeff;
		//	}
		//}
	}

	PRINT_DEBUG_INFO_1("KERNEL MAIN - END \t\t\t", "radianceToCompute : %v4f", radianceToCompute);

	{ // Statistics
		atomic_inc(&global__rayDepths[r.reflectionId]);

		if(r.numIntersectedBBx < MAX_INTERSETCION_NUMBER)
			atomic_inc(&global__rayIntersectionBBx[r.numIntersectedBBx]);
		else
			ASSERT_AND_INFO("global__rayIntersectionBBx to large", r.numIntersectedBBx < MAX_INTERSETCION_NUMBER, "r.numIntersectedBBx = %u", r.numIntersectedBBx);

		if(r.numIntersectedTri < MAX_INTERSETCION_NUMBER)
			atomic_inc(&global__rayIntersectionTri[r.numIntersectedTri]);
		else
			ASSERT_AND_INFO("global__rayIntersectionTri to large", r.numIntersectedTri < MAX_INTERSETCION_NUMBER, "r.numIntersectedTri = %u", r.numIntersectedTri);
	}

	int2 pixel = (int2) ( (int) ((r.sample.x+0.5) * IMAGE_WIDTH), ( (int) ((r.sample.y+0.5) * IMAGE_HEIGHT) ) );
	pixel = min(pixel, (int2) (IMAGE_WIDTH-1, IMAGE_HEIGHT-1) );

	int globalImageOffset = pixel.y * IMAGE_WIDTH + pixel.x;
	ASSERT_AND_INFO("KERNEL MAIN - global offset to large", globalImageOffset < IMAGE_WIDTH*IMAGE_HEIGHT, "r.pixel : %v2i", r.sample);

	float4 sumBefore = global__imageColor[globalImageOffset];
	float4 sumAfter  = sumBefore + radianceToCompute;
	uint nRayBefore  = global__imageRayNb[globalImageOffset];
	uint nRayAfter   = nRayBefore + 1;

	global__imageRayNb[globalImageOffset] = nRayAfter;
	global__imageColor[globalImageOffset] = sumAfter;
	if(kernel__iterationNum == 1)
		global__imageV[globalImageOffset] += radianceToCompute*(radianceToCompute - sumAfter/nRayAfter);
	else if(kernel__iterationNum > 1)
		global__imageV[globalImageOffset] += (radianceToCompute - sumBefore/nRayBefore)*(radianceToCompute - sumAfter/nRayAfter);
}