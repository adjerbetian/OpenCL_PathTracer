#ifndef PATHTRACER_PATHTRACERLUMENRTIMPORTER
#define PATHTRACER_PATHTRACERLUMENRTIMPORTER

#include "Viewer/Headers_Core.h"
#include "Viewer/Headers_UI.h"
#include "RealTimeBuilder/Headers.h"
#include "RealTimeEngine/PublicHeader.h"
#include "toolsLog.h"

#include "dds/ATI_Compress.h"

#pragma comment(lib, "ATI_Compress_MT_DLL_VC9.lib")

#include "../Controleur/PathTracer_Importer.h"

namespace PathTracerNS
{

	class Matrix44
	{
	public:
		Matrix44()
		{
			for(int i=0; i<4; i++)
				for(int j=0; j<4; j++)
					m[i][j] = 0;
		}

		Matrix44(const RealTimeBuilder::RTBStdObject& rtb_object, uint instanceId)
		{
			Matrix4x4 m4x4;
			rtb_object.GetWorldMatrix(m4x4, instanceId);
			for(int i=0; i<4; i++)
				for(int j=0; j<4; j++)
					m[i][j] = m4x4[j][i];

		}


		inline const float& operator () (int i, int j) const	{ return m[i][j];};
		inline float& operator () (int i, int j)				{ return m[i][j];};

		Float4 Mult(const Float4& v) const
		{
			Float4 u = Float4(
				m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z + m[0][3],
				m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z + m[1][3],
				m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z + m[2][3],
				0);

			float w = m[3][0]*v.x + m[3][1]*v.y + m[3][2]*v.z + m[3][3];
			u /= w;
			return u;
		}

		float m[4][4];

	};



	class PathTracerLumenRTImporter : public PathTracerImporter
	{
	public :

		PathTracerLumenRTImporter(Viewer::Scene const *lumenRTScene):PathTracerImporter()
		{
			scene = lumenRTScene;
		}


		virtual void Import				();

		void ImportScene				();
		void SetCam						( const RealTimeEngine::DataType::Camera& camera);
		void ImportObject				( const RealTimeBuilder::RTBStdObject& rtb_object, uint materialId, std::list<Triangle>& triangulationList);
		void MergeTriangulation			( std::list<Triangle>& triangulationList);

		void Light_Create				( Light *This, RealTimeBuilder::RTBLight const *l);
		void Material_Create			( Material *This, RealTimeBuilder::BuilderMaterial *builderMaterial, std::map<std::string, uint> *texturesNameVSId);
		uint Sky_GetTextureDataOffset	( RealTimeBuilder::RTBSky const *rtb_sky);
		void Sky_Create					( Sky *This, RealTimeBuilder::RTBSky const *rtb_sky, Float4 const *sunDirection );
		void SunLight_Create			( SunLight *This, RealTimeBuilder::RTBLight const *l );
		uint Texture_Create				( Texture *This, const RealTimeBuilder::CDDSTextureInMemory& rtb_texture, ATI_TC_Texture *ATITexture, uint offset);
		uint Texture_CreateFromCubeMap	( Texture *This, const RealTimeBuilder::CDDSTextureInMemory& rtb_texture, ATI_TC_Texture *ATITexture, int faceId, uint offset);
		void Triangle_MergeWith			(Triangle		*This, Triangle const *t);
		void Triangle_Create			(Triangle *This,
			Float4 const *s1, Float4 const *s2, Float4 const *s3,
			Float2 const *p1, Float2 const *p2, Float2 const *p3,
			Float4 const *n1, Float4 const *n2, Float4 const *n3,
			Float4 const *t1, Float4 const *t2, Float4 const *t3,
			Float4 const *bt1, Float4 const *bt2, Float4 const *bt3,
			uint matIndex );

		inline float		Triangle_MaxSquaredDistanceTo	(Triangle const	*This, Triangle const *t) { return max(max(Vector_SquaredDistanceTo(&This->S1, &t->S1), Vector_SquaredDistanceTo(&This->S2, &t->S2)), Vector_SquaredDistanceTo(&This->S3, &t->S3));};

		static Viewer::Scene const *scene;

	};



}


#endif