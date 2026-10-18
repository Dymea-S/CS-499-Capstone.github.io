///////////////////////////////////////////////////////////////////////////////
// SceneManager.cpp
// ============
// manage the loading and rendering of 3D scenes
//
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
//  ENHANCED BY: Crystal Schippers - CS 499 Computer Science Capstone, 2026
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <glm/gtx/transform.hpp>

// declaration of global variables
namespace
{
	const char* g_ModelName = "model";
	const char* g_ColorValueName = "objectColor";
	const char* g_TextureValueName = "objectTexture";
	const char* g_UseTextureName = "bUseTexture";
	const char* g_UseLightingName = "bUseLighting";
	const char* g_SelectedName = "bObjectSelected";
}

/***********************************************************
 *  SceneManager()
 *
 *  The constructor for the class
 ***********************************************************/
SceneManager::SceneManager(ShaderManager* pShaderManager)
	: m_pShaderManager(pShaderManager),
	  m_basicMeshes(new ShapeMeshes()),
	  m_loadedTextures(0),
	  m_isPrepared(false)
{
}

void SceneManager::SetSelectionHighlight(const std::string& objectId)
{
	const SceneObject* object = m_sceneObjects.FindById(objectId);
	m_pShaderManager->setBoolValue(g_SelectedName, object != nullptr && object->selected);
}

const SceneObject* SceneManager::SelectObject(const Ray& ray)
{
	return m_sceneObjects.SelectNearest(ray);
}

/***********************************************************
 *  ~SceneManager()
 *
 *  The destructor for the class
 ***********************************************************/
SceneManager::~SceneManager()
{
	DestroyGLTextures();
	m_pShaderManager = nullptr;
	delete m_basicMeshes;
	m_basicMeshes = nullptr;
}

/***********************************************************
 *  CreateGLTexture()
 *
 *  This method is used for loading textures from image files,
 *  configuring the texture mapping parameters in OpenGL,
 *  generating the mipmaps, and loading the read texture into
 *  the next available texture slot in memory.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, const std::string& tag)
{
	int width = 0;
	int height = 0;
	int colorChannels = 0;
	GLuint textureID = 0;

	if (m_loadedTextures >= MAX_TEXTURES)
	{
		std::cerr << "Texture limit reached. Skipping: " << filename << std::endl;
		return false;
	}

	if (FindTextureSlot(tag) >= 0)
	{
		std::cerr << "Duplicate texture tag rejected: " << tag << std::endl;
		return false;
	}

	// indicate to always flip images vertically when loaded
	stbi_set_flip_vertically_on_load(true);

	// try to parse the image data from the specified image file
	unsigned char* image = stbi_load(
		filename,
		&width,
		&height,
		&colorChannels,
		0);

	// if the image was successfully read from the image file
	if (image)
	{
		std::cout << "Successfully loaded image:" << filename << ", width:" << width << ", height:" << height << ", channels:" << colorChannels << std::endl;

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// if the loaded image is in RGB format
		if (colorChannels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		// if the loaded image is in RGBA format - it supports transparency
		else if (colorChannels == 4)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		else
		{
			std::cerr << "Unsupported image channel count (" << colorChannels
				<< ") for: " << filename << std::endl;
			stbi_image_free(image);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDeleteTextures(1, &textureID);
			return false;
		}

		// generate the texture mipmaps for mapping textures to lower resolutions
		glGenerateMipmap(GL_TEXTURE_2D);

		// free the image data from local memory
		stbi_image_free(image);
		glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

		// register the loaded texture and associate it with the special tag string
		m_textureIDs[m_loadedTextures].ID = textureID;
		m_textureIDs[m_loadedTextures].tag = tag;
		m_loadedTextures++;

		return true;
	}

	std::cout << "Could not load image:" << filename << std::endl;

	// Error loading the image
	return false;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  This method is used for binding the loaded textures to
 *  OpenGL texture memory slots.  There are up to 16 slots.
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		// bind textures on corresponding texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  This method is used for freeing the memory in all the
 *  used texture memory slots.
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	if (m_loadedTextures > 0)
	{
		GLuint textureIDs[MAX_TEXTURES] = {};
		for (int i = 0; i < m_loadedTextures; ++i)
		{
			textureIDs[i] = m_textureIDs[i].ID;
			m_textureIDs[i].ID = 0;
			m_textureIDs[i].tag.clear();
		}
		glDeleteTextures(m_loadedTextures, textureIDs);
		m_loadedTextures = 0;
	}
}

/***********************************************************
 *  FindTextureID()
 *
 *  This method is used for getting an ID for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureID(const std::string& tag)
{
	int textureID = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureID = m_textureIDs[index].ID;
			bFound = true;
		}
		else
			index++;
	}

	return(textureID);
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  This method is used for getting a slot index for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureSlot(const std::string& tag)
{
	int textureSlot = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureSlot = index;
			bFound = true;
		}
		else
			index++;
	}

	return(textureSlot);
}

/***********************************************************
 *  FindMaterial()
 *
 *  This method is used for getting a material from the previously
 *  defined materials list that is associated with the passed in tag.
 ***********************************************************/
