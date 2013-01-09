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


vvRayRend::vvRayRend(vvVolDesc* vd, vvRenderState renderState)
  : vvIbrRenderer(vd, renderState)
{
  vvDebugMsg::msg(1, "vvRayRend::vvRayRend()");

  glewInit();

  bool ok;

  // Free "cuda error cache".
  vvCudaTools::checkError(&ok, cudaGetLastError(), "vvRayRend::vvRayRend() - free cuda error cache");

  _volumeCopyToGpuOk = true;

  _earlyRayTermination = true;
  _illumination = false;
  _interpolation = true;
  _opacityCorrection = true;
  _twoPassIbr = (_ibrMode == VV_REL_THRESHOLD || _ibrMode == VV_EN_EX_MEAN);

  _rgbaTF = NULL;

  ::d_depth = NULL;
  intImg = new vvCudaImg(0, 0);
  allocIbrArrays(0, 0);

  const vvCudaImg::Mode mode = dynamic_cast<vvCudaImg*>(intImg)->getMode();
  if (mode == vvCudaImg::TEXTURE)
  {
    setWarpMode(CUDATEXTURE);
  }

  factorViewMatrix();

  initVolumeTexture();

  ::d_transferFuncArray = NULL;
  updateTransferFunction();
}

vvRayRend::~vvRayRend()
{
  vvDebugMsg::msg(1, "vvRayRend::~vvRayRend()");

  bool ok;
  for (size_t f = 0; f < ::d_volumeArrays.size(); ++f)
  {
    vvCudaTools::checkError(&ok, cudaFreeArray(::d_volumeArrays[f]),
                       "vvRayRend::~vvRayRend() - free volume frame");
  }

  vvCudaTools::checkError(&ok, cudaFreeArray(::d_transferFuncArray),
                     "vvRayRend::~vvRayRend() - free tf");
  vvCudaTools::checkError(&ok, cudaFree(::d_depth),
                     "vvRayRend::~vvRayRend() - free depth");

  delete[] _rgbaTF;
}

int vvRayRend::getLUTSize() const
{
   vvDebugMsg::msg(2, "vvRayRend::getLUTSize()");
   return (vd->getBPV()==2) ? 4096 : 256;
}

void vvRayRend::updateTransferFunction()
{
  vvDebugMsg::msg(3, "vvRayRend::updateTransferFunction()");

  bool ok;

  int lutEntries = getLUTSize();
  delete[] _rgbaTF;
  _rgbaTF = new float[4 * lutEntries];

  vd->computeTFTexture(lutEntries, 1, 1, _rgbaTF);

  cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<float4>();

  vvCudaTools::checkError(&ok, cudaFreeArray(::d_transferFuncArray),
                     "vvRayRend::updateTransferFunction() - free tf texture");
  vvCudaTools::checkError(&ok, cudaMallocArray(&::d_transferFuncArray, &channelDesc, lutEntries, 1),
                     "vvRayRend::updateTransferFunction() - malloc tf texture");
  vvCudaTools::checkError(&ok, cudaMemcpyToArray(::d_transferFuncArray, 0, 0, _rgbaTF, lutEntries * 4 * sizeof(float),
                                            cudaMemcpyHostToDevice),
                     "vvRayRend::updateTransferFunction() - copy tf texture to device");


  tfTexture.filterMode = cudaFilterModeLinear;
  tfTexture.normalized = true;    // access with normalized texture coordinates
  tfTexture.addressMode[0] = cudaAddressModeClamp;   // wrap texture coordinates

  vvCudaTools::checkError(&ok, cudaBindTextureToArray(tfTexture, ::d_transferFuncArray, channelDesc),
                     "vvRayRend::updateTransferFunction() - bind tf texture");
}

