#pragma once

#include <cstdint>
#include <string>

#if defined(_WIN32) || defined(_WIN64) // Windows-specific macros
#define PLATFORM_WINDOWS
#else // POSIX systems (Linux/macOS)
#define PLATFORM_UNIX
#endif

namespace x {
namespace time {

class StampView {
public:
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint16_t millisecond;
  std::string toString() const;
};

class GapView {
public:
  uint32_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint16_t millisecond;
  std::string toString() const;
};

class Gap;

class Stamp {
public:
  static Stamp InValid();
  static Stamp Now();
  static Stamp When(int year, int month = 1, int day = 1, int hour = 0,
                    int minute = 0, int second = 0, int millisecond = 0);

  Stamp(uint64_t = 0);
  Stamp(const Stamp &);
  uint64_t MilliSecondsSinceEpoch() const;
  StampView View() const;

  bool operator<(const Stamp &) const;
  bool isPast() const;
  bool isValid() const;

  Gap operator-(const Stamp &) const;
  Stamp operator+(const Gap &) const;
  Stamp &operator+=(const Gap &);
  Stamp operator-(const Gap &) const;
  Stamp &operator-=(const Gap &);

protected:
  uint64_t milliseconds_since_epoch;
};

class Gap {
public:
  static Gap InValid();
  static Gap Weeks(uint64_t);
  static Gap Days(uint64_t);
  static Gap Hours(uint64_t);
  static Gap Minutes(uint64_t);
  static Gap Seconds(uint64_t);
  static Gap MilliSeconds(uint64_t);

  Gap(uint64_t = 0);
  Gap(const Gap &);

  bool isValid() const;
  GapView View() const;
  uint64_t MilliSeconds() const;

  Stamp operator+(const Stamp &) const;
  Gap operator+(const Gap &) const;
  Gap operator-(const Gap &) const;

protected:
  uint64_t milliseconds;
};

constexpr bool isValidDateTime(int year, int month, int day, int hour,
                               int minute, int second, int millisecond);
}; // namespace time
}; // namespace x
