#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

const static struct {
	const char *name;
	const char *value;
} omx_actions_[] =      {{"volu", 		"int32:18"},
                         {"vold", 		"int32:17"},
                         {"stop", 		"int32:15"},
                         {"pause", 		"int32:16"},
                         {"hidevideo", 	"int32:28"},
                         {"showvideo", 	"int32:29"},
                         {"togglesubs", "int32:12"},
                         {"hidesubs", 	"int32:30"},
                         {"showsubs", 	"int32:31"},
                         {0, 0}};

enum omx_action_ {OMX_VOLU, OMX_VOLD, OMX_STOP, OMX_PAUSE, OMX_HIDEVID,
                  OMX_SHOWVID, OMX_SUBS_TOGGLE, OMX_SUBS_HIDE, OMX_SUBS_SHOW};


enum vol_ { VOL_UP_, VOL_DOWN_ };

#define _ERR_MSG( format, ... ) printf ("%s [%d]: " format "\n", \
                                        __FUNCTION__,  __LINE__, __VA_ARGS__)

static char *
astrcat(const char *str, ...)
{
	va_list v;
	const size_t mem_inc = 1024;
	char *dest = NULL;
	size_t dest_size = 0;
	size_t dest_len = 0;
	char *write = NULL;
	char *p;
	const char *s;
	size_t s_len;

	if (! str)
		return NULL;

	va_start(v, str);

	for (s = str; s; s = va_arg(v, const char *))
	{
		s_len = strlen(s);

		/* allocate more memory if necessary */
		if ((dest_len + s_len + 1) > dest_size)
		{
			dest_size = dest_size + mem_inc + s_len;
			p = realloc(dest, dest_size);
			if (! p)
			{
				free(dest);
				return NULL;
			}
			dest = p;
			write = dest + dest_len;
		}

		/* concatenate string */
		write = memcpy(write, s, s_len) + s_len;
		dest_len += s_len;
	}

	/* terminate string */
	*write = '\0';

	/* free unused memory */
	p = realloc(dest, dest_len + 1);
	if (p)
		dest = p;

	return dest;
}

static char *
_get_file_contents (const char *path, unsigned int *size)
{
	struct stat fstats;
	int fd = -1;
	char *fcontents = NULL;

	if (! path)
	{
		_ERR_MSG("NULL path", 0);
		return NULL;
	}

	fd = open (path, O_RDONLY);
	if (fd == -1)
	{
		_ERR_MSG("open %s", path);
		goto error;
	}

	if (fstat (fd, &fstats) == -1)
	{
		_ERR_MSG("stat fd %d", fd);
		goto error;
	}

	fcontents = malloc (fstats.st_size + 1);
	if (! fcontents)
	{
		_ERR_MSG("malloc %u", fstats.st_size);
		goto error;
	}

	if (read (fd, fcontents, fstats.st_size) != fstats.st_size)
	{
		_ERR_MSG("read %u bytes", fstats.st_size);
		goto error;
	}
	fcontents[fstats.st_size] = 0;

	if (size)
		*size = fstats.st_size;

cleanup:
	close (fd);

	return fcontents;

error:
	free (fcontents);
	fcontents = NULL;
	goto cleanup;
}

static char *
_chomp (char *str)
{
	char *ptr = str;

	if (ptr)
	{
		/* move to end of string */

		while (*ptr)
			++ptr;

		/* move towards start nullifying trailing white space */

		while (! (--ptr < str))
		{
			if (isspace (*ptr))
				*ptr = '\0';
			else
				break;
		}
	}

	return ptr;
}

static int
_init(void)
{
	const char *user;
	const char *file_dbus_addr;
	const char *file_dbus_pid;
	char *dbus_addr;
	char *dbus_pid;

	user = getenv("USER");
	if (! user)
		user = "";

	file_dbus_addr = astrcat("/tmp/omxplayerdbus.", user, NULL);
	file_dbus_pid  = astrcat(file_dbus_addr, ".pid", NULL);
	if (! file_dbus_addr || ! file_dbus_pid)
		return 0;

	dbus_addr = _get_file_contents (file_dbus_addr, NULL);
	dbus_pid  = _get_file_contents (file_dbus_pid, NULL);
	if (! dbus_addr || ! dbus_pid)
		return 0;

	_chomp(dbus_addr);
	_chomp(dbus_pid);

	if (-1 == setenv("DBUS_SESSION_BUS_ADDRESS", dbus_addr, 1) ||
		-1 == setenv("DBUS_SESSION_BUS_PID", dbus_pid, 1))
		return 0;

	return 1;
}



