#pragma once

#include <maya/MArgList.h>
#include <maya/MObject.h>
#include <maya/MGlobal.h>
#include <maya/MPxCommand.h>
#include <maya/MImage.h>
#include <maya/MSyntax.h>

class RayTracer : public MPxCommand 
{
public:

	RayTracer() {};

	virtual MStatus doIt(const MArgList& argList);
	static void* creator();

	const static MString cameraName;
	const static MString destinationFile;
	const static MString fileExtension;

};