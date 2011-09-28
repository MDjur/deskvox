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

#ifndef _VVTRANSFUNC_H_
#define _VVTRANSFUNC_H_

#include "vvexport.h"
#include "vvinttypes.h"
#include "vvtfwidget.h"
#include "vvsllist.h"

/** Description of a transfer function.
  @author Jurgen P. Schulze (jschulze@ucsd.edu)
  @see vvTFWidget
*/
class VIRVOEXPORT vvTransFunc
{
  private:
    enum                                          /// number of elements in ring buffer
    {
      BUFFER_SIZE = 20
    };
    vvSLList<vvTFWidget*> _buffer[BUFFER_SIZE];   ///< ring buffer which can be used to implement Undo functionality
    int _nextBufferEntry;                         ///< index of next ring buffer entry to use for storage
    int _bufferUsed;                              ///< number of ring buffer entries used
    int _discreteColors;                          ///< number of discrete colors to use for color interpolation (0 for smooth colors)

  public:
    vvSLList<vvTFWidget*> _widgets;  ///< TF widget list

    vvTransFunc();
    vvTransFunc(vvTransFunc*);
    virtual ~vvTransFunc();
    bool isEmpty();
    void deleteColorWidgets();
    void setDefaultColors(int, float, float);
    int  getNumDefaultColors();
    void setDefaultAlpha(int, float, float);
    int  getNumDefaultAlpha();
    int  getNumWidgets(vvTFWidget::WidgetType);
    void deleteWidgets(vvTFWidget::WidgetType);
    void computeTFTexture(int, int, int, float*, float, float, float=0.0f, float=0.0f, float=0.0f, float=0.0f);
    vvColor computeBGColor(float, float, float);
    void computeTFTextureGamma(int, float*, float, float, int, float[], float[]);
    void computeTFTextureHighPass(int, float*, float, float, int, float[], float[], float[]);
    void computeTFTextureHistCDF(int, float*, float, float, int, int, uint*, float[], float[]);
    vvColor computeColor(float, float=-1.0f, float=-1.0f);
    float computeOpacity(float, float=-1.0f, float=-1.0f);
    void makeColorBar(int, uchar*, float, float, bool);
    void makeAlphaTexture(int, int, uchar*, float, float);
    void make2DTFTexture(int, int, uchar*, float, float, float, float);
    void make2DTFTexture2(int, int, uchar*, float, float, float, float);
    void make8bitLUT(int, uchar*, float, float);
    void makeFloatLUT(int, float*);
    void makePreintLUTOptimized(int width, uchar *preintLUT, float thickness=1.0, float min=0.0, float max=1.0);
    void makePreintLUTCorrect(int width, uchar *preintLUT, float thickness=1.0, float min=0.0, float max=1.0);
    void makeMinMaxTable(int width, uchar *minmax, float min=0.0, float max=1.0);
    static void copy(vvSLList<vvTFWidget*>*, vvSLList<vvTFWidget*>*);
    void putUndoBuffer();
    void getUndoBuffer();
    void clearUndoBuffer();
    void setDiscreteColors(int);
    int  getDiscreteColors();
    int  saveMeshviewer(const char*);
    int  saveBinMeshviewer(const char*);
    int  loadMeshviewer(const char*);

    /*!
     * \brief     Copy all widgets from another transfer function.
     * \param     rhs The other transfer function.
     */
    vvTransFunc& operator=(vvTransFunc& rhs);
};
#endif

//============================================================================
// End of File
//============================================================================
// vim: sw=2:expandtab:softtabstop=2:ts=2:cino=\:0g0t0
