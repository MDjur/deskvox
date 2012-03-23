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

#ifndef _VVECMATH_H_
#define _VVECMATH_H_

#include <float.h>
#include <iostream>

#include "vvexport.h"

//============================================================================
// Constant Definitions
//============================================================================

                                                  ///< compiler independent definition for pi
const float VV_PI = 3.1415926535897932384626433832795028841971693993751058f;
const float VV_FLT_MAX = FLT_MAX;                 ///< maximum float value

//============================================================================
// Forward Declarations
//============================================================================

template <typename T>
class VIRVOEXPORT vvBaseVector3;
template <typename T>
class VIRVOEXPORT vvBaseVector4;
class VIRVOEXPORT vvMatrix;

//============================================================================
// Common vector types
//============================================================================

typedef vvBaseVector3<int> vvVector3i;
typedef vvBaseVector3<unsigned int> vvVector3ui;
typedef vvBaseVector3<short> vvVector3s;
typedef vvBaseVector3<unsigned short> vvVector3us;
typedef vvBaseVector3<long> vvVector3l;
typedef vvBaseVector3<unsigned long> vvVector3ul;
typedef vvBaseVector3<float> vvVector3f;
typedef vvBaseVector3<double> vvVector3d;
typedef vvVector3f vvVector3;

typedef vvBaseVector4<int> vvVector4i;
typedef vvBaseVector4<unsigned int> vvVector4ui;
typedef vvBaseVector4<short> vvVector4s;
typedef vvBaseVector4<unsigned short> vvVector4us;
typedef vvBaseVector4<long> vvVector4l;
typedef vvBaseVector4<unsigned long> vvVector4ul;
typedef vvBaseVector4<float> vvVector4f;
typedef vvBaseVector4<double> vvVector4d;
typedef vvVector4f vvVector4;

//============================================================================
// Class Definitions
//============================================================================

class VIRVOEXPORT vvVecmath
{
  public:
    static float sgn(float);
};

/** 4x4 matrix type.
 Matrix elements are: e[row][column]
 @author Jurgen P. Schulze (jschulze@ucsd.edu)
*/
class VIRVOEXPORT vvMatrix
{
  private:
    void LUDecomposition(int index[4], float &d);
    void LUBackSubstitution(int index[4], float b[4]);

  public:
    float e[4][4];                                ///< matrix elements: [row][column]

    vvMatrix();
    vvMatrix(const vvMatrix*);
    vvMatrix(float* glf);
    vvMatrix operator+(const vvMatrix& operand) const;
    vvMatrix operator-(const vvMatrix& operand) const;
    vvMatrix operator*(const vvMatrix& operand) const;
    void print(const char*) const;
    void identity();
    void zero();
    void translate(float, float, float);
    void translate(const vvVector3*);
    void scale(float, float, float);
    void scale(float);
    vvMatrix rotate(float, float, float, float);
    vvMatrix rotate(float, const vvVector3*);
    void multiplyPre(const vvMatrix*);
    void multiplyPost(const vvMatrix*);
    void transpose();
    float diagonal();
    void invertOrtho();
    void invert2D();
    void copy(const vvMatrix*);
    void copyTrans(const vvMatrix*);
    void copyRot(const vvMatrix*);
    void transOnly();
    void rotOnly();
    void killTrans();
    void killRot();
    bool equal(const vvMatrix*) const;
    void getGL(float*) const;
    void setGL(const float*);
    void setGL(const double*);
    void get(float*) const;
    void set(const float*);
    void get(double*) const;
    void set(const double*);
    void setRow(int, float, float, float, float);
    void setRow(int, vvVector3*);
    void setColumn(int, float, float, float, float);
    void setColumn(int, const vvVector3& vec);
    void getRow(int, float*, float*, float*, float*);
    void getRow(int, vvVector3*);
    void getColumn(int, float*, float*, float*, float*);
    void getColumn(int, vvVector3& vec);
    void random(int, int);
    void random(float, float);
    void invert();
    void swapRows(int, int);
    void swapColumns(int, int);
    void setProjOrtho(float, float, float, float, float, float);
    void getProjOrtho(float*, float*, float*, float*, float*, float*);
    void setProjPersp(float, float, float, float, float, float);
    void getProjPersp(float*, float*, float*, float*, float*, float*);
    bool isProjOrtho() const;
    void makeLookAt(float, float, float, float, float, float, float, float, float);
    float getNearPlaneZ() const;
    vvMatrix trackballRotation(int, int, int, int, int, int);
    void computeEulerAngles(float*, float*, float*);
};

/** 3D vector primitive.
 @author Juergen Schulze-Doebold (schulze@hlrs.de)
*/
template <typename T>
class VIRVOEXPORT vvBaseVector4
{
  public:
    T e[4];                                   ///< vector elements (x|y|z|w)

    vvBaseVector4();
    explicit vvBaseVector4(T val);
    vvBaseVector4(T x, T y, T z, T w);
    vvBaseVector4(const vvBaseVector4*);
    vvBaseVector4(const vvBaseVector3<T>*, const T w);
    T &operator[](int);
    T operator[](int) const;
    void set(T x, T y, T z, T w);
    void multiply(const vvMatrix*);
    void copy(const vvBaseVector4*);
    void add(const vvBaseVector4*);
    void sub(const vvBaseVector4*);
    void print(const char* text = 0) const;
    void perspectiveDivide();

