
#include "PathTracer_LumenRTImporter.h"
#include "PathTracer_TextureImporter.h"


namespace PathTracerNS
{

	Viewer::Scene const * PathTracerLumenRTImporter::scene = NULL;

	void PathTracerLumenRTImporter::Import()
	{
		SetCam(scene->GetMainCamera().GetData());
		ImportScene();
	}


	void PathTracerLumenRTImporter::SetCam(const RealTimeEngine::DataType::Camera& camera)
	{
		*ptr__global__imageWidth = camera.GetWidth();
		*ptr__global__imageHeight = camera.GetHeight();
		*ptr__global__imageSize = (*ptr__global__imageWidth) * (*ptr__global__imageHeight);

		ptr__global__cameraDirection->x = camera.GetViewDirection().x;
		ptr__global__cameraDirection->y = camera.GetViewDirection().y;
		ptr__global__cameraDirection->z = camera.GetViewDirection().z;
		ptr__global__cameraDirection->w = 0;

		ptr__global__cameraPosition->x  = camera.GetPosition().x;
		ptr__global__cameraPosition->y  = camera.GetPosition().y;
		ptr__global__cameraPosition->z  = camera.GetPosition().z;
		ptr__global__cameraPosition->w  = 0;

		float3 cameraDirectionX3, cameraDirectionY3;
		camera.ScreenToVector(cameraDirectionX3, float2(1,0));
		camera.ScreenToVector(cameraDirectionY3, float2(0,1));

		ptr__global__cameraScreenX->x = cameraDirectionX3.x;
		ptr__global__cameraScreenX->y = cameraDirectionX3.y;
		ptr__global__cameraScreenX->z = cameraDirectionX3.z;
		ptr__global__cameraScreenX->w = 0;

		ptr__global__cameraScreenY->x = cameraDirectionY3.x;
		ptr__global__cameraScreenY->y = cameraDirectionY3.y;
		ptr__global__cameraScreenY->z = cameraDirectionY3.z;
		ptr__global__cameraScreenY->w = 0;

		(*ptr__global__cameraScreenX) /= dot((*ptr__global__cameraScreenX), (*ptr__global__cameraDirection));
		(*ptr__global__cameraScreenY) /= dot((*ptr__global__cameraScreenY), (*ptr__global__cameraDirection));

		(*ptr__global__cameraScreenX) -= (*ptr__global__cameraDirection);
		(*ptr__global__cameraScreenY) -= (*ptr__global__cameraDirection);

	}





