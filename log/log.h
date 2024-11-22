#pragma once

#include <glog/logging.h>

inline void log_init(const char *progarm = "a.out") {
  google::InitGoogleLogging(progarm);
  FLAGS_log_dir = "./output";
  FLAGS_log_prefix = true;
  FLAGS_colorlogtostderr = true; // 设置记录到标准输出的颜色消息（如果终端支持）
  FLAGS_log_prefix = true; // 设置日志前缀是否应该添加到每行输出
  google::LogToStderr();
}

inline void log_finish() { google::ShutdownGoogleLogging(); }
