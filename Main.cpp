#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Shader.h"
#include "Camera.h"
#include "Sphere.h"

// Variables
unsigned int SCR_WIDTH = 1280;
unsigned int SCR_HEIGHT = 720;


// GLFW Functions
void processInput(GLFWwindow* window); // Function to process input
void framebuffer_size_callback(GLFWwindow* window, int width, int height); // Function for establishing window size and resize detection and response
void mouse_callback(GLFWwindow* window, double xPosIn, double yPosIn); // Function to check for mouse movement

// Camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

bool firstClick = true;

// Time Management
float deltaTime = 0.0f;// time between current frame and last frame
float lastFrame = 0.0f;

// Main Function
int main()
{
	// GLFW Initialization
	glfwInit();

	// Tells GLFW which version of OpenGL we are using (v3.3)
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	// Tells OpenGL which profile we are using (Core means only modern OpenGL functions)
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// Window Creation
	// Creates a window in OpenGL
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Galaxy Simulation", NULL, NULL);

	// Error checking for window if it fails to create
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	// Defining a monitor
	const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

	// Centerring the window
	glfwSetWindowPos(window, (mode->width - SCR_HEIGHT / 2) - (mode->width / 2), (mode->height - SCR_HEIGHT / 2) - (mode->height / 2));

	// Make the window current context so we can use it
	glfwMakeContextCurrent(window);
	// Changes viewport to fit the screen width
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	// Loads GLAD so we can use OpenGL and check for errors if it fails
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	double previousTime = glfwGetTime();
	int frameCount = 0;

	// Shaders
	// Creates a vertex & fragment shader and attaches it to the source code for the shader then compiles it
	Shader defaultShader("default.vert", "default.frag");
	Shader bloomShader("bloom.vert", "bloom.frag");

	// Vertex management
	// Generates a Vertex Array Object & Sphere Objects and stores the vertices into them before sending them to the GPU
	unsigned int VAO;
	glGenVertexArrays(1, &VAO);

	// Binds Vertex Array Object
	glBindVertexArray(VAO);

	// Creates the sphere models for drawing
	Sphere star(1.0f);

	// Trasnformations
	unsigned int transformLoc = glGetUniformLocation(defaultShader.ID, "transform");

	// 3D Rendering
	// Fixes the Z-Axis buffer layering when drawing 
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	float time;

	// Main While Loop
	while (!glfwWindowShouldClose(window))
	{
		time = glfwGetTime();
		// Per-frame time logic
		float currentFrame = static_cast<float>(time);
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		frameCount++;

		if (currentFrame - previousTime >= 1.0)
		{
			std::cout << "FPS: " << frameCount << std::endl;
			std::cout << "Time: " << time << std::endl;
			frameCount = 0;
			previousTime = currentFrame;
		}

		processInput(window);

		// Render
		glClearColor(0.0f, 0.0f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		defaultShader.use();

		// Creates the model, view & projection matrices
		glm::mat4 projection = glm::mat4(1.0f);
		glm::mat4 view = glm::mat4(1.0f);

		// Look at function (cameraPos, cameraTarget, worldUp)
		view = camera.GetViewMatrix();
		projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 10000.0f);

		int viewLoc = glGetUniformLocation(defaultShader.ID, "view");
		int projectionLoc = glGetUniformLocation(defaultShader.ID, "projection");

		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
		glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));


		glm::mat4 model = glm::mat4(1.0f);      // identity
		model = glm::translate(model, glm::vec3(0.0f));

		// ----------------------------
		// STEP 1: Compute sphere center + radius in screen space
		// ----------------------------

		glm::vec3 sphereCenter = glm::vec3(0.0f, 0.0f, 0.0f);   // your sphere position
		float sphereRadius = star.getRadius();              // 1.0f in your Sphere.h

		// Project center to NDC
		glm::vec4 clipCenter = projection * view * glm::vec4(sphereCenter, 1.0f);
		glm::vec3 ndcCenter = glm::vec3(clipCenter) / clipCenter.w;

		// Convert to screen pixels
		glm::vec2 centerScreen;
		centerScreen.x = (ndcCenter.x * 0.5f + 0.5f) * SCR_WIDTH;
		centerScreen.y = (ndcCenter.y * 0.5f + 0.5f) * SCR_HEIGHT;

		// *** use CAMERA RIGHT, not world +X ***
		glm::vec3 cameraRight(view[0][0], view[1][0], view[2][0]); // first column of view = right vector
		cameraRight = glm::normalize(cameraRight);

		// Project a point on the sphere surface in camera-right direction
		glm::vec4 clipEdge = projection * view *
			glm::vec4(sphereCenter + cameraRight * sphereRadius, 1.0f);
		glm::vec3 ndcEdge = glm::vec3(clipEdge) / clipEdge.w;

		glm::vec2 edgeScreen;
		edgeScreen.x = (ndcEdge.x * 0.5f + 0.5f) * SCR_WIDTH;
		edgeScreen.y = (ndcEdge.y * 0.5f + 0.5f) * SCR_HEIGHT;

		// Radius of sphere on the screen, in pixels
		float sphereRadiusPx = glm::length(edgeScreen - centerScreen);

		// How wide the glow band should be (relative to radius in pixels)
		float glowWidthPx = sphereRadiusPx * 0.4f;  // tweak 0.3–0.5 to taste

		// solid core
		defaultShader.use();
		defaultShader.setMat4("view", view);
		defaultShader.setMat4("projection", projection);
		defaultShader.setMat4("model", model);
		defaultShader.setVec3("color", glm::vec3(1.0f));   // white
		star.draw();

		// ---- Glow pass ----

		// slightly bigger model for the halo
		glm::mat4 glowModel = glm::scale(model, glm::vec3(1.8f)); // try 1.5–2.0f

		// don’t write depth, and use additive-ish blending
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		// src alpha controls strength, but we add to existing color
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);

		bloomShader.use();
		bloomShader.setMat4("view", view);
		bloomShader.setMat4("projection", projection);
		bloomShader.setMat4("model", glowModel);

		bloomShader.setVec3("color", glm::vec3(1.0f));
		bloomShader.setFloat("glowStrength", 3.0f);

		bloomShader.setVec2("centerScreen", centerScreen);
		bloomShader.setFloat("sphereRadiusPx", sphereRadiusPx);
		bloomShader.setFloat("glowWidthPx", glowWidthPx);

		bloomShader.setFloat("time", time);

		star.draw();

		// restore state
		glDepthMask(GL_TRUE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glBindVertexArray(0);
	glDeleteVertexArrays(1, &VAO);
	glDeleteShader(defaultShader.ID);

	glfwTerminate();
	return 0;
}



