#pragma once
#include <stdint.h>
#include "CalendarData.h"

// Gregorian month layout + official, offline HKO lunar/solar-term lookup.
// Sunday=0. No epoch / localtime assumptions in this pure date module.
namespace calendar_logic {
constexpr bool leapYear(int y) { return y%4==0 && (y%100!=0 || y%400==0); }
constexpr int monthDays(int y,int m) { return m<1||m>12?0:m==2?(leapYear(y)?29:28):(m==4||m==6||m==9||m==11?30:31); }
constexpr bool validDate(int y,int m,int d) { return y>=1&&y<=9999&&m>=1&&m<=12&&d>=1&&d<=monthDays(y,m); }
constexpr int beforeMonth(int y,int m) { return m<=1?0:monthDays(y,m-1)+beforeMonth(y,m-1); }
constexpr int monthOffset(int m) { return m==1?0:m==2?3:m==3?2:m==4?5:m==5?0:m==6?3:m==7?5:m==8?1:m==9?4:m==10?6:m==11?2:4; }
constexpr int weekdayRaw(int y,int m,int d) { return (y+y/4-y/100+y/400+monthOffset(m)+d)%7; }
constexpr int weekday(int y,int m,int d) { return validDate(y,m,d)?weekdayRaw(y-(m<3),m,d):-1; }
constexpr int rows(int y,int m) { return validDate(y,m,1)?(weekday(y,m,1)+monthDays(y,m)+6)/7:0; }
constexpr int beforeYear(int y) { return y<=calendar_data::kFirstYear?0:365+leapYear(y-1)+beforeYear(y-1); }
constexpr bool supported(int y,int m,int d) { return y>=calendar_data::kFirstYear&&y<=calendar_data::kLastYear&&validDate(y,m,d); }
constexpr uint16_t packed(int y,int m,int d) { return supported(y,m,d)?calendar_data::kDays[beforeYear(y)+beforeMonth(y,m)+d-1]:0; }
constexpr int lunarDay(int y,int m,int d) { return packed(y,m,d)&31; }
constexpr int lunarMonth(int y,int m,int d) { return (packed(y,m,d)>>5)&15; }
constexpr bool lunarLeap(int y,int m,int d) { return (packed(y,m,d)&512)!=0; }
constexpr int termIndex(int y,int m,int d) { return packed(y,m,d)>>10; }
static constexpr const char* kTerms[]={"","小寒","大寒","立春","雨水","惊蛰","春分","清明","谷雨","立夏","小满","芒种","夏至","小暑","大暑","立秋","处暑","白露","秋分","寒露","霜降","立冬","小雪","大雪","冬至"};
static constexpr const char* kLunarMonths[]={"","正月","二月","三月","四月","五月","六月","七月","八月","九月","十月","十一月","十二月"};
static constexpr const char* kLunarDays[]={"","初一","初二","初三","初四","初五","初六","初七","初八","初九","初十","十一","十二","十三","十四","十五","十六","十七","十八","十九","二十","廿一","廿二","廿三","廿四","廿五","廿六","廿七","廿八","廿九","三十"};
constexpr const char* term(int y,int m,int d) { return kTerms[termIndex(y,m,d)]; }
constexpr const char* fixedFestival(int m,int d) { return m==1&&d==1?"元旦":m==5&&d==1?"劳动节":m==10&&d==1?"国庆节":m==9&&d==10?"教师节":""; }
constexpr const char* lunarFestivalRaw(int m,int d) { return m==1&&d==1?"春节":m==1&&d==15?"元宵节":m==5&&d==5?"端午节":m==7&&d==7?"七夕":m==8&&d==15?"中秋节":m==9&&d==9?"重阳节":""; }
constexpr const char* lunarFestival(int y,int m,int d) { return lunarLeap(y,m,d)?"":lunarFestivalRaw(lunarMonth(y,m,d),lunarDay(y,m,d)); }
constexpr const char* weekdayFestival(int y,int m,int d) { return m==5&&d>=8&&d<=14&&weekday(y,m,d)==0?"母亲节":m==6&&d>=15&&d<=21&&weekday(y,m,d)==0?"父亲节":""; }
constexpr const char* festival(int y,int m,int d) {
  return !validDate(y,m,d)?"":*fixedFestival(m,d)?fixedFestival(m,d):*lunarFestival(y,m,d)?lunarFestival(y,m,d):
    *weekdayFestival(y,m,d)?weekdayFestival(y,m,d):termIndex(y,m,d)==7?"清明节":"";
}
// 2026 official State Council holiday notice. Future-year arrangements are never inferred.
struct Holiday { int month,first,last; const char* name; };
static constexpr Holiday kHolidays[]={
  {1,1,3,"元旦"},{2,15,23,"春节"},{4,4,6,"清明"},{5,1,5,"劳动节"},
  {6,19,21,"端午"},{9,25,27,"中秋"},{10,1,7,"国庆"}
};
static constexpr const char* kHolidayLabels[]={"元旦假期","春节假期","清明假期","劳动假期","端午假期","中秋假期","国庆假期"};
constexpr bool holidayYearKnown(int y) { return y==2026; }
constexpr int holidayIndex(int y,int m,int d) {
  return !holidayYearKnown(y)||!validDate(y,m,d)?-1:m==1&&d<=3?0:m==2&&d>=15&&d<=23?1:m==4&&d>=4&&d<=6?2:
    m==5&&d<=5?3:m==6&&d>=19&&d<=21?4:m==9&&d>=25&&d<=27?5:m==10&&d<=7?6:-1;
}
constexpr bool workday(int y,int m,int d) {
  return y==2026&&((m==1&&d==4)||(m==2&&(d==14||d==28))||(m==5&&d==9)||(m==9&&d==20)||(m==10&&d==10));
}
// R=official holiday, W=make-up workday, empty=ordinary or unknown (not a workday claim).
constexpr char badge(int y,int m,int d) { return holidayIndex(y,m,d)>=0?'R':workday(y,m,d)?'W':0; }
constexpr const char* label(int y,int m,int d) {
  return *festival(y,m,d)?festival(y,m,d):*term(y,m,d)?term(y,m,d):workday(y,m,d)?"调休上班":holidayIndex(y,m,d)>=0?kHolidayLabels[holidayIndex(y,m,d)]:"";
}
constexpr bool same(const char* a,const char* b) { return *a==*b&&(*a=='\0'||same(a+1,b+1)); }
static_assert(rows(2026,8)==6 && rows(2026,9)==5 && rows(2026,2)==4,"4/5/6-row months");
static_assert(monthDays(2028,2)==29 && monthDays(2100,2)==28 && weekday(2026,9,25)==5,"Gregorian boundaries");
static_assert(lunarMonth(2026,9,25)==8 && lunarDay(2026,9,25)==15,"Mid-autumn lunar mapping");
static_assert(same(festival(2026,2,17),"春节") && same(festival(2026,3,3),"元宵节"),"Lunar festivals");
static_assert(same(festival(2026,6,19),"端午节") && same(festival(2026,8,19),"七夕") && same(festival(2026,10,18),"重阳节"),"Traditional festivals");
static_assert(same(festival(2026,5,10),"母亲节") && same(festival(2026,6,21),"父亲节"),"Variable weekday festivals");
static_assert(termIndex(2026,6,21)==12 && same(label(2026,6,21),"父亲节"),"Festival wins cell, term still available");
static_assert(termIndex(2026,9,7)==17 && termIndex(2026,9,23)==18,"Solar term dates");
static_assert(badge(2026,9,20)=='W' && badge(2026,9,25)=='R' && badge(2026,9,27)=='R' && !badge(2026,9,19),"Official rest/work labels");
static_assert(!badge(2027,9,25) && !supported(2031,1,1) && !packed(2026,2,30),"No invented unsupported data");
} // namespace calendar_logic