bool SceneManager::FindMaterial(const std::string& tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.size() == 0)
	{
		return(false);
	}

	int index = 0;
	bool bFound = false;
	while ((index < m_objectMaterials.size()) && (bFound == false))
	{
		if (m_objectMaterials[index].tag.compare(tag) == 0)
		{
			bFound = true;
			material.ambientColor = m_objectMaterials[index].ambientColor;
			material.ambientStrength = m_objectMaterials[index].ambientStrength;
			material.diffuseColor = m_objectMaterials[index].diffuseColor;
			material.specularColor = m_objectMaterials[index].specularColor;
			material.shininess = m_objectMaterials[index].shininess;
		}
		else
		{
			index++;
		}
	}

	return(bFound);
}

/***********************************************************
 *  SetTransformations()
 *
 *  This method is used for setting the transform buffer
 *  using the passed in transformation values.
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	// variables for this method
	glm::mat4 modelView;
	glm::mat4 scale;
	glm::mat4 rotationX;
	glm::mat4 rotationY;
	glm::mat4 rotationZ;
	glm::mat4 translation;

	// set the scale value in the transform buffer
	scale = glm::scale(scaleXYZ);
	// set the rotation values in the transform buffer
	rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	// set the translation value in the transform buffer
	translation = glm::translate(positionXYZ);

	modelView = translation * rotationX * rotationY * rotationZ * scale;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, modelView);
	}
}

/***********************************************************
 *  SetShaderColor()
 *
 *  This method is used for setting the passed in color
 *  into the shader for the next draw command
 ***********************************************************/
void SceneManager::SetShaderColor(
	float redColorValue,
	float greenColorValue,
	float blueColorValue,
	float alphaValue)
{
	// variables for this method
	glm::vec4 currentColor;

	currentColor.r = redColorValue;
	currentColor.g = greenColorValue;
	currentColor.b = blueColorValue;
	currentColor.a = alphaValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
	}
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  This method is used for setting the texture data
 *  associated with the passed in ID into the shader.
 ***********************************************************/
