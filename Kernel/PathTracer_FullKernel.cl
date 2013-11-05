


#include "PathTracer_FullKernel_header.cl"


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
float Material_BRDF( Material const *This, Ray3D const *ri, float4 const *N, Ray3D const *rr)
{
	if(This->type == MAT_STANDART)
		return PATH_PI_INVERSE;

	if(This->type == MAT_GLASS)
		return 1;

	if(This->type == MAT_WATER)		//	Scattering
	{
		//	Schlick model
		float denom = 1 + MATERIAL_KSCHLICK * dot(ri->direction, rr->direction);
		return ( 1 - MATERIAL_KSCHLICK*MATERIAL_KSCHLICK ) / ( 4 * PATH_PI * denom * denom );
	}

	if(This->type == MAT_VARNHISHED)
	{
		//	La lumière entre entre
		float rFresnel1 = Material_FresnelVarnishReflectionFraction(This, ri, N, false, NULL);

		//	on cherche la direction de de diffusion originelle de rr
		float4 originalRefractionDirection;
		Material_FresnelVarnishReflectionFraction(This, rr, N, false, &originalRefractionDirection);

		//	Calcul du deuxième coefficient de Fresnel
		Ray3D insideRay;
		float4 originalRefractionDirectionOpposite = -originalRefractionDirection;
		float4 NOpposite = -(*N);

		Ray3D_Create(&insideRay, &rr->origin, &originalRefractionDirectionOpposite, rr->isInWater, 0, 0);
		float rFresnel2 = Material_FresnelVarnishReflectionFraction(This, &insideRay, &NOpposite, true, NULL);

		return (1 - rFresnel1) * (1 - rFresnel2) * PATH_PI_INVERSE;
	}

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

	float cos1 = min(1.0f , - dot(r->direction, *N) );
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



RGBAColor Sky_GetColorValue( Sky __global const *This ,IMAGE3D global__textures3DData, Ray3D const *r)
{
	float t;

	// Sol

	if(!r->positiveDirection[2])
	{
		t = - r->origin.z / r->direction.z;
		float xSol = r->origin.x + t * r->direction.x;
		float ySol = r->origin.y + t * r->direction.y;

		return Sky_GetFaceColorValue	 ( This, global__textures3DData, 0, xSol*This->groundScale , ySol*This->groundScale );
	}

	// Ciel

	float x = This->cosRotationAngle * r->direction.x - This->sinRotationAngle * r->direction.y;
	float y = This->sinRotationAngle * r->direction.x + This->cosRotationAngle * r->direction.y;
	float z = r->direction.z;

	int faceId = 0;
	float u = 0, v = 0;

	if( y > -x)
	{
		if( y < x )		// Face 1 : +x
		{
			if( z > x )		// Au dessus
			{
				faceId = 5;
				u = (1 + x/z) / 2;
				v = (1 - y/z) / 2;
			}
			else								    
			{
				faceId = 1;
				u = (1 - y/x) / 2;
				v = (1 + z/x) / 2;
			}
		}
		else			// Face 2 : +y			    
		{
			if( z > y )		// Au dessus		    
			{
				faceId = 5;
				u = (1 + x/z) / 2;
				v = (1 - y/z) / 2;
			}
			else								    
			{
				faceId = 2;
				u = (1 + x/y) / 2;
				v = (1 + z/y) / 2;
			}
		}
	}
	else										    
	{
		if( y > x )		//Face 3 : -x			    
		{
			if( z > - x )	// Au dessus		    
			{
				faceId = 5;
				u = (1 + x/z) / 2 ;
				v = (1 - y/z) / 2 ;
			}
			else								    
			{
				faceId = 3;
				u = (1 - y/x) / 2 ;
				v = (1 - z/x) / 2 ;
			}
		}
		else			//Face 4 : -y			    
		{
			if( z > - y )	// Au dessus		    
			{
				faceId = 5;
				u = (1 + x/z) / 2 ;
				v = (1 - y/z) / 2 ;
			}
			else								    
			{
				faceId = 4;
				u = (1 + x/y) / 2 ;
				v = (1 - z/y) / 2 ;
			}
		}
	}

	return Sky_GetFaceColorValue( This, global__textures3DData, faceId, u , v );
}

RGBAColor Sky_GetFaceColorValue( Sky __global const *This, IMAGE3D global__textures3DData, int faceId, float u, float v)
{
	//Reorganisation des indices
	if(faceId == 1)			// +x
		faceId = 1;
	else if(faceId == 3)	// -x
		faceId = 2;
	else if(faceId == 5)	// +z
		faceId = 3;
	else if(faceId == 0)	// +z
		faceId = 4;
	else if(faceId == 2)	// +y
		faceId = 5;
	else if(faceId == 4)	// -y
		faceId = 6;

	RGBAColor rgba = Texture_GetPixelColorValue(&This->skyTextures[faceId-1], faceId-1, global__textures3DData, u, 1-v);

	if(faceId != 4)
	{
		int expo = (255.f * (1.f - rgba.w)) - This->exposantFactorX;
		float coeff = pow(This->exposantFactorY, expo);
		rgba *= coeff;
	}
	rgba.w = 0;

	return rgba;
}


////////////////////////////////////////////////////////////////////////////////////////
///						TRIANGLE
////////////////////////////////////////////////////////////////////////////////////////

bool Triangle_Intersects(Texture __global const *global__textures, Material __global const *global__materiaux, IMAGE3D global__textures3DData, Triangle const *This, Ray3D const *r, float *squaredDistance, float4 *intersectionPoint, Material *intersectedMaterial, RGBAColor *intersectionColor, float *sBestTriangle, float *tBestTriangle)
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
	RGBAColor const materialColor	= Triangle_GetColorValueAt(global__textures, global__materiaux, global__textures3DData, This, nd < 0 ,s,t);

	if(mat.hasAlphaMap && RGBAColor_IsTransparent(&materialColor)) // Transparent
		return false;

	*squaredDistance = newSquaredDistance;

	if(intersectedMaterial	!= NULL) *intersectedMaterial = mat;
	if(intersectionColor	!= NULL) *intersectionColor = materialColor;
	if(sBestTriangle		!= NULL) *sBestTriangle = s;
	if(tBestTriangle		!= NULL) *tBestTriangle = t;
	if(intersectionPoint	!= NULL) *intersectionPoint = q;

	return true;
}

