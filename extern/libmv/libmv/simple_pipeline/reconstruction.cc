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

void EuclideanReconstruction::InsertView(int camera,
                                         int image,
                                         const Mat3 &R,
                                         const Vec3 &t) {
  LG << "InsertView camera " << camera << ", image " << image
     << ":\nR:\n"<< R << "\nt:\n" << t;
  if (camera >= views_.size()) {
    views_.resize(camera + 1);
  }
  if (image >= views_[camera].size()) {
    views_[camera].resize(image + 1);
  }
  views_[camera][image].camera = camera;
  views_[camera][image].image = image;
  views_[camera][image].R = R;
  views_[camera][image].t = t;
}

void EuclideanReconstruction::InsertPoint(int track, const Vec3 &X) {
  LG << "InsertPoint " << track << ":\n" << X;
  if (track >= points_.size()) {
    points_.resize(track + 1);
  }
  points_[track].track = track;
  points_[track].X = X;
}

EuclideanView *EuclideanReconstruction::ViewForImage(
    int camera, int image) {
  return const_cast<EuclideanView *>(
      static_cast<const EuclideanReconstruction *>(
          this)->ViewForImage(camera, image));
}

const EuclideanView *EuclideanReconstruction::ViewForImage(
    int camera, int image) const {
  if (camera < 0 || camera >= views_.size() ||
      image < 0 || image >= views_[camera].size()) {
    return NULL;
  }
  const EuclideanView *view = &views_[camera][image];
  if (view->camera == -1 || view->image == -1) {
    return NULL;
  }
  return view;
}

std::vector<vector<EuclideanView> > EuclideanReconstruction::AllViews(
    ) const {
  std::vector<vector<EuclideanView> > views;
  views.resize(views_.size());
  for (int i = 0; i < views_.size(); ++i) {
    for (int j = 0; j < views_[i].size(); ++j) {
      if (views_[i][j].camera != -1 && views_[i][j].image != -1) {
        views[i].push_back(views_[i][j]);
      }
    }
  }
  return views;
}

vector<EuclideanView> EuclideanReconstruction::AllViewsForCamera(
    int camera) const {
  vector<EuclideanView> views;
  if (camera >= 0 && camera < views_.size()) {
    for (int i = 0; i < views_[camera].size(); ++i) {
      if (views_[camera][i].camera != -1 && views_[camera][i].image != -1) {
        views.push_back(views_[camera][i]);
      }
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

void ProjectiveReconstruction::InsertView(int camera, int image,
                                          const Mat34 &P) {
  LG << "InsertView camera " << camera << ", image " << image
     << ":\nP:\n"<< P;
  if (camera >= views_.size()) {
    views_.resize(camera + 1);
  }
  if (image >= views_[camera].size()) {
    views_[camera].resize(image + 1);
  }
  views_[camera][image].camera = camera;
  views_[camera][image].image = image;
  views_[camera][image].P = P;
}

void ProjectiveReconstruction::InsertPoint(int track, const Vec4 &X) {
  LG << "InsertPoint " << track << ":\n" << X;
  if (track >= points_.size()) {
    points_.resize(track + 1);
  }
  points_[track].track = track;
  points_[track].X = X;
}

ProjectiveView *ProjectiveReconstruction::ViewForImage(int camera, int image) {
  return const_cast<ProjectiveView *>(
      static_cast<const ProjectiveReconstruction *>(
          this)->ViewForImage(camera, image));
}

const ProjectiveView *ProjectiveReconstruction::ViewForImage(
    int camera, int image) const {
  if (camera < 0 || camera >= views_.size() ||
      image < 0 || image >= views_[camera].size()) {
    return NULL;
  }
  const ProjectiveView *view = &views_[camera][image];
  if (view->camera == -1 || view->image == -1) {
    return NULL;
  }
  return view;
}

std::vector<vector<ProjectiveView> > ProjectiveReconstruction::AllViews() const {
  std::vector<vector<ProjectiveView> > views;
  views.resize(views_.size());
  for (int i = 0; i < views_.size(); ++i) {
    for (int j = 0; j < views_[i].size(); ++j) {
      if (views_[i][j].camera != 1 && views_[i][j].image != -1) {
        views[i].push_back(views_[i][j]);
      }
    }
  }
  return views;
}

vector<ProjectiveView> ProjectiveReconstruction::AllViewsForCamera(
    int camera) const {
  vector<ProjectiveView> views;
  if (camera < views_.size()) {
    for (int i = 0; i < views_[camera].size(); ++i) {
      if (views_[camera][i].camera != -1 && views_[camera][i].image != -1) {
        views.push_back(views_[camera][i]);
      }
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
