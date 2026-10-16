#ifndef SIMTIME_H
#define SIMTIME_H

// Simulation clock: the day count since the J2000 epoch (2000-01-01 12:00 TT),
// the active speed preset (sim-days per second of real time), and the pause
// flag. Every body's position is a pure function of the day count, so there is
// no per-frame accumulation. See the .c for the date math.
void SimTime_Init(void);                 // set to the current UTC date/time
void SimTime_Advance(double dDays);      // day count += dDays
void SimTime_SetPreset(int index);       // select one of the speed presets
void SimTime_TogglePause(void);
double SimTime_Days(void);
double SimTime_DaysPerSec(void);
int SimTime_PresetIndex(void);
int SimTime_PresetCount(void);
const char *SimTime_PresetLabel(int index);
int SimTime_Paused(void);
// Broken-down UTC date/time from an absolute Julian Date (Fliegel-Van Flandern).
void SimTime_JulianDateToCalendar(double JD, int *yy, int *mo, int *dd,
                                  int *hh, int *mi, int *ss);

#endif
