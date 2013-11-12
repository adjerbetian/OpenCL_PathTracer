
#include "PathTracer_MayaImporter.h"
#include "../Controleur/PathTracer_Utils.h"
#include <maya/MItDag.h>
#include <maya/MFnMesh.h>
#include <maya/MIOStream.h>
#include <maya/MPointArray.h>
#include <maya/MDagPath.h>
#include <maya/MFnCamera.h>
#include <maya/MImage.h>
#include <maya/MMatrix.h>
#include <maya/MFnTransform.h>
#include <maya/MEulerRotation.h>


namespace PathTracerNS
{

	void PathTracerMayaImporter::Import()
	{
		SetCam();
		ImportScene();

		*ptr__global__lights = new Light[1];
		*ptr__global__materiaux = new Material[1];
		*ptr__global__textures = new Texture[1];
	}

	bool PathTracerMayaImporter::GetCam(const MString &cameraName, MDagPath &camera)
	{
		MItDag itCam(MItDag::kDepthFirst, MFn::kCamera);
		bool cameraFound = false;

		cout << "Looking for camera " << cameraName << " : ";
		while(!itCam.isDone() && !cameraFound)
		{
			itCam.getPath(camera);
			MFnCamera fn(camera);
			if(fn.name() == cameraName)
			{
				cout << "found." << endl;
				cameraFound = true;
			}
			itCam.next();
		}

		if(!cameraFound)
			cout << "not found." << endl;

		return cameraFound;
	}


	void PathTracerMayaImporter::SetCam()
	{
		const MString cameraName = "perspShape";
		MDagPath camera;

		GetCam(cameraName, camera);

		MFnCamera		fnCamera(camera);
		MFnTransform	fnTransformCamera(camera);
		MMatrix			MCamera = fnTransformCamera.transformationMatrix();

		MPoint	C		= fnCamera.eyePoint		  (MSpace::kWorld);
		MVector D		= fnCamera.viewDirection  (MSpace::kWorld);
		MVector Up		= fnCamera.upDirection	  (MSpace::kWorld);
		MVector Right	= fnCamera.rightDirection (MSpace::kWorld);
		double	pinHole	= fnCamera.focusDistance  ();


		*ptr__global__imageWidth = 1280;
		*ptr__global__imageHeight = 720;
		*ptr__global__imageSize = (*ptr__global__imageWidth) * (*ptr__global__imageHeight);

		ptr__global__cameraDirection->x = D.x;
		ptr__global__cameraDirection->y = D.y;
		ptr__global__cameraDirection->z = D.z;

		ptr__global__cameraPosition->x  = C.x;
		ptr__global__cameraPosition->y  = C.y;
		ptr__global__cameraPosition->z  = C.z;
		ptr__global__cameraPosition->w  = 0;

		ptr__global__cameraRight->x = Right.x;
		ptr__global__cameraRight->y = Right.y;
		ptr__global__cameraRight->z = Right.z;
		ptr__global__cameraRight->w = 0;

		ptr__global__cameraUp->x = Up.x;
		ptr__global__cameraUp->y = Up.y;
		ptr__global__cameraUp->z = Up.z;
		ptr__global__cameraUp->w = 0;

	}

	void PathTracerMayaImporter::ImportScene()
	{
		MItDag itMesh(MItDag::kDepthFirst,MFn::kMesh);
		MDagPath objPath;

		// First we get the total number of triangles in the scene
		*ptr__global__triangulationSize = 0;
		while(!itMesh.isDone())
		{
			itMesh.getPath(objPath);
			MFnMesh fnMesh(objPath);
			cout << "Object : " << fnMesh.name() << endl;

			MIntArray TriangleCount;
			MIntArray TraingleVertices;

			fnMesh.getTriangles(TriangleCount, TraingleVertices);

			for(uint i=0; i<TriangleCount.length(); i++)
				*ptr__global__triangulationSize += TriangleCount[i];

			itMesh.next();
			cout << endl;
		}

		itMesh.reset();
		*ptr__global__triangulation = new Triangle[*ptr__global__triangulationSize];
		uint triangleId = 0;

		while(!itMesh.isDone())
		{
			itMesh.getPath(objPath);
			MFnMesh fnMesh(objPath);
			cout << "Object : " << fnMesh.name() << endl;

			MIntArray TriangleCount;
			MIntArray TriangleVertices;
			MPointArray Points;

			fnMesh.getPoints(Points, MSpace::kWorld);
			fnMesh.getTriangles(TriangleCount, TriangleVertices);
			Float2 temp2;
			Double4 temp4;
			for(uint i=0; i<TriangleVertices.length(); i+=3)
			{
				Triangle_Create(
					*ptr__global__triangulation + triangleId,
					&Points[TriangleVertices[i]],
					&Points[TriangleVertices[i+1]],
					&Points[TriangleVertices[i+2]],
					&temp2,&temp2,&temp2,
					&temp4, &temp4, &temp4,
					&temp4, &temp4, &temp4,
					&temp4, &temp4, &temp4,
					0);
				triangleId++;
			}

			itMesh.next();
			cout << endl;
		}
	}


