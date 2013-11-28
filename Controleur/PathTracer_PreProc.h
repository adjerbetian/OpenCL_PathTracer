
#ifndef PATHTRACER_PREPROC
#define PATHTRACER_PREPROC


/***********************************************************************************/
//							PARTIE SPECIFIQUE A LA MACHINE


//#define PATHTRACER_FOLDER "C:\\Users\\Alexandre Djerbetian\\Documents\\Visual Studio 2012\\Projects\\OpenCL_PathTracer\\src\\"
//#define PATHTRACER_SCENE_FOLDER "C:\\Users\\Alexandre Djerbetian\\Pictures\\Maya\\"
#define PATHTRACER_FOLDER "%HOMEPATH%\\Documents\\Visual Studio 2012\\Projects\\OpenCL_PathTracer\\src\\"
#define PATHTRACER_SCENE_FOLDER __FILE__"\\Pictures\\Maya\\"
/***********************************************************************************/


#define MAX_REFLECTION_NUMBER 10
#define BVH_MAX_DEPTH 30
#define MAX_LIGHT_SIZE 30
#define ALIGN(X) __declspec(align(X))



//	Cette variable permet d'enlever le conflit entre STR ou WSTR lorsque le projet est seul
//#define UNICODE

#ifdef MAYA
#include <maya/MIOStream.h>
#define CONSOLE cout
#define ENDL endl
#else
#include <iostream>
#define CONSOLE std::cout
#define ENDL std::endl
#endif

#include <assert.h>
#define RTASSERT(X) assert(X)


#endif
