
#ifndef PATHTRACER_PREPROC
#define PATHTRACER_PREPROC


/***********************************************************************************/
//							SPECIFIC TO THE COMPUTER

#define PATHTRACER_HOME "C:\\Users\\djerbeti\\"
//#define PATHTRACER_HOME "C:\\Users\\Alexandre Djerbetian\\"

#define PATHTRACER_FOLDER PATHTRACER_HOME"Documents\\Visual Studio 2012\\Projects\\OpenCL_PathTracer\\src\\"
#define PATHTRACER_SCENE_FOLDER PATHTRACER_HOME"\\Pictures\\Maya\\"


/***********************************************************************************/
//							GLOBAL CONSTANTS

#define MAX_REFLECTION_NUMBER 10
#define MAX_INTERSETCION_NUMBER 5000 // for statistics - for both bbx and tri
#define BVH_MAX_DEPTH 30
#define MAX_LIGHT_SIZE 30
#define ALIGN(X) __declspec(align(X))


/***********************************************************************************/
//							SOME UTILITARIES

#include <assert.h>
#define ASSERT(X) assert(X)


/***********************************************************************************/
//					Variables depending on the project configuration

#ifdef MAYA

#include <maya/MIOStream.h>
#include <maya/MGlobal.h>
#include <fstream>
#define COUT cout
#define ENDL endl

#else

#include <iostream>
#define COUT std::cout
#define ENDL std::endl

#endif

/***********************************************************************************/
//							OUT STREAMS

class mstream // To out the stream in both the console and the log file
{
public:
	std::ofstream coss;
	mstream(void) : coss(PATHTRACER_SCENE_FOLDER"PathTracer_log.txt") {};

	void reset() {coss.close(); coss.open(PATHTRACER_SCENE_FOLDER"PathTracer_log.txt");};

	mstream& operator<< (std::ostream& (*pfun)(std::ostream&))
	{
		pfun(coss);
		pfun(COUT);
		return *this;
	};

};

template <class T>
mstream& operator<< (mstream& st, T val)
{
	st.coss << val;
	COUT << val;
	return st;
};

mstream extern console; // defined in PathTracer.cpp
#define CONSOLE console



#endif