void vvRayRend::compositeVolume(int, int)
{
  vvDebugMsg::msg(3, "vvRayRend::compositeVolume()");

  bool ok;

  if(!_volumeCopyToGpuOk)
  {
    std::cerr << "vvRayRend::compositeVolume() aborted because of previous CUDA-Error" << std::endl;
    return;
  }
  vvDebugMsg::msg(1, "vvRayRend::compositeVolume()");

  const vvGLTools::Viewport vp = vvGLTools::getViewport();

  allocIbrArrays(vp[2], vp[3]);
  int w = vvToolshed::getTextureSize(vp[2]);
  int h = vvToolshed::getTextureSize(vp[3]);
  intImg->setSize(w, h);

  vvCudaImg* cudaImg = dynamic_cast<vvCudaImg*>(intImg);
  if (cudaImg == NULL)
  {
    vvDebugMsg::msg(0, "vvRayRend::compositeVolume() - cannot map CUDA image");
    return;
  }
  cudaImg->map();

  dim3 blockSize(16, 16);
  dim3 gridSize = dim3(vvToolshed::iDivUp(vp[2], blockSize.x), vvToolshed::iDivUp(vp[3], blockSize.y));
  const vvVector3 size(vd->getSize());

  vvVector3 probePosObj;
  vvVector3 probeSizeObj;
  vvVector3 probeMin;
  vvVector3 probeMax;
  calcProbeDims(probePosObj, probeSizeObj, probeMin, probeMax);

  vvVector3 clippedProbeSizeObj = probeSizeObj;
  for (int i=0; i<3; ++i)
  {
    if (clippedProbeSizeObj[i] < vd->getSize()[i])
    {
      clippedProbeSizeObj[i] = vd->getSize()[i];
    }
  }

  if (_isROIUsed && !_sphericalROI)
  {
    drawBoundingBox(probeSizeObj, _roiPos, _probeColor);
  }

  const float diagonalVoxels = sqrtf(float(vd->vox[0] * vd->vox[0] +
                                           vd->vox[1] * vd->vox[1] +
                                           vd->vox[2] * vd->vox[2]));
  int numSlices = max(1, static_cast<int>(_quality * diagonalVoxels));

  vvMatrix Mv, MvPr;
  vvGLTools::getModelviewMatrix(&Mv);
  vvGLTools::getProjectionMatrix(&MvPr);
  MvPr.multiplyRight(Mv);

  float* mvprM = new float[16];
  MvPr.get(mvprM);
  cudaMemcpyToSymbol(c_MvPrMatrix, mvprM, sizeof(float4) * 4);

  vvMatrix invMv;
  invMv = vvMatrix(Mv);
  invMv.invert();

  vvMatrix pr;
  vvGLTools::getProjectionMatrix(&pr);

  vvMatrix invMvpr;
  vvGLTools::getModelviewMatrix(&invMvpr);
  invMvpr.multiplyLeft(pr);
  invMvpr.invert();

  float* viewM = new float[16];
  invMvpr.get(viewM);
  cudaMemcpyToSymbol(c_invViewMatrix, viewM, sizeof(float4) * 4);
  delete[] viewM;

  const float3 volPos = make_float3(vd->pos[0], vd->pos[1], vd->pos[2]);
  float3 probePos = volPos;
  if (_isROIUsed && !_sphericalROI)
  {
    probePos = make_float3(probePosObj[0],  probePosObj[1], probePosObj[2]);
  }
  vvVector3 sz = vd->getSize();
  const float3 volSize = make_float3(sz[0], sz[1], sz[2]);
  float3 probeSize = make_float3(probeSizeObj[0], probeSizeObj[1], probeSizeObj[2]);
  if (_sphericalROI)
  {
    probeSize = make_float3((float)vd->vox[0], (float)vd->vox[1], (float)vd->vox[2]);
  }

  const bool isOrtho = pr.isProjOrtho();

  vvVector3 eye;
  getEyePosition(&eye);
  eye.multiply(invMv);

  vvVector3 origin;

  // use GL_LIGHT0 for local lighting
  GLfloat lv[4];
  float constAtt = 1.0f;
  float linearAtt = 0.0f;
  float quadAtt = 0.0f;
  if (glIsEnabled(GL_LIGHTING))
  {
    glGetLightfv(GL_LIGHT0, GL_POSITION, &lv[0]);
    glGetLightfv(GL_LIGHT0, GL_CONSTANT_ATTENUATION, &constAtt);
    glGetLightfv(GL_LIGHT0, GL_LINEAR_ATTENUATION, &linearAtt);
    glGetLightfv(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, &quadAtt);
  }
  const float3 L = -normalize(make_float3(lv[0], lv[1], lv[2]));

  vvVector3 normal;
  getShadingNormal(normal, origin, eye, invMv, isOrtho);

  // viewing direction equals normal direction
  const float3 V = make_float3(normal[0], normal[1], normal[2]);

  // Half way vector.
  const float3 H = normalize(L + V);

  // Clip sphere.
  const float3 center = make_float3(_roiPos[0], _roiPos[1], _roiPos[2]);
  const float radius = _roiSize[0] * vd->getSize()[0];

  // Clip plane.
  const float3 pnormal = normalize(make_float3(_clipNormal[0], _clipNormal[1], _clipNormal[2]));
  const float pdist = _clipNormal.dot(_clipPoint);

  if (_clipMode && _clipPerimeter)
  {
    drawPlanePerimeter(size, vd->pos, _clipPoint, _clipNormal, _clipColor);
  }

  GLfloat bgcolor[4];
  glGetFloatv(GL_COLOR_CLEAR_VALUE, bgcolor);
  float4 backgroundColor = make_float4(bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3]);

  renderKernel kernel = getKernel(this);

  if (kernel != NULL)
  {
    if (vd->bpc == 1)
    {
      cudaBindTextureToArray(volTexture8, ::d_volumeArrays[vd->getCurrentFrame()], ::channelDesc);
    }
    else if (vd->bpc == 2)
    {
      cudaBindTextureToArray(volTexture16, ::d_volumeArrays[vd->getCurrentFrame()], ::channelDesc);
    }

    float* d_firstIbrPass = NULL;
    if (_twoPassIbr)
    {
      const size_t size = vp[2] * vp[3] * sizeof(float);
      vvCudaTools::checkError(&ok, cudaMalloc(&d_firstIbrPass, size),
                         "vvRayRend::compositeVolume() - malloc first ibr pass array");
      vvCudaTools::checkError(&ok, cudaMemset(d_firstIbrPass, 0, size),
                         "vvRayRend::compositeVolume() - memset first ibr pass array");

      (kernel)<<<gridSize, blockSize>>>(cudaImg->getDeviceImg(), vp[2], vp[3],
                                        backgroundColor, intImg->width,diagonalVoxels / (float)numSlices,
                                        volPos, volSize * 0.5f,
                                        probePos, probeSize * 0.5f,
                                        L, H,
                                        constAtt, linearAtt, quadAtt,
                                        false, false, false,
                                        center, radius * radius,
                                        pnormal, pdist, ::d_depth, _depthPrecision,
                                        make_float2(_depthRange[0], _depthRange[1]),
                                        getIbrMode(_ibrMode), true, d_firstIbrPass);
    }
    (kernel)<<<gridSize, blockSize>>>(cudaImg->getDeviceImg(), vp[2], vp[3],
                                      backgroundColor, intImg->width,diagonalVoxels / (float)numSlices,
                                      volPos, volSize * 0.5f,
                                      probePos, probeSize * 0.5f,
                                      L, H,
                                      constAtt, linearAtt, quadAtt,
                                      false, false, false,
                                      center, radius * radius,
                                      pnormal, pdist, ::d_depth, _depthPrecision,
                                      make_float2(_depthRange[0], _depthRange[1]),
                                      getIbrMode(_ibrMode), false, d_firstIbrPass);
    vvCudaTools::checkError(&ok, cudaFree(d_firstIbrPass),
                            "vvRayRend::compositeVolume() - free first ibr pass array");
  }
  cudaImg->unmap();

  // For bounding box, tf palette display, etc.
  vvRenderer::renderVolumeGL();
}

