// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025, 2026 South Australia Medical Imaging

#include "spect.h"

#include <chrono>
#include <cmath>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <gdcmAttribute.h>
#include <gdcmDataSet.h>
#include <gdcmItem.h>
#include <gdcmReader.h>
#include <gdcmSequenceOfItems.h>
#include <gdcmSmartPointer.h>
#include <gdcmTag.h>
#include <gdcmVR.h>
#include <gtest/gtest.h>

#include "tz_compat.h"

namespace
{

void
SetRadiopharmaceuticalStartDateTime(std::string date_time, gdcm::DataSet& ds)
{
  // Create or replace the DICOM attribute
  // RadiopharmaceuticalInformationSequence in dataset DS with a
  // single-item sequence containing only the attribute
  // RadiopharmaceuticalStartDateTime set to DATE_TIME.
  gdcm::Attribute<0x0018, 0x1078> a; // RadiopharmaceuticalStartDateTime
  a.SetValue(date_time);

  gdcm::Item item;
  gdcm::DataSet& nds = item.GetNestedDataSet();
  nds.Insert(a.GetAsDataElement());

  gdcm::SmartPointer<gdcm::SequenceOfItems> seq = new gdcm::SequenceOfItems;
  seq->AddItem(item);

  gdcm::Tag seq_tag(0x0054, 0x0016); // RadiopharmaceuticalInformationSequence
  gdcm::DataElement seq_de(seq_tag);
  seq_de.SetVR(gdcm::VR::SQ);
  seq_de.SetValue(*seq);
  ds.Insert(seq_de);
}

void
SetRadionuclideHalfLife(double half_life, gdcm::DataSet& ds)
{
  // Create or replace the DICOM attribute
  // RadiopharmaceuticalInformationSequence in dataset DS with a
  // single-item sequence containing only the attribute
  // RadionuclideHalfLife set to HALF_LIFE.
  gdcm::Attribute<0x0018, 0x1075> a; // RadionuclideHalfLife
  a.SetValue(half_life);

  gdcm::Item item;
  gdcm::DataSet& nds = item.GetNestedDataSet();
  nds.Insert(a.GetAsDataElement());

  gdcm::SmartPointer<gdcm::SequenceOfItems> seq = new gdcm::SequenceOfItems;
  seq->AddItem(item);

  gdcm::Tag seq_tag(0x0054, 0x0016); // RadiopharmaceuticalInformationSequence
  gdcm::DataElement seq_de(seq_tag);
  seq_de.SetVR(gdcm::VR::SQ);
  seq_de.SetValue(*seq);
  // seq_de.SetVLToUndefined();
  ds.Insert(seq_de);
}

constexpr char kTestFilename[] = SPIDER_TEST_DATA_DIR
    "/PETAC/"
    "C11PHANTOM.PT.PET-MR_QA_MULTIPLE_PET_PHANTOM.30003.0077.2018.11.07.17.59."
    "46.799308.153692236.IMA";

} // namespace

TEST(GetPatientNameTest, Example)
{
  std::string patient_name = "DOE^JOHN";
  gdcm::Attribute<0x0010, 0x0010> at; // PatientName
  at.SetValue(patient_name);
  gdcm::DataSet ds;
  ds.Insert(at.GetAsDataElement());
  EXPECT_EQ(spider::GetPatientName(ds), patient_name);
}

TEST(GetPatientNameTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetPatientName(ds), "C11Phantom");
}

TEST(GetRadiopharmaceuticalStartDateTimeTest, Example)
{
  std::string date_time = "19930822134652";
  gdcm::DataSet ds;
  SetRadiopharmaceuticalStartDateTime(date_time, ds);
  EXPECT_EQ(spider::GetRadiopharmaceuticalStartDateTime(ds), date_time);
}

TEST(GetRadiopharmaceuticalStartDateTimeTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetRadiopharmaceuticalStartDateTime(ds),
            "20181105120000.000000 ");
}

TEST(GetAcquisitionDateTest, Example)
{
  std::string date = "19930822";
  gdcm::Attribute<0x0008, 0x0022> at; // AcquisitionDate
  at.SetValue(date);
  gdcm::DataSet ds;
  ds.Insert(at.GetAsDataElement());
  EXPECT_EQ(spider::GetAcquisitionDate(ds), date);
}

TEST(GetAcquisitionDateTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetAcquisitionDate(ds), "20181105");
}

TEST(GetAcquisitionTimeTest, Example)
{
  std::string time = "070907.0705 ";
  gdcm::Attribute<0x0008, 0x0032> at; // AcquisitionTime
  at.SetValue(time);
  gdcm::DataSet ds;
  ds.Insert(at.GetAsDataElement());
  EXPECT_EQ(spider::GetAcquisitionTime(ds), time);
}

TEST(GetAcquisitionTimeTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetAcquisitionTime(ds), "122601.000000 ");
}

TEST(GetSeriesDateTest, Example)
{
  std::string date = "19930822";
  gdcm::Attribute<0x0008, 0x0021> at; // SeriesDate
  at.SetValue(date);
  gdcm::DataSet ds;
  ds.Insert(at.GetAsDataElement());
  EXPECT_EQ(spider::GetSeriesDate(ds), date);
}

TEST(GetSeriesDateTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetSeriesDate(ds), "20181105");
}

TEST(GetSeriesTimeTest, Example)
{
  std::string time = "070907.0705 ";
  gdcm::Attribute<0x0008, 0x0031> at; // SeriesTime
  at.SetValue(time);
  gdcm::DataSet ds;
  ds.Insert(at.GetAsDataElement());
  EXPECT_EQ(spider::GetSeriesTime(ds), time);
}

TEST(GetSeriesTimeTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetSeriesTime(ds), "122601.000000 ");
}

TEST(GetFrameReferenceTimeTest, Example)
{
  double frt = 12310.0;
  gdcm::Attribute<0x0054, 0x1300> at; // FrameReferenceTime
  at.SetValue(frt);
  gdcm::DataSet ds;
  ds.Insert(at.GetAsDataElement());
  EXPECT_EQ(spider::GetFrameReferenceTime(ds), frt);
}

TEST(GetFrameReferenceTimeTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetFrameReferenceTime(ds), 566124.53848358);
}

