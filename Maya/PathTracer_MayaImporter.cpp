
#include "PathTracer_MayaImporter.h"
#include "../Controleur/PathTracer_Utils.h"
#include "../Alone/PathTracer_bitmap.h"

#include <maya/MItDag.h>
#include <maya/MIOStream.h>
#include <maya/MPointArray.h>
#include <maya/MDagPath.h>
#include <maya/MFnCamera.h>
#include <maya/MImage.h>
#include <maya/MMatrix.h>
#include <maya/MFnTransform.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MItDependencyNodes.h>

#include <vector>

namespace PathTracerNS
{

	bool PathTracerMayaImporter::Import(bool loadSky)
	{
		SetCam();
		ImportMaterials();
		ImportScene();
		ImportLights();
		LoadSky(loadSky);

		*ptr__global__textures = new Texture[1];

		return true;
	}

	bool PathTracerMayaImporter::GetCam(const MString &cameraName, MDagPath &camera)
	{
		MItDag itCam(MItDag::kDepthFirst, MFn::kCamera);
		bool cameraFound = false;

		cout << "Looking for camera " << cameraName << " : ";
		while(!itCam.isDone() && !cameraFound)
		{
			itCam.getPath(camera);
			MFnCamera fn(camera);
			if(fn.name() == cameraName)
			{
				cout << "found." << endl;
				cameraFound = true;
			}
			itCam.next();
		}

		if(!cameraFound)
			cout << "not found." << endl;

		return cameraFound;
	}


	void PathTracerMayaImporter::SetCam()
	{
		const MString cameraName = "raytraceCamShape";
		MDagPath camera;

		if(!GetCam(cameraName, camera))
			GetCam("perspShape", camera);

		MFnCamera		fnCamera(camera);
		MFnTransform	fnTransformCamera(camera);
		MMatrix			MCamera = fnTransformCamera.transformationMatrix();

		MPoint	C		= fnCamera.eyePoint		  (MSpace::kWorld);
		MVector D		= fnCamera.viewDirection  (MSpace::kWorld);
		MVector Up		= fnCamera.upDirection	  (MSpace::kWorld);
		MVector Right	= fnCamera.rightDirection (MSpace::kWorld);
		double	pinHole	= fnCamera.focusDistance  ();



		*ptr__global__imageWidth = 1280;
		*ptr__global__imageHeight = 720;

		Up /= (float) (*ptr__global__imageWidth) / (float) (*ptr__global__imageHeight); 

		*ptr__global__imageSize = (*ptr__global__imageWidth) * (*ptr__global__imageHeight);

		*ptr__global__cameraDirection	= permute_xyz_to_zxy(D);
		*ptr__global__cameraPosition	= permute_xyz_to_zxy(C);
		*ptr__global__cameraRight		= permute_xyz_to_zxy(Right);;
		*ptr__global__cameraUp			= permute_xyz_to_zxy(Up);;
	}