void vvRayRend::getColorBuffer(uchar** colors) const
{
  cudaMemcpy(*colors, static_cast<vvCudaImg*>(intImg)->getDeviceImg(), intImg->width*intImg->height*4, cudaMemcpyDeviceToHost);
}

void vvRayRend::getDepthBuffer(uchar** depths) const
{
  cudaMemcpy(*depths, ::d_depth, intImg->width*intImg->height*_depthPrecision/8, cudaMemcpyDeviceToHost);
}

//----------------------------------------------------------------------------
// see parent
void vvRayRend::setParameter(ParameterType param, const vvParam& newValue)
{
  vvDebugMsg::msg(3, "vvRayRend::setParameter()");

  switch (param)
  {
  case vvRenderer::VV_SLICEINT:
    {
      const bool newInterpol = newValue;
      if (_interpolation != newInterpol)
      {
        _interpolation = newInterpol;
        initVolumeTexture();
        updateTransferFunction();
      }
    }
    break;
  case vvRenderer::VV_LIGHTING:
    _illumination = newValue;
    break;
  case vvRenderer::VV_OPCORR:
    _opacityCorrection = newValue;
    break;
  case vvRenderer::VV_TERMINATEEARLY:
    _earlyRayTermination = newValue;
    break;
  default:
    vvIbrRenderer::setParameter(param, newValue);
    break;
  }
}