static int
_omx_action (enum omx_action_ action)
{
	const char *action_val = omx_actions_[action].value;
	pid_t pid;
	int exit_status;

	pid = fork();
	if (pid == -1)
		return 0;
	else if (pid == 0)
	{
		freopen("/dev/null", "r", stdin);
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);

		execl("/usr/bin/dbus-send", "dbus-send", "--print-reply=literal",
		      "--session", "--reply-timeout=500",
		      "--dest=org.mpris.MediaPlayer2.omxplayer",
		      "/org/mpris/MediaPlayer2",
		      "org.mpris.MediaPlayer2.Player.Action", action_val, NULL);

		exit(EXIT_FAILURE);
	}

	waitpid(pid, &exit_status, 0);

	if (exit_status)
	{
		_ERR_MSG("bad exit status", 0);
		return 0;
	}

	return 1;
}


int _omx_is_running (void)
{
	pid_t pid;
	int exit_status;

	pid = fork();
	if (pid == -1)
		return 0;
	else if (pid == 0)
	{
		freopen("/dev/null", "r", stdin);
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);

		execl("/usr/bin/pgrep", "pgrep", "omxplayer", NULL);

		exit(EXIT_FAILURE);
	}

	waitpid(pid, &exit_status, 0);

	if (exit_status)
	{
		_ERR_MSG("bad exit status", 0);
		return 0;
	}

	return 1;
}

static int
_omx_play (const char *file)
{
	if (! file || ! *file)
		return 0;

	if (_omx_is_running ())
		_omx_action (OMX_STOP);

	pid_t pid;
	int exit_status;

	pid = fork();
	if (pid == -1)
		return 0;
	else if (pid == 0)
	{
		char *volume = _get_file_contents("volume", NULL);

		freopen("/dev/null", "r", stdin);
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);

		execl("/usr/bin/omxplayer", "omxplayer", "-o", "local",
		      "--vol", volume, file, NULL);

		exit(EXIT_FAILURE);
	}

	return 1;
}

static int
_omx_vol (const enum vol_ direction, unsigned int repeat)
{
	int vol;
	int new_vol;
	int vol_adjust;
	char *vol_s = _get_file_contents("volume", NULL);
	enum omx_action_ action;

	if (! vol_s)
		return 0;

	if (direction == VOL_DOWN_)
	{
		action = OMX_VOLD;
		vol_adjust = -300;
	}
	else
	{
		action = OMX_VOLU;
		vol_adjust = 300;
	}

	vol = atoi(vol_s);
	new_vol = vol;
	for ( ; repeat; --repeat)
	{
		if (_omx_action(action))
			new_vol += vol_adjust;
	}
	if (new_vol != vol)
	{
		FILE *fp = fopen("volume", "w");
		if (fp)
		{
			fprintf(fp, "%d", new_vol);
			fclose(fp);
		}

		return 1;
	}

	return 0;
}



int
main (int argc, char *argv[])
{
	if (argc < 2)
		return 1;

	_init();

	const char *command = argv[1];
	const char *arg     = argv[2];

	int ret = 1;

	switch (*command)
	{
		case 'p':
			if (! strcmp(command, "play"))
			{
				if (_omx_play (arg))
					ret = 0;
			}
			break;

		case 's':
			if (! strcmp (command, "stop"))
			{
				if (_omx_action (OMX_STOP))
					ret = 0;
			}
			break;

		case 'v':
			if (! strncmp (command, "vol", 3))
			{
				enum vol_ dir;
				const unsigned int num = atoi (arg);

				switch (command[3])
				{
					case 'u':	dir = VOL_UP_;		break;
					case 'd':	dir = VOL_DOWN_;	break;
					default:	break;
				}

				if (_omx_vol(dir, num))
					ret = 0;
			}
			break;

		default:
			printf ("Unknown command: %s\n", command);
	}

	return ret;
}





