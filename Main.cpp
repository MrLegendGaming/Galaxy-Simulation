
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"
#include "Camera.h"
#include "Sphere.h"

// Variables
unsigned int SCR_WIDTH = 1280;
unsigned int SCR_HEIGHT = 720;

// GLFW Functions
void processInput(GLFWwindow* window); // Function to process input
void framebuffer_size_callback(GLFWwindow* window, int width, int height); // Resize
void mouse_callback(GLFWwindow* window, double xPosIn, double yPosIn); // Mouse

// Camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
bool firstClick = true;

// Time Management
float deltaTime = 0.0f; // time between current frame and last frame
float lastFrame = 0.0f;

int main()
{
    // GLFW Initialization
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Window Creation
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Galaxy Simulation", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Center window
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    glfwSetWindowPos(window, (mode->width - SCR_HEIGHT / 2) - (mode->width / 2),
        (mode->height - SCR_HEIGHT / 2) - (mode->height / 2));
    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    double previousTime = glfwGetTime();
    int frameCount = 0;

    // --- Shaders ---
    Shader defaultShader("default.vert", "default.frag");
    // We keep bloomShader if you still need mesh-based glow elsewhere, but we won't use it in this screen-space path.
    Shader bloomShader("bloom.vert", "bloom.frag");
    Shader glowScreenShader("glow_screen.vert", "bloom.frag"); // NEW: screen-space glow pipeline

    // --- Geometry ---
    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    Sphere star(1.0f);

    // Transform uniform location (if used by default shader)
    unsigned int transformLoc = glGetUniformLocation(defaultShader.ID, "transform");

    // 3D Rendering state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND); // We will toggle blend funcs around passes
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ---- Full-screen quad for screen-space glow ----
    unsigned int fsVAO = 0, fsVBO = 0;
    {
        // Triangle strip covering entire screen in NDC
        float fsQuad[] = {
            -1.0f, -1.0f,
             1.0f, -1.0f,
            -1.0f,  1.0f,
             1.0f,  1.0f
        };

        glGenVertexArrays(1, &fsVAO);
        glGenBuffers(1, &fsVBO);
        glBindVertexArray(fsVAO);

        glBindBuffer(GL_ARRAY_BUFFER, fsVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fsQuad), fsQuad, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);
    }

    float time;

    // ----- Main Loop -----
    while (!glfwWindowShouldClose(window))
    {
        time = glfwGetTime();
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

        // ---- Matrices ----
        defaultShader.use();

        glm::mat4 projection = glm::mat4(1.0f);
        glm::mat4 view = glm::mat4(1.0f);

        view = camera.GetViewMatrix();
        projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 10000.0f);

        int viewLoc = glGetUniformLocation(defaultShader.ID, "view");
        int projectionLoc = glGetUniformLocation(defaultShader.ID, "projection");
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f)); // sphere at origin

        // ---- STEP 1: Compute sphere center + radius in screen space ----
        glm::vec3 sphereCenter = glm::vec3(0.0f, 0.0f, 0.0f);
        float sphereRadius = star.getRadius(); // 1.0f

        // Project center to clip/NDC
        glm::vec4 clipCenter = projection * view * glm::vec4(sphereCenter, 1.0f);
        glm::vec3 ndcCenter = glm::vec3(clipCenter) / clipCenter.w;

        // Convert to screen pixels
        glm::vec2 centerScreen;
        centerScreen.x = (ndcCenter.x * 0.5f + 0.5f) * SCR_WIDTH;
        centerScreen.y = (ndcCenter.y * 0.5f + 0.5f) * SCR_HEIGHT;

        // Camera right (first column of view matrix) for pixel radius estimate
        glm::vec3 cameraRight(view[0][0], view[1][0], view[2][0]);
        cameraRight = glm::normalize(cameraRight);

        glm::vec4 clipEdge = projection * view *
            glm::vec4(sphereCenter + cameraRight * sphereRadius, 1.0f);
        glm::vec3 ndcEdge = glm::vec3(clipEdge) / clipEdge.w;

        glm::vec2 edgeScreen;
        edgeScreen.x = (ndcEdge.x * 0.5f + 0.5f) * SCR_WIDTH;
        edgeScreen.y = (ndcEdge.y * 0.5f + 0.5f) * SCR_HEIGHT;

        float sphereRadiusPx = glm::length(edgeScreen - centerScreen);
        float glowWidthPx = sphereRadiusPx * 0.4f; // tweak to taste

        // ---- Visibility gate: only draw glow when sphere is in front and in frustum ----
        glm::vec3 viewCenter = glm::vec3(view * glm::vec4(sphereCenter, 1.0f));
        bool inFrontOfCamera = (viewCenter.z < 0.0f);

        bool ndcValid = (clipCenter.w > 0.0f) &&
            (ndcCenter.x >= -1.0f && ndcCenter.x <= 1.0f) &&
            (ndcCenter.y >= -1.0f && ndcCenter.y <= 1.0f) &&
            (ndcCenter.z >= -1.0f && ndcCenter.z <= 1.0f);

        bool drawGlow = inFrontOfCamera && ndcValid;

        // ---- Solid core (regular sphere) ----
        defaultShader.use();
        defaultShader.setMat4("view", view);
        defaultShader.setMat4("projection", projection);
        defaultShader.setMat4("model", model);
        defaultShader.setVec3("color", glm::vec3(1.0f)); // white
        star.draw();

        // ---- Screen-space glow pass ----
        if (drawGlow)
        {
            glDisable(GL_DEPTH_TEST);                // full-screen overlay; no depth clip
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);       // additive blend

            glowScreenShader.use();
            glowScreenShader.setVec3("color", glm::vec3(1.0f));       // white glow
            glowScreenShader.setFloat("glowStrength", 3.0f);          // intensity
            glowScreenShader.setVec2("centerScreen", centerScreen);   // uniforms already computed
            glowScreenShader.setFloat("sphereRadiusPx", sphereRadiusPx);
            glowScreenShader.setFloat("glowWidthPx", glowWidthPx);
            glowScreenShader.setFloat("time", time);

            glBindVertexArray(fsVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);

            // restore
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &VAO);
    glDeleteVertexArrays(1, &fsVAO);
    glDeleteBuffers(1, &fsVBO);

    glDeleteShader(defaultShader.ID);
    glDeleteShader(bloomShader.ID);
    glDeleteShader(glowScreenShader.ID);

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
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { camera.ProcessKeyboard(FORWARD, deltaTime); }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { camera.ProcessKeyboard(BACKWARD, deltaTime); }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { camera.ProcessKeyboard(LEFT, deltaTime); }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { camera.ProcessKeyboard(RIGHT, deltaTime); }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) { camera.ProcessKeyboard(UP, deltaTime); }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) { camera.ProcessKeyboard(DOWN, deltaTime); }

    // -- MOUSE --
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        if (firstClick)
        {
            glfwSetCursorPosCallback(window, mouse_callback);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstClick = false;
        }
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE)
    {
        if (!firstClick)
        {
            glfwSetCursorPosCallback(window, NULL);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            firstClick = true;
            firstMouse = true;
        }
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    // If you want perfect pixel mapping for the glow when resizing, also update SCR_WIDTH/HEIGHT
    SCR_WIDTH = width;
    SCR_HEIGHT = height;
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
    float yOffset = lastY - yPos; // reversed since y-coordinates go from bottom to top

    lastX = xPos;
    lastY = yPos;

    camera.ProcessMouseMovement(xOffset, yOffset);
}
