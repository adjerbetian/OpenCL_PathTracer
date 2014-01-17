#ifndef PATHTRACER_BVH
#define PATHTRACER_BVH

#include "PathTracer_Utils.h"
#include "PathTracer_Structs.h"
#include <string>

namespace PathTracerNS
{

	void	BVH_Create				(GlobalVars& globalVars);
	int		BVH_BuildStructure		(GlobalVars& globalVars, int currentIndex);
	void	BVH_GetCharacteristics	(Node *global__bvh, uint currentNodeId, uint depth, uint& BVHMaxLeafSize, uint& BVHMinLeafSize, uint& BVHMaxDepth, uint& BVHMinDepth, uint& nNodes, uint& nLeafs, std::string& BVHMaxLeafSizeComments);
	void	BVH_CreateNode			(Node *This, int _triangleStartIndex, int _nTriangles, BoundingBox const *_trianglesAABB, BoundingBox const *_centroidsAABB, Triangle *triangulation);

}

#endif