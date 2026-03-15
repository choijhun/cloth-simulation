
#include <glad/glad.h>

#include <GLFW/glfw3.h>
#include<glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>  // lookAt, perspective, radians
#include <glm/gtc/type_ptr.hpp>          // value_ptr (uniform 넘길 때)

#include <iostream>
#include <vector>
#include <fstream>
#include<sstream>
#include<string>
#include <filesystem>

#include "shader.h"
const float PI = 3.141592;
const float k_air = -0.1;
const int pnum = 100;
float fov = 50.f;
double lastX, lastY;
float phi, theta = 0;
//func

using namespace std;
using namespace glm;

GLuint VAO = 0;
GLuint VBO = 0;
GLuint IBO = 0;
GLuint NBO = 0;
GLuint Program = 0;



struct Particle {
    vec3 pos;
    vec3 vel;
    vec3 force;
    float m;
    bool fixed = false;

    Particle(vec3 x,vec3 v=vec3(0), float mass = 0.01)
    :pos(x), vel(v),force(0), m(mass){}
    void clearForce() { force = vec3(0); }
    void addForce(vec3 F) {
        force += F;
    }
    void update(float dt) {
        if (!fixed) {
            vel += dt * force / m;
            pos += dt * vel;
        }
        else vel = vec3(0);
                
    }  
    void resolveCollision() {

    }
      

};
struct Spring {
    Particle& a, & b;
    float r;
    float ks = 10000;
    float kd = 1;
    Spring(Particle& x, Particle& y) :
        a(x), b(y), r(length(y.pos - x.pos)){}

    void addForce() {
        vec3 dx = b.pos - a.pos;
        vec3 ndx = normalize(dx);
        vec3 f = -(ks * (length(dx) - r) + kd * dot((b.vel - a.vel), ndx)) * ndx;
        a.addForce(-f);
        b.addForce(f);
    }

    
};
vector< Particle > particles;
vector<unsigned int> indices;
vector<Spring> springs;
vector<vec3> normals;

void init();
void render(GLFWwindow* window);

void mouseButtonCB(GLFWwindow* window, int button, int action, int mods) {
    glfwGetCursorPos(window, &lastX, &lastY);
}
void cursorPosCB(GLFWwindow* window, double xpos, double ypos) {
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS) {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        theta = theta - PI * (ypos - lastY) / height;
        phi = phi - PI * (xpos - lastX) / width;
        lastX = xpos;
        lastY = ypos;
    }
}
void scrollCB(GLFWwindow* window, double xoffset, double yoffset) {
    fov = fov * pow(1.1, yoffset);
}
void keyCB(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_1) {
        particles[(pnum-1) * pnum + 0].fixed = !particles[(pnum-1) * pnum + 0].fixed;
    }
    if (key == GLFW_KEY_2) {
        particles[(pnum - 1) * pnum + (pnum - 1)].fixed = !particles[(pnum - 1) * pnum + pnum - 1].fixed;
    }
    if (key == GLFW_KEY_3) {
        particles[(pnum - 1) * pnum + (pnum - 1)/2].fixed = !particles[(pnum - 1) * pnum + (pnum - 1)/2].fixed;
    }
}




