
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
#include <maya/MFloatArray.h>
#include <maya/MItMeshPolygon.h>

namespace PathTracerNS
{

	void PathTracerMayaImporter::Import(uint image_width, uint image_height, bool loadSky)
	{
		*ptr__global__imageWidth = image_width;
		*ptr__global__imageHeight = image_height;
		*ptr__global__imageSize = image_width * image_height;

		SetCam(image_width, image_height);
		ImportTexturesAndSky(loadSky);
		ImportMaterials();
		ImportMesh();
		ImportLights();

		MaterialNameId.clear();
		TextureNameId.clear();

		if(*ptr__global__triangulationSize == 0)
			throw std::runtime_error("Scene is empty. Stopping.");
	}

	bool PathTracerMayaImporter::GetCam(const MString &cameraName, MDagPath &camera)
	{
		MItDag itCam(MItDag::kDepthFirst, MFn::kCamera);
		bool cameraFound = false;

		while(!itCam.isDone() && !cameraFound)
		{
			itCam.getPath(camera);
			MFnCamera fn(camera);
			cameraFound = fn.name() == cameraName;
			itCam.next();
		}

		return cameraFound;
	}


	void PathTracerMayaImporter::SetCam(uint image_width, uint image_height)
	{
		const MString cameraName = "raytraceCamShape";
		MDagPath camera;

		if(!GetCam(cameraName, camera))
		{
			CONSOLE_LOG << "WARNING : Camera \"" << cameraName << "\" was not found. Looking for persp camera. " << ENDL;
			if(!GetCam("perspShape", camera))
				throw std::runtime_error("None of the standard cameras were found.");
		}

		MFnCamera		fnCamera(camera);
		MFnTransform	fnTransformCamera(camera);
		MMatrix			MCamera = fnTransformCamera.transformationMatrix();

		// First, we make the camera be coherent with what the user entered in width and height
		double apectRatio =  ( (double) image_width ) / ( (double)  image_height ) ;
		fnCamera.setAspectRatio(apectRatio);

		MPoint	C			= fnCamera.eyePoint		  (MSpace::kWorld);
		MVector D			= fnCamera.viewDirection  (MSpace::kWorld);
		MVector Up			= fnCamera.upDirection	  (MSpace::kWorld);
		MVector Right		= fnCamera.rightDirection (MSpace::kWorld);
		double	focalLength	= fnCamera.focalLength	  ();

		double apertureX, apertureY, offsetX, offsetY;
		fnCamera.getViewParameters(apectRatio, apertureX, apertureY, offsetX, offsetY);		

		// apertures are in inches... conversion in mm
		apertureX *= 25.4;
		apertureY *= 25.4;

		// We want to scale the vectors Right and Up to a factor that will prevent us from taking the focal length into account
		Right *= apertureX / focalLength;
		Up    *= apertureY / focalLength;

		*ptr__global__cameraDirection	= permute_xyz_to_zxy(D);
		*ptr__global__cameraPosition	= permute_xyz_to_zxy(C);
		*ptr__global__cameraRight		= permute_xyz_to_zxy(Right);
		*ptr__global__cameraUp			= permute_xyz_to_zxy(Up);
	}


	// ********************************************************************
	// From the site : http://ewertb.soundlinker.com/api/api.018.php
	//
	// MItMeshPolygon::getTriangle() returns object-relative vertex
	// indices; BUT MItMeshPolygon::normalIndex() and ::getNormal() need
	// face-relative vertex indices! This converts vertex indices from
	// object-relative to face-relative.
	//
	// param  getVertices: Array of object-relative vertex indices for
	//                     entire face.
	// param  getTriangle: Array of object-relative vertex indices for
	//                     local triangle in face.
	//
	// return Array of face-relative indicies for the specified vertices.
	//        Number of elements in returned array == number in getTriangle
	//        (should be 3).
	//
	// note   If getTriangle array does not include a corresponding vertex
	//        in getVertices array then a value of (-1) will be inserted
	//        in that position within the returned array.
	// ********************************************************************
	MIntArray GetLocalIndex( MIntArray const& getVertices, MIntArray const& getTriangle )
	{
		MIntArray   localIndex;
		unsigned    gv, gt;

		int test = getTriangle.length();

		assert ( getTriangle.length() % 3 == 0 );    // Should always deal with a triangle

		for ( gt = 0; gt < getTriangle.length(); gt++ )
		{
			for ( gv = 0; gv < getVertices.length(); gv++ )
			{
				if ( getTriangle[gt] == getVertices[gv] )
				{
					localIndex.append( gv );
					break;
				}
			}

			// if nothing was added, add default "no match"
			if ( localIndex.length() == gt )
				localIndex.append( -1 );
		}

		return localIndex;
	}

