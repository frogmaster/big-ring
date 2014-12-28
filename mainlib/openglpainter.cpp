#include "openglpainter.h"

#include <QtGui/QOpenGLContext>
#include <QtCore/QtMath>
extern "C" {
#include <gst/video/video-info.h>
}

OpenGLPainter::OpenGLPainter(QGLWidget* widget, QObject *parent) :
    QObject(parent), _widget(widget), _openGLInitialized(false), _currentSample(nullptr),
    _texturesInitialized(false), _vertexBuffer(QOpenGLBuffer::VertexBuffer), _pixelBuffer(QGLBuffer::PixelUnpackBuffer)
{
    Q_INIT_RESOURCE(shaders);
}

OpenGLPainter::~OpenGLPainter()
{
    if (_currentSample) {
        gst_sample_unref(_currentSample);
    }
}

void OpenGLPainter::loadPlaneTexturesFromPbo(int glTextureUnit, int textureUnit,
                                           int lineSize, int height, size_t offset)
{
    glActiveTexture(glTextureUnit);
    glBindTexture(GL_TEXTURE_RECTANGLE, textureUnit);

    _pixelBuffer.bind();

    if (_texturesInitialized) {
        glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, 0, lineSize, height,
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, (void*) offset);
    } else {
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_LUMINANCE, lineSize, height,
                     0,GL_LUMINANCE,GL_UNSIGNED_BYTE, (void*) offset);
    }

    _pixelBuffer.release();
    glTexParameteri(GL_TEXTURE_RECTANGLE,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
}

void OpenGLPainter::uploadTextures()
{
    GstBuffer* buffer = gst_sample_get_buffer(_currentSample);
    GstMapInfo mapInfo;
    if (gst_buffer_map(buffer, &mapInfo, GST_MAP_READ)) {
        for (int i = 0; i < _textureCount; ++i) {
            GLuint textureId;
            if (i == 0) {
                textureId = _yTextureId;
            } else if (i == 1) {
                textureId = _uTextureId;
            } else {
                textureId = _vTextureId;
            }
            glBindTexture(GL_TEXTURE_RECTANGLE, textureId);
            glTexImage2D(
                    GL_TEXTURE_RECTANGLE,
                    0,
                    GL_LUMINANCE,
                    _textureWidths[i],
                    _textureHeights[i],
                    0,
                    GL_LUMINANCE,
                    GL_UNSIGNED_BYTE,
                    mapInfo.data + _textureOffsets[i]);
            glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        gst_buffer_unmap(buffer, &mapInfo);
    }
}

void OpenGLPainter::paint(QPainter *painter, const QRectF &rect)
{
    if (!_openGLInitialized) {
        initializeOpenGL();
    }
    if (!_currentSample) {
        painter->fillRect(rect, Qt::black);
        return;
    }

    adjustPaintAreas(rect);

    // if these are enabled, we need to reenable them after beginNativePainting()
    // has been called, as they may get disabled
    bool stencilTestEnabled = glIsEnabled(GL_STENCIL_TEST);
    bool scissorTestEnabled = glIsEnabled(GL_SCISSOR_TEST);

    painter->beginNativePainting();

    if (stencilTestEnabled)
        glEnable(GL_STENCIL_TEST);
    if (scissorTestEnabled)
        glEnable(GL_SCISSOR_TEST);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    _program.bind();

    loadPlaneTexturesFromPbo(GL_TEXTURE0, _yTextureId, _textureWidths[0], _textureHeights[0], (size_t) 0);
    loadPlaneTexturesFromPbo(GL_TEXTURE1, _uTextureId, _textureWidths[1], _textureHeights[1], _textureOffsets[1]);
    loadPlaneTexturesFromPbo(GL_TEXTURE2, _vTextureId, _textureWidths[2], _textureHeights[2], _textureOffsets[2]);

    _texturesInitialized = true;

    // set the texture and vertex coordinates using VBOs.
    _textureCoordinatesBuffer.bind();
    glTexCoordPointer(2, GL_FLOAT, 0, 0);
    _textureCoordinatesBuffer.release();

    _vertexBuffer.bind();
    glVertexPointer(2, GL_FLOAT, 0, 0);
    _vertexBuffer.release();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, _yTextureId);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, _uTextureId);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_RECTANGLE, _vTextureId);
    glActiveTexture(GL_TEXTURE0);

    _program.setUniformValue("yTex", 0);
    _program.setUniformValue("uTex", 1);
    _program.setUniformValue("vTex", 2);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    _program.release();

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    painter->endNativePainting();
    painter->fillRect(_blackBar1, Qt::black);
    painter->fillRect(_blackBar2, Qt::black);
}



