#pragma once

#include <maya/MArgList.h>
#include <maya/MObject.h>
#include <maya/MGlobal.h>
#include <maya/MPxCommand.h>
#include <maya/MImage.h>

class RayTracer : public MPxCommand 
{
public:

	RayTracer() {};

	virtual MStatus doIt(const MArgList& argList);

	static void* creator();

	void	plotOnImage			(MImage &image, int x, int y);
	bool	getCamera			(const MString &cameraName, MDagPath &camera);
	MPoint	computeBarycenter	(const MDagPath& obj);

	const static MString cameraName;
	const static MString destinationFile;
	const static MString fileExtension;

};