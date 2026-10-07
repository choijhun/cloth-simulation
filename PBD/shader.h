#pragma once
#include <glad/glad.h>
GLuint loadShaders(const char* vspath, const char* fspath);
GLuint loadComputeShader(const char* fpath);
void checkCompileError(GLuint shader);