TEST(GetTimezoneOffsetFromUtcTest, Example)
{
  std::string offset = "+0200 ";
  gdcm::Attribute<0x0008, 0x0201> at; // TimezoneOffsetFromUtc
  at.SetValue(offset);
  gdcm::DataSet ds;
  ds.Insert(at.GetAsDataElement());
  EXPECT_EQ(spider::GetTimezoneOffsetFromUtc(ds), offset);
}

TEST(GetTimezoneOffsetFromUtcTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_FALSE(spider::GetTimezoneOffsetFromUtc(ds).has_value());
}

TEST(GetDecayCorrectionTest, Example)
{
  std::string decay_correction = "ADMIN ";
  gdcm::Attribute<0x0054, 0x1102> at; // DecayCorrection
  at.SetValue(decay_correction);
  gdcm::DataSet ds;
  ds.Insert(at.GetAsDataElement());
  EXPECT_EQ(spider::GetDecayCorrection(ds), decay_correction);
}

TEST(GetDecayCorrectionTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetDecayCorrection(ds), "START ");
}

TEST(GetRadionuclideHalfLifeTest, AttributePresent)
{
  double half_life = 574300.0;
  gdcm::DataSet ds;
  SetRadionuclideHalfLife(half_life, ds);
  EXPECT_EQ(spider::GetRadionuclideHalfLife(ds), half_life);
}

TEST(GetRadionuclideHalfLifeTest, AttributeAbsent)
{
  gdcm::DataSet ds;
  EXPECT_EQ(spider::GetRadionuclideHalfLife(ds), std::nullopt);
}

TEST(GetRadionuclideHalfLifeTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetRadionuclideHalfLife(ds), 1223.0);
}

TEST(ReadDicomSpectTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);

  EXPECT_EQ(spect.patient_name, "C11Phantom");
  EXPECT_EQ(spect.radiopharmaceutical_start_date_time,
            "20181105120000.000000 ");
  EXPECT_EQ(spect.acquisition_date, "20181105");
  EXPECT_EQ(spect.acquisition_time, "122601.000000 ");
  EXPECT_EQ(spect.series_date, "20181105");
  EXPECT_EQ(spect.series_time, "122601.000000 ");
  EXPECT_EQ(spect.frame_reference_time, 566124.53848358);
  EXPECT_FALSE(spect.timezone_offset_from_utc.has_value());
  EXPECT_EQ(spect.decay_correction, "START ");
  EXPECT_EQ(spect.radionuclide_half_life, 1223.0);
}

TEST(ParseDicomDateTest, Example)
{
  std::optional<spider::DateComplete> date
      = spider::ParseDicomDate("19930822");
  ASSERT_TRUE(date.has_value());
  EXPECT_EQ(date.value().year, 1993);
  EXPECT_EQ(date.value().month, 8);
  EXPECT_EQ(date.value().day, 22);
}

TEST(ParseDicomDateTest, NonFourDigitYear)
{
  std::optional<spider::DateComplete> date
      = spider::ParseDicomDate("00310822");
  ASSERT_TRUE(date.has_value());
  EXPECT_EQ(date.value().year, 31);
  EXPECT_EQ(date.value().month, 8);
  EXPECT_EQ(date.value().day, 22);
}

TEST(ParseDicomTimeTest, HourOnly)
{
  std::optional<spider::DicomTime> time = spider::ParseDicomTime("07");
  ASSERT_TRUE(time.has_value());
  EXPECT_EQ(time.value().hour, 7);
  EXPECT_FALSE(time.value().minute.has_value());
  EXPECT_FALSE(time.value().second.has_value());
}

TEST(ParseDicomTimeTest, HourAndMinuteOnly)
{
  std::optional<spider::DicomTime> time = spider::ParseDicomTime("1009");
  ASSERT_TRUE(time.has_value());
  EXPECT_EQ(time.value().hour, 10);
  ASSERT_TRUE(time.value().minute.has_value());
  EXPECT_EQ(time.value().minute.value(), 9);
  EXPECT_FALSE(time.value().second.has_value());
}

TEST(ParseDicomTimeTest, HourAndMinuteAndSecondOnly)
{
  std::optional<spider::DicomTime> time = spider::ParseDicomTime("151305");
  ASSERT_TRUE(time.has_value());
  EXPECT_EQ(time.value().hour, 15);
  ASSERT_TRUE(time.value().minute.has_value());
  EXPECT_EQ(time.value().minute.value(), 13);
  ASSERT_TRUE(time.value().second.has_value());
  EXPECT_EQ(time.value().second.value(), 5);
}

TEST(ParseDicomTimeTest, HourAndMinuteAndSecondAndFraction)
{
  // If DICOM TM value contains a fractional part of a second, that
  // component is ignored.
  std::optional<spider::DicomTime> time
      = spider::ParseDicomTime("030914.0103");
  ASSERT_TRUE(time.has_value());
  EXPECT_EQ(time.value().hour, 3);
  ASSERT_TRUE(time.value().minute.has_value());
  EXPECT_EQ(time.value().minute.value(), 9);
  ASSERT_TRUE(time.value().second.has_value());
  EXPECT_EQ(time.value().second.value(), 14);
}

TEST(ParseDicomDateTimeTest, YearOnly)
{
  std::optional<spider::DicomDateTime> date_time
      = spider::ParseDicomDateTime("2023");
  ASSERT_TRUE(date_time.has_value());
  EXPECT_EQ(date_time.value().year, 2023);
  EXPECT_FALSE(date_time.value().month.has_value());
  EXPECT_FALSE(date_time.value().day.has_value());
  EXPECT_FALSE(date_time.value().hour.has_value());
  EXPECT_FALSE(date_time.value().minute.has_value());
  EXPECT_FALSE(date_time.value().second.has_value());

  // Table 6.2-1, Date Time VR, Note 5: "The offset may be included
  // regardless of null components; e.g., 2007-0500 is a legal value."
  std::optional<spider::DicomDateTime> date_time_with_offset
      = spider::ParseDicomDateTime("2007-0500");
  ASSERT_TRUE(date_time_with_offset.has_value());
  EXPECT_EQ(date_time_with_offset.value().year, 2007);
  EXPECT_FALSE(date_time_with_offset.value().month.has_value());
  EXPECT_FALSE(date_time_with_offset.value().day.has_value());
  EXPECT_FALSE(date_time_with_offset.value().hour.has_value());
  EXPECT_FALSE(date_time_with_offset.value().minute.has_value());
  EXPECT_FALSE(date_time_with_offset.value().second.has_value());
  ASSERT_TRUE(date_time_with_offset.value().utc_offset.has_value());
  EXPECT_EQ(date_time_with_offset.value().utc_offset.value(),
            std::chrono::minutes{ -5 * 60 });
}

