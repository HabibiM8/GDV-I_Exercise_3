#include "shader.h"

GLint getProgramLogLength(QOpenGLFunctions_3_3_Core* f, GLuint obj) {
    GLint infologLength = 0;
    f->glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
    return infologLength;
}

GLint getShaderLogLength(QOpenGLFunctions_3_3_Core* f, GLuint obj) {
    GLint infologLength = 0;
    f->glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
    return infologLength;
}

std::vector<GLchar> getShaderInfoLogAsVector(QOpenGLFunctions_3_3_Core* f, GLuint obj) {
    GLint infologLength = getShaderLogLength(f, obj);
    std::vector<GLchar> result;
    
    if (infologLength > 0)
	{
        result.resize(infologLength);
		f->glGetShaderInfoLog(obj, infologLength, nullptr, result.data());
	}
	
	return result;
}

QString getShaderInfoLogAsQString(QOpenGLFunctions_3_3_Core* f, GLuint obj) {
    auto infoLog = getShaderInfoLogAsVector(f, obj);
    if (infoLog.empty()) return QString();
    else return QString(infoLog.data());
}

void printShaderInfoLog(QOpenGLFunctions_3_3_Core* f, GLuint obj)
{
    auto infoLog = getShaderInfoLogAsVector(f, obj);
    if (!infoLog.empty()) {
		std::cout << infoLog.data() << std::endl;
    }
}

std::vector<GLchar> getProgramInfoLogAsVector(QOpenGLFunctions_3_3_Core* f, GLuint obj) {
    GLint infologLength = getProgramLogLength(f, obj);
    std::vector<GLchar> result;
    
    if (infologLength > 0)
	{
        result.resize(infologLength);
		f->glGetProgramInfoLog(obj, infologLength, nullptr, result.data());
	}
	
	return result;
}

QString getProgramInfoLogAsQString(QOpenGLFunctions_3_3_Core* f, GLuint obj) {
    auto infoLog = getProgramInfoLogAsVector(f, obj);
    if (infoLog.empty()) return QString();
    else return QString(infoLog.data());
}

void printProgramInfoLog(QOpenGLFunctions_3_3_Core* f, GLuint obj)
{
    auto infoLog = getProgramInfoLogAsVector(f, obj);
    if (!infoLog.empty()) {
		std::cout << infoLog.data() << std::endl;
    }
}


GLuint compileShaders(QOpenGLFunctions_3_3_Core* f, const char* vertexShaderSrc, GLint vertexShaderSize, const char* fragmentShaderSrc, GLint fragmentShaderSize) {
    // create shaders, set source and compile
    GLuint vertexShader = f->glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = f->glCreateShader(GL_FRAGMENT_SHADER);
    f->glShaderSource(vertexShader, 1, &vertexShaderSrc, &vertexShaderSize);
    f->glShaderSource(fragmentShader, 1, &fragmentShaderSrc, &fragmentShaderSize);
    f->glCompileShader(vertexShader);
    f->glCompileShader(fragmentShader);

    // create a program, attach both shaders and link the program (shaders can be deleted now)
    GLuint program = f->glCreateProgram();
    f->glAttachShader(program, vertexShader);
    f->glAttachShader(program, fragmentShader);
    f->glLinkProgram(program);
    f->glDeleteShader(vertexShader);
    f->glDeleteShader(fragmentShader);

    // check if compilation was successful and return the programID
    GLint success = 0;
    f->glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        QMessageBox failureBox;
        failureBox.setIcon(QMessageBox::Critical);
        failureBox.setStandardButtons(QMessageBox::StandardButton::Ok);
        failureBox.setText(QObject::tr("Compilerfehler oder Linkerfehler. Der Shader wurde nicht geladen."));
        failureBox.setInformativeText(QObject::tr("Der eingelesene Quellcode, die Fehlerausgabe des Compilers und des Linkers sind in den Details beigelegt."));
        failureBox.setDetailedText(QStringLiteral(
            "===== Vertex Shader =====\n"
            "%1\n"
            "===== Vertex Shader Info Log =====\n"
            "%2\n"
            "===== Fragment Shader =====\n"
            "%3\n"
            "===== Fragment Shader Info Log =====\n"
            "%4\n"
            "===== Program Info Log =====\n"
            "%5\n")
            .arg(vertexShaderSrc,
                 getShaderInfoLogAsQString(f, vertexShader),
                 fragmentShaderSrc,
                 getShaderInfoLogAsQString(f, fragmentShader),
                 getProgramInfoLogAsQString(f, program))
        );
        
        failureBox.exec();
        
        f->glDeleteProgram(program);
        program = 0;
    }
    return program;
}

GLuint readShaders(QOpenGLFunctions_3_3_Core* f, const QString& vertexShaderPath, const QString& fragmentShaderPath) {
    QFile vertexShaderFile(vertexShaderPath);
    QFile fragmentShaderFile(fragmentShaderPath);

    //Open and read vertex shader file
    if (!vertexShaderFile.open(QFile::OpenModeFlag::ReadOnly)) {
        qDebug() << "readShaders(): could not open file " << vertexShaderPath;
        return 0;
    }

    const auto vertexShaderText = vertexShaderFile.readAll();

    //Open and read fragment shader file
    if (!fragmentShaderFile.open(QFile::OpenModeFlag::ReadOnly)) {
        qDebug() << "readShaders(): could not open file " << fragmentShaderPath;
        return 0;
    }

    const auto fragmentShaderText = fragmentShaderFile.readAll();

  return compileShaders(f, vertexShaderText.constData(), vertexShaderText.size(), fragmentShaderText.constData(), fragmentShaderText.size());
}