//----------------------------------------------------------------------------
// see parent
vvParam vvRayRend::getParameter(ParameterType param) const
{
  vvDebugMsg::msg(3, "vvRayRend::getParameter()");

  switch (param)
  {
  case vvRenderer::VV_SLICEINT:
    return _interpolation;
  case vvRenderer::VV_LIGHTING:
    return _illumination;
  case vvRenderer::VV_OPCORR:
    return _opacityCorrection;
  case vvRenderer::VV_TERMINATEEARLY:
    return _earlyRayTermination;
  default:
    return vvIbrRenderer::getParameter(param);
  }
}

bool vvRayRend::getEarlyRayTermination() const
{
  vvDebugMsg::msg(3, "vvRayRend::getEarlyRayTermination()");

  return _earlyRayTermination;
}
bool vvRayRend::getIllumination() const
{
  vvDebugMsg::msg(3, "vvRayRend::getIllumination()");

  return _illumination;
}

bool vvRayRend::getInterpolation() const
{
  vvDebugMsg::msg(3, "vvRayRend::getInterpolation()");

  return _interpolation;
}

bool vvRayRend::getOpacityCorrection() const
{
  vvDebugMsg::msg(3, "vvRayRend::getOpacityCorrection()");

  return _opacityCorrection;
}

void vvRayRend::initVolumeTexture()
{
  vvDebugMsg::msg(3, "vvRayRend::initVolumeTexture()");

  bool ok;

  cudaExtent volumeSize = make_cudaExtent(vd->vox[0], vd->vox[1], vd->vox[2]);
  if (vd->bpc == 1)
  {
    ::channelDesc = cudaCreateChannelDesc<uchar>();
  }
  else if (vd->bpc == 2)
  {
    ::channelDesc = cudaCreateChannelDesc<ushort>();
  }
  ::d_volumeArrays.resize(vd->frames);

  int outOfMemFrame = -1;
  for (int f=0; f<vd->frames; ++f)
  {
    vvCudaTools::checkError(&_volumeCopyToGpuOk, cudaMalloc3DArray(&::d_volumeArrays[f],
                                            &::channelDesc,
                                            volumeSize),
                       "vvRayRend::initVolumeTexture() - try to alloc 3D array");
    size_t availableMem;
    size_t totalMem;
    vvCudaTools::checkError(&ok, cudaMemGetInfo(&availableMem, &totalMem),
                       "vvRayRend::initVolumeTexture() - get mem info from device");

    if(!_volumeCopyToGpuOk)
    {
      outOfMemFrame = f;
      break;
    }

    vvDebugMsg::msg(1, "Total CUDA memory (MB):     ", (int)(totalMem/1024/1024));
    vvDebugMsg::msg(1, "Available CUDA memory (MB): ", (int)(availableMem/1024/1024));

    cudaMemcpy3DParms copyParams = { 0 };

    const size_t size = vd->getBytesize(0);
    if (vd->bpc == 1)
    {
      copyParams.srcPtr = make_cudaPitchedPtr(vd->getRaw(f), volumeSize.width*vd->bpc, volumeSize.width, volumeSize.height);
    }
    else if (vd->bpc == 2)
    {
      uchar* raw = vd->getRaw(f);
      uchar* data = new uchar[size];

      for (size_t i=0; i<size; i+=2)
      {
        int val = ((int) raw[i] << 8) | (int) raw[i + 1];
        val >>= 4;
        data[i] = raw[i];
        data[i + 1] = val;
      }
      copyParams.srcPtr = make_cudaPitchedPtr(data, volumeSize.width*vd->bpc, volumeSize.width, volumeSize.height);
    }
    copyParams.dstArray = ::d_volumeArrays[f];
    copyParams.extent = volumeSize;
    copyParams.kind = cudaMemcpyHostToDevice;
    vvCudaTools::checkError(&ok, cudaMemcpy3D(&copyParams),
                       "vvRayRend::initVolumeTexture() - copy volume frame to 3D array");
  }

  if (outOfMemFrame >= 0)
  {
    cerr << "Couldn't accomodate the volume" << endl;
    for (int f=0; f<=outOfMemFrame; ++f)
    {
      vvCudaTools::checkError(&ok, cudaFree(::d_volumeArrays[f]),
                         "vvRayRend::initVolumeTexture() - free memory after failure");
      ::d_volumeArrays[f] = NULL;
    }
  }

  if (vd->bpc == 1)
  {
    for (int f=0; f<outOfMemFrame; ++f)
    {
      vvCudaTools::checkError(&ok, cudaFreeArray(::d_volumeArrays[f]),
                         "vvRayRend::initVolumeTexture() - why do we do this right here?");
      ::d_volumeArrays[f] = NULL;
    }
  }

  if (_volumeCopyToGpuOk)
  {
    if (vd->bpc == 1)
    {
        volTexture8.normalized = true;
        if (_interpolation)
        {
          volTexture8.filterMode = cudaFilterModeLinear;
        }
        else
        {
          volTexture8.filterMode = cudaFilterModePoint;
        }
        volTexture8.addressMode[0] = cudaAddressModeClamp;
        volTexture8.addressMode[1] = cudaAddressModeClamp;
        vvCudaTools::checkError(&ok, cudaBindTextureToArray(volTexture8, ::d_volumeArrays[0], ::channelDesc),
                           "vvRayRend::initVolumeTexture() - bind volume texture (bpc == 1)");
    }
    else if (vd->bpc == 2)
    {
        volTexture16.normalized = true;
        if (_interpolation)
        {
          volTexture16.filterMode = cudaFilterModeLinear;
        }
        else
        {
          volTexture16.filterMode = cudaFilterModePoint;
        }
        volTexture16.addressMode[0] = cudaAddressModeClamp;
        volTexture16.addressMode[1] = cudaAddressModeClamp;
        vvCudaTools::checkError(&ok, cudaBindTextureToArray(volTexture16, ::d_volumeArrays[0], ::channelDesc),
                           "vvRayRend::initVolumeTexture() - bind volume texture (bpc == 2)");
    }
  }
}

