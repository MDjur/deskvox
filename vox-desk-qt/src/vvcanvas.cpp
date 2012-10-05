// Virvo - Virtual Reality Volume Rendering
// Copyright (C) 1999-2003 University of Stuttgart, 2004-2005 Brown University
// Contact: Jurgen P. Schulze, jschulze@ucsd.edu
//
// This file is part of Virvo.
//
// Virvo is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library (see license.txt); if not, write to the
// Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

#include <GL/glew.h>

#include "vvcanvas.h"

#include <virvo/vvdebugmsg.h>
#include <virvo/vvfileio.h>
#include <virvo/vvgltools.h>

#include <QSettings>
#include <QTimer>

#include <iostream>

using vox::vvObjView;

vvCanvas::vvCanvas(const QGLFormat& format, const QString& filename, QWidget* parent)
  : QGLWidget(format, parent)
  , _vd(NULL)
  , _renderer(NULL)
  , _projectionType(vox::vvObjView::PERSPECTIVE)
  , _doubleBuffering(format.doubleBuffer())
  , _superSamples(format.samples())
  , _stillQuality(1.0f)
  , _movingQuality(1.0f)
{
  vvDebugMsg::msg(1, "vvCanvas::vvCanvas()");

  if (filename != "")
  {
    _vd = new vvVolDesc(filename.toStdString().c_str());
  }
  else
  {
    // load default volume
    _vd = new vvVolDesc;
    _vd->vox[0] = 32;
    _vd->vox[1] = 32;
    _vd->vox[2] = 32;
    _vd->frames = 0;
  }

  // init ui
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);

  // read persistent settings
  QSettings settings;
  QColor qcolor = settings.value("canvas/bgcolor").value<QColor>();
  _bgColor = vvColor(qcolor.redF(), qcolor.greenF(), qcolor.blueF());

  _animTimer = new QTimer(this);
  connect(_animTimer, SIGNAL(timeout()), this, SLOT(incTimeStep()));
}

vvCanvas::~vvCanvas()
{
  vvDebugMsg::msg(1, "vvCanvas::~vvCanvas()");

  delete _renderer;
  delete _vd;
}

void vvCanvas::setVolDesc(vvVolDesc* vd)
{
  vvDebugMsg::msg(3, "vvCanvas::setVolDesc()");

  delete _vd;
  _vd = vd;

  if (_vd != NULL)
  {
    createRenderer();
  }

  foreach (vvPlugin* plugin, _plugins)
  {
    plugin->setVolDesc(_vd);
  }

  std::string str;
  _vd->makeInfoString(&str);
  emit statusMessage(str);
  emit newVolDesc(_vd);
}

void vvCanvas::setPlugins(const QList<vvPlugin*>& plugins)
{
  vvDebugMsg::msg(3, "vvCanvas::setPlugins()");

  _plugins = plugins;
}

vvVolDesc* vvCanvas::getVolDesc() const
{
  vvDebugMsg::msg(3, "vvCanvas::getVolDesc()");

  return _vd;
}

vvRenderer* vvCanvas::getRenderer() const
{
  vvDebugMsg::msg(3, "vvCanvas::getRenderer()");

  return _renderer;
}

void vvCanvas::loadCamera(const QString& filename)
{
  vvDebugMsg::msg(3, "vvCanvas::loadCamera()");

  QByteArray ba = filename.toLatin1();
  _ov.loadCamera(ba.data());
}

void vvCanvas::saveCamera(const QString& filename)
{
  vvDebugMsg::msg(3, "vvCanvas::saveCamera()");

  QByteArray ba = filename.toLatin1();
  _ov.saveCamera(ba.data());
}

void vvCanvas::initializeGL()
{
  vvDebugMsg::msg(1, "vvCanvas::initializeGL()");

  glewInit();
  init();
}

void vvCanvas::paintGL()
{
  vvDebugMsg::msg(3, "vvCanvas::paintGL()");

  if (_renderer == NULL)
  {
    return;
  }

  if (_doubleBuffering)
  {
    glDrawBuffer(GL_BACK);
  }
  else
  {
    glDrawBuffer(GL_FRONT);
  }

  glClearColor(_bgColor[0], _bgColor[1], _bgColor[2], 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_MODELVIEW);
  _ov.setModelviewMatrix(vvObjView::CENTER);

  foreach (vvPlugin* plugin, _plugins)
  {
    if (plugin->isActive())
    {
      plugin->prerender();
    }
  }

  _renderer->renderVolumeGL();

  foreach (vvPlugin* plugin, _plugins)
  {
    if (plugin->isActive())
    {
      plugin->postrender();
    }
  }
}