RGBAColor Triangle_GetColorValueAt(Texture __global const *global__textures, Material __global const *global__materiaux, IMAGE3D global__textures3DData, Triangle const *This, bool positiveNormal, float s, float t)
{
	Material const mat = positiveNormal ? global__materiaux[This->materialWithPositiveNormalIndex] : global__materiaux[This->materialWithNegativeNormalIndex];

	if(mat.isSimpleColor)
		return mat.simpleColor;

	float2 uvText = positiveNormal ? (This->UVP1*(1-s-t)) + (This->UVP2*s) + (This->UVP3*t) : (This->UVN1*(1-s-t)) + (This->UVN2*s) + (This->UVN3*t);
	return Texture_GetPixelColorValue( &global__textures[mat.textureId], mat.textureId + 6, global__textures3DData, uvText.x, uvText.y );
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

/*	Fonction qui teste si un rayon intersect le BVH
 *	La traversée de l'arbre se fait par paquet avec le groupe entier
 *
 *	Les données comme local__currentNode sont communes au groupe, pour éviter la divergence
 */

bool BVH_IntersectRay(KERNEL_GLOBAL_VAR_DECLARATION, const Ray3D *r, float4 *intersectionPoint, float *s, float *t, Triangle *intersetedTriangle, Material *intersectedMaterial, RGBAColor *intersectionColor)
{
	float squaredDistance = INFINITY;
	*local__currentReadStackIdx = -1;
	Triangle triangle;
	BoundingBox bbx1, bbx2;
	int nLoop = 0;

	bool hasIntersection = false;	
	*local__currentNode = global__bvh[0];

	PRINT_DEBUG_INFO("BVH_IntersectRay START", , 0);

	while(true)	// On sort de la boucle lorsque l'on dépile le stack et que ce dernier est vide
	{
		barrier(CLK_LOCAL_MEM_FENCE);
		nLoop++;

		PRINT_DEBUG_INFO("BVH_IntersectRay LOOP", nLoop : %i, nLoop);

		if(local__currentNode->isLeaf)
		{
			// Si on entre dans une feuille, on teste notre rayon contre tous les trirangles de la feuille

			PRINT_DEBUG_INFO("BVH_IntersectRay LEAF", , 0);

			for(uint i=local__currentNode->triangleStartIndex; i < local__currentNode->triangleStartIndex+local__currentNode->nbTriangles; i++)
			{
				triangle = global__triangulation[i];

				if(Triangle_Intersects(global__textures, global__materiaux, global__textures3DData, &triangle, r, &squaredDistance, intersectionPoint, intersectedMaterial, intersectionColor, s, t))
				{
					*intersetedTriangle = triangle;
					hasIntersection = true;

					PRINT_DEBUG_INFO("BVH_IntersectRay Intersect Triangle", triangle id : %i , i);
				}
			}

			//	On a finit de tester le rayon contre la feuille, il faut dépiler, sauf si la pile est vide

			barrier(CLK_LOCAL_MEM_FENCE);
			if((*local__currentReadStackIdx) < 0) // La pile est vide
			{
				break;
				PRINT_DEBUG_INFO("BVH_IntersectRay Break inside Leaf", local__currentReadStackIdx : %i, *local__currentReadStackIdx);
			}

			mem_fence(CLK_LOCAL_MEM_FENCE);

			if(get_local_id(0) == 0)
			{
				*local__currentNode = local__stack[*local__currentReadStackIdx];
				(*local__currentReadStackIdx)--;
			}

			barrier(CLK_LOCAL_MEM_FENCE);
		}
		else
		{
			//	Nous ne somme pas dans une feuille. On teste alors si au moins un rayon veut entrer dans 1 ou 2 fils

			PRINT_DEBUG_INFO("BVH_IntersectRay NOT LEAF", , 0);

			*local__son1 = global__bvh[local__currentNode->son1Id];
			*local__son2 = global__bvh[local__currentNode->son2Id];

			local__M[0] = false;	//	Initialisation des variables pour la suite
			local__M[1] = false;
			local__M[2] = false;
			local__M[3] = false;

			barrier(CLK_LOCAL_MEM_FENCE);

			//	On teste si notre rayon veut aller dans le fils 1 puis le fils 2
			bbx1 = local__son1->trianglesAABB;
			bool b1 = BoundingBox_Intersects( &bbx1, r, squaredDistance);
			bbx2 = local__son2->trianglesAABB;
			bool b2 = BoundingBox_Intersects( &bbx2, r, squaredDistance);

			PRINT_DEBUG_INFO("BVH_IntersectRay NOT LEAF", ( bb1 , bb2 ) : ( %i , %i ) , (int2)(b1, b2));

			//	On a 2*b2 + b1	= 0 si notre rayon n'intersecte rien
			//					= 1 si notre rayon interscte le fils 1
			//					= 2 si notre rayon interscte le fils 2
			//					= 3 si notre rayon interscte les fils 1 & 2
			local__M[2*b2 + b1] = true;

			//	la valeur de Local__M[0] est vrai si au moins un rayon n'intersecte rien
			//	                     [1]                               intersecte le fils 1
			//	                     [2]                               intersecte le fils 2
			//	                     [3]                               intersecte les fils 1 & 2
			barrier(CLK_LOCAL_MEM_FENCE);

			if(local__M[3] || (local__M[2] && local__M[1])) // Il faut parcourir les 2 fils
			{
				PRINT_DEBUG_INFO("BVH_IntersectRay NOT LEAF PARCOURS 2 FILS", , 0);

				//Choix de quel fils en premier
				local__M[get_local_id(0)] = 2*r->positiveDirection[local__currentNode->cutAxis] - 1;
				barrier(CLK_LOCAL_MEM_FENCE);
				PrefixSum_int(local__M);

				PRINT_DEBUG_INFO("BVH_IntersectRay NOT LEAF 2 FILS SOMME PARTIELLE DONE", local_M[id] : %i, local__M[get_local_id(0)]);

				// Si local__M[255] > 0, alors plus de rayons veulent aller dans le fils 1 en premier
				// Si local__M[255] < 0, alors plus de rayons veulent aller dans le fils 2 en premier

				if(local__M[255] > 0 )
				{
					PRINT_DEBUG_INFO("BVH_IntersectRay NOT LEAF 2 FILS - FILS 1 --> FILS 2", , 0);

					if(get_local_id(0) == 0)
					{
						*local__currentNode = *local__son1;
						local__stack[++(*local__currentReadStackIdx)] = *local__son2;
					}
				}
				else
				{
					PRINT_DEBUG_INFO("BVH_IntersectRay NOT LEAF 2 FILS - FILS 2 --> FILS 1", , 0);

					if(get_local_id(0) == 0)
					{
						*local__currentNode = *local__son2;
						local__stack[++(*local__currentReadStackIdx)] = *local__son1;
					}
				}

			}
			else if(local__M[1])	//	On ne parcours que le fils 1
			{
				PRINT_DEBUG_INFO("BVH_IntersectRay NOT LEAF FILS 1", , 0);
				*local__currentNode = *local__son1;
				barrier(CLK_LOCAL_MEM_FENCE);
			}
			else if(local__M[2])	// On ne parcours que le fils 2
			{
				PRINT_DEBUG_INFO("BVH_IntersectRay NOT LEAF FILS 1", , 0);
				*local__currentNode = *local__son2;
				barrier(CLK_LOCAL_MEM_FENCE);
			}
			else	// personne n'intersecte les fils, on dépile
			{
				PRINT_DEBUG_INFO("BVH_IntersectRay NOT LEAF AUCUN FILS - DEPILE", local__currentReadStackIdx : %i, *local__currentReadStackIdx);

				if(*local__currentReadStackIdx < 0)
				{
					break;
					PRINT_DEBUG_INFO("BVH_IntersectRay NOT LEAF AUCUN FILS - DEPILE - BREAK", local__currentReadStackIdx : %i, *local__currentReadStackIdx);
				}
				mem_fence(CLK_LOCAL_MEM_FENCE);

				if(get_local_id(0) == 0)
				{
					*local__currentNode = local__stack[(*local__currentReadStackIdx)--];
				}
				barrier(CLK_LOCAL_MEM_FENCE);
			}
		}
	}

	PRINT_DEBUG_INFO("BVH_IntersectRay END", hasIntersection : %i, hasIntersection);

	return hasIntersection;
}


bool BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR_DECLARATION, const Ray3D *r, float squaredDistance, RGBAColor *tint)
{
	//	cf BVH_IntersectRay pour le code

	*local__currentReadStackIdx = -1;
	Triangle triangle;
	BoundingBox bbx1, bbx2;
	int nLoop = 0;
	Material intersectedMaterial;

	bool hasIntersection = false;	
	*local__currentNode = global__bvh[0];

	PRINT_DEBUG_INFO("BVH_IntersectShadowRay START", , 0);


	local__M[0] = true;
	barrier(CLK_LOCAL_MEM_FENCE);

	while(local__M[0])
	{
		nLoop++;

		PRINT_DEBUG_INFO("BVH_IntersectShadowRay LOOP", nLoop : %i, nLoop);

		if(local__currentNode->isLeaf)
		{
			PRINT_DEBUG_INFO("BVH_IntersectShadowRay LEAF", , 0);

			for(uint i=local__currentNode->triangleStartIndex; i < local__currentNode->triangleStartIndex+local__currentNode->nbTriangles; i++)
			{
				triangle = global__triangulation[i];

				if(Triangle_Intersects(global__textures, global__materiaux, global__textures3DData, &triangle, r, &squaredDistance, NULL, &intersectedMaterial, NULL, NULL, NULL))
				{
					if(intersectedMaterial.type == MAT_STANDART || intersectedMaterial.type == MAT_VARNHISHED || intersectedMaterial.type == MAT_METAL)
						hasIntersection = true;

					PRINT_DEBUG_INFO("BVH_IntersectShadowRay Intersect Triangle", triangle id : %i , i);
				}
			}

			barrier(CLK_LOCAL_MEM_FENCE);
			if((*local__currentReadStackIdx) < 0)
			{
				break;
				PRINT_DEBUG_INFO("BVH_IntersectShadowRay Break inside Leaf", local__currentReadStackIdx : %i, *local__currentReadStackIdx);
			}

			barrier(CLK_LOCAL_MEM_FENCE);

			*local__currentNode = local__stack[*local__currentReadStackIdx];

			barrier(CLK_LOCAL_MEM_FENCE);

			if(get_local_id(0) == 0)
			{
				(*local__currentReadStackIdx)--;
			}

			barrier(CLK_LOCAL_MEM_FENCE);
		}
		else
		{
			PRINT_DEBUG_INFO("BVH_IntersectShadowRay NOT LEAF", , 0);

			*local__son1 = global__bvh[local__currentNode->son1Id];
			*local__son2 = global__bvh[local__currentNode->son2Id];

			local__M[0] = false;
			local__M[1] = false;
			local__M[2] = false;
			local__M[3] = false;

			barrier(CLK_LOCAL_MEM_FENCE);

			bbx1 = local__son1->trianglesAABB;
			bool b1 = BoundingBox_Intersects( &bbx1, r, squaredDistance);
			bbx2 = local__son2->trianglesAABB;
			bool b2 = BoundingBox_Intersects( &bbx2, r, squaredDistance);

			PRINT_DEBUG_INFO("BVH_IntersectShadowRay NOT LEAF", ( bb1 , bb2 ) : ( %i , %i ) , (int2)(b1, b2));

			barrier(CLK_LOCAL_MEM_FENCE);

			local__M[2*b2 + b1] = true;

			barrier(CLK_LOCAL_MEM_FENCE);

			if(local__M[3] || (local__M[2] && local__M[1])) // On parcours les deux fils
			{
				PRINT_DEBUG_INFO("BVH_IntersectShadowRay NOT LEAF PARCOURS 2 FILS", , 0);

				//Choix de quel fils en premier
				local__M[get_local_id(0)] = 2*r->positiveDirection[local__currentNode->cutAxis] - 1;
				barrier(CLK_LOCAL_MEM_FENCE);
				//ParallelSum(local__M, get_local_id(0), nLoop);
				barrier(CLK_LOCAL_MEM_FENCE);

				PRINT_DEBUG_INFO("BVH_IntersectShadowRay NOT LEAF 2 FILS SOMME PARTIELLE DONE", local_M[id] : %i, local__M[get_local_id(0)]);

				if(local__M[255] > 0 )
				{
					PRINT_DEBUG_INFO("BVH_IntersectShadowRay NOT LEAF 2 FILS - FILS 1 --> FILS 2", , 0);

					*local__currentNode = *local__son1;
					if(get_local_id(0) == 0)
						local__stack[++(*local__currentReadStackIdx)] = *local__son2;
				}
				else
				{
					PRINT_DEBUG_INFO("BVH_IntersectShadowRay NOT LEAF 2 FILS - FILS 2 --> FILS 1", , 0);

					*local__currentNode = *local__son2;
					if(get_local_id(0) == 0)
						local__stack[++(*local__currentReadStackIdx)] = *local__son1;
				}

			}
			else if(local__M[1])
			{
				PRINT_DEBUG_INFO("BVH_IntersectShadowRay NOT LEAF FILS 1", , 0);
				*local__currentNode = *local__son1;
				barrier(CLK_LOCAL_MEM_FENCE);
			}
			else if(local__M[2])
			{
				PRINT_DEBUG_INFO("BVH_IntersectShadowRay NOT LEAF FILS 1", , 0);
				*local__currentNode = *local__son2;
				barrier(CLK_LOCAL_MEM_FENCE);
			}
			else
			{
				PRINT_DEBUG_INFO("BVH_IntersectShadowRay NOT LEAF AUCUN FILS - DEPILE", local__currentReadStackIdx : %i, *local__currentReadStackIdx);

				if(*local__currentReadStackIdx < 0)
				{
					break;
					PRINT_DEBUG_INFO("BVH_IntersectShadowRay NOT LEAF AUCUN FILS - DEPILE - BREAK", local__currentReadStackIdx : %i, *local__currentReadStackIdx);
				}
				barrier(CLK_LOCAL_MEM_FENCE);
				if(get_local_id(0) == 0)
				{
					*local__currentNode = local__stack[(*local__currentReadStackIdx)--];
				}
				barrier(CLK_LOCAL_MEM_FENCE);
			}
		}

		// teste si quelqu'un veut continuer

		barrier(CLK_LOCAL_MEM_FENCE);
		local__M[0] = false;
		barrier(CLK_LOCAL_MEM_FENCE);
		if(!hasIntersection)
			local__M[0] = true;
		barrier(CLK_LOCAL_MEM_FENCE);
	}

	PRINT_DEBUG_INFO("BVH_IntersectShadowRay END", hasIntersection : %i, hasIntersection);

	return hasIntersection;
}



