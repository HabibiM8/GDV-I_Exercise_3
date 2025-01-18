// ========================================================================= //
// Authors: Daniel Ströter, Roman Getto, Matthias Bein                       //
//                                                                           //
// GRIS - Graphisch Interaktive Systeme                                      //
// Technische Universität Darmstadt                                          //
// Fraunhoferstrasse 5                                                       //
// D-64283 Darmstadt, Germany                                                //
//                                                                           //
// Content: shader functions                                                 //
// ========================================================================= //

#ifndef UEBUNG_03_SHADER_H
#define UEBUNG_03_SHADER_H

#include <iostream>      // cout
#include <string>

#include <QDebug>
#include <QtGlobal>
#include <QFile>
#include <QMessageBox>
#include <QString>

#include <QOpenGLFunctions_3_3_Core>

//Constants for shader locations
const GLuint POSITION_LOCATION = 0;
const GLuint NORMAL_LOCATION = 1;
const GLuint COLOR_LOCATION = 2;
const GLuint TEXCOORD_LOCATION = 3;
const GLuint TANGENT_LOCATION = 4;

GLint getProgramLogLength(QOpenGLFunctions_3_3_Core* f, GLuint obj);
GLint getShaderLogLength(QOpenGLFunctions_3_3_Core* f, GLuint obj);
std::vector<GLchar> getShaderInfoLogAsVector(QOpenGLFunctions_3_3_Core* f, GLuint obj);
QString getShaderInfoLogAsQString(QOpenGLFunctions_3_3_Core* f, GLuint obj);
void printShaderInfoLog(QOpenGLFunctions_3_3_Core* f, GLuint obj);
std::vector<GLchar> getProgramInfoLogAsVector(QOpenGLFunctions_3_3_Core* f, GLuint obj);
QString getProgramInfoLogAsQString(QOpenGLFunctions_3_3_Core* f, GLuint obj);
void printProgramInfoLog(QOpenGLFunctions_3_3_Core* f, GLuint obj);
GLuint compileShaders(QOpenGLFunctions_3_3_Core* f, const char* vertexShaderSrc, GLint vertexShaderSize, const char* fragmentShaderSrc, GLint fragmentShaderSize);
GLuint readShaders(QOpenGLFunctions_3_3_Core* f, const QString& vertexShaderPath, const QString& fragmentShaderPath);

#endif  //UEBUNG_03_SHADER_H