void vvCanvas::resizeGL(const int w, const int h)
{
  vvDebugMsg::msg(3, "vvCanvas::resizeGL()");

  glViewport(0, 0, w, h);
  if (h > 0)
  {
    _ov.setAspectRatio(static_cast<float>(w) / static_cast<float>(h));
  }
  updateGL();

  emit resized(QSize(w, h));
}

void vvCanvas::mouseMoveEvent(QMouseEvent* event)
{
  vvDebugMsg::msg(3, "vvCanvas::mouseMoveEvent()");

  switch (_mouseButton)
  {
  case Qt::LeftButton:
  {
    _ov._camera.trackballRotation(width(), height(),
      _lastMousePos.x(), _lastMousePos.y(),
      event->pos().x(), event->pos().y());
    break;
  }
  case Qt::MiddleButton:
  {
    const float pixelInWorld = _ov.getViewportWidth() / static_cast<float>(width());
    const float dx = static_cast<float>(event->pos().x() - _lastMousePos.x());
    const float dy = static_cast<float>(event->pos().y() - _lastMousePos.y());
    vvVector2f pan(pixelInWorld * dx, pixelInWorld * dy);
    _ov._camera.translate(pan[0], -pan[1], 0.0f);
    break;
  }
  case Qt::RightButton:
  {
    const float factor = event->pos().y() - _lastMousePos.y();
    _ov._camera.translate(0.0f, 0.0f, factor);
    break;
  }
  default:
    break;
  }
  _lastMousePos = event->pos();
  updateGL();
}

void vvCanvas::mousePressEvent(QMouseEvent* event)
{
  vvDebugMsg::msg(3, "vvCanvas::mousePressEvent()");

  _stillQuality = _renderer->getParameter(vvRenderer::VV_QUALITY);
  _renderer->setParameter(vvRenderer::VV_QUALITY, _movingQuality);
  _mouseButton = event->button();
  _lastMousePos = event->pos();
}

void vvCanvas::mouseReleaseEvent(QMouseEvent*)
{
  vvDebugMsg::msg(3, "vvCanvas::mouseReleaseEvent()");

  _mouseButton = Qt::NoButton;
  _renderer->setParameter(vvRenderer::VV_QUALITY, _stillQuality);
  updateGL();
}

void vvCanvas::init()
{
  vvDebugMsg::msg(3, "vvCanvas::init()");

  
  vvFileIO* fio = new vvFileIO;
  fio->loadVolumeData(_vd, vvFileIO::ALL_DATA);
  delete fio;

  // default transfer function
  if (_vd->tf.isEmpty())
  {
    _vd->tf.setDefaultAlpha(0, _vd->real[0], _vd->real[1]);
    _vd->tf.setDefaultColors((_vd->chan == 1) ? 0 : 3, _vd->real[0], _vd->real[1]);
  }

  // init renderer
  if (_vd != NULL)
  {
    _currentRenderer = "viewport";
    _currentOptions["voxeltype"] = "arb";
    createRenderer();
  }

  updateProjection();

  foreach (vvPlugin* plugin, _plugins)
  {
    plugin->setVolDesc(_vd);
  }

  emit newVolDesc(_vd);
}

void vvCanvas::createRenderer()
{
  vvDebugMsg::msg(3, "vvCanvas::createRenderer()");

  vvRenderState state;
  if (_renderer)
  {
    state = *_renderer;
    delete _renderer;
  }

  const float DEFAULT_OBJ_SIZE = 0.6f;
  _vd->resizeEdgeMax(_ov.getViewportWidth() * DEFAULT_OBJ_SIZE);

  vvRendererFactory::Options opt(_currentOptions);
  _renderer = vvRendererFactory::create(_vd, state, _currentRenderer.c_str(), opt);
}

