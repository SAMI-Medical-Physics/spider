// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025, 2026 South Australia Medical Imaging

#ifndef SPIDER_TIA_EXP_FIT_FUNCTOR_H
#define SPIDER_TIA_EXP_FIT_FUNCTOR_H

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
class ExpFitFunctor
{
public:
  using InPixelType = itk::VariableLengthVector<float>;
  using OutPixelType = float;

  // XXX: It is the caller's responsibility to ensure that the number
  // of time points is equal to the number of scalar images.
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

  // It was originally inlined because it was templated.
  inline OutPixelType
  operator()(const InPixelType& y) const
  {
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
        = std::accumulate(logy.begin(), logy.end(), 0.0) / num_time_points_;

    // Compute the slope and intercept of logy = intercept + slope *
    // t.
    double slope_numerator = 0.0;
    for (std::size_t i = 0; i < num_time_points_; ++i)
      {
        slope_numerator += time_point_deviation_s_[i] * (logy[i] - logy_mean);
      }
    const double slope = slope_numerator / slope_denominator_s2_; // -b
    // Avoid returning a negative TIA for the pixel.
    if (slope >= 0.0)
      // return static_cast<OutPixelType>(0.0);
      return 0.0f;
    const double intercept = logy_mean - slope * time_points_mean_s_; // log(A)
    const double b_est = -slope;
    // FIXME: Would it help to have a lower limit on b_est
    // corresponding to the physical half-life?
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
};
} // namespace spider

#endif // SPIDER_TIA_EXP_FIT_FUNCTOR_H