TEST(ParseDicomDateTimeTest, YearMonthOnly)
{
  std::optional<spider::DicomDateTime> date_time
      = spider::ParseDicomDateTime("202303");
  ASSERT_TRUE(date_time.has_value());
  EXPECT_EQ(date_time.value().year, 2023);
  ASSERT_TRUE(date_time.value().month.has_value());
  EXPECT_EQ(date_time.value().month.value(), 3);
  EXPECT_FALSE(date_time.value().day.has_value());
  EXPECT_FALSE(date_time.value().hour.has_value());
  EXPECT_FALSE(date_time.value().minute.has_value());
  EXPECT_FALSE(date_time.value().second.has_value());

  std::optional<spider::DicomDateTime> date_time_with_offset
      = spider::ParseDicomDateTime("202303-0500");
  ASSERT_TRUE(date_time_with_offset.has_value());
  EXPECT_EQ(date_time_with_offset.value().year, 2023);
  ASSERT_TRUE(date_time_with_offset.value().month.has_value());
  EXPECT_EQ(date_time_with_offset.value().month.value(), 3);
  EXPECT_FALSE(date_time_with_offset.value().day.has_value());
  EXPECT_FALSE(date_time_with_offset.value().hour.has_value());
  EXPECT_FALSE(date_time_with_offset.value().minute.has_value());
  EXPECT_FALSE(date_time_with_offset.value().second.has_value());
  ASSERT_TRUE(date_time_with_offset.value().utc_offset.has_value());
  EXPECT_EQ(date_time_with_offset.value().utc_offset.value(),
            std::chrono::minutes{ -5 * 60 });
}

TEST(ParseDicomDateTimeTest, YearMonthDayOnly)
{
  std::optional<spider::DicomDateTime> date_time
      = spider::ParseDicomDateTime("20230314");
  ASSERT_TRUE(date_time.has_value());
  EXPECT_EQ(date_time.value().year, 2023);
  ASSERT_TRUE(date_time.value().month.has_value());
  EXPECT_EQ(date_time.value().month.value(), 3);
  ASSERT_TRUE(date_time.value().day.has_value());
  EXPECT_EQ(date_time.value().day.value(), 14);
  EXPECT_FALSE(date_time.value().hour.has_value());
  EXPECT_FALSE(date_time.value().minute.has_value());
  EXPECT_FALSE(date_time.value().second.has_value());

  std::optional<spider::DicomDateTime> date_time_with_offset
      = spider::ParseDicomDateTime("20230314-0500");
  ASSERT_TRUE(date_time_with_offset.has_value());
  EXPECT_EQ(date_time_with_offset.value().year, 2023);
  ASSERT_TRUE(date_time_with_offset.value().month.has_value());
  EXPECT_EQ(date_time_with_offset.value().month.value(), 3);
  ASSERT_TRUE(date_time_with_offset.value().day.has_value());
  EXPECT_EQ(date_time_with_offset.value().day.value(), 14);
  EXPECT_FALSE(date_time_with_offset.value().hour.has_value());
  EXPECT_FALSE(date_time_with_offset.value().minute.has_value());
  EXPECT_FALSE(date_time_with_offset.value().second.has_value());
  ASSERT_TRUE(date_time_with_offset.value().utc_offset.has_value());
  EXPECT_EQ(date_time_with_offset.value().utc_offset.value(),
            std::chrono::minutes{ -5 * 60 });
}

TEST(ParseDicomDateTimeTest, YearMonthDayHourOnly)
{
  std::optional<spider::DicomDateTime> date_time
      = spider::ParseDicomDateTime("2023031409");
  ASSERT_TRUE(date_time.has_value());
  EXPECT_EQ(date_time.value().year, 2023);
  ASSERT_TRUE(date_time.value().month.has_value());
  EXPECT_EQ(date_time.value().month.value(), 3);
  ASSERT_TRUE(date_time.value().day.has_value());
  EXPECT_EQ(date_time.value().day.value(), 14);
  ASSERT_TRUE(date_time.value().hour.has_value());
  EXPECT_EQ(date_time.value().hour.value(), 9);
  EXPECT_FALSE(date_time.value().minute.has_value());
  EXPECT_FALSE(date_time.value().second.has_value());

  std::optional<spider::DicomDateTime> date_time_with_offset
      = spider::ParseDicomDateTime("2023031409-0500");
  ASSERT_TRUE(date_time_with_offset.has_value());
  EXPECT_EQ(date_time_with_offset.value().year, 2023);
  ASSERT_TRUE(date_time_with_offset.value().month.has_value());
  EXPECT_EQ(date_time_with_offset.value().month.value(), 3);
  ASSERT_TRUE(date_time_with_offset.value().day.has_value());
  EXPECT_EQ(date_time_with_offset.value().day.value(), 14);
  ASSERT_TRUE(date_time_with_offset.value().hour.has_value());
  EXPECT_EQ(date_time_with_offset.value().hour.value(), 9);
  EXPECT_FALSE(date_time_with_offset.value().minute.has_value());
  EXPECT_FALSE(date_time_with_offset.value().second.has_value());
  ASSERT_TRUE(date_time_with_offset.value().utc_offset.has_value());
  EXPECT_EQ(date_time_with_offset.value().utc_offset.value(),
            std::chrono::minutes{ -5 * 60 });
}