    vvBaseVector4 operator + (const vvBaseVector4 &other) const;
    vvBaseVector4 operator - (const vvBaseVector4 &other) const;
    vvBaseVector4 operator * (const vvBaseVector4 &other) const;
};

/** base vector primitive
 @author Jurgen P. Schulze (jschulze@ucsd.edu)
*/
template <typename T>
class VIRVOEXPORT vvBaseVector3
{
  public:
    T e[3];                                   ///< vector elements (x|y|z)

    vvBaseVector3();
    explicit vvBaseVector3(T);
    vvBaseVector3(T x, T y, T z);
    vvBaseVector3(const vvBaseVector3*);
    vvBaseVector3 operator^(const vvBaseVector3) const;
    T &operator[](const int);
    T operator[](const int) const;
    void  set(T x, T y, T z);
    void  get(T* x, T* y, T* z) const;
    void  copy(const vvBaseVector3*);
    void  copy(const vvBaseVector3&);
    void  copy(const vvBaseVector4<T>*);
    void  add(const vvBaseVector3*);
    void  add(T val);
    void  add(T x, T y, T z);
    void  sub(const vvBaseVector3*);
    void  sub(T val);
    void  scale(T s);
    void  scale(const vvBaseVector3*);
    void  scale(T x, T y, T z);
    T dot(const vvBaseVector3*) const;
    T angle(const vvBaseVector3*) const;
    void  cross(const vvBaseVector3*);
    void  multiply(const vvMatrix*);
    T distance(const vvBaseVector3*) const;
    T length() const;
    void  planeNormalPPV(const vvBaseVector3*, const vvBaseVector3*, const vvBaseVector3*);
    T distPointPlane(const vvBaseVector3*, const vvBaseVector3*) const;
    void  normalize();
    void  negate();
    bool  equal(const vvBaseVector3*);
    void  random(int, int);
    void  random(float, float);
    void  random(double, double);
    void  print(const char* text = 0) const;
    void  getRow(const vvMatrix*, const int);
    void  getColumn(const vvMatrix*, const int);
    void  swap(vvBaseVector3<T>*);
    bool  isectPlaneLine(const vvBaseVector3*, const vvBaseVector3*, const vvBaseVector3*, const vvBaseVector3*);
    bool  isectPlaneRay(const vvBaseVector3*, const vvBaseVector3*, const vvBaseVector3*, const vvBaseVector3*);
    int   isectPlaneCuboid(const vvBaseVector3*, const vvBaseVector3*, const vvBaseVector3*, const vvBaseVector3*);
    int   isectRayCylinder(const vvBaseVector3*, const vvBaseVector3*, T, const vvBaseVector3*, const vvBaseVector3*);
    bool  isectRayTriangle(const vvBaseVector3*, const vvBaseVector3*, const vvBaseVector3*, const vvBaseVector3*, const vvBaseVector3*);
    T isectLineLine(const vvBaseVector3&, const vvBaseVector3&, const vvBaseVector3&, const vvBaseVector3&);
    bool  isSameSideLine2D(const vvBaseVector3*, const vvBaseVector3*, const vvBaseVector3*, const vvBaseVector3*);
    bool  isInTriangle(const vvBaseVector3*, const vvBaseVector3*, const vvBaseVector3*);
    void  cyclicSort(const int, const vvBaseVector3*);
    void  zero();
    bool  isZero() const;
    void  getSpherical(T*, T*, T*);
    void  directionCosines(const vvBaseVector3*);
    vvBaseVector3 operator + (const vvBaseVector3 &other) const;
    vvBaseVector3 operator - (const vvBaseVector3 &other) const;
    vvBaseVector3 operator * (const vvBaseVector3 &other) const;

    vvBaseVector3 operator * (const T scalar) const;

    vvBaseVector3& operator = (const vvBaseVector3 &other);
    vvBaseVector3& operator += (const vvBaseVector3 &other);
    vvBaseVector3& operator -= (const vvBaseVector3 &other);

    vvBaseVector3 operator + (void) const;
    vvBaseVector3 operator - (void) const;
};

/** 3D plane primitive.
 @author Jurgen Schulze (jschulze@ucsd.edu)
*/
class vvPlane
{
  public:
    vvVector3 _point;
    vvVector3 _normal;

    vvPlane();
    vvPlane(const vvVector3& p, const vvVector3& n);
    vvPlane(const vvVector3& p, const vvVector3& dir1, const vvVector3& dir2);
    bool isSameSide(const vvVector3&, const vvVector3&) const;
    float dist(const vvVector3&) const;
};

inline std::ostream& operator<<(std::ostream& out, const vvMatrix& m)
{
  for (int i = 0; i < 4; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      out << " " << m.e[i][j];
    }
    out << "\n";
  }
  return out;
}

#include "vvvecmath.impl.h"

#endif

// EOF
// vim: sw=2:expandtab:softtabstop=2:ts=2:cino=\:0g0t0
