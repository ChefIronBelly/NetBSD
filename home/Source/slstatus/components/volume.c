/* See LICENSE file for copyright and license details. */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../slstatus.h"
#include "../util.h"

#if defined(__OpenBSD__) | defined(__FreeBSD__)
	#include <poll.h>
	#include <sndio.h>
	#include <stdlib.h>
	#include <sys/queue.h>

	struct control {
		LIST_ENTRY(control)	next;
		unsigned int		addr;
	#define CTRL_NONE	0
	#define CTRL_LEVEL	1
	#define CTRL_MUTE	2
		unsigned int		type;
		unsigned int		maxval;
		unsigned int		val;
	};

	static LIST_HEAD(, control) controls = LIST_HEAD_INITIALIZER(controls);
	static struct pollfd *pfds;
	static struct sioctl_hdl *hdl;
	static int initialized;

	/*
	 * Call-back to obtain the description of all audio controls.
	 */
	static void
	ondesc(void *unused, struct sioctl_desc *desc, int val)
	{
		struct control *c, *ctmp;
		unsigned int type = CTRL_NONE;

		if (desc == NULL)
			return;

		/* Delete existing audio control with the same address. */
		LIST_FOREACH_SAFE(c, &controls, next, ctmp) {
			if (desc->addr == c->addr) {
				LIST_REMOVE(c, next);
				free(c);
				break;
			}
		}

		/* Only match output.level and output.mute audio controls. */
		if (desc->group[0] != 0 ||
		    strcmp(desc->node0.name, "output") != 0)
			return;
		if (desc->type == SIOCTL_NUM &&
		    strcmp(desc->func, "level") == 0)
			type = CTRL_LEVEL;
		else if (desc->type == SIOCTL_SW &&
			 strcmp(desc->func, "mute") == 0)
			type = CTRL_MUTE;
		else
			return;

		c = malloc(sizeof(struct control));
		if (c == NULL) {
			warn("sndio: failed to allocate audio control\n");
			return;
		}

		c->addr = desc->addr;
		c->type = type;
		c->maxval = desc->maxval;
		c->val = val;
		LIST_INSERT_HEAD(&controls, c, next);
	}

	/*
	 * Call-back invoked whenever an audio control changes.
	 */
	static void
	onval(void *unused, unsigned int addr, unsigned int val)
	{
		struct control *c;

		LIST_FOREACH(c, &controls, next) {
			if (c->addr == addr)
				break;
		}
		c->val = val;
	}

	static void
	cleanup(void)
	{
		struct control *c;

		if (hdl) {
			sioctl_close(hdl);
			hdl = NULL;
		}

		free(pfds);
		pfds = NULL;

		while (!LIST_EMPTY(&controls)) {
			c = LIST_FIRST(&controls);
			LIST_REMOVE(c, next);
			free(c);
		}
	}

	static int
	init(void)
	{
		hdl = sioctl_open(SIO_DEVANY, SIOCTL_READ, 0);
		if (hdl == NULL) {
			warn("sndio: cannot open device");
			goto failed;
		}

		if (!sioctl_ondesc(hdl, ondesc, NULL)) {
			warn("sndio: cannot set control description call-back");
			goto failed;
		}

		if (!sioctl_onval(hdl, onval, NULL)) {
			warn("sndio: cannot set control values call-back");
			goto failed;
		}

		pfds = calloc(sioctl_nfds(hdl), sizeof(struct pollfd));
		if (pfds == NULL) {
			warn("sndio: cannot allocate pollfd structures");
			goto failed;
		}

		return 1;
	failed:
		cleanup();
		return 0;
	}

	const char *
	vol_perc(const char *unused)
	{
		struct control *c;
		int n, v, value;

		if (!initialized)
			initialized = init();

		if (hdl == NULL)
			return NULL;

		n = sioctl_pollfd(hdl, pfds, POLLIN);
		if (n > 0) {
			n = poll(pfds, n, 0);
			if (n > 0) {
				if (sioctl_revents(hdl, pfds) & POLLHUP) {
					warn("sndio: disconnected");
					cleanup();
					initialized = 0;
					return NULL;
				}
			}
		}

		value = 100;
		LIST_FOREACH(c, &controls, next) {
			if (c->type == CTRL_MUTE && c->val == 1)
				value = 0;
			else if (c->type == CTRL_LEVEL) {
				v = (c->val * 100 + c->maxval / 2) / c->maxval;
				/* For multiple channels return the minimum. */
				if (v < value)
					value = v;
			}
		}

		return bprintf("%d", value);
	}
