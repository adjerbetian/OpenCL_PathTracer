#pragma once


#include "../Controleur/PathTracer_PreProc.h"
#include <maya/MPxCommand.h>
#include <maya/MArgList.h>

class RayTracer : public MPxCommand 
{
public:

	RayTracer() {};

	virtual MStatus doIt(const MArgList& argList);
	static void* creator();
};