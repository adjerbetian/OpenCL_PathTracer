
#ifndef PATHTRACER_PREPROC
#define PATHTRACER_PREPROC


/***********************************************************************************/
//							PARTIE SPECIFIQUE A LA MACHINE


//#define PATHTRACER_FOLDER "m:\\LumenRT_src\\viewer\\Code\\Viewer\\pathtracer\\"
//#define PATHTRACER_SCENE_FOLDER "C:\\Users\\matrem\\Desktop\\Alexandre\\PathTracer_VS2010\\"

//#define PATHTRACER_FOLDER "k:\\lumenRT\\viewer\\Code\\Viewer\\pathtracer\\"
//#define PATHTRACER_SCENE_FOLDER "C:\\Users\\Viewer\\Desktop\\Alexandre\\PathTracer_VS2010\\"

//#define PATHTRACER_FOLDER "C:\\Users\\Alexandre\\Documents\\Visual_Studio_2010\\Projects\\PathTracer\\PathTracer\\src\\"
//#define PATHTRACER_SCENE_FOLDER "C:\\Users\\Alexandre\\Documents\\Visual_Studio_2010\\Projects\\PathTracer\\PathTracer\\"

#if !defined(PATHTRACER_FOLDER) || !defined(PATHTRACER_SCENE_FOLDER)
#error "variables PATHTRACER_FOLDER or PATHTRACER_SCENE_FOLDER are not defined."
#endif

/***********************************************************************************/


#define MAX_REFLECTION_NUMBER 10
#define BVH_MAX_DEPTH 30
#define MAX_LIGHT_SIZE 30
#define ALIGN(X) __declspec(align(X))


#ifdef LRT_PRODUCT

#include "Tools/Defines.h"
#include "toolsLog.h"
#define CONSOLE tools::ConsoleOut()
#define ENDL tools::endl


#else

//	Cette variable permet d'enlever le conflit entre STR ou WSTR lorsque le projet est seul
#define UNICODE

#include <assert.h>
#include <iostream>
#define RTASSERT(X) assert(X)
#define CONSOLE std::cout
#define ENDL std::endl

#endif


#endif
