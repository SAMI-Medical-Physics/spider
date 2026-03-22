// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025, 2026 South Australia Medical Imaging

#ifndef SPIDER_TIA_EXP_FIT_FUNCTOR_H
#define SPIDER_TIA_EXP_FIT_FUNCTOR_H

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>   // std::log, std::exp
#include <cstddef> // std::size_t
#include <numeric> // std::accumulate
#include <vector>

#include <itkVariableLengthVector.h>

// TODO: Output a vector image that additionally contains the fit
// parameters and sum of squared residuals?

namespace spider
{
// Fit y = A * exp(-b * t) to pixel values y_i at time points t_i.
// This is a log-linear model so can we obtain the fit using simple
// linear regression:
// <https://en.wikipedia.org/wiki/Simple_linear_regression>.  The
// functor returns the time-integrated activity (TIA) in units of the
// pixel units * seconds.
//
// XXX: SetTimePoints must be called before operator().  The size of
// the operator() argument (itk::VariableLengthVector<float>) must
// match the number of time points last passed to SetTimePoints, and
// must be at least 2.
//
// XXX: SetRadionuclideHalfLife must be called before operator().
class ExpFitFunctor
{
public:
  using InPixelType = itk::VariableLengthVector<float>;
  using OutPixelType = float;

  void
  SetTimePoints(const std::vector<std::chrono::seconds>& time_points)
  {
    // These are the same for all pixels in the vector image.
    num_time_points_ = time_points.size();

    double sum_s = 0.0;
    for (const auto& tp : time_points)
      sum_s += std::chrono::duration<double>(tp).count();
    time_points_mean_s_ = sum_s / num_time_points_;

    time_point_deviation_s_.resize(num_time_points_);
    slope_denominator_s2_ = 0.0;
    for (std::size_t i = 0; i < num_time_points_; ++i)
      {
        time_point_deviation_s_[i]
            = std::chrono::duration<double>(time_points[i]).count()
              - time_points_mean_s_;
        slope_denominator_s2_
            += time_point_deviation_s_[i] * time_point_deviation_s_[i];
      }
  }

  void
  SetRadionuclideHalfLife(std::chrono::seconds half_life)
  {
    half_life_s_ = std::chrono::duration<double>(half_life).count();
  }

  // It was originally inlined because it was templated.
  inline OutPixelType
  operator()(const InPixelType& y) const
  {
    assert(y.GetSize() == num_time_points_);
    assert(num_time_points_ > 1);
    // The log-linear method requires all y_i > 0.  Registation with
    // elastix introduces large negative values.
    for (std::size_t i = 0; i < num_time_points_; ++i)
      {
        if (y[i] <= 0.0)
          return 0.0f;
      }

    std::vector<double> logy(num_time_points_);
    for (std::size_t i = 0; i < num_time_points_; ++i)
      {
        // Calculate the log with double precision instead of floating
        // point.
        logy[i] = std::log(static_cast<double>(y[i]));
      }

    const double logy_mean
        = std::accumulate(logy.cbegin(), logy.cend(), 0.0) / num_time_points_;

    // Compute the slope and intercept of logy = intercept + slope *
    // t.
    double slope_numerator = 0.0;
    for (std::size_t i = 0; i < num_time_points_; ++i)
      {
        slope_numerator += time_point_deviation_s_[i] * (logy[i] - logy_mean);
      }
    const double slope = slope_numerator / slope_denominator_s2_;     // -b
    const double intercept = logy_mean - slope * time_points_mean_s_; // log(A)
    // If the slope is positive or b_est is slower than physical
    // decay, use A_est and physical decay.
    assert(half_life_s_ != 0.0);
    const double b_est = std::max(-slope, std::log(2) / half_life_s_);
    const double A_est = std::exp(intercept);
    // Return the TIA in units of pixel units * seconds.
    const double time_integrated_activity = A_est / b_est;
    return static_cast<OutPixelType>(time_integrated_activity);
  }

private:
  std::size_t num_time_points_ = 0;
  double time_points_mean_s_ = 0.0;
  std::vector<double> time_point_deviation_s_;
  double slope_denominator_s2_ = 0.0;
  double half_life_s_ = 0.0;
};
} // namespace spider

#endif // SPIDER_TIA_EXP_FIT_FUNCTOR_H
