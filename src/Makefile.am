AM_CXXFLAGS = -Wall -g3

#
# lib
#
lib_LTLIBRARIES = libcockroach.la cockroach.la

# libcockroach.so
libcockroach_la_SOURCES = \
  utils.cc mapped_lib_manager.cc mapped_lib_info.cc shm_param_note.cc \
  probe.cc side_code_area_manager.cc disassembler.cc opecode.cc \
  opecode_relocator.cc rip_relative_relocator.cc \
  cockroach-time-measure.cc time_measure_probe.cc data_on_shm.cc
libcockroach_la_LDFLAGS = -lrt -dl -lpthread

# cockroach.so
cockroach_la_SOURCES = cockroach-instance.cc cockroach.cc
cockroach_la_LDFLAGS = \
  -lcockroach \
  -ldl \
  -avoid-version \
  -module -no-undefined \
  -export-dynamic

#
# bin
#
bin_PROGRAMS = \
  cockroach-loader cockroach-time-measure-tool cockroach-record-data-tool

# cockroach-loader
cockroach_loader_SOURCES = cockroach-loader.cc
cockroach_loader_LDFLAGS = -ldl -lcockroach

# cockroach-time-measure-tool
cockroach_time_measure_tool_SOURCES = cockroach-time-measure-tool.cc
cockroach_time_measure_tool_LDFLAGS = -lcockroach -lrt -ldl -pthread

# cockroach-record-data-tool
cockroach_record_data_tool_SOURCES = cockroach-record-data-tool.cc
cockroach_record_data_tool_LDFLAGS = -lcockroach -lrt -ldl
