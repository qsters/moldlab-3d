#include "GameEngine.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>

GameEngine::GameEngine(const int width, const int height, std::string  title, const bool vSync)
    : window(nullptr), width(width), height(height), title(std::move(title)), lastFrameTime(0.0f), deltaTime(0.0f), timeSinceStart(0.0f), vSyncEnabled(vSync) {

    init();
}

void GameEngine::init() {
    initGLFW();
    initImGui();
    ComputeShaderInitializationAndCheck();
}

void GameEngine::initImGui() const {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Setup platform/renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430"); // Match your OpenGL version
}


GameEngine::~GameEngine() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();


    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

void GameEngine::printFramerate(float& frameTimeAccumulator, int& frameCount) const {
    if (displayFramerate && frameTimeAccumulator >= 1.0f) {
        const float averageFrameRate = static_cast<float>(frameCount) / frameTimeAccumulator;
        std::cout << "Average Frame Rate: " << averageFrameRate << " FPS" << std::endl;

        frameCount = 0;
        frameTimeAccumulator = 0.0f;
    }
}


void GameEngine::run() {
    renderingStart();
    start();

    int frameCount = 0;
    float frameTimeAccumulator = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        // Calculate delta time
        const float currentTime = static_cast<float>(glfwGetTime());
        deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        timeSinceStart += deltaTime;
        frameCount++;
        frameTimeAccumulator += deltaTime;
        printFramerate(frameTimeAccumulator, frameCount);

        inputManager.handleInput(window); // Process inputs
        update(deltaTime);

        glClear(GL_COLOR_BUFFER_BIT); // Clear the screen buffer

        render();

        // Render UI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderUI();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

std::pair<int, int> GameEngine::getScreenSize() const {
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    return {width, height};
}

int GameEngine::getScreenHeight() const {
    auto [width, height] = getScreenSize();
    return height;
}

int GameEngine::getScreenWidth() const {
    auto [width, height] = getScreenSize();
    return width;
}

void GameEngine::SetVsyncStatus(const bool status) {
    vSyncEnabled = status;
    glfwSwapInterval(static_cast<int>(vSyncEnabled));
}

bool GameEngine::GetVsyncStatus() const {
    return vSyncEnabled;
}

std::string GameEngine::LoadShaderSource(const std::string &filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open shader file: " << filepath << std::endl;
        exit(EXIT_FAILURE);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::pair<std::string, std::string> GameEngine::LoadCombinedShaderSource(const std::string &filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open shader file: " << filepath << std::endl;
        exit(EXIT_FAILURE);
    }

    std::stringstream vertex_shader_stream;
    std::stringstream fragment_shader_stream;

    std::string line;
    std::stringstream* current_stream = nullptr;

    while (std::getline(file, line)) {
        if (line.find("#type vertex") != std::string::npos) {
            current_stream = &vertex_shader_stream;
        } else if (line.find("#type fragment") != std::string::npos) {
            current_stream = &fragment_shader_stream;
        } else if (current_stream) {
            *current_stream << line << '\n';
        }
    }

    return { vertex_shader_stream.str(), fragment_shader_stream.str() };
}

std::string ReplaceDefinitionWithText(const std::string& definition, const std::string& replacementText, const std::string& source) {
    // Preprocess the replacement text to exclude specific lines
    std::string processedText;
    std::string line;
    std::istringstream input(replacementText);
    std::ostringstream output;

    while (std::getline(input, line)) {
        if (line.find("#DEFINE_REMOVE_FROM_SHADER") == std::string::npos) {
            output << line << '\n';
        }
    }
    processedText = output.str();

    // Copy the original source
    std::string processedSource = source;

    // Replace the definition in the source
    size_t position = processedSource.find(definition);
    if (position != std::string::npos) {
        processedSource.replace(position, definition.length(), processedText);
    }

    return processedSource;
}


std::string ReplaceDefinitionWithFile(const std::string& definition, const std::string& filePath, const std::string& source) {
    // Read the file contents
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open shader definition file: " << filePath << std::endl;
        exit(EXIT_FAILURE);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string fileContents = buffer.str();

    // Delegate to ReplaceDefinitionWithText
    return ReplaceDefinitionWithText(definition, fileContents, source);
}



GLuint GameEngine::CompileShader(const std::string& source, GLenum shader_type) {
    GLuint shader = glCreateShader(shader_type);

    // Start with the original source
    std::string processedSource = source;

    // Replace each placeholder with its corresponding definition
    for (const auto& [placeholder, filePath] : shaderDefinitions) {
        if (filePath == "") {
            processedSource = ReplaceDefinitionWithText(placeholder, "", processedSource);
        } else {
            processedSource = ReplaceDefinitionWithFile(placeholder, filePath, processedSource);
        }
    }

    // Convert processed source to C-string
    const char* source_cstr = processedSource.c_str();

    // Compile the shader
    glShaderSource(shader, 1, &source_cstr, nullptr);
    glCompileShader(shader);

    // Check for compilation errors
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << processedSource << " -- Shader Compilation Error: " << infoLog << std::endl;
        exit(EXIT_FAILURE);
    }

    return shader;
}