	int TotalNumberOfTriangles()
	{
		MItDag itMesh(MItDag::kDepthFirst,MFn::kMesh);
		MDagPath objPath;

		// First we get the total number of triangles in the scene
		int numTriangles = 0;
		while(!itMesh.isDone())
		{
			itMesh.getPath(objPath);
			MFnMesh fnMesh(objPath);
			MIntArray TriangleCount;
			MIntArray TraingleVertices;

			fnMesh.getTriangles(TriangleCount, TraingleVertices);

			for(uint i=0; i<TriangleCount.length(); i++)
				numTriangles += TriangleCount[i];

			itMesh.next();
		}
		return numTriangles;
	}

	void PathTracerMayaImporter::ImportMesh()
	{

		// Also inspired from the site : http://ewertb.soundlinker.com/api/api.018.php

		*ptr__global__triangulationSize = TotalNumberOfTriangles();
		*ptr__global__triangulation = new Triangle[*ptr__global__triangulationSize];

		uint triangleId = 0;

		for(MItDag itMesh(MItDag::kDepthFirst,MFn::kMesh); !itMesh.isDone(); itMesh.next())
		{
			MDagPath objPath;
			itMesh.getPath(objPath);
			MFnMesh  fnMesh( objPath );
			uint materialId = Get_MeshMaterialId(fnMesh);

			//  Cache positions for each vertex
			MPointArray meshPoints;
			fnMesh.getPoints( meshPoints, MSpace::kWorld );

			//  Cache normals for each vertex
			MFloatVectorArray  meshNormals;
			// Normals are per-vertex per-face..
			// use MItMeshPolygon::normalIndex() for index
			fnMesh.getNormals( meshNormals );

			// Get UVSets for this mesh
			MStringArray  UVSets;
			fnMesh.getUVSetNames( UVSets );

			// Get all UVs for the first UV set.
			MFloatArray   u, v;
			fnMesh.getUVs( u, v, &UVSets[0] );

			for ( MItMeshPolygon  itPolygon( objPath, MObject::kNullObj ); !itPolygon.isDone(); itPolygon.next() )
			{
				// Get object-relative indices for the vertices in this face.
				MIntArray polygonVertices;
				itPolygon.getVertices( polygonVertices );

				// Get triangulation of this poly.
				int numTriangles;
				itPolygon.numTriangles(numTriangles);
				while ( numTriangles-- )
				{
					MPointArray nonTweaked;
					// object-relative vertex indices for each triangle
					MIntArray triangleVertices;
					// face-relative vertex indices for each triangle
					MIntArray localIndex;

					itPolygon.getTriangle( numTriangles,
						nonTweaked,
						triangleVertices,
						MSpace::kWorld );

					// --------  Get Positions  --------

					// While it may be tempting to use the points array returned
					// by MItMeshPolygon::getTriangle(), don't. It does not represent
					// poly tweaks in its coordinates!

					// Positions are:
					Float4 s1 = permute_xyz_to_zxy(meshPoints[triangleVertices[0]]);
					Float4 s2 = permute_xyz_to_zxy(meshPoints[triangleVertices[1]]);
					Float4 s3 = permute_xyz_to_zxy(meshPoints[triangleVertices[2]]);

					// --------  Get Normals  --------

					// (triangleVertices) is the object-relative vertex indices
					// BUT MItMeshPolygon::normalIndex() and ::getNormal() needs
					// face-relative vertex indices!

					// Get face-relative vertex indices for this triangle
					localIndex = GetLocalIndex( polygonVertices,
						triangleVertices );

					// Normals are:
					Float4 n1 = permute_xyz_to_zxy( MVector( meshNormals[ itPolygon.normalIndex( localIndex[0] ) ] ) );
					Float4 n2 = permute_xyz_to_zxy( MVector( meshNormals[ itPolygon.normalIndex( localIndex[1] ) ] ) );
					Float4 n3 = permute_xyz_to_zxy( MVector( meshNormals[ itPolygon.normalIndex( localIndex[2] ) ] ) );

					// --------  Get UVs  --------

					// Note: In this example I'm only considering the first UV Set.
					// If you want more simply loop through the sets in the UVSets array.

					int uvID[3];

					// Get UV values for each vertex within this polygon
					for (int vtxInPolygon = 0; vtxInPolygon < 3; vtxInPolygon++ )
					{
						itPolygon.getUVIndex( localIndex[vtxInPolygon],
							uvID[vtxInPolygon],
							&UVSets[0] );
					}

					// UVs are:
					Float2 uv1 = Float2(u[uvID[0]], v[uvID[0]]);
					Float2 uv2 = Float2(u[uvID[1]], v[uvID[1]]);
					Float2 uv3 = Float2(u[uvID[2]], v[uvID[2]]);

					Triangle_Create(
						*ptr__global__triangulation + triangleId,
						// Vertices
						s1, s2, s3,
						// UV Positiv normal face
						uv1, uv2, uv3,
						// UV Negativ normal face
						uv1, uv2, uv3,
						// Normal
						n1, n2, n3,
						// Tangents
						Float4(), Float4(), Float4(),
						// Bitangents
						Float4(), Float4(), Float4(),
						//Materials
						materialId, materialId, triangleId);
					triangleId++;


				}    // while (triangles)

			}    // itPolygon()
		}


	}

