
#include "PathTracer_MayaImporter.h"
#include "../Controleur/PathTracer_Utils.h"
#include "../Alone/PathTracer_bitmap.h"
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
#include <maya/MMaterial.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MFnLambertShader.h>
#include <maya/MFnLight.h>

namespace PathTracerNS
{

	void PathTracerMayaImporter::Import()
	{
		SetCam();
		ImportScene();
		LoadSky();

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

		Up /= (float) (*ptr__global__imageWidth) / (float) (*ptr__global__imageHeight); 

		*ptr__global__imageSize = (*ptr__global__imageWidth) * (*ptr__global__imageHeight);

		*ptr__global__cameraDirection	= permute_xyz_to_zxy(MPoint(D));
		*ptr__global__cameraPosition	= permute_xyz_to_zxy(C);
		*ptr__global__cameraRight		= permute_xyz_to_zxy(MPoint(Right));;
		*ptr__global__cameraUp			= permute_xyz_to_zxy(MPoint(Up));;

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
			MIntArray TriangleCount;
			MIntArray TraingleVertices;

			fnMesh.getTriangles(TriangleCount, TraingleVertices);

			for(uint i=0; i<TriangleCount.length(); i++)
				*ptr__global__triangulationSize += TriangleCount[i];

			itMesh.next();
		}

		itMesh.reset();
		*ptr__global__triangulation = new Triangle[*ptr__global__triangulationSize];
		uint triangleId = 0;