void processInput(GLFWwindow* window)
{
	// -- WINDOW --
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		exit(EXIT_FAILURE);
	}

	// -- MOVEMENT --
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		camera.ProcessKeyboard(FORWARD, deltaTime);
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	}

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		camera.ProcessKeyboard(LEFT, deltaTime);
	}

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		camera.ProcessKeyboard(RIGHT, deltaTime);
	}

	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
	{
		camera.ProcessKeyboard(UP, deltaTime);
	}

	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
	{
		camera.ProcessKeyboard(DOWN, deltaTime);
	}

	//  -- MOUSE --
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
	{
		if (firstClick)
		{
			glfwSetCursorPosCallback(window, mouse_callback);
			// Tells GLFW to hide and capture the mouse
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			firstClick = false;
		}
	}


	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE)
	{
		if (!firstClick)
		{
			glfwSetCursorPosCallback(window, NULL);
			// Tells GLFW to hide and capture the mouse
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			firstClick = true;
			firstMouse = true;
		}
	}
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xPosIn, double yPosIn)
{

	float xPos = static_cast<float>(xPosIn);
	float yPos = static_cast<float>(yPosIn);

	if (firstMouse)
	{
		lastX = xPos;
		lastY = yPos;
		firstMouse = false;
	}

	float xOffset = xPos - lastX;
	float yOffset = lastY - yPos;
	lastX = xPos;
	lastY = yPos;

	camera.ProcessMouseMovement(xOffset, yOffset);

}