#include "RayTracer.h"
#include "../Controleur/PathTracer.h"
#include "PathTracer_MayaImporter.h"
#include <maya/MGlobal.h>
#include <ctime>


void* RayTracer::creator() 
{ 
	return new RayTracer; 
}

void RayTracer::loadArgs(MArgList argList, uint& image_width, uint& image_height, uint& numImageToRender, bool& loadSky, bool& saveRenderedImages)
{
	int argListLength = argList.length();
	for(int i=0; i<argListLength; i++)
	{
		MString s = argList.asString(i);

		if(s.asChar()[0] == '-')
		{
			switch(s.asChar()[1])
			{
			case 'w':
				image_width = argList.asInt(i+1);
				break;
				
			case 'h':
				image_height = argList.asInt(i+1);
				break;

			case 'i':
				numImageToRender = argList.asInt(i+1);
				break;

			case 's':
				loadSky = true;
				break;

			case 'r':
				saveRenderedImages = true;
				break;

			}
		}
	}
}


MStatus RayTracer::doIt(const MArgList& argList) 
{
	MGlobal::displayInfo("Computing...");
	
	bool exportScene = false;

	bool saveRenderedImages = false;
	bool loadSky = true;
	uint image_width = 1920;
	uint image_height = 1080;
	uint numImageToRender = 1;

	loadArgs(argList, image_width, image_height, numImageToRender, loadSky, saveRenderedImages);

	clock_t start = clock();
	PathTracerNS::PathTracer_SetImporter(new PathTracerNS::PathTracerMayaImporter());
	PathTracerNS::PathTracer_Main(image_width, image_height, numImageToRender, saveRenderedImages, loadSky, exportScene);

	double timing = (clock() - start)/1000.0;

	CONSOLE << "Time spend : " << timing << "s" << ENDL;
	MGlobal::displayInfo("done.");

	return MS::kSuccess;
}
