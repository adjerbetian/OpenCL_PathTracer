#include "raytracer.h"
#include "../Controleur/PathTracer.h"
#include "../Maya/PathTracer_MayaImporter.h"

#include <maya/MFnPlugin.h>

MStatus initializePlugin(MObject obj)
//
//	Description:
//		this method is called when the plug-in is loaded into Maya.  It 
//		registers all of the services that this plug-in provides with 
//		Maya.
//
//	Arguments:
//		obj - a handle to the plug-in object (use MFnPlugin to access it)
//
{ 
	MFnPlugin plugin(obj, "IS", "1.0", "Any");

#ifdef _DEBUG
	MStatus status = plugin.registerCommand("raytrace_d", RayTracer::creator);
#else
	MStatus status = plugin.registerCommand("raytrace_r", RayTracer::creator);
#endif
	CHECK_MSTATUS_AND_RETURN_IT(status);
	return status;
}

MStatus uninitializePlugin(MObject obj)
//
//	Description:
//		this method is called when the plug-in is unloaded from Maya. It 
//		deregisters all of the services that it was providing.
//
//	Arguments:
//		obj - a handle to the plug-in object (use MFnPlugin to access it)
//
{
	MFnPlugin plugin(obj);

#ifdef _DEBUG
	MStatus status = plugin.deregisterCommand("raytrace_d");
#else
	MStatus status = plugin.deregisterCommand("raytrace_r");
#endif

	CHECK_MSTATUS_AND_RETURN_IT(status);
	return status;
}
