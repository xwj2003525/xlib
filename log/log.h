#pragma once

#include <glog/logging.h>

inline void log_init(const char *progarm = "a.out") {
  google::InitGoogleLogging(progarm);
  FLAGS_log_dir = "./output";
  FLAGS_log_prefix=true;
  FLAGS_logtostderr = false;
  FLAGS_alsologtostderr=false;
}

inline void log_finish() { google::ShutdownGoogleLogging(); }