TEST(ParseDicomDateTimeTest, YearMonthDayHourMinuteOnly)
{
  std::optional<spider::DicomDateTime> date_time
      = spider::ParseDicomDateTime("202303140945");
  ASSERT_TRUE(date_time.has_value());
  EXPECT_EQ(date_time.value().year, 2023);
  ASSERT_TRUE(date_time.value().month.has_value());
  EXPECT_EQ(date_time.value().month.value(), 3);
  ASSERT_TRUE(date_time.value().day.has_value());
  EXPECT_EQ(date_time.value().day.value(), 14);
  ASSERT_TRUE(date_time.value().hour.has_value());
  EXPECT_EQ(date_time.value().hour.value(), 9);
  ASSERT_TRUE(date_time.value().minute.has_value());
  EXPECT_EQ(date_time.value().minute.value(), 45);
  EXPECT_FALSE(date_time.value().second.has_value());

  std::optional<spider::DicomDateTime> date_time_with_offset
      = spider::ParseDicomDateTime("202303140945-0500");
  ASSERT_TRUE(date_time_with_offset.has_value());
  EXPECT_EQ(date_time_with_offset.value().year, 2023);
  ASSERT_TRUE(date_time_with_offset.value().month.has_value());
  EXPECT_EQ(date_time_with_offset.value().month.value(), 3);
  ASSERT_TRUE(date_time_with_offset.value().day.has_value());
  EXPECT_EQ(date_time_with_offset.value().day.value(), 14);
  ASSERT_TRUE(date_time_with_offset.value().hour.has_value());
  EXPECT_EQ(date_time_with_offset.value().hour.value(), 9);
  ASSERT_TRUE(date_time_with_offset.value().minute.has_value());
  EXPECT_EQ(date_time_with_offset.value().minute.value(), 45);
  EXPECT_FALSE(date_time_with_offset.value().second.has_value());
  ASSERT_TRUE(date_time_with_offset.value().utc_offset.has_value());
  EXPECT_EQ(date_time_with_offset.value().utc_offset.value(),
            std::chrono::minutes{ -5 * 60 });
}

TEST(ParseDicomDateTimeTest, YearMonthDayHourMinuteSecondOnly)
{
  std::optional<spider::DicomDateTime> date_time
      = spider::ParseDicomDateTime("20230314094556");
  ASSERT_TRUE(date_time.has_value());
  EXPECT_EQ(date_time.value().year, 2023);
  ASSERT_TRUE(date_time.value().month.has_value());
  EXPECT_EQ(date_time.value().month.value(), 3);
  ASSERT_TRUE(date_time.value().day.has_value());
  EXPECT_EQ(date_time.value().day.value(), 14);
  ASSERT_TRUE(date_time.value().hour.has_value());
  EXPECT_EQ(date_time.value().hour.value(), 9);
  ASSERT_TRUE(date_time.value().minute.has_value());
  EXPECT_EQ(date_time.value().minute.value(), 45);
  ASSERT_TRUE(date_time.value().second.has_value());
  EXPECT_EQ(date_time.value().second.value(), 56);

  std::optional<spider::DicomDateTime> date_time_with_offset
      = spider::ParseDicomDateTime("20230314094556-0500");
  ASSERT_TRUE(date_time_with_offset.has_value());
  EXPECT_EQ(date_time_with_offset.value().year, 2023);
  ASSERT_TRUE(date_time_with_offset.value().month.has_value());
  EXPECT_EQ(date_time_with_offset.value().month.value(), 3);
  ASSERT_TRUE(date_time_with_offset.value().day.has_value());
  EXPECT_EQ(date_time_with_offset.value().day.value(), 14);
  ASSERT_TRUE(date_time_with_offset.value().hour.has_value());
  EXPECT_EQ(date_time_with_offset.value().hour.value(), 9);
  ASSERT_TRUE(date_time_with_offset.value().minute.has_value());
  EXPECT_EQ(date_time_with_offset.value().minute.value(), 45);
  ASSERT_TRUE(date_time_with_offset.value().second.has_value());
  EXPECT_EQ(date_time_with_offset.value().second.value(), 56);
  ASSERT_TRUE(date_time_with_offset.value().utc_offset.has_value());
  EXPECT_EQ(date_time_with_offset.value().utc_offset.value(),
            std::chrono::minutes{ -5 * 60 });
}

TEST(ParseDicomDateTimeTest, YearMonthDayHourMinuteSecondFraction)
{
  // If DICOM DT value contains a fractional part of a second, that
  // component is ignored.
  std::optional<spider::DicomDateTime> date_time
      = spider::ParseDicomDateTime("20230314094556.30");
  ASSERT_TRUE(date_time.has_value());
  EXPECT_EQ(date_time.value().year, 2023);
  ASSERT_TRUE(date_time.value().month.has_value());
  EXPECT_EQ(date_time.value().month.value(), 3);
  ASSERT_TRUE(date_time.value().day.has_value());
  EXPECT_EQ(date_time.value().day.value(), 14);
  ASSERT_TRUE(date_time.value().hour.has_value());
  EXPECT_EQ(date_time.value().hour.value(), 9);
  ASSERT_TRUE(date_time.value().minute.has_value());
  EXPECT_EQ(date_time.value().minute.value(), 45);
  ASSERT_TRUE(date_time.value().second.has_value());
  EXPECT_EQ(date_time.value().second.value(), 56);

  std::optional<spider::DicomDateTime> date_time_with_offset
      = spider::ParseDicomDateTime("20230314094556.30-0500");
  ASSERT_TRUE(date_time_with_offset.has_value());
  EXPECT_EQ(date_time_with_offset.value().year, 2023);
  ASSERT_TRUE(date_time_with_offset.value().month.has_value());
  EXPECT_EQ(date_time_with_offset.value().month.value(), 3);
  ASSERT_TRUE(date_time_with_offset.value().day.has_value());
  EXPECT_EQ(date_time_with_offset.value().day.value(), 14);
  ASSERT_TRUE(date_time_with_offset.value().hour.has_value());
  EXPECT_EQ(date_time_with_offset.value().hour.value(), 9);
  ASSERT_TRUE(date_time_with_offset.value().minute.has_value());
  EXPECT_EQ(date_time_with_offset.value().minute.value(), 45);
  ASSERT_TRUE(date_time_with_offset.value().second.has_value());
  EXPECT_EQ(date_time_with_offset.value().second.value(), 56);
  ASSERT_TRUE(date_time_with_offset.value().utc_offset.has_value());
  EXPECT_EQ(date_time_with_offset.value().utc_offset.value(),
            std::chrono::minutes{ -5 * 60 });
}

TEST(ParseDicomUtcOffsetTest, PositiveOffset)
{
  std::optional<std::chrono::minutes> offset
      = spider::ParseDicomUtcOffset("+0930");
  ASSERT_TRUE(offset.has_value());
  EXPECT_EQ(offset.value(), std::chrono::minutes{ 9 * 60 + 30 });
}

TEST(ParseDicomUtcOffsetTest, NegativeOffset)
{
  std::optional<std::chrono::minutes> offset
      = spider::ParseDicomUtcOffset("-0500");
  ASSERT_TRUE(offset.has_value());
  EXPECT_EQ(offset.value(), std::chrono::minutes{ -5 * 60 });
}