////////////////////////////////////////////////////////////////////////////////////////
///						SCENE
////////////////////////////////////////////////////////////////////////////////////////

RGBAColor Scene_ComputeRadiance(KERNEL_GLOBAL_VAR_DECLARATION, const float4 *p, Ray3D *r, Triangle const *triangle, Material const * mat, RGBAColor const * materialColor, float s, float t, RGBAColor const *directIlluminationRadiance, float4 const *Ng,  float4 const *Ns)
{
	float4 N = r->direction;

	RGBAColor radianceToCompute = RGBACOLOR(0,0,0,0);

	float4 outDirection = r->direction;

	if(mat->type == MAT_STANDART)
	{
		r->transferFunction *= (*materialColor);
		radianceToCompute = (*directIlluminationRadiance) * (r->transferFunction);
		outDirection = Material_CosineSampleHemisphere(p__itemSeed, Ns);
		N = *Ns;
	}

	else if(mat->type == MAT_GLASS)
	{
		float rFresnel = Material_FresnelGlassReflectionFraction(mat, r, Ns);

		//	Reflection
		if( random(p__itemSeed) < rFresnel )
		{
			outDirection = Material_FresnelReflection(mat, &r->direction, Ns);
			N = *Ng;
		}
		else
		{
			r->transferFunction *= (*materialColor) * (1 - mat->opacity);
			N = r->direction;
		}
	}

	else if(mat->type == MAT_WATER)
	{
		float4 refractedDirection;
		float refractionMultCoeff;
		float rFresnel = Material_FresnelWaterReflectionFraction(mat, r, Ns, &refractedDirection, &refractionMultCoeff);

		//	Reflection
		if(random(p__itemSeed) < rFresnel)
		{
			outDirection = Material_FresnelReflection(mat, &r->direction, Ns);
			N = *Ng;
		}
		else //	Refraction
		{
			r->isInWater = !r->isInWater;
			outDirection = refractedDirection;
			N = -(*Ng);
			r->transferFunction *= refractionMultCoeff;
		}
	}

	r->origin = *p;
	Vector_PutInSameHemisphereAs(&outDirection, &N);
	Ray3D_SetDirection(r, &outDirection);

	return radianceToCompute;



	//if(mat.type == MAT_VARNHISHED)
	//{

	//	//	Illumination directe
	//	*radianceToCompute += directIlluminationRadiance * materialColor * (*transferFunction);

	//	float4 refractedDirection;
	//	float rFresnel1 = Material_FresnelVarnishReflectionFraction(&mat, r, &Ns, false, &refractedDirection);

	//	//	Reflection
	//	if(random(p__itemSeed) < rFresnel1)
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

	//	outDirection = Material_CosineSampleHemisphere(p__itemSeed, &Ns);
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

	// Lampes

	if(global__lightsSize > 0 && Scene_PickLight(KERNEL_GLOBAL_VAR, p, cameraRay, mat, N, &light, &lightContribution))
	{
		Ray3D lightRay;
		float4 fullRay = light.position - (*p);
		Ray3D_Create(&lightRay, p, &fullRay, cameraRay->isInWater, 0, 0);

		if(!BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR, &lightRay, Vector_SquaredDistanceTo(&light.position, p), &tint))		//Lumière visible
			L += light.color * tint * lightContribution;
	}

	//	Soleil

	tint = RGBACOLOR(1,1,1,1);
	Ray3D sunRay;
	float4 sunOpositeDirection = - global__sun->direction;
	Ray3D_Create(&sunRay, p, &sunOpositeDirection, cameraRay->isInWater, 0, 0);
	float G = dot(sunRay.direction, *N);
	float BRDF = Material_BRDF( mat, &sunRay, N, cameraRay);

	ASSERT( BRDF >= 0 );


	if(false)	// Si on veut smapler le soleil selon sa contribution potentielle, mettre ce test à true
	{
		if( (random(p__itemSeed) < (G * BRDF)) && !BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR, &sunRay, INFINITY, &tint))
		{
			RGBAColor LSun = global__sun->color * global__sun->power * tint;
			L += LSun;
		}
	}
	else	// Le soleil est sampler tout le temps
	{
		if( (G * BRDF) > 0 && !BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR, &sunRay, INFINITY, &tint))
		{
			RGBAColor LSun = global__sun->color * global__sun->power * tint;
			L += LSun * (G * BRDF);
		}
	}

	ASSERT(L.x >= 0 && L.y >= 0 && L.z >= 0);

	return L;
}

