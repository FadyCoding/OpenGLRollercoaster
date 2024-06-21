#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Texture.h"
#include "shaderClass.h"
#include "VAO.h"
#include "VBO.h"
#include "EBO.h"
#include "Camera.h"

const unsigned int width = 800;
const unsigned int height = 800;

bool showSkybox = true;

float skyboxVertices[] =
{
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f
};

unsigned int skyboxIndices[] =
{
    1, 2, 6,
    6, 5, 1,
    0, 4, 7,
    7, 3, 0,
    4, 5, 6,
    6, 7, 4,
    0, 3, 2,
    2, 1, 0,
    0, 1, 5,
    5, 4, 0,
    3, 7, 6,
    6, 2, 3
};

struct Mesh {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

Mesh loadModel(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        exit(-1);
    }

    aiMesh* mesh = scene->mMeshes[0];
    Mesh myMesh;

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        myMesh.vertices.push_back(mesh->mVertices[i].x);
        myMesh.vertices.push_back(mesh->mVertices[i].y);
        myMesh.vertices.push_back(mesh->mVertices[i].z);

        if (mesh->HasNormals()) {
            myMesh.vertices.push_back(mesh->mNormals[i].x);
            myMesh.vertices.push_back(mesh->mNormals[i].y);
            myMesh.vertices.push_back(mesh->mNormals[i].z);
        }
        else {
            myMesh.vertices.push_back(0.0f);
            myMesh.vertices.push_back(0.0f);
            myMesh.vertices.push_back(0.0f);
        }

        if (mesh->mTextureCoords[0]) {
            myMesh.vertices.push_back(mesh->mTextureCoords[0][i].x);
            myMesh.vertices.push_back(mesh->mTextureCoords[0][i].y);
        }
        else {
            myMesh.vertices.push_back(0.0f);
            myMesh.vertices.push_back(0.0f);
        }
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            myMesh.indices.push_back(face.mIndices[j]);
        }
    }

    return myMesh;
}

// Spline movements functions
glm::vec3 hermiteInterpolate(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& m0, const glm::vec3& m1, float t) {
    float t2 = t * t;
    float t3 = t2 * t;

    float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    float h10 = t3 - 2.0f * t2 + t;
    float h01 = -2.0f * t3 + 3.0f * t2;
    float h11 = t3 - t2;

    return h00 * p0 + h10 * m0 + h01 * p1 + h11 * m1;
}

glm::vec3 calculateTangent(const glm::vec3& p0, const glm::vec3& p1) {
    return (p1 - p0) * 0.5f;
}

void processInput(GLFWwindow* window)
{
    static bool keyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS && !keyPressed)
    {
        showSkybox = !showSkybox;
        keyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_RELEASE)
    {
        keyPressed = false;
    }
}

