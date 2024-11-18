#pragma once

#include <cstdint>
#include <string>

#if defined(_WIN32) || defined(_WIN64)  // Windows-specific macros
#define PLATFORM_WINDOWS
#else  // POSIX systems (Linux/macOS)
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
            std::string toString() const;

            const uint16_t week;
            const uint8_t day;
            const uint8_t hour;
            const uint8_t minute;
            const uint8_t second;
            const uint16_t millisecond;

        };

        class Gap;

        class Stamp {
        public:
            static const uint64_t MIN_MILLISECONDS_SINCE_EPOCH;
            static const uint64_t MAX_MILLISECONDS_SINCE_EPOCH;
            static const uint16_t MAX_YEAR;
            static const uint16_t MIN_YEAR;
            
            Stamp(const StampView&);
            Stamp(const Stamp&);
            static Stamp Now();
            static Stamp DateTime(int year, int month, int day, int hour, int minute, int second, int millisecond);

            Gap operator-(const Stamp&) const;
            Stamp operator+(const Gap&) const;
            Stamp& operator+=(const Gap&);
            Stamp operator-(const Gap&) const;
            Stamp& operator-=(const Gap&);

            StampView View()const;
        protected:
            friend class Gap;
            Stamp() = delete;
            Stamp(uint64_t);

            uint64_t milliseconds_since_epoch;
        };

        class Gap {
        public:
            static const uint64_t MAX_MILLISECONDS;
            static const uint16_t MAX_WEEK;

            Gap(const Gap&);
            static Gap Weeks(uint64_t);
            static Gap Days(uint64_t);
            static Gap Hours(uint64_t);
            static Gap Minutes(uint64_t);
            static Gap Seconds(uint64_t);
            static Gap MilliSeconds(uint64_t);

            Stamp operator+(const Stamp&)const;
            Gap operator+(const Gap&) const;
            Gap& operator+=(const Gap&);

            GapView View()const;
        protected:
            friend class Stamp;
            Gap() = delete;
            Gap(uint64_t);

            uint64_t milliseconds;
        };

        bool isValidDateTime(int year, int month, int day, int hour, int minute, int second, int millisecond);

    };


};