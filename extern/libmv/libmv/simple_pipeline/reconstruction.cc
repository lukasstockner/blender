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
  cameras_ = other.cameras_;
  points_ = other.points_;
}

EuclideanReconstruction &EuclideanReconstruction::operator=(
    const EuclideanReconstruction &other) {
  if (&other != this) {
    cameras_ = other.cameras_;
    points_ = other.points_;
  }
  return *this;
}

void EuclideanReconstruction::InsertCamera(int view,
                                           int image,
                                           const Mat3 &R,
                                           const Vec3 &t) {
  LG << "InsertCamera view " << view << ", image " << image
     << ":\nR:\n"<< R << "\nt:\n" << t;
  if (view >= cameras_.size()) {
    cameras_.resize(view + 1);
  }
  if (image >= cameras_[view].size()) {
    cameras_[view].resize(image + 1);
  }
  cameras_[view][image].view = view;
  cameras_[view][image].image = image;
  cameras_[view][image].R = R;
  cameras_[view][image].t = t;
}

void EuclideanReconstruction::InsertPoint(int track, const Vec3 &X) {
  LG << "InsertPoint " << track << ":\n" << X;
  if (track >= points_.size()) {
    points_.resize(track + 1);
  }
  points_[track].track = track;
  points_[track].X = X;
}

EuclideanCamera *EuclideanReconstruction::CameraForViewImage(
    int view, int image) {
  return const_cast<EuclideanCamera *>(
      static_cast<const EuclideanReconstruction *>(
          this)->CameraForViewImage(view, image));
}

const EuclideanCamera *EuclideanReconstruction::CameraForViewImage(
    int view, int image) const {
  if (view < 0 || view >= cameras_.size() ||
      image < 0 || image >= cameras_[view].size()) {
    return NULL;
  }
  const EuclideanCamera *camera = &cameras_[view][image];
  if (camera->view == -1 || camera->image == -1) {
    return NULL;
  }
  return camera;
}

std::vector<vector<EuclideanCamera> > EuclideanReconstruction::AllCameras(
    ) const {
  std::vector<vector<EuclideanCamera> > cameras;
  cameras.resize(cameras_.size());
  for (int i = 0; i < cameras_.size(); ++i) {
    for (int j = 0; j < cameras_[i].size(); ++j) {
      if (cameras[i][j].view != -1 && cameras_[i][j].image != -1) {
        cameras[i].push_back(cameras_[i][j]);
      }
    }
  }
  return cameras;
}

vector<EuclideanCamera> EuclideanReconstruction::AllCamerasForView(
    int view) const {
  vector<EuclideanCamera> cameras;
  if (view >= 0 && view < cameras.size()) {
    for (int i = 0; i < cameras_[view].size(); ++i) {
      if (cameras_[view][i].view != -1 && cameras_[view][i].image != -1) {
        cameras.push_back(cameras_[view][i]);
      }
    }
  }
  return cameras;
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

void ProjectiveReconstruction::InsertCamera(int view, int image,
                                            const Mat34 &P) {
  LG << "InsertCamera view " << view << ", image " << image
     << ":\nP:\n"<< P;
  if (view >= cameras_.size()) {
    cameras_.resize(view + 1);
  }
  if (image >= cameras_[view].size()) {
    cameras_[view].resize(image + 1);
  }
  cameras_[view][image].view = view;
  cameras_[view][image].image = image;
  cameras_[view][image].P = P;
}

void ProjectiveReconstruction::InsertPoint(int track, const Vec4 &X) {
  LG << "InsertPoint " << track << ":\n" << X;
  if (track >= points_.size()) {
    points_.resize(track + 1);
  }
  points_[track].track = track;
  points_[track].X = X;
}

ProjectiveCamera *ProjectiveReconstruction::CameraForViewImage(int view, int image) {
  return const_cast<ProjectiveCamera *>(
      static_cast<const ProjectiveReconstruction *>(
          this)->CameraForViewImage(view, image));
}

const ProjectiveCamera *ProjectiveReconstruction::CameraForViewImage(
    int view, int image) const {
  if (view < 0 || view >= cameras_.size() ||
      image < 0 || image >= cameras_[view].size()) {
    return NULL;
  }
  const ProjectiveCamera *camera = &cameras_[view][image];
  if (camera->view == -1 || camera->image == -1) {
    return NULL;
  }
  return camera;
}

std::vector<vector<ProjectiveCamera> > ProjectiveReconstruction::AllCameras() const {
  std::vector<vector<ProjectiveCamera> > cameras;
  cameras.resize(cameras_.size());
  for (int i = 0; i < cameras_.size(); ++i) {
    for (int j = 0; j < cameras_[i].size(); ++j) {
      if (cameras[i][j].view != 1 && cameras_[i][j].image != -1) {
        cameras[i].push_back(cameras_[i][j]);
      }
    }
  }
  return cameras;
}

vector<ProjectiveCamera> ProjectiveReconstruction::AllCamerasForView(
    int view) const {
  vector<ProjectiveCamera> cameras;
  if (view < cameras_.size()) {
    for (int i = 0; i < cameras_[view].size(); ++i) {
      if (cameras_[view][i].view != -1 && cameras_[view][i].image != -1) {
        cameras.push_back(cameras_[view][i]);
      }
    }
  }
  return cameras;
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