TEST(MakeDateCompleteTest, Example)
{
  spider::DicomDateTime date_time{ .year = 2023,
                                   .month = 2,
                                   .day = 14,
                                   .hour = std::nullopt,
                                   .minute = std::nullopt,
                                   .second = std::nullopt,
                                   .utc_offset = std::nullopt };
  auto date_complete = spider::MakeDateComplete(date_time);
  ASSERT_TRUE(date_complete.has_value());
  EXPECT_EQ(date_complete.value().year, 2023);
  EXPECT_EQ(date_complete.value().month, 2);
  EXPECT_EQ(date_complete.value().day, 14);
}

TEST(MakeTimeCompleteTest, DicomTime)
{
  spider::DicomTime dicom_time{ .hour = 14, .minute = 34, .second = 1 };
  auto time_complete = spider::MakeTimeComplete(dicom_time);
  ASSERT_TRUE(time_complete.has_value());
  EXPECT_EQ(time_complete.value().hour, 14);
  EXPECT_EQ(time_complete.value().minute, 34);
  EXPECT_EQ(time_complete.value().second, 1);
}

TEST(MakeTimeCompleteTest, DicomDateTime)
{
  spider::DicomDateTime date_time{ .year = 2020,
                                   .month = 2,
                                   .day = 4,
                                   .hour = 14,
                                   .minute = 34,
                                   .second = 1,
                                   .utc_offset = std::nullopt };
  auto time_complete = spider::MakeTimeComplete(date_time);
  ASSERT_TRUE(time_complete.has_value());
  EXPECT_EQ(time_complete.value().hour, 14);
  EXPECT_EQ(time_complete.value().minute, 34);
  EXPECT_EQ(time_complete.value().second, 1);
}

TEST(MakeZonedTimeTest, Example)
{
  spider::DateComplete date = { .year = 1997, .month = 7, .day = 15 };
  spider::TimeComplete time = { .hour = 16, .minute = 43, .second = 9 };
  const spider::tz::time_zone* tz = spider::tz::locate_zone("Asia/Tokyo");
  auto zt = spider::MakeZonedTime(date, time, *tz);
  ASSERT_TRUE(zt.has_value()) << spider::ToString(zt.error());

  spider::tz::local_seconds lt_valid
      = spider::tz::local_days{ spider::tz::year{ 1997 }
                                / spider::tz::month{ 7 }
                                / spider::tz::day{ 15 } }
        + std::chrono::hours{ 16 } + std::chrono::minutes{ 43 }
        + std::chrono::seconds{ 9 };
  auto zt_valid = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt_valid };
  EXPECT_EQ(zt.value(), zt_valid);
}

TEST(MakeZonedTimeTest, NonexistentLocalTime)
{
  // MakeZonedTime does not return a nonexistent local time, e.g. one
  // during a DST spring-forward.
  spider::DateComplete date = { .year = 2024, .month = 10, .day = 6 };
  spider::TimeComplete time = { .hour = 2, .minute = 45, .second = 0 };
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("Australia/Adelaide");
  auto zt_test = spider::MakeZonedTime(date, time, *tz);
  EXPECT_FALSE(zt_test.has_value());
}

TEST(MakeZonedTimeTest, NonexistentLocalTimeDifferentTimeZone)
{
  spider::DateComplete date = { .year = 2025, .month = 3, .day = 30 };
  spider::TimeComplete time = { .hour = 2, .minute = 20, .second = 0 };
  const spider::tz::time_zone* tz = spider::tz::locate_zone("Europe/Berlin");
  auto zt_test = spider::MakeZonedTime(date, time, *tz);
  EXPECT_FALSE(zt_test.has_value());
}

TEST(MakeZonedTimeTest, AmbiguousLocalTime)
{
  // If the local time is ambiguous, e.g. due to a DST fall-back,
  // MakeZonedTime returns the earlier time point.
  spider::DateComplete date = { .year = 2024, .month = 4, .day = 7 };
  spider::TimeComplete time = { .hour = 2, .minute = 45, .second = 0 };
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("Australia/Adelaide");
  auto zt_beg = spider::MakeZonedTime(date, time, *tz);
  ASSERT_TRUE(zt_beg.has_value()) << spider::ToString(zt_beg.error());

  spider::tz::local_seconds lt_end
      = spider::tz::local_days{ spider::tz::year{ 2024 }
                                / spider::tz::month{ 4 }
                                / spider::tz::day{ 7 } }
        + std::chrono::hours{ 4 } + std::chrono::minutes{ 0 }
        + std::chrono::seconds{ 0 };
  auto zt_end = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt_end };
  std::chrono::seconds secs
      = zt_end.get_sys_time() - zt_beg.value().get_sys_time();
  // If the later time point was chosen, the difference would be 1
  // hour and 15 mins.
  EXPECT_EQ(secs, std::chrono::seconds{ (2 * 60 * 60) + (15 * 60) });
}

TEST(MakeSysTimeFromOffsetTest, PositiveOffset)
{
  spider::DateComplete d{ 2025, 1, 2 };
  spider::TimeComplete t{ 0, 0, 0 };
  std::chrono::minutes offset{ 9 * 60 + 30 };
  auto s = spider::MakeSysTimeFromOffset(d, t, offset);
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  std::chrono::sys_seconds expected
      = std::chrono::sys_days{ spider::tz::year{ 2025 } / 1 / 1 }
        + std::chrono::hours{ 14 } + std::chrono::minutes{ 30 };
  EXPECT_EQ(s.value(), expected);
}

TEST(MakeSysTimeFromOffsetTest, NegativeOffset)
{
  spider::DateComplete d{ 2025, 1, 2 };
  spider::TimeComplete t{ 0, 0, 0 };
  std::chrono::minutes offset{ -(9 * 60 + 30) };
  auto s = spider::MakeSysTimeFromOffset(d, t, offset);
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  std::chrono::sys_seconds expected
      = std::chrono::sys_days{ spider::tz::year{ 2025 } / 1 / 2 }
        + std::chrono::hours{ 9 } + std::chrono::minutes{ 30 };
  EXPECT_EQ(s.value(), expected);
}

