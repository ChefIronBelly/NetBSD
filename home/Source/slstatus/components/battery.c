/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <string.h>

#include "../slstatus.h"
#include "../util.h"

#if defined(__linux__)
/*
 * https://www.kernel.org/doc/html/latest/power/power_supply_class.html
 */
	#include <limits.h>
	#include <stdint.h>
	#include <unistd.h>

	#define POWER_SUPPLY_CAPACITY "/sys/class/power_supply/%s/capacity"
	#define POWER_SUPPLY_STATUS   "/sys/class/power_supply/%s/status"
	#define POWER_SUPPLY_CHARGE   "/sys/class/power_supply/%s/charge_now"
	#define POWER_SUPPLY_ENERGY   "/sys/class/power_supply/%s/energy_now"
	#define POWER_SUPPLY_CURRENT  "/sys/class/power_supply/%s/current_now"
	#define POWER_SUPPLY_POWER    "/sys/class/power_supply/%s/power_now"

	static const char *
	pick(const char *bat, const char *f1, const char *f2, char *path,
	     size_t length)
	{
		if (esnprintf(path, length, f1, bat) > 0 &&
		    access(path, R_OK) == 0)
			return f1;

		if (esnprintf(path, length, f2, bat) > 0 &&
		    access(path, R_OK) == 0)
			return f2;

		return NULL;
	}

	const char *
	battery_perc(const char *bat)
	{
		int cap_perc;
		char path[PATH_MAX];

		if (esnprintf(path, sizeof(path), POWER_SUPPLY_CAPACITY, bat) < 0)
			return NULL;
		if (pscanf(path, "%d", &cap_perc) != 1)
			return NULL;

		return bprintf("%d", cap_perc);
	}

	const char *
	battery_state(const char *bat)
	{
		static struct {
			char *state;
			char *symbol;
		} map[] = {
			{ "Charging",    "+" },
			{ "Discharging", "-" },
			{ "Full",        "o" },
			{ "Not charging", "o" },
		};
		size_t i;
		char path[PATH_MAX], state[12];

		if (esnprintf(path, sizeof(path), POWER_SUPPLY_STATUS, bat) < 0)
			return NULL;
		if (pscanf(path, "%12[a-zA-Z ]", state) != 1)
			return NULL;

		for (i = 0; i < LEN(map); i++)
			if (!strcmp(map[i].state, state))
				break;

		return (i == LEN(map)) ? "?" : map[i].symbol;
	}

	const char *
	battery_remaining(const char *bat)
	{
		uintmax_t charge_now, current_now, m, h;
		double timeleft;
		char path[PATH_MAX], state[12];

		if (esnprintf(path, sizeof(path), POWER_SUPPLY_STATUS, bat) < 0)
			return NULL;
		if (pscanf(path, "%12[a-zA-Z ]", state) != 1)
			return NULL;

		if (!pick(bat, POWER_SUPPLY_CHARGE, POWER_SUPPLY_ENERGY, path,
		          sizeof(path)) ||
		    pscanf(path, "%ju", &charge_now) < 0)
			return NULL;

		if (!strcmp(state, "Discharging")) {
			if (!pick(bat, POWER_SUPPLY_CURRENT, POWER_SUPPLY_POWER, path,
			          sizeof(path)) ||
			    pscanf(path, "%ju", &current_now) < 0)
				return NULL;

			if (current_now == 0)
				return NULL;

			timeleft = (double)charge_now / (double)current_now;
			h = timeleft;
			m = (timeleft - (double)h) * 60;

			return bprintf("%juh %jum", h, m);
		}

		return "";
	}
