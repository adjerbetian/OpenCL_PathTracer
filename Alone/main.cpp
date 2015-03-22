
#include "../Controleur/PathTracer.h"
#include "../Controleur/PathTracer_FileImporter.h"
#include <Windows.h>
#include <Mmsystem.h>
#pragma comment(lib, "winmm.lib")


int main()
{
#ifdef _DEBUG
	CONSOLE_LOG.open(PATHTRACER_SCENE_FOLDER"PathTracer_log_deb.txt");
#else
	CONSOLE_LOG.open(PATHTRACER_SCENE_FOLDER"PathTracer_log_rel.txt");
#endif

	bool exportScene = false;
	bool saveRenderedImages = true;
	bool loadSky = true;
    bool printLogInfos = true;
    bool superSampling = false;
    unsigned int image_width = 1920;
	unsigned int image_height = 1080;
	unsigned int numImageToRender = 1;
	PathTracerNS::Sampler sampler = PathTracerNS::Sampler::JITTERED;
	unsigned int rayMaxDepth = 10;

	PathTracerNS::PathTracer_SetImporter(new PathTracerNS::PathTracerFileImporter());
    bool success = PathTracerNS::PathTracer_Main(image_width, image_height, numImageToRender, saveRenderedImages, loadSky, exportScene, sampler, rayMaxDepth, printLogInfos, superSampling);

	CONSOLE_LOG.close();
	if(success)	PlaySound(L"C:\\Windows\\Media\\notify.wav", NULL, SND_ASYNC );
	else		PlaySound(L"C:\\Windows\\Media\\chord.wav", NULL, SND_ASYNC );

	system("pause");
	return success;
}