/*
 * (C) Copyright 2021 UK Met Office
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 */

#include "ufo/identity/ObsIdentityTLAD.h"

#include <ostream>
#include <vector>

#include "ioda/ObsSpace.h"
#include "ioda/ObsVector.h"

#include "oops/base/Variables.h"
#include "oops/util/Logger.h"
#include "oops/util/missingValues.h"

#include "ufo/GeoVaLs.h"
#include "ufo/utils/OperatorUtils.h"  // for getOperatorVariables

namespace ufo {

// -----------------------------------------------------------------------------
static LinearObsOperatorMaker<ObsIdentityTLAD> makerIdentityTL_("Identity");
// -----------------------------------------------------------------------------

ObsIdentityTLAD::ObsIdentityTLAD(const ioda::ObsSpace & odb,
                                 const eckit::Configuration & config)
  : LinearObsOperatorBase(odb)
{
  oops::Log::trace() << "ObsIdentityTLAD constructor starting" << std::endl;

  getOperatorVariables(config, odb.obsvariables(), operatorVars_, operatorVarIndices_);
  requiredVars_ += operatorVars_;

  oops::Log::trace() << "ObsIdentityTLAD constructor finished" << std::endl;
}

// -----------------------------------------------------------------------------

ObsIdentityTLAD::~ObsIdentityTLAD() {
  oops::Log::trace() << "ObsIdentityTLAD destructed" << std::endl;
}

// -----------------------------------------------------------------------------

void ObsIdentityTLAD::setTrajectory(const GeoVaLs & geovals, ObsDiagnostics &) {
  // The trajectory is not needed because the observation operator is linear.
  oops::Log::trace() << "ObsIdentityTLAD: trajectory set" << std::endl;
}

// -----------------------------------------------------------------------------

void ObsIdentityTLAD::simulateObsTL(const GeoVaLs & dx, ioda::ObsVector & dy) const {
  oops::Log::trace() << "ObsIdentityTLAD: TL observation operator starting" << std::endl;

  std::vector<double> vec(dy.nlocs());
  for (int jvar : operatorVarIndices_) {
    const std::string& varname = dy.varnames().variables()[jvar];
    // Fill dy with dx at the lowest level.
    dx.getAtLevel(vec, varname, 0);
    for (size_t jloc = 0; jloc < dy.nlocs(); ++jloc) {
      const size_t idx = jloc * dy.nvars() + jvar;
      dy[idx] = vec[jloc];
    }
  }

  oops::Log::trace() << "ObsIdentityTLAD: TL observation operator finished" << std::endl;
}

// -----------------------------------------------------------------------------

void ObsIdentityTLAD::simulateObsAD(GeoVaLs & dx, const ioda::ObsVector & dy) const {
  oops::Log::trace() << "ObsIdentityTLAD: adjoint observation operator starting" << std::endl;

  const double missing = util::missingValue(missing);

  std::vector<double> vec(dy.nlocs());
  for (int jvar : operatorVarIndices_) {
    const std::string& varname = dy.varnames().variables()[jvar];
    // Get current value of dx at the lowest level.
    dx.getAtLevel(vec, varname, 0);
    // Increment dx with non-missing values of dy.
    for (size_t jloc = 0; jloc < dy.nlocs(); ++jloc) {
      const size_t idx = jloc * dy.nvars() + jvar;
      if (dy[idx] != missing)
        vec[jloc] += dy[idx];
    }
    // Store new value of dx.
    dx.putAtLevel(vec, varname, 0);
  }

  oops::Log::trace() << "ObsIdentityTLAD: adjoint observation operator finished" << std::endl;
}

// -----------------------------------------------------------------------------

void ObsIdentityTLAD::print(std::ostream & os) const {
  os << "ObsIdentityTLAD operator" << std::endl;
}

// -----------------------------------------------------------------------------

}  // namespace ufo
