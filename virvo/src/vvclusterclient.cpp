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

#include "vvbsptreevisitors.h"
#include "vvgltools.h"
#include "vvclusterclient.h"
#include "vvtexrend.h"

vvClusterClient::vvClusterClient(std::vector<const char*>& slaveNames, std::vector<int>& slavePorts,
                                 std::vector<const char*>& slaveFileNames,
                                 const char* fileName)
  : vvRemoteClient(slaveNames, slavePorts, slaveFileNames, fileName)
{
  _threads = NULL;
  _threadData = NULL;
  _visitor = new vvSlaveVisitor();
}

vvClusterClient::~vvClusterClient()
{
  destroyThreads();
  delete _visitor;
}

vvRemoteClient::ErrorType vvClusterClient::setRenderer(vvRenderer* renderer)
{
  vvTexRend* texRend = dynamic_cast<vvTexRend*>(renderer);
  if (texRend == NULL)
  {
    cerr << "vvRenderMaster::setRenderer(): Renderer is no texture based renderer" << endl;
    return vvRemoteClient::VV_WRONG_RENDERER;
  }

  // This will build up the bsp tree of the master node.
  texRend->prepareDistributedRendering(_slaveNames.size());

  // Store a pointer to the bsp tree and set its visitor.
  _bspTree = texRend->getBspTree();
  _bspTree->setVisitor(_visitor);

  _renderer = texRend;

  // Distribute the bricks from the bsp tree
  std::vector<BrickList>** bricks = texRend->getBrickListsToDistribute();
  for (int s=0; s<_sockets.size(); ++s)
  {
    for (int f=0; f<texRend->getVolDesc()->frames; ++f)
    {
      switch (_sockets[s]->putBricks(bricks[s]->at(f)))
      {
      case vvSocket::VV_OK:
        cerr << "Brick outlines transferred successfully" << endl;
        break;
      default:
        cerr << "Unable to transfer brick outlines" << endl;
        return VV_SOCKET_ERROR;
      }
    }
  }
  return VV_OK;
}

vvRemoteClient::ErrorType vvClusterClient::render()
{
  vvTexRend* renderer = dynamic_cast<vvTexRend*>(_renderer);

  if (renderer == NULL)
  {
    cerr << "Renderer is no texture based renderer" << endl;
    return vvRemoteClient::VV_WRONG_RENDERER;
  }
  float matrixGL[16];

  vvMatrix pr;
  glGetFloatv(GL_PROJECTION_MATRIX, matrixGL);
  pr.set(matrixGL);

  vvMatrix mv;
  glGetFloatv(GL_MODELVIEW_MATRIX, matrixGL);
  mv.set(matrixGL);

  for (int s=0; s<_sockets.size(); ++s)
  {
    _sockets[s]->putCommReason(vvSocketIO::VV_MATRIX);
    _sockets[s]->putMatrix(&pr);
    _sockets[s]->putMatrix(&mv);
  }

  renderer->calcProjectedScreenRects();

  pthread_barrier_wait(&_barrier);

  glDrawBuffer(GL_BACK);
  glClearColor(_bgColor[0], _bgColor[1], _bgColor[2], 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Retrieve the eye position for bsp-tree traversal
  vvVector3 eye;
  _renderer->getEyePosition(&eye);
  vvMatrix invMV(&mv);
  invMV.invert();
  // This is a gl matrix ==> transpose.
  invMV.transpose();
  eye.multiply(&invMV);

  // Orthographic projection.
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();

  // Fix the proxy quad for the frame buffer texture.
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  // Setup compositing.
  glDisable(GL_CULL_FACE);
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_COLOR_MATERIAL);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  _bspTree->traverse(eye);

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  clearImages();

  return VV_OK;
}

void vvClusterClient::exit()
{
  for (int s=0; s<_sockets.size(); ++s)
  {
    _sockets[s]->putCommReason(vvSocketIO::VV_EXIT);
    delete _sockets[s];
  }
}

void vvClusterClient::resize(const int w, const int h)
{
  for (int s=0; s<_sockets.size(); ++s)
  {
    if (_sockets[s]->putCommReason(vvSocketIO::VV_RESIZE) == vvSocket::VV_OK)
    {
      _sockets[s]->putWinDims(w, h);
    }
  }
}

void vvClusterClient::setCurrentFrame(const int index)
{
  vvDebugMsg::msg(3, "vvRenderMaster::setCurrentFrame()");
  vvRemoteClient::setCurrentFrame(index);
  for (int s=0; s<_sockets.size(); ++s)
  {
    if (_sockets[s]->putCommReason(vvSocketIO::VV_CURRENT_FRAME) == vvSocket::VV_OK)
    {
      _sockets[s]->putInt32(index);
    }
  }
}

void vvClusterClient::setMipMode(const int mipMode)
{
  for (int s=0; s<_sockets.size(); ++s)
  {
    if (_sockets[s]->putCommReason(vvSocketIO::VV_MIPMODE) == vvSocket::VV_OK)
    {
      _sockets[s]->putInt32(mipMode);
    }
  }
}

void vvClusterClient::setObjectDirection(const vvVector3* od)
{
  vvDebugMsg::msg(3, "vvRenderMaster::setObjectDirection()");
  for (int s=0; s<_sockets.size(); ++s)
  {
    if (_sockets[s]->putCommReason(vvSocketIO::VV_OBJECT_DIRECTION) == vvSocket::VV_OK)
    {
      _sockets[s]->putVector3(*od);
    }
  }
}

