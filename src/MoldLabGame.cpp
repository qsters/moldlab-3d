#include <iostream>
#include <linmath.h>
#include <cmath>
#include "MoldLabGame.h"
#include "MeshData.h"
#include "imgui.h"

const std::string USE_TRANSPARENCY_DEFINITION = "#define USE_TRANSPARENCY";
const std::string SIMULATION_SETTINGS_DEFINITION = "#define SIMULATION_SETTINGS";
const std::string SPORE_DEFINITION = "#define SPORE_STRUCT";
const std::string WRAP_GRID_DEFINITION = "#define WRAP_AROUND";


constexpr int GRID_TEXTURE_LOCATION = 0;
constexpr int SDF_TEXTURE_READ_LOCATION = 1;
constexpr int SDF_TEXTURE_WRITE_LOCATION = 2;

constexpr int SPORE_BUFFER_LOCATION = 0;
constexpr int SIMULATION_BUFFER_LOCATION = 1;

// ============================
// Constructor/Destructor
// ============================
void assignDefaultsToSimulationData(SimulationData& data, float aspectRatio) {
    data.spore_count = SimulationDefaults::SPORE_COUNT;
    data.grid_size = SimulationDefaults::GRID_SIZE;
    data.sdf_reduction = SimulationDefaults::SDF_REDUCTION_FACTOR;
    data.spore_speed = SimulationDefaults::SPORE_SPEED;
    data.decay_speed = SimulationDefaults::SPORE_DECAY;
    data.turn_speed = SimulationDefaults::SPORE_TURN_SPEED;
    data.sensor_distance = SimulationDefaults::SPORE_SENSOR_DISTANCE;
    data.sensor_angle = SimulationDefaults::SPORE_SENSOR_ANGLE;
    data.aspect_ratio = aspectRatio;
}

MoldLabGame::MoldLabGame(const int width, const int height, const std::string &title)
    : GameEngine(width, height, title, false) {
    displayFramerate = true;

    addShaderDefinition(SIMULATION_SETTINGS_DEFINITION, "include/SimulationData.h");
    if (useTransparency) {
        addShaderDefinition(USE_TRANSPARENCY_DEFINITION, "");
    }
    if (wrapGrid) {
        addShaderDefinition(WRAP_GRID_DEFINITION, "");
    }
    addShaderDefinition(SPORE_DEFINITION, "include/Spore.h");

    // Set the simulation Settings to the Defaults
    assignDefaultsToSimulationData(simulationSettings,  static_cast<float>(getScreenWidth()) / static_cast<float>(getScreenHeight()));
}

MoldLabGame::~MoldLabGame() {
    if (triangleVbo)
        glDeleteBuffers(1, &triangleVbo);
    if (triangleVao)
        glDeleteVertexArrays(1, &triangleVao);
    if (sporesBuffer)
        glDeleteBuffers(1, &sporesBuffer);
    if (simulationSettingsBuffer)
        glDeleteBuffers(1, &simulationSettingsBuffer);
    if (voxelGridTexture)
        glDeleteTextures(1, &voxelGridTexture);
    if (sdfTexBuffer1)
        glDeleteTextures(1, &sdfTexBuffer1);
    if (sdfTexBuffer2)
        glDeleteTextures(1, &sdfTexBuffer2);

    std::cout << "Exiting..." << std::endl;
}

// ============================
// Initialization Helpers
// ============================
void MoldLabGame::initializeRenderShader(bool useTransparency) {
    if (!useTransparency) {
        addShaderDefinition(USE_TRANSPARENCY_DEFINITION, "");
    } else {
        removeShaderDefinition(USE_TRANSPARENCY_DEFINITION);
    }

    shaderProgram = CreateShaderProgram({
        {"shaders/renderer.glsl", GL_VERTEX_SHADER, true} // Combined vertex and fragment shaders
    });
}

void MoldLabGame::initializeMoveSporesShader(bool wrapAround) {
    if (!wrapAround) {
        addShaderDefinition(WRAP_GRID_DEFINITION, "");
    } else {
        removeShaderDefinition(WRAP_GRID_DEFINITION);
    }

    moveSporesShaderProgram = CreateShaderProgram({
        {"shaders/move_spores.glsl", GL_COMPUTE_SHADER, false}
    });
}

