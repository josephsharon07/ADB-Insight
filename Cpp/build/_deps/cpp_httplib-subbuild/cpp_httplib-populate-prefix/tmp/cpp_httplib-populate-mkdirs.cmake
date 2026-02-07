# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/sharon/Documents/Applications/ADB-Insight/Cpp/build/_deps/cpp_httplib-src"
  "/home/sharon/Documents/Applications/ADB-Insight/Cpp/build/_deps/cpp_httplib-build"
  "/home/sharon/Documents/Applications/ADB-Insight/Cpp/build/_deps/cpp_httplib-subbuild/cpp_httplib-populate-prefix"
  "/home/sharon/Documents/Applications/ADB-Insight/Cpp/build/_deps/cpp_httplib-subbuild/cpp_httplib-populate-prefix/tmp"
  "/home/sharon/Documents/Applications/ADB-Insight/Cpp/build/_deps/cpp_httplib-subbuild/cpp_httplib-populate-prefix/src/cpp_httplib-populate-stamp"
  "/home/sharon/Documents/Applications/ADB-Insight/Cpp/build/_deps/cpp_httplib-subbuild/cpp_httplib-populate-prefix/src"
  "/home/sharon/Documents/Applications/ADB-Insight/Cpp/build/_deps/cpp_httplib-subbuild/cpp_httplib-populate-prefix/src/cpp_httplib-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/sharon/Documents/Applications/ADB-Insight/Cpp/build/_deps/cpp_httplib-subbuild/cpp_httplib-populate-prefix/src/cpp_httplib-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/sharon/Documents/Applications/ADB-Insight/Cpp/build/_deps/cpp_httplib-subbuild/cpp_httplib-populate-prefix/src/cpp_httplib-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
