// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

#ifndef SPIDER_TIA_EXP_FIT_FUNCTOR_H
#define SPIDER_TIA_EXP_FIT_FUNCTOR_H

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
  SetTimePoints(const std::vector<double>& time_points /* hours */)
  {
    time_points_ = time_points;
    // These are the same for all pixels in the vector image.
    time_points_mean_
        = std::accumulate(time_points.begin(), time_points.end(), 0.0)
          / time_points.size();
    for (std::size_t i = 0; i < time_points.size(); ++i)
      {
        slope_denominator_ += (time_points[i] - time_points_mean_)
                              * (time_points[i] - time_points_mean_);
      }
  }

  // It was originally inlined because it was templated.
  inline OutPixelType
  operator()(const InPixelType& y) const
  {
    // The log-linear method requires all y_i > 0.  Registation with
    // elastix introduces large negative values.
    for (unsigned int i = 0; i < y.GetSize(); ++i)
      {
        if (y[i] <= 0.0)
          return 0.0f;
      }

    std::vector<double> logy(y.GetSize());
    for (unsigned int i = 0; i < y.GetSize(); ++i)
      {
        // Calculate the log with double precision instead of floating
        // point.
        logy[i] = std::log(static_cast<double>(y[i]));
      }

    const double logy_mean
        = std::accumulate(logy.begin(), logy.end(), 0.0) / logy.size();

    // Compute the slope and intercept of logy = intercept + slope *
    // t.
    double slope_numerator = 0.0;
    for (std::size_t i = 0; i < time_points_.size(); ++i)
      {
        // FIXME: We could compute std::vector<double> of
        // (time_points_[i] - time_points_mean_) in SetTimePoints.
        slope_numerator
            += (time_points_[i] - time_points_mean_) * (logy[i] - logy_mean);
      }
    const double slope = slope_numerator / slope_denominator_; // -b
    // Avoid returning a negative TIA for the pixel.
    if (slope >= 0.0)
      // return static_cast<OutPixelType>(0.0);
      return 0.0f;
    const double intercept = logy_mean - slope * time_points_mean_; // log(A)
    const double b_est = -slope;
    // FIXME: Would it help to have a lower limit on b_est
    // corresponding to the physical half-life?
    const double A_est = std::exp(intercept);
    // Return the TIA in units of pixel units * seconds.
    const double time_integrated_activity = A_est * 60.0 * 60.0 / b_est;
    return static_cast<OutPixelType>(time_integrated_activity);
  }

private:
  std::vector<double> time_points_;
  double time_points_mean_;
  double slope_denominator_ = 0.0;
};
} // namespace spider

#endif // SPIDER_TIA_EXP_FIT_FUNCTOR_H