void OpenGLPainter::setCurrentSample(GstSample *sample)
{
    if (_currentSample) {
        gst_sample_unref(_currentSample);
    }
    _currentSample = sample;
    _currentSampleUploaded = false;
    if (!_openGLInitialized) {
        _widget->context()->makeCurrent();
        initializeOpenGL();
    }
    if (getSizeFromSample(_currentSample) != _sourceSize) {
        _sourceSizeDirty = true;
        _sourceSize = getSizeFromSample(_currentSample);
        initYuv420PTextureInfo();
    }
    int pboSize = _textureWidths[0] * _textureHeights[0] * (1.5);

    _pixelBuffer.bind();
    _pixelBuffer.allocate(pboSize);

    void* ptr = _pixelBuffer.map(QGLBuffer::WriteOnly);
    if(ptr) {
        GstBuffer* buffer = gst_sample_get_buffer(_currentSample);
        GstMapInfo mapInfo;
        if (gst_buffer_map(buffer, &mapInfo, GST_MAP_READ)) {
            // load all three planes
            memcpy(ptr, mapInfo.data, _textureWidths[0] * _textureHeights[0]);
            memcpy(ptr + _textureOffsets[1], mapInfo.data + _textureOffsets[1], _textureWidths[1] * _textureHeights[1]);
            memcpy(ptr + _textureOffsets[2], mapInfo.data + _textureOffsets[2], _textureWidths[2] * _textureHeights[2]);
            gst_buffer_unmap(buffer, &mapInfo);
        }
        _pixelBuffer.unmap();
    }
    _pixelBuffer.release();
}


QSizeF OpenGLPainter::getSizeFromSample(GstSample* sample)
{
    GstCaps* caps = gst_sample_get_caps(sample);
    GstVideoInfo videoInfo;
    gst_video_info_from_caps(&videoInfo, caps);
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    gint width, height;
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);

    return QSizeF(width, height);
}

/**
 * @brief compile the opengl shader program from the sources and link it, if the program is not yet linked.
 */
void OpenGLPainter::initializeOpenGL()
{
    Q_ASSERT_X(!_program.isLinked(), "initializeOpenGL", "OpenGL already initialized");

    _glFunctions = QGLFunctions(QGLContext::currentContext());
    if (!_program.addShaderFromSourceFile(QGLShader::Vertex, ":///shaders/vertexshader.glsl")) {
        qFatal("Unable to add vertex shader: %s", qPrintable(_program.log()));
    }
    if (!_program.addShaderFromSourceFile(QGLShader::Fragment, ":/shaders/fragmentshader.glsl")) {
        qFatal("Unable to add fragment shader: %s", qPrintable(_program.log()));
    }
    if (!_program.link()) {
        qFatal("Unable to link shader program: %s", qPrintable(_program.log()));
    }
    for (auto extension: QOpenGLContext::currentContext()->extensions()) {
        qDebug() << "extension" << extension;
    }
    QOpenGLContext* glContext = QOpenGLContext::currentContext();
    if (!_glFunctions.hasOpenGLFeature(QGLFunctions::NPOTTextures)) {
        qFatal("OpenGL needs to have support for 'Non power of two textures'");
    }
    if (!glContext->hasExtension("GL_ARB_pixel_buffer_object")) {
        qFatal("GL_ARB_pixel_buffer_object is missing");
    }
    if (!_glFunctions.hasOpenGLFeature(QGLFunctions::Buffers)) {
        qFatal("OpenGL needs to have support for vertex buffers");
    }

    qDebug() << "generating textures.";
    glGenTextures(1, &_yTextureId);
    glGenTextures(1, &_uTextureId);
    glGenTextures(1, &_vTextureId);

    _textureCoordinatesBuffer.create();
    _vertexBuffer.create();
    _pixelBuffer.create();

    _openGLInitialized = true;
}

