
#include "PathTracer_BVH.h"

namespace PathTracerNS
{

	/*	Création du BVH
	 *		- Initialisation des variables
	 *		- Appel de la fonction récursive de création de l'arbre
	 */

	void BVH_Create(uint triangulationSize, Triangle* triangulation, uint *global__bvhMaxDepth, uint *global__bvhSize, Node **global__bvh)
	{
		// 1 - Calcul de la Bounding Box de la scene entiere
		BoundingBox fullTrianglesBoundingBox;
		BoundingBox fullCentroidsBoundingBox;
		BoundingBox_Reset(&fullTrianglesBoundingBox);
		BoundingBox_Reset(&fullCentroidsBoundingBox);

		for(uint i=0; i < triangulationSize; i++)
		{
			BoundingBox_UniteWith(&fullTrianglesBoundingBox, &triangulation[i].AABB);
			BoundingBox_AddPoint(&fullCentroidsBoundingBox, &triangulation[i].AABB.centroid);
		}

		// 2 - Initialisation des variables du BVH
		*global__bvhMaxDepth	= 0;
		*global__bvhSize		= 0;
		*global__bvh			= new Node[2*triangulationSize-1];


		// 3 - Creation de la racine du BVH
		BVH_CreateNode(*global__bvh, 0, triangulationSize, &fullTrianglesBoundingBox, &fullCentroidsBoundingBox, triangulation);

		// 4 - Lancement de la fonction récursive de création de l'arbre à partir de la racine
		BVH_BuildStructure(0, global__bvhSize, global__bvhMaxDepth, *global__bvh, triangulation);
	}

	/*	Fonction qui simule un constructeur sur This déjà aloué
	 */

	void BVH_CreateNode( Node *This, int _triangleStartIndex, int _nTriangles, BoundingBox const *_trianglesAABB, BoundingBox const *_centroidsAABB, Triangle *triangulation)
	{
		ASSERT(This != NULL);
		This->triangleStartIndex = _triangleStartIndex;
		This->nbTriangles = _nTriangles;
		This->son1Id = 0;
		This->son2Id = 0;
		This->isLeaf = false;
		This->cutAxis = 0;
		This->trianglesAABB = *_trianglesAABB;
		This->centroidsAABB = *_centroidsAABB;
	}



	/*	Fonction récursive de création du BVH
	 *
	 *	La structure et l'algorithme de construction du BVH sont tirés du papier :
	 *			"On fast Construction of SAH-based Bounding Volume Hierarchies"
	 *									Ingo Wald
	 *
	 *
	 *	Description des étapes du code (se référer aux numéros dans le code)
	 *
	 *		0 -	Terminaison s'il y a moins de 4 triangles.
	 *
	 *		1 -	Choisir un axe de découpage : l'axe selon lequel la bounding box est la plus étendue
	 *			Remarque :	le papier suggère de prendre comme axe la direction dont la Bounding Box des
	 *						centroids est la plus étendue.
	 *			Remarque2 :	le papier suggère également de peut-être tester les 3 axes, ce qui peut éventuellement
	 *						donner de meilleurs résultats.
	 *
	 *		2 -	Précalculer les termes utilisés dans le calcul du binId (page 4)
	 *
	 *		3 -	Diviser la région courante par K plans suivant l'axe choisi
	 *			(Dans le papier, appremment K=16 est plus que très bien)
	 *			Remarque :	Le papier suggère non pas de diviser l'axe de la Bounding Box par K plans,
	 *						mais de subdiviser la bouding box des centroids des AABB des triangles
	 *						pour ainsi avoir une division plus dense de l'espace
	 *
	 *		4 -	parcourir les triangles de la région courante et les répartir dans les K+1 espaces selon
	 *			le centroid de leur AABB
	 *			--> Vérifier comment faire pour les bounding box... c'est pas si évident.
	 *
	 *		5 - Calculer les éléments nécessaire au calcul de la SAH :
	 *			b)	passer une première fois de gauche à droite pour additionner le nombre de triangles dans
	 *				chaque zone et faire l'union des BoundingBox
	 *			c)	idem de droite à gauche
	 *				Remarque :	si une région est vide, il faut lui mettre une aire négative car les zone vides
	 *							ne sont pas autorisées
	 *
	 *		6 -	Calcul de la SAH = Nl*Al + Nr*Ar
	 *			a)	Nl et Nr sont calculés avec le point 5
	 *			b)	Al et Ar sont calculés avec les BoundingBox du point 5
	 *					Remarque :	La bounding box est prise en 3D et non seulement suivant l'axe de découpe
	 *								pour être plus précis.
	 *
	 *		7 -	Minimiser la SAH
	 *
	 *		8 - Terminaison si le SAH de la subdivision n'est pas intéressant
	 *
	 *		9 -	diviser la région selon le plan choisi et créer des sous-listes de pointeur vers triangles pour
	 *			les deux nouvelles régions
	 *
	 *		10 - relancer le calcul récursif
	 */