	void PathTracerLumenRTImporter::ImportScene()
	{

		std::map<std::string, uint> texturesNameVSId;
		uint offset = 0;

		// CIEL
		RealTimeBuilder::RTBSky rtb_sky = scene->rtbScene.GetSky();
		offset = Sky_GetTextureDataOffset(&rtb_sky);

		// TEXTURES

		(*ptr__global__texturesSize) = (uint) scene->rtbScene.GetGlobalTextureCount();
		(*ptr__global__textures) = new Texture[(*ptr__global__texturesSize)];

		ATI_TC_Texture *ATITextureArray = new ATI_TC_Texture[*ptr__global__texturesSize];

		for(uint i = 0; i < (*ptr__global__texturesSize); i++ )
		{
			offset = Texture_Create( &((*ptr__global__textures)[i]), scene->rtbScene.GetGlobalTexture(i), &ATITextureArray[i], offset);
			texturesNameVSId[(std::string) scene->rtbScene.GetGlobalTexture(i).GetName()] = i;
		}

		*ptr__global__textureDataSize = offset;
		*ptr__global__textureData = new Uchar4[*ptr__global__textureDataSize];
		for(uint i = 0; i < (*ptr__global__texturesSize); i++ )
		{
			uint texOffset = (*ptr__global__textures)[i].offset;
			uint endIndex = ATITextureArray[i].dwWidth * ATITextureArray[i].dwHeight;
			for(int j = 0; j < endIndex; j++)
				(*ptr__global__textureData)[texOffset + j] = *((Uchar4 *) &ATITextureArray[i].pData[4*j]);

			//uint copySize = ATITextureArray[i].dwWidth * ATITextureArray[i].dwHeight * sizeof(Float4);
			//memcpy( &((*ptr__global__textureData)[texOffset]) , ATITextureArray[i].pData, copySize);
			delete[] ATITextureArray[i].pData;
		}

		delete[] ATITextureArray;


		// OBJETS

		std::list<Triangle> triangulationList;
		uint objectCount = (uint) scene->rtbScene.GetObjectCount();
		*ptr__global__materiauxSize = objectCount;
		*ptr__global__materiaux = new Material[*ptr__global__materiauxSize];

		for(uint i = 0; i < objectCount; i++)
		{
			const RealTimeBuilder::RTBStdObject rtb_object = scene->rtbScene.GetObject(i);
			RealTimeBuilder::BuilderMaterial rtb_Material = rtb_object.GetMaterial(0);
			Material_Create( &((*ptr__global__materiaux)[i]), &rtb_Material, &texturesNameVSId);
			ImportObject(rtb_object, i, triangulationList);
		}

		MergeTriangulation(triangulationList);

		// LUMIERES

		*ptr__global__lightsSize = (uint) scene->rtbScene.GetLightCount() - 1;
		*ptr__global__lights = new Light[*ptr__global__lightsSize];
		int indexLight = 0;
		for(uint i = 0; i < *ptr__global__lightsSize + 1; i++)
		{
			RealTimeBuilder::RTBLight l = scene->rtbScene.GetLight(i);
			if(l.IsDirectional())
				SunLight_Create( ptr__global__sun, &l);
			else
				Light_Create( &((*ptr__global__lights)[indexLight++]), &l );
		}

		// CIEL
		Sky_Create( ptr__global__sky, &rtb_sky, &ptr__global__sun->direction);

	}

	uint GetIndex(char *buffer, uint i, uint indexSize)
	{
		if (indexSize == 2)
		{
			VBT::UInt16 *p = (VBT::UInt16 *)(buffer + i * indexSize);
			return uint(*p);
		}
		else
		{
			VBT::UInt32 *p = (VBT::UInt32 *)(buffer + i * indexSize);
			return uint(*p);
		}
	}

