#pragma once

#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MArgList.h>
#include <maya/MPxCommand.h>

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