	void PathTracerMayaImporter::ImportLights()
	{
		*ptr__global__lightsSize = CountObject(MFn::kLight);
		*ptr__global__lights = new Light[*ptr__global__lightsSize];
		uint lightIndex = 0;

		// Importing Lights
		MItDag itLight(MItDag::kDepthFirst, MFn::kLight);
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


	int PathTracerMayaImporter::Get_MeshMaterialId(MFnMesh const& fnMesh)
	{
		// get the number of instances
		int NumInstances = fnMesh.parentCount();

		// attach a function set to the first instance parent transform
		MFnDependencyNode fn( fnMesh.parent(0) );

		// Print debug info
		//CONSOLE_LOG << fnMesh.name() << ENDL;
		//CONSOLE_LOG << "\tnumMeshInstances " << NumInstances << ENDL;
		//CONSOLE_LOG << "\t\tparent " << fn.name().asChar() << ENDL;

		// this will hold references to the shaders used on the meshes
		MObjectArray Shaders;

		// this is used to hold indices to the materials returned in the object array
		MIntArray    FaceIndices;

		// get the shaders used by the i'th mesh instance
		fnMesh.getConnectedShaders(0,Shaders,FaceIndices);

		int shaderLength = Shaders.length();
		if(Shaders.length() != 1)
			CONSOLE_LOG << "WARNING : the mesh " << fnMesh.name() << " has more than one or no material. We will take only the first." << ENDL;

		MString shaderName = GetShaderName( Shaders[0] );

		// If no material associated or unknown material, return standart material
		if(shaderName == "none")
		{
			CONSOLE_LOG << "ERROR : MATERIAL \"none\" for " << fnMesh.name() << ENDL;
			return 0;
		}

		//Looking for the material
		if(MaterialNameId.find(shaderName.asChar()) != MaterialNameId.end())
			return MaterialNameId[shaderName.asChar()];

		// If we get here, then the material has not been found
		CONSOLE_LOG << "ERROR : MATERIAL NOT FOUND for " << fnMesh.name() << " -> material name : " << shaderName << ENDL;
		return 0;

	}



	RGBAColor PathTracerMayaImporter::GetMaterialColor(MFnDependencyNode const& fn,const char* name)
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

		{
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
		}

		return color;

	}

	int PathTracerMayaImporter::GetTextureId(MFnDependencyNode const& fn)
	{
		MPlug p;

		// will hold the texture node name
		MString texname;

		// get plugs connected to colour attribute
		MPlugArray plugs;
		p.connectedTo(plugs,true,false);
		fn.findPlug("color").connectedTo(plugs,true,false);
		//fn.findPlug("outColor").connectedTo(plugs,true,false);
		uint plugsLength = plugs.length();

		// see if any file textures are present
		for(int i = 0; i != plugsLength; ++i)
		{
			// if file texture found
			if(plugs[i].node().apiType() == MFn::kFileTexture) 
			{
				// bind a function set to it ....
				MFnDependencyNode fnDep(plugs[i].node());

				// to get the node name
				texname = fnDep.name();

				if(TextureNameId.find(texname.asChar()) != TextureNameId.end())
					return TextureNameId[texname.asChar()];
			}
		}

		//No texture found
		return -1;
	}

