// ========================================================================= //
// Authors: Daniel Rutz, Daniel Ströter, Roman Getto, Matthias Bein          //
//                                                                           //
// GRIS - Graphisch Interaktive Systeme                                      //
// Technische Universität Darmstadt                                          //
// Fraunhoferstrasse 5                                                       //
// D-64283 Darmstadt, Germany                                                //
//                                                                           //
// Content: Widget for showing OpenGL scene, SOLUTION                        //
// ========================================================================= //

#include <cmath>

#include <QtDebug>
#include <QMatrix4x4>
#include <QOpenGLVersionFunctionsFactory>

#include "shader.h"
#include "openglview.h"

GLuint OpenGLView::csVAO = 0;
GLuint OpenGLView::csVBOs[2] = {0, 0};

OpenGLView::OpenGLView(QWidget* parent) : QOpenGLWidget(parent) {
    setDefaults();

    connect(&fpsCounterTimer, &QTimer::timeout, this, &OpenGLView::refreshFpsCounter);
    fpsCounterTimer.setInterval(1000);
    fpsCounterTimer.setSingleShot(false);
    fpsCounterTimer.start();
}

void OpenGLView::setGridSize(int gridSize)
{
    this->gridSize = gridSize;
    emit triangleCountChanged(getTriangleCount());
}

void OpenGLView::initializeGL()
{
    f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(QOpenGLContext::currentContext());
    const GLubyte* versionString = f->glGetString(GL_VERSION);
    std::cout << "The current OpenGL version is: " << versionString << std::endl;
    state.setOpenGLFunctions(f);

    //black screen
    f->glClearColor(0.f, 0.f, 0.f, 1.f);
    //enable depth buffer
    f->glEnable(GL_DEPTH_TEST);

    GLuint testTexture = loadImageIntoTexture(f, "../Textures/TEST_GRID.bmp");

    GLuint diffuseTexture = loadImageIntoTexture(f, "../Textures/rough_block_wall_diff_1k.jpg", true);
    GLuint normalTexture = loadImageIntoTexture(f, "../Textures/rough_block_wall_nor_1k.jpg", true);
    GLuint displacementTexture = loadImageIntoTexture(f, "../Textures/rough_block_wall_disp_1k.jpg", true);

    //Load the sphere of the light
    sphereMesh.setGLFunctionPtr(f);
    sphereMesh.loadOFF("../Models/sphere.off");
    sphereMesh.setStaticColor(Vec3f(1.0f, 1.0f, 0.0f));

    //load meshes
    meshes.emplace_back(f);
    meshes[0].loadOFF("../Models/doppeldecker.off");
    meshes[0].setStaticColor(Vec3f(0.0f, 1.0f, 0.0f));
    meshes[0].setTexture(testTexture);
    meshes[0].setColoringMode(TriangleMesh::ColoringType::TEXTURE);

    meshes.emplace_back(f);
    meshes[1].generateTerrain(50, 50, 4000);
    meshes[1].setStaticColor(Vec3f(1.f, 1.f, 0.f));
    meshes[1].setColoringMode(TriangleMesh::ColoringType::COLOR_ARRAY);

    bumpSphereMesh.generateSphere(f);
    bumpSphereMesh.setStaticColor(Vec3f(0.8f, 0.8f, 0.8f));
    bumpSphereMesh.setColoringMode(TriangleMesh::ColoringType::BUMP_MAPPING);
    bumpSphereMesh.setTexture(diffuseTexture);
    bumpSphereMesh.setNormalTexture(normalTexture);
    bumpSphereMesh.setDisplacementTexture(displacementTexture);

    //load coordinate system
    csVAO = genCSVAO();

    //load shaders
    GLuint lightShaderID = readShaders(f, "../Shader/only_mvp.vert", "../Shader/constant_color.frag");
    if (lightShaderID) {
        programIDs.push_back(lightShaderID);
        state.setStandardProgram(lightShaderID);
    }
    GLuint shaderID = readShaders(f, "../Shader/only_mvp.vert", "../Shader/lambert.frag");
    if (shaderID != 0) programIDs.push_back(shaderID);
    currentProgramID = lightShaderID;

    bumpProgramID = readShaders(f, "../Shader/bump.vert", "../Shader/bump.frag");

    emit shaderCompiled(0);
    emit shaderCompiled(1);
}