void vvCanvas::updateProjection()
{
  vvDebugMsg::msg(3, "vvCanvas::updateProjection()");

  if (_projectionType == vvObjView::PERSPECTIVE)
  {
    _ov.setProjection(vvObjView::PERSPECTIVE, vvObjView::DEF_FOV, vvObjView::DEF_CLIP_NEAR, vvObjView::DEF_CLIP_FAR);
  }
  else if (_projectionType == vvObjView::ORTHO)
  {
    _ov.setProjection(vvObjView::ORTHO, vvObjView::DEF_VIEWPORT_WIDTH, vvObjView::DEF_CLIP_NEAR, vvObjView::DEF_CLIP_FAR);
  }
}

void vvCanvas::setCurrentFrame(const int frame)
{
  vvDebugMsg::msg(3, "vvCanvas::setCurrentFrame()");

  _renderer->setCurrentFrame(frame);
  emit currentFrame(frame);

  // inform plugins of new frame
  foreach (vvPlugin* plugin, _plugins)
  {
    plugin->timestep();
  }

  updateGL();
}

void vvCanvas::setParameter(vvParameters::ParameterType param, const vvParam& value)
{
  vvDebugMsg::msg(3, "vvCanvas::setParameter()");

  switch (param)
  {
  case vvParameters::VV_BG_COLOR:
    _bgColor = value;
    break;
  case vvParameters::VV_DOUBLEBUFFERING:
    _doubleBuffering = value;
    break;
  case vvParameters::VV_MOVING_QUALITY:
    _movingQuality = value;
    break;
  case vvParameters::VV_SUPERSAMPLES:
    _superSamples = value;
    break;
  case vvParameters::VV_PROJECTIONTYPE:
    _projectionType = static_cast<vvObjView::ProjectionType>(value.asInt());
    updateProjection();
  default:
    break;
  }
  updateGL();
}

void vvCanvas::setParameter(vvRenderer::ParameterType param, const vvParam& value)
{
  vvDebugMsg::msg(3, "vvCanvas::setParameter()");

  if (_renderer != NULL)
  {
    _renderer->setParameter(param, value);
    updateGL();
  }
}

vvParam vvCanvas::getParameter(vvParameters::ParameterType param) const
{
  switch (param)
  {
  case vvParameters::VV_BG_COLOR:
    return _bgColor;
  case vvParameters::VV_DOUBLEBUFFERING:
    return _doubleBuffering;
  case vvParameters::VV_SUPERSAMPLES:
    return _superSamples;
  case vvParameters::VV_PROJECTIONTYPE:
    return static_cast<int>(_projectionType);
  default:
    return vvParam();
  }
}

vvParam vvCanvas::getParameter(vvRenderer::ParameterType param) const
{
  if (_renderer != NULL)
  {
    return _renderer->getParameter(param);
  }
  return vvParam();
}

void vvCanvas::startAnimation(const double fps)
{
  vvDebugMsg::msg(3, "vvCanvas::startAnimation()");

  _vd->dt = 1.0f / static_cast<float>(fps);
  const float delay = std::abs(_vd->dt * 1000.0f);
  _animTimer->start(static_cast<int>(delay));
}

void vvCanvas::stopAnimation()
{
  vvDebugMsg::msg(3, "vvCanvas::stopAnimation()");

  _animTimer->stop();
}

void vvCanvas::setTimeStep(const int step)
{
  vvDebugMsg::msg(3, "vvCanvas::setTimeStep()");

  int f = step;
  while (f < 0)
  {
    f += _vd->frames; 
  }

  while (f >= _vd->frames)
  {
    f -= _vd->frames;
  }

  setCurrentFrame(f);
}

void vvCanvas::incTimeStep()
{
  vvDebugMsg::msg(3, "vvCanvas::incTimeStep()");

  int f = _renderer->getCurrentFrame();
  f = (f >= _vd->frames - 1) ? 0 : f + 1;
  setCurrentFrame(f);
}

void vvCanvas::decTimeStep()
{
  vvDebugMsg::msg(3, "vvCanvas::decTimeStep()");

  int f = _renderer->getCurrentFrame();
  f = (f <= 0) ? _vd->frames - 1 : f - 1;
  setCurrentFrame(f);
}

void vvCanvas::firstTimeStep()
{
  vvDebugMsg::msg(3, "vvCanvas::firstTimeStep()");

  setCurrentFrame(0);
}

void vvCanvas::lastTimeStep()
{
  vvDebugMsg::msg(3, "vvCanvas::lastTimeStep()");

  setCurrentFrame(_vd->frames - 1);
}