void GameEngine::addShaderDefinition(const std::string &placeholder, const std::string &filePath) {
    shaderDefinitions[placeholder] = filePath;
}

void GameEngine::removeShaderDefinition(const std::string &placeholder) {
    // Check if the placeholder exists in the map
    auto item = shaderDefinitions.find(placeholder);
    if (item != shaderDefinitions.end()) {
        shaderDefinitions.erase(item); // Remove the placeholder and its associated file path
    } else {
        std::cerr << "Warning: Attempt to remove non-existent shader definition: " << placeholder << std::endl;
    }
}

GLuint GameEngine::CompileAndAttachShader(const std::string& source, const GLenum shaderType, const GLuint program) {
    const GLuint shader = CompileShader(source, shaderType);
    glAttachShader(program, shader);
    return shader;
}


GLuint GameEngine::CreateShaderProgram(const std::vector<std::tuple<std::string, GLenum, bool>>& shaders) {
    // Create a new program
    const GLuint program = glCreateProgram();

    // Keep track of all shaders to clean up later
    std::vector<GLuint> shaderObjects;

    for (const auto& [filePath, shaderType, isCombined] : shaders) {
        if (isCombined) {
            // Load combined shader source and compile both vertex and fragment shaders

            if (shaderType != GL_VERTEX_SHADER && shaderType != GL_FRAGMENT_SHADER) {
                std::cerr << "Error: Combined shaders must use GL_VERTEX_SHADER or GL_FRAGMENT_SHADER as the shader type." << std::endl;
                continue;
            }
            auto [vertexShaderSource, fragmentShaderSource] = LoadCombinedShaderSource(filePath);

            shaderObjects.push_back(CompileAndAttachShader(vertexShaderSource, GL_VERTEX_SHADER, program));
            shaderObjects.push_back(CompileAndAttachShader(fragmentShaderSource, GL_FRAGMENT_SHADER, program));
        } else {
            // Load and compile a single shader
            std::string source = LoadShaderSource(filePath);
            shaderObjects.push_back(CompileAndAttachShader(source, shaderType, program));
        }
    }

    // Link the program
    glLinkProgram(program);
    CheckProgramLinking(program);

    // Detach and delete the shaders after linking
    for (const GLuint shader : shaderObjects) {
        glDetachShader(program, shader);
        glDeleteShader(shader);
    }

    return program;
}




void GameEngine::CheckProgramLinking(const GLuint program) {
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
}


void GameEngine::initGLFW() {
    glfwSetErrorCallback(errorCallback);

#if defined(__linux__) && defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_X11)
    // Force X11 on Linux to avoid Wayland/EGL loader mismatches.
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    // Set the user pointer to this instance
    glfwSetWindowUserPointer(window, this);

    glfwSetKeyCallback(window, keyCallback);

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize OpenGL loader (glad)." << std::endl;
        exit(EXIT_FAILURE);
    }
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    SetVsyncStatus(vSyncEnabled);
}

void GameEngine::errorCallback(int error, const char* description) {
    std::cerr << "Error: " << description << std::endl;
}

void GameEngine::keyCallback(GLFWwindow *window, const int key, int scancode, const int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE)
        glfwSetWindowShouldClose(window, GLFW_TRUE); // Close window on Escape key
}

void GameEngine::framebufferSizeCallback(GLFWwindow* window, const int width, const int height) {
    glViewport(0, 0, width, height);
}

// Helper function for catching errors.
void GameEngine::CheckGLError(const std::string& context) {
    GLenum error = glGetError();
    while (error != GL_NO_ERROR) {
        std::string errorString;
        switch (error) {
            case GL_INVALID_ENUM: errorString = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE: errorString = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorString = "GL_INVALID_OPERATION"; break;
            case GL_STACK_OVERFLOW: errorString = "GL_STACK_OVERFLOW"; break;
            case GL_STACK_UNDERFLOW: errorString = "GL_STACK_UNDERFLOW"; break;
            case GL_OUT_OF_MEMORY: errorString = "GL_OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: errorString = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
            default: errorString = "UNKNOWN_ERROR"; break;
        }
        std::cerr << "OpenGL Error in " << context << ": " << errorString << " (" << error << ")" << std::endl;
        error = glGetError(); // Check for more errors
    }
}

