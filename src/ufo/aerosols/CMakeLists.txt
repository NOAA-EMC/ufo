# (C) Copyright 2019 UCAR.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.


add_subdirectory( AOP )

if( ${geos-aero_FOUND} )
  add_subdirectory( MR )
endif( ${geos-aero_FOUND} )

PREPEND( _p_aodext_files  "aerosols/AOP"  ${aodext_src_files} )

if( ${geos-aero_FOUND} )
  PREPEND( _p_aodgeos_files   "aerosols/MR"   ${aodgeos_src_files} )
endif( ${geos-aero_FOUND} )

set ( aerosols_src_files
    ${_p_aodext_files}
    ${_p_aodgeos_files}
    PARENT_SCOPE
)