TEST(MakeSysTimeFromOffsetOrTimeZoneTest, WithOffset)
{
  // If a UTC offset is provided, it is used and the time zone
  // argument has no effect.
  spider::DateComplete d{ 2025, 1, 2 };
  spider::TimeComplete t{ 0, 0, 0 };
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver"); // not used
  auto s = spider::MakeSysTimeFromOffsetOrTimeZone(d, t, "-0930", tz);
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  std::chrono::sys_seconds expected
      = std::chrono::sys_days{ spider::tz::year{ 2025 } / 1 / 2 }
        + std::chrono::hours{ 9 } + std::chrono::minutes{ 30 };
  EXPECT_EQ(s.value(), expected);
}

TEST(MakeSysTimeFromOffsetOrTimeZoneTest, NoOffset)
{
  // If the UTC offset is empty, the time zone is used.
  spider::DateComplete d{ 2025, 1, 2 };
  spider::TimeComplete t{ 15, 23, 12 };
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver");
  auto s = spider::MakeSysTimeFromOffsetOrTimeZone(d, t, std::nullopt, tz);
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  spider::tz::local_seconds lt
      = spider::tz::local_days{ spider::tz::year{ 2025 }
                                / spider::tz::month{ 1 }
                                / spider::tz::day{ 2 } }
        + std::chrono::hours{ 15 } + std::chrono::minutes{ 23 }
        + std::chrono::seconds{ 12 };
  auto zt = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt };
  EXPECT_EQ(s.value(), zt.get_sys_time());
}

TEST(MakeSysTimeFromDicomDateAndTimeTest, WithOffset)
{
  // If a UTC offset is provided, it is used and the time zone
  // argument has no effect.
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver"); // not used
  auto s = spider::MakeSysTimeFromDicomDateAndTime("20250102", "000000",
                                                   "+1030", tz);
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  std::chrono::sys_seconds expected
      = std::chrono::sys_days{ spider::tz::year{ 2025 } / 1 / 1 }
        + std::chrono::hours{ 13 } + std::chrono::minutes{ 30 };
  EXPECT_EQ(s.value(), expected);
}

TEST(MakeSysTimeFromDicomDateAndTimeTest, NoOffset)
{
  // If the UTC offset is empty, the time zone is used.
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver");
  auto s = spider::MakeSysTimeFromDicomDateAndTime("20230314", "123301",
                                                   std::nullopt, tz);
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  spider::tz::local_seconds lt
      = spider::tz::local_days{ spider::tz::year{ 2023 }
                                / spider::tz::month{ 3 }
                                / spider::tz::day{ 14 } }
        + std::chrono::hours{ 12 } + std::chrono::minutes{ 33 }
        + std::chrono::seconds{ 1 };
  auto zt = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt };
  EXPECT_EQ(s.value(), zt.get_sys_time());
}

TEST(MakeSysTimeFromDicomDateTimeTest, OffsetInDateTime)
{
  // If the DT value contains a UTC offset suffix, it is used and the
  // separate offset argument and time zone argument have no effect.
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver");
  auto s = spider::MakeSysTimeFromDicomDateTime("20250102000000+0330",
                                                "-0500", // not used
                                                tz);     // not used
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  std::chrono::sys_seconds expected
      = std::chrono::sys_days{ spider::tz::year{ 2025 } / 1 / 1 }
        + std::chrono::hours{ 20 } + std::chrono::minutes{ 30 };
  EXPECT_EQ(s.value(), expected);
}

TEST(MakeSysTimeFromDicomDateTimeTest, SeparateOffset)
{
  // If the DT value does not contain a UTC offset suffix, the
  // separate offset argument is used.  If the separate offset
  // argument is not empty, the time zone argument has no effect.
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver");
  auto s = spider::MakeSysTimeFromDicomDateTime("20250102000000",
                                                "-0500", // used
                                                tz);     // not used
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  std::chrono::sys_seconds expected
      = std::chrono::sys_days{ spider::tz::year{ 2025 } / 1 / 2 }
        + std::chrono::hours{ 5 };
  EXPECT_EQ(s.value(), expected);
}

TEST(MakeSysTimeFromDicomDateTimeTest, NoOffset)
{
  // If the DT value does not include a UTC offset suffix, and the
  // separate UTC offset argument is empty, the time zone is used.
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver");
  auto s = spider::MakeSysTimeFromDicomDateTime("20230314123301", std::nullopt,
                                                tz);
  ASSERT_TRUE(s.has_value()) << spider::ToString(s.error());

  spider::tz::local_seconds lt
      = spider::tz::local_days{ spider::tz::year{ 2023 }
                                / spider::tz::month{ 3 }
                                / spider::tz::day{ 14 } }
        + std::chrono::hours{ 12 } + std::chrono::minutes{ 33 }
        + std::chrono::seconds{ 1 };
  auto zt = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt };
  EXPECT_EQ(s.value(), zt.get_sys_time());
}

TEST(MakeAcquisitionSysTimeTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);
  // This test DICOM file does not have a TimezoneOffsetFromUtc, so a
  // caller-supplied time zone is used.
  EXPECT_EQ(spect.acquisition_date, "20181105");
  EXPECT_EQ(spect.acquisition_time, "122601.000000 ");
  EXPECT_FALSE(spect.timezone_offset_from_utc.has_value());
  // Hand-craft a sys time consistent with spect.acquisition_date and
  // spect.acquisition_time.
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver");
  spider::tz::local_seconds lt
      = spider::tz::local_days{ spider::tz::year{ 2018 }
                                / spider::tz::month{ 11 }
                                / spider::tz::day{ 5 } }
        + std::chrono::hours{ 12 } + std::chrono::minutes{ 26 }
        + std::chrono::seconds{ 1 };
  auto zt = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt };

  auto st = spider::MakeAcquisitionSysTime(spect, tz);
  ASSERT_TRUE(st.has_value()) << spider::ToString(st.error());
  EXPECT_EQ(st.value(), zt.get_sys_time());
}

TEST(MakeRadiopharmaceuticalStartSysTimeTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);
  // This test DICOM file has a RadiopharmaceuticalStartDateTime
  // without a UTC offset suffix and TimezoneOffsetFromUtc is absent,
  // so a caller-supplied time zone is used.
  EXPECT_EQ(spect.radiopharmaceutical_start_date_time,
            "20181105120000.000000 ");
  EXPECT_FALSE(spect.timezone_offset_from_utc.has_value());
  // Hand-craft a sys time consistent with
  // spect.radiopharmaceutical_start_date_time.
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Vancouver");
  spider::tz::local_seconds lt = spider::tz::local_days{
    spider::tz::year{ 2018 } / spider::tz::month{ 11 } / spider::tz::day{ 5 }
  } + std::chrono::hours{ 12 };
  auto zt = spider::tz::zoned_time<std::chrono::seconds>{ tz, lt };

  auto st = spider::MakeRadiopharmaceuticalStartSysTime(spect, tz);
  ASSERT_TRUE(st.has_value()) << spider::ToString(st.error());
  EXPECT_EQ(st.value(), zt.get_sys_time());
}