	void PathTracerLumenRTImporter::ImportObject(const RealTimeBuilder::RTBStdObject& rtb_object, uint materialId, std::list<Triangle>& triangulationList)
	{
		RealTimeBuilder::RTBMesh mesh = rtb_object.GetMesh();

		Float4 triangleVerticesPositions[3];
		Float4 triangleVerticesNormals[3];
		Float4 triangleVerticesTangentes[3];
		Float4 triangleVerticesBiTangentes[3];
		Float2 triangleUVVertices[3];

		uint nbInstances = (uint) rtb_object.GetNumMatrices();
		for(uint instanceId = 0; instanceId < nbInstances; instanceId++)
		{
			Matrix44 transformMatrix(rtb_object, instanceId);

			for (index_t i = 0; i < mesh.GetSubMeshCount(); i++)
			{
				int lodIndex = 0;
				char *indexBuffer = (char*)mesh.GetIndexBufferPtr(i, lodIndex);

				// check position is 3 floats at the begining of the vertex (should be always the case)
				SComponentDesc desc;
				mesh.GetVertexBufferDesc(0, desc);
				RTASSERT(desc.Usage == SComponentDesc::e_Position);
				RTASSERT(desc.Type == SComponentDesc::e_Float);
				RTASSERT(desc.Count == 4);
				RTASSERT(desc.Offset == 0);

				int uvOffset = -1, normalOffset = -1, tangenteOffset = -1, biTangenteOffset = -1;
				for (int j = 0; j < mesh.GetVertexBufferComponentCount(); j++)
				{
					SComponentDesc desc;
					mesh.GetVertexBufferDesc(j, desc);
					if(desc.Usage == SComponentDesc::e_UV0)
						uvOffset = desc.Offset;
					if(desc.Usage == SComponentDesc::e_Normal)
						normalOffset = desc.Offset;
					if(desc.Usage == SComponentDesc::e_Tangent)
						tangenteOffset = desc.Offset;
					if(desc.Usage == SComponentDesc::e_BiNormal)
						biTangenteOffset = desc.Offset;

				}
				RTASSERT(uvOffset != -1 && normalOffset != -1 && tangenteOffset != -1 && biTangenteOffset != -1);

				uint vertexSize = (uint) mesh.GetVertexSize(0);
				uint indexSize = (uint) mesh.GetSizeOfIndex();
				uint nbVertex = (uint) mesh.GetVertexCount();
				char* vb = (char*) mesh.GetVertexBufferPtr(0);

				uint indexCount = (uint) mesh.GetIndexCount(i, lodIndex);
				for(uint j = 0; j < indexCount; j += 3)
				{
					for (uint k = 0; k < 3; ++k)
					{
						uint ind = GetIndex(indexBuffer, j + k, indexSize);
						RTASSERT(ind < nbVertex);
						float px = *((float*) (vb + vertexSize * ind));
						float py = *((float*) (vb + vertexSize * ind) + 1);
						float pz = *((float*) (vb + vertexSize * ind) + 2);

						float u = *((float*) (vb + vertexSize * ind + uvOffset));
						float v = *((float*) (vb + vertexSize * ind + uvOffset) + 1);

						float nx = *((float*) (vb + vertexSize * ind + normalOffset));
						float ny = *((float*) (vb + vertexSize * ind + normalOffset) + 1);
						float nz = *((float*) (vb + vertexSize * ind + normalOffset) + 2);

						float tx = *((float*) (vb + vertexSize * ind + tangenteOffset));
						float ty = *((float*) (vb + vertexSize * ind + tangenteOffset) + 1);
						float tz = *((float*) (vb + vertexSize * ind + tangenteOffset) + 2);

						float btx = *((float*) (vb + vertexSize * ind + biTangenteOffset));
						float bty = *((float*) (vb + vertexSize * ind + biTangenteOffset) + 1);
						float btz = *((float*) (vb + vertexSize * ind + biTangenteOffset) + 2);

						Float4 pObject = Float4(px, py, pz, 0);
						Float4 pInstance = Float4(transformMatrix.Mult(pObject));
						triangleVerticesPositions[k] = pInstance;

						triangleUVVertices[k] = Float2(u, v);

						Float4 nObject = Float4(nx, ny, nz, 0);
						triangleVerticesNormals[k] = nObject;

						Float4 tObject = Float4(tx, ty, tz, 0);
						triangleVerticesTangentes[k] = tObject;

						Float4 btObject = Float4(btx, bty, btz, 0);
						triangleVerticesBiTangentes[k] = btObject;

					}

					Triangle triangle;
					Triangle_Create(&triangle,
						&triangleVerticesPositions[0],	&triangleVerticesPositions[1],	&triangleVerticesPositions[2],
						&triangleUVVertices[0],			&triangleUVVertices[1],			&triangleUVVertices[2],
						&triangleVerticesNormals[0],	&triangleVerticesNormals[1],	&triangleVerticesNormals[2],
						&triangleVerticesTangentes[0],	&triangleVerticesTangentes[1],	&triangleVerticesTangentes[2],
						&triangleVerticesBiTangentes[0],&triangleVerticesBiTangentes[1],&triangleVerticesBiTangentes[2],
						materialId );

					triangulationList.push_back(triangle);
				}
			}
		}
	}