	void PathTracerMayaImporter::ImportTexturesAndSky(bool loadSky)
	{
		// 1 - We get the number of textures
		*ptr__global__texturesSize = CountObject(MFn::kFileTexture);
		*ptr__global__textures = new Texture[*ptr__global__texturesSize];

		// 2 - We get the size of all the textures
		*ptr__global__texturesDataSize = 0;
		MItDependencyNodes it1(MFn::kFileTexture);
		MObjectArray Textures;
		while(!it1.isDone())
		{
			MFnDependencyNode fn(it1.item());

			Textures.append(it1.item());

			// use the MImage class to get the image data
			MImage img;
			img.readFromTextureNode(it1.item());

			// get the image size
			unsigned int w,h;
			img.getSize(w,h);

			*ptr__global__texturesDataSize += max(w * h,1);
			it1.next();
		}

		// 3 - We load the sky
		uint offset = LoadSkyAndAllocateTextureMemory(loadSky);

		// 4 - We load the other textures
		for(uint textureId = 0; textureId < Textures.length(); textureId++)
		{
			Texture* tex = *ptr__global__textures + textureId;
			tex->offset = offset;

			// attach a dependency node to the file node
			MFnDependencyNode fn(Textures[textureId]);
			TextureNameId[fn.name().asChar()] = textureId;

			// get the attribute for the full texture path
			MPlug ftn = fn.findPlug("ftn");

			// get the filename from the attribute
			MString filename;
			ftn.getValue(filename);

			// use the MImage class to get the image data
			MImage img;
			img.readFromTextureNode(Textures[textureId]);

			// get the image size
			uint d =img.depth();
			uint w,h;
			img.getSize(w, h);
			tex->width  = max(w ,1);
			tex->height = max(h, 1);
			offset += tex->width * tex->height;

			// write image attributes
			//CONSOLE_LOG << "Texture : " << fn.name().asChar() << " from the file : " << filename.asChar() << ENDL;
			//CONSOLE_LOG << "\tWidth  : " << tex->width << ENDL;
			//CONSOLE_LOG << "\tHeight : " << tex->height << ENDL;
			//CONSOLE_LOG << "\tOffset : " << tex->offset << ENDL;
			//CONSOLE_LOG << "\tDepth  : " << d << ENDL;

			// write either 24 or 32 bit data
			if(d==4)
			{
				memcpy(
					(void *) (*ptr__global__texturesData + tex->offset),
					(void *) img.pixels(),
					4 * sizeof(char) * w * h
					);
			}
			else if(d == 3) // we supppose d = 3
			{
				// write 24bit data in chunks so we skip 
				// the fourth alpha byte
				for(unsigned int j=0; j < w*h*4; j+=4 )
				{
					Uchar4 *destPixel = (*ptr__global__texturesData + tex->offset + j);
					uchar *srcPixel = img.pixels()+j;
					destPixel->x = *(srcPixel);
					destPixel->y = *(srcPixel+1);
					destPixel->z = *(srcPixel+2);
					destPixel->w = 1;
				}
			}
			else
			{
				CONSOLE_LOG << "WARNING : The texture " << fn.name().asChar() << " from the file " << filename << " has a depth different of 3 or 4. The texture will be ommited." << ENDL;
			}
		}

	}