	int BVH_BuildStructure(int currentIndex, uint *global__bvhSize, uint *global__bvhMaxDepth, Node *global__bvh, Triangle *triangulation)
	{

		//	Parametres utilises pour la  construction du BVH

		static const float		const__KI					= 1;
		static const float		const__KT					= 0.01f;
		static const size_t		const__leafMaxSize			= 4;					// Nombre maximum de triangles dans une feuille
		static const float		const__leafMinDiagLength	= 0.001f;				// Taille minimale de la diagonale de la Bounding Box des centroids
		static const size_t		const__K					= 64;					// Nombre de plans testes

		static uint currentDepth = 0;

		//Initialisation à la racine
		if( currentIndex == 0 )
		{
			*global__bvhSize = 1;
			*global__bvhMaxDepth = 0;
			currentDepth = 0;
		}

		Node *N = &global__bvh[currentIndex];

		//0 - Terminaison
		if( N->nbTriangles <= const__leafMaxSize )
		{
			N->isLeaf = true;
			N->comments = NODE_LEAF_MAX_SIZE;
			if(currentDepth > *global__bvhMaxDepth)
				*global__bvhMaxDepth = currentDepth;
			return currentIndex+1;
		}

		if( Vector_SquaredDistanceTo(N->centroidsAABB.pMax, N->centroidsAABB.pMin) < const__leafMinDiagLength )
		{
			N->isLeaf = true;
			N->comments = NODE_LEAF_MIN_DIAG;
			if(currentDepth > *global__bvhMaxDepth)
				*global__bvhMaxDepth = currentDepth;
			return currentIndex+1;
		}

		int triangleEndIndex = N->triangleStartIndex + N->nbTriangles - 1;


		static BoundingBox elementaryBoundingBoxes[3][const__K];
		static int elementaryNTriangles[3][const__K];
		static float SAH[3][const__K-1];
		static BoundingBox leftToRightBoundingBoxes[3][const__K];
		static BoundingBox rightToLeftBoundingBoxes[3][const__K];
		static int leftToRightNTriangles[3][const__K];
		static int rightToLeftNTriangles[3][const__K];
		static float k1[3];

		//3 - Reset du decoupage en K region
		for(int cutAxis=0; cutAxis<3; cutAxis++)
		{
			for(int i=0; i < const__K; i++)
			{
				BoundingBox_Reset( &elementaryBoundingBoxes[cutAxis][i] );
				elementaryNTriangles[cutAxis][i] = 0;
				SAH[cutAxis][i] = (float) INT_MAX;
			}
		}


		//1 - Choix de l'axe de découpe :
		for(int cutAxis=0; cutAxis<3; cutAxis++)
		{
			double cutLength;

			if(cutAxis == 0)		cutLength = N->centroidsAABB.pMax.x - N->centroidsAABB.pMin.x;
			else if(cutAxis == 1)	cutLength = N->centroidsAABB.pMax.y - N->centroidsAABB.pMin.y;
			else					cutLength = N->centroidsAABB.pMax.z - N->centroidsAABB.pMin.z;

			if(cutLength < const__leafMinDiagLength)
				continue;

			//2 - Precalcul
			k1[cutAxis] = (float) ( const__K*(0.999f)/cutLength );


			//4 - Repartition des triangles
			int triangleBin = 0;
			for(int i=N->triangleStartIndex; i<=triangleEndIndex; i++)
			{
				if(cutAxis == 0)		triangleBin = (int) (k1[cutAxis]*(triangulation[i].AABB.centroid.x - N->centroidsAABB.pMin.x));
				else if(cutAxis == 1)	triangleBin = (int) (k1[cutAxis]*(triangulation[i].AABB.centroid.y - N->centroidsAABB.pMin.y));
				else					triangleBin = (int) (k1[cutAxis]*(triangulation[i].AABB.centroid.z - N->centroidsAABB.pMin.z));

				ASSERT( triangleBin < const__K );

				elementaryNTriangles[cutAxis][triangleBin]++;
				BoundingBox_UniteWith( &elementaryBoundingBoxes[cutAxis][triangleBin], &triangulation[i].AABB );
			}


			//5 - prendre le meilleur plan parmis les K
			for(int i=0; i < const__K; i++) // Initialisation
			{
				leftToRightBoundingBoxes[cutAxis][i] = elementaryBoundingBoxes[cutAxis][i];
				rightToLeftBoundingBoxes[cutAxis][i] = elementaryBoundingBoxes[cutAxis][i];
				leftToRightNTriangles[cutAxis][i] = elementaryNTriangles[cutAxis][i];
				rightToLeftNTriangles[cutAxis][i] = elementaryNTriangles[cutAxis][i];
			}
			int i=1, j = const__K - 2;
			while(i < const__K) // Remplissage des données
			{
				BoundingBox_UniteWith( &leftToRightBoundingBoxes[cutAxis][i], &leftToRightBoundingBoxes[cutAxis][i-1]);
				leftToRightNTriangles[cutAxis][i] += leftToRightNTriangles[cutAxis][i-1];

				BoundingBox_UniteWith( &rightToLeftBoundingBoxes[cutAxis][j], &rightToLeftBoundingBoxes[cutAxis][j+1]);
				rightToLeftNTriangles[cutAxis][j] += rightToLeftNTriangles[cutAxis][j+1];

				i++;
				j--;
			}


			//6 - Calcul de la SAH
			for(int i=0; i < const__K-1; i++)
			{
				SAH[cutAxis][i] = (float) (
					leftToRightNTriangles[cutAxis][i] * BoundingBox_Area( &leftToRightBoundingBoxes[cutAxis][i] )
					+
					rightToLeftNTriangles[cutAxis][i+1] * BoundingBox_Area( &rightToLeftBoundingBoxes[cutAxis][i+1] ));
			}


		}// fin du choix de l'axe de découpe


		//7 - Minimisation de la SAH
		int bestCutAxis = 0;
		int bestCutIndex = 0;
		float bestSAH(SAH[0][0]);
		for(int cutAxis = 0; cutAxis<3; cutAxis++)
		{
			for(int i=1; i < const__K-1; i++) // choix du meilleur plan de coupe
			{
				if(SAH[cutAxis][i] < bestSAH)
				{
					bestCutAxis = cutAxis;
					bestSAH = SAH[cutAxis][i];
					bestCutIndex = i;
				}
			}
		}


		//8 - terminaison en cas de mauvaise SAH
		if( const__KI*bestSAH + const__KT > N->nbTriangles * BoundingBox_Area( &N->trianglesAABB ) )
		{
			N->isLeaf = true;
			N->comments = NODE_BAD_SAH;
			if(currentDepth > *global__bvhMaxDepth)
				*global__bvhMaxDepth = currentDepth;
			return currentIndex+1;
		}

		N->cutAxis = bestCutAxis;

		//9 - diviser la région selon le plan choisi
		int leftSortIndex = N->triangleStartIndex;
		int rightSortIndex = triangleEndIndex;
		//float bestCutPlanPosition = N->centroidsAABB.pMin[bestCutAxis] + (bestCutIndex+1)*(N->centroidsAABB.pMax[bestCutAxis]-N->centroidsAABB.pMin[bestCutAxis])/K;

		if(bestCutAxis == 0)
		{
			while(leftSortIndex < rightSortIndex)
			{
				while( (k1[bestCutAxis]*(triangulation[leftSortIndex ].AABB.centroid.x - N->centroidsAABB.pMin.x)) <  bestCutIndex+1 && leftSortIndex < rightSortIndex)
					leftSortIndex++;
				while( (k1[bestCutAxis]*(triangulation[rightSortIndex].AABB.centroid.x - N->centroidsAABB.pMin.x)) >= bestCutIndex+1 && leftSortIndex < rightSortIndex)
					rightSortIndex--;
				if(leftSortIndex < rightSortIndex)
					std::swap(triangulation[leftSortIndex], triangulation[rightSortIndex]);
			}
		}
		else if(bestCutAxis == 1)
		{
			while(leftSortIndex < rightSortIndex)
			{
				while( (k1[bestCutAxis]*(triangulation[leftSortIndex ].AABB.centroid.y - N->centroidsAABB.pMin.y)) <  bestCutIndex+1 && leftSortIndex < rightSortIndex)
					leftSortIndex++;
				while( (k1[bestCutAxis]*(triangulation[rightSortIndex].AABB.centroid.y - N->centroidsAABB.pMin.y)) >= bestCutIndex+1 && leftSortIndex < rightSortIndex)
					rightSortIndex--;
				if(leftSortIndex < rightSortIndex)
					std::swap(triangulation[leftSortIndex], triangulation[rightSortIndex]);
			}
		}
		else
		{
			while(leftSortIndex < rightSortIndex)
			{
				while( (k1[bestCutAxis]*(triangulation[leftSortIndex ].AABB.centroid.z - N->centroidsAABB.pMin.z)) <  bestCutIndex+1 && leftSortIndex < rightSortIndex)
					leftSortIndex++;
				while( (k1[bestCutAxis]*(triangulation[rightSortIndex].AABB.centroid.z - N->centroidsAABB.pMin.z)) >= bestCutIndex+1 && leftSortIndex < rightSortIndex)
					rightSortIndex--;
				if(leftSortIndex < rightSortIndex)
					std::swap(triangulation[leftSortIndex], triangulation[rightSortIndex]);
			}
		}

		BoundingBox leftCentroidsAABB, rightCentroidsAABB;
		BoundingBox_Reset(&leftCentroidsAABB);
		BoundingBox_Reset(&rightCentroidsAABB);
		for(int i=N->triangleStartIndex; i<leftSortIndex; i++)
			BoundingBox_AddPoint( &leftCentroidsAABB, &triangulation[i].AABB.centroid);
		for(int i=triangleEndIndex; i>=leftSortIndex; i--)
			BoundingBox_AddPoint( &rightCentroidsAABB, &triangulation[i].AABB.centroid);


		//10 - recursivite
		//enregistrements des données pour le fils droit (a causes des tableaus statiques
		int triangleStartIndexSon2 = N->triangleStartIndex+leftToRightNTriangles[bestCutAxis][bestCutIndex];
		int nTrianglesSon2 = rightToLeftNTriangles[bestCutAxis][bestCutIndex+1];
		BoundingBox trianglesAABBSon2 = rightToLeftBoundingBoxes[bestCutAxis][bestCutIndex+1];

		currentDepth++;
		*global__bvhSize += 2;

		//Fils gauche

		N->son1Id = currentIndex+1;
		BVH_CreateNode(
			&global__bvh[N->son1Id],
			N->triangleStartIndex,
			leftToRightNTriangles[bestCutAxis][bestCutIndex],
			&leftToRightBoundingBoxes[bestCutAxis][bestCutIndex],
			&leftCentroidsAABB,
			triangulation);

		N->son2Id = BVH_BuildStructure(N->son1Id, global__bvhSize, global__bvhMaxDepth, global__bvh, triangulation);

		//Fils droit
		BVH_CreateNode(
			&global__bvh[N->son2Id],
			triangleStartIndexSon2,
			nTrianglesSon2,
			&trianglesAABBSon2,
			&rightCentroidsAABB,
			triangulation);

		int nextIndex = BVH_BuildStructure(N->son2Id, global__bvhSize, global__bvhMaxDepth, global__bvh, triangulation);
		currentDepth--;
		return nextIndex;
	}