/*	Calcule une contribution suplémentaire dans l'eau par in-scaterring
 *	
 *
 */
RGBAColor Scene_ComputeScatteringIllumination(KERNEL_GLOBAL_VAR_DECLARATION, const Ray3D *cameraRay, const float4 *intersectionPoint)
{
	RGBAColor L = RGBACOLOR(0,0,0,0);

	if(MATERIAL_WATER_SCATTERING_PART == 0)
		return L;

	Material water;
	Material_Create(&water, MAT_WATER);

	const uint nSample = 1;	//	Nombre de points d'entrée de la lumière extérieure
	RGBAColor water_absorption = MATERIAL_WATER_ABSORPTION_COLOR;
	float sigmaTotalMean = Vector_Mean(&water_absorption);

	for(uint i=0; i < nSample; i++)
	{

		RGBAColor transferFunction = RGBACOLOR(1,1,1,1);

		float4 scatteringPoint = cameraRay->origin;
		float fullRayDist = distance(*intersectionPoint, cameraRay->origin);
		float xsi = random(p__itemSeed);
		xsi = (0.98f * xsi) + 0.01f; // On met xsi entre 0.01 et 0.99

		//	On met xsi à l'échelle de l'abosption en exponentielle de l'eau pour obtenir le point où nous calculerons une contribution supplémentaire
		float scatteringRayDist = - ( 1.0f / sigmaTotalMean ) * log( 1 - (1 - exp( - sigmaTotalMean * fullRayDist ) ) * xsi );

		//	pdfScatteringPointInverse est l'inverse de la probabilité d'avoir tiré ce point
		float pdfScatteringPointInverse = ( 1 - exp( - sigmaTotalMean * fullRayDist ) ) / ( sigmaTotalMean * exp( - sigmaTotalMean * scatteringRayDist ) );

		ASSERT(scatteringRayDist/fullRayDist >= 0 && scatteringRayDist/fullRayDist <= 1);

		// Calcul du point de in-scaterring
		scatteringPoint += ((*intersectionPoint) - cameraRay->origin) * (scatteringRayDist / fullRayDist);

		//	Absorption da la contribution jusqu'au point d'intersection
		transferFunction *= Material_WaterAbsorption(scatteringRayDist) * (MATERIAL_WATER_SCATTERING_PART * pdfScatteringPointInverse);

		// Calcul de l'Illumination directe du Soleil au point scatteringPoint

		RGBAColor tint = RGBACOLOR(1,1,1,1);
		Ray3D sunRay;			//Pointing toward the sun
		Ray3D sunOpositeRay;	//Pointing away from the sun
		float4 sunOpositeDirection = -global__sun->direction;
		Ray3D_Create( &sunRay, &scatteringPoint, &sunOpositeDirection, cameraRay->isInWater, 0, 0);
		float BRDF;

		if( !BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR, &sunRay, INFINITY, &tint) )
		{
			sunOpositeRay = Ray3D_Opposite(&sunRay);
			BRDF = Material_BRDF(&water, &sunOpositeRay, NULL, cameraRay);
			RGBAColor LSun = global__sun->color * tint * global__sun->power;
			L += LSun * transferFunction * BRDF;
		}


		//	Calcul de l'Illumination directe des lumières au point scatteringPoint
		//	Cette illumination est faite de manière brutale pour toutes les lampes.
		//	La fonctionnalité ne compilant pas, je n'ai pas fait les amélioration nécessaire...

		Ray3D lightRay;
		Ray3D lightOpositeRay;
		Light light;

		for(uint il=0; il<global__lightsSize; il++)
		{
			light = global__lights[il];
			tint = RGBACOLOR(1,1,1,1);
			float4 fullRay = light.position - scatteringPoint;
			Ray3D_Create(&lightRay, &scatteringPoint, &fullRay, cameraRay->isInWater, 0, 0);

			if( !BVH_IntersectShadowRay(KERNEL_GLOBAL_VAR, &lightRay, INFINITY, &tint) )
			{
				lightOpositeRay = Ray3D_Opposite(&lightRay);
				BRDF = Material_BRDF(&water, &lightOpositeRay, NULL, cameraRay);
				float lightPower = Light_PowerToward( &light, &lightOpositeRay.direction) / Vector_SquaredDistanceTo(&light.position, &scatteringPoint);

				RGBAColor LLight = light.color * tint * lightPower;
				L += LLight * transferFunction * BRDF;
			}
		}
	}

	ASSERT(L.x >= 0 && L.y >= 0 && L.z >= 0);

	return L / ((float) nSample);

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
		Ray3D_Create(&lightRay, p, &fullRay, cameraRay->isInWater, 0, 0);
		Ray3D lightOpositeRay = Ray3D_Opposite(&lightRay);

		G = - dot(lightRay.direction, *N ) * Light_PowerToward(&light, &lightRay.direction) / Vector_SquaredDistanceTo(&light.position, p); 	// = cos(angle) * power / d²
		if(G < 0)
			G = 0;

		BRDF = Material_BRDF(mat, &lightOpositeRay, N, cameraRay);

		lightContributions[i] = G * BRDF;

		distribution[i+1] = distribution[i] + lightContributions[i];
	}

	if( distribution[global__lightsSize] <= 0.0f ) // Pas d'éclairage directe
		return false;

	// Tirage au sort
	float x = random(p__itemSeed) * distribution[global__lightsSize];

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