void OpenGLView::resizeGL(int width, int height) {
    //Calculate new projection matrix
    const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    state.loadIdentityProjectionMatrix();
    state.getCurrentProjectionMatrix().perspective(65.f, aspectRatio, 0.5f, 10000.f);

    //set projection matrix in OpenGL shader
    state.switchToStandardProgram();
    f->glUniformMatrix4fv(state.getProjectionUniform(), 1, GL_FALSE, state.getCurrentProjectionMatrix().constData());
    state.setCurrentProgram(bumpProgramID);
    f->glUniformMatrix4fv(state.getProjectionUniform(), 1, GL_FALSE, state.getCurrentProjectionMatrix().constData());
    for (GLuint progID : programIDs) {
        state.setCurrentProgram(progID);
        f->glUniformMatrix4fv(state.getProjectionUniform(), 1, GL_FALSE, state.getCurrentProjectionMatrix().constData());
    }

    //Resize viewport
    f->glViewport(0, 0, width, height);
}

void OpenGLView::drawSkybox() {
    // TODO(3.2): Draw a skybox
    
    //shader configuration and activation
    GLuint skybox_shaderID = readShaders(f, "../Shader/skybox1.vert", "../Shader/skybox1.frag");
    if (skybox_shaderID != 0) {
        state.setCurrentProgram(skybox_shaderID);
    }

    f->glUniform1i(f->glGetUniformLocation(skybox_shaderID, "skybox"), 0);

    //load cubemap imgs
    const char* filename[6] = {
        "../Textures/skybox1/pos_x.bmp",
        "../Textures/skybox1/neg_x.bmp",
        "../Textures/skybox1/pos_y.bmp",
        "../Textures/skybox1/neg_y.bmp",
        "../Textures/skybox1/pos_z.bmp",
        "../Textures/skybox1/neg_z.bmp"
    };
    
    GLuint cubemap_texture = loadCubeMap(f, filename);

    // set buffers
#define SKY_SIZE 10.0f
    float skyboxVertices[] = {
        // positions          
        -SKY_SIZE,  SKY_SIZE, -SKY_SIZE,
        -SKY_SIZE, -SKY_SIZE, -SKY_SIZE,
         SKY_SIZE, -SKY_SIZE, -SKY_SIZE,
         SKY_SIZE, -SKY_SIZE, -SKY_SIZE,
         SKY_SIZE,  SKY_SIZE, -SKY_SIZE,
        -SKY_SIZE,  SKY_SIZE, -SKY_SIZE,

        -SKY_SIZE, -SKY_SIZE,  SKY_SIZE,
        -SKY_SIZE, -SKY_SIZE, -SKY_SIZE,
        -SKY_SIZE,  SKY_SIZE, -SKY_SIZE,
        -SKY_SIZE,  SKY_SIZE, -SKY_SIZE,
        -SKY_SIZE,  SKY_SIZE,  SKY_SIZE,
        -SKY_SIZE, -SKY_SIZE,  SKY_SIZE,

         SKY_SIZE, -SKY_SIZE, -SKY_SIZE,
         SKY_SIZE, -SKY_SIZE,  SKY_SIZE,
         SKY_SIZE,  SKY_SIZE,  SKY_SIZE,
         SKY_SIZE,  SKY_SIZE,  SKY_SIZE,
         SKY_SIZE,  SKY_SIZE, -SKY_SIZE,
         SKY_SIZE, -SKY_SIZE, -SKY_SIZE,

        -SKY_SIZE, -SKY_SIZE,  SKY_SIZE,
        -SKY_SIZE,  SKY_SIZE,  SKY_SIZE,
         SKY_SIZE,  SKY_SIZE,  SKY_SIZE,
         SKY_SIZE,  SKY_SIZE,  SKY_SIZE,
         SKY_SIZE, -SKY_SIZE,  SKY_SIZE,
        -SKY_SIZE, -SKY_SIZE,  SKY_SIZE,

        -SKY_SIZE,  SKY_SIZE, -SKY_SIZE,
         SKY_SIZE,  SKY_SIZE, -SKY_SIZE,
         SKY_SIZE,  SKY_SIZE,  SKY_SIZE,
         SKY_SIZE,  SKY_SIZE,  SKY_SIZE,
        -SKY_SIZE,  SKY_SIZE,  SKY_SIZE,
        -SKY_SIZE,  SKY_SIZE, -SKY_SIZE,

        -SKY_SIZE, -SKY_SIZE, -SKY_SIZE,
        -SKY_SIZE, -SKY_SIZE,  SKY_SIZE,
         SKY_SIZE, -SKY_SIZE, -SKY_SIZE,
         SKY_SIZE, -SKY_SIZE, -SKY_SIZE,
        -SKY_SIZE, -SKY_SIZE,  SKY_SIZE,
         SKY_SIZE, -SKY_SIZE,  SKY_SIZE
    };
    unsigned int skyboxVAO, skyboxVBO;
    f->glGenVertexArrays(1, &skyboxVAO);
    f->glGenBuffers(1, &skyboxVBO);
    f->glBindVertexArray(skyboxVAO);
    f->glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    f->glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    f->glEnableVertexAttribArray(0);
    f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);


    // draw
    f->glDepthFunc(GL_LEQUAL);
    f->glUniformMatrix4fv(state.getModelViewUniform(), 1, GL_FALSE, state.getCurrentModelViewMatrix().constData());
    f->glUniformMatrix4fv(state.getProjectionUniform(), 1, GL_FALSE, state.getCurrentProjectionMatrix().constData());  
    f->glUniform3f(state.getCameraPositionUniform(), cameraPos.x(), cameraPos.y(), cameraPos.z());
    
    f->glBindVertexArray(skyboxVAO);
    f->glActiveTexture(GL_TEXTURE0);
    f->glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_texture);
    f->glDrawArrays(GL_TRIANGLES, 0, 36);
    
    // restore matrix and attributes
    f->glBindVertexArray(0);
    f->glDepthFunc(GL_LESS); // set depth function back to default
}

