
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
#define BVH_MAX_DEPTH 30
#define MAX_LIGHT_SIZE 30
#define ALIGN(X) __declspec(align(X))


/***********************************************************************************/
//							SOME UTILITARIES

#include <assert.h>
#define ASSERT(X) assert(X)


/***********************************************************************************/
//							OUT STREAMS


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

//class teebuf: public std::streambuf {
//public:
//	typedef std::char_traits<char> traits_type;
//	typedef traits_type::int_type  int_type;
//
//	teebuf(std::streambuf* sb1, std::streambuf* sb2):
//		m_sb1(sb1),
//		m_sb2(sb2)
//	{}
//	int_type overflow(int_type c) {
//		if (m_sb1->sputc(c) == traits_type::eof()
//			|| m_sb1->sputc(c) == traits_type::eof())
//			return traits_type::eof();
//		return c;
//	}
//private:
//	std::streambuf* m_sb1;
//	std::streambuf* m_sb2;
//};


class mstream // To out the stream in both the console and the log file
{
public:
	std::ofstream coss;
	mstream(void) : coss(PATHTRACER_SCENE_FOLDER"PathTracer_log.txt") {};

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
