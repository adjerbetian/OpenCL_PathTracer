#include "RayTracer.h"
#include "../Controleur/PathTracer.h"
#include "PathTracer_MayaImporter.h"

#include <maya/MGlobal.h>
#include <maya/MArgParser.h>

#include <ctime>
#include <Windows.h>
#include <Mmsystem.h>
#pragma comment(lib, "winmm.lib")


static const char* loadSky_flag =  "-sky";
static const char* loadSky_long_flag = "-loadSky";

static const char* save_flag = "-sv";
static const char* save_long_flag = "-save";

static const char* images_flag = "-i";
static const char* images_long_flag = "-images";

static const char* width_flag = "-w";
static const char* width_long_flag = "-width";

static const char* height_flag = "-h";
static const char* height_long_flag = "-height";


void* RayTracer::creator() 
{ 
	return new RayTracer; 
}

MSyntax RayTracer::newSyntax() 
{ 
	MSyntax syntax; 
	MStatus st;

	st = syntax.addFlag( loadSky_flag	, loadSky_long_flag	); 
	st = syntax.addFlag( save_flag		, save_long_flag	);
	st = syntax.addFlag( images_flag	, images_long_flag	, MSyntax::kLong);
	st = syntax.addFlag( width_flag		, width_long_flag	, MSyntax::kLong);
	st = syntax.addFlag( height_flag	, height_long_flag	, MSyntax::kLong);

	return syntax; 
}


void RayTracer::loadArgs(MArgList argList, uint& image_width, uint& image_height, uint& numImageToRender, bool& loadSky, bool& saveRenderedImages)
{
	MArgParser parser(newSyntax(), argList);

	if(parser.isFlagSet(loadSky_long_flag))
		loadSky = true;

	if(parser.isFlagSet(save_flag)) 
		saveRenderedImages = true;

	if(parser.isFlagSet(width_flag)) 
		parser.getFlagArgument( width_flag, 0, image_width );

	if(parser.isFlagSet(height_flag)) 
		parser.getFlagArgument( height_flag, 0, image_height );

	if(parser.isFlagSet(images_flag)) 
		parser.getFlagArgument( images_flag, 0, numImageToRender );

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