__kernel void Kernel_CreateRays(
	uint	kernel__iterationNum,
	float4	kernel__cameraDirection,
	float4	kernel__cameraScreenX,
	float4	kernel__cameraScreenY,
	float4	kernel__cameraPosition,
	uint	kernel__imageWidth,
	uint	kernel__imageHeight,

	void	__global	*global__void__rays
	)
{
	Ray3D __global *global__rays = (Ray3D __global *) global__void__rays;

	int  p__itemSeedValue = 0;
	int *p__itemSeed = &p__itemSeedValue;

	uint x = get_global_id(0);
	uint y = get_global_id(1);

	//	On stocke les rayons du même groupe de manière continue pour avoir une plus grande cohérence lors du premier tir
	const uint groupSize = get_local_size(0) * get_local_size(1);
	const uint globalGroupRayArrayOffset = (get_group_id(1) * get_num_groups(0) + get_group_id(0)) * groupSize;
	const uint localItemOffset = (get_local_id(1) * get_local_size(0)) + get_local_id(0);
	const uint globalRayArrayOffset = globalGroupRayArrayOffset + localItemOffset;

	PRINT_DEBUG_INFO("Kernel_CreateRays", globalRayArrayOffset : %u, globalRayArrayOffset);

	*p__itemSeed = InitializeRandomSeed(x, y, kernel__imageWidth, kernel__imageHeight, kernel__iterationNum);

	//	0n tire un nombre au hasard dans [-0.5 , 0.5] et on l'ajoute à (x,y) pour faire du anti-aliasing
	x += random(p__itemSeed) - 1;
	y += random(p__itemSeed) - 1;

	//	On calcul les coordonées dans l'écran de la caméra entre [-1 , 1]
	float xScreenSpacePosition = (2.0f * x)/kernel__imageWidth - 1.0f;
	float yScreenSpacePosition = (2.0f * y)/kernel__imageHeight - 1.0f;

	float4 shotDirection = kernel__cameraDirection + ( kernel__cameraScreenX * xScreenSpacePosition ) + ( kernel__cameraScreenY * yScreenSpacePosition );
	Ray3D r;
	Ray3D_Create( &r, &kernel__cameraPosition, &shotDirection, false, x, y);

	global__rays[globalRayArrayOffset] = r;
}


/*	Ce noyaux calcule les valueur de hashage de 4 rayons à la fois.
 *	La valeur de hashage est fonction de la position et la direction des rayons
 *
 *	Ce calcul puis tri des rayons est basé sur le papier :
 *
 *		Fast Ray Sorting and Breadth-First Packet Traversal for GPU Ray Tracing
 *						Kirill Garanzha and Charles Loop
 *
 *
 */

