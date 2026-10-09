///////////////////////////////////////////////////////////////////////////////
// ViewManager.h
// =============
// AUTHOR: Crystal Schippers
// 
// PURPOSE: Declares the camera, input, projection, and viewport-management
// interface used to view and navigate the 3D scene.
// 
// CAPSTONE ENHANCEMENT: Provides a modular viewing component that supports the
// enhanced application's interactive selection and manipulation workflow.
// 
// COURSE: CS 499 Computer Science Capstone, 2026
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ShaderManager.h"
#include "camera.h"

// GLFW library
#include "GLFW/glfw3.h" 

class ViewManager
{
public:
	// constructor
	ViewManager(
		ShaderManager* pShaderManager);
	// destructor
	~ViewManager();

	// mouse position callback for mouse interaction with the 3D scene
	static void Mouse_Position_Callback(GLFWwindow* window, double xMousePos, double yMousePos);

private:
	// pointer to shader manager object
	ShaderManager* m_pShaderManager;
	// active OpenGL display window
	GLFWwindow* m_pWindow;
	glm::mat4 m_viewMatrix;
	glm::mat4 m_projectionMatrix;

	// process keyboard events for interaction with the 3D scene
	void ProcessKeyboardEvents();

public:
	// create the initial OpenGL display window
	GLFWwindow* CreateDisplayWindow(const char* windowTitle);
	
	// prepare the conversion from 3D object display to 2D scene display
	void PrepareSceneView();
	glm::mat4 GetViewMatrix() const;
	glm::mat4 GetProjectionMatrix() const;
	glm::vec3 GetCameraPosition() const;
};
