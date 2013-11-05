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

	MStatus status = plugin.registerCommand("raytrace", RayTracer::creator);
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

	MStatus status = plugin.deregisterCommand("raytrace");
	CHECK_MSTATUS_AND_RETURN_IT(status);
	return status;
}
