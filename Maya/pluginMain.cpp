#include "raytracer.h"
#include "../Controleur/PathTracer.h"
#include "../Maya/PathTracer_MayaImporter.h"

#include <maya/MFnPlugin.h>

using namespace std;

extern "C"
#ifdef _WIN32
__declspec(dllexport)
#endif
MStatus initializePlugin(MObject obj)
{ 
	MFnPlugin plugin(obj, "IS", "1.0", "Any");
	MStatus status = plugin.registerCommand(COMMAND, RayTracer::creator, RayTracer::newSyntax);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	return status;
}


extern "C"
#ifdef _WIN32
__declspec(dllexport)
#endif
MStatus uninitializePlugin(MObject obj)
{
	MFnPlugin plugin(obj);
	MStatus status = plugin.deregisterCommand(COMMAND);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	return status;
}
