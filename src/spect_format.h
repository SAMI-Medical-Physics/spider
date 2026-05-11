// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 South Australia Medical Imaging

#ifndef SPIDER_SPECT_FORMAT_H
#define SPIDER_SPECT_FORMAT_H

#include <format> // std::formatter, std::format_parse_context,
                  // std::format_context, std::format_to

#include "spect.h"

// Teach std::format, std::print, etc. how to format a Spect.

namespace std
{
template <> struct formatter<spider::Spect>
{
  constexpr auto
  parse(format_parse_context& ctx)
  {
    return ctx.begin();
  }

  auto
  format(const spider::Spect& s, format_context& ctx) const
  {
    auto out = format_to(ctx.out(), "{{");
    if (s.patient_name.has_value())
      format_to(out, "patient_name={}, ", s.patient_name.value());
    if (s.radiopharmaceutical_start_date_time.has_value())
      format_to(out, "radiopharmaceutical_start_date_time={}, ",
                s.radiopharmaceutical_start_date_time.value());
    if (s.acquisition_date.has_value())
      format_to(out, "acquisition_date={}, ", s.acquisition_date.value());
    if (s.acquisition_time.has_value())
      format_to(out, "acquisition_time={}, ", s.acquisition_time.value());
    if (s.series_date.has_value())
      format_to(out, "series_date={}, ", s.series_date.value());
    if (s.series_time.has_value())
      format_to(out, "series_time={}, ", s.series_time.value());
    if (s.frame_reference_time.has_value())
      format_to(out, "{} ms, ", s.frame_reference_time.value());
    if (s.timezone_offset_from_utc.has_value())
      format_to(out, "timezone_offset_from_utc={}, ",
                s.timezone_offset_from_utc.value());
    if (s.decay_correction.has_value())
      format_to(out, "decay_correction={}, ", s.decay_correction.value());
    if (s.radionuclide_half_life.has_value())
      format_to(out, "radionuclide_half_life={} s",
                s.radionuclide_half_life.value());
    return format_to(out, "}}");
  }
};
} // namespace std

#endif // SPIDER_SPECT_FORMAT_H
