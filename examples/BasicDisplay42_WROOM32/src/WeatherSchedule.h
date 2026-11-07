#pragma once
#include <stdint.h>
namespace weather_schedule {
constexpr int64_t kValidClock=1704067200LL;
constexpr uint32_t kRetryMs=10U*60U*1000U;
constexpr int64_t latestSlot(int64_t now){return now/3600*3600;}
constexpr int64_t nextSlot(int64_t now){return latestSlot(now)+3600;}
constexpr bool due(int64_t now,int64_t fetchedAt){return now>=kValidClock&&(fetchedAt<kValidClock||fetchedAt<latestSlot(now)||fetchedAt>now+300);}
static_assert(due(1793466000LL,1793465999LL),"new hour due");
static_assert(!due(1793466001LL,1793466000LL),"catchup once");
static_assert(!due(0,0),"wait for clock");
}
