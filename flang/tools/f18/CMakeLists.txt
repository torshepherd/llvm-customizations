set(LLVM_LINK_COMPONENTS
  FrontendOpenACC
  FrontendOpenMP
  Support
  )

# Define the list of Fortran module files that need to be compiled
# to produce an object file for inclusion into the FortranRuntime
# library.
set(MODULES_WITH_IMPLEMENTATION
  "iso_fortran_env_impl"
)

# Define the list of Fortran module files for which it is
# sufficient to generate the module file via -fsyntax-only.
set(MODULES_WITHOUT_IMPLEMENTATION
  "__fortran_builtins"
  "__fortran_ieee_exceptions"
  "__fortran_type_info"
  "__ppc_types"
  "__ppc_intrinsics"
  "mma"
  "__cuda_builtins"
  "__cuda_device"
  "cudadevice"
  "ieee_arithmetic"
  "ieee_exceptions"
  "ieee_features"
  "iso_c_binding"
  "iso_fortran_env"
)

set(MODULES ${MODULES_WITH_IMPLEMENTATION} ${MODULES_WITHOUT_IMPLEMENTATION})

# Check if 128-bit float computations can be done via long double.
check_cxx_source_compiles(
  "#include <cfloat>
   #if LDBL_MANT_DIG != 113
   #error LDBL_MANT_DIG != 113
   #endif
   int main() { return 0; }
  "
  HAVE_LDBL_MANT_DIG_113)

# Figure out whether we can support REAL(KIND=16)
if (FLANG_RUNTIME_F128_MATH_LIB)
  set(FLANG_SUPPORT_R16 "1")
elseif (HAVE_LDBL_MANT_DIG_113)
  set(FLANG_SUPPORT_R16 "1")
else()
  set(FLANG_SUPPORT_R16 "0")
endif()

# Init variable to hold extra object files coming from the Fortran modules;
# these module files will be contributed from the CMakeLists in flang/tools/f18.
set(module_objects "")