	void PathTracerLumenRTImporter::MergeTriangulation(std::list<Triangle>& triangulationList)
	{
		RTASSERT(triangulationList.size() % 2 == 0);
		*ptr__global__triangulationSize = (uint) triangulationList.size()/2;

		std::list<Triangle>::iterator it1 = triangulationList.begin();
		std::list<Triangle>::iterator it2 = triangulationList.begin();
		std::list<Triangle>::iterator itMinDist = triangulationList.begin();

		int iTriangulation = 0;
		float minDist, dist;

		while(it1!=triangulationList.end())
		{
			it2 = it1;
			it2++;
			minDist = std::numeric_limits<float>::infinity();

			while(it2 != triangulationList.end())
			{
				dist = Triangle_MaxSquaredDistanceTo(&(*it1), &(*it2));
				if(dist < minDist)
				{
					minDist = dist;
					itMinDist = it2;
				}
				it2++;
			}

			if(minDist < 1e-8)
			{
				Triangle_MergeWith(&(*it1), &(*itMinDist));
				triangulationList.erase(itMinDist);
			}

			iTriangulation++;
			it1++;
		}

		RTASSERT(triangulationList.size() == *ptr__global__triangulationSize);

		// recopie

		*ptr__global__triangulation = new Triangle[*ptr__global__triangulationSize];
		it1 = triangulationList.begin();
		for(uint i=0; i<*ptr__global__triangulationSize; i++, it1++)
			(*ptr__global__triangulation)[i] = *it1;
	}

	void PathTracerLumenRTImporter::Light_Create( Light *This, RealTimeBuilder::RTBLight const *l)
	{
		This->position	= Float4(l->GetPosition().x, l->GetPosition().y, l->GetPosition().z, 0.f);
		This->direction = Float4(l->GetDirection().x, l->GetDirection().y, l->GetDirection().z, 0.f);
		This->color		= RGBAColor(l->GetColor().x, l->GetColor().y, l->GetColor().z, 0.f);

		This->power = l->GetNominalPower();
		This->cosOfInnerFallOffAngle = cos(l->GetInnerFalloffAngle());
		This->cosOfOuterFallOffAngle = cos(l->GetOuterFalloffAngle());
		This->type = (l->IsPoint() ? LIGHT_POINT : (l->IsSpot() ? LIGHT_SPOT : LIGHT_UNKNOWN));

		RTASSERT( This->type != LIGHT_UNKNOWN );
	};

	void PathTracerLumenRTImporter::SunLight_Create( SunLight *This, RealTimeBuilder::RTBLight const *l )
	{
		RTASSERT(l->IsDirectional());

		This->direction = Float4(l->GetDirection().x, l->GetDirection().y, l->GetDirection().z, 0.f);
		This->color = RGBAColor(l->GetColor().x, l->GetColor().y, l->GetColor().z, 0.f);

		This->power		= l->GetNominalPower();
	};

