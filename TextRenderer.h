#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Shader.h"

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

class TextRenderer
{
private:
    Shader shader;
    unsigned int VAO, VBO;
    int screenWidth, screenHeight;

public:
    TextRenderer(int width, int height)
        : shader("text.vert", "text.frag"),
        screenWidth(width),
        screenHeight(height)
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, 100000, NULL, GL_DYNAMIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
            2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);
    }

    void resize(int width, int height)
    {
        screenWidth = width;
        screenHeight = height;
    }

    void draw(const std::string& text, float x, float y, float scale = 1.0f)
    {
        char buffer[99999];

        int num_quads = stb_easy_font_print(
            x, y,
            (char*)text.c_str(),
            NULL,
            buffer,
            sizeof(buffer)
        );

        std::vector<float> vertices;
        vertices.reserve(num_quads * 6 * 2);

        float* quad = (float*)buffer;

        for (int i = 0; i < num_quads; i++)
        {
            float x0 = quad[i * 16 + 0] * scale;
            float y0 = quad[i * 16 + 1] * scale;

            float x1 = quad[i * 16 + 4] * scale;
            float y1 = quad[i * 16 + 5] * scale;

            float x2 = quad[i * 16 + 8] * scale;
            float y2 = quad[i * 16 + 9] * scale;

            float x3 = quad[i * 16 + 12] * scale;
            float y3 = quad[i * 16 + 13] * scale;

            vertices.insert(vertices.end(), { x0,y0, x1,y1, x2,y2 });
            vertices.insert(vertices.end(), { x0,y0, x2,y2, x3,y3 });
        }

        glDisable(GL_DEPTH_TEST);

        shader.use();

        glm::mat4 projection = glm::ortho(
            0.0f, (float)screenWidth,
            (float)screenHeight, 0.0f
        );

        shader.setMat4("projection", projection);
        shader.setVec3("color", glm::vec3(1.0f));

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
            vertices.size() * sizeof(float),
            vertices.data(),
            GL_DYNAMIC_DRAW);

        glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 2);

        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
    }
};
