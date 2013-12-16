#pragma once


#include "../Controleur/PathTracer_PreProc.h"
#include <maya/MPxCommand.h>
#include <maya/MArgList.h>
#include <maya/MSyntax.h>


#ifdef _DEBUG
#define COMMAND "raytrace_d"
#else
#define COMMAND "raytrace_r"
#endif



class RayTracer : public MPxCommand 
{
public:

	RayTracer() {};

	virtual MStatus doIt(const MArgList& argList);
	static void* creator();
	static void loadArgs(MArgList argList, uint& image_width, uint& image_height, uint& numImageToRender, bool& loadSky, bool& saveRenderedImages);
	static MSyntax newSyntax();

};