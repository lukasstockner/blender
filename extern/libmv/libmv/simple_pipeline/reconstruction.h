// Copyright (c) 2011 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef LIBMV_SIMPLE_PIPELINE_RECONSTRUCTION_H_
#define LIBMV_SIMPLE_PIPELINE_RECONSTRUCTION_H_

#include <vector>

#include "libmv/base/vector.h"
#include "libmv/numeric/numeric.h"

namespace libmv {

struct ReconstructionOptions {
  // threshold value of reconstruction error which is still considered successful
  // if reconstruction error bigger than this value, fallback reconstruction
  // algorithm would be used (if enabled)
  double success_threshold;

  // use fallback reconstruction algorithm in cases main reconstruction algorithm
  // failed to reconstruct
  bool use_fallback_reconstruction;
};

/*!
    A EuclideanView is the location and orientation of a camera viewing an
    \a image. A EuclideanView may be associated with a particular \a camera for
    a multicamera reconstruction. All EuclideanViews for the same \a camera
    represent the motion of a particular video camera across all of its
    corresponding images.

    \a image identify which image from \l Tracks this view represents.
    \a R is a 3x3 matrix representing the rotation of the view.
    \a t is a translation vector representing its positions.
    \a camera identify which camera from \l Tracks this view is associated with.

    \sa Reconstruction
*/
struct EuclideanView {
  EuclideanView() : image(-1), camera(0) {}
  EuclideanView(const EuclideanView &v)
      : image(v.image), R(v.R), t(v.t), camera(v.camera) {}

  int image;
  Mat3 R;
  Vec3 t;
  int camera;
};

/*!
    A Point is the 3D location of a track.

    \a track identify which track from \l Tracks this point corresponds to.
    \a X represents the 3D position of the track.

    \sa Reconstruction
*/
struct EuclideanPoint {
  EuclideanPoint() : track(-1) {}
  EuclideanPoint(const EuclideanPoint &p) : track(p.track), X(p.X) {}
  int track;
  Vec3 X;
};

/*!
    The EuclideanReconstruction class stores \link EuclideanView views
    \endlink and \link EuclideanPoint points \endlink.

    The EuclideanReconstruction container is intended as the store of 3D
    reconstruction data to be used with the MultiView API.

    The container has lookups to query a \a EuclideanView for an \a image, or a
    \a EuclideanPoint from a \a track.

    \sa View, Point
*/
class EuclideanReconstruction {
 public:
  // Default constructor starts with no cameras.
  EuclideanReconstruction();

  /// Copy constructor.
  EuclideanReconstruction(const EuclideanReconstruction &other);

  EuclideanReconstruction &operator=(const EuclideanReconstruction &other);

  /*!
      Insert a view into the set. If there is already a view for the given
      \a image, the existing view is replaced. If there is no view for the given
      \a image, a new one is added. A view may be associated with a \a camera
      in a multicamera reconstruction.

      \a image and \a camera are the keys used to retrieve the views with the
      other methods in this class.

      \note You should use the same \a camera and \a image identifiers as in
            \l Tracks.
      \note All markers for a single \a image should have the same \a camera
            identifiers.
  */
  void InsertView(int image, const Mat3 &R, const Vec3 &t, int camera = 0);

  /*!
      Insert a point into the reconstruction. If there is already a point for
      the given \a track, the existing point is replaced. If there is no point
      for the given \a track, a new one is added.

      \a track is the key used to retrieve the points with the
      other methods in this class.

      \note You should use the same \a track identifier as in \l Tracks.
  */
  void InsertPoint(int track, const Vec3 &X);

  /// Returns a pointer to the view corresponding to \a image.
  EuclideanView *ViewForImage(int image);
  const EuclideanView *ViewForImage(int image) const;

  /// Returns all views for all images.
  vector<EuclideanView> AllViews() const;

  /// Returns all views for a particular \a camera.
  vector<EuclideanView> AllViewsForCamera(int camera) const;

  /// Returns a pointer to the point corresponding to \a track.
  EuclideanPoint *PointForTrack(int track);
  const EuclideanPoint *PointForTrack(int track) const;

  /// Returns all points.
  vector<EuclideanPoint> AllPoints() const;

 private:
  vector<EuclideanView> views_;
  vector<EuclideanPoint> points_;
};

/*!
    A ProjectiveView is the projection matrix for the view of an \a image. A
    ProjectiveView may be associated with a particular \a camera for a
    multicamera reconstruction. All ProjectiveViews for the same \a camera
    represent the motion of a particular video camera across all of its
    corresponding images.

    \a image identify which image from \l Tracks this view represents.
    \a P is the 3x4 projection matrix.
    \a camera identify which camera from \l Tracks this view is associated with.

    \sa ProjectiveReconstruction
*/
struct ProjectiveView {
  ProjectiveView() : image(-1), camera(0) {}
  ProjectiveView(const ProjectiveView &v)
      : image(v.image), P(v.P), camera(v.camera) {}

  int image;
  Mat34 P;
  int camera;
};

/*!
    A Point is the 3D location of a track.

    \a track identifies which track from \l Tracks this point corresponds to.
    \a X is the homogeneous 3D position of the track.

    \sa Reconstruction
*/
struct ProjectivePoint {
  ProjectivePoint() : track(-1) {}
  ProjectivePoint(const ProjectivePoint &p) : track(p.track), X(p.X) {}
  int track;
  Vec4 X;
};

/*!
    The ProjectiveReconstruction class stores \link ProjectiveView views
    \endlink and \link ProjectivePoint points \endlink.

    The ProjectiveReconstruction container is intended as the store of 3D
    reconstruction data to be used with the MultiView API.

    The container has lookups to query a \a ProjectiveView for an \a image, or
    a \a ProjectivePoint from a \a track.

    \sa View, Point
*/
class ProjectiveReconstruction {
 public:
  /*!
      Insert a view into the set. If there is already a view for the given
      \a image, the existing view is replaced. If there is no view for the given
      \a image, a new one is added. A view may be associated with a \a camera
      in a multicamera reconstruction.

      \a image and \a camera are the keys used to retrieve the views with the
      other methods in this class.

      \note You should use the same \a camera and \a image identifiers as in
            \l Tracks.
      \note All markers for a single \a image should have the same \a camera
            identifiers.
  */
  void InsertView(int image, const Mat34 &P, int camera = 0);

  /*!
      Insert a point into the reconstruction. If there is already a point for
      the given \a track, the existing point is replaced. If there is no point
      for the given \a track, a new one is added.

      \a track is the key used to retrieve the points with the
      other methods in this class.

      \note You should use the same \a track identifier as in \l Tracks.
  */
  void InsertPoint(int track, const Vec4 &X);

  /// Returns a pointer to the view corresponding to \a image.
  ProjectiveView *ViewForImage(int image);
  const ProjectiveView *ViewForImage(int image) const;

  /// Returns all views.
  vector<ProjectiveView> AllViews() const;

  /// Returns all views for a particular \a camera.
  vector<ProjectiveView> AllViewsForCamera(int camera) const;

  /// Returns a pointer to the point corresponding to \a track.
  ProjectivePoint *PointForTrack(int track);
  const ProjectivePoint *PointForTrack(int track) const;

  /// Returns all points.
  vector<ProjectivePoint> AllPoints() const;

 private:
  vector<ProjectiveView> views_;
  vector<ProjectivePoint> points_;
};

}  // namespace libmv

#endif  // LIBMV_SIMPLE_PIPELINE_RECONSTRUCTION_H_