	void PathTracerLumenRTImporter::Material_Create( Material *This, RealTimeBuilder::BuilderMaterial *builderMaterial, std::map<std::string, uint> *texturesNameVSId)
	{
		This->type = MAT_UNKNOWN;
		This->textureName = NULL;
		This->textureId = 0;
		This->isSimpleColor = true;
		This->hasAlphaMap = false;
		This->opacity = 1;

		common_viewer::MaterialType builderType = builderMaterial->GetType();

		//////////////////////////////////////////////////////////////////////////
		//Definition du type
		//////////////////////////////////////////////////////////////////////////

		if(builderType == common_viewer::MaterialType::Opaque)
			This->type = MAT_STANDART;
		else if(builderType == common_viewer::MaterialType::Water)
			This->type = MAT_WATER;
		else if(builderType == common_viewer::MaterialType::Glass)
			This->type = MAT_GLASS;
		else if(builderType == common_viewer::MaterialType::ReflectiveOpaque)
			This->type = MAT_VARNHISHED;

		RTASSERT(This->type != MAT_UNKNOWN);

		//////////////////////////////////////////////////////////////////////////
		//Import des paramètres
		//////////////////////////////////////////////////////////////////////////

		for(int j=0; j < builderMaterial->GetBoolParameterCount(); j++)
		{
			if(builderMaterial->GetBoolParameterName(j) == "bSimpleColor")
				This->isSimpleColor = builderMaterial->GetBoolParameterValue(j);
			if(builderMaterial->GetBoolParameterName(j) == "bAlphaTest")
				This->hasAlphaMap = builderMaterial->GetBoolParameterValue(j);
		}

		if(This->isSimpleColor) // On récupère la couleur
		{
			for(int j=0; j < builderMaterial->GetVectorParameterCount(); j++)
			{
				if(builderMaterial->GetVectorParameterName(j) == "ObjColor")
					This->simpleColor =  RGBAColor(
					builderMaterial->GetVectorParameterValue(j).r,
					builderMaterial->GetVectorParameterValue(j).g,
					builderMaterial->GetVectorParameterValue(j).b,
					0.f);
			}
		}
		else // On récupère la texture
		{
			RTASSERT(builderMaterial->GetTextureParameterCount()>0);

			// ATTENTION : le cas de plusieurs textures n'est pas traite

			std::string textureName = builderMaterial->GetTextureParameterName(0);
			if(textureName == "ReflectPlaneTexture")
			{
				if(builderMaterial->GetTextureParameterCount()>1)
				{
					textureName = builderMaterial->GetTextureParameterName(1);
					RTASSERT(texturesNameVSId->find(textureName) != texturesNameVSId->end());
					This->textureId = (*texturesNameVSId)[textureName];
					This->textureName = textureName.c_str();
				}
			}
			else 
			{
				RTASSERT(texturesNameVSId->find(textureName) != texturesNameVSId->end());
				This->textureId = (*texturesNameVSId)[textureName];
				This->textureName = textureName.c_str();
			}
		}

		if(This->type == MAT_GLASS) // On récupère le degré de transparence
		{
			for(int j=0; j < builderMaterial->GetFloatParameterCount(); j++)
			{
				if(builderMaterial->GetFloatParameterName(j) == "Transparency")
					This->opacity = builderMaterial->GetFloatParameterValue(j);
			}
		}

		//if(type == WATER) // On récupère l'absorption
		//{
		//	waterAbsorptionCoeff = builderMaterial->GetGlobalAlphaAtt();
		//}

		//Verification de presence de donnees alpha
		if(!This->isSimpleColor)
			This->hasAlphaMap &= Texture_HasAlpha( &(*ptr__global__textures)[This->textureId], *ptr__global__textureData );

	}


	void PathTracerLumenRTImporter::Sky_Create( Sky *This, RealTimeBuilder::RTBSky const *rtb_sky, Float4 const *sunDirection )
	{
		This->groundScale = 1.0f;

		ATI_TC_Texture ATITextureArray[6];

		uint offset = 0;
		int i;
		for(i = 0; i < 3; i++)
			offset = Texture_CreateFromCubeMap( &This->skyTextures[i], rtb_sky->GetTexture(0), &ATITextureArray[i], i+1, offset);
		offset = Texture_Create( &This->skyTextures[i] , rtb_sky->GetTexture(1), &ATITextureArray[i], offset );
		for(i = 4; i < 6; i++)
			offset = Texture_CreateFromCubeMap( &This->skyTextures[i], rtb_sky->GetTexture(0), &ATITextureArray[i], i+1, offset);

		for(i=0; i<6; i++)
		{
			uint texOffset = This->skyTextures[i].offset;
			uint endIndex = ATITextureArray[i].dwWidth * ATITextureArray[i].dwHeight;
			for(int j = 0; j < endIndex; j++)
				(*ptr__global__textureData)[texOffset + j] = *((Uchar4 *) &ATITextureArray[i].pData[4*j]);

			delete[] ATITextureArray[i].pData;
		}

		for(i = 0; i < rtb_sky->GetMaterial().GetVectorParameterCount(); i++)
		{
			if(rtb_sky->GetMaterial().GetVectorParameterName(i) == "Exposant_Factor")
			{
				This->exposantFactorX = rtb_sky->GetMaterial().GetVectorParameterValue(i).x;
				This->exposantFactorY = std::abs(rtb_sky->GetMaterial().GetVectorParameterValue(i).y);
			}
		}

		// La carte doit etre tournée pour que le soleil soit vers l'axe -X, d'où cet angle
		float angle = PI - atan2(sunDirection->y, sunDirection->x);
		This->cosRotationAngle = cos(angle);
		This->sinRotationAngle = sin(angle);

	}

