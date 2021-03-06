/*
 * (C) Crown copyright 2020, Met Office
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 */

#ifndef UFO_VARIABLETRANSFORMS_CAL_WIND_H_
#define UFO_VARIABLETRANSFORMS_CAL_WIND_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "oops/util/ObjectCounter.h"
#include "ufo/variabletransforms/TransformBase.h"

namespace ufo {

/*!
* \brief Wind Speed And Direction filter
*
* \details  Performs a variable conversion from the wind components, eastward_wind and
*  northward_wind, to wind_speed and wind_from_direction. The newly calculated variables
*  are included in the same obs space. The filter does not have any configuration options.
///
*
* See VariableTransformsParameters for filter setup.
*/
class Cal_WindSpeedAndDirection : public TransformBase {
 public:
  Cal_WindSpeedAndDirection(const VariableTransformsParameters &options,
                            ioda::ObsSpace &os,
                            const std::shared_ptr<ioda::ObsDataVector<int>> &flags,
                            const std::vector<bool> &apply);
  // Run variable conversion
  void runTransform() override;
};

/*!
* \brief Retrieve wind components.
*
* \details Performs a variable conversion from wind_speed and wind_from_direction to
*  the wind components, eastward_wind and northward_wind. The newly calculated variables
*  are included in the same obs space.
*
* See VariableTransformsParameters for filter setup.
*/
class Cal_WindComponents : public TransformBase {
 public:
  Cal_WindComponents(const VariableTransformsParameters &options,
                     ioda::ObsSpace &os,
                     const std::shared_ptr<ioda::ObsDataVector<int>> &flags,
                     const std::vector<bool> &apply);
  // Run check
  void runTransform() override;
};
}  // namespace ufo

#endif  // UFO_VARIABLETRANSFORMS_CAL_WIND_H_
