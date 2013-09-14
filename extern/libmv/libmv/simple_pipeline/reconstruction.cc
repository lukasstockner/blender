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

#include <vector>

#include "libmv/simple_pipeline/reconstruction.h"
#include "libmv/numeric/numeric.h"
#include "libmv/logging/logging.h"

namespace libmv {

EuclideanReconstruction::EuclideanReconstruction() {}
EuclideanReconstruction::EuclideanReconstruction(
    const EuclideanReconstruction &other) {
  views_ = other.views_;
  points_ = other.points_;
}

EuclideanReconstruction &EuclideanReconstruction::operator=(
    const EuclideanReconstruction &other) {
  if (&other != this) {
    views_ = other.views_;
    points_ = other.points_;
  }
  return *this;
}

void EuclideanReconstruction::InsertView(int image,
                                         const Mat3 &R,
                                         const Vec3 &t,
                                         int camera) {
  LG << "InsertView camera " << camera << ", image " << image
     << ":\nR:\n"<< R << "\nt:\n" << t;
  if (image >= views_.size()) {
    views_.resize(image + 1);
  }
  views_[image].image = image;
  views_[image].R = R;
  views_[image].t = t;
  views_[image].camera = camera;
}

void EuclideanReconstruction::InsertPoint(int track, const Vec3 &X) {
  LG << "InsertPoint " << track << ":\n" << X;
  if (track >= points_.size()) {
    points_.resize(track + 1);
  }
  points_[track].track = track;
  points_[track].X = X;
}

EuclideanView *EuclideanReconstruction::ViewForImage(int image) {
  return const_cast<EuclideanView *>(
      static_cast<const EuclideanReconstruction *>(
          this)->ViewForImage(image));
}

const EuclideanView *EuclideanReconstruction::ViewForImage(int image) const {
  if (image < 0 || image >= views_.size()) {
    return NULL;
  }
  const EuclideanView *view = &views_[image];
  if (view->camera == -1 || view->image == -1) {
    return NULL;
  }
  return view;
}

vector<EuclideanView> EuclideanReconstruction::AllViews(
    ) const {
  vector<EuclideanView> views;
  for (int i = 0; i < views_.size(); ++i) {
    if (views_[i].camera != -1 && views_[i].image != -1) {
      views.push_back(views_[i]);
    }
  }
  return views;
}

vector<EuclideanView> EuclideanReconstruction::AllViewsForCamera(
    int camera) const {
  vector<EuclideanView> views;
  for (int i = 0; i < views_.size(); ++i) {
    if (views_[i].image != -1 && views_[i].camera == camera) {
      views.push_back(views_[i]);
    }
  }
  return views;
}

EuclideanPoint *EuclideanReconstruction::PointForTrack(int track) {
  return const_cast<EuclideanPoint *>(
      static_cast<const EuclideanReconstruction *>(this)->PointForTrack(track));
}

const EuclideanPoint *EuclideanReconstruction::PointForTrack(int track) const {
  if (track < 0 || track >= points_.size()) {
    return NULL;
  }
  const EuclideanPoint *point = &points_[track];
  if (point->track == -1) {
    return NULL;
  }
  return point;
}

vector<EuclideanPoint> EuclideanReconstruction::AllPoints() const {
  vector<EuclideanPoint> points;
  for (int i = 0; i < points_.size(); ++i) {
    if (points_[i].track != -1) {
      points.push_back(points_[i]);
    }
  }
  return points;
}

void ProjectiveReconstruction::InsertView(int image,
                                          const Mat34 &P,
                                          int camera) {
  LG << "InsertView camera " << camera << ", image " << image
     << ":\nP:\n"<< P;
  if (image >= views_.size()) {
    views_.resize(image + 1);
  }
  views_[image].image = image;
  views_[image].P = P;
  views_[image].camera = camera;
}

void ProjectiveReconstruction::InsertPoint(int track, const Vec4 &X) {
  LG << "InsertPoint " << track << ":\n" << X;
  if (track >= points_.size()) {
    points_.resize(track + 1);
  }
  points_[track].track = track;
  points_[track].X = X;
}

ProjectiveView *ProjectiveReconstruction::ViewForImage(int image) {
  return const_cast<ProjectiveView *>(
      static_cast<const ProjectiveReconstruction *>(
          this)->ViewForImage(image));
}

const ProjectiveView *ProjectiveReconstruction::ViewForImage(int image) const {
  if (image < 0 || image >= views_.size()) {
    return NULL;
  }
  const ProjectiveView *view = &views_[image];
  if (view->camera == -1 || view->image == -1) {
    return NULL;
  }
  return view;
}

vector<ProjectiveView> ProjectiveReconstruction::AllViews() const {
  vector<ProjectiveView> views;
  for (int i = 0; i < views_.size(); ++i) {
    if (views_[i].camera != 1 && views_[i].image != -1) {
      views.push_back(views_[i]);
    }
  }
  return views;
}

vector<ProjectiveView> ProjectiveReconstruction::AllViewsForCamera(
    int camera) const {
  vector<ProjectiveView> views;
  for (int i = 0; i < views_.size(); ++i) {
    if (views_[i].image != -1 && views_[i].camera == camera) {
      views.push_back(views_[i]);
    }
  }
  return views;
}

ProjectivePoint *ProjectiveReconstruction::PointForTrack(int track) {
  return const_cast<ProjectivePoint *>(
      static_cast<const ProjectiveReconstruction *>(this)->PointForTrack(track));
}

const ProjectivePoint *ProjectiveReconstruction::PointForTrack(int track) const {
  if (track < 0 || track >= points_.size()) {
    return NULL;
  }
  const ProjectivePoint *point = &points_[track];
  if (point->track == -1) {
    return NULL;
  }
  return point;
}

vector<ProjectivePoint> ProjectiveReconstruction::AllPoints() const {
  vector<ProjectivePoint> points;
  for (int i = 0; i < points_.size(); ++i) {
    if (points_[i].track != -1) {
      points.push_back(points_[i]);
    }
  }
  return points;
}

}  // namespace libmv