float GameEngine::DeltaTime() const {
    return deltaTime;
}

float GameEngine::TimeSinceStart() const {
    return timeSinceStart;
}

void GameEngine::ComputeShaderInitializationAndCheck() {
    const GLubyte* version = glGetString(GL_VERSION);
    if (!version) {
        std::cerr << "Failed to query OpenGL version (no current context or driver error)." << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "OpenGL Version: " << version << std::endl;

    if (GLVersion.major >= 4 && GLVersion.minor >= 3) {
        std::cout << "Compute Shaders Supported!" << std::endl;

        // Check the maximum compute work group counts
        GLint workGroupCount[3];
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &workGroupCount[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &workGroupCount[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &workGroupCount[2]);

        std::cout << "Max Compute Work Group Count: "
                  << workGroupCount[0] << ", "
                  << workGroupCount[1] << ", "
                  << workGroupCount[2] << std::endl;

        maxWorkGroupCountX = workGroupCount[0];
        maxWorkGroupCountY = workGroupCount[1];
        maxWorkGroupCountZ = workGroupCount[2];

        // Check the maximum work group size
        GLint workGroupSize[3];
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &workGroupSize[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &workGroupSize[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &workGroupSize[2]);

        std::cout << "Max Compute Work Group Size: "
                  << workGroupSize[0] << ", "
                  << workGroupSize[1] << ", "
                  << workGroupSize[2] << std::endl;

        maxWorkGroupSizeX = workGroupSize[0];
        maxWorkGroupSizeY = workGroupSize[1];
        maxWorkGroupSizeZ = workGroupSize[2];
    } else {
        throw std::runtime_error("Compute Shaders Not Supported! Program cannot continue.");
    }
}

int GameEngine::getMaxWorkGroupCountX() const {
    return maxWorkGroupCountX;
}

int GameEngine::getMaxWorkGroupCountY() const {
    return maxWorkGroupCountY;
}

int GameEngine::getMaxWorkGroupCountZ() const {
    return maxWorkGroupCountZ;
}

int GameEngine::getMaxWorkGroupSizeX() const {
    return maxWorkGroupSizeX;
}

int GameEngine::getMaxWorkGroupSizeY() const {
    return maxWorkGroupSizeY;
}

int GameEngine::getMaxWorkGroupSizeZ() const {
    return maxWorkGroupSizeZ;
}

void GameEngine::DispatchComputeShader(const GLuint computeShaderProgram,
                                       const int itemsX, const int itemsY, const int itemsZ) const {
    if (itemsX < 1 || itemsY < 1 || itemsZ < 1) {
        throw std::runtime_error("Dispatch item must be above 0");
    }

    if (computeShaderProgram == 0) {
        throw std::runtime_error("Shader Program not initialized");
    }

    // Bind the compute shader program
    glUseProgram(computeShaderProgram);

    // Get the local work group sizes
    GLint localSize[3];
    glGetProgramiv(computeShaderProgram, GL_COMPUTE_WORK_GROUP_SIZE, localSize);

    const int localSizeX = localSize[0];
    const int localSizeY = localSize[1];
    const int localSizeZ = localSize[2];

    // Calculate the number of work groups required for each dimension
    const int workGroupCountX = (itemsX + localSizeX - 1) / localSizeX; // ceil(itemsX / localSizeX)
    const int workGroupCountY = (itemsY + localSizeY - 1) / localSizeY;
    const int workGroupCountZ = (itemsZ + localSizeZ - 1) / localSizeZ;

    // Validate against maximum work group count bounds
    if (workGroupCountX > maxWorkGroupCountX ||
        workGroupCountY > maxWorkGroupCountY ||
        workGroupCountZ > maxWorkGroupCountZ) {
        throw std::runtime_error("Dispatch exceeds maximum work group counts.");
        }

    // Validate against maximum local work group size bounds
    if (localSizeX > maxWorkGroupSizeX ||
        localSizeY > maxWorkGroupSizeY ||
        localSizeZ > maxWorkGroupSizeZ) {
        throw std::runtime_error("Compute shader local size exceeds maximum limits.");
        }

    // Dispatch the compute shader
    glDispatchCompute(workGroupCountX, workGroupCountY, workGroupCountZ);

    // Check for errors
    GLenum err;
    // ReSharper disable once CppDFALoopConditionNotUpdated
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL Error: " << err << std::endl;
        throw std::runtime_error("Error occurred during compute shader dispatch.");
    }

    // Ensure the compute shader completes before continuing
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}
