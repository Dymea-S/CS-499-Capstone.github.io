#include <iostream>         // error handling and output
#include <cstdlib>          // EXIT_FAILURE

#include <GL/glew.h>        // GLEW library
#include "GLFW/glfw3.h"     // GLFW library

// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "SceneManager.h"
#include "ViewManager.h"
#include "ShapeMeshes.h"
#include "ShaderManager.h"

// Namespace for declaring global variables
namespace
{
	// Macro for window title
	const char* const WINDOW_TITLE = "Dymea Schippers - Interactive 3D Desktop Scene";

	// Main GLFW window
	GLFWwindow* g_Window = nullptr;

	// scene manager object for managing the 3D scene prepare and render
	SceneManager* g_SceneManager = nullptr;
	// shader manager object for dynamic interaction with the shader code
	ShaderManager* g_ShaderManager = nullptr;
	// view manager object for managing the 3D view setup and projection to 2D
	ViewManager* g_ViewManager = nullptr;
	bool g_PreviousLeftMouseState = false;
}

// Function declarations - all functions that are called manually
// need to be pre-declared at the beginning of the source code.
bool InitializeGLFW();
bool InitializeGLEW();
Ray BuildCenterScreenRay();
void ProcessObjectSelection();

Ray BuildCenterScreenRay()
{
	// Convert the center of the viewport from clip space into world space.
	// Center-screen picking fits the captured-mouse camera controls: the user
	// aims the camera at an object and clicks the left mouse button.
	const glm::vec4 clipDirection(0.0f, 0.0f, -1.0f, 1.0f);
	glm::vec4 eyeDirection = glm::inverse(g_ViewManager->GetProjectionMatrix()) * clipDirection;
	eyeDirection = glm::vec4(eyeDirection.x, eyeDirection.y, -1.0f, 0.0f);
	const glm::vec3 worldDirection = glm::normalize(glm::vec3(
		glm::inverse(g_ViewManager->GetViewMatrix()) * eyeDirection));
	return { g_ViewManager->GetCameraPosition(), worldDirection };
}

void ProcessObjectSelection()
{
	const bool leftMousePressed =
		glfwGetMouseButton(g_Window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
	if (leftMousePressed && !g_PreviousLeftMouseState)
	{
		const SceneObject* selected = g_SceneManager->SelectObject(BuildCenterScreenRay());
		if (selected == nullptr)
		{
			std::cout << "Selection: no object intersected." << std::endl;
		}
		else
		{
			std::cout << "Selection: " << selected->displayName
				<< " [" << selected->id << "]" << std::endl;
		}
	}
	g_PreviousLeftMouseState = leftMousePressed;
}


/***********************************************************
 *  main(int, char*)
 *
 *  This function gets called after the application has been
 *  launched.
 ***********************************************************/
int main(int argc, char* argv[])
{
	// if GLFW fails initialization, then terminate the application
	if (InitializeGLFW() == false)
	{
		return(EXIT_FAILURE);
	}

	// try to create a new shader manager object
	g_ShaderManager = new ShaderManager();
	// try to create a new view manager object
	g_ViewManager = new ViewManager(
		g_ShaderManager);

	// try to create the main display window
	g_Window = g_ViewManager->CreateDisplayWindow(WINDOW_TITLE);
	if (g_Window == nullptr)
	{
		delete g_ViewManager;
		g_ViewManager = nullptr;
		delete g_ShaderManager;
		g_ShaderManager = nullptr;
		return(EXIT_FAILURE);
	}

	// if GLEW fails initialization, then terminate the application
	if (InitializeGLEW() == false)
	{
		return(EXIT_FAILURE);
	}

	// load the shader code from the external GLSL files
	const GLuint shaderProgram = g_ShaderManager->LoadShaders(
		"../../Utilities/shaders/vertexShader.glsl",
		"../../Utilities/shaders/fragmentShader.glsl");
	if (shaderProgram == 0)
	{
		std::cerr << "Unable to create the shader program." << std::endl;
		delete g_ViewManager;
		g_ViewManager = nullptr;
		delete g_ShaderManager;
		g_ShaderManager = nullptr;
		glfwTerminate();
		return(EXIT_FAILURE);
	}
	g_ShaderManager->use();

	// try to create a new scene manager object and prepare the 3D scene
	g_SceneManager = new SceneManager(g_ShaderManager);
	g_SceneManager->PrepareScene();

	// loop will keep running until the application is closed 
	// or until an error has occurred
	while (!glfwWindowShouldClose(g_Window))
	{
		// Enable z-depth
		glEnable(GL_DEPTH_TEST);

		// Clear the frame and z buffers
		// Use a soft blue-gray background so unlit edges remain visible.
		glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// convert from 3D object space to 2D view
		g_ViewManager->PrepareSceneView();
		ProcessObjectSelection();

		// refresh the 3D scene
		g_SceneManager->RenderScene();


		// Flips the the back buffer with the front buffer every frame.
		glfwSwapBuffers(g_Window);

		// query the latest GLFW events
		glfwPollEvents();
	}

	// clear the allocated manager objects from memory
	if (NULL != g_SceneManager)
	{
		delete g_SceneManager;
		g_SceneManager = NULL;
	}
	if (NULL != g_ViewManager)
	{
		delete g_ViewManager;
		g_ViewManager = NULL;
	}
	if (NULL != g_ShaderManager)
	{
		delete g_ShaderManager;
		g_ShaderManager = NULL;
	}

	// Terminates the program successfully
	glfwTerminate();
	return(EXIT_SUCCESS);
}

/***********************************************************
 *	InitializeGLFW()
 *
 *  This function is used to initialize the GLFW library.
 ***********************************************************/
bool InitializeGLFW()
{
	// GLFW: initialize and configure library
	// --------------------------------------
	if (glfwInit() == GLFW_FALSE)
	{
		std::cerr << "Failed to initialize GLFW." << std::endl;
		return false;
	}

#ifdef __APPLE__
	// set the version of OpenGL and profile to use
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
	// set the version of OpenGL and profile to use
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
	// GLFW: end -------------------------------

	return(true);
}

/***********************************************************
 *	InitializeGLEW()
 *
 *  This function is used to initialize the GLEW library.
 ***********************************************************/
bool InitializeGLEW()
{
	// GLEW: initialize
	// -----------------------------------------
	GLenum GLEWInitResult = GLEW_OK;

	// try to initialize the GLEW library
	GLEWInitResult = glewInit();
	if (GLEW_OK != GLEWInitResult)
	{
		std::cerr << glewGetErrorString(GLEWInitResult) << std::endl;
		return false;
	}
	// GLEW: end -------------------------------

	// Displays a successful OpenGL initialization message
	std::cout << "INFO: OpenGL Successfully Initialized\n";
	std::cout << "INFO: OpenGL Version: " << glGetString(GL_VERSION) << "\n" << std::endl;

	return(true);
}
///////////////////////////////////////////////////////////////////////////////
// MainCode.cpp
// ============
// Starts the OpenGL application, creates the rendering services, and runs the
// frame loop for the interactive CS 330 desktop scene.
//
// ORIGINAL COURSE FRAMEWORK: SNHU CS 330
// ENHANCED BY: Dymea Schippers - CS 499 Computer Science Capstone, 2026
///////////////////////////////////////////////////////////////////////////////