	void PathTracerMayaImporter::ImportScene()
	{
		MItDag itMesh(MItDag::kDepthFirst,MFn::kMesh);
		MDagPath objPath;

		// First we get the total number of triangles in the scene
		*ptr__global__triangulationSize = 0;
		while(!itMesh.isDone())
		{
			itMesh.getPath(objPath);
			MFnMesh fnMesh(objPath);
			MIntArray TriangleCount;
			MIntArray TraingleVertices;

			fnMesh.getTriangles(TriangleCount, TraingleVertices);

			for(uint i=0; i<TriangleCount.length(); i++)
				*ptr__global__triangulationSize += TriangleCount[i];

			itMesh.next();
		}

		itMesh.reset();
		itMesh.next();
		*ptr__global__triangulation = new Triangle[*ptr__global__triangulationSize];
		uint triangleId = 0;

		while(!itMesh.isDone())
		{
			itMesh.getPath(objPath);
			MFnMesh fnMesh(objPath);
			MIntArray TriangleCount;
			MIntArray TriangleVertices;
			MPointArray Points;
			MFloatVectorArray	fNormalArray;
			MFloatVectorArray	fTangentArray;
			MFloatVectorArray	fBinormalArray;
			MString				fCurrentUVSetName;

			uint materialId = Get_MeshMaterialId(fnMesh);

			/////////////////////////////////////////////////////////////////////////
			fnMesh.getNormals(fNormalArray, MSpace::kWorld);
			fnMesh.getCurrentUVSetName(fCurrentUVSetName);
			fnMesh.getTangents(fTangentArray, MSpace::kWorld, &fCurrentUVSetName);
			fnMesh.getBinormals(fBinormalArray, MSpace::kWorld, &fCurrentUVSetName);
			fnMesh.getPoints(Points, MSpace::kWorld);
			fnMesh.getTriangles(TriangleCount, TriangleVertices);

			Float2 temp2;
			Float4 temp4;
			const int TriangleVerticesLength = TriangleVertices.length();
			for(uint i=0; i<TriangleVerticesLength; i+=3)
			{
				Triangle_Create(
					*ptr__global__triangulation + triangleId,
					// Vertices
					&permute_xyz_to_zxy(Points[TriangleVertices[i]]),
					&permute_xyz_to_zxy(Points[TriangleVertices[i+1]]),
					&permute_xyz_to_zxy(Points[TriangleVertices[i+2]]),
					// UV
					&temp2,&temp2,&temp2,
					// Normal
					&permute_xyz_to_zxy(MVector(fNormalArray[TriangleVertices[i]])),
					&permute_xyz_to_zxy(MVector(fNormalArray[TriangleVertices[i+1]])),
					&permute_xyz_to_zxy(MVector(fNormalArray[TriangleVertices[i+2]])),
					// Tangents
					&permute_xyz_to_zxy(MVector(fTangentArray[TriangleVertices[i]])),
					&permute_xyz_to_zxy(MVector(fTangentArray[TriangleVertices[i+1]])),
					&permute_xyz_to_zxy(MVector(fTangentArray[TriangleVertices[i+2]])),
					// Bitangents
					&permute_xyz_to_zxy(MVector(fBinormalArray[TriangleVertices[i]])),
					&permute_xyz_to_zxy(MVector(fBinormalArray[TriangleVertices[i+1]])),
					&permute_xyz_to_zxy(MVector(fBinormalArray[TriangleVertices[i+2]])),
					materialId, materialId);
				triangleId++;
			}

			itMesh.next();
		}
	}

