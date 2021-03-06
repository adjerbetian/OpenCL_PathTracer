#ifndef PATHTRACER_PATHTRACERFILEIMPORTER
#define PATHTRACER_PATHTRACERFILEIMPORTER


#include "PathTracer_Utils.h"
#include "PathTracer_Importer.h"

#include <string>
#include <stdlib.h>
#include <stdio.h>

namespace PathTracerNS
{

	class PathTracerFileImporter : public PathTracerImporter
	{

	public:

		PathTracerFileImporter();
		virtual void Import(uint image_width, uint image_height, bool loadSky);
		void Export();

		static const std::string fileSizesPath;
		static const std::string filePointersPath;
		static const std::string fileTextureDataPath;
		
	};
}

#endif