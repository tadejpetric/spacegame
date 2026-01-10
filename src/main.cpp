#include <SDL.h>
#include <SDL_opengles2.h>
#include <emscripten.h>
#include <cmath>
#include <vector>
#include <iostream>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

// Global variables
SDL_Window* window;
SDL_GLContext context;
GLuint program;
GLuint vbo;

const char* vertex_shader_source = 
    "attribute vec2 position;\n"
    "void main() {\n"
    "   gl_Position = vec4(position, 0.0, 1.0);\n"
    "}\n";

const char* fragment_shader_source = 
    "precision mediump float;\n"
    "uniform vec4 color;\n"
    "void main() {\n"
    "   gl_FragColor = color;\n"
    "}\n";

GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

// This function runs every frame (simulating a game loop)
void main_loop() {
    // Poll events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT) {
            emscripten_cancel_main_loop();
            return;
        }
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Show a simple window
    {
        static float f = 0.0f;
        static int counter = 0;
        ImGui::Begin("Hello, world!");
        ImGui::Text("This is some useful text.");
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
        if (ImGui::Button("Button"))
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }

    // Rendering
    ImGui::Render();

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program);
    
    GLint posAttrib = glGetAttribLocation(program, "position");
    glEnableVertexAttribArray(posAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

    GLint colorUniform = glGetUniformLocation(program, "color");

    // Draw Triangle
    glUniform4f(colorUniform, 1.0f, 0.0f, 0.0f, 1.0f); // Red
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Draw Circle (using the vertices starting at index 3)
    glUniform4f(colorUniform, 0.0f, 0.0f, 1.0f, 1.0f); // Blue
    glDrawArrays(GL_TRIANGLE_FAN, 3, 32);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);
}

int main() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        return -1;
    }

    // Create the browser window (Canvas)
    // We request an OpenGL ES 2.0 context (compatible with WebGL 1)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    window = SDL_CreateWindow("Wasm Demo", 
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                              800, 600, 
                              SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    context = SDL_GL_CreateContext(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, context);
    ImGui_ImplOpenGL3_Init("#version 100");

    // Compile Shaders
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // Prepare Geometry
    std::vector<float> vertices;
    
    // Triangle (top-leftish)
    vertices.push_back(-0.5f); vertices.push_back(0.5f);
    vertices.push_back(-0.8f); vertices.push_back(-0.5f);
    vertices.push_back(-0.2f); vertices.push_back(-0.5f);

    // Circle (bottom-rightish)
    float centerX = 0.5f;
    float centerY = -0.2f;
    float radius = 0.3f;
    int segments = 30;
    vertices.push_back(centerX); vertices.push_back(centerY); // Center
    for(int i = 0; i <= segments; i++) {
        float angle = 2.0f * M_PI * float(i) / float(segments);
        vertices.push_back(centerX + cos(angle) * radius);
        vertices.push_back(centerY + sin(angle) * radius);
    }

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Tell Emscripten to use our main_loop function
    // 0 = 0 fps (use browser's requestAnimationFrame), 1 = simulate infinite loop
    emscripten_set_main_loop(main_loop, 0, 1);

    return 0;
}