	void PathTracerMayaImporter::Light_Create()
	{
	};

	void PathTracerMayaImporter::SunLight_Create(SunLight *This)
	{
	};

	void PathTracerMayaImporter::Material_Create( Material *This)
	{
		This->type = MAT_STANDART;
		This->textureName = NULL;
		This->textureId = 0;
		This->isSimpleColor = true;
		This->hasAlphaMap = false;
		This->opacity = 1;

		This->simpleColor = RGBAColor(1,0,0,0);
	}

	void PathTracerMayaImporter::Triangle_Create(Triangle *This,
		Double4 const *s1, Double4 const *s2, Double4 const *s3,
		Float2 const *p1, Float2 const *p2, Float2 const *p3,
		Double4 const *n1, Double4 const *n2, Double4 const *n3,
		Double4 const *t1, Double4 const *t2, Double4 const *t3,
		Double4 const *bt1, Double4 const *bt2, Double4 const *bt3,
		uint matIndex )
	{
		This->materialWithPositiveNormalIndex = matIndex;
		This->materialWithNegativeNormalIndex = 0;

		BoundingBox_Create( &This->AABB, s1, s2, s3);

		This->N = normalize(cross( (*s2)-(*s1), (*s3)-(*s1) ));

		if(Vector_LexLessThan(s1, s2))
		{
			if(Vector_LexLessThan(s2, s3))
			{
				This->S1 = (*s1);	This->S2 = (*s2);	This->S3 = (*s3);
				This->UVP1 = (*p1);	This->UVP2 = (*p2);	This->UVP3 = (*p3);
				This->N1 = (*n1);	This->N2 = (*n2);	This->N3 = (*n3);
				This->T1 = (*t1);	This->T2 = (*t2);	This->T3 = (*t3);
				This->BT1 = (*bt1);	This->BT2 = (*bt2);	This->BT3 = (*bt3);
			}
			else if(Vector_LexLessThan(s1, s3))
			{
				This->S1 = (*s1);	This->S2 = (*s3);	This->S3 = (*s2);
				This->UVP1 = (*p1);	This->UVP2 = (*p3);	This->UVP3 = (*p2);
				This->N1 = (*n1);	This->N2 = (*n3);	This->N3 = (*n2);
				This->T1 = (*t1);	This->T2 = (*t3);	This->T3 = (*t2);
				This->BT1 = (*bt1);	This->BT2 = (*bt3);	This->BT3 = (*bt2);
			}
			else
			{
				This->S1 = (*s3);	This->S2 = (*s1);	This->S3 = (*s2);
				This->UVP1 = (*p3);	This->UVP2 = (*p1);	This->UVP3 = (*p2);
				This->N1 = (*n3);	This->N2 = (*n1);	This->N3 = (*n2);
				This->T1 = (*t3);	This->T2 = (*t1);	This->T3 = (*t2);
				This->BT1 = (*bt3);	This->BT2 = (*bt1);	This->BT3 = (*bt2);
			}
		}
		else
		{
			if(Vector_LexLessThan(s1, s3))
			{
				This->S1 = (*s2);	This->S2 = (*s1);	This->S3 = (*s3);
				This->UVP1 = (*p2);	This->UVP2 = (*p1);	This->UVP3 = (*p3);
				This->N1 = (*n2);	This->N2 = (*n1);	This->N3 = (*n3);
				This->T1 = (*t2);	This->T2 = (*t1);	This->T3 = (*t3);
				This->BT1 = (*bt2);	This->BT2 = (*bt1);	This->BT3 = (*bt3);
			}
			else if(Vector_LexLessThan(s2, s3))
			{
				This->S1 = (*s2);	This->S2 = (*s3);	This->S3 = (*s1);
				This->UVP1 = (*p2);	This->UVP2 = (*p3);	This->UVP3 = (*p1);
				This->N1 = (*n2);	This->N2 = (*n3);	This->N3 = (*n1);
				This->T1 = (*t2);	This->T2 = (*t3);	This->T3 = (*t1);
				This->BT1 = (*bt2);	This->BT2 = (*bt3);	This->BT3 = (*bt1);
			}
			else
			{
				This->S1 = (*s3);	This->S2 = (*s2);	This->S3 = (*s1);
				This->UVP1 = (*p3);	This->UVP2 = (*p2);	This->UVP3 = (*p1);
				This->N1 = (*n3);	This->N2 = (*n2);	This->N3 = (*n1);
				This->T1 = (*t3);	This->T2 = (*t2);	This->T3 = (*t1);
				This->BT1 = (*bt3);	This->BT2 = (*bt2);	This->BT3 = (*bt1);
			}
		}

		RTASSERT(Vector_LexLessThan(&This->S1, &This->S2) && Vector_LexLessThan(&This->S2, &This->S3));

	};


}