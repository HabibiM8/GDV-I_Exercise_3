// =========================================================================== //
// Authors: Daniel Rutz                                                        //
//                                                                             //
// GRIS - Graphisch Interaktive Systeme                                        //
// Technische Universität Darmstadt                                            //
// Fraunhoferstrasse 5                                                         //
// D-64283 Darmstadt, Germany                                                  //
//                                                                             //
// Content: Utility class for correct moving, and other misc helpers, SOLUTION //
// =========================================================================== //

#ifndef UTILITES_H
#define UTILITES_H

#include <utility>

#include <QOpenGLFunctions_3_3_Core>

/*
 * This struct makes sure that moving works correctly. For example, if you have:
 * struct A {
 *  GLuint VAO;
 * };
 * the VAO is a POD. When you move A, VAO will be moved by copying the value. This could lead to double-deleting GPU resources or deleting GPU resources in use by the move destination of A.
 * However, in the following case:
 * struct B {
 *  autoMoved<GLuint> VAO;
 * };
 * the default move-constructor of A shows the expected behaviour. The value inside autoMoved can be read and written by using the public val member (e.g. VAO.val = 5;). Furthermore, the value can be read using the () operator (e.g. VAO()).
 */
template<typename T>
struct autoMoved {
    T val;

    autoMoved() : val() {}
    explicit autoMoved(T val) : val(val) {}

    autoMoved(autoMoved&& other) noexcept : val(other.val) {
        other.val = T();
    }

    autoMoved& operator=(autoMoved&& other) noexcept {
        using std::swap;
        swap(other.val, val);
        return *this;
    }

    autoMoved(const autoMoved& other) = delete;
    autoMoved& operator=(const autoMoved& other) = delete;
    T operator()() { return val; }
};

extern const GLfloat BoxVertices[];
extern const size_t BoxVerticesSize;
extern const GLuint BoxLineIndices[];
extern const size_t BoxLineIndicesSize;
extern const GLuint BoxTriangleIndices[];
extern const size_t BoxTriangleIndicesSize;

//Automatically load a texture into a OpenGL Texture Object of type GL_TEXTURE_2D. Returns 0 on failure.
GLuint loadImageIntoTexture(QOpenGLFunctions_3_3_Core* f, const char* fileName, bool wrap = false);
//Automatically load six textures into a OpenGL Texture Object of type GL_TEXTURE_CUBE_MAP. Returns 0 on failure. The order of the textures is POS_X, NEG_X, POS_Y, NEG_Y, POS_Z, NEG_Z.
GLuint loadCubeMap(QOpenGLFunctions_3_3_Core* f, const char* fileName[6]);

#endif //UTILITES_H
