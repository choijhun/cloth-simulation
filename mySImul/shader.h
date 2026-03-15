#pragma once
#include <glad/glad.h>
GLuint loadShaders(const char* vspath, const char* fspath);
void checkCompileError(GLuint shader);