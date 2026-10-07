
        #include <glad/glad.h>

        #include <GLFW/glfw3.h>
        #include<glm/glm.hpp>
        #include <glm/gtc/matrix_transform.hpp>  // lookAt, perspective, radians
        #include <glm/gtc/type_ptr.hpp>          // value_ptr (uniform 넘길 때)
        #include <algorithm>
        #include <iostream>
        #include <vector>
        #include <fstream>
        #include<sstream>
        #include<string>
        #include <filesystem>
        #include <algorithm>
        #include <random>
        #include <unordered_map>
        #include<set>
        
        #include<numeric>


        #include "shader.h"
        const float PI = 3.141592;
        const int pnum = 40;
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
        GLuint particleSSBO = 0;
        GLuint floorVAO = 0;
        GLuint floorVBO = 0;
        // ===== [GPU-PREDICT] =====
        GLuint PredictProgram = 0;
        // =========================
        // ===== [GPU-CONSTRAINT] =====
        GLuint DConstraintSSBO = 0;
        GLuint SolveDConstraintProgram = 0;
        vector<int> colorGroupStart;   // 색 그룹별 SSBO 시작 인덱스
        vector<int> colorGroupCount;   // 색 그룹별 constraint 수
        
        struct DistConstraint {
            int idx1, idx2;
            float restlength;
            DistConstraint(int prt1, int prt2, float length) :
                idx1(prt1), idx2(prt2), restlength(length){ }
        };
        struct SelfCollideConstraint {
            int q;
            int p1, p2, p3;
            vec3 N;
            float h;
            SelfCollideConstraint(int _q, int _p1, int _p2, int _p3, vec3 _N, float thickness = 0.5) :
                q(_q), p1(_p1), p2(_p2), p3(_p3), N(_N), h(thickness) { }

            
        };

        struct Particle {
            vec3 pos;
            vec3 vel;
            vec3 newP;
   
            float m;
            bool fixed = false;

            Particle(vec3 x, vec3 v = vec3(0), float mass = 0.1)
                :pos(x), vel(v), m(mass) {
            }
        };
        struct spartialHash {
            float cellsize;
            unordered_map<int64_t, vector<int>> cells;
            
            int64_t hashKey(int x, int y, int z) {
                return (int64_t(x) * 73856093) ^ (int64_t(y) * 19349663) ^ (int64_t(z) * 83492791);
            }
            ivec3 toCells(vec3 pos) {
                return ivec3(floor(pos.x / float(cellsize)), floor(pos.y / float(cellsize)), floor(pos.z / float(cellsize)));
            }
            void insertTriangles(int triNum, vec3 p0, vec3 p1, vec3 p2) {
                vec3 mx= max(max(p0, p1), p2);
                vec3 mn = min(min(p0, p1), p2);
                ivec3  high = toCells(mx);
                ivec3 low = toCells(mn);
                for (int i=low.x;i<=high.x; i++)
                for (int j = low.y;j <= high.y; j++)
                for (int k = low.z;k <= high.z; k++) 
                    cells[hashKey(i, j, k)].push_back(triNum);               
            }
            void query(vec3 p, vector<int> & candidates) {
               
                ivec3 c = toCells(p);
                for (int i = c.x - 1; i <= c.x + 1; i++)
                    for (int j = c.y - 1; j <= c.y + 1; j++)
                        for (int k = c.z - 1; k <= c.z + 1; k++) {
                            auto it = cells.find(hashKey(i, j, k));
                            if (it != cells.end())
                                candidates.insert(candidates.end(), it->second.begin(), it->second.end());
                        }
      
            }
            void clear() {
                cells.clear();
            }

        };

        vector< Particle > particles;
        vector<unsigned int> indices;

        vector<vec3> normals;

        vector<DistConstraint> Dconst;
        vector<SelfCollideConstraint> SCconst;

        // ===== [MULTICOLOR-GS] =====
        vector<vector<int>> colorGroups;
        mt19937 rng{ random_device{}() };
   

        vector <vec3> SCcorrections;
        vector <int> SCcorrectnums;


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
                particles[(pnum - 1) * pnum + 0].fixed = !particles[(pnum - 1) * pnum + 0].fixed;
            }
            if (key == GLFW_KEY_2) {
                particles[(pnum - 1) * pnum + (pnum - 1)].fixed = !particles[(pnum - 1) * pnum + pnum - 1].fixed;
            }
            if (key == GLFW_KEY_3) {
                particles[(pnum - 1) * pnum + (pnum - 1) / 2].fixed = !particles[(pnum - 1) * pnum + (pnum - 1) / 2].fixed;
            }
        }

        int main() {

            if (!glfwInit()) {
                std::cerr << "GLFW init failed";
                return -1;
            }
    
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
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
            glEnable(GL_DEPTH_TEST);
            vector<vec3> ground = { vec3(-1000,-70,1000), vec3(0,1,0),vec3(1000,-70,1000),vec3(0,1,0),
                                    vec3(1000,-70,-1000),vec3(0,1,0),vec3(1000,-70,-1000), vec3(0,1,0),
                                    vec3(-1000,-70,-1000),vec3(0,1,0),vec3(-1000,-70,1000), vec3(0,1,0) };


            glGenVertexArrays(1, &floorVAO);
            glBindVertexArray(floorVAO);
            glGenBuffers(1, &floorVBO);
            glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
            glBufferData(GL_ARRAY_BUFFER, ground.size() * sizeof(vec3), ground.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);

            glBindVertexArray(0);

            glGenVertexArrays(1, &VAO);
            glBindVertexArray(VAO);
            glGenBuffers(1, &VBO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(Particle) * particles.size(), particles.data(), GL_DYNAMIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), 0);
            glEnableVertexAttribArray(0);
   
            glGenBuffers(1, &IBO);
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
            Program = loadShaders("shader.vert", "shader.frag");
            for (int i = 0;i < pnum;i++) {
                for (int j = 0;j < pnum;j++) {
                    particles.push_back(Particle(vec3(-40 + j * 80/float(pnum), -40 + i * 80 / float(pnum), randf() * 0.1f)));
                }
            }
            for (int i = 0; i < pnum - 1;i++) for (int j = 0;j < pnum - 1;j++) {
                indices.push_back(i * pnum + j);
                indices.push_back(i * pnum + j + 1);
                indices.push_back((i + 1) * pnum + j + 1);

                indices.push_back(i * pnum + j);
                indices.push_back((i + 1) * pnum + j + 1);
                indices.push_back((i + 1) * pnum + j);

            }
            //gpu
            vector<vec4> gpuData(particles.size() * 3);
            for (int i = 0; i < particles.size();i++) {
                gpuData[i * 3] = vec4(particles[i].pos, particles[i].m);
                gpuData[i * 3+1] = vec4(particles[i].vel, particles[i].fixed);
                gpuData[i * 3 + 2] = vec4(particles[i].newP, 0.0f);
            }
            
            glGenBuffers(1, &particleSSBO);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
            glBufferData(GL_SHADER_STORAGE_BUFFER, gpuData.size() * sizeof(vec4), gpuData.data(),GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);
            // ===== [GPU-PREDICT] =====
            PredictProgram = loadComputeShader("predict.comp");
            // =========================

            normals.resize(particles.size(), vec3(0));

            for (int i = 0; i < pnum;i++) for (int j = 0;j < pnum - 1;j++) {
                int a = pnum * i + j;
                int b = pnum * i + j + 1;
                Dconst.emplace_back(a, b, length(particles[a].pos - particles[b].pos));
            }
            for (int i = 0; i < pnum - 1;i++) for (int j = 0;j < pnum;j++) {
                int a = pnum * i + j;
                int b = pnum * (i + 1) + j;
                Dconst.emplace_back(a, b, length(particles[a].pos - particles[b].pos));
            }
            for (int i = 0; i < pnum - 1; i++) for (int j = 0;j < pnum - 1;j++) {
                int a = i * pnum + j;
                int b = (i + 1) * pnum + j + 1;
                Dconst.emplace_back(a, b, length(particles[b].pos - particles[a].pos));
                a = i * pnum + j + 1;
                b = (i + 1) * pnum + j;
                Dconst.emplace_back(a, b, length(particles[b].pos - particles[a].pos));
            }

            //bending force
            for (int i = 0; i < pnum - 2; i++) for (int j = 0; j < pnum; j++) {
                int a = i * pnum + j;
                int b = (i + 2) * pnum + j;
                Dconst.emplace_back(a, b, length(particles[b].pos - particles[a].pos));
            }
            for (int i = 0; i < pnum; i++) for (int j = 0; j < pnum - 2; j++) {
                int a = i * pnum + j;
                int b = i * pnum + j + 2;
                Dconst.emplace_back(a, b, length(particles[b].pos - particles[a].pos));
            }

            particles[pnum * (pnum - 1)].fixed = true;
            particles[pnum * pnum - 1].fixed = true;
            particles[(pnum - 1) * pnum + (pnum - 1) / 2].fixed = true;


            SCcorrections.resize(particles.size());
            SCcorrectnums.resize(particles.size());

            // ===== [MULTICOLOR-GS] =====
            // Greedy graph coloring: 같은 파티클을 공유하는 constraint끼리 다른 색 부여
            {
                vector<vector<int>> ptoc(particles.size()); // particle → constraint 인덱스 목록
                for (int ci = 0; ci < Dconst.size(); ci++) {
                    ptoc[Dconst[ci].idx1].push_back(ci);
                    ptoc[Dconst[ci].idx2].push_back(ci);
                }
                vector<int> constraintColor(Dconst.size(), -1);
                for (int ci = 0; ci < Dconst.size(); ci++) {
                    set<int> used;
                    for (int pi : { Dconst[ci].idx1, Dconst[ci].idx2 }) {
                        for (int other : ptoc[pi]) {
                            if (other != ci && constraintColor[other] >= 0)
                                used.insert(constraintColor[other]);
                        }
                    }
                    int c = 0;
                    while (used.count(c)) c++;
                    constraintColor[ci] = c;
                }
                int numColors = *max_element(constraintColor.begin(), constraintColor.end()) + 1;
                colorGroups.resize(numColors);
                for (int ci = 0; ci < Dconst.size(); ci++)
                    colorGroups[constraintColor[ci]].push_back(ci);
                cout << "Distance constraint colors: " << numColors << endl;
            }

            // ===== [GPU-CONSTRAINT] =====
            // colorGroups를 색 그룹 순서대로 flatten → DConstraintSSBO 생성
            {
                vector<vec4> DConstraintData; // vec4(idx1, idx2, restLength, 0)
                colorGroupStart.resize(colorGroups.size());
                colorGroupCount.resize(colorGroups.size());

                for (int col = 0; col < colorGroups.size(); col++) {
                    colorGroupStart[col] = DConstraintData.size();
                    for (int ci : colorGroups[col]) {
                        DConstraintData.push_back(vec4(
                            Dconst[ci].idx1,
                            Dconst[ci].idx2,
                            Dconst[ci].restlength,
                            0.0f
                        ));
                    }
                    colorGroupCount[col] = (int)DConstraintData.size() - colorGroupStart[col];
                }

                glGenBuffers(1, &DConstraintSSBO);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, DConstraintSSBO);
                glBufferData(GL_SHADER_STORAGE_BUFFER,
                    DConstraintData.size() * sizeof(vec4),
                    DConstraintData.data(), GL_STATIC_DRAW);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, DConstraintSSBO);
                SolveDConstraintProgram = loadComputeShader("solveDConstraint.comp");
            }
        }
        


        void render(GLFWwindow* window)
        {
            
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            float cameraDist = 100.f;
            vec3 cameraPos = vec3(0, 100, cameraDist);
            vec3 lightPos = vec3(120, 120, 120);
            vec3 objectColor = vec3(0.1, 0.1, 0.1);
            float lightIntensity;
            cameraPos = rotate(mat4(1), phi, vec3(0, 1, 0)) * rotate(mat4(1), theta, vec3(1, 0, 0)) * vec4(cameraPos, 1.0f);

            mat4 modelMat{ mat4(1) };
            mat4 viewMat = lookAt(cameraPos, vec3(0), vec3(0, 1, 0));
            mat4 projMat = perspective(radians(fov), width / float(height), 10.f, 1000.f);

            glUseProgram(Program);
            glUniformMatrix4fv(glGetUniformLocation(Program, "modelMat"), 1, false, value_ptr(modelMat));
            glUniformMatrix4fv(glGetUniformLocation(Program, "viewMat"), 1, false, value_ptr(viewMat));
            glUniformMatrix4fv(glGetUniformLocation(Program, "projMat"), 1, false, value_ptr(projMat));

            glUniform3fv(glGetUniformLocation(Program, "lightPos"), 1, value_ptr(lightPos));
            glUniform3fv(glGetUniformLocation(Program, "cameraPos"), 1, value_ptr(cameraPos));
            glUniform3fv(glGetUniformLocation(Program, "objectColor"), 1, value_ptr(objectColor));
            glViewport(0, 0, width, height);

            spartialHash hash;
            hash.cellsize = 1.f;


            int substep = 5;
            float dt = 0.016 / substep;
            for (int i = 0; i < substep;i++) {
               
                

                // ===== [GPU-PREDICT] 시작: CPU predict → GPU compute shader =====
                // 1. 현재 CPU 파티클 상태를 SSBO에 업로드
                {
                    vector<vec4> gpuData(particles.size() * 3);
                    for (int k = 0; k < (int)particles.size(); k++) {
                        gpuData[k*3+0] = vec4(particles[k].pos,  particles[k].m);
                        gpuData[k*3+1] = vec4(particles[k].vel,  particles[k].fixed ? 1.0f : 0.0f);
                        gpuData[k*3+2] = vec4(particles[k].newP, 0.0f);
                    }
                    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
                    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                        gpuData.size() * sizeof(vec4), gpuData.data());
                }
                // 2. GPU에서 predict 실행 (중력, 감쇠, newP 계산)
                glUseProgram(PredictProgram);
                glUniform1f(glGetUniformLocation(PredictProgram, "dt"), dt);
                glUniform1i(glGetUniformLocation(PredictProgram, "numParticles"), particles.size());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);
                glDispatchCompute((GLuint)((particles.size() + 255) / 256), 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                // 3. GPU 결과(vel, newP)를 CPU로 읽어와 constraint solver에 전달
                {
                    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);

                    vec4* ptr = (vec4*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                        particles.size() * 3 * sizeof(vec4), GL_MAP_READ_BIT);
                    for (int k = 0; k < (int)particles.size(); k++) {
                        particles[k].vel  = vec3(ptr[k*3+1]);
                        particles[k].newP = vec3(ptr[k*3+2]);
                    }
                    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
                }
                // ===== [GPU-PREDICT] 끝 =====
                vector<int> C;
                for (int it = 0;it <= 5;it++) {


                    std::fill(SCcorrections.begin(), SCcorrections.end(), vec3(0));
                    std::fill(SCcorrectnums.begin(), SCcorrectnums.end(), 0);

                    // ===== [GPU-CONSTRAINT] =====
                    // 색 그룹 순서 랜덤 셔플 후 그룹별 순차 dispatch
                    {
                        vector<int> colorOrder(colorGroups.size());
                        iota(colorOrder.begin(), colorOrder.end(), 0);
                        shuffle(colorOrder.begin(), colorOrder.end(), rng);

                        glUseProgram(SolveDConstraintProgram);
                        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);
                        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, DConstraintSSBO);

                        for (int col : colorOrder) {
                            glUniform1i(glGetUniformLocation(SolveDConstraintProgram, "startIdx"), colorGroupStart[col]);
                            glUniform1i(glGetUniformLocation(SolveDConstraintProgram, "count"),    colorGroupCount[col]);
                            glDispatchCompute((colorGroupCount[col] + 255) / 256, 1, 1);
                            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                        }
                    }

                   // ===== [GPU-CONSTRAINT] SC sync: GPU → CPU (newP readback) =====
                    {
                        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
                        vec4* ptr = (vec4*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                            particles.size() * 3 * sizeof(vec4), GL_MAP_READ_BIT);
                        for (int k = 0; k < particles.size(); k++)
                            particles[k].newP = vec3(ptr[k * 3 + 2]);
                        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
                    }

                    if (it % 5 == 0) {
                        hash.clear();
                        SCconst.clear();

                        for (int i = 0; i < indices.size(); i += 3)
                            hash.insertTriangles(i / 3, particles[indices[i]].newP, particles[indices[i + 1]].newP, particles[indices[i + 2]].newP);

                        for (int i = 0; i < particles.size(); i++) {
                            vec3 np = particles[i].newP;
                            vec3 p  = particles[i].pos;
                            C.clear();
                            hash.query(np, C);
                            for (int j = 0; j < C.size(); j++) {
                                int triidx = C[j];
                                int i0 = indices[triidx * 3];
                                int i1 = indices[triidx * 3 + 1];
                                int i2 = indices[triidx * 3 + 2];
                                if (i == i0 || i == i1 || i == i2) continue;
                                vec3 p0 = particles[i0].newP;
                                vec3 p1 = particles[i1].newP;
                                vec3 p2 = particles[i2].newP;

                                vec3 cr   = cross(p1 - p0, p2 - p0);
                                float area = length(cr);
                                if (area < 1e-8f) continue;
                                vec3 N = cr / area;

                                float dist = dot(N, np - p0);
                                if (abs(dist) > 0.5) continue;

                                if (dist < 0) N = -N;
                                if (dot((np - p), N) > 0) continue;

                                vec3  proj = np - dist * N;
                                vec3  v0 = p1 - p0, v1 = p2 - p0, v2 = proj - p0;
                                float d00 = dot(v0,v0), d01 = dot(v0,v1), d11 = dot(v1,v1);
                                float d20 = dot(v2,v0), d21 = dot(v2,v1);
                                float denom = d00*d11 - d01*d01;
                                float u = (d11*d20 - d01*d21) / denom;
                                float v = (d00*d21 - d01*d20) / denom;
                                if (u < 0 || v < 0 || u + v > 1) continue;

                                SCconst.emplace_back(i, i0, i1, i2, N);
                            }
                        }
                    }

                    std::fill(SCcorrections.begin(), SCcorrections.end(), vec3(0));
                    std::fill(SCcorrectnums.begin(), SCcorrectnums.end(), 0);
                    for (auto& sc : SCconst) {
                        Particle& q  = particles[sc.q];
                        Particle& p1 = particles[sc.p1];
                        Particle& p2 = particles[sc.p2];
                        Particle& p3 = particles[sc.p3];
                        vec3 cr = cross(p2.newP - p1.newP, p3.newP - p1.newP);
                        if (length(cr) < 1e-10f) continue;
                        vec3  normal = cr / length(cr);
                        float dist   = dot(normal, q.newP - p1.newP);
                        if (dist < 0) normal = -normal;

                        float Cv = dot(vec3(q.newP - p1.newP), normal) - sc.h;
                        if (Cv >= 0) continue;
                        float wq = q.fixed  ? 0.0f : 1.0f / q.m;
                        float w1 = p1.fixed ? 0.0f : 1.0f / p1.m;
                        float w2 = p2.fixed ? 0.0f : 1.0f / p2.m;
                        float w3 = p3.fixed ? 0.0f : 1.0f / p3.m;

                        vec3 Gq = normal, G1 = -normal/3.f, G2 = -normal/3.f, G3 = -normal/3.f;
                        float denom = wq + (w1+w2+w3)/9.f;
                        if (denom < 1e-8f) continue;
                        float s = -Cv / denom;

                        if (!q.fixed)  SCcorrections[sc.q]  += wq * s * Gq;
                        if (!p1.fixed) SCcorrections[sc.p1] += w1 * s * G1;
                        if (!p2.fixed) SCcorrections[sc.p2] += w2 * s * G2;
                        if (!p3.fixed) SCcorrections[sc.p3] += w3 * s * G3;
                        SCcorrectnums[sc.q]++;
                        SCcorrectnums[sc.p1]++;
                        SCcorrectnums[sc.p2]++;
                        SCcorrectnums[sc.p3]++;
                    }
                    for (int i = 0; i < particles.size(); i++) {
                        if (!particles[i].fixed && SCcorrectnums[i] > 0)
                            particles[i].newP += SCcorrections[i] / float(SCcorrectnums[i]);
                    }

                    // ===== [GPU-CONSTRAINT] SC sync: CPU → GPU (newP upload) =====
                    {
                        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
                        vec4* ptr = (vec4*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                            particles.size() * 3 * sizeof(vec4), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
                        for (int k = 0; k < particles.size(); k++)
                            ptr[k * 3 + 2] = vec4(particles[k].newP, 0.0f);
                        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
                    }


                }
                // ===== [GPU-CONSTRAINT] 최종 newP readback → vel/pos 업데이트 =====
                {
                    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
                    vec4* ptr = (vec4*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                        particles.size() * 3 * sizeof(vec4), GL_MAP_READ_BIT);
                    for (int k = 0; k < particles.size(); k++)
                        particles[k].newP = vec3(ptr[k * 3 + 2]);
                    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
                }
                for (auto& p : particles) {
                    p.vel = (p.newP - p.pos) / dt;
                    p.pos = p.newP;
                }
            }

            std::fill(normals.begin(), normals.end(), vec3(0));
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
    
      
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Particle) * particles.size(), particles.data());

            glBindBuffer(GL_ARRAY_BUFFER, NBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec3) * normals.size(), normals.data());

            glClearColor(0,0,0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(Program);
            glUniform3f(glGetUniformLocation(Program, "objectColor"), 0.2f, 0.2f, 0.8f);

            glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
            glUniform3f(glGetUniformLocation(Program, "objectColor"), 0.3f, 0.3f, 0.3f);
        
            glBindVertexArray(floorVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
                
            glfwSwapBuffers(window);



        }
