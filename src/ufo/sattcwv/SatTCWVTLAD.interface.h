/*
 * (C) Copyright 2021 Met Office
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 */

#ifndef UFO_SATTCWV_SATTCWVTLAD_INTERFACE_H_
#define UFO_SATTCWV_SATTCWVTLAD_INTERFACE_H_

#include "ioda/ObsSpace.h"
#include "ufo/Fortran.h"

namespace ufo {

/// Interface to Fortran UFO routines
/*!
 * The core of the UFO is coded in Fortran.
 * Here we define the interfaces to the Fortran code.
 */

extern "C" {
// -----------------------------------------------------------------------------
// Precipitable water tl/ad observation operators
// -----------------------------------------------------------------------------
  void ufo_sattcwv_tlad_setup_f90(F90hop &, const eckit::Configuration * const *);
  void ufo_sattcwv_tlad_delete_f90(F90hop &);
  void ufo_sattcwv_tlad_settraj_f90(const F90hop &, const F90goms &,
                                             const ioda::ObsSpace &);
  void ufo_sattcwv_simobs_tl_f90(
          const F90hop &, const F90goms &, const ioda::ObsSpace &, const int &, double &);
  void ufo_sattcwv_simobs_ad_f90(
          const F90hop &, const F90goms &, const ioda::ObsSpace &, const int &, const double &);
// -----------------------------------------------------------------------------

}  // extern C

}  // namespace ufo
#endif  // UFO_SATTCWV_SATTCWVTLAD_INTERFACE_H_
