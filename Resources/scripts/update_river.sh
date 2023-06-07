#!/bin/bash
set -ex

PACKAGE_NAME="river-cpp";
PACKAGE_VERSION="1.0.7";
ARCHS=("osx-arm64" "osx-64" "win-64" "linux-64");
ARCHS_OUTPUTS=("OSX/arm64" "OSX/x64" "Windows/x64" "Linux/x64");

i=0;

rm -rf tmp_river
mkdir -p tmp_river

rm -rf Generic
mkdir -p Generic

for arch in ${ARCHS[@]}; do
  output_dir_relative=${ARCHS_OUTPUTS[$i]};
  echo "Processing architecture ${arch}, output directory ${output_dir_relative}."

  tmp_arch_dir="tmp_river/${arch}";
  mkdir -p ${tmp_arch_dir};

  # Creates a conda environment locally, which implicitly downloads the libraries/
  # includes needed
  CONDA_SUBDIR=${arch} conda create -p ${tmp_arch_dir} -c conda-forge -y "${PACKAGE_NAME}==${PACKAGE_VERSION}"

  mkdir -p ${output_dir_relative}
  rm -rf ${output_dir_relative}/*

  if [[ "$arch" == "win"* ]]; then
    tmp_install_dir=${tmp_arch_dir}/Library
  else
    tmp_install_dir=${tmp_arch_dir}
  fi

  if [ $i -eq 0 ]; then
    # Includes are the same regardless of arch, so just do it once:
    mkdir -p Generic/include
    mv -v ${tmp_install_dir}/include/{hiredis,river} Generic/include/
  fi

  mkdir -p ${output_dir_relative}/lib
  if [[ "$arch" == "win"* ]]; then
    tmp_install_dir_prefixes=("${tmp_install_dir}/bin" "${tmp_install_dir}/lib")
  else
    tmp_install_dir_prefixes=("${tmp_install_dir}/lib")
  fi
  for tmp_install_dir_prefix in ${tmp_install_dir_prefixes[@]}; do
    mv -v ${tmp_install_dir_prefix}/{lib,}{glog,gflags,hiredis,river}.{*so*,*dylib*,dll,lib} ${output_dir_relative}/lib 2>/dev/null || true
  done

  i=$((i+1))
done

rm -rf tmp_river