	void PathTracerMayaImporter::ImportLights()
	{
		// Point Lights
		MItDag itLight(MItDag::kDepthFirst, MFn::kLight);

		// First we get the total number of lights in the scene
		*ptr__global__lightsSize = 0;
		while(!itLight.isDone())
		{
			(*ptr__global__lightsSize)++;
			itLight.next();
		}

		*ptr__global__lights = new Light[*ptr__global__lightsSize];
		uint lightIndex = 0;

		// Importing Point Lights
		itLight.reset();
		itLight.next();
		MDagPath objPath;
		while(!itLight.isDone())
		{
			itLight.getPath(objPath);
			MMatrix M = objPath.inclusiveMatrix();
			if(objPath.hasFn( MFn::kPointLight))
				Light_Create((*ptr__global__lights)[lightIndex], M, MFnPointLight(objPath));
			else if(objPath.hasFn( MFn::kDirectionalLight))
				Light_Create((*ptr__global__lights)[lightIndex], M, MFnDirectionalLight(objPath));
			else if(objPath.hasFn( MFn::kSpotLight))
				Light_Create((*ptr__global__lights)[lightIndex], M, MFnSpotLight(objPath));

			lightIndex++;
			itLight.next();
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	MString GetShaderName(MObject shadingEngine)
	{
		// attach a function set to the shading engine
		MFnDependencyNode fn( shadingEngine );

		// get access to the surfaceShader attribute. This will be connected to
		// lambert , phong nodes etc.
		MPlug sshader = fn.findPlug("surfaceShader");

		// will hold the connections to the surfaceShader attribute
		MPlugArray materials;

		// get the material connected to the surface shader
		sshader.connectedTo(materials,true,false);

		// if we found a material
		if(materials.length())
		{
			MFnDependencyNode fnMat(materials[0].node());
			return fnMat.name();
		}
		return "none";
	}


	void OutputMeshInstances(MObject mesh)
	{
		MFnMesh fnMesh(mesh);

		// get the number of instances
		int NumInstances = fnMesh.parentCount();

		cout << "\tnumMeshInstances " << NumInstances << endl;

		// loop through each instance of the mesh
		for(int i=0;i< NumInstances;++i)
		{
			// attach a function set to this instances parent transform
			MFnDependencyNode fn( fnMesh.parent(i) );

			// write the name of the parent transform
			cout << "\t\tparent " << fn.name().asChar() << endl;

			// this will hold references to the shaders used on the meshes
			MObjectArray Shaders;

			// this is used to hold indices to the materials returned in the object array
			MIntArray    FaceIndices;

			// get the shaders used by the i'th mesh instance
			fnMesh.getConnectedShaders(i,Shaders,FaceIndices);

			switch(Shaders.length()) {

				// if no shader applied to the mesh instance
			case 0:
				{
					cout << "\t\tmaterials 0\n";
				}
				break;

				// if all faces use the same material
			case 1:
				{
					cout << "\t\tmaterials 1\n";
					cout << "\t\t\t"
						<< GetShaderName( Shaders[0] ).asChar()
						<< endl;
				}
				break;

				// if more than one material is used, write out the face indices the materials
				// are applied to.
			default:
				{
					cout << "\t\tmaterials " << Shaders.length() << endl;

					// i'm going to sort the face indicies into groups based on
					// the applied material - might as well... ;)
					std::vector< std::vector< int > > FacesByMatID;

					// set to same size as num of shaders
					FacesByMatID.resize(Shaders.length());

					// put face index into correct array
					for(uint j=0;j < FaceIndices.length();++j)
					{
						FacesByMatID[ FaceIndices[j] ].push_back(j);
					}

					// now write each material and the face indices that use them
					for(uint j=0;j < Shaders.length();++j)
					{
						cout << "\t\t\t"
							<< GetShaderName( Shaders[j] ).asChar()
							<< "\n\t\t\t"
							<< FacesByMatID[j].size()
							<< "\n\t\t\t\t";

						std::vector< int >::iterator it = FacesByMatID[j].begin();
						for( ; it != FacesByMatID[j].end(); ++it )
						{
							cout << *it << " ";
						}
						cout << endl;
					}
				}
				break;
			}
		}
	}

	int PathTracerMayaImporter::Get_MeshMaterialId(MFnMesh const& fnMesh)
	{
		// get the number of instances
		int NumInstances = fnMesh.parentCount();

		CONSOLE << fnMesh.name() << ENDL;
		cout << "\tnumMeshInstances " << NumInstances << endl;

		// attach a function set to the first instance parent transform
		MFnDependencyNode fn( fnMesh.parent(0) );

		// write the name of the parent transform
		cout << "\t\tparent " << fn.name().asChar() << endl;

		// this will hold references to the shaders used on the meshes
		MObjectArray Shaders;

		// this is used to hold indices to the materials returned in the object array
		MIntArray    FaceIndices;

		// get the shaders used by the i'th mesh instance
		fnMesh.getConnectedShaders(0,Shaders,FaceIndices);

		int shaderLength = Shaders.length();
		if(Shaders.length() != 1)
		{
			CONSOLE << "ERROR : the material " << fnMesh.name() << " has more than one or no material. " << ENDL;
			return 0;
		}

		MString shaderName = GetShaderName( Shaders[0] );
		//Looking for the material :
		for(uint i=0; i < *ptr__global__materiauxSize; i++)
		{
			if(MString((*ptr__global__materiaux)[i].textureName) == shaderName)
				return i;
		}

		CONSOLE << "ERROR : MATERIAL NOT FOUND for " << fnMesh.name() << ENDL;
		return 0;

	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	RGBAColor PathTracerMayaImporter::OutputColor(MFnDependencyNode const& fn,const char* name)
	{
		MPlug p;

		MString r = name;
		r += "R";
		MString g = name;
		g += "G";
		MString b = name;
		b += "B";
		MString a = name;
		a += "A";

		// get the color value
		RGBAColor color;

		// get a plug to the attribute
		p = fn.findPlug(r);
		p.getValue(color.x);
		p = fn.findPlug(g);
		p.getValue(color.y);
		p = fn.findPlug(b);
		p.getValue(color.z);
		p = fn.findPlug(a);
		p.getValue(color.w);
		p = fn.findPlug(name);

		return color;

		/*

		// will hold the texture node name
		MString texname;

		// get plugs connected to colour attribute
		MPlugArray plugs;
		p.connectedTo(plugs,true,false);

		// see if any file textures are present
		for(int i=0;i!=plugs.length();++i)
		{
			// if file texture found
			if(plugs[i].node().apiType() == MFn::kFileTexture) 
			{
				// bind a function set to it ....
				MFnDependencyNode fnDep(plugs[i].node());

				// to get the node name
				texname = fnDep.name();

				// stop looping
				break;

			}

		}
		if(	name == "color" && 
			color.r <0.01 && 
			color.g < 0.01 && 
			color.b < 0.01)

		{
			color.r=color.g=color.b=0.6f;

		}
		// output the name, color and texture ID
		cout	<< "\t" << name << " "
			<< color.r << " "
			<< color.g << " "
			<< color.b << " "
			<< color.a << " tex= "
			<< texname.asChar() << "\n";

		*/
	}


	void PathTracerMayaImporter::CreateMaterials()
	{
		int materialId = 0;
		MItDependencyNodes itDep(MFn::kLambert);
		while (!itDep.isDone()) 
		{
			switch(itDep.item().apiType())
			{
				// if found phong shader
			case MFn::kPhong:
				{
					MFnPhongShader fn( itDep.item() );
					Material_Create(&(*ptr__global__materiaux)[materialId], fn);
				}
				break;
				// if found lambert shader
			case MFn::kLambert:
				{
					MFnLambertShader fn( itDep.item() );
					Material_Create(&(*ptr__global__materiaux)[materialId], fn);
					//OutputBumpMap(itDep.item());
					//OutputEnvMap(itDep.item());
				}
				break;
				// if found blinn shader
			case MFn::kBlinn:
				{
					MFnBlinnShader fn( itDep.item() );
					Material_Create(&(*ptr__global__materiaux)[materialId], fn);
					cout	<<"\teccentricity "
						<<fn.eccentricity()<< endl;
					cout	<< "\tspecularRollOff "
						<< fn.specularRollOff()<< endl;
					//OutputBumpMap(itDep.item());
					//OutputEnvMap(itDep.item()); 
				}
				break;
			default:
				break;
			}

			materialId++;
			itDep.next();

		}
	}

	int PathTracerMayaImporter::CountMaterials()
	{
		int numMaterials = 0;
		// iterate through the mesh nodes in the scene
		MItDependencyNodes itDep(MFn::kLambert);

		// we have to keep iterating until we get through
		// all of the nodes in the scene
		//
		while (!itDep.isDone()) 
		{
			numMaterials++;
			itDep.next();
		}

		return numMaterials;
	}

	void PathTracerMayaImporter::ImportMaterials()
	{

		// from : http://nccastaff.bournemouth.ac.uk/jmacey/RobTheBloke/www/research/maya/mfnmesh.htm
		// and  : http://nccastaff.bournemouth.ac.uk/jmacey/RobTheBloke/www/research/maya/mfnmaterial.htm

		int numMaterials = CountMaterials();

		*ptr__global__materiauxSize = numMaterials;
		*ptr__global__materiaux = new Material[numMaterials];

		CreateMaterials();
	}






	void PathTracerMayaImporter::LoadSky(bool loadSky)
	{
		ptr__global__sky->cosRotationAngle = 1;
		ptr__global__sky->sinRotationAngle = 0;
		ptr__global__sky->exposantFactorX = 0;
		ptr__global__sky->exposantFactorY = 0;
		ptr__global__sky->groundScale = 1;

		if(!loadSky)
		{
			for(int i=0; i<6; i++)
			{
				ptr__global__sky->skyTextures[i].height = 1;
				ptr__global__sky->skyTextures[i].width = 1;
				ptr__global__sky->skyTextures[i].offset = 0;
			}
			*ptr__global__texturesDataSize = 1;
			*ptr__global__texturesData = new Uchar4;
			*ptr__global__texturesData[0] = Uchar4(0,0,0,0);

			return;
		}


		int fullWidth, fullHeight;
		long int size;
		LPCTSTR filePath = L"C:\\Users\\Alexandre Djerbetian\\Pictures\\Maya\\cubemap.bmp";
		uchar* sky = (uchar *) LoadBMP(&fullWidth,&fullHeight,&size, filePath);

		const uint texWidth = fullWidth/4;
		const uint texHeight = fullHeight/3;

		*ptr__global__texturesDataSize = 6*texWidth*texHeight;
		*ptr__global__texturesData = new Uchar4[*ptr__global__texturesDataSize];

		uint texIndex = 0;

		// Upper face
		ptr__global__sky->skyTextures[0].width = texWidth	;
		ptr__global__sky->skyTextures[0].height = texHeight;
		ptr__global__sky->skyTextures[0].offset = 0;
		for(uint y = 0; y<texHeight; y++)
		{
			for(uint fullIndex = y * fullWidth + texWidth; fullIndex < y * fullWidth + 2*texWidth; fullIndex++)
			{
				uchar b = sky[3*fullIndex];
				uchar g = sky[3*fullIndex+1];
				uchar r = sky[3*fullIndex+2];
				uchar a = 255;

 				(*ptr__global__texturesData)[texIndex].x = r;
 				(*ptr__global__texturesData)[texIndex].y = g;
 				(*ptr__global__texturesData)[texIndex].z = b;
 				(*ptr__global__texturesData)[texIndex].w = a;
				texIndex++;
			}
		}

		//// Side faces
		for(int i=0; i<4; i++)
		{
			ptr__global__sky->skyTextures[i+1].width = texWidth;
			ptr__global__sky->skyTextures[i+1].height = texHeight;
			ptr__global__sky->skyTextures[i+1].offset = (i+1)*texWidth*texHeight;
			for(uint y = texHeight; y<2*texHeight; y++)
			{
				for(uint fullIndex = y * fullWidth + i*texWidth; fullIndex < y * fullWidth + (i+1)*texWidth; fullIndex++)
				{
					uchar b = sky[3*fullIndex];
					uchar g = sky[3*fullIndex+1];
					uchar r = sky[3*fullIndex+2];
					uchar a = 255;

 					(*ptr__global__texturesData)[texIndex].x = r;
 					(*ptr__global__texturesData)[texIndex].y = g;
 					(*ptr__global__texturesData)[texIndex].z = b;
 					(*ptr__global__texturesData)[texIndex].w = a;
					texIndex++;
				}
			}
		}

		// Bottom face - ground
		ptr__global__sky->skyTextures[5].width = texWidth;
		ptr__global__sky->skyTextures[5].height = texHeight;
		ptr__global__sky->skyTextures[5].offset = 5*texWidth*texHeight;
		for(uint y = 2*texHeight; y<3*texHeight; y++)
		{
			for(uint fullIndex = y * fullWidth + texWidth; fullIndex < y * fullWidth + 2*texWidth; fullIndex++)
			{
				uchar b = sky[3*fullIndex];
				uchar g = sky[3*fullIndex+1];
				uchar r = sky[3*fullIndex+2];
				uchar a = 255;

 				(*ptr__global__texturesData)[texIndex].x = r;
 				(*ptr__global__texturesData)[texIndex].y = g;
 				(*ptr__global__texturesData)[texIndex].z = b;
 				(*ptr__global__texturesData)[texIndex].w = a;
				texIndex++;
			}
		}
	};

	void PathTracerMayaImporter::Light_Create(Light& l, const MMatrix& M, const MFnPointLight& fnLight)
	{
		l.type						= LightType::LIGHT_POINT;
		l.color						= ToFloat4(fnLight.color());
		l.cosOfInnerFallOffAngle	= 0;
		l.cosOfOuterFallOffAngle	= 0;
		l.direction					= Float4(0,0,0,1);
		l.position					= permute_xyz_to_xzy(MPoint()*M);
		l.power						= fnLight.intensity();
	};

	void PathTracerMayaImporter::Light_Create(Light& l, const MMatrix& M, const MFnDirectionalLight& fnLight)
	{
		l.type						= LightType::LIGHT_DIRECTIONNAL;
		l.color						= ToFloat4(fnLight.color());
		l.cosOfInnerFallOffAngle	= 0;
		l.cosOfOuterFallOffAngle	= 0;
		l.direction					= normalize(permute_xyz_to_zxy(MVector(0,0,-1)*M));
		l.position					= Float4(0,0,0,1);
		l.power						= fnLight.intensity();
	};

	void PathTracerMayaImporter::Light_Create(Light& l, const MMatrix& M, const MFnSpotLight& fnLight)
	{
		l.type						= LightType::LIGHT_SPOT;
		l.color						= ToFloat4(fnLight.color());
		l.cosOfInnerFallOffAngle	= (float) fnLight.penumbraAngle();
		l.cosOfOuterFallOffAngle	= (float) fnLight.coneAngle();
		l.direction					= normalize(ToFloat4(MVector(0,0,-1)*M));
		l.position					= permute_xyz_to_xzy(MPoint()*M);
		l.power						= fnLight.intensity();
	};

	void PathTracerMayaImporter::Material_Create(Material *This, MFnPhongShader const& fn)
	{
		This->type = MAT_STANDART;
		This->textureName = fn.name().asChar();
		This->textureId = 0;
		This->isSimpleColor = true;
		This->hasAlphaMap = false;

		RGBAColor transparencyColor = OutputColor(fn,"transparency");
		This->opacity = transparencyColor.w;
		This->simpleColor = OutputColor(fn,"color");
	}

	void PathTracerMayaImporter::Material_Create(Material *This, MFnLambertShader const& fn)
	{
		This->type = MAT_STANDART;
		This->textureName = fn.name().asChar();
		This->textureId = 0;
		This->isSimpleColor = true;
		This->hasAlphaMap = false;

		This->opacity = 1 - length(OutputColor(fn,"transparency"));
		This->simpleColor = OutputColor(fn,"color");
	}

	void PathTracerMayaImporter::Material_Create(Material *This, MFnBlinnShader const& fn)
	{
		This->type = MAT_STANDART;
		This->textureName = fn.name().asChar();
		This->textureId = 0;
		This->isSimpleColor = true;
		This->hasAlphaMap = false;

		RGBAColor transparencyColor = OutputColor(fn,"transparency");
		This->opacity = transparencyColor.w;
		This->simpleColor = OutputColor(fn,"color");
	}

	void PathTracerMayaImporter::Triangle_Create(Triangle *This,
		Float4 const *s1, Float4 const *s2, Float4 const *s3,
		Float2 const *p1, Float2 const *p2, Float2 const *p3,
		Float4 const *n1, Float4 const *n2, Float4 const *n3,
		Float4 const *t1, Float4 const *t2, Float4 const *t3,
		Float4 const *bt1, Float4 const *bt2, Float4 const *bt3,
		uint positiveMatIndex, uint negativeMatIndex )
	{
		This->materialWithPositiveNormalIndex = positiveMatIndex;
		This->materialWithNegativeNormalIndex = negativeMatIndex;

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

		This->N1 = normalize(This->N1);
		This->N2 = normalize(This->N2);
		This->N3 = normalize(This->N3);
		This->T1 = normalize(This->T1);
		This->T2 = normalize(This->T2);
		This->T3 = normalize(This->T3);
		This->BT1 = normalize(This->BT1);
		This->BT2 = normalize(This->BT2);
		This->BT3 = normalize(This->BT3);

		RTASSERT(Vector_LexLessThan(&This->S1, &This->S2) && Vector_LexLessThan(&This->S2, &This->S3));
	};


}