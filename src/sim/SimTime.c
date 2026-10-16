#include "SimTime.h"
#include <time.h>
#include <math.h>

// --- Simulation clock -------------------------------------------------------
// All motion is a pure function of simDays, the number of calendar days since
// the J2000 epoch (2000-01-01 12:00 TT). On startup simDays is set to the
// current UTC date/time, so the planets begin at their real heliocentric mean
// longitudes. Each frame, if not paused, simDays advances by
// simDaysPerSec * deltaTime and the app recomputes every angle absolutely
// (no per-frame accumulation), so changing speed never drifts or teleports a
// body. simDaysPerSec is chosen from the three on-screen presets.
static double simDays = 0.0;
static double simDaysPerSec = 1.0;
static int simSpeedIndex = 0;
static int stopRotation = 0;

#define SIM_SPEED_PRESET_COUNT 3
static const double simSpeedPresetDaysPerSec[SIM_SPEED_PRESET_COUNT] = {
	1.0,        // 1 day per second of real time (default)
	7.0,        // 1 week per second
	365.25      // 1 year per second
};
static const char *simSpeedLabels[SIM_SPEED_PRESET_COUNT] = {
	"1 DAY/SEC", "1 WK/SEC", "1 YR/SEC"
};

// Calendar (struct tm, UTC) -> Julian Date (days; 00:00 UT is a .5).
static double tmToJD(const struct tm *t)
{
	int y = t->tm_year + 1900;
	int m = t->tm_mon + 1;
	if (m <= 2)
	{
		y--;
		m += 12;
	}
	int A = y / 100;
	int B = 2 - A + A / 4;
	long JDN = (long)(365.25 * (y + 4716)) + (long)(30.6001 * (m + 1))
		+ t->tm_mday + B - 1524;
	return (double)JDN - 0.5
		+ t->tm_hour / 24.0 + t->tm_min / 1440.0 + t->tm_sec / 86400.0;
}

void SimTime_JulianDateToCalendar(double JD, int *yy, int *mo, int *dd,
                                  int *hh, int *mi, int *ss)
{
	// Julian Date -> broken-down UTC date/time (Fliegel-Van Flandern).
	double Z = floor(JD + 0.5);
	double F = JD + 0.5 - Z;                 // fraction of the day (0 at 00:00 UT)
	long alpha = (long)((Z - 1867216.25) / 36524.25);
	long A = (long)Z + 1 + alpha - alpha / 4;
	long B = A + 1524;
	long C = (long)((B - 122.1) / 365.25);
	long D = (long)(365.25 * C);
	long E = (long)((B - D) / 30.6001);
	long day = B - D - (long)(30.6001 * E);  // 1-based day of month
	long month = (E < 14) ? (E - 1) : (E - 13);
	long year = (month > 2) ? (C - 4716) : (C - 4715);
	*yy = (int)year;
	*mo = (int)month;
	*dd = (int)day;
	int total = (int)(F * 86400.0);
	*hh = total / 3600;
	total %= 3600;
	*mi = total / 60;
	*ss = total % 60;
}

// Set simDays to the current UTC date/time (J2000-relative) so every body
// starts at its real heliocentric position for "now".
void SimTime_Init(void)
{
	time_t now = time(NULL);
	const struct tm *utc = gmtime(&now);
	if (utc)
		simDays = tmToJD(utc) - 2451545.0;
}

void SimTime_Advance(double dDays) { simDays += dDays; }

void SimTime_SetPreset(int index)
{
	if (index < 0 || index >= SIM_SPEED_PRESET_COUNT)
		return;
	simSpeedIndex = index;
	simDaysPerSec = simSpeedPresetDaysPerSec[index];
}

void SimTime_TogglePause(void) { stopRotation = stopRotation ? 0 : 1; }
double SimTime_Days(void) { return simDays; }
double SimTime_DaysPerSec(void) { return simDaysPerSec; }
int SimTime_PresetIndex(void) { return simSpeedIndex; }
int SimTime_PresetCount(void) { return SIM_SPEED_PRESET_COUNT; }
const char *SimTime_PresetLabel(int index) { return simSpeedLabels[index]; }
int SimTime_Paused(void) { return stopRotation; }