TEST(ParseDicomDecayCorrectionTest, Parse)
{
  EXPECT_EQ(spider::ParseDicomDecayCorrection("NONE").value(),
            spider::DecayCorrection::kNone);
  EXPECT_EQ(spider::ParseDicomDecayCorrection("START ").value(),
            spider::DecayCorrection::kStart);
  EXPECT_EQ(spider::ParseDicomDecayCorrection("ADMIN ").value(),
            spider::DecayCorrection::kAdmin);
  EXPECT_FALSE(spider::ParseDicomDecayCorrection("").has_value());
}

TEST(ComputeDecayFactorNoneTest, ExtraHalfLife)
{
  // The decay factor returned by ComputeDecayFactorNone
  // decay-corrects from frame reference time to acquisition start.
  // If the frame reference time is extended by one half-life, the
  // decay factor should double.
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);

  // This test DICOM file does not have the attribute
  // TimezoneOffsetFromUtc, so ComputeDecayFactorNone must be supplied
  // with a time zone.
  const spider::tz::time_zone* tz
      = spider::tz::locate_zone("America/Barbados");
  auto df = spider::ComputeDecayFactorNone(spect, tz);
  ASSERT_TRUE(df.has_value()) << spider::ToString(df.error());

  ASSERT_TRUE(spect.frame_reference_time.has_value());
  ASSERT_TRUE(spect.radionuclide_half_life.has_value());
  // Spect::frame_reference_time is in milliseconds and
  // Spect::radionuclide_half_life is in seconds.
  spect.frame_reference_time.value()
      += spect.radionuclide_half_life.value() * 1000.0;
  auto df_twice = spider::ComputeDecayFactorNone(spect, tz);
  ASSERT_TRUE(df_twice.has_value()) << spider::ToString(df_twice.error());
  EXPECT_DOUBLE_EQ(df_twice.value(), 2.0 * df.value());
}

TEST(ComputeDecayFactorAdminTest, UtcOffsetSuffix)
{
  // The decay factor returned by ComputeDecayFactorAdmin
  // decay-corrects from administration (radiopharmaceutical start
  // date time) to acquisition start.  If this time difference is
  // extended by dt, the decay factor should decrease by a factor
  // corresponding to physcial decay for dt.  This test modifies the
  // time difference by adding a UTC offset suffix to
  // Spect::radiopharmaceutical_start_date_time so it is interpreted
  // in a different time zone to Spect::acquisiton_date and
  // Spect::acquisition_time.
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);

  // This test DICOM file does not have the attribute
  // TimezoneOffsetFromUtc, so ComputeDecayFactorAdmin must be
  // supplied with a time zone.
  const spider::tz::time_zone* tz = spider::tz::locate_zone("Asia/Singapore");
  auto df = spider::ComputeDecayFactorAdmin(spect, tz);
  ASSERT_TRUE(df.has_value()) << spider::ToString(df.error());

  // Now modify Spect::radiopharmaceutical_start_date_time.
  EXPECT_EQ(spect.radiopharmaceutical_start_date_time,
            "20181105120000.000000 ");
  // Asia/Singapore is +08:00 (and does not observe DST), so
  // specifying a UTC offset of +14:00 makes the administration 6
  // hours earlier.
  spect.radiopharmaceutical_start_date_time = "20181105120000.000000+1400";
  auto df_new = spider::ComputeDecayFactorAdmin(spect, tz);
  ASSERT_TRUE(df_new.has_value()) << spider::ToString(df_new.error());

  // Calculate the decay factor over 6 hours, noting that
  // Spect::radionuclide_half_life is in seconds.
  ASSERT_TRUE(spect.radionuclide_half_life.has_value());
  double df_extra = std::exp(-std::log(2) * (60.0 * 60.0 * 6.0)
                             / spect.radionuclide_half_life.value());

  EXPECT_DOUBLE_EQ(df_new.value(), df.value() * df_extra);
}

TEST(ComputeDecayFactorTest, DefersToNone)
{
  // Check that ComputeDecayFactor defers to ComputeDecayFactorNone
  // when Spect::decay_correction is "NONE".
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);
  spect.decay_correction = "NONE";
  const spider::tz::time_zone* tz = spider::tz::locate_zone("Australia/Perth");
  auto df_none = spider::ComputeDecayFactorNone(spect, tz);
  ASSERT_TRUE(df_none.has_value()) << spider::ToString(df_none.error());
  auto df = spider::ComputeDecayFactor(spect, tz);
  ASSERT_TRUE(df.has_value()) << spider::ToString(df.error());
  EXPECT_DOUBLE_EQ(df.value(), df_none.value());
}

TEST(ComputeDecayFactorTest, DecayCorrectionStart)
{
  // This test DICOM file has DecayCorrection START, so the decay
  // factor is unity by definition.
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);

  ASSERT_TRUE(spect.decay_correction.has_value());
  auto dc = spider::ParseDicomDecayCorrection(spect.decay_correction.value());
  ASSERT_TRUE(dc.has_value());
  EXPECT_EQ(dc.value(), spider::DecayCorrection::kStart);

  auto df = spider::ComputeDecayFactor(spect);
  ASSERT_TRUE(df.has_value()) << spider::ToString(df.error());
  EXPECT_DOUBLE_EQ(df.value(), 1.0);
}

TEST(ComputeDecayFactorTest, DefersToAdmin)
{
  // Check that ComputeDecayFactor defers to ComputeDecayFactorAdmin
  // when Spect::decay_correction is "ADMIN ".
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);
  spect.decay_correction = "ADMIN ";
  const spider::tz::time_zone* tz = spider::tz::locate_zone("Africa/Cairo");
  auto df_admin = spider::ComputeDecayFactorAdmin(spect, tz);
  ASSERT_TRUE(df_admin.has_value()) << spider::ToString(df_admin.error());
  auto df = spider::ComputeDecayFactor(spect, tz);
  ASSERT_TRUE(df.has_value()) << spider::ToString(df.error());
  EXPECT_DOUBLE_EQ(df.value(), df_admin.value());
}

