// ========================================================================= //
// Authors: Daniel Rutz, Daniel Ströter, Roman Getto, Matthias Bein          //
//                                                                           //
// GRIS - Graphisch Interaktive Systeme                                      //
// Technische Universität Darmstadt                                          //
// Fraunhoferstrasse 5                                                       //
// D-64283 Darmstadt, Germany                                                //
//                                                                           //
// Content: Simple class for reading and rendering triangle meshes, SOLUTION //
//   * readOFF                                                               //
//   * draw                                                                  //
//   * transformations                                                       //
// ========================================================================= //

#include <cmath>
#include <array>
#include <cfloat>
#include <algorithm>
#include <random>
#include <array>

#include <fstream>
#include <iostream>
#include <iomanip>

#include <QtMath>
#include <QOpenGLFunctions_3_3_Core>

#include "trianglemesh.h"
#include "renderstate.h"
#include "utilities.h"
#include "clipplane.h"
#include "shader.h"

using glVertexAttrib3fvPtr = void (*)(GLuint index, const GLfloat *v);
using glVertexAttrib3fPtr = void (*)(GLuint index, GLfloat v1, GLfloat v2, GLfloat v3);

TriangleMesh::TriangleMesh(QOpenGLFunctions_3_3_Core *f)
    : staticColor(1.f, 1.f, 1.f), f(f)
{
    clear();
}

TriangleMesh::~TriangleMesh()
{
    // clear data
    clear();
}

void TriangleMesh::clear()
{
    // clear mesh data
    vertices.clear();
    triangles.clear();
    normals.clear();
    colors.clear();
    texCoords.clear();
    // clear bounding box data
    boundingBoxMin = Vec3f(FLT_MAX, FLT_MAX, FLT_MAX);
    boundingBoxMax = Vec3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    boundingBoxMid.zero();
    boundingBoxSize.zero();
    // draw mode data
    coloringType = ColoringType::STATIC_COLOR;
    withBB = false;
    withNormals = false;
    textureID.val = 0;
    cleanupVBO();
}

void TriangleMesh::coutData()
{
    std::cout << std::endl;
    std::cout << "=== MESH DATA ===" << std::endl;
    std::cout << "nr. triangles: " << triangles.size() << std::endl;
    std::cout << "nr. vertices:  " << vertices.size() << std::endl;
    std::cout << "nr. normals:   " << normals.size() << std::endl;
    std::cout << "nr. colors:    " << colors.size() << std::endl;
    std::cout << "nr. texCoords: " << texCoords.size() << std::endl;
    std::cout << "BB: (" << boundingBoxMin << ") - (" << boundingBoxMax << ")" << std::endl;
    std::cout << "  BBMid: (" << boundingBoxMid << ")" << std::endl;
    std::cout << "  BBSize: (" << boundingBoxSize << ")" << std::endl;
    std::cout << "  VAO ID: " << VAO() << ", VBO IDs: f=" << VBOf() << ", v=" << VBOv() << ", n=" << VBOn() << ", c=" << VBOc() << ", t=" << VBOt() << std::endl;
    std::cout << "coloring using: ";
    switch (coloringType)
    {
    case ColoringType::STATIC_COLOR:
        std::cout << "a static color" << std::endl;
        break;
    case ColoringType::COLOR_ARRAY:
        std::cout << "a color array" << std::endl;
        break;
    case ColoringType::TEXTURE:
        std::cout << "a texture" << std::endl;
        break;

    case ColoringType::BUMP_MAPPING:
        std::cout << "a bump map" << std::endl;
        break;
    }
}

// ================
// === RAW DATA ===
// ================