int main() {
    
    if (!glfwInit()) {
        std::cerr << "GLFW init failed";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(800, 600, "Simulation", nullptr, nullptr);
       
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "GLAD init failed" << endl;
    }
    glfwSetMouseButtonCallback(window, mouseButtonCB);
    glfwSetCursorPosCallback(window, cursorPosCB);
    glfwSetScrollCallback(window, scrollCB);
    glfwSetKeyCallback(window, keyCB);
    glfwSwapInterval(1);
    init();
    
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Particle) * particles.size(), particles.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), 0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), indices.data(), GL_STATIC_DRAW);
    
    glGenBuffers(1, &NBO);
    glBindBuffer(GL_ARRAY_BUFFER, NBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * normals.size(), normals.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);


    
    

    // 메인 루프
    
    while (!glfwWindowShouldClose(window)) {
        render(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
float randf() {
    return rand() / float(RAND_MAX);
}
void init()
{
    Program= loadShaders("shader.vert", "shader.frag");
    for (int i = 0;i < pnum;i++) {
        for (int j = 0;j < pnum;j++) {
            particles.push_back(Particle(vec3(-4 + j * 0.08f, -4 + i * 0.08f, randf() * 0.01f)));
        }
    }

    

    for (int i = 0; i < pnum-1;i++) for (int j = 0;j < pnum-1;j++) {
        indices.push_back(i * pnum + j);
        indices.push_back(i * pnum + j + 1);
        indices.push_back((i + 1) * pnum + j + 1);

        indices.push_back(i * pnum + j);
        indices.push_back((i + 1) * pnum + j + 1);
        indices.push_back((i + 1) * pnum + j);

    } 
    normals.resize(particles.size(), vec3(0));

   

    for (int i = 0; i < pnum;i++) for (int j = 0;j < pnum-1;j++) {
        springs.emplace_back(particles[i * pnum + j], particles[i * pnum + j + 1]);
    }
    for (int i = 0; i < pnum-1;i++) for (int j = 0;j < pnum;j++) {
        springs.emplace_back(particles[i * pnum + j], particles[i * pnum + pnum + j]);
    }
    for (int i = 0; i < pnum - 1; i++) for (int j = 0;j < pnum - 1;j++) {
        springs.emplace_back(particles[i * pnum + j], particles[(i + 1) * pnum + j + 1]);
        springs.emplace_back(particles[i * pnum + j + 1], particles[(i + 1) * pnum + j]);
    }
    for (int i = 0; i < pnum-2; i++)
        for (int j = 0; j < pnum; j++)
            springs.emplace_back(particles[i * pnum + j], particles[(i + 2) * pnum + j]);
    for (int i = 0; i < pnum; i++)
        for (int j = 0; j < pnum-2; j++)
            springs.emplace_back(particles[i * pnum + j], particles[i * pnum + j + 2]);
    
    particles[pnum*(pnum-1)].fixed = true;
    particles[pnum* pnum-1].fixed = true;
    particles[(pnum - 1) * pnum + (pnum - 1) / 2].fixed = true;

}
const int substep = 20;
void render(GLFWwindow* window)
{
    
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float cameraDist = 10.f;
    vec3 cameraPos = vec3 (0, 0, cameraDist);
    vec3 lightPos = vec3(3, 3, 3);
    float lightIntensity;
    cameraPos = rotate(mat4(1), phi, vec3(0, 1, 0)) * rotate(mat4(1), theta, vec3(1, 0, 0)) * vec4(cameraPos, 1.0f);
    
    mat4 modelMat{ mat4(1) };
    mat4 viewMat = lookAt(cameraPos, vec3(0), vec3(0, 1, 0));
    mat4 projMat = perspective(radians(fov), width / float (height), 0.1f, 100.f);

    glUseProgram(Program);
    glUniformMatrix4fv (glGetUniformLocation(Program, "modelMat"),1,false, value_ptr(modelMat));
    glUniformMatrix4fv (glGetUniformLocation(Program, "viewMat"), 1, false, value_ptr(viewMat));
    glUniformMatrix4fv (glGetUniformLocation(Program, "projMat"),1,false,value_ptr(projMat));

    glUniform3fv(glGetUniformLocation(Program, "lightPos"),1,value_ptr(lightPos));
    glUniform3fv(glGetUniformLocation(Program, "cameraPos"), 1, value_ptr(cameraPos));
    glViewport(0, 0, width, height);
    for (int i = 0; i < substep; i++) {
        for (auto& p : particles) p.clearForce();
        for (auto& p : particles) {
            p.addForce(vec3(0, -980, 0) * p.m);
            p.addForce(p.vel * k_air);
        }
        for (auto& s : springs) s.addForce();
        for (auto& p : particles) p.update(0.0016f/substep);

        for (int i = 0; i < indices.size(); i = i + 3) {
            int i0 = indices[i];
            int i1 = indices[i + 1];
            int i2 = indices[i + 2];
            vec3 p0 = particles[i0].pos;
            vec3 p1 = particles[i1].pos;
            vec3 p2 = particles[i2].pos;
            vec3 N = cross((p1 - p0), (p2 - p0));

            normals[i0] += N;
            normals[i1] += N;
            normals[i2] += N;
        }

        for (auto& N : normals) {
            N = normalize(N);
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Particle) * particles.size(), particles.data());

    glBindBuffer(GL_ARRAY_BUFFER, NBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec3) * normals.size(), normals.data());   

    glClearColor(0.3, 0.3, 0.3, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(Program);
    
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glfwSwapBuffers(window);



}