TEST(UsesTimeZoneTest, DateAndTimeUsesTimeZone)
{
  // This test demonstrates that, for the given test DICOM file,
  // UsesTimeZone returns true and a caller-supplied time zone is used
  // when forming time points from DICOM DA and TM values in spect.
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);

  // This DICOM file does not have the attribute
  // TimezoneOffsetFromUtc, so a caller-supplied time zone is used to
  // form time points from DICOM DA and TM values in spect.
  EXPECT_TRUE(spider::UsesTimeZone(spect));
  // MakeAcquisitionSysTime forms a time point from the DICOM DA
  // AcquisitionDate and TM AcquisitionTime.  Since UsesTimeZone
  // returned true, MakeAcquisitionSysTime should return an error when
  // the caller does not supply a time zone.
  EXPECT_FALSE(spider::MakeAcquisitionSysTime(spect).has_value());

  // When the caller provides a time zone, a value is returned.
  auto st = spider::MakeAcquisitionSysTime(
      spect, spider::tz::locate_zone("Asia/Singapore"));
  ASSERT_TRUE(st.has_value()) << spider::ToString(st.error());
  // We know it uses the supplied time zone because a different time
  // zone gives a different result.
  auto st_other = spider::MakeAcquisitionSysTime(
      spect, spider::tz::locate_zone("America/Halifax"));
  ASSERT_TRUE(st_other.has_value()) << spider::ToString(st_other.error());
  EXPECT_NE(st.value(), st_other.value());
}

TEST(UsesTimeZoneTest, DateAndTimeDoesNotUseTimeZone)
{
  // This test demonstrates that, after modifying Spect to include a
  // TimezoneOffsetFromUtc, UsesTimeZone returns false and
  // caller-supplied time zones are not used when forming time points
  // from DICOM DA and TM values in spect.
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);

  // This DICOM file does not have the attribute
  // TimezoneOffsetFromUtc, so modify the parsed Spect.
  spect.timezone_offset_from_utc = "+0300";
  EXPECT_FALSE(spider::UsesTimeZone(spect));
  // MakeAcquisitionSysTime forms a time point from the DICOM DA
  // AcquisitionDate and TM AcquisitionTime.  Since UsesTimeZone
  // returned false, MakeAcquisitionSysTime should return a value even
  // when the caller does not supply a time zone.
  auto st = spider::MakeAcquisitionSysTime(spect);
  ASSERT_TRUE(st.has_value()) << spider::ToString(st.error());

  // When a time zone is provided, it has no effect on the returned
  // value. To demonstrate this, supply a time zone with a different
  // UTC offset (+08:00).
  auto st_tz = spider::MakeAcquisitionSysTime(
      spect, spider::tz::locate_zone("Asia/Singapore"));
  ASSERT_TRUE(st_tz.has_value()) << spider::ToString(st_tz.error());
  EXPECT_EQ(st.value(), st_tz.value());
}

TEST(UsesTimeZoneTest, DateTimeWithoutUtcSuffixUsesTimeZone)
{
  // This test demonstrates that, for the given test DICOM file,
  // UsesTimeZone returns true and a caller-supplied time zone is used
  // when forming a time point from a DICOM DT value without a UTC
  // offset suffix in Spect.
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);

  // This DICOM file does not have the attribute
  // TimezoneOffsetFromUtc, so a caller-supplied time zone is used to
  // form time points from DICOM DT values that lack a UTC offset
  // suffix.
  EXPECT_TRUE(spider::UsesTimeZone(spect));
  // MakeRadiopharmaceuticalStartSysTime forms a time point from the
  // DICOM DT RadiopharmaceuticalStartDateTime.  The
  // RadiopharmaceuticalStartDateTime in this DICOM file does not have
  // a UTC offset suffix.  Since UsesTimeZone returned true,
  // MakeRadiopharmaceuticalStartSysTime should return an error when
  // the caller does not supply a time zone.
  EXPECT_FALSE(spider::MakeRadiopharmaceuticalStartSysTime(spect).has_value());

  // When the caller provides a time zone, a value is returned.
  auto st = spider::MakeRadiopharmaceuticalStartSysTime(
      spect, spider::tz::locate_zone("Asia/Singapore"));
  ASSERT_TRUE(st.has_value()) << spider::ToString(st.error());
  // We know it uses the supplied time zone because a different time
  // zone gives a different result.
  auto st_other = spider::MakeRadiopharmaceuticalStartSysTime(
      spect, spider::tz::locate_zone("America/Halifax"));
  ASSERT_TRUE(st_other.has_value()) << spider::ToString(st_other.error());
  EXPECT_NE(st.value(), st_other.value());
}

TEST(UsesTimeZoneTest, DateTimeWithoutUtcSuffixDoesNotUseTimeZone)
{
  // This test demonstrates that, after modifying Spect to include a
  // TimezoneOffsetFromUtc, UsesTimeZone returns false and
  // caller-supplied time zones are not used when forming time points
  // from DICOM DT values without a UTC offset suffix in Spect.
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_TRUE(r.Read());
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);

  // This DICOM file does not have the attribute
  // TimezoneOffsetFromUtc, so modify the parsed Spect.
  spect.timezone_offset_from_utc = "+0300";
  EXPECT_FALSE(spider::UsesTimeZone(spect));
  // MakeRadiopharmaceuticalStartSysTime forms a time point from the
  // DICOM DT RadiopharmaceuticalStartDateTime.  The
  // RadiopharmaceuticalStartDateTime in this DICOM file does not have
  // a UTC offset suffix.  Since UsesTimeZone returned false,
  // MakeRadiopharmaceuticalStartSysTime should return a value even
  // when the caller does not supply a time zone.
  auto st = spider::MakeRadiopharmaceuticalStartSysTime(spect);
  ASSERT_TRUE(st.has_value()) << spider::ToString(st.error());

  // When a time zone is provided, it has no effect on the returned
  // value.  To demonstrate this, supply a time zone with a different
  // UTC offset (+08:00).
  auto st_tz = spider::MakeRadiopharmaceuticalStartSysTime(
      spect, spider::tz::locate_zone("Asia/Singapore"));
  ASSERT_TRUE(st_tz.has_value()) << spider::ToString(st_tz.error());
  EXPECT_EQ(st.value(), st_tz.value());
}
