#ifndef PATHTRACER_PATHTRACERDIALOG
#define PATHTRACER_PATHTRACERDIALOG


#include "../Controleur/PathTracer_PreProc.h"
#include "../Controleur/PathTracer_Utils.h"
#include "../Controleur/PathTracer_Structs.h"

//#define SWP_NOZORDER        0x0004
//#define SWP_NOSIZE          0x0001

namespace PathTracerNS
{
	class PathTracer;

	class PathTracerDialog
	{

	public:

		PathTracerDialog();

		void			PaintWindow(RGBAColor const * imageColor, uint const * imageRay);
		void			PaintTexture(Uchar4 const * global__texturesData, Texture& global__textures);


		inline void		SetWidth				(uint w)					{ pathTracerWidth = w; };
		inline void		SetHeight				(uint h)					{ pathTracerHeight = h; };


		//	Méthodes ici inutiles, mais qu'il faut implémenter par comatibilité avec la version LumenRT de PathTracer_Dialog

		inline void		TestPaintWindow			()							{};
		inline void		SetNumberOfTriangles	(uint n)					{};
		inline void		SetSizeOfBVH			(uint n)					{};
		inline void		SetMaxDepthOfBVH		(uint n)					{};
		inline void		SetRaysPerPixel			(uint n)					{};
		inline void		SetPixelPerSeconds		(uint n)					{};
		inline void		SetRaysPerSeconds		(uint n)					{};
		inline void		SetDepthsRaysStatistics	(ulong const * const stat)	{};
		inline void		UpdateElapsedTime		()							{};
		inline void		Create					()							{};
		inline void		OnClose					()							{};


		// Attributs

		uint				pathTracerWidth;
		uint				pathTracerHeight;
		bool				saveRenderedImages;

		static const char*	exportFolderPath;

	};

}


#endif