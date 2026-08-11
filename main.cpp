#define GL_SILENCE_DEPRECATION
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "cy/cyTriMesh.h"
#include "cy/cyGL.h"
#include "cy/cyMatrix.h"
#include "lodepng.h"
#include <iostream>
#include <iterator>

const int width = 1280;
const int height = 720;
bool isLeftDragging = false;
bool isRightDragging = false;
bool movingLight = false;
double prev_x = width / 2.0;
double prev_y = height / 2.0;
float yRot;
float xRot;
double prev_x_l = width / 2.0;
double prev_y_l = height / 2.0;
float yRot_l = cy::Deg2Rad(-40.0f);
float xRot_l = cy::Deg2Rad(210.0f);
bool swap = true;
float zoom = -50.0;
float zoomFactor = 100.0f;
float znear = 0.1f;
float zfar = 10000.0f;
float sensitivity = 1.0;
cyTriMesh triMesh;
cyGLSLProgram shaderProgram;

struct material {
    std::string name;

    cyVec3f ambientColor;
    cyVec3f diffuseColor;
    cyVec3f specularColor;
    float specularExp;
    
    cyGLTexture2D diffuseTex;
    cyGLTexture2D specularTex;

    bool hasDiffuseTex = false;
    bool hasSpecularTex = false;
};

struct textureImage {
    std::vector<unsigned char> image;
    unsigned int width;
    unsigned int height;
};

void display(GLFWwindow *window, GLuint vao, int nv, std::vector<material>& materials) {
    // clear screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shaderProgram.Bind();
    // glEnableVertexAttribArray(0);
    // glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(vao);

    for (int i = 0; i < materials.size(); i++) {
        const auto &material = materials[i];
        
        shaderProgram["hasDiffTex"] = material.hasDiffuseTex;
        shaderProgram["hasSpecTex"] = material.hasSpecularTex;
        
        if (material.hasDiffuseTex) {
            material.diffuseTex.Bind(0);
        } else {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        shaderProgram["diffTex"] = 0;
        
        if (material.hasSpecularTex) {
            material.specularTex.Bind(1);
        } else {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        shaderProgram["specTex"] = 1;

        shaderProgram["ka"] = material.ambientColor;
        shaderProgram["kd"] = material.diffuseColor;
        shaderProgram["ks"] = material.specularColor;
        shaderProgram["ns"] = material.specularExp;

        GLint start;
        GLsizei vertexCount;
        if (materials.size() == 1) {
            start = 0;
            vertexCount = nv;
        } else {
            int firstFaceIndex = triMesh.GetMaterialFirstFace(i);
            int faceCount = triMesh.GetMaterialFaceCount(i);
            start = firstFaceIndex * 3;
            vertexCount = faceCount * 3;
        }
        glDrawArrays(GL_TRIANGLES, start, vertexCount);
    }
    
    glBindVertexArray(0);

    // swap buffers (double buffering, done rendering image so swap front and back buffers)
    glfwSwapBuffers(window);
}

void setLightDirection() {
    shaderProgram.Bind();
    
    // std::cout << xRot_l << " " << yRot_l << std::endl;
    cyVec3f lightDir = cyVec3f(-cos(yRot_l)*sin(xRot_l), -sin(yRot_l), -cos(yRot_l)*cos(xRot_l));
    // if (swap) {
    //     lightDir = cyVec3f(-cos(yRot_l)*sin(xRot_l), -cos(yRot_l)*cos(xRot_l), -sin(yRot_l));
    // }
    lightDir.Normalize();
    shaderProgram["lightDir"] = lightDir;
}

cyMatrix4f getSwapMatrix(bool swap) {
    if (!swap) return cyMatrix4f::Identity();

    cyMatrix4f s;
    s.SetRotationX(cy::Deg2Rad(-90.0f));
    return s;
}

void updateCameraMatrices() {
    shaderProgram.Bind();

    cyMatrix4f transMatrix = cyMatrix4f::Identity();
    transMatrix.SetTranslation(cyVec3f(0,0,zoom));

    cyMatrix3f rotMatrix = cyMatrix3f::RotationX(yRot) * cyMatrix3f::RotationY(xRot);
    cyMatrix4f projMatrix = cyMatrix4f::Perspective(
        cy::Deg2Rad(40),
        float(width)/float(height),
        znear,
        zfar);
    cyMatrix4f swapMatrix = getSwapMatrix(swap);

    cyMatrix4f mv = transMatrix * rotMatrix * swapMatrix;
    cyMatrix4f mvp = projMatrix * mv;


    shaderProgram.SetUniformMatrix4("mvp", &mvp[0]);
    shaderProgram.SetUniformMatrix4("mv", &mv[0]);
}

void centerObject() {
    // center object with bounding box
    triMesh.ComputeBoundingBox();

    cyVec3f bmin = triMesh.GetBoundMin();
    cyVec3f bmax = triMesh.GetBoundMax();
    cyVec3f center = (bmin + bmax) * 0.5f;

    cyVec3f diagonal = bmax - bmin;
    float objectSize = diagonal.Length();

    float fov = cy::Deg2Rad(40);
    float radius = objectSize * 0.5f;
    float cameraDistance = radius / std::sin(fov * 0.5f);
    znear = objectSize * 0.01f;
    zfar = cameraDistance + objectSize * 2.0f;

    zoom = -cameraDistance;

    for (int i = 0; i < triMesh.NV(); i++) {
        triMesh.V(i) -= center;
    }
    triMesh.ComputeBoundingBox();
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    switch (key)
    {
    case GLFW_KEY_ESCAPE: // close window
        glfwSetWindowShouldClose(window, true);
        break;
    case GLFW_KEY_F6: // recompile shaders
    {
        // recompile shaders
        shaderProgram.BuildFiles("shader.vert", "shader.frag");
        updateCameraMatrices();
        setLightDirection();
        break;
    }
    case GLFW_KEY_LEFT_SUPER: // (CMD) change light direction
    {
        double x, y;
        glfwGetCursorPos(window, &x, &y);

        if (action == GLFW_PRESS) {
            movingLight = true;
            prev_x_l = x;
            prev_y_l = y;
        } else if (action == GLFW_RELEASE) {
            movingLight = false;
        }
        break;
    }
    case GLFW_KEY_F: // focus object
        if (action == GLFW_PRESS) {
            centerObject();
            xRot = 0;
            yRot = 0;
            updateCameraMatrices();
            setLightDirection();
        }
        break;
    default:
        break;
    }
}

static void cursor_callback(GLFWwindow* window, double xpos, double ypos) {
    // std::cout << xpos << " " << ypos << std::endl;
    if (isLeftDragging && !movingLight) {    
        yRot += cy::Deg2Rad(ypos - prev_y) * sensitivity;
        xRot += cy::Deg2Rad(xpos - prev_x) * sensitivity;

        prev_y = ypos;
        prev_x = xpos;
        updateCameraMatrices();
    } else if (isRightDragging && !movingLight) {
        if (ypos > prev_y) {
            zoom += zoomFactor * sensitivity;
        } else if (ypos < prev_y) {
            zoom -= zoomFactor * sensitivity;
        }
        prev_y = ypos;
        updateCameraMatrices();
    } else if (movingLight && isLeftDragging) {
        yRot_l += cy::Deg2Rad(ypos - prev_y_l) * sensitivity;
        xRot_l += cy::Deg2Rad(xpos - prev_x_l) * sensitivity;

        prev_y_l = ypos;
        prev_x_l = xpos;
        prev_y = ypos;
        prev_x = xpos;

        setLightDirection();
    }

    // std::cout << (isLeftDragging ? "Dragging" : "Not dragging") << std::endl;
}

void mouse_callback(GLFWwindow* window, int button, int action, int mods) {
    double x, y;
    glfwGetCursorPos(window, &x, &y);

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        isLeftDragging = true;
        // std::cout << "Dragging" << std::endl;
        if (movingLight) {
            prev_x_l = x;
            prev_y_l = y;
        } else {
            prev_x = x;
            prev_y = y;
        }
        
        // updateCameraAngle(window);

        return;
    } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        isLeftDragging = false;
        // std::cout << "Not dragging" << std::endl;
        // std::cout << x << " " << y << std::endl;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        isRightDragging = true;
        // std::cout << "Zooming" << std::endl;

        // updateCameraAngle(window);
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        isRightDragging = false;

    }
}

