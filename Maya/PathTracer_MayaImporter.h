#ifndef PATHTRACER_PATHTRACERLUMENRTIMPORTER
#define PATHTRACER_PATHTRACERLUMENRTIMPORTER

#include "../Controleur/PathTracer_Importer.h"
#include <maya\MString.h>
#include <maya\MDagPath.h>

namespace PathTracerNS
{

	class PathTracerMayaImporter : public PathTracerImporter
	{
	public :

		PathTracerMayaImporter():PathTracerImporter() {}


		virtual void Import				();

		void ImportScene				();
		void LoadSky					();

		void SetCam						();
		bool GetCam						(const MString &cameraName, MDagPath &camera);

		void Light_Create				();
		void Material_Create			(Material *This);
		void SunLight_Create			(SunLight *This);
		void Triangle_Create			(Triangle *This,
			Double4 const *s1, Double4 const *s2, Double4 const *s3,
			Float2 const *p1, Float2 const *p2, Float2 const *p3,
			Double4 const *n1, Double4 const *n2, Double4 const *n3,
			Double4 const *t1, Double4 const *t2, Double4 const *t3,
			Double4 const *bt1, Double4 const *bt2, Double4 const *bt3,
			uint matIndex );

		inline double		Triangle_MaxSquaredDistanceTo	(Triangle const	*This, Triangle const *t) { return max(max(Vector_SquaredDistanceTo(&This->S1, &t->S1), Vector_SquaredDistanceTo(&This->S2, &t->S2)), Vector_SquaredDistanceTo(&This->S3, &t->S3));};

	};



}


#endif