#elif defined(__OpenBSD__)
	#include <fcntl.h>
	#include <machine/apmvar.h>
	#include <sys/ioctl.h>
	#include <unistd.h>

	static int
	load_apm_power_info(struct apm_power_info *apm_info)
	{
		int fd;

		fd = open("/dev/apm", O_RDONLY);
		if (fd < 0) {
			warn("open '/dev/apm':");
			return 0;
		}

		memset(apm_info, 0, sizeof(struct apm_power_info));
		if (ioctl(fd, APM_IOC_GETPOWER, apm_info) < 0) {
			warn("ioctl 'APM_IOC_GETPOWER':");
			close(fd);
			return 0;
		}
		return close(fd), 1;
	}

	const char *
	battery_perc(const char *unused)
	{
		struct apm_power_info apm_info;

		if (load_apm_power_info(&apm_info))
			return bprintf("%d", apm_info.battery_life);

		return NULL;
	}

	const char *
	battery_state(const char *unused)
	{
		struct {
			unsigned int state;
			char *symbol;
		} map[] = {
			{ APM_AC_ON,      "+" },
			{ APM_AC_OFF,     "-" },
		};
		struct apm_power_info apm_info;
		size_t i;

		if (load_apm_power_info(&apm_info)) {
			for (i = 0; i < LEN(map); i++)
				if (map[i].state == apm_info.ac_state)
					break;

			return (i == LEN(map)) ? "?" : map[i].symbol;
		}

		return NULL;
	}

	const char *
	battery_remaining(const char *unused)
	{
		struct apm_power_info apm_info;
		unsigned int h, m;

		if (load_apm_power_info(&apm_info)) {
			if (apm_info.ac_state != APM_AC_ON) {
				h = apm_info.minutes_left / 60;
				m = apm_info.minutes_left % 60;
				return bprintf("%uh %02um", h, m);
			} else {
				return "";
			}
		}

		return NULL;
	}
#elif defined(__FreeBSD__)
	#include <sys/sysctl.h>

	#define BATTERY_LIFE  "hw.acpi.battery.life"
	#define BATTERY_STATE "hw.acpi.battery.state"
	#define BATTERY_TIME  "hw.acpi.battery.time"

	const char *
	battery_perc(const char *unused)
	{
		int cap_perc;
		size_t len;

		len = sizeof(cap_perc);
		if (sysctlbyname(BATTERY_LIFE, &cap_perc, &len, NULL, 0) < 0 || !len)
			return NULL;

		return bprintf("%d", cap_perc);
	}

	const char *
	battery_state(const char *unused)
	{
		int state;
		size_t len;

		len = sizeof(state);
		if (sysctlbyname(BATTERY_STATE, &state, &len, NULL, 0) < 0 || !len)
			return NULL;

		switch (state) {
		case 0: /* FALLTHROUGH */
		case 2:
			return "+";
		case 1:
			return "-";
		default:
			return "?";
		}
	}

	const char *
	battery_remaining(const char *unused)
	{
		int rem;
		size_t len;

		len = sizeof(rem);
		if (sysctlbyname(BATTERY_TIME, &rem, &len, NULL, 0) < 0 || !len
		    || rem < 0)
			return NULL;

		return bprintf("%uh %02um", rem / 60, rem % 60);
	}
#elif defined(__NetBSD__)
#define _NO_APM 1
#define _PATH_APM_NORMAL       "/dev/apm"
#include <sys/types.h>
#include <sys/time.h>

#define ENVSYSUNITNAMES
#include <sys/param.h>
#include <sys/envsys.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#ifndef _NO_APM
#include <machine/apmvar.h>
#else
#define APM_AC_OFF	0x00
#define APM_AC_ON	0x01
#endif



/*
 * pre:  fd contains a valid file descriptor of an envsys(4) supporting device
 *       && ns is the number of sensors
 *       && etds and ebis are arrays of sufficient size
 * post: returns 0 and etds and ebis arrays are filled with sensor info
 *       or returns -1 on failure
 */
static int
fillsensors(int fd, envsys_tre_data_t *etds, envsys_basic_info_t *ebis,
    size_t ns)
{
	int i;

	for (i = 0; i < ns; ++i) {
		ebis[i].sensor = i;
		if (ioctl(fd, ENVSYS_GTREINFO, &ebis[i]) == -1) {
			warn("Can't get sensor info for sensor %d", i);
			return 0;
		}

		etds[i].sensor = i;
		if (ioctl(fd, ENVSYS_GTREDATA, &etds[i]) == -1) {
			warn("Can't get sensor data for sensor %d", i);
			return 0;
		}
	}
	return 1;
}

/*
 * pre:  fd contains a valid file descriptor of an envsys(4) supporting device
 * post: returns the number of valid sensors provided by the device
 *       or -1 on error
 */
static size_t
numsensors(int fd)
{
	int count = 0, valid = 1;
	envsys_tre_data_t etd;
	etd.sensor = 0;

	while (valid) {
		if (ioctl(fd, ENVSYS_GTREDATA, &etd) == -1)
			err(1, "Can't get sensor data");

		valid = etd.validflags & ENVSYS_FVALID;
		if (valid)
			++count;

		++etd.sensor;
	}

	return count;
}