void MoldLabGame::initializeShaders() {
    initializeRenderShader(useTransparency);

    // Initialize the compute shaders
    drawSporesShaderProgram = CreateShaderProgram({
        {"shaders/draw_spores.glsl", GL_COMPUTE_SHADER, false}
    });

    initializeMoveSporesShader(wrapGrid);

    decaySporesShaderProgram = CreateShaderProgram({
        {"shaders/decay_spores.glsl", GL_COMPUTE_SHADER, false}
    });

    jumpFloodInitShaderProgram = CreateShaderProgram({
        {"shaders/jump_flood_init.glsl", GL_COMPUTE_SHADER, false}
    });

    jumpFloodStepShaderProgram = CreateShaderProgram({
        {"shaders/jump_flood_step.glsl", GL_COMPUTE_SHADER, false}
    });

    clearGridShaderProgram = CreateShaderProgram({
    {"shaders/clear_grid.glsl", GL_COMPUTE_SHADER, false}
    });

    randomizeSporesShaderProgram = CreateShaderProgram({
    {"shaders/randomize_spores.glsl", GL_COMPUTE_SHADER, false}
    });

    scaleSporesShaderProgram = CreateShaderProgram({
    {"shaders/scale_spores.glsl", GL_COMPUTE_SHADER, false}
    });
}


void MoldLabGame::initializeUniformVariables() {
    static int jfaStep = simulationSettings.grid_size;
    static int maxSporeSize = SimulationDefaults::SPORE_COUNT;

    jfaStepSV = ShaderVariable(jumpFloodStepShaderProgram, &jfaStep, "stepSize");
    maxSporeSizeSV = ShaderVariable(scaleSporesShaderProgram, &maxSporeSize, "maxSporeSize");
}


void MoldLabGame::initializeVertexBuffers() {
    GLint positionAttributeLocation = glGetAttribLocation(shaderProgram, "position");

    glGenVertexArrays(1, &triangleVao);
    glBindVertexArray(triangleVao);

    glGenBuffers(1, &triangleVbo);
    glBindBuffer(GL_ARRAY_BUFFER, triangleVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Enable and set the position attribute
    glEnableVertexAttribArray(positionAttributeLocation);
    glVertexAttribPointer(positionAttributeLocation, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, position)));

    glBindVertexArray(0); // Unbind VAO
}

void MoldLabGame::initializeVoxelGridBuffer() {
     int voxelGridSize = SimulationDefaults::MAX_GRID_SIZE;

    // ** Create Voxel Grid Texture **
    glGenTextures(1, &voxelGridTexture);
    glBindTexture(GL_TEXTURE_3D, voxelGridTexture);

    // Allocate storage for the 3D texture
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_R32F, voxelGridSize, voxelGridSize, voxelGridSize);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // Bind the texture as an image unit for compute shader access
    glBindImageTexture(GRID_TEXTURE_LOCATION, voxelGridTexture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32F);

    glBindTexture(GL_TEXTURE_3D, 0); // Unbind the texture
}


void MoldLabGame::initializeSDFBuffer() {
     int reducedGridSize = simulationSettings.grid_size / simulationSettings.sdf_reduction;

    glGenTextures(1, &sdfTexBuffer1);
    glBindTexture(GL_TEXTURE_3D, sdfTexBuffer1);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA32F, reducedGridSize, reducedGridSize, reducedGridSize);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_3D, 0);

    glGenTextures(1, &sdfTexBuffer2);
    glBindTexture(GL_TEXTURE_3D, sdfTexBuffer2);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA32F, reducedGridSize, reducedGridSize, reducedGridSize);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_3D, 0);
}


void uploadSettingsBuffer(GLuint &simulationSettingsBuffer, const SimulationData &settings) {
    if (simulationSettingsBuffer == 0) {
        glGenBuffers(1, &simulationSettingsBuffer);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, simulationSettingsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(SimulationData), &settings, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SIMULATION_BUFFER_LOCATION, simulationSettingsBuffer); // Binding index 2 for settings

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // Unbind
}

void MoldLabGame::initializeSimulationBuffers() {
     GLsizeiptr sporesSize = sizeof(Spore) * simulationSettings.spore_count;


    glGenBuffers(1, &sporesBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sporesBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sporesSize, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SPORE_BUFFER_LOCATION, sporesBuffer); // Binding index 1 for spores
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // Unbind

    // **Settings Buffer**
    uploadSettingsBuffer(simulationSettingsBuffer, simulationSettings);
}