	void PathTracerMayaImporter::CreateMaterials()
	{

		//First, we create a standart material to handle all the materials we don't know
		Material_Create(*ptr__global__materiaux);

		int materialId = 1;
		MItDependencyNodes itDep(MFn::kLambert);
		while (!itDep.isDone()) 
		{
			switch(itDep.item().apiType())
			{
				// if found phong shader
			case MFn::kPhong:
				{
					MFnPhongShader fn( itDep.item() );
					MaterialNameId[fn.name().asChar()] = materialId;
					Material_Create(&(*ptr__global__materiaux)[materialId], fn);
				}
				break;
				// if found lambert shader
			case MFn::kLambert:
				{
					MFnLambertShader fn( itDep.item() );
					MaterialNameId[fn.name().asChar()] = materialId;
					Material_Create(&(*ptr__global__materiaux)[materialId], fn);
					//OutputBumpMap(itDep.item());
					//OutputEnvMap(itDep.item());
				}
				break;
				// if found blinn shader
			case MFn::kBlinn:
				{
					MFnBlinnShader fn( itDep.item() );
					MaterialNameId[fn.name().asChar()] = materialId;
					Material_Create(&(*ptr__global__materiaux)[materialId], fn);
					//cout	<<"\teccentricity "
					//	<<fn.eccentricity()<< endl;
					//cout	<< "\tspecularRollOff "
					//	<< fn.specularRollOff()<< endl;
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

	int PathTracerMayaImporter::CountObject(MFn::Type objType)
	{
		int numObjects = 0;
		// iterate through the mesh nodes in the scene
		MItDependencyNodes itDep(objType);

		// we have to keep iterating until we get through
		// all of the nodes in the scene
		//
		while (!itDep.isDone()) 
		{
			numObjects++;
			itDep.next();
		}

		return numObjects;
	}

	void PathTracerMayaImporter::ImportMaterials()
	{

		// from : http://nccastaff.bournemouth.ac.uk/jmacey/RobTheBloke/www/research/maya/mfnmesh.htm
		// and  : http://nccastaff.bournemouth.ac.uk/jmacey/RobTheBloke/www/research/maya/mfnmaterial.htm

		int numMaterials = CountObject(MFn::kLambert)+1;

		*ptr__global__materiauxSize = numMaterials;
		*ptr__global__materiaux = new Material[numMaterials];

		CreateMaterials();
	}






	int PathTracerMayaImporter::LoadSkyAndAllocateTextureMemory(bool loadSky)
	{
		ptr__global__sky->cosRotationAngle = 1;
		ptr__global__sky->sinRotationAngle = 0;
		ptr__global__sky->exposantFactorX = 0;
		ptr__global__sky->exposantFactorY = 0;
		ptr__global__sky->groundScale = 1;

		if(!loadSky)
		{
			// We set the information about the texture
			for(int i=0; i<6; i++)
			{
				ptr__global__sky->skyTextures[i].height = 1;
				ptr__global__sky->skyTextures[i].width = 1;
				ptr__global__sky->skyTextures[i].offset = 0;
			}

			//We allocate the texture memory and then store the data
			*ptr__global__texturesDataSize += 1;
			*ptr__global__texturesData = new Uchar4[*ptr__global__texturesDataSize];
			*ptr__global__texturesData[0] = Uchar4(0,0,0,0);

			return 1;
		}

		int fullWidth, fullHeight;
		long int size;

		// We load the sky bmp file
		uchar* sky = (uchar *) LoadBMP(&fullWidth,&fullHeight,&size, PATHTRACER_SCENE_FOLDER"cubemap.bmp");

		if(sky == NULL)
		{
			CONSOLE_LOG << "ERROR : cannot open cube map. Creating a black sky." << ENDL;
			// we create a black cube map
			fullWidth = 4;
			fullHeight = 3;
			sky = new uchar[3*(3*4)]; 
			for(int i=0; i<3*(3*4); i++)
				sky[i] = 0;
		}


		// We store the width of the texture (it is a cube map)
		const uint texWidth = fullWidth/4;
		const uint texHeight = fullHeight/3;

		// We allocate the memory
		*ptr__global__texturesDataSize += 6*texWidth*texHeight;
		*ptr__global__texturesData = new Uchar4[*ptr__global__texturesDataSize];

		//Now we store the bmp data to the allocated memory
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

		// Side faces
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

		return 6*texWidth*texHeight;
	};

	void PathTracerMayaImporter::Light_Create(Light& l, const MMatrix& M, const MFnPointLight& fnLight)
	{
		l.type						= LightType::LIGHT_POINT;
		l.color						= ToFloat4(fnLight.color());
		l.cosOfInnerFallOffAngle	= 0;
		l.cosOfOuterFallOffAngle	= 0;
		l.direction					= Float4(0,0,0,1);
		l.position					= permute_xyz_to_zxy(MPoint()*M);
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
		l.cosOfInnerFallOffAngle	= (float) cos(fnLight.coneAngle()/2);
		l.cosOfOuterFallOffAngle	= (float) cos(fnLight.coneAngle()/2 + fnLight.penumbraAngle());
		l.direction					= normalize(permute_xyz_to_zxy(MVector(0,0,-1)*M));
		l.position					= permute_xyz_to_zxy(MPoint()*M);
		l.power						= fnLight.intensity();
	};

	void PathTracerMayaImporter::Material_Create(Material *This)
	{
		This->type = MAT_STANDART;
		This->textureName = "";
		This->textureId = 0;
		This->isSimpleColor = true;
		This->hasAlphaMap = false;
		This->opacity = 1;
		This->simpleColor = RGBAColor(0,0,0,0);
	}

	void PathTracerMayaImporter::Material_Create(Material *This, MFnLambertShader const& fn)
	{
		//Material type
		This->type = MAT_STANDART;

		//Texture
		This->textureId = GetTextureId(fn);
		This->isSimpleColor = This->textureId < 0;

		//Alpha map
		This->hasAlphaMap = false;

		//Transparency
		RGBAColor transparencyColor = GetMaterialColor(fn,"transparency");
		This->opacity = transparencyColor.w;

		//Color
		This->simpleColor = GetMaterialColor(fn,"color");
		if(This->simpleColor.x < 0.01 && This->simpleColor.y < 0.01 && This->simpleColor.z < 0.01)
			This->simpleColor = RGBAColor(1,1,1,0)*0.8f;

	}

	void PathTracerMayaImporter::Material_Create(Material *This, MFnPhongShader const& fn)
	{
		//Material type
		//This->type = MAT_VARNHISHED;
		This->type = MAT_STANDART;

		//Texture
		This->textureId = GetTextureId(fn);
		This->isSimpleColor = This->textureId < 0;

		//Alpha map
		This->hasAlphaMap = false;

		//Transparency
		RGBAColor transparencyColor = GetMaterialColor(fn,"transparency");
		This->opacity = transparencyColor.w;

		//Color
		This->simpleColor = GetMaterialColor(fn,"color");
		if(This->simpleColor.x < 0.01 && This->simpleColor.y < 0.01 && This->simpleColor.z < 0.01)
			This->simpleColor = RGBAColor(1,1,1,0)*0.8f;

	}

	void PathTracerMayaImporter::Material_Create(Material *This, MFnBlinnShader const& fn)
	{
		//Material type
		//This->type = MAT_VARNHISHED;
		This->type = MAT_STANDART;

		//Texture
		This->textureId = GetTextureId(fn);
		This->isSimpleColor = This->textureId < 0;

		//Alpha map
		This->hasAlphaMap = false;

		//Transparency
		RGBAColor transparencyColor = GetMaterialColor(fn,"transparency");
		This->opacity = transparencyColor.w;

		//Color
		This->simpleColor = GetMaterialColor(fn,"color");
		if(This->simpleColor.x < 0.01 && This->simpleColor.y < 0.01 && This->simpleColor.z < 0.01)
			This->simpleColor = RGBAColor(1,1,1,0)*0.8f;

	}

	void PathTracerMayaImporter::Triangle_Create(Triangle *This,
		Float4 const& s1,	Float4 const& s2,	Float4 const& s3,
		Float2 const& uvp1, Float2 const& uvp2, Float2 const& uvp3,
		Float2 const& uvn1, Float2 const& uvn2, Float2 const& uvn3,
		Float4 const& n1,	Float4 const& n2,	Float4 const& n3,
		Float4 const& t1,	Float4 const& t2,	Float4 const& t3,
		Float4 const& bt1,	Float4 const& bt2,	Float4 const& bt3,
		uint positiveMatIndex, uint negativeMatIndex, uint id)
	{

		This->materialWithPositiveNormalIndex = positiveMatIndex;
		This->materialWithNegativeNormalIndex = negativeMatIndex;
		This->id = id;

		BoundingBox_Create( This->AABB, s1, s2, s3);

		This->N = normalize(cross( s2-s1, s3-s1 ));

		if(Vector_LexLessThan(s1, s2))
		{
			if(Vector_LexLessThan(s2, s3))
			{
				This->S1		= s1;	This->S2		= s2;	This->S3		= s3;
				This->UVP1	= uvp1;	This->UVP2	= uvp2;	This->UVP3	= uvp3;
				This->UVN1	= uvn1;	This->UVN2	= uvn2;	This->UVN3	= uvn3;
				This->N1		= n1;	This->N2		= n2;	This->N3		= n3;
				This->T1		= t1;	This->T2		= t2;	This->T3		= t3;
				This->BT1	= bt1;	This->BT2	= bt2;	This->BT3	= bt3;
			}
			else if(Vector_LexLessThan(s1, s3))
			{
				This->S1		= s1;	This->S2		= s3;	This->S3		= s2;
				This->UVP1	= uvp1;	This->UVP2	= uvp3;	This->UVP3	= uvp2;
				This->UVN1	= uvn1;	This->UVN2	= uvn3;	This->UVN3	= uvn2;
				This->N1		= n1;	This->N2		= n3;	This->N3		= n2;
				This->T1		= t1;	This->T2		= t3;	This->T3		= t2;
				This->BT1	= bt1;	This->BT2	= bt3;	This->BT3	= bt2;
			}
			else
			{
				This->S1 = s3;	This->S2 = s1;	This->S3 = s2;
				This->UVP1 = uvp3;	This->UVP2 = uvp1;	This->UVP3 = uvp2;
				This->UVN1 = uvn3;	This->UVN2 = uvn1;	This->UVN3 = uvn2;
				This->N1 = n3;	This->N2 = n1;	This->N3 = n2;
				This->T1 = t3;	This->T2 = t1;	This->T3 = t2;
				This->BT1 = bt3;	This->BT2 = bt1;	This->BT3 = bt2;
			}
		}
		else
		{
			if(Vector_LexLessThan(s1, s3))
			{
				This->S1 = s2;	This->S2 = s1;	This->S3 = s3;
				This->UVP1 = uvp2;	This->UVP2 = uvp1;	This->UVP3 = uvp3;
				This->UVN1 = uvn2;	This->UVN2 = uvn1;	This->UVN3 = uvn3;
				This->N1 = n2;	This->N2 = n1;	This->N3 = n3;
				This->T1 = t2;	This->T2 = t1;	This->T3 = t3;
				This->BT1 = bt2;	This->BT2 = bt1;	This->BT3 = bt3;
			}
			else if(Vector_LexLessThan(s2, s3))
			{
				This->S1 = s2;	This->S2 = s3;	This->S3 = s1;
				This->UVP1 = uvp2;	This->UVP2 = uvp3;	This->UVP3 = uvp1;
				This->UVN1 = uvn2;	This->UVN2 = uvn3;	This->UVN3 = uvn1;
				This->N1 = n2;	This->N2 = n3;	This->N3 = n1;
				This->T1 = t2;	This->T2 = t3;	This->T3 = t1;
				This->BT1 = bt2;	This->BT2 = bt3;	This->BT3 = bt1;
			}
			else
			{
				This->S1 = s3;	This->S2 = s2;	This->S3 = s1;
				This->UVP1 = uvp3;	This->UVP2 = uvp2;	This->UVP3 = uvp1;
				This->UVN1 = uvn3;	This->UVN2 = uvn2;	This->UVN3 = uvn1;
				This->N1 = n3;	This->N2 = n2;	This->N3 = n1;
				This->T1 = t3;	This->T2 = t2;	This->T3 = t1;
				This->BT1 = bt3;	This->BT2 = bt2;	This->BT3 = bt1;
			}
		}

		// We have noticed that sometime, maya gives wrong normals of length 0).
		// Therefore, we check the validity of the inputs
		// Actually, if the data are correct, the following line are equivalent to these 3 commented lines
		//
		//		This->N1 = normalize(This->N1);
		//		This->N2 = normalize(This->N2);
		//		This->N3 = normalize(This->N3);

		float
			l1 = length(This->N1),
			l2 = length(This->N2),
			l3 = length(This->N3);
		if(l1 < 0.5) This->N1 = This->N; else This->N1 /= l1;
		if(l2 < 0.5) This->N2 = This->N; else This->N2 /= l2;
		if(l3 < 0.5) This->N3 = This->N; else This->N3 /= l3;


		This->T1 = normalize(This->T1);
		This->T2 = normalize(This->T2);
		This->T3 = normalize(This->T3);
		This->BT1 = normalize(This->BT1);
		This->BT2 = normalize(This->BT2);
		This->BT3 = normalize(This->BT3);

		ASSERT(Vector_LexLessThan(This->S1, This->S2) && Vector_LexLessThan(This->S2, This->S3));
	};


}