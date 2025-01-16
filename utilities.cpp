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

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <QOpenGLFunctions_3_3_Core>

#include "utilities.h"

const GLfloat BoxVertices[] = {
        -0.5f, -0.5f, 0.5f,
        0.5f, -0.5f, 0.5f,
        0.5f, 0.5f, 0.5f,
        -0.5f, 0.5f, 0.5f,
        -0.5f, -0.5f, -0.5f,
        0.5f, -0.5f, -0.5f,
        0.5f, 0.5f, -0.5f,
        -0.5f, 0.5f, -0.5f,
};

const size_t BoxVerticesSize = sizeof(BoxVertices);

const GLuint BoxLineIndices[] = {
        0, 1,
        1, 2,
        2, 3,
        3, 0,
        4, 5,
        5, 6,
        6, 7,
        7, 4,
        0, 4,
        1, 5,
        2, 6,
        3, 7,
};

const size_t BoxLineIndicesSize = sizeof(BoxLineIndices);

const GLuint BoxTriangleIndices[] = {
        0, 1, 3,
        1, 2, 3,
        1, 5, 2,
        5, 6, 2,
        5, 4, 7,
        5, 7, 6,
        4, 0, 7,
        0, 3, 7,
        3, 2, 6,
        6, 7, 3,
        0, 1, 5,
        5, 4, 0,
};

const size_t BoxTriangleIndicesSize = sizeof(BoxTriangleIndices);

GLuint loadImageIntoTexture(QOpenGLFunctions_3_3_Core* f, const char* fileName, bool wrap) {
    //flip all images on load because origin of OpenGL textures is at lower left, not upper left
    stbi_set_flip_vertically_on_load(true);

    int width, height, temp;
    unsigned char* pixelData = stbi_load(fileName, &width, &height, &temp, 3);
    if (!pixelData) return 0;
    GLuint result;

    f->glGenTextures(1, &result);
    f->glBindTexture(GL_TEXTURE_2D, result);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    f->glTexImage2D(GL_TEXTURE_2D,      //Current texture binding
                    0,             //level 0, this refers to the mipmapping level
                    GL_RGB,             //internal OpenGL format
                    width,              //texture width
                    height,             //texture height
                    0,           //MUST be 0
                    GL_RGB,             //format of the provided texture
                    GL_UNSIGNED_BYTE,   //type of the pixels
                    pixelData           //pointer to pixels
    );
    f->glGenerateMipmap(GL_TEXTURE_2D);
    f->glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixelData);
    return result;
}

GLuint loadCubeMap(QOpenGLFunctions_3_3_Core* f, const char* fileName[6]) {
    //For whatever reason, cubemaps are not flipped per standard.
    stbi_set_flip_vertically_on_load(false);

    int width, height, temp;
    GLuint result;

    f->glGenTextures(1, &result);
    f->glBindTexture(GL_TEXTURE_CUBE_MAP, result);
    f->glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    for (GLenum target = GL_TEXTURE_CUBE_MAP_POSITIVE_X; target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; ++target) {
        unsigned char* pixelData = stbi_load(fileName[target - GL_TEXTURE_CUBE_MAP_POSITIVE_X], &width, &height, &temp, 3);
        if (!pixelData) return 0;
        f->glTexImage2D(target,      //Current texture binding
                        0,             //level 0, this refers to the mipmapping level
                        GL_RGB,             //internal OpenGL format
                        width,              //texture width
                        height,             //texture height
                        0,           //MUST be 0
                        GL_RGB,             //format of the provided texture
                        GL_UNSIGNED_BYTE,   //type of the pixels
                        pixelData           //pointer to pixels
        );
        stbi_image_free(pixelData);
    }

    return result;
}
