/*
 * (C) Crown copyright 2020, Met Office
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 */

#include "ufo/profile/ProfilePressure.h"

namespace ufo {

  static ProfileCheckMaker<ProfilePressure>
  makerProfilePressure_("Pressure");

  ProfilePressure::ProfilePressure
  (const ProfileConsistencyCheckParameters &options)
    : ProfileCheckBase(options)
  {}

  void ProfilePressure::runCheck(ProfileDataHandler &profileDataHandler)
  {
    oops::Log::debug() << " Pressure calculations" << std::endl;

    const int numProfileLevels = profileDataHandler.getNumProfileLevels();

    // Retrieve the observed geopotential height and associated metadata.
    std::vector <float> &zObs =
      profileDataHandler.get<float>(ufo::VariableNames::obs_geopotential_height);
    const std::vector <int> &ObsType =
      profileDataHandler.get<int>(ufo::VariableNames::ObsType);
    std::vector <int> &ReportFlags =
      profileDataHandler.get<int>(ufo::VariableNames::qcflags_observation_report);

    if (!oops::allVectorsSameNonZeroSize(zObs, ObsType, ReportFlags)) {
      oops::Log::warning() << "At least one vector is the wrong size. "
                           << "Profile pressure routine will not run." << std::endl;
      oops::Log::warning() << "Vector sizes: "
                           << oops::listOfVectorSizes(zObs, ObsType, ReportFlags)
                           << std::endl;
      return;
    }

    // Retrieve the model background fields.
    const std::vector <float> &orogGeoVaLs =
      profileDataHandler.getGeoVaLVector(ufo::VariableNames::geovals_orog);
    const std::vector <float> &pressureGeoVaLs =
      profileDataHandler.getGeoVaLVector(ufo::VariableNames::geovals_pressure);

    if (!oops::allVectorsSameNonZeroSize(orogGeoVaLs, pressureGeoVaLs)) {
      oops::Log::warning() << "At least one GeoVaLs vector is the wrong size. "
                           << "Profile pressure routine will not run." << std::endl;
      oops::Log::warning() << "Vector sizes: "
                           << oops::listOfVectorSizes(orogGeoVaLs, pressureGeoVaLs)
                           << std::endl;
      return;
    }

    // Retrive the vector of observed pressures.
    std::vector <float> &pressures =
      profileDataHandler.get<float>(ufo::VariableNames::obs_air_pressure);
    // If pressures have not been recorded, initialise the vector with missing values.
    if (pressures.empty())
      pressures.assign(numProfileLevels, missingValueFloat);

    // Determine whether the instrument that recorded this profile had a pressure sensor.
    const bool ObsHasNoPressureSensor =
      (ObsType[0] == MetOfficeObsIDs::AtmosphericProfile::PilotLand ||
       ObsType[0] == MetOfficeObsIDs::AtmosphericProfile::PilotShip ||
       ObsType[0] == MetOfficeObsIDs::AtmosphericProfile::PilotMobile ||
       ObsType[0] == MetOfficeObsIDs::AtmosphericProfile::WindProf ||
       ObsType[0] == MetOfficeObsIDs::AtmosphericProfile::Pilot);

    // Determine whether all levels have a valid pressure.
    const int NumValidPLevels = std::count_if(pressures.begin(), pressures.end(),
                                              [](float PLev){return PLev >= 0;});
    bool AllLevelsHaveValidP = NumValidPLevels == numProfileLevels;
    // Allow a small amount of flexibility for BUFR sondes.
    if (ObsType[0] == MetOfficeObsIDs::AtmosphericProfile::Sonde ||
        ObsType[0] == MetOfficeObsIDs::AtmosphericProfile::TSTSonde) {
      AllLevelsHaveValidP = NumValidPLevels >= std::max(numProfileLevels - 2,
                                                        static_cast<int>(round(numProfileLevels
                                                                               * 0.98)));
    }

    // Set a QC flag for wind profilers and for BUFR sondes for which not
    // all levels have a valid pressure.
    if (ObsType[0] == MetOfficeObsIDs::AtmosphericProfile::WindProf ||
        (!AllLevelsHaveValidP && (ObsType[0] == MetOfficeObsIDs::AtmosphericProfile::Sonde ||
                                  ObsType[0] == MetOfficeObsIDs::AtmosphericProfile::TSTSonde))) {
      for (int jlev = 0; jlev < numProfileLevels; ++jlev) {
        ReportFlags[jlev] |= MetOfficeQCFlags::WholeObReport::NoPressureSensor;
      }
    }

    // Determine whether any values of geopotential height are missing.
    const int NumValidZLevels = std::count_if(zObs.begin(), zObs.end(),
                                              [](float ZLev){return ZLev >= 0.0;});
    const bool AllLevelsHaveValidZ = NumValidZLevels == numProfileLevels;

    // Override AllLevelsHaveValidP if the observation has been flagged as having no pressure sensor
    // either in this routine or in an earlier one.
    // In this case the pressures will be (re)calculated from the geopotential heights.
    // This aims to improved the accuracy of the reported pressures if they are already present.
    if (AllLevelsHaveValidP &&
        ReportFlags[0] & MetOfficeQCFlags::WholeObReport::NoPressureSensor &&
        AllLevelsHaveValidZ) {
      AllLevelsHaveValidP = false;
    }

    // Perform this for instruments without a pressure sensor
    // that do not have a valid pressure on all levels.
    // In all other cases assume pressure is already fine.
    if (ObsHasNoPressureSensor &&
        !AllLevelsHaveValidP) {
      // Compute model heights on rho and theta levels.
      // todo(ctgh): This could be performed when retrieving the GeoVaLs.
      // zThetaGeoVaLs are not used further in this routine but do appear elsewhere in the code.
      std::vector <float> zRhoGeoVaLs;
      std::vector <float> zThetaGeoVaLs;
      ufo::CalculateModelHeight(options_.DHParameters.ModParameters,
                                orogGeoVaLs[0],
                                zRhoGeoVaLs,
                                zThetaGeoVaLs);
      // Compute observation pressures based on vertical interpolation from model heights.
      ufo::profileVerticalInterpolation(zRhoGeoVaLs,
                                        pressureGeoVaLs,
                                        zObs,
                                        pressures);
    }
  }
}  // namespace ufo