__kernel void Kernel_SortRays_1_ComputeHashValues_Part1(
	float4	kernel__dimGrid,
	float4	kernel__sceneMin,
	float4	kernel__sceneMax,
	void	__global const	*global__void__rays,
	uint4	__global		*global__hashValues,
	uint4	__global		*global__headFlags
	)
{
	Ray3D __global const	*global__rays				= (Ray3D __global const	*) global__void__rays;

	const uint globalArrayOffset = 4*get_global_id(0);

	PRINT_DEBUG_INFO("Kernel_SortRays_1_ComputeHashValues_Part1", globalArrayOffset : %u, globalArrayOffset);

	Ray3D rays[4] = {
		global__rays[globalArrayOffset + 0],
		global__rays[globalArrayOffset + 1],
		global__rays[globalArrayOffset + 2],
		global__rays[globalArrayOffset + 3]
	};

	uint4 rayHashValues;


	if(!rays[0].isActive && !rays[1].isActive && !rays[2].isActive && !rays[3].isActive)	//	Si aucun rayon n'est actif
	{
		rayHashValues = (uint4) (0, 0, 0, 0);
		PRINT_DEBUG_INFO("Kernel_SortRays_1_ComputeHashValues_Part1 inactive all rays", rayHashValues : ( %v4u ), rayHashValues);
	}
	else
	{
		const uint4 rayGridIndex[4] = {
			convert_uint4( (rays[0].origin - kernel__sceneMin) / (kernel__sceneMax - kernel__sceneMin) * ( kernel__dimGrid - (float4) (0.001) ) ),
			convert_uint4( (rays[1].origin - kernel__sceneMin) / (kernel__sceneMax - kernel__sceneMin) * ( kernel__dimGrid - (float4) (0.001) ) ),
			convert_uint4( (rays[2].origin - kernel__sceneMin) / (kernel__sceneMax - kernel__sceneMin) * ( kernel__dimGrid - (float4) (0.001) ) ),
			convert_uint4( (rays[3].origin - kernel__sceneMin) / (kernel__sceneMax - kernel__sceneMin) * ( kernel__dimGrid - (float4) (0.001) ) )
		};

		const uint4 rayPositionHashValues = (uint4) (
			rayGridIndex[0].x + ( rayGridIndex[0].y * kernel__dimGrid.x ) + ( rayGridIndex[0].z * kernel__dimGrid.x * kernel__dimGrid.y ),
			rayGridIndex[1].x + ( rayGridIndex[1].y * kernel__dimGrid.x ) + ( rayGridIndex[1].z * kernel__dimGrid.x * kernel__dimGrid.y ),
			rayGridIndex[2].x + ( rayGridIndex[2].y * kernel__dimGrid.x ) + ( rayGridIndex[2].z * kernel__dimGrid.x * kernel__dimGrid.y ),
			rayGridIndex[3].x + ( rayGridIndex[3].y * kernel__dimGrid.x ) + ( rayGridIndex[3].z * kernel__dimGrid.x * kernel__dimGrid.y )
			);

		const uint4 rayDirectionHashValues = (uint4) (
			dot(convert_float4(-(rays[0].direction > 0)), (float4) (1,2,4,0)),
			dot(convert_float4(-(rays[1].direction > 0)), (float4) (1,2,4,0)),
			dot(convert_float4(-(rays[2].direction > 0)), (float4) (1,2,4,0)),
			dot(convert_float4(-(rays[3].direction > 0)), (float4) (1,2,4,0))
			);

		//	On trie les rayons d'abord par direction, puis par position.
		//	Ainsi, comme les valeurs de directions sont faibles par rapport à celle de position
		//	les rayons contingues seront de même direction pour une disparité spatiale pas beaucoup au dessus
		//	a celle obtenu en triant d'abord par position puis direction

		//Direction first
		rayHashValues = rayPositionHashValues + rayDirectionHashValues * ((uint4) ( kernel__dimGrid.x * kernel__dimGrid.y * kernel__dimGrid.z ));

		rayHashValues *= (uint4) (
			rays[0].isActive,
			rays[1].isActive,
			rays[2].isActive,
			rays[3].isActive
			);
	}

	//	Comme on trie 4 rayons à la fois, on peut calculer les valeur du Head Flags pour les 3 valeurs intermédiaires
	uint4 rayHeadFlags = convert_uint4( - (rayHashValues != rayHashValues.wxyz) ); // Attention : rayHeadFlags.x est faux

	PRINT_DEBUG_INFO("Kernel_SortRays_1_ComputeHashValues_Part1 ray 0", rayHeadFlags : ( %v4u ), rayHeadFlags);

	global__hashValues[get_global_id(0)] = rayHashValues;
	global__headFlags[get_global_id(0)] = rayHeadFlags;
}

/*	Calcul de la valeur du Head Flag manquant dans la fonction d'avant
 *	Il ne faut en calculer qu'un quart de la taille totale puisque le reste
 *	est déjà bon.
 */
__kernel void Kernel_SortRays_2_ComputeHashValues_Part2(
	uint	__global	*global__hashValues,
	uint	__global	*global__headFlags
	)
{
	const uint globalArrayOffset = 4*get_global_id(0);
	global__headFlags[globalArrayOffset] = (globalArrayOffset == 0) || (global__hashValues[globalArrayOffset] != global__hashValues[globalArrayOffset - 1]);
}

/*	Cette fonction fait une somme prefix sur un tableau de taille inconnue.
 *		1 - On recopie les valeurs qui nous intéressent en mémoire locale
 *		2 - On appelle la fonction de calcul prefix en valeurs locale
 *		3 - On stocke la valeur du totale du groupe dans le tableau global__blockSum à l'indice de notre block
 *		4 - On recopie les valeurs calculées en mémoire globale
 */

__kernel void Kernel_SortRays_3_PrefixSum(
	uint __global *global__values,
	uint __global *global__blockSum
	)
{
	PRINT_DEBUG_INFO("Kernel_SortRays_3_PrefixSum START", global__value : %u, global__values[get_global_id(0)]);

	__local uint volatile local__values[16*16];
	local__values[get_local_id(0)] = global__values[get_global_id(0)];

	barrier(CLK_LOCAL_MEM_FENCE);

	PrefixSum_uint(local__values);

	global__values[get_global_id(0)] = local__values[get_local_id(0)];
	
	if(get_local_id(0) == 0 && get_num_groups(0) > 1)
		global__blockSum[get_group_id(0)] = local__values[get_local_size(0)-1];

	PRINT_DEBUG_INFO("Kernel_SortRays_3_PrefixSum END", global__value : %u, global__values[get_global_id(0)]);
}

/*	Cette fonction permet d'additionner à global__values la valeur contenue dans global__blockSum
 *	et ce par groupe
 */
__kernel void Kernel_SortRays_4_AdditionBlockOffset(
	uint __global		*global__values,
	uint __global const *global__blockSum
	)
{
	if(get_group_id(0) == 0)
		return;

	const uint blockOffsset = global__blockSum[get_group_id(0)-1];
	global__values[get_global_id(0)] += blockOffsset;
}

__kernel void Kernel_SortRays_5_Compress(
	uint __global const	*global__hashValues,
	uint __global const	*global__headFlags,
	uint __global		*global__chunkHash,
	uint __global		*global__chunkBase
	)
{
	uint itemHeadFlag = global__headFlags[get_global_id(0)] - 1;

	if( (get_global_id(0) == 0) || (itemHeadFlag == global__headFlags[get_global_id(0)-1]) )
	{
		global__chunkHash[itemHeadFlag] = global__hashValues[get_global_id(0)];
		global__chunkBase[itemHeadFlag] = get_global_id(0);
	}
}

__kernel void Kernel_SortRays_6_ChunkSize(
	uint __global	const	*global__headFlags,
	uint __global	const	*global__chunkBase,
	uint __global			*global__chunkSize
	)
{
	const uint endFlag = global__headFlags[get_global_size(0)-1];

	if(get_global_id(0) > endFlag)
		return;

	const uint nextChange = ( get_global_id(0) == endFlag ) ? get_global_size(0) : global__chunkBase[get_global_id(0) + 1];

	global__chunkSize[get_global_id(0)] = nextChange - global__chunkBase[get_global_id(0)];
}


