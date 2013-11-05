#ifndef PATHTRACER_BVH
#define PATHTRACER_BVH

#include "PathTracer_Utils.h"
#include "PathTracer_Structs.h"
#include <string>

namespace PathTracerNS
{

	void	BVH_Create				(uint triangulationSize, Triangle* triangulation, uint *global__bvhMaxDepth, uint *global__bvhSize, Node **global__bvh);
	int		BVH_BuildStructure		(int currentIndex, uint *global__bvhSize, uint *global__bvhMaxDepth, Node *global__bvh, Triangle *triangulation);
	void	BVH_GetCharacteristics	(Node *global__bvh, uint currentNodeId, uint depth, uint& BVHMaxLeafSize, uint& BVHMinLeafSize, uint& BVHMaxDepth, uint& BVHMinDepth, uint& nNodes, uint& nLeafs, std::string& BVHMaxLeafSizeComments);
	void	BVH_CreateNode			(Node *This, int _triangleStartIndex, int _nTriangles, BoundingBox const *_trianglesAABB, BoundingBox const *_centroidsAABB, Triangle *triangulation);

}

#endif