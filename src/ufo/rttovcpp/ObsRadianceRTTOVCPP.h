/*
 * (C) Copyright 2017-2021 UCAR
 * 
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0. 
 */

#ifndef UFO_RTTOVCPP_OBSRADIANCERTTOVCPP_H_
#define UFO_RTTOVCPP_OBSRADIANCERTTOVCPP_H_

#include <ostream>
#include <string>
#include <vector>

#include "oops/base/Variables.h"
#include "oops/util/Logger.h"
#include "oops/util/ObjectCounter.h"
#include "ufo/ObsOperatorBase.h"

#include "rttov/wrapper/RttovSafe.h"

namespace eckit {
  class Configuration;
  class LocalConfiguration;
}

namespace ioda {
  class ObsSpace;
  class ObsVector;
}

namespace ufo {
  class GeoVaLs;
  class ObsDiagnostics;

// -----------------------------------------------------------------------------
/// RadianceRTTOVCPP observation operator class
class ObsRadianceRTTOVCPP : public ObsOperatorBase,
                   private util::ObjectCounter<ObsRadianceRTTOVCPP> {
 public:
  static const std::string classname() {return "ufo::ObsRadianceRTTOVCPP";}

  ObsRadianceRTTOVCPP(const ioda::ObsSpace &, const eckit::Configuration &);
  virtual ~ObsRadianceRTTOVCPP();

// Obs Operator
  void simulateObs(const GeoVaLs &, ioda::ObsVector &, ObsDiagnostics &) const override;

// Other: declare variable function with return type of oops:Variables
  const oops::Variables & requiredVars() const override {return varin_;}

 private:
  void print(std::ostream &) const override;
  const ioda::ObsSpace& odb_;
  oops::Variables varin_;
  std::string        CoefFileName;
  std::vector<int>   channels_;
  mutable std::size_t        nlevels;  // need this in order to allocate dx

// Declare a RttovSafe object for one single sensor
  mutable rttov::RttovSafe  aRttov_ = rttov::RttovSafe();
};

// -----------------------------------------------------------------------------

}  // namespace ufo
#endif  // UFO_RTTOVCPP_OBSRADIANCERTTOVCPP_H_
