#include "RayTracer.h"
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

	cout << endl << endl;
	cout << "*****************************************************************" << endl;
	cout << "                         RAYTRACER" << endl;
	cout << "*****************************************************************" << endl;
	cout << endl;

	// 1 - Create Image

	MImage image;
	image.create(1920,1080);

	// 2 - Get the camera and the its projection information

	const MString cameraFrontName = "frontShape";
	MDagPath camera;
	if(!getCamera(cameraName, camera))
		if(!getCamera(cameraFrontName, camera))
			MS::kFailure;
	cout << endl;

	MFnCamera		fnCamera(camera);
	MFnTransform	fnTransformCamera(camera);
	MMatrix			MCamera = fnTransformCamera.transformationMatrix();

	fnCamera.setAspectRatio(16.0/9.0);
	fnCamera.setIsOrtho(true);

	MPoint	C		= fnCamera.eyePoint		  (MSpace::kWorld);
	MVector D		= fnCamera.viewDirection  (MSpace::kWorld);
	MVector Up		= fnCamera.upDirection	  (MSpace::kWorld);
	MVector Right	= fnCamera.rightDirection (MSpace::kWorld);

	float w = fnCamera.orthoWidth();
	float h = ( w * 9.0 / 16.0 );

	cout << "Camera : " << endl;
	cout << "\tposition : " << toString(C) << endl;
	cout << "\tdirection : " << toString(D) << endl;
	cout << "\tw x h : " << w << " x " << h << endl;
	cout << endl;

	// 3 - Mesh traversal

	MItDag itMesh(MItDag::kDepthFirst,MFn::kMesh);
	MDagPath objPath;
	while(!itMesh.isDone())
	{
		itMesh.getPath(objPath);
		MFnMesh fnMesh(objPath);
		cout << "Object : " << fnMesh.name() << endl;

		// Get the barycenter
		MPoint M = computeBarycenter(objPath);
		cout << "\tBarycenter : " << toString(M) << endl;

		// Compute the projection on the camera orthographic plane
		MPoint P = M - (MVector(M - C)*D)*D;
		cout << "\tProjection : " << toString(P) << endl;

		// Compute the coordinates in the camera's basis
		float x = MVector(P-C)*Right;
		float y = MVector(P-C)*Up;
		cout << "\tCoordinates in Camera : [ " << x << " , " << y << " ]" << endl;

		// normalize the coordinates in [ -1 , 1 ]
		x = (x / w) + 0.5;
		y = (y / h) + 0.5;
		cout << "\tNormalized coordinates : [ " << x << " , " << y << " ]" << endl;

		// compute the pixel position and print it
		if( -1 < x && x < 1 && -1 < y && y < 1 )
		{
			int yPixel = y * 1080;
			int xPixel = x * 1920;
			cout << "\tPixel : [ " << xPixel << " , " << yPixel << " ]" << endl;

			plotOnImage(image, xPixel, yPixel);
		}

		itMesh.next();
		cout << endl;
	}

	cout << "writting to file " << destinationFile << " ... " ;
	if(image.writeToFile(destinationFile, fileExtension) == MS::kSuccess)
		cout << "success." << endl;
	else
		cout << "fail to save file." << endl;

	return MS::kSuccess;
}

