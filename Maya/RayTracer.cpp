#include "RayTracer.h"
#include "../Controleur/PathTracer.h"
#include "PathTracer_MayaImporter.h"

#include <maya/MGlobal.h>
#include <maya/MArgParser.h>

#include <ctime>
#include <Windows.h>
#include <Mmsystem.h>
#pragma comment(lib, "winmm.lib")


static const char* long_flag_loadSky = "-loadSky";
static const char*      flag_loadSky =  "-sky";

static const char* long_flag_save = "-save";
static const char*      flag_save = "-sv";

static const char* long_flag_images = "-images";
static const char*      flag_images = "-i";

static const char* long_flag_width = "-width";
static const char*      flag_width = "-w";

static const char* long_flag_height = "-height";
static const char*      flag_height = "-h";

static const char* long_flag_sampler = "-sampler";
static const char*      flag_sampler = "-s";

static const char* long_flag_sSampling = "-superSampling";
static const char*      flag_sSampling = "-ss";

static const char* long_flag_depth = "-rayDepth";
static const char*      flag_depth = "-rd";

static const char* long_flag_log = "-logInfos";
static const char*      flag_log = "-l";


void* RayTracer::creator() 
{ 
	return new RayTracer; 
}

MSyntax RayTracer::newSyntax() 
{ 
	MSyntax syntax; 
	MStatus st;

	st = syntax.addFlag( flag_loadSky	, long_flag_loadSky	 ); 
	st = syntax.addFlag( flag_save		, long_flag_save	 );
	st = syntax.addFlag( flag_sSampling , long_flag_sSampling);
	st = syntax.addFlag( flag_log		, long_flag_log		 );
	st = syntax.addFlag( flag_images	, long_flag_images	, MSyntax::kLong);
	st = syntax.addFlag( flag_width		, long_flag_width	, MSyntax::kLong);
	st = syntax.addFlag( flag_height	, long_flag_height	, MSyntax::kLong);
	st = syntax.addFlag( flag_sampler	, long_flag_sampler	, MSyntax::kString);
	st = syntax.addFlag( flag_depth 	, long_flag_depth	, MSyntax::kLong);

	return syntax; 
}


void RayTracer::loadArgs(MArgList argList, uint& image_width, uint& image_height, uint& numImageToRender, bool& loadSky, bool& saveRenderedImages, PathTracerNS::Sampler& sampler, uint& rayMaxDepth, bool& printLogInfos, bool& superSampling)
{
	MArgParser parser(newSyntax(), argList);
	MString samplerString;

	if( parser.isFlagSet(flag_loadSky)         )     loadSky = true;
	if( parser.isFlagSet(flag_save)            )     saveRenderedImages = true;
	if( parser.isFlagSet(flag_log)             )     printLogInfos = true;
	if( parser.isFlagSet(flag_sSampling)       )     superSampling = true;
	if( parser.isFlagSet(flag_width)           )     parser.getFlagArgument( flag_width  , 0, image_width      );
	if( parser.isFlagSet(flag_height)          )     parser.getFlagArgument( flag_height , 0, image_height     );
	if( parser.isFlagSet(flag_images)          )     parser.getFlagArgument( flag_images , 0, numImageToRender );
	if( parser.isFlagSet(flag_sampler)         )     parser.getFlagArgument( flag_sampler, 0, samplerString    );
	if( parser.isFlagSet(flag_depth)           )     parser.getFlagArgument( flag_depth  , 0, rayMaxDepth      );

	if( parser.isFlagSet(long_flag_loadSky)    )     loadSky = true;
	if( parser.isFlagSet(long_flag_save)       )     saveRenderedImages = true;
	if( parser.isFlagSet(long_flag_log)        )     printLogInfos = true;
	if( parser.isFlagSet(long_flag_sSampling)  )     superSampling = true;
	if( parser.isFlagSet(long_flag_width)      )     parser.getFlagArgument( long_flag_width  , 0, image_width      );
	if( parser.isFlagSet(long_flag_height)     )     parser.getFlagArgument( long_flag_height , 0, image_height     );
	if( parser.isFlagSet(long_flag_images)     )     parser.getFlagArgument( long_flag_images , 0, numImageToRender );
	if( parser.isFlagSet(long_flag_sampler)    )     parser.getFlagArgument( long_flag_sampler, 0, samplerString    );
	if( parser.isFlagSet(long_flag_depth)      )     parser.getFlagArgument( long_flag_depth  , 0, rayMaxDepth      );

	if(samplerString == "uniform")		sampler = PathTracerNS::Sampler::UNIFORM;
	else if(samplerString == "random")	sampler = PathTracerNS::Sampler::RANDOM;
	else								sampler = PathTracerNS::Sampler::JITTERED;
}


MStatus RayTracer::doIt(const MArgList& argList) 
{

#ifdef _DEBUG
	CONSOLE_LOG.open(PATHTRACER_SCENE_FOLDER"PathTracer_log_deb.txt");
#else
	CONSOLE_LOG.open(PATHTRACER_SCENE_FOLDER"PathTracer_log_rel.txt");
#endif

	bool exportScene = false; // in case we don't want to load each time the scene...

	bool saveRenderedImages = false;
	bool loadSky = false;
	bool printLogInfos = false;
	bool superSampling = false;
	uint image_width = 1920;
	uint image_height = 1080;
	uint numImageToRender = 1;
	PathTracerNS::Sampler sampler;
	uint rayMaxDepth = 10;

	loadArgs(argList, image_width, image_height, numImageToRender, loadSky, saveRenderedImages, sampler, rayMaxDepth, printLogInfos, superSampling);

	PathTracerNS::PathTracer_SetImporter(new PathTracerNS::PathTracerMayaImporter());
	bool success = PathTracerNS::PathTracer_Main(image_width, image_height, numImageToRender, saveRenderedImages, loadSky, exportScene, sampler, rayMaxDepth, printLogInfos, superSampling);

	if(success)
	{
		PlaySound(L"C:\\Windows\\Media\\notify.wav", NULL, SND_ASYNC );
		CONSOLE_LOG << "Done successfully " << ENDL;
		CONSOLE_LOG.close();
		return MS::kSuccess;
	}

	PlaySound(L"C:\\Windows\\Media\\chord.wav", NULL, SND_ASYNC );
	CONSOLE_LOG << "Ended with error " << ENDL;
	CONSOLE_LOG.close();
	return MS::kFailure;
}
