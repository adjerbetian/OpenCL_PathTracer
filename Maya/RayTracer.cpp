#include "RayTracer.h"
#include "../Controleur/PathTracer.h"
#include "PathTracer_MayaImporter.h"
#include <maya/MGlobal.h>
#include <ctime>

#include <Windows.h>
#include <Mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include <direct.h>



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
	bool exportScene = false; // in case we don't want to load each time the scene...

	bool saveRenderedImages = false;
	bool loadSky = false;
	uint image_width = 1920;
	uint image_height = 1080;
	uint numImageToRender = 1;

	loadArgs(argList, image_width, image_height, numImageToRender, loadSky, saveRenderedImages);

#ifdef _DEBUG
	CONSOLE_LOG.open(PATHTRACER_SCENE_FOLDER"PathTracer_log_deb.txt");
#else
	CONSOLE_LOG.open(PATHTRACER_SCENE_FOLDER"PathTracer_log_rel.txt");
#endif

	PathTracerNS::PathTracer_SetImporter(new PathTracerNS::PathTracerMayaImporter());
	bool success = PathTracerNS::PathTracer_Main(image_width, image_height, numImageToRender, saveRenderedImages, loadSky, exportScene);

	CONSOLE_LOG.close();
	PlaySound(L"C:\\Windows\\Media\\notify.wav", NULL, SND_ASYNC );
	return success ? MS::kSuccess : MS::kFailure;
}
