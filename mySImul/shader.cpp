#include "shader.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include<string>

using namespace std;
GLuint loadShaders(const char* vspath, const char* fspath)
{
    cout << "loadshader ½ÃÀÛ" << endl;
    GLuint vertexshaderID = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragshaderID = glCreateShader(GL_FRAGMENT_SHADER);

    ifstream vfile(vspath);
    stringstream vss;
    vss << vfile.rdbuf();
    string vertexCode = vss.str();
    const char* vertSrc = vertexCode.c_str();

    ifstream ffile(fspath);
    stringstream fss;
    fss << ffile.rdbuf();
    string fragCode = fss.str();
    const char* fragSrc = fragCode.c_str();

    glShaderSource(vertexshaderID, 1, &vertSrc, nullptr);
    glShaderSource(fragshaderID, 1, &fragSrc, nullptr);

    glCompileShader(vertexshaderID);
    checkCompileError(vertexshaderID);
    glCompileShader(fragshaderID);
    checkCompileError(fragshaderID);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexshaderID);
    glAttachShader(program, fragshaderID);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        cerr << "Shader link error: " << log << endl;
    }
    glDeleteShader(vertexshaderID);
    glDeleteShader(fragshaderID);

    return program;

    
}

void checkCompileError(GLuint shader) {
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        cerr << "compile error: " << log << endl;
    }
}