// ============================
// Update Helpers
// ============================
void MoldLabGame::HandleCameraMovement(const float orbitRadius, const float deltaTime) {
    static float angle = 0.0f; // Current angle of rotation

    constexpr float rotationSpeed = 100.0f;

    // Update the angle
    angle += rotationSpeed * deltaTime;

     float gridCenter = (static_cast<float>(simulationSettings.grid_size) - 1.0f) * 0.5f; // Adjust for the centered cube positions
    set_vec4(simulationSettings.camera_focus, gridCenter, gridCenter, gridCenter, 0.0);

    // Adjust horizontal angle
    if (inputState.isLeftPressed) {
        horizontalAngle += rotationSpeed * deltaTime;
    }
    if (inputState.isRightPressed) {
        horizontalAngle -= rotationSpeed * deltaTime;
    }

    // Adjust vertical angle (clamped to avoid flipping)
    if (inputState.isUpPressed) {
        verticalAngle = std::min(verticalAngle + rotationSpeed * deltaTime, 89.0f);
    }
    if (inputState.isDownPressed) {
        verticalAngle = std::max(verticalAngle - rotationSpeed * deltaTime, -89.0f);
    }

    // Ensure angles wrap around properly
    if (horizontalAngle > 360.0f) horizontalAngle -= 360.0f;
    if (horizontalAngle < 0.0f) horizontalAngle += 360.0f;
    // Convert angles to radians
    float azimuth = horizontalAngle * SimulationDefaults::PI / 180.0f; // Azimuth in radians
    float altitude = verticalAngle * SimulationDefaults::PI / 180.0f; // Altitude in radians

    // Compute camera position in spherical coordinates relative to the origin
    float x = orbitRadius * cos(altitude) * sin(azimuth);
    float y = orbitRadius * sin(altitude);
    float z = orbitRadius * cos(altitude) * cos(azimuth);

    // Get the focus point position
    const vec4 &focusPoint = simulationSettings.camera_focus;

    // Offset camera position by focus point
    set_vec4(simulationSettings.camera_position, focusPoint[0] + x, focusPoint[1] + y, focusPoint[2] + z, 0.0);
}

void MoldLabGame::clearGrid() const {
    const int gridSize = simulationSettings.grid_size;
    DispatchComputeShader(clearGridShaderProgram, gridSize, gridSize, gridSize);
}


void MoldLabGame::resetSporesAndGrid() const {
    clearGrid();
    DispatchComputeShader(randomizeSporesShaderProgram, simulationSettings.spore_count, 1, 1);
}


void MoldLabGame::DispatchComputeShaders() {
    int gridSize = simulationSettings.grid_size;

    uploadSettingsBuffer(simulationSettingsBuffer, simulationSettings);

    if (!gridSizeChanged) {
        DispatchComputeShader(decaySporesShaderProgram, gridSize, gridSize, gridSize);

        DispatchComputeShader(moveSporesShaderProgram, simulationSettings.spore_count, 1, 1);

        DispatchComputeShader(drawSporesShaderProgram, simulationSettings.spore_count, 1, 1);
    } else {
        resetSporesAndGrid();
    }

    gridSizeChanged = false;

    executeJFA();
}