	uint PathTracerLumenRTImporter::Sky_GetTextureDataOffset( RealTimeBuilder::RTBSky const *rtb_sky)
	{
		ATI_TC_Texture ATICompressedTexture;

		uint offset = 0;
		int i;
		for(i = 0; i < 3; i++)
		{
			ATICompressedTexture = Texture_LoadDDSFromCubeMapBuffer( (char *) rtb_sky->GetTexture(0).GetBuffer(), i+1);
			offset += ATICompressedTexture.dwWidth*ATICompressedTexture.dwHeight;
		}
		ATICompressedTexture = Texture_LoadDDSFromBuffer( (char *) rtb_sky->GetTexture(1).GetBuffer());
		offset += ATICompressedTexture.dwWidth*ATICompressedTexture.dwHeight;

		for(i = 4; i < 6; i++)
		{
			ATICompressedTexture = Texture_LoadDDSFromCubeMapBuffer( (char *) rtb_sky->GetTexture(0).GetBuffer(), i+1);
			offset += ATICompressedTexture.dwWidth*ATICompressedTexture.dwHeight;
		}

		return offset;
	}

	uint PathTracerLumenRTImporter::Texture_Create( Texture *This, const RealTimeBuilder::CDDSTextureInMemory& rtb_texture, ATI_TC_Texture *ATITexture, uint offset)
	{
		ATI_TC_Texture compressedTexture;

		compressedTexture = Texture_LoadDDSFromBuffer((char*) rtb_texture.GetBuffer());
		Texture_Decompress(compressedTexture, *ATITexture);
		delete[] compressedTexture.pData;

		This->width = ATITexture->dwWidth;
		This->height = ATITexture->dwHeight;
		This->offset = offset;

		return This->offset + ( This->width * This->height );
	}

	uint PathTracerLumenRTImporter::Texture_CreateFromCubeMap( Texture *This, const RealTimeBuilder::CDDSTextureInMemory& rtb_texture, ATI_TC_Texture *ATITexture, int faceId, uint offset)
	{
		ATI_TC_Texture compressedTexture;
		
		compressedTexture = Texture_LoadDDSFromCubeMapBuffer((char*) rtb_texture.GetBuffer(), faceId);
		Texture_Decompress(compressedTexture, *ATITexture);

		This->width = ATITexture->dwWidth;
		This->height = ATITexture->dwHeight;
		This->offset = offset;

		return This->offset + ( This->width * This->height );
	}