textureImage loadTexImg(char const *filename) {
    textureImage texImg;

    unsigned error_diff = lodepng::decode(texImg.image, texImg.width, texImg.height, filename);
    if (error_diff) std::cout << "decoder error " << error_diff << ": " << lodepng_error_text(error_diff) << std::endl;
    
    return texImg;
}

void loadMtls(std::vector<material>& materials) {
    // load all texture maps for all materials
    auto nm = triMesh.NM();
    for (int i = 0; i < nm; i++) {
        auto mtl = triMesh.M(i);
        materials[i].ambientColor = cyVec3f(mtl.Ka);
        materials[i].diffuseColor = cyVec3f(mtl.Kd);
        materials[i].specularColor = cyVec3f(mtl.Ks);
        materials[i].specularExp = mtl.Ns;

        if (mtl.map_Kd.data != nullptr) {
            textureImage diffuse = loadTexImg(mtl.map_Kd);
            materials[i].diffuseTex.Initialize();
            materials[i].diffuseTex.SetImage(diffuse.image.data(), 4, diffuse.width, diffuse.height);
            materials[i].diffuseTex.BuildMipmaps();
            materials[i].hasDiffuseTex = true;
        }
        if (mtl.map_Ks.data != nullptr) {
            textureImage specular = loadTexImg(mtl.map_Ks);
            materials[i].specularTex.Initialize();
            materials[i].specularTex.SetImage(specular.image.data(), 4, specular.width, specular.height);
            materials[i].specularTex.BuildMipmaps();
            materials[i].hasSpecularTex = true;
        }
    }
    std::cout << "Loaded Texture Maps" << std::endl;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Please enter the name of your .obj file" << std::endl;
        return -1;
    }
    // initialize library
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    std::cout << "initialized glfw" << std::endl;
    
    // create window
    GLFWwindow *window = glfwCreateWindow(width, height, "Textures", NULL, NULL);

    if (!window) {
        glfwTerminate();
        return -1;
    }

    // set main context
    glfwMakeContextCurrent(window);

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        return -1;
    }
    std::cout << "initialized glew" << std::endl;


    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_callback);
    glfwSetMouseButtonCallback(window, mouse_callback);

    char const *filename = argv[1];

    // load mesh
    triMesh = cyTriMesh();
    bool loaded = triMesh.LoadFromFileObj(filename);
    if (loaded) std::cout << "Loaded obj" << std::endl;
    if (!loaded) return 0;

    centerObject(); // center object by its bounding box

    
    std::vector<material> materials(triMesh.NM() == 0 ? 1 : triMesh.NM());
    loadMtls(materials);
    
    if (triMesh.NM() == 0) {
        auto &m = materials[0];
        m.diffuseColor = cyVec3f(0.5,0.5,0.5);
    }
    
    auto nv = triMesh.NV();
    auto nf = triMesh.NF();

    std::vector<cyVec3f> vertices;
    std::vector<cyVec3f> normals;
    std::vector<cyVec2f> texCoords;
    for (int i = 0; i < nf; i++) {
        // vertices of each face
        vertices.push_back(triMesh.V(triMesh.F(i).v[0]));
        vertices.push_back(triMesh.V(triMesh.F(i).v[1]));
        vertices.push_back(triMesh.V(triMesh.F(i).v[2]));

        // surface normals
        if (triMesh.HasNormals()) {
            normals.push_back(triMesh.VN(triMesh.FN(i).v[0]));
            normals.push_back(triMesh.VN(triMesh.FN(i).v[1]));
            normals.push_back(triMesh.VN(triMesh.FN(i).v[2]));
        } else {
            cyVec3f nrm = triMesh.GetNormal(i, cyVec3f(0.333f,0.333f,0.334f));
            normals.push_back(nrm);
            normals.push_back(nrm);
            normals.push_back(nrm);
        }

        // texture coordinates
        if (triMesh.HasTextureVertices()) {
            cyVec2f uv0 = triMesh.VT(triMesh.FT(i).v[0]).XY();
            cyVec2f uv1 = triMesh.VT(triMesh.FT(i).v[1]).XY();
            cyVec2f uv2 = triMesh.VT(triMesh.FT(i).v[2]).XY();
            // flip uv coordinates since image starts at top left instead of bottom left
            texCoords.push_back(cyVec2f(uv0.x, 1.0f - uv0.y));
            texCoords.push_back(cyVec2f(uv1.x, 1.0f - uv1.y));
            texCoords.push_back(cyVec2f(uv2.x, 1.0f - uv2.y));
        }
    }
    
    nv = vertices.size();
    
    shaderProgram.BuildFiles("shader.vert", "shader.frag");

    GLuint vao {};
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    
    GLuint vbo;
    glGenBuffers(1, &vbo);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(cyVec3f), vertices.data(), GL_STATIC_DRAW);

    GLuint pos = glGetAttribLocation(shaderProgram.GetID(), "pos");
    glEnableVertexAttribArray(pos);
    glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, sizeof(cyVec3f), (GLvoid*)0);

    // normal buffer
    GLuint nbo;
    glGenBuffers(1, &nbo);

    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, normals.size()*sizeof(cyVec3f), normals.data(), GL_STATIC_DRAW);

    GLuint nrm = glGetAttribLocation(shaderProgram.GetID(), "aNrm");
    glEnableVertexAttribArray(nrm);
    glVertexAttribPointer(nrm, 3, GL_FLOAT, GL_FALSE, sizeof(cyVec3f), (GLvoid*)0);


    // vertex buffer for texture coords
    GLuint tbo;
    glGenBuffers(1, &tbo);

    glBindBuffer(GL_ARRAY_BUFFER, tbo);
    glBufferData(GL_ARRAY_BUFFER, texCoords.size()*sizeof(cyVec2f), texCoords.data(), GL_STATIC_DRAW);

    GLuint txc = glGetAttribLocation(shaderProgram.GetID(), "txc");
    glEnableVertexAttribArray(txc);
    glVertexAttribPointer(txc, 2, GL_FLOAT, GL_FALSE, sizeof(cyVec2f), (GLvoid*)0);

    updateCameraMatrices();
    setLightDirection();


    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // loop until window closed
    while (!glfwWindowShouldClose(window)) {
        glViewport(0, 0, width, height);
        display(window, vao, nv, materials);

        // poll and process events
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