void OpenGLView::paintGL() {
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    state.loadIdentityModelViewMatrix();

    //translate to center, rotate and render coordinate system and light sphere
    QVector3D cameraLookAt = cameraPos + cameraDir;
    static QVector3D upVector(0.0f, 1.0f, 0.0f);
    state.getCurrentModelViewMatrix().lookAt(cameraPos, cameraLookAt, upVector);
    drawSkybox();
    state.switchToStandardProgram();
    drawCS();

    if (lightMoves) moveLight();

    drawLight();

    unsigned int trianglesDrawn = 0;
    // draw bump mapping sphere
    state.setCurrentProgram(bumpProgramID);
    state.pushModelViewMatrix();
    state.setLightUniform();
    state.getCurrentModelViewMatrix().translate(0, 5, 0);
    trianglesDrawn += bumpSphereMesh.draw(state);
    state.popModelViewMatrix();

    state.setCurrentProgram(currentProgramID);
    state.setLightUniform();

    // draw objects. count triangles and objects drawn.
    state.pushModelViewMatrix();
    for (int i = 0; i < gridSize; ++i) {
        state.getCurrentModelViewMatrix().translate(static_cast<float>(1.f), 0.f);
        trianglesDrawn += meshes[0].draw(state);
    }
    state.popModelViewMatrix();
    for (size_t i = 1; i < meshes.size(); ++i) {
        trianglesDrawn += meshes[i].draw(state);
    }
    // cout number of objects and triangles if different from last run
    if (trianglesDrawn != trianglesLastRun) {
        trianglesLastRun = trianglesDrawn;
        emit triangleCountChanged(trianglesDrawn);
    }

    frameCounter++;
    update();
}

void OpenGLView::drawCS() {
    f->glUniformMatrix4fv(state.getModelViewUniform(), 1, GL_FALSE, state.getCurrentModelViewMatrix().constData());
    f->glBindVertexArray(csVAO);
    f->glDrawArrays(GL_LINES, 0, 6);
    f->glBindVertexArray(GL_NONE);
}

void OpenGLView::drawLight() {
    // draw yellow sphere for light source
    state.pushModelViewMatrix();
    Vec3f& lp = state.getLightPos();
    state.getCurrentModelViewMatrix().translate(lp.x(), lp.y(), lp.z());
    sphereMesh.draw(state);
    state.popModelViewMatrix();
}

void OpenGLView::moveLight()
{
    state.getLightPos().rotY(lightMotionSpeed * (deltaTimer.restart() / 1000.f));
}

unsigned int OpenGLView::getTriangleCount() const
{
    size_t result = 0;

    return result;
}

void OpenGLView::setDefaults() {
    // scene Information
    cameraPos = QVector3D(0.0f, 0.0f, -3.0f);
    cameraDir = QVector3D(0.f, 0.f, -1.f);
    movementSpeed = 0.02f;
    angleX = 0.0f;
    angleY = 0.0f;
    // light information
    state.getLightPos() = Vec3f(0.0f, 5.0f, 20.0f);
    lightMotionSpeed = 10.f;
    // mouse information
    mouseSensitivy = 1.0f;

    gridSize = 3;
    // last run: 0 objects and 0 triangles
    objectsLastRun = 0;
    trianglesLastRun = 0;
}

void OpenGLView::refreshFpsCounter()
{
    emit fpsCountChanged(frameCounter);
    frameCounter = 0;
}