void SceneManager::SetShaderTexture(
	const std::string& textureTag)
{
	if (NULL != m_pShaderManager)
	{
		const int textureSlot = FindTextureSlot(textureTag);
		if (textureSlot < 0)
		{
			m_pShaderManager->setIntValue(g_UseTextureName, false);
			std::cerr << "Texture tag not found: " << textureTag << std::endl;
			return;
		}

		m_pShaderManager->setIntValue(g_UseTextureName, true);
		m_pShaderManager->setSampler2DValue(g_TextureValueName, textureSlot);
	}
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  This method is used for setting the texture UV scale
 *  values into the shader.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetShaderMaterial()
 *
 *  This method is used for passing the material values
 *  into the shader.
 ***********************************************************/
void SceneManager::SetShaderMaterial(
	const std::string& materialTag)
{
	if (m_objectMaterials.size() > 0)
	{
		OBJECT_MATERIAL material;
		bool bReturn = false;

		bReturn = FindMaterial(materialTag, material);
		if (bReturn == true)
		{
			m_pShaderManager->setVec3Value("material.ambientColor", material.ambientColor);
			m_pShaderManager->setFloatValue("material.ambientStrength", material.ambientStrength);
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
		}
	}
}

/**************************************************************/
/*** STUDENTS CAN MODIFY the code in the methods BELOW for  ***/
/*** preparing and rendering their own 3D replicated scenes.***/
/*** Please refer to the code in the OpenGL sample project  ***/
/*** for assistance.                                        ***/
/**************************************************************/

/***********************************************************
  *  LoadSceneTextures()
  *
  *  This method is used for preparing the 3D scene by loading
  *  the shapes, textures in memory to support the 3D scene
  *  rendering
  ***********************************************************/
void SceneManager::LoadSceneTextures() {
	struct TextureAsset
	{
		const char* path;
		const char* tag;
	};

	const TextureAsset assets[] = {
		{ "../../Utilities/textures/rusticwood.jpg", "wood" },
		{ "../../Utilities/textures/abstract.jpg", "onyx" },
		{ "../../Utilities/textures/pavers.jpg", "grass" },
		{ "../../Utilities/textures/stainless.jpg", "stainless" }
	};

	for (const TextureAsset& asset : assets)
	{
		if (!CreateGLTexture(asset.path, asset.tag))
		{
			std::cerr << "Scene preparation could not load texture: "
				<< asset.tag << std::endl;
		}
	}

	BindGLTextures();
}
/***********************************************************
 *  DefineObjectMaterials()
 *
 *  This method is used for configuring the various material
 *  settings for all of the objects within the 3D scene.
 ***********************************************************/
void SceneManager::DefineObjectMaterials() {
	m_objectMaterials.clear();

	OBJECT_MATERIAL woodMaterial;
	woodMaterial.ambientColor = glm::vec3(0.55f, 0.45f, 0.35f);
	woodMaterial.ambientStrength = 0.55f;
	woodMaterial.diffuseColor = glm::vec3(0.85f, 0.75f, 0.65f);
	woodMaterial.specularColor = glm::vec3(0.20f, 0.18f, 0.15f);
	woodMaterial.shininess = 8.0f;
	woodMaterial.tag = "wood";

	m_objectMaterials.push_back(woodMaterial);

	OBJECT_MATERIAL glassMaterial;
	glassMaterial.ambientColor = glm::vec3(0.4f, 0.4f, 0.4f);
	glassMaterial.ambientStrength = 0.5f;
	glassMaterial.diffuseColor = glm::vec3(0.75f, 0.80f, 0.85f);
	glassMaterial.specularColor = glm::vec3(0.6f, 0.6f, 0.6f);
	glassMaterial.shininess = 85.0;
	glassMaterial.tag = "glass";

	m_objectMaterials.push_back(glassMaterial);

	OBJECT_MATERIAL grassMaterial;
	grassMaterial.ambientColor = glm::vec3(0.1f, 0.25f, 0.1f); // deep green base
	grassMaterial.ambientStrength = 0.4f;
	grassMaterial.diffuseColor = glm::vec3(0.13f, 0.55f, 0.13f); // “forest green” diffuse tone
	grassMaterial.specularColor = glm::vec3(0.1f, 0.2f, 0.1f); // subtle highlights (matte)
	grassMaterial.shininess = 16.0f; // low shininess for soft reflection
	grassMaterial.tag = "grass";

	m_objectMaterials.push_back(grassMaterial);

	OBJECT_MATERIAL stainlessMaterial;
	stainlessMaterial.ambientColor = glm::vec3(0.55f, 0.55f, 0.58f);
	stainlessMaterial.ambientStrength = 0.55f;
	stainlessMaterial.diffuseColor = glm::vec3(0.75f, 0.75f, 0.78f);
	stainlessMaterial.specularColor = glm::vec3(0.95f, 0.95f, 1.0f);
	stainlessMaterial.shininess = 64.0f;
	stainlessMaterial.tag = "stainless";

	m_objectMaterials.push_back(stainlessMaterial);

	OBJECT_MATERIAL onyxMaterial;
	onyxMaterial.ambientColor = glm::vec3(0.30f, 0.30f, 0.34f);
	onyxMaterial.ambientStrength = 0.50f;
	onyxMaterial.diffuseColor = glm::vec3(0.65f, 0.65f, 0.72f);
	onyxMaterial.specularColor = glm::vec3(0.45f, 0.45f, 0.50f);
	onyxMaterial.shininess = 32.0f;
	onyxMaterial.tag = "onyx";

	m_objectMaterials.push_back(onyxMaterial);
}

/***********************************************************
 *  SetupSceneLights()
 *
 *  This method is called to add and configure the light
 *  sources for the 3D scene.  There are up to 4 light sources.
 ***********************************************************/
void SceneManager::SetupSceneLights() {
	//
	//	// this line of code is NEEDED for telling the shaders to render 
	//	// the 3D scene with custom lighting - to use the default rendered 
	//	// lighting then comment out the following line
	m_pShaderManager->setBoolValue(g_UseLightingName, true);

	// =======================
	// Light 0: Overhead ceiling light
	// =======================
	m_pShaderManager->setVec3Value("lightSources[0].position", 0.0f, 10.0f, 0.0f);
	m_pShaderManager->setVec3Value("lightSources[0].ambientColor", 0.55f, 0.52f, 0.48f);   // bright warm base
	m_pShaderManager->setVec3Value("lightSources[0].diffuseColor", 1.10f, 1.04f, 0.96f);   // strong warm-white light
	m_pShaderManager->setVec3Value("lightSources[0].specularColor", 0.9f, 0.85f, 0.7f);
	m_pShaderManager->setFloatValue("lightSources[0].focalStrength", 32.0f);
	m_pShaderManager->setFloatValue("lightSources[0].specularIntensity", 0.25f);

	// =======================
	// Light 1: Candle flame
	// =======================
	m_pShaderManager->setVec3Value("lightSources[1].position", -1.0f, 2.1f, 2.0f);
	m_pShaderManager->setVec3Value("lightSources[1].ambientColor", 0.14f, 0.09f, 0.04f);  // soft orange fill
	m_pShaderManager->setVec3Value("lightSources[1].diffuseColor", 1.0f, 0.6f, 0.25f);
	m_pShaderManager->setVec3Value("lightSources[1].specularColor", 1.0f, 0.8f, 0.3f);
	m_pShaderManager->setFloatValue("lightSources[1].focalStrength", 64.0f);
	m_pShaderManager->setFloatValue("lightSources[1].specularIntensity", 0.4f);

	m_pShaderManager->setVec3Value("lightSources[2].position", 10.0f, 0.0f, -10.0f);
	m_pShaderManager->setVec3Value("lightSources[2].ambientColor", 0.20f, 0.22f, 0.26f);
	m_pShaderManager->setVec3Value("lightSources[2].diffuseColor", 0.85f, 0.90f, 1.0f);
	m_pShaderManager->setVec3Value("lightSources[2].specularColor", 1.0f, 1.0f, 1.0f);
	m_pShaderManager->setFloatValue("lightSources[2].focalStrength", 16.0f);
	m_pShaderManager->setFloatValue("lightSources[2].specularIntensity", 0.05f);

	// =======================
	// Global Ambient Fill (prevents total darkness)
	// =======================
	m_pShaderManager->setVec3Value("globalAmbientColor", 0.40f, 0.42f, 0.46f); // neutral fill for shadowed surfaces
}

/***********************************************************
 *  PrepareScene()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene
 *  rendering
 ***********************************************************/
void SceneManager::PrepareScene()
{
	if (m_isPrepared)
	{
		return;
	}

	// load the texture image files for the textures applied
	// to objects in the 3D scene
	LoadSceneTextures();

	// define the materials that will be used for the objects
	// in the 3D scene
	DefineObjectMaterials();

	// add and defile the light sources for the 3D scene
	SetupSceneLights();

	m_basicMeshes->LoadPlaneMesh();
	m_basicMeshes->LoadCylinderMesh(); // cup body, lid, straw
	m_basicMeshes->LoadSphereMesh();   // candle flame
	m_basicMeshes->LoadConeMesh();     // plant leaves
	m_basicMeshes->LoadBoxMesh();      // laptop base + screen

	// Logical scene objects use unit local bounds and model matrices. The vector
	// preserves stable traversal order, while the registry's hash map supports
	// average O(1) lookup by the IDs used during rendering.
	const BoundingBox unitBounds = { glm::vec3(-0.5f), glm::vec3(0.5f) };
	auto model = [](const glm::vec3& center, const glm::vec3& dimensions)
	{
		return glm::translate(center) * glm::scale(dimensions);
	};
	m_sceneObjects.AddObject({ "table", "Table", unitBounds,
		model(glm::vec3(0.0f, -0.05f, 0.0f), glm::vec3(20.0f, 0.1f, 10.0f)), false });
	m_sceneObjects.AddObject({ "cup", "Cup", unitBounds,
		model(glm::vec3(8.0f, 1.6f, 0.0f), glm::vec3(2.2f, 3.4f, 2.2f)), false });
	m_sceneObjects.AddObject({ "candle", "Candle", unitBounds,
		model(glm::vec3(-1.0f, 0.9f, 2.0f), glm::vec3(1.6f, 2.2f, 1.6f)), false });
	m_sceneObjects.AddObject({ "plant", "Succulent Plant", unitBounds,
		model(glm::vec3(2.0f, 0.8f, -1.0f), glm::vec3(2.4f, 2.2f, 2.4f)), false });
	m_sceneObjects.AddObject({ "laptop", "Laptop", unitBounds,
		model(glm::vec3(5.0f, 1.1f, -0.45f), glm::vec3(3.2f, 2.6f, 2.2f)), false });

	// Enable transparency blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	m_isPrepared = true;
}

/***********************************************************
 *  RenderScene()
 *
 *  This method is used for rendering the 3D scene by
 *  transforming and drawing the basic 3D shapes
 ***********************************************************/

void SceneManager::RenderScene()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/

	// ===================== 
	// Floor plane (table)
	// ===================== 
	SetSelectionHighlight("table");
	scaleXYZ = glm::vec3(20.0f, 1.0f, 10.0f);
	positionXYZ = glm::vec3(0.0f, 0.0f, 0.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// SetShaderColor(0.55f, 0.27f, 0.07f, 1.0f); // brown 

	// Set the active texture in the shader to use the "wod" texture
	SetShaderTexture("wood");

	// Set the UV scaling for the texture mapping to 1.0 on both U and V axes
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("wood");
	m_basicMeshes->DrawPlaneMesh();

	// ===================== 
	//  Cup body
	// ===================== 
	SetSelectionHighlight("cup");
	scaleXYZ = glm::vec3(1.0f, 3.0f, 1.0f);
	positionXYZ = glm::vec3(8.0f, 0.1f, 0.0f); // shifted left 
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.8f, 0.9f, 1.0f, 0.3f); // alpha < 1 
	SetShaderMaterial("glass");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("glass");
	m_basicMeshes->DrawCylinderMesh();

	// Cup lid 
	scaleXYZ = glm::vec3(1.1f, 0.2f, 1.1f);
	positionXYZ = glm::vec3(8.0f, 3.0f, 0.0f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.9f, 0.9f, 0.9f, 0.5f); // alpha < 1
	SetShaderMaterial("glass");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("glass");
	m_basicMeshes->DrawCylinderMesh();

	// Straw 
	scaleXYZ = glm::vec3(0.1f, 2.0f, 0.1f);
	positionXYZ = glm::vec3(8.0f, 1.7f, 0.0f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.0f, 0.6f, 0.0f, 1.0f);
	m_basicMeshes->DrawCylinderMesh();

	// ===================== // 
	// Candle 
	// ===================== // 
	// Candle body 
	SetSelectionHighlight("candle");
	scaleXYZ = glm::vec3(0.8f, 1.5f, 0.8f);
	positionXYZ = glm::vec3(-1.0f, 0.1f, 2.0f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	// SetShaderColor(1.0f, 0.95f, 0.8f, 1.0f);

	// Set the active texture in the shader to use the "onyx" texture
	SetShaderTexture("onyx");

	// Set the UV scaling for the texture mapping to 1.0 on both U and V axes
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("onyx");
	m_basicMeshes->DrawCylinderMesh();

	// Candle flame (small sphere) 
	scaleXYZ = glm::vec3(0.2f, 0.3f, 0.2f);
	positionXYZ = glm::vec3(-1.0f, 1.50f, 2.0f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(1.0f, 0.6f, 0.0f, 1.0f); // orange
	m_basicMeshes->DrawSphereMesh();

	// ===================== 
	// Succulent plant 
	// ===================== 
	// Pot 
	SetSelectionHighlight("plant");
	scaleXYZ = glm::vec3(1.2f, 0.8f, 1.2f);
	positionXYZ = glm::vec3(2.0f, 0.1f, -1.0f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(1.0f, 1.0f, 1.0f, 1.0f); // white
	m_basicMeshes->DrawCylinderMesh();

	// Leaves (cones pointing outward) 
	for (int i = 0; i < 6; i++) {
		float angle = i * 60.0f;
		scaleXYZ = glm::vec3(0.3f, 0.8f, 0.3f);
		positionXYZ = glm::vec3(2.0f + cos(glm::radians(angle)) * 0.6f, 1.0f, -1.0f + sin(glm::radians(angle)) * 0.6f);
		SetTransformations(scaleXYZ, -15.0f, angle, 0.0f, positionXYZ);
		SetShaderColor(0.0f, 0.5f, 0.0f, 1.0f);
		SetShaderMaterial("grass");
		SetTextureUVScale(1.0, 1.0);
		m_basicMeshes->DrawConeMesh();
	}

	// ===================== 
	// Laptop 
	// ===================== 
	// // Base (keyboard part) 
	SetSelectionHighlight("laptop");
	scaleXYZ = glm::vec3(3.0f, 0.2f, 2.0f);
	positionXYZ = glm::vec3(5.0f, 0.1f, 0.0f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.5f, 0.5f, 0.5f, 1.0f);
	m_basicMeshes->DrawBoxMesh();

	// Screen (upright box tilted slightly back) 
	scaleXYZ = glm::vec3(3.0f, 2.0f, 0.1f);
	positionXYZ = glm::vec3(5.0f, 1.166f, -0.95f);
	SetTransformations(scaleXYZ, -15.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderTexture("stainless");
	SetTextureUVScale(1.0, 1.0);
	SetShaderColor(0.1f, 0.1f, 0.1f, 1.0f);
	SetShaderMaterial("stainless");
	m_basicMeshes->DrawBoxMesh();
	m_pShaderManager->setBoolValue(g_SelectedName, false);
	/****************************************************************/
}
