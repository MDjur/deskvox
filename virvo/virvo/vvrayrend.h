//
// This software contains source code provided by NVIDIA Corporation.
//

#ifndef _VV_RAYREND_H_
#define _VV_RAYREND_H_

#include "vvexport.h"
#include "vvopengl.h"
#include "vvsoftvr.h"
#include "vvtransfunc.h"
#include "vvvoldesc.h"
#include "vvimage.h"

#ifdef HAVE_CONFIG_H
#include "vvconfig.h"
#endif

#if defined(HAVE_CUDA) && defined(NV_PROPRIETARY_CODE)

#include "vvcuda.h"

#endif

/** Ray casting renderer. Based on the volume
  rendering implementation from the NVIDIA CUDA SDK,
  as of November 25th, 2010 could be downloaded from
  the following location:
  http://developer.download.nvidia.com/compute/cuda/sdk/website/samples.html
 */
class VIRVOEXPORT vvRayRend : public vvSoftVR
{
public:
  enum IbrMode
  {
    VV_MAX_GRADIENT,
    VV_MIDDLE,
    VV_SURFACE
  };

#if defined(HAVE_CUDA) && defined(NV_PROPRIETARY_CODE)
  vvRayRend(vvVolDesc* vd, vvRenderState renderState);
  ~vvRayRend();

  int getLUTSize() const;
  void updateTransferFunction();
  void compositeVolume(int w = -1, int h = -1);
  void setParameter(ParameterType param, float newValue);
  void setNumSpaceSkippingCells(const int numSpaceSkippingCells[3]);
  void setDepthPrecision(const vvImage2_5d::DepthPrecision dp);

  bool getEarlyRayTermination() const;
  bool getIllumination() const;
  bool getInterpolation() const;
  bool getOpacityCorrection() const;
  bool getSpaceSkipping() const;

  uchar*  _depthUchar;
  ushort* _depthUshort;
  uint*   _depthUint;

private:
  cudaChannelFormatDesc _channelDesc;
  std::vector<cudaArray*> d_volumeArrays;
  cudaArray* d_transferFuncArray;
  cudaArray* d_randArray;
  cudaArray* d_spaceSkippingArray;

  bool* h_spaceSkippingArray;
  int* h_cellMinValues;
  int* h_cellMaxValues;
  int h_numCells[3];

  float* _rgbaTF;

  bool _earlyRayTermination;        ///< Terminate ray marching when enough alpha was gathered
  bool _illumination;               ///< Use local illumination
  bool _interpolation;              ///< interpolation mode: true=linear interpolation (default), false=nearest neighbor
  bool _opacityCorrection;          ///< true = opacity correction on
  bool _spaceSkipping;              ///< true = skip over homogeneous regions
  bool _volumeCopyToGpuOk;          ///< must be true for memCopy to be run

  vvImage2_5d::DepthPrecision _depthPrecision; ///< enum indicating precision of depth buffer for image based rendering

  void initRandTexture();
  void initSpaceSkippingTexture();
  void initVolumeTexture();
  void factorViewMatrix();
  void findAxisRepresentations();

  void calcSpaceSkippingGrid();
  void computeSpaceSkippingTexture();
#endif // HAVE_CUDA
};

#endif