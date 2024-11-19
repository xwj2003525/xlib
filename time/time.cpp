#include "time.h"
#include <assert.h>
#include <chrono>

static constexpr uint8_t days_in_month[13] = {0,  31, 28, 31, 30, 31, 30,
                                              31, 31, 30, 31, 30, 31};
static constexpr uint64_t MS_IN_A_SECOND = 1000ULL;
static constexpr uint64_t MS_IN_A_MINUTE = 60 * MS_IN_A_SECOND;
static constexpr uint64_t MS_IN_AN_HOUR = 60 * MS_IN_A_MINUTE;
static constexpr uint64_t MS_IN_A_DAY = 24 * MS_IN_AN_HOUR;
static constexpr uint64_t MS_IN_A_WEEK = 7 * MS_IN_A_DAY;

static inline constexpr bool is_leap_year(uint16_t year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static uint64_t sub(uint64_t a, uint64_t b) {
  if (a >= b)
    return a - b;
  return b - a;
}

static uint64_t now_milliseconds_since_epoch() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

static uint64_t DateTimeToMilliSeconds(int year, int month, int day, int hour,
                                       int minute, int second,
                                       int millisecond) {
  assert(x::time::isValidDateTime(year, month, day, hour, minute, second,
                                  millisecond));

  std::tm tm{};
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = second;

  std::time_t timeSinceEpoch = mktime(&tm);
  return timeSinceEpoch * MS_IN_A_SECOND + millisecond;
}

static std::tm MilliSecondstoTm(uint64_t milliSecondsSinceEpoch_) {
  std::time_t seconds = milliSecondsSinceEpoch_ / MS_IN_A_SECOND;
  std::tm tm;

#ifdef PLATFORM_WINDOWS
  localtime_s(&tm, &seconds); // Windows-safe version
#elif defined(PLATFORM_UNIX)
  localtime_r(&seconds, &tm); // POSIX-safe version
#endif

  return tm;
}

constexpr bool x::time::isValidDateTime(int year, int month, int day, int hour,
                                        int minute, int second,
                                        int millisecond) {
  if (!(1 <= month && month <= 12))
    return false;

  auto max_days = days_in_month[month];
  if (month == 2)
    max_days += is_leap_year(year);

  return (2000 <= year && year <= 2999) && (1 <= day && day <= max_days) &&
         (0 <= hour && hour <= 23) && (0 <= minute && minute <= 59) &&
         (0 <= second && second <= 59) &&
         (0 <= millisecond && millisecond <= 999);
}

x::time::Stamp::Stamp(uint64_t m) : milliseconds_since_epoch(m) {}

x::time::Stamp x::time::Stamp::Now() {
  return Stamp(now_milliseconds_since_epoch());
}

x::time::Stamp x::time::Stamp::When(int year, int month, int day, int hour,
                                    int minute, int second, int millisecond) {
  return Stamp(DateTimeToMilliSeconds(year, month, day, hour, minute, second,
                                      millisecond));
}

x::time::Stamp::Stamp(const Stamp &s)
    : milliseconds_since_epoch(s.milliseconds_since_epoch) {}

uint64_t x::time::Stamp::MilliSecondsSinceEpoch() const {
  return milliseconds_since_epoch;
}

bool x::time::Stamp::operator<(const Stamp &s) const {
  return milliseconds_since_epoch < s.milliseconds_since_epoch;
}

bool x::time::Stamp::isPast() const {
  return milliseconds_since_epoch < Now().milliseconds_since_epoch;
}

x::time::Gap x::time::Stamp::operator-(const Stamp &s) const {
  return Gap(sub(milliseconds_since_epoch, s.milliseconds_since_epoch));
}

x::time::Stamp x::time::Stamp::operator+(const Gap &g) const {
  return Stamp(milliseconds_since_epoch + g.MilliSeconds());
}

x::time::Stamp &x::time::Stamp::operator+=(const Gap &g) {
  milliseconds_since_epoch += g.MilliSeconds();
  return *this;
}

x::time::Stamp x::time::Stamp::operator-(const Gap &g) const {
  return Stamp(milliseconds_since_epoch - g.MilliSeconds());
}

x::time::Stamp &x::time::Stamp::operator-=(const Gap &g) {
  milliseconds_since_epoch -= g.MilliSeconds();
  return *this;
}

x::time::StampView x::time::Stamp::View() const {
  auto tm = MilliSecondstoTm(milliseconds_since_epoch);
  return {static_cast<uint16_t>(tm.tm_year + 1900),
          static_cast<uint8_t>(tm.tm_mon + 1),
          static_cast<uint8_t>(tm.tm_mday),
          static_cast<uint8_t>(tm.tm_hour),
          static_cast<uint8_t>(tm.tm_min),
          static_cast<uint8_t>(tm.tm_sec),
          static_cast<uint16_t>(milliseconds_since_epoch % MS_IN_A_SECOND)};
}

x::time::Gap::Gap(uint64_t m) : milliseconds(m) {}

x::time::Gap::Gap(const Gap &g) : Gap(g.milliseconds) {}

uint64_t x::time::Gap::MilliSeconds() const { return milliseconds; }

x::time::Gap x::time::Gap::MilliSeconds(uint64_t m) { return x::time::Gap(m); }

x::time::Gap x::time::Gap::Seconds(uint64_t s) {
  return x::time::Gap(s * MS_IN_A_SECOND);
}

x::time::Gap x::time::Gap::Minutes(uint64_t m) {
  return x::time::Gap(m * MS_IN_A_MINUTE);
}

x::time::Gap x::time::Gap::Hours(uint64_t h) {
  return x::time::Gap(h * MS_IN_AN_HOUR);
}

x::time::Gap x::time::Gap::Days(uint64_t d) {
  return x::time::Gap(d * MS_IN_A_DAY);
}

x::time::Gap x::time::Gap::Weeks(uint64_t w) {
  return x::time::Gap(w * MS_IN_A_WEEK);
}

x::time::Stamp x::time::Gap::operator+(const Stamp &s) const {
  return Stamp(milliseconds + s.MilliSecondsSinceEpoch());
}

x::time::Gap x::time::Gap::operator+(const Gap &g) const {
  return Gap(milliseconds + g.milliseconds);
}

x::time::Gap x::time::Gap::operator-(const Gap &g) const {
  return Gap(sub(milliseconds, g.milliseconds));
}

x::time::GapView x::time::Gap::View() const {
  uint64_t remainingTime = milliseconds;
  uint32_t days = remainingTime / MS_IN_A_DAY;
  remainingTime %= MS_IN_A_DAY;
  uint8_t hours = remainingTime / MS_IN_AN_HOUR;
  remainingTime %= MS_IN_AN_HOUR;
  uint8_t minutes = remainingTime / MS_IN_A_MINUTE;
  remainingTime %= MS_IN_A_MINUTE;
  uint8_t seconds = remainingTime / MS_IN_A_SECOND;
  uint16_t milliseconds_ = remainingTime % MS_IN_A_SECOND;

  return {days, hours, minutes, seconds, milliseconds_};
}

std::string x::time::StampView::toString() const {
  std::ostringstream oss;
  oss << std::setw(4) << std::setfill('0') << year << "-" << std::setw(2)
      << std::setfill('0') << static_cast<uint16_t>(month) << "-"
      << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(day) << " "
      << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(hour) << ":"
      << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(minute)
      << ":" << std::setw(2) << std::setfill('0')
      << static_cast<uint16_t>(second) << "." << std::setw(3)
      << std::setfill('0') << millisecond;
  return oss.str();
}

std::string x::time::GapView::toString() const {
  std::ostringstream oss;
  oss << static_cast<uint32_t>(day) << " " << std::setw(2) << std::setfill('0')
      << static_cast<uint16_t>(hour) << ":" << std::setw(2) << std::setfill('0')
      << static_cast<uint16_t>(minute) << ":" << std::setw(2)
      << std::setfill('0') << static_cast<uint16_t>(second) << "."
      << std::setw(3) << std::setfill('0') << millisecond;
  return oss.str();
}