void TriangleMesh::flipNormals(bool createVBOs)
{
    for (auto &n : normals)
        n *= -1.0f;
    // correct VBO
    if (createVBOs && VBOn() != 0)
    {
        if (!f)
            return;
        f->glBindBuffer(GL_ARRAY_BUFFER, VBOn());
        f->glBufferSubData(GL_ARRAY_BUFFER, 0, normals.size() * sizeof(Normal), normals.data());
        f->glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void TriangleMesh::translateToCenter(const Vec3f &newBBmid, bool createVBOs)
{
    Vec3f trans = newBBmid - boundingBoxMid;
    for (auto &vertex : vertices)
        vertex += trans;
    boundingBoxMin += trans;
    boundingBoxMax += trans;
    boundingBoxMid += trans;
    // data changed => delete VBOs and create new ones (not efficient but easy)
    if (createVBOs)
    {
        cleanupVBO();
        createAllVBOs();
    }
}

void TriangleMesh::scaleToLength(const float newLength, bool createVBOs)
{
    float length = std::max(std::max(boundingBoxSize.x(), boundingBoxSize.y()), boundingBoxSize.z());
    float scale = newLength / length;
    for (auto &vertex : vertices)
        vertex *= scale;
    boundingBoxMin *= scale;
    boundingBoxMax *= scale;
    boundingBoxMid *= scale;
    boundingBoxSize *= scale;
    // data changed => delete VBOs and create new ones (not efficient but easy)
    if (createVBOs)
    {
        cleanupVBO();
        createAllVBOs();
    }
}

// =================
// === LOAD MESH ===
// =================

void TriangleMesh::loadOFF(const char *filename, bool createVBOs)
{
    // clear any existing mesh
    clear();
    // load from off
    std::ifstream in(filename);
    if (!in.is_open())
    {
        std::cout << "loadOFF: can not find " << filename << std::endl;
        return;
    }
    const int MAX = 256;
    char s[MAX];
    in >> std::setw(MAX) >> s;
    // differentiate between OFF (vertices only) and NOFF (vertices and normals)
    bool noff = false;
    if (s[0] == 'O' && s[1] == 'F' && s[2] == 'F')
        ;
    else if (s[0] == 'N' && s[1] == 'O' && s[2] == 'F' && s[3] == 'F')
        noff = true;
    else
        return;
    // get number of vertices nv, faces nf and edges ne
    int nv, nf, ne;
    in >> std::setw(MAX) >> nv;
    in >> std::setw(MAX) >> nf;
    in >> std::setw(MAX) >> ne;
    if (nv <= 0 || nf <= 0)
        return;
    // read vertices
    vertices.resize(nv);
    for (int i = 0; i < nv; ++i)
    {
        in >> std::setw(MAX) >> vertices[i][0];
        in >> std::setw(MAX) >> vertices[i][1];
        in >> std::setw(MAX) >> vertices[i][2];
        boundingBoxMin[0] = std::min(vertices[i][0], boundingBoxMin[0]);
        boundingBoxMin[1] = std::min(vertices[i][1], boundingBoxMin[1]);
        boundingBoxMin[2] = std::min(vertices[i][2], boundingBoxMin[2]);
        boundingBoxMax[0] = std::max(vertices[i][0], boundingBoxMax[0]);
        boundingBoxMax[1] = std::max(vertices[i][1], boundingBoxMax[1]);
        boundingBoxMax[2] = std::max(vertices[i][2], boundingBoxMax[2]);
        if (noff)
        {
            in >> std::setw(MAX) >> normals[i][0];
            in >> std::setw(MAX) >> normals[i][1];
            in >> std::setw(MAX) >> normals[i][2];
        }
    }
    boundingBoxMid = 0.5f * boundingBoxMin + 0.5f * boundingBoxMax;
    boundingBoxSize = boundingBoxMax - boundingBoxMin;
    // read triangles
    triangles.resize(nf);
    for (int i = 0; i < nf; ++i)
    {
        int three;
        in >> std::setw(MAX) >> three;
        in >> std::setw(MAX) >> triangles[i][0];
        in >> std::setw(MAX) >> triangles[i][1];
        in >> std::setw(MAX) >> triangles[i][2];
    }
    // close ifstream
    in.close();
    // calculate normals if not given
    if (!noff)
        calculateNormalsByArea();
    // calculate texture coordinates
    calculateTexCoordsSphereMapping();
    // createVBO
    if (createVBOs)
    {
        createAllVBOs();
    }
}

void TriangleMesh::loadOFF(const char *filename, const Vec3f &BBmid, const float BBlength)
{
    loadOFF(filename, false);
    translateToCenter(BBmid, false);
    scaleToLength(BBlength, true);
}

void TriangleMesh::calculateNormalsByArea()
{
    // sum up triangle normals in each vertex
    normals.resize(vertices.size());
    for (auto &triangle : triangles)
    {
        unsigned int
            id0 = triangle[0],
            id1 = triangle[1],
            id2 = triangle[2];
        Vec3f
            vec1 = vertices[id1] - vertices[id0],
            vec2 = vertices[id2] - vertices[id0],
            normal = cross(vec1, vec2);
        normals[id0] += normal;
        normals[id1] += normal;
        normals[id2] += normal;
    }
    // normalize normals
    for (auto &normal : normals)
        normal.normalize();
}

void TriangleMesh::calculateTexCoordsSphereMapping()
{
    texCoords.clear();
    // texCoords by central projection on unit sphere
    // optional ...
    for (const auto &vertex : vertices)
    {
        const auto dist = vertex - boundingBoxMid;
        float u = (M_1_PI / 2) * std::atan2(dist.x(), dist.z()) + 0.5;
        float v = M_1_PI * std::asin(dist.y() / std::sqrt(dist.x() * dist.x() + dist.y() * dist.y() + dist.z() * dist.z()));
        texCoords.push_back(TexCoord{u, v});
    }
}

void TriangleMesh::calculateBB()
{
    // clear bounding box data
    boundingBoxMin = Vec3f(FLT_MAX, FLT_MAX, FLT_MAX);
    boundingBoxMax = Vec3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    boundingBoxMid.zero();
    boundingBoxSize.zero();
    // iterate over vertices
    for (auto &vertex : vertices)
    {
        boundingBoxMin[0] = std::min(vertex[0], boundingBoxMin[0]);
        boundingBoxMin[1] = std::min(vertex[1], boundingBoxMin[1]);
        boundingBoxMin[2] = std::min(vertex[2], boundingBoxMin[2]);
        boundingBoxMax[0] = std::max(vertex[0], boundingBoxMax[0]);
        boundingBoxMax[1] = std::max(vertex[1], boundingBoxMax[1]);
        boundingBoxMax[2] = std::max(vertex[2], boundingBoxMax[2]);
    }
    boundingBoxMid = 0.5f * boundingBoxMin + 0.5f * boundingBoxMax;
    boundingBoxSize = boundingBoxMax - boundingBoxMin;
}

GLuint TriangleMesh::createVBO(QOpenGLFunctions_3_3_Core *f, const void *data, int dataSize, GLenum target, GLenum usage)
{

    // 0 is reserved, glGenBuffers() will return non-zero id if success
    GLuint id = 0;
    // create a vbo
    f->glGenBuffers(1, &id);
    // activate vbo id to use
    f->glBindBuffer(target, id);
    // upload data to video card
    f->glBufferData(target, dataSize, data, usage);
    // check data size in VBO is same as input array, if not return 0 and delete VBO
    int bufferSize = 0;
    f->glGetBufferParameteriv(target, GL_BUFFER_SIZE, &bufferSize);
    if (dataSize != bufferSize)
    {
        f->glDeleteBuffers(1, &id);
        id = 0;
        std::cout << "createVBO() ERROR: Data size (" << dataSize << ") is mismatch with input array (" << bufferSize << ")." << std::endl;
    }
    // unbind after copying data
    f->glBindBuffer(target, 0);
    return id;
}

void TriangleMesh::createBBVAO(QOpenGLFunctions_3_3_Core *f)
{
    f->glGenVertexArrays(1, &VAObb.val);

    // create VBOs of bounding box
    VBOvbb.val = createVBO(f, BoxVertices, BoxVerticesSize, GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    VBOfbb.val = createVBO(f, BoxLineIndices, BoxLineIndicesSize, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW);

    // bind VAO of bounding box
    f->glBindVertexArray(VAObb.val);
    f->glBindBuffer(GL_ARRAY_BUFFER, VBOvbb.val);
    f->glVertexAttribPointer(POSITION_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBOfbb.val);

    f->glEnableVertexAttribArray(POSITION_LOCATION);
    f->glBindVertexArray(0);
    f->glBindBuffer(GL_ARRAY_BUFFER, 0);
    f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void TriangleMesh::createNormalVAO(QOpenGLFunctions_3_3_Core *f)
{
    if (vertices.size() != normals.size())
        return;
    std::vector<Vec3f> normalArrowVertices;
    normalArrowVertices.reserve(2 * vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        normalArrowVertices.push_back(vertices[i]);
        normalArrowVertices.push_back(vertices[i] + 0.1 * normals[i]);
    }

    f->glGenVertexArrays(1, &VAOn.val);
    VBOvn.val = createVBO(f, normalArrowVertices.data(), normalArrowVertices.size() * sizeof(Vertex), GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    f->glBindVertexArray(VAOn.val);
    f->glBindBuffer(GL_ARRAY_BUFFER, VBOvn.val);
    f->glEnableVertexAttribArray(POSITION_LOCATION);
    f->glVertexAttribPointer(POSITION_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    f->glBindVertexArray(0);
    f->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void TriangleMesh::createAllVBOs()
{
    if (!f)
        return;
    // create VAOs
    f->glGenVertexArrays(1, &VAO.val);

    // create VBOs
    VBOf.val = createVBO(f, triangles.data(), triangles.size() * sizeof(Triangle), GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW);
    VBOv.val = createVBO(f, vertices.data(), vertices.size() * sizeof(Vertex), GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    VBOn.val = createVBO(f, normals.data(), normals.size() * sizeof(Normal), GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    if (colors.size() == vertices.size())
    {
        VBOc.val = createVBO(f, colors.data(), colors.size() * sizeof(Color), GL_ARRAY_BUFFER, GL_STATIC_DRAW);
        f->glEnableVertexAttribArray(COLOR_LOCATION);
    }
    if (texCoords.size() == vertices.size())
    {
        VBOt.val = createVBO(f, texCoords.data(), texCoords.size() * sizeof(TexCoord), GL_ARRAY_BUFFER, GL_STATIC_DRAW);
        f->glEnableVertexAttribArray(TEXCOORD_LOCATION);
    }
    if (tangents.size() == vertices.size())
    {
        VBOtan.val = createVBO(f, tangents.data(), tangents.size() * sizeof(Tangent), GL_ARRAY_BUFFER, GL_STATIC_DRAW);
        f->glEnableVertexAttribArray(TANGENT_LOCATION);
    }

    // bind VBOs to VAO object
    f->glBindVertexArray(VAO.val);
    f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBOf.val);
    f->glBindBuffer(GL_ARRAY_BUFFER, VBOv.val);
    f->glVertexAttribPointer(POSITION_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    f->glEnableVertexAttribArray(POSITION_LOCATION);
    f->glBindBuffer(GL_ARRAY_BUFFER, VBOn.val);
    f->glVertexAttribPointer(NORMAL_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    f->glEnableVertexAttribArray(NORMAL_LOCATION);
    if (VBOc.val)
    {
        f->glBindBuffer(GL_ARRAY_BUFFER, VBOc.val);
        f->glVertexAttribPointer(COLOR_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        f->glEnableVertexAttribArray(COLOR_LOCATION);
    }

    if (VBOt.val)
    {
        f->glBindBuffer(GL_ARRAY_BUFFER, VBOt.val);
        f->glVertexAttribPointer(TEXCOORD_LOCATION, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        f->glEnableVertexAttribArray(TEXCOORD_LOCATION);
    }
    if (VBOtan.val)
    {
        f->glBindBuffer(GL_ARRAY_BUFFER, VBOtan.val);
        f->glVertexAttribPointer(TANGENT_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        f->glEnableVertexAttribArray(TANGENT_LOCATION);
    }

    f->glBindVertexArray(0);

    createBBVAO(f);

    createNormalVAO(f);
}

void TriangleMesh::cleanupVBO()
{
    if (!f)
        return;
    cleanupVBO(f);
}

void TriangleMesh::cleanupVBO(QOpenGLFunctions_3_3_Core *f)
{
    // delete VAO
    if (VAO.val != 0)
        f->glDeleteVertexArrays(1, &VAO.val);
    // delete VBO
    if (VBOv.val != 0)
        f->glDeleteBuffers(1, &VBOv.val);
    if (VBOn.val != 0)
        f->glDeleteBuffers(1, &VBOn.val);
    if (VBOf.val != 0)
        f->glDeleteBuffers(1, &VBOf.val);
    if (VBOc.val != 0)
        f->glDeleteBuffers(1, &VBOc.val);
    if (VBOt.val != 0)
        f->glDeleteBuffers(1, &VBOt.val);
    if (VBOtan.val != 0)
        f->glDeleteBuffers(1, &VBOtan.val);
    if (VAObb.val != 0)
        f->glDeleteVertexArrays(1, &VAObb.val);
    if (VBOvbb.val != 0)
        f->glDeleteBuffers(1, &VBOvbb.val);
    if (VBOfbb.val != 0)
        f->glDeleteBuffers(1, &VBOfbb.val);
    if (VAOn.val != 0)
        f->glDeleteVertexArrays(1, &VAOn.val);
    if (VBOvn.val != 0)
        f->glDeleteBuffers(1, &VBOvn.val);
    VBOv.val = 0;
    VBOn.val = 0;
    VBOf.val = 0;
    VBOc.val = 0;
    VBOt.val = 0;
    VBOtan.val = 0;
    VAO.val = 0;
    VAObb.val = 0;
    VBOfbb.val = 0;
    VBOvbb.val = 0;
    VAOn.val = 0;
    VBOvn.val = 0;
}

unsigned int TriangleMesh::draw(RenderState &state)
{
    if (!boundingBoxIsVisible(state))
        return 0;
    if (VAO.val == 0)
        return 0;
    if (withBB || withNormals)
    {
        GLuint formerProgram = state.getCurrentProgram();
        state.switchToStandardProgram();
        if (withBB)
            drawBB(state);
        if (withNormals)
            drawNormals(state);
        state.setCurrentProgram(formerProgram);
    }
    drawVBO(state);

    return triangles.size();
}

void TriangleMesh::drawVBO(RenderState &state)
{
    auto *f = state.getOpenGLFunctions();

    // Bug in Qt: They flagged glVertexAttrib3f as deprecated in modern OpenGL, which is not true.
    // We have to load it manually. Make it static so we do it only once.
    static auto glVertexAttrib3fv = reinterpret_cast<glVertexAttrib3fvPtr>(QOpenGLContext::currentContext()->getProcAddress("glVertexAttrib3fv"));

    // The VAO keeps track of all the buffers and the element buffer, so we do not need to bind else except for the VAO
    f->glBindVertexArray(VAO.val);
    f->glUniformMatrix4fv(state.getModelViewUniform(), 1, GL_FALSE, state.getCurrentModelViewMatrix().data());
    f->glUniformMatrix3fv(state.getNormalMatrixUniform(), 1, GL_FALSE, state.calculateNormalMatrix().data());
    switch (coloringType)
    {
    case ColoringType::TEXTURE:
        if (textureID.val != 0)
        {
            f->glUniform1ui(state.getUseTextureUniform(), GL_TRUE);
            f->glActiveTexture(GL_TEXTURE0);
            f->glBindTexture(GL_TEXTURE_2D, textureID.val);
            f->glUniform1i(state.getTextureUniform(), 0);
            break;
        }
        //[[fallthrough]];

    case ColoringType::COLOR_ARRAY:
        if (VBOc.val != 0)
        {
            f->glUniform1ui(state.getUseTextureUniform(), GL_FALSE);
            f->glEnableVertexAttribArray(COLOR_LOCATION);
            break;
        }
        //[[fallthrough]];

    case ColoringType::STATIC_COLOR:
        f->glUniform1ui(state.getUseTextureUniform(), GL_FALSE);
        f->glDisableVertexAttribArray(COLOR_LOCATION); // By disabling the attribute array, it uses the value set in the following line.
        glVertexAttrib3fv(2, reinterpret_cast<const GLfloat *>(&staticColor));
        break;

    case ColoringType::BUMP_MAPPING:
        // Use static color as base color.
        f->glDisableVertexAttribArray(COLOR_LOCATION);
        glVertexAttrib3fv(2, reinterpret_cast<const GLfloat *>(&staticColor));

        GLint location;
        auto program = state.getCurrentProgram();

        location = f->glGetUniformLocation(program, "useDiffuse");
        f->glUniform1ui(location, enableDiffuseTexture);

        location = f->glGetUniformLocation(program, "useNormal");
        f->glUniform1ui(location, enableNormalMapping);

        location = f->glGetUniformLocation(program, "useDisplacement");
        f->glUniform1ui(location, enableDisplacementMapping);

        location = f->glGetUniformLocation(program, "diffuseTexture");
        f->glUniform1i(location, 0);
        f->glActiveTexture(GL_TEXTURE0);
        f->glBindTexture(GL_TEXTURE_2D, textureID.val);

        location = f->glGetUniformLocation(program, "normalTexture");
        f->glUniform1i(location, 1);
        f->glActiveTexture(GL_TEXTURE1);
        f->glBindTexture(GL_TEXTURE_2D, normalMapID.val);

        location = f->glGetUniformLocation(program, "displacementTexture");
        f->glUniform1i(location, 3);
        f->glActiveTexture(GL_TEXTURE3);
        f->glBindTexture(GL_TEXTURE_2D, displacementMapID.val);
        break;
    }
    f->glDrawElements(GL_TRIANGLES, 3 * triangles.size(), GL_UNSIGNED_INT, nullptr);
}

// ===========
// === VFC ===
// ===========

bool TriangleMesh::boundingBoxIsVisible(const RenderState &state)
{

    QMatrix4x4 mvMatrix = state.getCurrentModelViewMatrix();
    QMatrix4x4 projMatrix = state.getCurrentProjectionMatrix();
    QMatrix4x4 clipMatrix = projMatrix * mvMatrix;

    struct Plane
    {
        float a, b, c, d;
        void normalize()
        {
            float length = std::sqrt(a * a + b * b + c * c);
            a /= length;
            b /= length;
            c /= length;
            d /= length;
        }
    };

    Plane planes[6];
    const float *m = clipMatrix.data();
    planes[0] = {m[3] - m[0], m[7] - m[4], m[11] - m[8], m[15] - m[12]};
    planes[1] = {m[3] + m[0], m[7] + m[4], m[11] + m[8], m[15] + m[12]};
    planes[2] = {m[3] + m[1], m[7] + m[5], m[11] + m[9], m[15] + m[13]};
    planes[3] = {m[3] - m[1], m[7] - m[5], m[11] - m[9], m[15] - m[13]};
    planes[4] = {m[3] + m[2], m[7] + m[6], m[11] + m[10], m[15] + m[14]};
    planes[5] = {m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14]};

    // Normalize all planes
    for (auto &plane : planes)
    {
        plane.normalize();
    }

    // for (int i = 0; i < 6; ++i)
    // {
    //     // planes[i].normalize();
    //     planes[i] = {-planes[i].a, -planes[i].b, -planes[i].c, -planes[i].d}; // Flip the normal direction
    // }

    Vec3f minVec = getBoundingBoxMin();
    Vec3f maxVec = getBoundingBoxMax();

    QVector3D min(minVec.x(), minVec.y(), minVec.z());
    QVector3D max(maxVec.x(), maxVec.y(), maxVec.z());

    QVector3D corners[8] = {
        {min.x(), min.y(), min.z()}, {max.x(), min.y(), min.z()}, {min.x(), max.y(), min.z()}, {max.x(), max.y(), min.z()}, {min.x(), min.y(), max.z()}, {max.x(), min.y(), max.z()}, {min.x(), max.y(), max.z()}, {max.x(), max.y(), max.z()}};

    // Check if the bounding box is inside the frustum
    for (const auto &plane : planes)
    {
        bool allOutside = true;

        // Test all 8 corners against the current plane
        for (const auto &corner : corners)
        {
            if (plane.a * corner.x() + plane.b * corner.y() + plane.c * corner.z() + plane.d >= 0)
            {
                allOutside = false; // At least one corner is inside
                break;
            }
        }

        if (allOutside)
        {
            return false;
        }
    }

    return true; 
}

void TriangleMesh::setStaticColor(Vec3f color)
{
    staticColor = color;
}

void TriangleMesh::drawBB(RenderState &state)
{
    auto *f = state.getOpenGLFunctions();
    f->glBindVertexArray(VAObb.val);
    // Transform BB to correct position.
    state.pushModelViewMatrix();
    state.getCurrentModelViewMatrix().translate(boundingBoxMid.x(), boundingBoxMid.y(), boundingBoxMid.z());
    state.getCurrentModelViewMatrix().scale(boundingBoxSize.x(), boundingBoxSize.y(), boundingBoxSize.z());
    f->glUniformMatrix4fv(state.getModelViewUniform(), 1, GL_FALSE, state.getCurrentModelViewMatrix().data());
    // Set color to constant white.
    // Bug in Qt: They flagged glVertexAttrib3f as deprecated in modern OpenGL, which is not true.
    // We have to load it manually. Make it static so we do it only once.
    static auto glVertexAttrib3f = reinterpret_cast<glVertexAttrib3fPtr>(QOpenGLContext::currentContext()->getProcAddress("glVertexAttrib3f"));
    glVertexAttrib3f(2, 1.0f, 1.0f, 1.0f);

    f->glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, nullptr);
    state.popModelViewMatrix();
}

void TriangleMesh::drawNormals(RenderState &state)
{
    auto *f = state.getOpenGLFunctions();
    f->glBindVertexArray(VAOn.val);
    f->glUniformMatrix4fv(state.getModelViewUniform(), 1, GL_FALSE, state.getCurrentModelViewMatrix().data());

    // Set color to constant white.
    // Bug in Qt: They flagged glVertexAttrib3f as deprecated in modern OpenGL, which is not true.
    // We have to load it manually. Make it static so we do it only once.
    static auto glVertexAttrib3f = reinterpret_cast<glVertexAttrib3fPtr>(QOpenGLContext::currentContext()->getProcAddress("glVertexAttrib3f"));
    glVertexAttrib3f(2, 1.0f, 1.0f, 1.0f);

    f->glDrawArrays(GL_LINES, 0, vertices.size() * 2);
}

void TriangleMesh::generateSphere(QOpenGLFunctions_3_3_Core *f)
{
    // The sphere consists of latdiv rings of longdiv faces.
    int longdiv = 200; // minimum 4
    int latdiv = 100;  // minimum 2

    setGLFunctionPtr(f);

    // Generate vertices.
    for (int latitude = 0; latitude <= latdiv; latitude++)
    {
        float v = static_cast<float>(latitude) / static_cast<float>(latdiv);
        float latangle = v * M_PI;

        float extent = std::sin(latangle);
        float y = -std::cos(latangle);

        for (int longitude = 0; longitude <= longdiv; longitude++)
        {
            float u = static_cast<float>(longitude) / static_cast<float>(longdiv);
            float longangle = u * 2.0f * M_PI;

            float z = std::sin(longangle) * extent;
            float x = std::cos(longangle) * extent;

            Vec3f pos(x, y, z);

            vertices.push_back(pos);
            normals.push_back(pos);
            texCoords.push_back({2.0f - 2.0f * u, v});
            tangents.push_back(cross(Vec3f(0, 1, 0), pos));
        }
    }

    for (int latitude = 0; latitude < latdiv; latitude++)
    {
        unsigned int bottomBase = latitude * (longdiv + 1);
        unsigned int topBase = (latitude + 1) * (longdiv + 1);
        for (int longitude = 0; longitude < longdiv; longitude++)
        {
            unsigned int bottomCurrent = bottomBase + longitude;
            unsigned int bottomNext = bottomBase + (longitude + 1);
            unsigned int topCurrent = topBase + longitude;
            unsigned int topNext = topBase + (longitude + 1);
            triangles.emplace_back(bottomCurrent, bottomNext, topNext);
            triangles.emplace_back(topNext, topCurrent, bottomCurrent);
        }
    }

    boundingBoxMid = Vec3f(0, 0, 0);
    boundingBoxSize = Vec3f(2, 2, 2);
    boundingBoxMin = Vec3f(-1, -1, -1);
    boundingBoxMax = Vec3f(1, 1, 1);

    createAllVBOs();
}

void TriangleMesh::generateTerrain(unsigned int h, unsigned int w, unsigned int iterations)
{
    // TODO(3.1): Implement terrain generation.

    // Diamond-Square Algorithm:
    // https://janert.me/blog/2022/the-diamond-square-algorithm-for-terrain-generation/
    // https://medium.com/@nickobrien/diamond-square-algorithm-explanation-and-c-implementation-5efa891e486f
    // https://en.wikipedia.org/wiki/Diamond-square_algorithm

    // 1) Clear any old data.
    clear();

    // 2) Allocate a 2D heightmap of size (w+1) x (h+1).
    std::vector<std::vector<float>> heightmap(w + 1, std::vector<float>(h + 1, 0.0f));

    // 3) Initialize corners with random seeds.
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 5.0f); //range, corner heights of map
    heightmap[0][0] = dist(gen);
    heightmap[w][0] = dist(gen);
    heightmap[0][h] = dist(gen);
    heightmap[w][h] = dist(gen);

    // 4) Diamond-Square algorithm:
    //    The 'stepSize' is current subdivision size; it is halved each iteration.
    //    The 'roughness' controls how wild the random additions are each iteration.
    float roughness = 3.0f;
    int stepSize    = std::max(w, h);

    while (stepSize > 1) {
        int halfStep = stepSize / 2;

        // Diamond step
        for (int x = halfStep; x < (int)w; x += stepSize) {
            for (int z = halfStep; z < (int)h; z += stepSize) {
                float a = heightmap[x - halfStep][z - halfStep];
                float b = heightmap[x + halfStep][z - halfStep];
                float c = heightmap[x - halfStep][z + halfStep];
                float d = heightmap[x + halfStep][z + halfStep];
                float avg = (a + b + c + d) * 0.25f;

                float offset = dist(gen) * roughness - roughness * 0.5f;
                heightmap[x][z] = avg + offset;
            }
        }

        // Square step
        for (int x = 0; x <= (int)w; x += halfStep) {
            for (int z = ((x / halfStep) % 2 == 0) ? halfStep : 0; z <= (int)h; z += stepSize) {
                float sum     = 0.0f;
                int   count   = 0;
                if ((x - halfStep) >= 0 && (z - halfStep) >= 0) {
                    sum += heightmap[x - halfStep][z - halfStep];
                    ++count;
                }
                if ((x + halfStep) <= (int)w && (z - halfStep) >= 0) {
                    sum += heightmap[x + halfStep][z - halfStep];
                    ++count;
                }
                if ((x - halfStep) >= 0 && (z + halfStep) <= (int)h) {
                    sum += heightmap[x - halfStep][z + halfStep];
                    ++count;
                }
                if ((x + halfStep) <= (int)w && (z + halfStep) <= (int)h) {
                    sum += heightmap[x + halfStep][z + halfStep];
                    ++count;
                }
                float avg = (count > 0) ? sum / count : 0.0f;

                float offset = dist(gen) * roughness - roughness * 0.5f;
                heightmap[x][z] = avg + offset;
            }
        }

        // Halve the step size and reduce roughness
        stepSize  /= 2;
        roughness *= 0.5f;
    }

    // 5) Build the mesh from the heightmap
    //    for each grid cell create 2 triangles
    //    (w+1)*(h+1) vertices in total
    vertices.reserve((w+1)*(h+1));
    normals.reserve((w+1)*(h+1));
    colors.reserve((w+1)*(h+1));

    // prepare index buffer for triangles
    triangles.reserve(w * h * 2);

    // for color calculation:
    auto computeColor = [&](float heightValue) -> Vec3f {
        // clamp height for safety
        heightValue = std::clamp(heightValue, 0.0f, 10.0f);

        // example coloring (very rough):
        // 0 - 1.5: water (blue)
        // 1.5 - 2.5: sand (brownish)
        // 2.5 - 4.0: grass (green)
        // 4.0 - 6.0: rock (grey)
        // 6.0+ : snow (white)
        if (heightValue < 1.5f) return Vec3f(0.0f, 0.0f, 1.0f);
        if (heightValue < 2.5f) return Vec3f(0.5f, 0.35f, 0.05f);
        if (heightValue < 4.0f) return Vec3f(0.0f, 0.7f, 0.0f);
        if (heightValue < 6.0f) return Vec3f(0.5f, 0.5f, 0.5f);
        return Vec3f(1.0f, 1.0f, 1.0f);
    };

    // Loop through points in heightmap
    for (int z = 0; z <= (int)h; ++z) {
        for (int x = 0; x <= (int)w; ++x) {
            float y = heightmap[x][z];
            vertices.push_back(Vec3f(static_cast<float>(x), y, static_cast<float>(z)));
            // fill placeholder normal , refine later (calculateNormalsByArea).
            normals.push_back(Vec3f(0.0f, 1.0f, 0.0f));
            // Per-vertex color based on height:
            Vec3f c = computeColor(y);
            colors.push_back(c);
        }
    }

    // Create triangles: for each cell (x,z)two triangles
    for (int z = 0; z < (int)h; ++z) {
        for (int x = 0; x < (int)w; ++x) {
            // Indices in the vertex array:
            unsigned int i0 = z * (w + 1) + x;
            unsigned int i1 = i0 + 1;
            unsigned int i2 = (z + 1) * (w + 1) + x;
            unsigned int i3 = i2 + 1;

            triangles.emplace_back(i0, i2, i1);

            triangles.emplace_back(i1, i2, i3);
        }
    }

    // 6) recalculate the normals from new triangles
    calculateNormalsByArea();
    calculateBB(); // bounding box

    // 7) Upload to GPU
    createAllVBOs();

    //vertices.reserve(4);
    //vertices.emplace_back(0, 0, 0);
    //vertices.emplace_back(0, 0, 10);
    //vertices.emplace_back(10, 0, 10);
    //vertices.emplace_back(10, 0, 0);

    //triangles.reserve(2);
    //triangles.emplace_back(0, 1, 2);
    //triangles.emplace_back(0, 2, 3);

    //calculateNormalsByArea();
    //calculateBB();
    //createAllVBOs();
}