# Create module files directly from the top-level module source directory.
# If CMAKE_CROSSCOMPILING, then the newly built flang executable was
# cross compiled, and thus can't be executed on the build system and thus
# can't be used for generating module files.
if (NOT CMAKE_CROSSCOMPILING)
  foreach(filename ${MODULES})
    set(depends "")
    set(opts "")
    if(${filename} STREQUAL "__fortran_builtins" OR
       ${filename} STREQUAL "__ppc_types")
    elseif(${filename} STREQUAL "__ppc_intrinsics" OR
           ${filename} STREQUAL "mma")
      set(depends ${FLANG_INTRINSIC_MODULES_DIR}/__ppc_types.mod)
    elseif(${filename} STREQUAL "__cuda_device")
      set(opts -fc1 -xcuda)
      set(depends ${FLANG_INTRINSIC_MODULES_DIR}/__cuda_builtins.mod)
    elseif(${filename} STREQUAL "cudadevice")
      set(opts -fc1 -xcuda)
      set(depends ${FLANG_INTRINSIC_MODULES_DIR}/__cuda_device.mod)
    else()
      set(depends ${FLANG_INTRINSIC_MODULES_DIR}/__fortran_builtins.mod)
      if(${filename} STREQUAL "iso_fortran_env")
        set(depends ${depends} ${FLANG_INTRINSIC_MODULES_DIR}/iso_fortran_env_impl.mod)
      endif()
      if(${filename} STREQUAL "ieee_arithmetic" OR
         ${filename} STREQUAL "ieee_exceptions")
        set(depends ${depends} ${FLANG_INTRINSIC_MODULES_DIR}/__fortran_ieee_exceptions.mod)
      endif()
    endif()
    if(NOT ${filename} STREQUAL "__fortran_type_info" AND NOT ${filename} STREQUAL "__fortran_builtins")
      set(depends ${depends} ${FLANG_INTRINSIC_MODULES_DIR}/__fortran_type_info.mod)
    endif()

    # The module contains PPC vector types that needs the PPC target.
    if(${filename} STREQUAL "__ppc_intrinsics" OR
       ${filename} STREQUAL "mma")
      if (PowerPC IN_LIST LLVM_TARGETS_TO_BUILD)
        set(opts "--target=ppc64le")
      else()
        # Do not compile PPC module if the target is not available.
        continue()
      endif()
    endif()

    set(decls "")
    if (FLANG_SUPPORT_R16)
      set(decls "-DFLANG_SUPPORT_R16")
    endif()

    # Some modules have an implementation part that needs to be added to the
    # FortranRuntime library.
    set(compile_with "-fsyntax-only")
    set(object_output "")
    set(include_in_link FALSE)
    if(${filename} IN_LIST MODULES_WITH_IMPLEMENTATION AND FLANG_INCLUDE_RUNTIME)
      set(object_output "${CMAKE_CURRENT_BINARY_DIR}/${filename}${CMAKE_CXX_OUTPUT_EXTENSION}")
      set(compile_with -c -o ${object_output})
      set(include_in_link TRUE)
    endif()

    set(base ${FLANG_INTRINSIC_MODULES_DIR}/${filename})
    # TODO: We may need to flag this with conditional, in case Flang is built w/o OpenMP support
    add_custom_command(OUTPUT ${base}.mod ${object_output}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${FLANG_INTRINSIC_MODULES_DIR}
      COMMAND flang ${opts} ${decls} -cpp ${compile_with} -module-dir ${FLANG_INTRINSIC_MODULES_DIR}
        ${FLANG_SOURCE_DIR}/module/${filename}.f90
      DEPENDS flang ${FLANG_SOURCE_DIR}/module/${filename}.f90 ${FLANG_SOURCE_DIR}/module/__fortran_builtins.f90 ${depends}
    )
    list(APPEND MODULE_FILES ${base}.mod)
    install(FILES ${base}.mod DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/flang")

    # If a module has been compiled into an object file, add the file to
    # the link line for the FortranRuntime library.
    if(include_in_link)
      list(APPEND module_objects ${object_output})
    endif()
  endforeach()

  # Set a CACHE variable that is visible to the CMakeLists.txt in runtime/, so that
  # the compiled Fortran modules can be added to the link line of the FortranRuntime
  # library.
  set(FORTRAN_MODULE_OBJECTS ${module_objects} CACHE INTERNAL "" FORCE)

  # Special case for omp_lib.mod, because its source comes from openmp/runtime/src/include.
  # It also produces two module files: omp_lib.mod and omp_lib_kinds.mod.  Compile these
  # files only if OpenMP support has been configured.
  if (LLVM_TOOL_OPENMP_BUILD)
    message(STATUS "OpenMP runtime support enabled via LLVM_ENABLED_PROJECTS, building omp_lib.mod")
    set(base ${FLANG_INTRINSIC_MODULES_DIR}/omp_lib)
    add_custom_command(OUTPUT ${base}.mod ${base}_kinds.mod
      COMMAND ${CMAKE_COMMAND} -E make_directory ${FLANG_INTRINSIC_MODULES_DIR}
      COMMAND flang -cpp -fsyntax-only ${opts} -module-dir ${FLANG_INTRINSIC_MODULES_DIR}
        ${CMAKE_BINARY_DIR}/projects/openmp/runtime/src/omp_lib.F90
      DEPENDS flang ${FLANG_INTRINSIC_MODULES_DIR}/iso_c_binding.mod ${CMAKE_BINARY_DIR}/projects/openmp/runtime/src/omp_lib.F90 ${depends}
    )
    add_custom_command(OUTPUT ${base}.f18.mod
      DEPENDS ${base}.mod
      COMMAND ${CMAKE_COMMAND} -E copy ${base}.mod ${base}.f18.mod)
      add_custom_command(OUTPUT ${base}_kinds.f18.mod
        DEPENDS ${base}.mod
        COMMAND ${CMAKE_COMMAND} -E copy ${base}_kinds.mod ${base}_kinds.f18.mod)
    list(APPEND MODULE_FILES ${base}.mod ${base}.f18.mod ${base}_kinds.mod ${base}_kinds.f18.mod)
    install(FILES ${base}.mod ${base}.f18.mod ${base}_kinds.mod ${base}_kinds.f18.mod DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/flang")
  else()
    message(STATUS "Not building omp_lib.mod, no OpenMP runtime in LLVM_ENABLED_PROJECTS")
  endif()
endif()

add_custom_target(module_files ALL DEPENDS ${MODULE_FILES})
set_target_properties(module_files PROPERTIES FOLDER "Flang/Resources")

# TODO Move this to a more suitable location
# Copy the generated omp_lib.h header file, if OpenMP support has been configured.
if (LLVM_TOOL_OPENMP_BUILD)
  message(STATUS "OpenMP runtime support enabled via LLVM_ENABLED_PROJECTS, building omp_lib.h")
  file(COPY ${CMAKE_BINARY_DIR}/projects/openmp/runtime/src/omp_lib.h DESTINATION "${CMAKE_BINARY_DIR}/include/flang/OpenMP/" FILE_PERMISSIONS OWNER_READ OWNER_WRITE)
  install(FILES ${CMAKE_BINARY_DIR}/include/flang/OpenMP/omp_lib.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/flang/OpenMP")
else()
  message(STATUS "Not copying omp_lib.h, no OpenMP runtime in LLVM_ENABLED_PROJECTS")
endif()
