// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025 South Australia Medical Imaging

#include "spect.h"

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

namespace
{

void
SetHalfLife(double half_life, gdcm::DataSet& ds)
{
  // Create or replace the RadiopharmaceuticalInformationSequence in
  // dataset DS with a single-item sequence containing only the DICOM
  // attribute RadionuclideHalfLife set to HALF_LIFE.
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

TEST(GetAcquisitionTimestampTest, Example)
{
  std::string date = "19930822";
  gdcm::Attribute<0x0008, 0x0022> ad; // AcquisitionDate
  ad.SetValue(date);

  std::string time = "070907.0705 ";
  gdcm::Attribute<0x0008, 0x0032> at; // AcquisitionTime
  at.SetValue(time);

  gdcm::DataSet ds;
  ds.Insert(ad.GetAsDataElement());
  ds.Insert(at.GetAsDataElement());

  EXPECT_EQ(spider::GetAcquisitionTimestamp(ds), date + time);
}

TEST(GetAcquisitionTimestampTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_EQ(r.Read(), true);
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetAcquisitionTimestamp(ds), "20181105122601.000000 ");
}

TEST(GetHalfLifeTest, Example)
{
  double half_life = 574300.0;
  gdcm::DataSet ds;
  SetHalfLife(half_life, ds);
  EXPECT_EQ(spider::GetHalfLife(ds), half_life);
}

TEST(GetHalfLifeTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_EQ(r.Read(), true);
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  EXPECT_EQ(spider::GetHalfLife(ds), 1223.0);
}

TEST(ReadDicomSpectTest, Example)
{
  std::string date = "19930822";
  gdcm::Attribute<0x0008, 0x0022> at_date; // AcquisitionDate
  at_date.SetValue(date);

  std::string time = "070907.0705 ";
  gdcm::Attribute<0x0008, 0x0032> at_time; // AcquisitionTime
  at_time.SetValue(time);

  gdcm::DataSet ds;
  ds.Insert(at_date.GetAsDataElement());
  ds.Insert(at_time.GetAsDataElement());

  double half_life = 574300.0;
  SetHalfLife(half_life, ds);

  std::string correct_str = "ADMIN ";
  gdcm::Attribute<0x0054, 0x1102> at_correct; // DecayCorrection
  at_correct.SetValue(correct_str);
  ds.Insert(at_correct.GetAsDataElement());

  spider::Spect spect = spider::ReadDicomSpect(ds);

  EXPECT_EQ(spect.acquisition_timestamp, date + time);
  EXPECT_EQ(spect.half_life, half_life);
  EXPECT_EQ(spect.decay_correction_method, correct_str);
  EXPECT_EQ(spect.patient_name, "");
}

TEST(ReadDicomSpectTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_EQ(r.Read(), true);
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect spect = spider::ReadDicomSpect(ds);

  EXPECT_EQ(spect.acquisition_timestamp, "20181105122601.000000 ");
  EXPECT_EQ(spect.half_life, 1223.0);
  EXPECT_EQ(spect.decay_correction_method, "START ");
  EXPECT_EQ(spect.patient_name, "C11Phantom");
}

TEST(GetFirstLineTest, WithNewline)
{
  EXPECT_EQ(spider::GetFirstLine("Hello,\nWorld"), "Hello,");
}

TEST(GetFirstLineTest, WithoutNewline)
{
  std::string s = "Hello, World";
  EXPECT_EQ(spider::GetFirstLine(s), s);
}

TEST(WriteSpectsTest, Example)
{
  spider::Spect s1;
  s1.patient_name = "DOE^JOHN";
  s1.acquisition_timestamp = "20241013123250.1123 ";
  s1.decay_correction_method = "START ";
  s1.half_life = 23.11;

  spider::Spect s2;
  s2.patient_name = "DOE^JANE";
  s2.acquisition_timestamp = "2022031102";
  s2.decay_correction_method = "NONE";
  s2.half_life = 23121;

  std::ostringstream oss;
  spider::WriteSpects({ s1, s2 }, oss);

  std::string ans{ "This file was created by Spider.\n"
                   "If you edit it by hand, you could mess it up.\n"
                   "\nDOE^JOHN\n20241013123250.1123 \nSTART \n23.11\n"
                   "\nDOE^JANE\n2022031102\nNONE\n23121\n" };

  EXPECT_EQ(oss.str(), ans);
}

TEST(ReadDicomAttributesTest, RealDataset)
{
  gdcm::Reader r;
  r.SetFileName(kTestFilename);
  ASSERT_EQ(r.Read(), true);
  const gdcm::DataSet& ds = r.GetFile().GetDataSet();
  spider::Spect s1 = spider::ReadDicomSpect(ds);
  spider::Spect s2 = spider::ReadDicomSpect(ds);
  std::ostringstream oss;
  spider::WriteSpects({ s1, s2 }, oss);

  std::string ans{ "This file was created by Spider.\n"
                   "If you edit it by hand, you could mess it up.\n"
                   "\nC11Phantom\n20181105122601.000000 \nSTART \n1223\n"
                   "\nC11Phantom\n20181105122601.000000 \nSTART \n1223\n" };

  EXPECT_EQ(oss.str(), ans);
}

TEST(ReadSpectsTest, Example)
{
  std::istringstream input("This file was created by Spider.\n"
                           "If you edit it by hand, you could mess it up.\n"
                           "\nDOE^JOHN\n20241013123250.1123 \nSTART \n23.11\n"
                           "\nDOE^JANE\n2022031102\nNONE\n23121\n");
  std::vector<spider::Spect> spects = spider::ReadSpects(input);

  ASSERT_EQ(spects.size(), 2);
  EXPECT_EQ(spects[0].patient_name, "DOE^JOHN");
  EXPECT_EQ(spects[0].acquisition_timestamp, "20241013123250.1123 ");
  EXPECT_EQ(spects[0].decay_correction_method, "START ");
  EXPECT_EQ(spects[0].half_life, 23.11);
  EXPECT_EQ(spects[1].patient_name, "DOE^JANE");
  EXPECT_EQ(spects[1].acquisition_timestamp, "2022031102");
  EXPECT_EQ(spects[1].decay_correction_method, "NONE");
  EXPECT_EQ(spects[1].half_life, 23121);
}