__kernel void Kernel_SortRays_7_ComputeChunk16BaseInfo(
	uint passId,
	uint __global	const	*global__chunkHash,
	uint __global			*global__chunk16BaseInfo,
	uint __global			*global__blockSum
	)
{
	uint __local local__chunk16BaseInfo[16*256];

	for(uint i=0; i<16; i++)
		local__chunk16BaseInfo[16*get_local_id(0)+i] = 0;

	barrier(CLK_LOCAL_MEM_FENCE);

	uint chunkHash = global__chunkHash[get_global_id(0)];
	for(uint i=0; i<passId; i++)
		chunkHash >>= 4;
	chunkHash %= 16;

	local__chunk16BaseInfo[get_local_id(0)+chunkHash*256] = 1;

	barrier(CLK_LOCAL_MEM_FENCE);

	for(uint i=0; i<16; i++)
		PrefixSum_uint(&local__chunk16BaseInfo[256*i]);

	for(uint i=0; i<16; i++)
	{
		global__chunk16BaseInfo[get_global_id(0)+i*get_global_size(0)] = local__chunk16BaseInfo[get_local_id(0)+i*256];
	}

	if(get_local_id(0) < 16 && get_num_groups(0) > 1)
		global__blockSum[get_group_id(0) + get_num_groups(0)*get_local_id(0)] = local__chunk16BaseInfo[255 + 256*get_local_id(0)];
}

__kernel void Kernel_SortRays_9_AddComputedOffsetAndSort(
	uint passId,
	uint __global	const	*global__chunk16BaseInfo,
	uint __global	const	*global__blockSum,
	uint __global	const	*global__chunkHash,
	uint __global	const	*global__chunkBase,
	uint __global	const	*global__chunkSize,
	uint __global			*global__newChunkHash,
	uint __global			*global__newChunkBase,
	uint __global			*global__newChunkSize
	)
{
	const uint chunkHash = global__chunkHash[get_global_id(0)];

	uint chunkHash16Base = chunkHash;
	for(uint i=0; i<passId; i++)
		chunkHash16Base /= 16;
	chunkHash16Base %= 16;

	uint newPosition = global__chunk16BaseInfo[get_global_id(0)+chunkHash16Base*get_global_size(0)];
	newPosition += global__blockSum[get_group_id(0) + get_num_groups(0)*chunkHash16Base];

	global__newChunkHash[newPosition] = chunkHash;
	global__newChunkBase[newPosition] = global__chunkBase[get_global_id(0)];
	global__newChunkSize[newPosition] = global__chunkSize[get_global_id(0)];
}


__kernel void Kernel_SortRays_0_DebugHashValues(
	uint __global const *global__hashValues,
	uint __global const *global__headFlags ,
	uint __global const *global__blockSum1,
	uint __global const *global__blockSum2,
	uint __global const *global__chunkHash ,
	uint __global const *global__chunkBase ,
	uint __global const *global__chunkSize
	)
{
	printf(
		"Kernel_SortRays_0_DebugHashValues : group id : %i : local id : %i : global id : %i : hashValue : %u : headFlag : %u : blockSum1 : %u : blockSum2 : %u : chunkHash : %u : chunkBase : %u : chunkSize : %u\n",
		get_group_id(0),
		get_local_id(0),
		get_global_id(0),
		global__hashValues[get_global_id(0)],
		global__headFlags [get_global_id(0)],
		get_global_id(0) < 2880 ? global__blockSum1[get_global_id(0)] : -1,
		get_global_id(0) < 64   ? global__blockSum2[get_global_id(0)] : -1,
		global__chunkHash [get_global_id(0)],
		global__chunkBase [get_global_id(0)],
		global__chunkSize [get_global_id(0)]
	);
}