static envsys_tre_data_t *etds;
static envsys_basic_info_t *ebis;
static int *cetds;

#if defined(_PATH_SYSMON) && __NetBSD_Version__ >= 106110000
#define HAVE_NETBSD_ACPI
#endif

int first = 1;
	const char *
	battery_perc(const char *unused)
	{
    int fd, r, p;
#ifndef _NO_APM
		struct apm_power_info apm_info;
#endif

    int acpi;
    size_t ns;
    size_t cc;
    char *apmdev;
    int i;

    acpi = 0;
    apmdev = _PATH_APM_NORMAL;
    if ((fd = open(apmdev, O_RDONLY)) == -1) {
#ifdef HAVE_NETBSD_ACPI
      apmdev = _PATH_SYSMON;
      fd = open(apmdev, O_RDONLY);
      acpi = 1;
#endif
    }
    if (fd < 0) {
      fprintf(stderr, "slstatus: cannot open %s device\n", apmdev);
      exit(1);
    }

       if (acpi) {
#ifdef HAVE_NETBSD_ACPI
		if ((ns = numsensors(fd)) == 0) {
		       fprintf(stderr, "slstatus: no sensors found\n");
                exit(1);
    }
		if (first) {
			cetds = (int *)malloc(ns * sizeof(int));
			etds = (envsys_tre_data_t *)malloc(ns * sizeof(envsys_tre_data_t));
			ebis = (envsys_basic_info_t *)malloc(ns * sizeof(envsys_basic_info_t));

			if ((cetds == NULL) || (etds == NULL) || (ebis == NULL)) {
				err(1, "Out of memory");
			}
		}

		fillsensors(fd, etds, ebis, ns);

#endif

#ifndef _NO_APM
       } else {

	       memset(&info, 0, sizeof(info));
        if (ioctl(fd, APM_IOC_GETPOWER, &info) != 0) {
                fprintf(stderr, "slstatus: ioctl APM_IOC_GETPOWER failed\n");
                exit(1);
        }
#endif
       }

       close(fd);

       if (acpi) {
#ifdef HAVE_NETBSD_ACPI
		int32_t rtot = 0, maxtot = 0;
		int have_pct = 0;
		p = APM_AC_ON;
		for (i = 0 ; i < ns ; i++) {
			if ((etds[i].validflags & ENVSYS_FCURVALID) == 0)
				continue;
			cc = strlen(ebis[i].desc);
			if (strncmp(ebis[i].desc, "acpibat", 7) == 0 &&
			    (strcmp(&ebis[i].desc[cc - 7], " charge") == 0 ||
			     strcmp(&ebis[i].desc[cc - 7], " energy") == 0)) {
				rtot += etds[i].cur.data_s;
				maxtot += etds[i].max.data_s;
			}
			/*
			 * XXX: We should use acpiacad driver and look for
			 * " connected", but that's broken on some machines
			 * and we want this to work everywhere.  With this
			 * we will occasionally catch a machine conditioning
			 * a battery while connected, while other machines take
			 * 10-15 seconds to switch from "charging" to
			 * "discharging" and vice versa, but this is the best
			 * compromise.
			 */
			if ((ebis[i].units == ENVSYS_SWATTS || ebis[i].units == ENVSYS_SAMPS) &&
			    etds[i].cur.data_s &&
			    strncmp(ebis[i].desc, "acpibat", 7) == 0 &&
			    strcmp(&ebis[i].desc[cc - 14], "discharge rate") == 0) {
				p = APM_AC_OFF;
			}

			if (ebis[i].units == ENVSYS_INTEGER &&
			    strcmp(ebis[i].desc, "battery percent") == 0) {
				have_pct = 1;
				r = etds[i].cur.data_s;
			}
			if (ebis[i].units == ENVSYS_INDICATOR &&
			    strcmp(ebis[i].desc, "ACIN present") == 0 &&
			    etds[i].cur.data_s == 0) {
				p = APM_AC_OFF;
			}
		}
		if (!have_pct)
			r = (rtot * 100.0) / maxtot;
#endif

       }

       if(p == APM_AC_OFF) {
         return bprintf("-%d", r);
       } else {
         return bprintf("+%d", r);
       }
	}

#endif
