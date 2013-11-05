#include "RayTracer.h"
#include "../Controleur/PathTracer.h"
#include "PathTracer_MayaImporter.h"

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


const MString RayTracer::cameraName = "raytracingCamera";
const MString RayTracer::destinationFile = "C:\\Users\\Alexandre Djerbetian\\Pictures\\Maya\\raytrace.bmp";
const MString RayTracer::fileExtension = "bmp";


MString toString(const MPoint& M)
{
	MString s;
	s += "[ ";
	s += + M.x;
	s += " ,  ";
	s += + M.y;
	s += " , ";
	s += + M.z;
	s += " ]";
	return s;
}

void* RayTracer::creator() 
{ 
	return new RayTracer; 
}


bool RayTracer::getCamera(const MString &cameraName, MDagPath &camera)
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

void RayTracer::plotOnImage(MImage &image, int x, int y)
{
	unsigned int w,h;
	image.getSize(w,h);
	unsigned char* pixels = image.pixels();
	const int crossSize = 10;

	for(int i = -crossSize; i != crossSize + 1; i++)
	{
		pixels[4*(y * w + x + i) + 0] = 0;
		pixels[4*(y * w + x + i) + 1] = 255;
		pixels[4*(y * w + x + i) + 2] = 0;
		pixels[4*(y * w + x + i) + 3] = 255;
	}

	for(int j = -crossSize; j != crossSize + 1; j++)
	{
		pixels[4*((y+j) * w + x) + 0] = 0;
		pixels[4*((y+j) * w + x) + 1] = 255;
		pixels[4*((y+j) * w + x) + 2] = 0;
		pixels[4*((y+j) * w + x) + 3] = 255;
	}

}


MPoint RayTracer::computeBarycenter(const MDagPath& obj)
{
	MPoint barycenter;
	MFnMesh fn(obj);
	MPointArray vts;

	fn.getPoints(vts, MSpace::kWorld);

	cout << "\t" << vts.length() << " vertices :" << endl;
	for(int i=0; i != vts.length(); i++)
	{
		MPoint worldPoint = vts[i];
		cout << "\t\t" << i << toString(worldPoint) << endl;
		barycenter += worldPoint;
	}
	cout << endl;

	return (barycenter / vts.length());
}


MStatus RayTracer::doIt(const MArgList& argList) 
{
	PathTracerNS::PathTracer_SetImporter(new PathTracerNS::PathTracerMayaImporter());
	PathTracerNS::PathTracer_Main();
	return MS::kSuccess;
}

