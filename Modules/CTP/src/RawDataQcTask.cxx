// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

///
/// \file   RawDataQcTask.cxx
/// \author Marek Bombara
/// \author Lucia Anna Tarasovicova
///

#include <TCanvas.h>
#include <TH1.h>

#include "QualityControl/QcInfoLogger.h"
#include "CTP/RawDataQcTask.h"
#include "DetectorsRaw/RDHUtils.h"
#include "Headers/RAWDataHeader.h"
#include "DPLUtils/DPLRawParser.h"
#include "DataFormatsCTP/Digits.h"
#include "DataFormatsCTP/Configuration.h"
#include "DataFormatsCTP/RunManager.h"
#include <Framework/InputRecord.h>
#include <Framework/InputRecordWalker.h>
#include "Framework/TimingInfo.h"
#include "DetectorsBase/GRPGeomHelper.h"

namespace o2::quality_control_modules::ctp
{

CTPRawDataReaderTask::~CTPRawDataReaderTask()
{
  //delete mHistoInputs; <= not needed with smart pointers
  delete mHistoClasses;
  delete mHistoMTVXBC;
  delete mHistoInputRatios;
  delete mHistoClassRatios;
}

void CTPRawDataReaderTask::initialize(o2::framework::InitContext& /*ctx*/)
{
  ILOG(Debug, Devel) << "initialize CTPRawDataReaderTask" << ENDM; // QcInfoLogger is used. FairMQ logs will go to there as well.
  int ninps = o2::ctp::CTP_NINPUTS + 1;
  int nclasses = o2::ctp::CTP_NCLASSES + 1;
  int norbits = o2::constants::lhc::LHCMaxBunches;
  mHistoInputs = std::make_unique<TH1FRatio>("inputs", "Inputs distribution;;rate (kHz)", ninps, 0, ninps, true);
  mHistoInputs->getNum()->SetCanExtend(TH1::kAllAxes);
  mHistoClasses = new TH1F("classes", "Classes distribution", nclasses, 0, nclasses);
  mHistoMTVXBC = new TH1F("bcMTVX", "BC position of MTVX", norbits, 0, norbits);
  mHistoInputRatios = new TH1F("inputRatio", "Input Ratio distribution", ninps, 0, ninps);
  mHistoInputRatios->SetCanExtend(TH1::kAllAxes);
  mHistoClassRatios = new TH1F("classRatio", "Class Ratio distribution", nclasses, 0, nclasses);
  getObjectsManager()->startPublishing(mHistoInputs.get());
  getObjectsManager()->startPublishing(mHistoClasses);
  getObjectsManager()->startPublishing(mHistoClassRatios);
  getObjectsManager()->startPublishing(mHistoInputRatios);
  getObjectsManager()->startPublishing(mHistoMTVXBC);

  mDecoder.setDoLumi(1);
  mDecoder.setDoDigits(1);
}

void CTPRawDataReaderTask::startOfActivity(const Activity& activity)
{
  ILOG(Debug, Devel) << "startOfActivity " << activity.mId << ENDM;
  mHistoInputs->Reset();
  mHistoClasses->Reset();
  mHistoClassRatios->Reset();
  mHistoInputRatios->Reset();
  mHistoMTVXBC->Reset();
}

void CTPRawDataReaderTask::startOfCycle()
{
  ILOG(Debug, Devel) << "startOfCycle" << ENDM;
}

void CTPRawDataReaderTask::monitorData(o2::framework::ProcessingContext& ctx)
{
  static constexpr double sOrbitLengthInMS = o2::constants::lhc::LHCOrbitMUS / 1000;

  // LOG(info) << "============  Starting monitoring ================== ";
  //   get the input
  std::vector<o2::framework::InputSpec> filter;
  std::vector<o2::ctp::LumiInfo> lumiPointsHBF1;
  std::vector<o2::ctp::CTPDigit> outputDigits;

  auto nOrbitsPerTF = o2::base::GRPGeomHelper::instance().getNHBFPerTF();

  o2::framework::InputRecord& inputs = ctx.inputs();
  mDecoder.decodeRaw(inputs, filter, outputDigits, lumiPointsHBF1);

  std::string nameInput = "MTVX";
  auto indexTvx = o2::ctp::CTPInputsConfiguration::getInputIndexFromName(nameInput);
  mNTF++;
  for (auto const digit : outputDigits) {
    uint16_t bcid = digit.intRecord.bc;
    if (digit.CTPInputMask.count()) {
      for (int i = 0; i < o2::ctp::CTP_NINPUTS; i++) {
        if (digit.CTPInputMask[i]) {
          // store counts in numerator
          mHistoInputs->getNum()->Fill(ctpinputs[i], 1);
          mHistoInputRatios->Fill(ctpinputs[i], 1);
          if (i == indexTvx - 1)
            mHistoMTVXBC->Fill(bcid);
        }
      }
    }
    if (digit.CTPClassMask.count()) {
      for (int i = 0; i < o2::ctp::CTP_NCLASSES; i++) {
        if (digit.CTPClassMask[i]) {
          mHistoClasses->Fill(i);
          mHistoClassRatios->Fill(i);
        }
      }
    }
  }
  mHistoInputs->getNum()->Fill(o2::ctp::CTP_NINPUTS);
  mHistoClasses->Fill(o2::ctp::CTP_NCLASSES);

  // store total duration (in milliseconds) in denominator
  mHistoInputs->getDen()->Fill((double)0, sOrbitLengthInMS * nOrbitsPerTF);

  // std::cout << " N TFs:" << mNTF << std::endl;
}

void CTPRawDataReaderTask::endOfCycle()
{
  ILOG(Debug, Devel) << "endOfCycle" << ENDM;

  // update the ratio of histograms
  mHistoInputs->update();
}

void CTPRawDataReaderTask::endOfActivity(const Activity& /*activity*/)
{
  ILOG(Debug, Devel) << "endOfActivity" << ENDM;
}

void CTPRawDataReaderTask::reset()
{
  // clean all the monitor objects here

  ILOG(Debug, Devel) << "Resetting the histograms" << ENDM;
  mHistoInputs->Reset();
  mHistoClasses->Reset();
  mHistoInputRatios->Reset();
  mHistoClassRatios->Reset();
  mHistoMTVXBC->Reset();
}

} // namespace o2::quality_control_modules::ctp