__kernel void Kernel_SortRays_0_DebugRadixSort(
	uint __global const *global__chunkHash,
	uint __global const *global__chunkBase,
	uint __global const *global__chunkSize,
	uint __global const *global__newChunkHash,
	uint __global const *global__newChunkBase,
	uint __global const *global__newChunkSize,
	uint __global	const *global__chunk16BaseInfo
	)
{
	printf(
		"Kernel_SortRays_0_DebugRadixSort : group id : %i : local id : %i : global id : %i : chunkHash : %u : chunkBase : %u : chunkSize : %u : newChunkHash : %u : newChunkBase : %u : newChunkSize : %u : chunk16BaseInfo 0 : %u : chunk16BaseInfo 1 : %u : chunk16BaseInfo 2 : %u : chunk16BaseInfo 3 : %u : chunk16BaseInfo 4 : %u : chunk16BaseInfo 5 : %u : chunk16BaseInfo 6 : %u : chunk16BaseInfo 7 : %u : chunk16BaseInfo 8 : %u : chunk16BaseInfo 9 : %u : chunk16BaseInfo 10 : %u : chunk16BaseInfo 11 : %u : chunk16BaseInfo 12 : %u : chunk16BaseInfo 13 : %u : chunk16BaseInfo 14 : %u : chunk16BaseInfo 15 : %u \n",
		get_group_id(0),
		get_local_id(0),
		get_global_id(0),
		global__chunkHash		[get_global_id(0)],
		global__chunkBase		[get_global_id(0)],
		global__chunkSize		[get_global_id(0)],
		global__newChunkHash	[get_global_id(0)],
		global__newChunkBase	[get_global_id(0)],
		global__newChunkSize	[get_global_id(0)],
		global__chunk16BaseInfo	[get_global_id(0) + (0  * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (1  * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (2  * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (3  * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (4  * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (5  * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (6  * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (7  * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (8  * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (9  * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (10 * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (11 * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (12 * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (13 * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (14 * get_global_size(0))],
		global__chunk16BaseInfo	[get_global_id(0) + (15 * get_global_size(0))]
	);
}


__kernel void Kernel_CustomDebug(__global Ray3D const *rays)
{
}



__kernel void Kernel_Main(
	uint	kernel__iterationNum		,	//	Utile pour initialiser le seed du random
	uint	kernel__imageWidth			,	
	uint	kernel__imageHeight			,
	uint	global__lightsSize			,

	float4	__global			*global__imageColor			,
	uint	__global			*global__imageRayNb			,
	void	__global	const	*global__void__bvh			,
	void	__global	const	*global__void__triangulation,
	void	__global	const	*global__void__lights		,
	void	__global	const	*global__void__materiaux	,
	void	__global	const	*global__void__textures		,
	IMAGE3D						 global__textures3DData		,
	void	__global	const	*global__void__sun			,
	void	__global	const	*global__void__sky			,
	void	__global	const	*global__void__rays
	)
{

	///////////////////////////////////////////////////////////////////////////////////////
	///					INITIALISATION
	///////////////////////////////////////////////////////////////////////////////////////

	PRINT_DEBUG_INFO("KERNEL SORTED MAIN START", , 0);

	bool volatile __local local__boolTest;
	Ray3D __global *global__rays = (Ray3D __global *) global__void__rays;

	//	Récupération du rayon
	Ray3D r = global__rays[get_global_id(0)];

	///////////////////////////////////////////////////////////////////////////////////////
	///					TEST SI AU MOINS UN RAYON ACTIF
	///////////////////////////////////////////////////////////////////////////////////////

	local__boolTest = false;
	barrier(CLK_LOCAL_MEM_FENCE);
	if(r.isActive)
		local__boolTest = true;
	barrier(CLK_LOCAL_MEM_FENCE);

	if(!local__boolTest)
	{
		PRINT_DEBUG_INFO("KERNEL SORTED MAIN NO ACTIVE RAY", , 0);
		return;
	}

	mem_fence(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

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
	SunLight	__global	const *global__sun				= (SunLight __global	const *) global__void__sun;
	Sky			__global	const *global__sky				= (Sky		__global	const *) global__void__sky;

	//	On déclare toutes les variables locales (communes au groupe)
	//	La raison est qu'on ne peut faire ces déclaration que dans le noyau même
	//	REMARQUE : ne pas enlever l'attribut volatile, qui fait planter le programme,
	int		volatile	__local local__decl__currentReadStackIdx;
	Node	volatile	__local local__decl__son1;
	Node	volatile	__local local__decl__son2;
	Node	volatile	__local local__decl__currentNode;
	Node	volatile	__local local__stack[BVH_MAX_DEPTH+1];
	int		volatile	__local local__M[256];

	//	On déclare les pointeurs pour passer les variables par référence
	int		volatile	__local *local__currentReadStackIdx	= &local__decl__currentReadStackIdx;
	Node	volatile	__local *local__son1				= &local__decl__son1;
	Node	volatile	__local *local__son2				= &local__decl__son2;
	Node	volatile	__local *local__currentNode			= &local__decl__currentNode;

	const bool isRayActiveAtTheBegining = r.isActive;
	const uint x = r.pixel.x;
	const uint y = r.pixel.y;
	const int globalImageOffset = y * kernel__imageWidth + x;

	//	Initialisation du randomSeed
	int p__itemSeedValue = InitializeRandomSeed(x, y, kernel__imageWidth, kernel__imageHeight, kernel__iterationNum);
	int *p__itemSeed = &p__itemSeedValue;

	//	Variable utiles lors du parcours de l'arbre
	RGBAColor radianceToCompute = RGBACOLOR(0,0,0,0);
	float4 intersectionPoint = FLOAT4(0,0,0,0);
	float s = 0.0f, t = 0.0f;
	Triangle intersectedTriangle;
	Material intersectedMaterial;
	RGBAColor intersectionColor = RGBACOLOR(0,0,0,0);

	///////////////////////////////////////////////////////////////////////////////////////
	///					INTERSECTION CONTRE LE BVH
	///////////////////////////////////////////////////////////////////////////////////////

	PRINT_DEBUG_INFO("KERNEL MAIN PROCESS RAY START", (x, y, r.reflectionId ) : %v3i, (int3) (x,y,r.reflectionId));

	const bool intersectBVH =
		BVH_IntersectRay(KERNEL_GLOBAL_VAR, &r, &intersectionPoint, &s, &t, &intersectedTriangle, &intersectedMaterial, &intersectionColor)
		&& r.isActive;

	PRINT_DEBUG_INFO("KERNEL MAIN INTERSECTION", intersectBVH : %i, intersectBVH);

	///////////////////////////////////////////////////////////////////////////////////////
	///					TEST SI AU MOINS UN RAYON INTERSECTE LE BVH
	///////////////////////////////////////////////////////////////////////////////////////

	barrier(CLK_LOCAL_MEM_FENCE);
	local__boolTest = false;
	barrier(CLK_LOCAL_MEM_FENCE);
	if(intersectBVH)
		local__boolTest = true;
	barrier(CLK_LOCAL_MEM_FENCE);

	if(local__boolTest)
	{
		// Au moins 1 rayon intersecte le BVH
		PRINT_DEBUG_INFO("KERNEL SORTED MAIN GROUP INTERSECTION", , 0);

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

		RGBAColor directIlluminationRadiance = Scene_ComputeDirectIllumination(KERNEL_GLOBAL_VAR, &intersectionPoint, &r, &intersectedMaterial, &Ns);
		//RGBAColor directIlluminationRadiance = RGBACOLOR(0.5,0.5,0.5,0);

		if(intersectBVH)
		{
			radianceToCompute += Scene_ComputeRadiance(KERNEL_GLOBAL_VAR, &intersectionPoint, &r, &intersectedTriangle, &intersectedMaterial, &intersectionColor, s, t, &directIlluminationRadiance, &Ng, &Ns);
			r.reflectionId++;
		}
	}
	else
	{
		PRINT_DEBUG_INFO("KERNEL SORTED MAIN NO GROUP INTERSECTION", , 0);
	}

	///////////////////////////////////////////////////////////////////////////////////////
	///					TEST SI AU MOINS UN RAYON INTERSECTE LE CIEL
	///////////////////////////////////////////////////////////////////////////////////////

	barrier(CLK_LOCAL_MEM_FENCE);
	local__boolTest = false;
	barrier(CLK_LOCAL_MEM_FENCE);
	if(!intersectBVH && r.isActive)
		local__boolTest = true;
	barrier(CLK_LOCAL_MEM_FENCE);

	if(local__boolTest) // Au moins 1 rayon intersecte le ciel
	{
		RGBAColor skyColor = Sky_GetColorValue( global__sky, global__textures3DData, &r );

		if(!intersectBVH && r.isActive)
		{
			radianceToCompute += (skyColor * r.transferFunction);
			r.isActive = false;
		}
		PRINT_DEBUG_INFO("KERNEL MAIN SKY COLOR", skyColor : %v4f, skyColor);
	}

	mem_fence(CLK_LOCAL_MEM_FENCE);

	PRINT_DEBUG_INFO("KERNEL MAIN PROCESS RAY END", radianceToCompute : %v4f, radianceToCompute);

	///////////////////////////////////////////////////////////////////////////////////////
	///					MISE A JOUR DE L'ACTIVITE DU RAYON
	///////////////////////////////////////////////////////////////////////////////////////

	if(r.isActive)
	{ // REFLECTION NUMBER
		if(r.reflectionId > MAX_REFLECTION_NUMBER)
		{
			r.isActive = false;
			PRINT_DEBUG_INFO("KERNEL MAX_REFLECTION_NUMBER REACHED", MAX_REFLECTION_NUMBER : %i, MAX_REFLECTION_NUMBER);
		}
	}

	float maxContribution;

	if(r.isActive)
	{ // Max contribution
		maxContribution = Vector_Max(&r.transferFunction);
		if(maxContribution <= MIN_CONTRIBUTION_VALUE)
		{
			r.isActive = false;
			PRINT_DEBUG_INFO("KERNEL MAIN CONTRIBUTION TOO SMALL", maxContribution : %f, maxContribution);
		}
	}

	if( RUSSIAN_ROULETTE && r.isActive && r.reflectionId > MIN_REFLECTION_NUMBER )
	{
		float russianRouletteCoeff = maxContribution/(r.reflectionId - MIN_REFLECTION_NUMBER);
		if( russianRouletteCoeff < 1)
		{
			r.isActive &= random(p__itemSeed) > russianRouletteCoeff;
			r.transferFunction /= russianRouletteCoeff;
		}
	}


	// Si le rayon est termine

	barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

	if(isRayActiveAtTheBegining && !r.isActive)
		global__imageRayNb[globalImageOffset]++;

	global__rays[get_global_id(0)] = r;

	global__imageColor[globalImageOffset] += radianceToCompute;

	PRINT_DEBUG_INFO("KERNEL SORTED MAIN END", global__imageColor[x][y] : %v4f, global__imageColor[globalImageOffset]);
}
