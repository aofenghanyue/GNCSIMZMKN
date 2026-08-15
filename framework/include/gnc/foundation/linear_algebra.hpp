#pragma once

#include <Eigen/Core>

namespace gnc::foundation {

static_assert(EIGEN_WORLD_VERSION == 3 && EIGEN_MAJOR_VERSION == 4 &&
                  EIGEN_MINOR_VERSION == 0,
              "GNC foundation requires Eigen 3.4.0 exactly");

using Vec3 = Eigen::Vector3d;
using Mat3 = Eigen::Matrix3d;
using Vector = Eigen::VectorXd;
using Matrix = Eigen::MatrixXd;

static_assert(Vec3::RowsAtCompileTime == 3 &&
                  Vec3::ColsAtCompileTime == 1,
              "Vec3 must be a fixed three-component column vector");
static_assert(Mat3::RowsAtCompileTime == 3 &&
                  Mat3::ColsAtCompileTime == 3,
              "Mat3 must be a fixed 3x3 matrix");

} // namespace gnc::foundation
