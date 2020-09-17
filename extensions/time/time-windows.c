#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN  // trim down the Win32 headers
#include <windows.h>
#undef IS_ERROR

#include <process.h>
#include <assert.h>

#include "sys-core.h"


//
//  Get_Current_Datetime_Value: C
//
// Get the current system date/time in UTC plus zone offset (mins).
//
REBVAL *Get_Current_Datetime_Value(void)
{
    SYSTEMTIME stime;
    TIME_ZONE_INFORMATION tzone;

    GetSystemTime(&stime);

    if (TIME_ZONE_ID_DAYLIGHT == GetTimeZoneInformation(&tzone))
        tzone.Bias += tzone.DaylightBias;

    return rebValue("ensure date! (make-date-ymdsnz",
        rebI(stime.wYear),  // year
        rebI(stime.wMonth),  // month
        rebI(stime.wDay),  // day
        rebI(
            stime.wHour * 3600 + stime.wMinute * 60 + stime.wSecond
        ),  // "secs"
        rebI(1000000 * stime.wMilliseconds), // nano
        rebI(-tzone.Bias),  // zone
    ")", rebEND);
}

