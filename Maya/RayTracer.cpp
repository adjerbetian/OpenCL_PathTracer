#include "RayTracer.h"
#include "../Controleur/PathTracer.h"
#include "PathTracer_MayaImporter.h"
#include <maya/MGlobal.h>
#include <ctime>


void* RayTracer::creator() 
{ 
	return new RayTracer; 
}


MStatus RayTracer::doIt(const MArgList& argList) 
{
	MGlobal::displayInfo("Computing...");

	int numImageToRender = argList.asInt(0);
	if(numImageToRender == 0)
		numImageToRender = 1;

	bool saveRenderedImages = argList.asBool(1);
	bool loadSky = true;
	bool exportScene = false;

	clock_t start = clock();
	PathTracerNS::PathTracer_SetImporter(new PathTracerNS::PathTracerMayaImporter());
	PathTracerNS::PathTracer_Main(numImageToRender, saveRenderedImages, loadSky, exportScene);

	double timing = (clock() - start)/1000.0;

	CONSOLE << "Time spend : " << timing << "s" << ENDL;
	MGlobal::displayInfo("done.");

	return MS::kSuccess;
}
