/*
 * (C) Crown copyright 2020, Met Office
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 */

#include <algorithm>

#include "ufo/profile/EntireSampleDataHandler.h"
#include "ufo/profile/VariableNames.h"

namespace ufo {
  EntireSampleDataHandler::EntireSampleDataHandler(ioda::ObsSpace &obsdb,
                                                   const DataHandlerParameters &options)
    : obsdb_(obsdb),
      options_(options)
  {}

  void EntireSampleDataHandler::writeQuantitiesToObsdb()
  {
    // Write out all variables in particular groups.
    for (const auto& it_data : entireSampleData_) {
      std::string fullname = it_data.first;
      std::string varname;
      std::string groupname;
      ufo::splitVarGroup(fullname, varname, groupname);

      if (groupname == "QCFlags") {
        putDataVector(fullname, get<int>(fullname));
      } else if (groupname == "Corrections" ||
                 groupname == "DerivedValue") {
        // todo(ctgh): Add ModelLevelsDerivedValue, ModelRhoLevelsDerivedValue,
        // ModelLevelsFlags and ModelRhoLevelsFlags to this list when
        // it is possible to save variables with different nlocs.
        putDataVector(fullname, get<float>(fullname));
      }
    }

    // Write out the NumAnyErrors counter, which is used in multiple QC checks.
    const std::vector <int> &NumAnyErrors =
      get<int>(ufo::VariableNames::counter_NumAnyErrors);
    putDataVector(ufo::VariableNames::counter_NumAnyErrors, NumAnyErrors);
  }

  int EntireSampleDataHandler::defaultValue(const std::vector <int> &vec,
                                            const std::string &groupname)
  {
    if (groupname == "Counters")
      return 0;
    else
      return missingValueInt;
  }

  float EntireSampleDataHandler::defaultValue(const std::vector <float> &vec,
                                              const std::string &groupname)
  {
    if (groupname == "Corrections")
      return 0.0f;
    else
      return missingValueFloat;
  }

  std::string EntireSampleDataHandler::defaultValue(const std::vector <std::string> &vec,
                                                    const std::string &groupname)
  {
    return missingValueString;
  }
}  // namespace ufo