int main()
{
    // Initialize GLFW
    glfwInit();

    // OpenGL version and compatibility settings
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(width, height, "Rollercoaster", NULL, NULL);
    // Error check if the window fails to create
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    // Introduce the window into the current context
    glfwMakeContextCurrent(window);
    gladLoadGL();
    glViewport(0, 0, width, height);

    // Generates Shader object
    Shader rollerShader("roller.vert", "roller.frag");
    Shader skyboxShader("skybox.vert", "skybox.frag");

    skyboxShader.Activate();
    glUniform1i(glGetUniformLocation(skyboxShader.ID, "skybox"), 0);

    // Load models using Assimp
    Mesh rollerModel = loadModel("C:\\Users\\Fady\\Desktop\\rollercoaster\\projectRoller\\projectRoller\\rollerOBJ.obj");
    Mesh carModel = loadModel("C:\\Users\\Fady\\Desktop\\rollercoaster\\projectRoller\\projectRoller\\rollerCar.obj");

    // Generate and bind VAO, VBO, EBO for roller model
    VAO rollerVAO;
    rollerVAO.Bind();
    VBO rollerVBO(rollerModel.vertices.data(), rollerModel.vertices.size() * sizeof(float));
    EBO rollerEBO(rollerModel.indices.data(), rollerModel.indices.size() * sizeof(unsigned int));
    rollerVAO.LinkAttrib(rollerVBO, 0, 3, GL_FLOAT, 8 * sizeof(float), (void*)0);
    rollerVAO.LinkAttrib(rollerVBO, 1, 3, GL_FLOAT, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    rollerVAO.LinkAttrib(rollerVBO, 2, 2, GL_FLOAT, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    rollerVAO.Unbind();
    rollerVBO.Unbind();
    rollerEBO.Unbind();

    // Generate and bind VAO, VBO, EBO for car model
    VAO carVAO;
    carVAO.Bind();
    VBO carVBO(carModel.vertices.data(), carModel.vertices.size() * sizeof(float));
    EBO carEBO(carModel.indices.data(), carModel.indices.size() * sizeof(unsigned int));
    carVAO.LinkAttrib(carVBO, 0, 3, GL_FLOAT, 8 * sizeof(float), (void*)0);
    carVAO.LinkAttrib(carVBO, 1, 3, GL_FLOAT, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    carVAO.LinkAttrib(carVBO, 2, 2, GL_FLOAT, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    carVAO.Unbind();
    carVBO.Unbind();
    carEBO.Unbind();

    // Load texture
    Texture rollerTex("RED.png", GL_TEXTURE_2D, GL_TEXTURE0, GL_RGBA, GL_UNSIGNED_BYTE);
    rollerTex.texUnit(rollerShader, "tex0", 0);

    glEnable(GL_DEPTH_TEST);

    // Create camera object
    Camera camera(width, height, glm::vec3(0.0f, 0.0f, 5.0f));

    // Control points for the spline
    std::vector<glm::vec3> controlPoints = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.5f, 0.0f),
        glm::vec3(2.0f, 1.0f, 0.0f),
        glm::vec3(3.0f, 0.5f, 0.0f),
        glm::vec3(4.0f, 0.0f, 0.0f)
    };

    float time = 0.0f;

    // Create VAO, VBO, and EBO for the skybox
    unsigned int skyboxVAO, skyboxVBO, skyboxEBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glGenBuffers(1, &skyboxEBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyboxEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(skyboxIndices), &skyboxIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);


    std::string facesCubemap[6] =
    {
        "C:\\Users\\Fady\\Desktop\\rollercoaster\\projectRoller\\projectRoller\\right.jpg",
        "C:\\Users\\Fady\\Desktop\\rollercoaster\\projectRoller\\projectRoller\\left.jpg",
        "C:\\Users\\Fady\\Desktop\\rollercoaster\\projectRoller\\projectRoller\\top.jpg",
        "C:\\Users\\Fady\\Desktop\\rollercoaster\\projectRoller\\projectRoller\\bottom.jpg",
        "C:\\Users\\Fady\\Desktop\\rollercoaster\\projectRoller\\projectRoller\\front.jpg",
        "C:\\Users\\Fady\\Desktop\\rollercoaster\\projectRoller\\projectRoller\\back.jpg"
    };

    // Creates the cubemap texture object
    unsigned int cubemapTexture;
    glGenTextures(1, &cubemapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    // Attache textures to the cubemap object
    for (unsigned int i = 0; i < 6; i++)
    {
        int width, height, nrChannels;
        unsigned char* data = stbi_load(facesCubemap[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            stbi_set_flip_vertically_on_load(false);
            glTexImage2D
            (
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0,
                GL_RGB,
                width,
                height,
                0,
                GL_RGB,
                GL_UNSIGNED_BYTE,
                data
            );
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Failed to load texture: " << facesCubemap[i] << std::endl;
            stbi_image_free(data);
        }
    }

    // Main while loop
    while (!glfwWindowShouldClose(window))
    {
        // Process input
        processInput(window);

        // Background color
        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        rollerShader.Activate();

        camera.Inputs(window);
        camera.Matrix(45.0f, 0.1f, 100.0f, rollerShader, "camMatrix");

        rollerTex.Bind();

        rollerVAO.Bind();
        // Draw roller model
        glDrawElements(GL_TRIANGLES, rollerModel.indices.size(), GL_UNSIGNED_INT, 0);

        // Bind the VAO for car model and draw it
        carVAO.Bind();
        glDrawElements(GL_TRIANGLES, carModel.indices.size(), GL_UNSIGNED_INT, 0);

        if (showSkybox) {
            glDepthFunc(GL_LEQUAL); 
            skyboxShader.Activate();
            glm::mat4 view = glm::mat4(1.0f);
            glm::mat4 projection = glm::mat4(1.0f);
            view = glm::mat4(glm::mat3(glm::lookAt(camera.Position, camera.Position + camera.Orientation, camera.Up)));
            projection = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 100.0f);
            glUniformMatrix4fv(glGetUniformLocation(skyboxShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(skyboxShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

            glBindVertexArray(skyboxVAO); 
            glActiveTexture(GL_TEXTURE0); 
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture); 
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0); 
            glBindVertexArray(0); 
            glDepthFunc(GL_LESS); 
        }

        // Swap the back buffer with the front buffer
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Delete all the objects
    rollerVAO.Delete();
    rollerVBO.Delete();
    rollerEBO.Delete();
    carVAO.Delete();
    carVBO.Delete();
    carEBO.Delete();
    rollerTex.Delete();
    rollerShader.Delete();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