		while(!itMesh.isDone())
		{
			itMesh.getPath(objPath);
			MFnMesh fnMesh(objPath);
			MIntArray TriangleCount;
			MIntArray TriangleVertices;
			MPointArray Points;

			fnMesh.getPoints(Points, MSpace::kWorld);
			fnMesh.getTriangles(TriangleCount, TriangleVertices);
			Float2 temp2;
			Float4 temp4;
			for(uint i=0; i<TriangleVertices.length(); i+=3)
			{
				Triangle_Create(
					*ptr__global__triangulation + triangleId,
					&permute_xyz_to_zxy(Points[TriangleVertices[i]]),
					&permute_xyz_to_zxy(Points[TriangleVertices[i+1]]),
					&permute_xyz_to_zxy(Points[TriangleVertices[i+2]]),
					&temp2,&temp2,&temp2,
					&temp4, &temp4, &temp4,
					&temp4, &temp4, &temp4,
					&temp4, &temp4, &temp4,
					0);
				triangleId++;
			}

			itMesh.next();
		}
	}

	void PathTracerMayaImporter::ImportLights()
	{
		MItDag itLight(MItDag::kDepthFirst,MFn::kLight);
		MDagPath objPath;

		// First we get the total number of lights in the scene
		*ptr__global__lightsSize = 0;
		while(!itLight.isDone())
		{
			(*ptr__global__lightsSize)++;
			itLight.next();
		}

		itLight.reset();
		*ptr__global__lights = new Light[*ptr__global__lightsSize];
		uint lightIndex = 0;

		while(!itLight.isDone())
		{
			itLight.getPath(objPath);
			MFnLight fnLight(objPath);
			Light_Create((*ptr__global__lights)[lightIndex], fnLight);
			lightIndex++;
			itLight.next();
		}
	}

	void PathTracerMayaImporter::LoadSky()
	{
		ptr__global__sky->cosRotationAngle = 1;
		ptr__global__sky->sinRotationAngle = 0;
		ptr__global__sky->exposantFactorX = 0;
		ptr__global__sky->exposantFactorY = 0;
		ptr__global__sky->groundScale = 1;

		int fullWidth, fullHeight;
		long int size;
		LPCTSTR filePath = L"C:\\Users\\Alexandre Djerbetian\\Pictures\\Maya\\cubemap.bmp";
		uchar* sky = (uchar *) LoadBMP(&fullWidth,&fullHeight,&size, filePath);

		const uint texWidth = fullWidth/4;
		const uint texHeight = fullHeight/3;

		*ptr__global__texturesDataSize = 6*texWidth*texHeight;
		*ptr__global__texturesData = new Uchar4[*ptr__global__texturesDataSize];

		uint texIndex = 0;

		// Upper face
		ptr__global__sky->skyTextures[0].width = texWidth	;
		ptr__global__sky->skyTextures[0].height = texHeight;
		ptr__global__sky->skyTextures[0].offset = 0;
		for(uint y = 0; y<texHeight; y++)
		{
			for(uint fullIndex = y * fullWidth + texWidth; fullIndex < y * fullWidth + 2*texWidth; fullIndex++)
			{
				uchar b = sky[3*fullIndex];
				uchar g = sky[3*fullIndex+1];
				uchar r = sky[3*fullIndex+2];
				uchar a = 255;

 				(*ptr__global__texturesData)[texIndex].x = r;
 				(*ptr__global__texturesData)[texIndex].y = g;
 				(*ptr__global__texturesData)[texIndex].z = b;
 				(*ptr__global__texturesData)[texIndex].w = a;
				texIndex++;
			}
		}

		//// Side faces
		for(int i=0; i<4; i++)
		{
			ptr__global__sky->skyTextures[i+1].width = texWidth;
			ptr__global__sky->skyTextures[i+1].height = texHeight;
			ptr__global__sky->skyTextures[i+1].offset = (i+1)*texWidth*texHeight;
			for(uint y = texHeight; y<2*texHeight; y++)
			{
				for(uint fullIndex = y * fullWidth + i*texWidth; fullIndex < y * fullWidth + (i+1)*texWidth; fullIndex++)
				{
					uchar b = sky[3*fullIndex];
					uchar g = sky[3*fullIndex+1];
					uchar r = sky[3*fullIndex+2];
					uchar a = 255;

 					(*ptr__global__texturesData)[texIndex].x = r;
 					(*ptr__global__texturesData)[texIndex].y = g;
 					(*ptr__global__texturesData)[texIndex].z = b;
 					(*ptr__global__texturesData)[texIndex].w = a;
					texIndex++;
				}
			}
		}

		// Bottom face - ground
		ptr__global__sky->skyTextures[5].width = texWidth;
		ptr__global__sky->skyTextures[5].height = texHeight;
		ptr__global__sky->skyTextures[5].offset = 5*texWidth*texHeight;
		for(uint y = 2*texHeight; y<3*texHeight; y++)
		{
			for(uint fullIndex = y * fullWidth + texWidth; fullIndex < y * fullWidth + 2*texWidth; fullIndex++)
			{
				uchar b = sky[3*fullIndex];
				uchar g = sky[3*fullIndex+1];
				uchar r = sky[3*fullIndex+2];
				uchar a = 255;

 				(*ptr__global__texturesData)[texIndex].x = r;
 				(*ptr__global__texturesData)[texIndex].y = g;
 				(*ptr__global__texturesData)[texIndex].z = b;
 				(*ptr__global__texturesData)[texIndex].w = a;
				texIndex++;
			}
		}
	};

	void PathTracerMayaImporter::Light_Create(Light& l, MFnLight& fnLight)
	{
		l.color						= RGBAColor(fnLight.color());
		l.cosOfInnerFallOffAngle	= fnLight;
		l.cosOfOuterFallOffAngle	= fnLight;
		l.direction					= Float4(fnLight.lightDirection());
		l.position					= fnLight;
		l.power						= fnLight.intensity();;
		l.type						= fnLight;
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
		Float4 const *s1, Float4 const *s2, Float4 const *s3,
		Float2 const *p1, Float2 const *p2, Float2 const *p3,
		Float4 const *n1, Float4 const *n2, Float4 const *n3,
		Float4 const *t1, Float4 const *t2, Float4 const *t3,
		Float4 const *bt1, Float4 const *bt2, Float4 const *bt3,
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