void OpenGLView::triggerLightMovement(bool shouldMove)
{
    lightMoves = shouldMove;
    if (lightMoves) {
        if (deltaTimer.isValid()) {
            deltaTimer.restart();
        } else {
            deltaTimer.start();
        }
    }
}

void OpenGLView::cameraMoves(float deltaX, float deltaY, float deltaZ)
{
    QVector3D ortho(-cameraDir.z(),0.0f,cameraDir.x());
    QVector3D up = QVector3D::crossProduct(cameraDir, ortho).normalized();

    cameraPos += deltaX * ortho;
    cameraPos += deltaY * up;
    cameraPos += deltaZ * cameraDir;

    update();
}

void OpenGLView::cameraRotates(float deltaX, float deltaY)
{
    angleX = std::fmod(angleX + deltaX, 360.f);
    angleY += deltaY;
    angleY = std::max(-70.f, std::min(angleY, 70.f));

    cameraDir.setX(std::sin(angleX * M_RadToDeg) * std::cos(angleY * M_RadToDeg));
    cameraDir.setZ(-std::cos(angleX * M_RadToDeg) * std::cos(angleY * M_RadToDeg));
    cameraDir.setY(std::max(0.0f, std::min(std::sqrt(1.0f - cameraDir.x() * cameraDir.x() - cameraDir.z() * cameraDir.z()), 1.0f)));

    if (angleY < 0.f) cameraDir.setY(-cameraDir.y());

    update();
}

void OpenGLView::changeShader(unsigned int index) {
    makeCurrent();
    try {
        GLuint progID = programIDs.at(index);
        currentProgramID = progID;
    } catch (std::out_of_range& ex) {
        qFatal("Tried to access shader index that has not been loaded! %s", ex.what());
    }
    doneCurrent();
}

void OpenGLView::compileShader(const QString& vertexShaderPath, const QString& fragmentShaderPath) {
    GLuint programHandle = readShaders(f, vertexShaderPath, fragmentShaderPath);
    if (programHandle) {
        programIDs.push_back(programHandle);
        emit shaderCompiled(programIDs.size() - 1);
    }
}

void OpenGLView::changeColoringMode(TriangleMesh::ColoringType type)
{
    for (auto& mesh: meshes) mesh.setColoringMode(type);
}

void OpenGLView::toggleBoundingBox(bool enable)
{
    for (auto& mesh: meshes) mesh.toggleBB(enable);
    bumpSphereMesh.toggleBB(enable);
}

void OpenGLView::toggleNormals(bool enable)
{
    for (auto& mesh: meshes) mesh.toggleNormals(enable);
    bumpSphereMesh.toggleNormals(enable);
}

void OpenGLView::toggleDiffuse(bool enable)
{
    bumpSphereMesh.toggleDiffuse(enable);
}

void OpenGLView::toggleNormalMapping(bool enable)
{
    bumpSphereMesh.toggleNormalMapping(enable);
}

void OpenGLView::toggleDisplacementMapping(bool enable)
{
    bumpSphereMesh.toggleDisplacementMapping(enable);
}

void OpenGLView::recreateTerrain()
{
    makeCurrent();
    meshes[1].clear();
    meshes[1].generateTerrain(50, 50, 4000);
    doneCurrent();
}

// This creates a VAO that represents the coordinate system
GLuint OpenGLView::genCSVAO() {
    GLuint VAOresult;
    f->glGenVertexArrays(1, &VAOresult);
    f->glGenBuffers(2, csVBOs);

    f->glBindVertexArray(VAOresult);
    f->glBindBuffer(GL_ARRAY_BUFFER, csVBOs[0]);
    const static float vertices[] = {
            0.f, 0.f, 0.f,
            5.f, 0.f, 0.f,
            0.f, 0.f, 0.f,
            0.f, 5.f, 0.f,
            0.f, 0.f, 0.f,
            0.f, 0.f, 5.f,
    };
    f->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    f->glVertexAttribPointer(POSITION_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    f->glEnableVertexAttribArray(POSITION_LOCATION);
    f->glBindBuffer(GL_ARRAY_BUFFER, csVBOs[1]);
    const static float colors[] = {
            1.f, 0.f, 0.f,
            1.f, 0.f, 0.f,
            0.f, 1.f, 0.f,
            0.f, 1.f, 0.f,
            0.f, 0.f, 1.f,
            0.f, 0.f, 1.f,
    };
    f->glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
    f->glVertexAttribPointer(COLOR_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    f->glEnableVertexAttribArray(COLOR_LOCATION);
    f->glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
    f->glBindVertexArray(GL_NONE);
    return VAOresult;
}