void MoldLabGame::executeJFA() const {
    glUseProgram(jumpFloodInitShaderProgram);

    GLuint readTexture = sdfTexBuffer1;
    GLuint writeTexture = sdfTexBuffer2; // will be used later

    glBindImageTexture(SDF_TEXTURE_READ_LOCATION, readTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F); // set it to write ONLY for initialization


     int reducedGridSize = simulationSettings.grid_size / simulationSettings.sdf_reduction;
    DispatchComputeShader(jumpFloodInitShaderProgram, reducedGridSize, reducedGridSize, reducedGridSize);
    // inits the read, for later use
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);


    glUseProgram(jumpFloodStepShaderProgram);

    // Start with the largest power of 2 that's less than or equal to reducedGridSize
    int stepSize = 1;
    while (stepSize * 2 < reducedGridSize) {
        stepSize *= 2;
    }

    int iterations = 0;
    int testStopping = 1;

    while (stepSize >= 1) {
        iterations++;
        glBindImageTexture(SDF_TEXTURE_READ_LOCATION, readTexture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
        glBindImageTexture(SDF_TEXTURE_WRITE_LOCATION, writeTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);


        *jfaStepSV.value = stepSize;
        jfaStepSV.uploadToShader();


        DispatchComputeShader(jumpFloodStepShaderProgram, reducedGridSize, reducedGridSize, reducedGridSize);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        stepSize /= 2; // Halve step size
        std::swap(readTexture, writeTexture);

        if (iterations == testStopping) {
            // break;
        }
    }

    glBindImageTexture(SDF_TEXTURE_READ_LOCATION, readTexture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
    // set to read after last swap for rendering
}



// ============================
// Core Lifecycle Functions
// ============================
void MoldLabGame::renderingStart() {
    initializeShaders();

    initializeUniformVariables();

    initializeVertexBuffers();

    initializeVoxelGridBuffer();

    initializeSDFBuffer();

    // initializeSpores();

    initializeSimulationBuffers();
}

void MoldLabGame::start() {
    ImGuiIO& io = ImGui::GetIO();
    // Load the default font at a larger size (e.g., 24 pixels)
    io.Fonts->AddFontFromFileTTF("Fonts/ProggyClean.ttf", 15.0f);
    io.Fonts->AddFontFromFileTTF("Fonts/Roboto-Bold.ttf", 25.0f);

    inputManager.bindKeyState(GLFW_KEY_D, &inputState.isDPressed);
    inputManager.bindKeyState(GLFW_KEY_A, &inputState.isAPressed);
    inputManager.bindKeyState(GLFW_KEY_LEFT, &inputState.isLeftPressed);
    inputManager.bindKeyState(GLFW_KEY_RIGHT, &inputState.isRightPressed);
    inputManager.bindKeyState(GLFW_KEY_UP, &inputState.isUpPressed);
    inputManager.bindKeyState(GLFW_KEY_DOWN, &inputState.isDownPressed);


    resetSporesAndGrid();
}

void MoldLabGame::update(float deltaTime) {
    HandleCameraMovement(orbitRadius, deltaTime);

    simulationSettings.delta_time = deltaTime;

     float orbitDistanceChange = static_cast<float>(simulationSettings.grid_size) / 8.0f;

    if (inputState.isDPressed) {
        orbitRadius += orbitDistanceChange * deltaTime;
    } else if (inputState.isAPressed) {
        orbitRadius -= orbitDistanceChange * deltaTime;
    }

    DispatchComputeShaders();
}


void MoldLabGame::render() {
    // While using the
    glUseProgram(shaderProgram);

    // Draw the full-screen quad
    glBindVertexArray(triangleVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

bool SliderFloatWithTooltip(const char* label, const char* sliderId, float* value, float min, float max, const char* tooltip) {
    // Display the slider with the provided ID
    bool valueChanged = ImGui::SliderFloat(sliderId, value, min, max);

    // Place the slider on the same line
    ImGui::SameLine();

    // Display the label for the slider
    ImGui::Text("%s", label);
    if (ImGui::IsItemHovered() && tooltip) {
        ImGui::SetTooltip("%s", tooltip); // Show tooltip if provided
    }


    return valueChanged;
}

bool SliderIntWithTooltip(const char* label, const char* sliderId, int* value, int min, int max, const char* tooltip) {
    // Display the slider with the provided ID
    bool valueChanged = ImGui::SliderInt(sliderId, value, min, max);

    // Place the slider on the same line
    ImGui::SameLine();

    // Display the label for the slider
    ImGui::Text("%s", label);
    if (ImGui::IsItemHovered() && tooltip) {
        ImGui::SetTooltip("%s", tooltip); // Show tooltip if provided
    }


    return valueChanged;
}



void MoldLabGame::renderUI() {
    ImGui::GetStyle().Alpha = 0.8f;

    // Add sliders for test values or other parameters
    ImGui::Begin("Simulation Settings"); // Begin a window

    ImGuiIO& io = ImGui::GetIO();

    ImGui::Separator(); // Add a line separator for clarity
    ImGui::PushFont(io.Fonts->Fonts[1]); // Assuming the larger/bold font is at index 1
    ImGui::Text("Instructions:");
    ImGui::PopFont();
    ImGui::Spacing(); // Add vertical space

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.6f, 1.0f)); // Light orange text for emphasis
    ImGui::TextWrapped("Arrow Keys: Move the camera");
    ImGui::TextWrapped("A: Zoom in");
    ImGui::TextWrapped("D: Zoom Out");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::PushFont(io.Fonts->Fonts[1]); // Assuming the larger/bold font is at index 1
    ImGui::Text("Variables:");
    ImGui::PopFont();

    SliderIntWithTooltip("Spore Count", "##SporeCountSlider", &simulationSettings.spore_count, 1, SimulationDefaults::MAX_SPORE_COUNT, "Number of spores in the simulation.");

    int previousGridSize = simulationSettings.grid_size;
    gridSizeChanged = SliderIntWithTooltip("Grid Size", "##GridSizeSlider", &simulationSettings.grid_size, 25,
                         SimulationDefaults::MAX_GRID_SIZE,
                         "The number of voxels that make up one side length of the cube grid. "
                         "\nNote: This will Clear the current voxels and randomize spore positions. Will also scale grid-size dependent settings with it");

    if (gridSizeChanged) {
        // Ensure grid_size is divisible by sdf_reduction
        int reduction = simulationSettings.sdf_reduction;
        simulationSettings.grid_size = (simulationSettings.grid_size / reduction) * reduction;

        if (previousGridSize != simulationSettings.grid_size) {
            float gridResizeFactor = static_cast<float>(simulationSettings.grid_size) / static_cast<float>(previousGridSize);
            simulationSettings.spore_speed *= gridResizeFactor;
            simulationSettings.sensor_distance *= gridResizeFactor;

            orbitRadius *= gridResizeFactor;

            simulationSettings.grid_resize_factor = gridResizeFactor;
        }
    }

    if  (simulationSettings.grid_size % simulationSettings.sdf_reduction != 0) {
        // ReSharper disable once CppDFAUnreachableCode
        std::cerr << "Warning: GRID_SIZE is not evenly divisible by SDF_REDUCTION_FACTOR!" << std::endl;
    }
    SliderFloatWithTooltip("Spore Speed", "##SporeSpeedSlider", &simulationSettings.spore_speed, 0.0f, static_cast<float>(simulationSettings.grid_size) / 2.0f, "Sets the speed of the spores. Voxels per second.");
    SliderFloatWithTooltip("Turn Speed", "##TurnSpeedSlider", &simulationSettings.turn_speed, 0.0f, 5.0f, "Turn speed of spores. Rotations per second.");
    SliderFloatWithTooltip("Decay Speed", "##DecaySpeedSlider", &simulationSettings.decay_speed, 0.0f, 10.0f, "Decay speed of spores. 1/x seconds to fully decay.");
    SliderFloatWithTooltip("Sensor Distance", "##SensorDistanceSlider", &simulationSettings.sensor_distance, 0.0f, static_cast<float>(simulationSettings.grid_size) / 2.0f, "Sets the distance that the spore can see. In Voxels.");
    SliderFloatWithTooltip("Sensor Angle", "##SensorAngleSlider", &simulationSettings.sensor_angle, 0.0f, SimulationDefaults::PI, "Sets the angle that the spores see. In Radians. 0 is directly on the forward sensor, PI being directly behind it.");

    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::Button("Randomize Spores")) {
        resetSporesAndGrid(); // Call the function when the button is pressed
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "Randomizes Spore positions and resets Grid Values"); // Show tooltip if provided
    }


    bool previousTransparentState = useTransparency; // Track the previous state
    if (ImGui::Checkbox("Use Transparency", &useTransparency)) {
        if (useTransparency != previousTransparentState) {
            initializeRenderShader(useTransparency);
        }
    }


    bool previousWrappingState = wrapGrid; // Track the previous state
    if (ImGui::Checkbox("Wrap Grid", &wrapGrid)) {
        if (wrapGrid != previousWrappingState) {
            initializeMoveSporesShader(wrapGrid);
        }
    }


    // Add VSync toggle at the top
    bool currentVSync = GetVsyncStatus();
    if (ImGui::Checkbox("VSync", &currentVSync)) {
        SetVsyncStatus(currentVSync);
    }

    ImGui::End(); // End the window

    const float DISTANCE = 10.0f; // Distance from the edges
    ImVec2 windowPos = ImVec2(ImGui::GetIO().DisplaySize.x - DISTANCE, ImGui::GetIO().DisplaySize.y - DISTANCE);
    ImVec2 windowPivot = ImVec2(1.0f, 1.0f); // Bottom-right corner as pivot

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPivot);
    ImGui::SetNextWindowBgAlpha(0.3f); // Transparent background

    if (ImGui::Begin("Framerate Overlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
        ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
                                                     }
    ImGui::End();
}