void OpenGLPainter::adjustPaintAreas(const QRectF& targetRect)
{
    if (_sourceSizeDirty || targetRect != _targetRect) {
        _targetRect = targetRect;
        QSizeF videoSizeAdjusted = QSizeF(_sourceSize.width(), _sourceSize.height()).scaled(targetRect.size(), Qt::KeepAspectRatio);

        _videoRect = QRectF(QPointF(), videoSizeAdjusted);
        _videoRect.moveCenter(targetRect.center());

        QVector<GLfloat> vertexCoordinates =
        {
            GLfloat(_videoRect.left()), GLfloat(_videoRect.top()),
            GLfloat(_videoRect.right() + 1), GLfloat(_videoRect.top()),
            GLfloat(_videoRect.left()), GLfloat(_videoRect.bottom() + 1),
            GLfloat(_videoRect.right() + 1), GLfloat(_videoRect.bottom() + 1)
        };
        _vertexBuffer.bind();
        _vertexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
        _vertexBuffer.allocate(vertexCoordinates.data(), sizeof(GLfloat) * vertexCoordinates.size());
        _vertexBuffer.release();

        if (targetRect.left() == _videoRect.left()) {
            // black bars on top and bottom
            _blackBar1 = QRectF(targetRect.topLeft(), _videoRect.topRight());
            _blackBar2 = QRectF(_videoRect.bottomLeft(), _targetRect.bottomRight());
        } else {
            // black bars on the sidex
            _blackBar1 = QRectF(targetRect.topLeft(), _videoRect.bottomLeft());
            _blackBar2 = QRectF(_videoRect.topRight(), _targetRect.bottomRight());
        }
        _sourceSizeDirty = false;
    }
}

void OpenGLPainter::initYuv420PTextureInfo()
{
    int bytesPerLine = (_sourceSize.toSize().width() + 3) & ~3;
    int bytesPerLine2 = (_sourceSize.toSize().  width() / 2 + 3) & ~3;
    qDebug() << "bytes per line = " << bytesPerLine << bytesPerLine2;
    _textureCount = 3;
    _textureWidths[0] = bytesPerLine;
    _textureHeights[0] = _sourceSize.height();
    _textureOffsets[0] = 0;
    _textureWidths[1] = bytesPerLine2;
    _textureHeights[1] = _sourceSize.height() / 2;
    _textureOffsets[1] = bytesPerLine * _sourceSize.height();
    _textureWidths[2] = bytesPerLine2;
    _textureHeights[2] = _sourceSize.height() / 2;
    _textureOffsets[2] = bytesPerLine * _sourceSize.height() + bytesPerLine2 * _sourceSize.height()/2;

    QVector<GLfloat> textureCoordinates = {
        0, 0,
        static_cast<GLfloat>(_sourceSize.width()), 0,
        0, static_cast<GLfloat>(_sourceSize.height()),
        static_cast<GLfloat>(_sourceSize.width()), static_cast<GLfloat>(_sourceSize.height())
    };

    _textureCoordinatesBuffer.bind();
    _textureCoordinatesBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    _textureCoordinatesBuffer.allocate(textureCoordinates.data(), sizeof(GLfloat) * textureCoordinates.size());
    _textureCoordinatesBuffer.release();
}