	/*	Fonction récursive, utile pour le debuggage et les test de performance
	 */

	void BVH_GetCharacteristics(
		Node *global__bvh,
		uint currentNodeId,
		uint depth,
		uint& BVHMaxLeafSize,
		uint& BVHMinLeafSize,
		uint& BVHMaxDepth,
		uint& BVHMinDepth,
		uint& nNodes,
		uint& nLeafs,
		std::string& BVHMaxLeafSizeComments)
	{
		Node const *currentNode = &global__bvh[currentNodeId];

		if(currentNode->isLeaf)
		{
			nLeafs++;
			if(depth < BVHMinDepth)
				BVHMinDepth = depth;
			else if(depth > BVHMaxDepth)
				BVHMaxDepth = depth;

			if(currentNode->nbTriangles < BVHMinLeafSize)
			{
				BVHMinLeafSize = currentNode->nbTriangles;
			}
			else if(currentNode->nbTriangles > BVHMaxLeafSize)
			{
				BVHMaxLeafSize = currentNode->nbTriangles;
				BVHMaxLeafSizeComments = currentNode->comments;
			}

			return;
		}

		nNodes += 2;
		BVH_GetCharacteristics(global__bvh, currentNode->son1Id, depth+1, BVHMaxLeafSize, BVHMinLeafSize, BVHMaxDepth, BVHMinDepth, nNodes, nLeafs, BVHMaxLeafSizeComments);
		BVH_GetCharacteristics(global__bvh, currentNode->son2Id, depth+1, BVHMaxLeafSize, BVHMinLeafSize, BVHMaxDepth, BVHMinDepth, nNodes, nLeafs, BVHMaxLeafSizeComments);

	}

}