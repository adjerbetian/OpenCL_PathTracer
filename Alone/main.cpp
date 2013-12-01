

#include "../Controleur/PathTracer.h"
#include "../Controleur/PathTracer_FileImporter.h"

int main()
{
	PathTracerNS::PathTracer_SetImporter(new PathTracerNS::PathTracerFileImporter());
	PathTracerNS::PathTracer_Main(1000, true, true);
	//system("pause");
}