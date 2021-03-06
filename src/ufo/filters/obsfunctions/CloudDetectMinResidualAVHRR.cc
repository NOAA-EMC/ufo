/*
 * (C) Copyright 2019 UCAR
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 */

#include "ufo/filters/obsfunctions/CloudDetectMinResidualAVHRR.h"

#include <cmath>

#include <algorithm>
#include <cfloat>
#include <iomanip>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "ioda/ObsDataVector.h"
#include "oops/util/IntSetParser.h"
#include "oops/util/missingValues.h"
#include "ufo/filters/Variable.h"
#include "ufo/utils/Constants.h"

namespace ufo {

static ObsFunctionMaker<CloudDetectMinResidualAVHRR>
       makerCloudDetectMinResidualAVHRR_("CloudDetectMinResidualAVHRR");

// -----------------------------------------------------------------------------

CloudDetectMinResidualAVHRR::CloudDetectMinResidualAVHRR(const eckit::LocalConfiguration & conf)
  : invars_() {
  // Check options
  options_.deserialize(conf);

  // Get channels from options
  std::set<int> channelset = oops::parseIntSet(options_.channelList);
  std::copy(channelset.begin(), channelset.end(), std::back_inserter(channels_));
  ASSERT(channels_.size() > 0);

  // Get test groups from options
  const std::string &flaggrp = options_.testQCflag.value();
  const std::string &errgrp = options_.testObserr.value();
  const std::string &biasgrp = options_.testBias.value();
  const std::string &hofxgrp = options_.testHofX.value();

  // Include required variables from ObsDiag
  invars_ += Variable("brightness_temperature_jacobian_surface_temperature@ObsDiag", channels_);
  invars_ += Variable("brightness_temperature_jacobian_air_temperature@ObsDiag", channels_);
  invars_ += Variable("transmittances_of_atmosphere_layer@ObsDiag", channels_);
  invars_ += Variable("pressure_level_at_peak_of_weightingfunction@ObsDiag", channels_);

  // Include list of required data from ObsSpace
  invars_ += Variable("brightness_temperature@"+flaggrp, channels_);
  invars_ += Variable("brightness_temperature@"+errgrp, channels_);
  invars_ += Variable("brightness_temperature@"+biasgrp, channels_);
  invars_ += Variable("brightness_temperature@"+hofxgrp, channels_);
  invars_ += Variable("brightness_temperature@ObsValue", channels_);
  invars_ += Variable("brightness_temperature@ObsError", channels_);

  // Include list of required data from GeoVaLs
  invars_ += Variable("water_area_fraction@GeoVaLs");
  invars_ += Variable("land_area_fraction@GeoVaLs");
  invars_ += Variable("ice_area_fraction@GeoVaLs");
  invars_ += Variable("surface_snow_area_fraction@GeoVaLs");
  invars_ += Variable("average_surface_temperature_within_field_of_view@GeoVaLs");
  invars_ += Variable("air_pressure@GeoVaLs");
  invars_ += Variable("air_temperature@GeoVaLs");
  invars_ += Variable("tropopause_pressure@GeoVaLs");
}

// -----------------------------------------------------------------------------

CloudDetectMinResidualAVHRR::~CloudDetectMinResidualAVHRR() {}

// -----------------------------------------------------------------------------

void CloudDetectMinResidualAVHRR::compute(const ObsFilterData & in,
                                  ioda::ObsDataVector<float> & out) const {
  // Get channel use flags from options
  std::vector<int> use_flag = options_.useflagChannel.value();
  std::vector<int> use_flag_clddet = options_.useflagCloudDetect.value();

  // Get tuning parameters for surface sensitivity over sea/land/oce/snow/mixed from options
  std::vector<float> dtempf_in = options_.obserrScaleFactorTsfc.value();

  // Get dimensions
  size_t nlocs = in.nlocs();
  size_t nchans = channels_.size();
  size_t nlevs = in.nlevs(Variable("air_pressure@GeoVaLs"));

  // Get test groups from options
  const std::string &flaggrp = options_.testQCflag.value();
  const std::string &errgrp = options_.testObserr.value();
  const std::string &biasgrp = options_.testBias.value();
  const std::string &hofxgrp = options_.testHofX.value();

  // Get variables from ObsDiag
  // Load surface temperature jacobian
  std::vector<std::vector<float>> dbtdts(nchans, std::vector<float>(nlocs));
  for (size_t ichan = 0; ichan < nchans; ++ichan) {
    in.get(Variable("brightness_temperature_jacobian_surface_temperature@ObsDiag",
                     channels_)[ichan], dbtdts[ichan]);
  }

  // Get temperature jacobian
  std::vector<std::vector<std::vector<float>>>
       dbtdt(nchans, std::vector<std::vector<float>>(nlevs, std::vector<float>(nlocs)));
  for (size_t ichan = 0; ichan < nchans; ++ichan) {
    for (size_t ilev = 0; ilev < nlevs; ++ilev) {
      const int level = nlevs - ilev - 1;
      in.get(Variable("brightness_temperature_jacobian_air_temperature@ObsDiag",
                       channels_)[ichan], level, dbtdt[ichan][ilev]);
    }
  }

  // Get layer-to-space transmittance
  std::vector<std::vector<std::vector<float>>>
       tao(nchans, std::vector<std::vector<float>>(nlevs, std::vector<float>(nlocs)));
  for (size_t ichan = 0; ichan < nchans; ++ichan) {
    for (size_t ilev = 0; ilev < nlevs; ++ilev) {
      const int level = nlevs - ilev - 1;
      in.get(Variable("transmittances_of_atmosphere_layer@ObsDiag",
             channels_)[ichan], level,  tao[ichan][ilev]);
    }
  }

  // Get pressure level at the peak of the weighting function
  std::vector<float> values(nlocs, 0.0);
  std::vector<std::vector<float>> wfunc_pmaxlev(nchans, std::vector<float>(nlocs));
  for (size_t ichan = 0; ichan < nchans; ++ichan) {
    in.get(Variable("pressure_level_at_peak_of_weightingfunction@ObsDiag",
                     channels_)[ichan], values);
    for (size_t iloc = 0; iloc < nlocs; ++iloc) {
      wfunc_pmaxlev[ichan][iloc] = nlevs - values[iloc] + 1;
    }
  }

  // Get variables from ObsSpace
  // Get effective observation error and convert it to inverse of the error variance
  const float missing = util::missingValue(missing);
  std::vector<int> qcflag(nlocs, 0);
  std::vector<std::vector<float>> varinv_use(nchans, std::vector<float>(nlocs, 0.0));
  for (size_t ichan = 0; ichan < nchans; ++ichan) {
    in.get(Variable("brightness_temperature@"+errgrp, channels_)[ichan], values);
    in.get(Variable("brightness_temperature@"+flaggrp, channels_)[ichan], qcflag);
    for (size_t iloc = 0; iloc < nlocs; ++iloc) {
      if (flaggrp == "PreQC") values[iloc] == missing ? qcflag[iloc] = 100 : qcflag[iloc] = 0;
      (qcflag[iloc] == 0) ? (values[iloc] = 1.0 / pow(values[iloc], 2)) : (values[iloc] = 0.0);
      if (use_flag_clddet[ichan] > 0) varinv_use[ichan][iloc] = values[iloc];
    }
  }

  // Get bias corrected innovation (tbobs - hofx - bias)
  std::vector<std::vector<float>> innovation(nchans, std::vector<float>(nlocs));
  for (size_t ichan = 0; ichan < nchans; ++ichan) {
    in.get(Variable("brightness_temperature@ObsValue", channels_)[ichan], innovation[ichan]);
    in.get(Variable("brightness_temperature@"+hofxgrp, channels_)[ichan], values);
    for (size_t iloc = 0; iloc < nlocs; ++iloc) {
      innovation[ichan][iloc] = innovation[ichan][iloc] - values[iloc];
    }
    in.get(Variable("brightness_temperature@"+biasgrp, channels_)[ichan], values);
    for (size_t iloc = 0; iloc < nlocs; ++iloc) {
      innovation[ichan][iloc] = innovation[ichan][iloc] - values[iloc];
    }
  }

  // Get original observation error (uninflated) from ObsSpace
  std::vector<std::vector<float>> obserr(nchans, std::vector<float>(nlocs));
  for (size_t ichan = 0; ichan < nchans; ++ichan) {
    in.get(Variable("brightness_temperature@ObsError", channels_)[ichan], obserr[ichan]);
  }

  // Get tropopause pressure [Pa]
  std::vector<float> tropprs(nlocs);
  in.get(Variable("tropopause_pressure@GeoVaLs"), tropprs);

  // Get average surface temperature within FOV
  std::vector<float> tsavg(nlocs);
  in.get(Variable("average_surface_temperature_within_field_of_view@GeoVaLs"), tsavg);

  // Get area fraction of each surface type
  std::vector<float> water_frac(nlocs);
  std::vector<float> land_frac(nlocs);
  std::vector<float> ice_frac(nlocs);
  std::vector<float> snow_frac(nlocs);
  in.get(Variable("water_area_fraction@GeoVaLs"), water_frac);
  in.get(Variable("land_area_fraction@GeoVaLs"), land_frac);
  in.get(Variable("ice_area_fraction@GeoVaLs"), ice_frac);
  in.get(Variable("surface_snow_area_fraction@GeoVaLs"), snow_frac);

  // Determine dominant surface type in each FOV
  std::vector<bool> land(nlocs, false);
  std::vector<bool> sea(nlocs, false);
  std::vector<bool> ice(nlocs, false);
  std::vector<bool> snow(nlocs, false);
  std::vector<bool> mixed(nlocs, false);
  for (size_t iloc = 0; iloc < nlocs; ++iloc) {
    sea[iloc] = water_frac[iloc] >= 0.99;
    land[iloc] = land_frac[iloc] >= 0.99;
    ice[iloc] = ice_frac[iloc] >= 0.99;
    snow[iloc] = snow_frac[iloc] >= 0.99;
    mixed[iloc] = (!sea[iloc] && !land[iloc] && !ice[iloc] && !snow[iloc]);
  }

  // Setup weight given to each surface type
  std::vector<float> dtempf(nlocs);
  for (size_t iloc = 0; iloc < nlocs; ++iloc) {
    if (sea[iloc]) {
      dtempf[iloc] = dtempf_in[0];
    } else if (land[iloc]) {
      dtempf[iloc] = dtempf_in[1];
    } else if (ice[iloc]) {
      dtempf[iloc] = dtempf_in[2];
    } else if (snow[iloc]) {
      dtempf[iloc] = dtempf_in[3];
    } else {
      dtempf[iloc] = dtempf_in[4];
    }
  }

  // Get air pressure [Pa]
  std::vector<std::vector<float>> prsl(nlevs, std::vector<float>(nlocs));
  for (size_t ilev = 0; ilev < nlevs; ++ilev) {
    const size_t level = nlevs - ilev - 1;
    in.get(Variable("air_pressure@GeoVaLs"), level, prsl[ilev]);
  }

  // Get air temperature
  std::vector<std::vector<float>> tair(nlevs, std::vector<float>(nlocs));
  for (size_t ilev = 0; ilev < nlevs; ++ilev) {
    const size_t level = nlevs - ilev - 1;
    in.get(Variable("air_temperature@GeoVaLs"), level, tair[ilev]);
  }

  // Minimum Residual Method (MRM) for Cloud Detection:
  // Determine model level index of the cloud top (lcloud)
  // Find pressure of the cloud top (cldprs)
  // Estimate cloud fraction (cldfrac)
  // output: out = 0 clear channel
  //         out = 1 cloudy channel
  //         out = 2 clear channel but too sensitive to surface condition

  // Loop through locations
  const float btmax = 1000.f, btmin = 0.f;
  for (size_t iloc=0; iloc < nlocs; ++iloc) {
    float sum3 = 0.0;
    float tmp = 0.0;
    float cloudp = 0.0;
    std::vector<std::vector<float>> dbt(nchans, std::vector<float>(nlevs));
    for (size_t ichan=0; ichan < nchans; ++ichan) {
      if (varinv_use[ichan][iloc] > 0) {
        sum3 = sum3 + innovation[ichan][iloc] * innovation[ichan][iloc] * varinv_use[ichan][iloc];
      }
    }
    sum3 = 0.75 * sum3;

    // Set initial cloud condition
    int lcloud = 0;
    float cldfrac = 0.0;
    float cldprs = prsl[0][iloc] * 0.01;     // convert from [Pa] to [hPa]

    // Loop through vertical layer from surface to model top
    for (size_t k = 0 ; k < nlevs ; ++k) {
      for (size_t ichan = 0; ichan < nchans; ++ichan) dbt[ichan][k] = 0.0;
      // Perform cloud detection within troposphere
      if (prsl[k][iloc] * 0.01 > tropprs[iloc] * 0.01) {
        float sum = 0.0, sum2 = 0.0;
        for (size_t ichan = 0; ichan < nchans; ++ichan) {
          if (varinv_use[ichan][iloc] > 0.0) {
            dbt[ichan][k] = (tair[k][iloc] - tsavg[iloc]) * dbtdts[ichan][iloc];
            for (size_t kk = 0; kk < k; ++kk) {
              dbt[ichan][k] = dbt[ichan][k] + (tair[k][iloc] - tair[kk][iloc]) *
                              dbtdt[ichan][kk][iloc];
            }
            sum = sum + innovation[ichan][iloc] * dbt[ichan][k] * varinv_use[ichan][iloc];
            sum2 = sum2 +  dbt[ichan][k] * dbt[ichan][k] * varinv_use[ichan][iloc];
          }
        }
        if (fabs(sum2) < FLT_MIN) sum2 = copysign(1.0e-12, sum2);
        cloudp = std::min(std::max((sum/sum2), 0.f), 1.f);
        sum = 0.0;
        for (size_t ichan = 0; ichan < nchans; ++ichan) {
          if (varinv_use[ichan][iloc] > 0.0) {
          tmp = innovation[ichan][iloc] - cloudp * dbt[ichan][k];
          sum = sum + tmp * tmp * varinv_use[ichan][iloc];
          }
        }
        if (sum < sum3) {
          sum3 = sum;
          lcloud = k + 1;   // array index + 1 -> model coordinate index
          cldfrac = cloudp;
          cldprs = prsl[k][iloc] * 0.01;
        }
      }
    // end of vertical loop
    }
    // Cloud check
    for (size_t ichan = 0; ichan < nchans; ++ichan) {
      size_t ilev;
      out[ichan][iloc] = 0;
      for (ilev = 0; ilev < lcloud; ++ilev) {
        if (fabs(cldfrac * dbt[ichan][ilev]) > obserr[ichan][iloc]) {
          out[ichan][iloc]= 1;
          varinv_use[ichan][iloc]= 0.0;
          break;
        }
      }
    }
    // If no clouds is detected, do sensivity to surface temperature check
    // Initialize at each location
    float sumx = 0.0, sumx2 = 0.0;
    float dts = 0.0;
    float delta = 0.0;
    const float dts_threshold = 3.0;
    for (size_t ichan=0; ichan < nchans; ++ichan) {
      sumx = sumx + innovation[ichan][iloc] * dbtdts[ichan][iloc] * varinv_use[ichan][iloc];
      sumx2 = sumx2 + dbtdts[ichan][iloc] * dbtdts[ichan][iloc] * varinv_use[ichan][iloc];
    }
    if (fabs(sumx2) < FLT_MIN) sumx2 = copysign(1.0e-12, sumx2);
    dts = std::fabs(sumx / sumx2);
    float dts_save = dts;
    if (std::abs(dts) > 1.0) {
      if (sea[iloc] == false) {
        dts = std::min(dtempf[iloc], dts);
      } else {
        dts = std::min(dts_threshold, dts);
      }
      for (size_t ichan=0; ichan < nchans; ++ichan) {
        delta = obserr[ichan][iloc];
        if (std::abs(dts * dbtdts[ichan][iloc]) > delta) out[ichan][iloc] = 2;
      }
    }
  // end of location loop
  }
}

// -----------------------------------------------------------------------------

const ufo::Variables & CloudDetectMinResidualAVHRR::requiredVariables() const {
  return invars_;
}

// -----------------------------------------------------------------------------

}  // namespace ufo
