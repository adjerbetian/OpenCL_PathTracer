#ifndef PATHTRACER_PATHTRACERLUMENRTIMPORTER
#define PATHTRACER_PATHTRACERLUMENRTIMPORTER

#include "../Controleur/PathTracer_Importer.h"
#include <maya/MString.h>
#include <maya/MDagPath.h>
#include <maya/MPoint.h>
#include <maya/MColor.h>
#include <maya/MVector.h>

#include <maya/MFnMesh.h>

#include <maya/MFnLight.h>
#include <maya/MFnPointLight.h>
#include <maya/MFnDirectionalLight.h>
#include <maya/MFnSpotLight.h>

#include <maya/MFnLambertShader.h>
#include <maya/MFnPhongShader.h>
#include <maya/MFnBlinnShader.h>

namespace PathTracerNS
{

	inline Float4	permute_xyz_to_zxy	(const MPoint &m)	{return Float4((float) (m.z/m.w), (float) (m.x/m.w), (float) (m.y/m.w), 1.f);};
	inline Float4	permute_xyz_to_xzy	(const MPoint &m)	{return Float4((float) (m.x/m.w), (float) (m.z/m.w), (float) (m.y/m.w), 1.f);};

	inline Float4	permute_xyz_to_zxy	(const MVector &v)	{return Float4((float) v.z, (float) v.x, (float) v.y, 0.f);};
	inline Float4	permute_xyz_to_xzy	(const MVector &v)	{return Float4((float) v.x, (float) v.z, (float) v.y, 0.f);};

	inline Float4 ToFloat4 (const MPoint& v)	{return Float4((float) (v.x/v.w), (float) (v.y/v.w), (float) (v.z/v.w), 1.f);};
	inline Float4 ToFloat4 (const MVector& v)	{return Float4((float) v.x, (float) v.y, (float) v.z, 1.f);};
	inline Float4 ToFloat4 (const MColor& v)	{return Float4((float) v.r, (float) v.g, (float) v.b, v.a);};

	class PathTracerMayaImporter : public PathTracerImporter
	{
	public :

		PathTracerMayaImporter():PathTracerImporter() {}


		virtual bool Import				(bool loadSky);

		void ImportScene				();
		void ImportMaterials			();
		void ImportLights				();
		void LoadSky					(bool loadSky);

		void SetCam						();
		bool GetCam						(const MString &cameraName, MDagPath &camera);

		int			CountMaterials		();
		void		CreateMaterials		();
		RGBAColor	OutputColor			(MFnDependencyNode const& fn,const char* name);
		int			Get_MeshMaterialId	(MFnMesh const& fnMesh);


		void Light_Create				(Light& l, const MMatrix& M, const MFnPointLight& fnLight);
		void Light_Create				(Light& l, const MMatrix& M, const MFnDirectionalLight& fnLight);
		void Light_Create				(Light& l, const MMatrix& M, const MFnSpotLight& fnLight);

		void Material_Create			(Material *This, MFnPhongShader   const& fn);
		void Material_Create			(Material *This, MFnLambertShader const& fn);
		void Material_Create			(Material *This, MFnBlinnShader   const& fn);

		void Triangle_Create			(Triangle *This,
			Float4 const *s1, Float4 const *s2, Float4 const *s3,
			Float2 const *p1, Float2 const *p2, Float2 const *p3,
			Float4 const *n1, Float4 const *n2, Float4 const *n3,
			Float4 const *t1, Float4 const *t2, Float4 const *t3,
			Float4 const *bt1, Float4 const *bt2, Float4 const *bt3,
			uint positiveMatIndex, uint negativeMatIndex );

		inline double		Triangle_MaxSquaredDistanceTo	(Triangle const	*This, Triangle const *t) { return std::max<float>(std::max<float>(Vector_SquaredDistanceTo(&This->S1, &t->S1), Vector_SquaredDistanceTo(&This->S2, &t->S2)), Vector_SquaredDistanceTo(&This->S3, &t->S3));};

	};



}


#endif