void vvClusterClient::setViewingDirection(const vvVector3* vd)
{
  vvDebugMsg::msg(3, "vvRenderMaster::setViewingDirection()");
  for (int s=0; s<_sockets.size(); ++s)
  {
    if (_sockets[s]->putCommReason(vvSocketIO::VV_VIEWING_DIRECTION) == vvSocket::VV_OK)
    {
      _sockets[s]->putVector3(*vd);
    }
  }
}

void vvClusterClient::setPosition(const vvVector3* p)
{
  vvDebugMsg::msg(3, "vvRenderMaster::setPosition()");
  for (int s=0; s<_sockets.size(); ++s)
  {
    if (_sockets[s]->putCommReason(vvSocketIO::VV_POSITION) == vvSocket::VV_OK)
    {
      _sockets[s]->putVector3(*p);
    }
  }
}

void vvClusterClient::setROIEnable(const bool roiEnabled)
{
  vvDebugMsg::msg(1, "vvRenderMaster::setROIEnable()");
  for (int s=0; s<_sockets.size(); ++s)
  {
    if (_sockets[s]->putCommReason(vvSocketIO::VV_TOGGLE_ROI) == vvSocket::VV_OK)
    {
      _sockets[s]->putBool(roiEnabled);
    }
  }
}

void vvClusterClient::setProbePosition(const vvVector3* pos)
{
  vvDebugMsg::msg(1, "vvRenderMaster::setProbePosition()");
  for (int s=0; s<_sockets.size(); ++s)
  {
    if (_sockets[s]->putCommReason(vvSocketIO::VV_ROI_POSITION) == vvSocket::VV_OK)
    {
      _sockets[s]->putVector3(*pos);
    }
  }
}

void vvClusterClient::setProbeSize(const vvVector3* newSize)
{
  vvDebugMsg::msg(1, "vvRenderMaster::setProbeSize()");
  for (int s=0; s<_sockets.size(); ++s)
  {
    if (_sockets[s]->putCommReason(vvSocketIO::VV_ROI_SIZE) == vvSocket::VV_OK)
    {
      _sockets[s]->putVector3(*newSize);
    }
  }
}

void vvClusterClient::toggleBoundingBox()
{
  vvDebugMsg::msg(3, "vvRenderMaster::toggleBoundingBox()");
  for (int s=0; s<_sockets.size(); ++s)
  {
    _sockets[s]->putCommReason(vvSocketIO::VV_TOGGLE_BOUNDINGBOX);
  }
}

void vvClusterClient::updateTransferFunction(vvTransFunc& tf)
{
  vvDebugMsg::msg(1, "vvRenderMaster::updateTransferFunction()");
  for (int s=0; s<_sockets.size(); ++s)
  {
    if (_sockets[s]->putCommReason(vvSocketIO::VV_TRANSFER_FUNCTION) == vvSocket::VV_OK)
    {
      _sockets[s]->putTransferFunction(tf);
    }
  }
}

void vvClusterClient::setParameter(const vvRenderer::ParameterType param, const float newValue, const char*)
{
  vvDebugMsg::msg(3, "vvRenderMaster::setParameter()");
  switch (param)
  {
  case vvRenderer::VV_QUALITY:
    adjustQuality(newValue);
    break;
  case vvRenderer::VV_SLICEINT:
    setInterpolation((newValue != 0.0f));
    break;
  default:
    vvRemoteClient::setParameter(param, newValue);
    break;
  }
}

void vvClusterClient::adjustQuality(const float quality)
{
  for (int s=0; s<_sockets.size(); ++s)
  {
    if (_sockets[s]->putCommReason(vvSocketIO::VV_QUALITY) == vvSocket::VV_OK)
    {
      _sockets[s]->putFloat(quality);
    }
  }
}

void vvClusterClient::setInterpolation(const bool interpolation)
{
  vvDebugMsg::msg(3, "vvRenderMaster::setInterpolation()");
  for (int s=0; s<_sockets.size(); ++s)
  {
    if (_sockets[s]->putCommReason(vvSocketIO::VV_INTERPOLATION) == vvSocket::VV_OK)
    {
      _sockets[s]->putBool(interpolation);
    }
  }
}

void vvClusterClient::createThreads()
{
  _visitor->generateTextureIds(_sockets.size());
  _threadData = new ThreadArgs[_sockets.size()];
  _threads = new pthread_t[_sockets.size()];
  pthread_barrier_init(&_barrier, NULL, _sockets.size() + 1);
  for (int s=0; s<_sockets.size(); ++s)
  {
    _threadData[s].threadId = s;
    _threadData[s].clusterClient = this;
    _threadData[s].images = _images;
    pthread_create(&_threads[s], NULL, getImageFromSocket, (void*)&_threadData[s]);
  }
  _visitor->setImages(_images);
}

void vvClusterClient::destroyThreads()
{
  pthread_barrier_destroy(&_barrier);
  for (int s=0; s<_sockets.size(); ++s)
  {
    pthread_join(_threads[s], NULL);
  }
  delete[] _threads;
  delete[] _threadData;
  _threads = NULL;
  _threadData = NULL;
}

void* vvClusterClient::getImageFromSocket(void* threadargs)
{
  ThreadArgs* data = reinterpret_cast<ThreadArgs*>(threadargs);

  while (1)
  {
    vvImage* img = new vvImage();
    data->clusterClient->_sockets.at(data->threadId)->getImage(img);
    data->images->at(data->threadId) = img;

    pthread_barrier_wait(&data->clusterClient->_barrier);
  }
  pthread_exit(NULL);
}