void vvRayRend::factorViewMatrix()
{
  vvDebugMsg::msg(3, "vvRayRend::factorViewMatrix()");

  vvGLTools::Viewport vp = vvGLTools::getViewport();
  const int w = vvToolshed::getTextureSize(vp[2]);
  const int h = vvToolshed::getTextureSize(vp[3]);

  if ((intImg->width != w) || (intImg->height != h))
  {
    intImg->setSize(w, h);
    allocIbrArrays(w, h);
  }

  iwWarp.identity();
  iwWarp.translate(-1.0f, -1.0f, 0.0f);
  iwWarp.scaleLocal(1.0f / (static_cast<float>(vp[2]) * 0.5f), 1.0f / (static_cast<float>(vp[3]) * 0.5f), 0.0f);
}

void vvRayRend::findAxisRepresentations()
{
  // Overwrite default behavior.
}

bool vvRayRend::allocIbrArrays(const int w, const int h)
{
  vvDebugMsg::msg(3, "vvRayRend::allocIbrArrays()");

  bool ok = true;
  vvCudaTools::checkError(&ok, cudaFree(::d_depth),
                          "vvRayRend::allocIbrArrays() - free d_depth");
  vvCudaTools::checkError(&ok, cudaMalloc(&::d_depth, w * h * _depthPrecision/8),
                          "vvRayRend::allocIbrArrays() - malloc d_depth");
  vvCudaTools::checkError(&ok, cudaMemset(::d_depth, 0, w * h * _depthPrecision/8),
                          "vvRayRend::allocIbrArrays() - memset d_depth");
  return ok;
}