	void PathTracerLumenRTImporter::Triangle_Create(Triangle *This,
		Float4 const *s1, Float4 const *s2, Float4 const *s3,
		Float2 const *p1, Float2 const *p2, Float2 const *p3,
		Float4 const *n1, Float4 const *n2, Float4 const *n3,
		Float4 const *t1, Float4 const *t2, Float4 const *t3,
		Float4 const *bt1, Float4 const *bt2, Float4 const *bt3,
		uint matIndex )
	{
		This->materialWithPositiveNormalIndex = matIndex;
		This->materialWithNegativeNormalIndex = 0;

		BoundingBox_Create( &This->AABB, s1, s2, s3);

		This->N = normalize(cross( (*s2)-(*s1), (*s3)-(*s1) ));

		if(Vector_LexLessThan(s1, s2))
		{
			if(Vector_LexLessThan(s2, s3))
			{
				This->S1 = (*s1);	This->S2 = (*s2);	This->S3 = (*s3);
				This->UVP1 = (*p1);	This->UVP2 = (*p2);	This->UVP3 = (*p3);
				This->N1 = (*n1);	This->N2 = (*n2);	This->N3 = (*n3);
				This->T1 = (*t1);	This->T2 = (*t2);	This->T3 = (*t3);
				This->BT1 = (*bt1);	This->BT2 = (*bt2);	This->BT3 = (*bt3);
			}
			else if(Vector_LexLessThan(s1, s3))
			{
				This->S1 = (*s1);	This->S2 = (*s3);	This->S3 = (*s2);
				This->UVP1 = (*p1);	This->UVP2 = (*p3);	This->UVP3 = (*p2);
				This->N1 = (*n1);	This->N2 = (*n3);	This->N3 = (*n2);
				This->T1 = (*t1);	This->T2 = (*t3);	This->T3 = (*t2);
				This->BT1 = (*bt1);	This->BT2 = (*bt3);	This->BT3 = (*bt2);
			}
			else
			{
				This->S1 = (*s3);	This->S2 = (*s1);	This->S3 = (*s2);
				This->UVP1 = (*p3);	This->UVP2 = (*p1);	This->UVP3 = (*p2);
				This->N1 = (*n3);	This->N2 = (*n1);	This->N3 = (*n2);
				This->T1 = (*t3);	This->T2 = (*t1);	This->T3 = (*t2);
				This->BT1 = (*bt3);	This->BT2 = (*bt1);	This->BT3 = (*bt2);
			}
		}
		else
		{
			if(Vector_LexLessThan(s1, s3))
			{
				This->S1 = (*s2);	This->S2 = (*s1);	This->S3 = (*s3);
				This->UVP1 = (*p2);	This->UVP2 = (*p1);	This->UVP3 = (*p3);
				This->N1 = (*n2);	This->N2 = (*n1);	This->N3 = (*n3);
				This->T1 = (*t2);	This->T2 = (*t1);	This->T3 = (*t3);
				This->BT1 = (*bt2);	This->BT2 = (*bt1);	This->BT3 = (*bt3);
			}
			else if(Vector_LexLessThan(s2, s3))
			{
				This->S1 = (*s2);	This->S2 = (*s3);	This->S3 = (*s1);
				This->UVP1 = (*p2);	This->UVP2 = (*p3);	This->UVP3 = (*p1);
				This->N1 = (*n2);	This->N2 = (*n3);	This->N3 = (*n1);
				This->T1 = (*t2);	This->T2 = (*t3);	This->T3 = (*t1);
				This->BT1 = (*bt2);	This->BT2 = (*bt3);	This->BT3 = (*bt1);
			}
			else
			{
				This->S1 = (*s3);	This->S2 = (*s2);	This->S3 = (*s1);
				This->UVP1 = (*p3);	This->UVP2 = (*p2);	This->UVP3 = (*p1);
				This->N1 = (*n3);	This->N2 = (*n2);	This->N3 = (*n1);
				This->T1 = (*t3);	This->T2 = (*t2);	This->T3 = (*t1);
				This->BT1 = (*bt3);	This->BT2 = (*bt2);	This->BT3 = (*bt1);
			}
		}

		RTASSERT(Vector_LexLessThan(&This->S1, &This->S2) && Vector_LexLessThan(&This->S2, &This->S3));

	};


	void PathTracerLumenRTImporter::Triangle_MergeWith(Triangle *This, Triangle const *t)
	{
		RTASSERT( ( dot(This->N, t->N) + 1 ) * ( dot(This->N, t->N) + 1 ) < 0.00001f);
		This->materialWithNegativeNormalIndex = t->materialWithPositiveNormalIndex;
		This->UVN1 = t->UVP1;
		This->UVN2 = t->UVP2;
		This->UVN3 = t->UVP3;
	}




}