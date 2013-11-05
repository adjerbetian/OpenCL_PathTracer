#ifndef PATHTRACER_PATHTRACERLUMENRTIMPORTER
#define PATHTRACER_PATHTRACERLUMENRTIMPORTER

#include "../Controleur/PathTracer_Importer.h"

namespace PathTracerNS
{

	class PathTracerMayaImporter : public PathTracerImporter
	{
	public :

		PathTracerMayaImporter():PathTracerImporter() {}


		virtual void Import				();

		void ImportScene				();

		void SetCam						();
		bool GetCam						(const MString &cameraName, MDagPath &camera);

		void Light_Create				();
		void Material_Create			(Material *This);
		void SunLight_Create			(SunLight *This);
		void Triangle_Create			(Triangle *This,
			Float4 const *s1, Float4 const *s2, Float4 const *s3,
			Float2 const *p1, Float2 const *p2, Float2 const *p3,
			Float4 const *n1, Float4 const *n2, Float4 const *n3,
			Float4 const *t1, Float4 const *t2, Float4 const *t3,
			Float4 const *bt1, Float4 const *bt2, Float4 const *bt3,
			uint matIndex );

		inline float		Triangle_MaxSquaredDistanceTo	(Triangle const	*This, Triangle const *t) { return max(max(Vector_SquaredDistanceTo(&This->S1, &t->S1), Vector_SquaredDistanceTo(&This->S2, &t->S2)), Vector_SquaredDistanceTo(&This->S3, &t->S3));};

	};



}


#endif