#elif defined(__NetBSD__)
  #include <sys/audioio.h>
  #include <sys/ioctl.h>
  #include <sys/types.h>
  #include <paths.h>
	#include <stdlib.h>
  #include <string.h>

  static struct field {
  	char *name;
  	mixer_ctrl_t *valp;
  	mixer_devinfo_t *infp;
  	char changed;
  } *fields, *rfields;

  static mixer_ctrl_t *values;
  static mixer_devinfo_t *infos;

  static const char mixer_path[] = _PATH_MIXER;

  static char *
  catstr(char *p, char *q)
  {
    char *r;

    asprintf(&r, "%s.%s", p, q);
    if (!r)
      warn("malloc");
    return r;
  }

	const char *
	vol_perc(const char *mixterctlVariable)
	{
    int fd, i, j, pos;
    const char *file;
    mixer_devinfo_t dinfo;
    int ndev;
    int cvol = 0;
    int chan = 1;

    file = mixer_path;

    fd = open(file, O_RDWR);
    /* Try with mixer0 but only if using the default device. */
    if (fd == -1 && file == mixer_path) {
      file = _PATH_MIXER0;
      fd = open(file, O_RDWR);
    }

    if (fd == -1)
      warn("Can't open `%s'", file);

    for (ndev = 0; ; ndev++) {
      dinfo.index = ndev;
      if (ioctl(fd, AUDIO_MIXER_DEVINFO, &dinfo) == -1)
        break;
    }
    rfields = calloc(ndev, sizeof *rfields);
    fields = calloc(ndev, sizeof *fields);
    infos = calloc(ndev, sizeof *infos);
    values = calloc(ndev, sizeof *values);

    for (i = 0; i < ndev; i++) {
      infos[i].index = i;
      if (ioctl(fd, AUDIO_MIXER_DEVINFO, &infos[i]) == -1)
        warn("AUDIO_MIXER_DEVINFO for %d", i);
    }

    for (i = 0; i < ndev; i++) {
      rfields[i].name = infos[i].label.name;
      rfields[i].valp = &values[i];
      rfields[i].infp = &infos[i];
    }

    for (i = 0; i < ndev; i++) {
      values[i].dev = i;
      values[i].type = infos[i].type;
      if (infos[i].type != AUDIO_MIXER_CLASS) {
        values[i].un.value.num_channels = 2;
        if (ioctl(fd, AUDIO_MIXER_READ, &values[i]) == -1) {
          values[i].un.value.num_channels = 1;
          if (ioctl(fd, AUDIO_MIXER_READ, &values[i])
              == -1)
            warn("AUDIO_MIXER_READ");
        }
      }
    }

    for (j = i = 0; i < ndev; i++) {
      if (infos[i].type != AUDIO_MIXER_CLASS &&
          infos[i].type != -1) {
        fields[j++] = rfields[i];
        for (pos = infos[i].next; pos != AUDIO_MIXER_LAST;
             pos = infos[pos].next) {
          fields[j] = rfields[pos];
          fields[j].name = catstr(rfields[i].name,
                                  infos[pos].label.name);
          infos[pos].type = -1;
          j++;
        }
      }
    }

    for (i = 0; i < j; i++) {
      int cls = fields[i].infp->mixer_class;
      if (cls >= 0 && cls < ndev)
        fields[i].name = catstr(infos[cls].label.name,
                                fields[i].name);
    }

    /*
      If dual channel, add both channels up and take it into account
      when we get the percent.
     */
		for (i = 0; i < j; i++) {
      mixer_ctrl_t *m;
			m = fields[i].valp;
      if(strncmp(fields[i].name, mixterctlVariable, strlen(mixterctlVariable)) == 0) {
        if (m->un.value.num_channels == 1) {
          cvol = m->un.value.level[0];
        } else {
          cvol = m->un.value.level[0] + m->un.value.level[1];
          chan = 2;
        }
        break;
      }
		}

    if(chan == 1) {
      cvol = cvol * 100 / AUDIO_MAX_GAIN;
    } else {
      cvol = cvol * 50 / AUDIO_MAX_GAIN;
    }

		return bprintf("%d", cvol);
	}
#else
	#include <sys/soundcard.h>

	const char *
	vol_perc(const char *card)
	{
		size_t i;
		int v, afd, devmask;
		char *vnames[] = SOUND_DEVICE_NAMES;

		if ((afd = open(card, O_RDONLY | O_NONBLOCK)) < 0) {
			warn("open '%s':", card);
			return NULL;
		}

		if (ioctl(afd, (int)SOUND_MIXER_READ_DEVMASK, &devmask) < 0) {
			warn("ioctl 'SOUND_MIXER_READ_DEVMASK':");
			close(afd);
			return NULL;
		}
		for (i = 0; i < LEN(vnames); i++) {
			if (devmask & (1 << i) && !strcmp("vol", vnames[i])) {
				if (ioctl(afd, MIXER_READ(i), &v) < 0) {
					warn("ioctl 'MIXER_READ(%ld)':", i);
					close(afd);
					return NULL;
				}
			}
		}

		close(afd);

		return bprintf("%d", v & 0xff);
	}
#endif
