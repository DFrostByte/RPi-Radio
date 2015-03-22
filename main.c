#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

static struct {
	const char *radio_exe;
	const char *omxctl_exe;
	const char *pls_dir;
} cfg_ = { "./radio-test.sh",
		   "./omxctl.sh",
		   "/srv/ftp/radio" };

static struct control_ {
	const char *cmd;
	const char *arg;
	const char *label;
} controls_[] = { "stop",	NULL,  	"Stop",
                  "volu",	"1",	"Vol +",
                  "volu",	"3",   	"Vol +++",
                  "vold",	"3",  	"Vol ---",
                  "vold",	"1",   	"Vol -" };
static const size_t controls_n_ = (sizeof (controls_) /
                                   sizeof (struct control_));

#define _ERR_MSG( format, ... ) printf ("<p><strong>%s [%d]: " format "</strong></p>", \
                                        __FUNCTION__,  __LINE__, __VA_ARGS__)

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


static int
_print_radio_playlists (void)
{
	int fsize = 0;
	char *fcontents = _get_file_contents ("stations.basename", &fsize);

	if (! fcontents)
		return 0;

	char *eof = fcontents + fsize;
	char *line = fcontents;
	char *line_end = NULL;

	puts ("<ul id=\"stations\">");
	while (line < eof)
	{
		line_end = strchr (line, '\n');
		if (line_end)
			*line_end = '\0';

		printf ("<li><a href=\"?%s\">%s</a></li>\n", line, line);

		line = line_end + 1;
	}
	puts ("</ul>");

	free (fcontents);

	return 1;
}

static int
_send_to_player (const char *command, const char *arg)
{
	if (! command || ! *command)
		return 0;

	pid_t pid;
	int exit_status;

	pid = fork();
	if (pid == -1)
		return 0;
	else if (pid == 0)
	{
		execl(cfg_.omxctl_exe, cfg_.omxctl_exe, command, arg, NULL);
		exit(EXIT_FAILURE);
	}

	waitpid(pid, &exit_status, 0);

	if (exit_status)
	{
		_ERR_MSG("execl() failed: %s %s %s", cfg_.omxctl_exe, command, arg);
		return 0;
	}

	return 1;
}

static int
_is_control_index (const char *str)
{
	unsigned long index = 0;
	char *nan = NULL;

	if (str)
	{
		index = strtoul (str, &nan, 10);
		if (nan != str && *nan == '\0')
		{
			if (index < controls_n_)
				return index;
		}
	}

	return -1;
}

static int
_process_post_controls (void)
{
	const char ctl_spec[] = "control=";
	char buf[sizeof (ctl_spec) + 1 + 1 + 1];
	int ctl_index;

	if (! fgets (buf, sizeof (buf), stdin))
	{
		if (ferror (stdin))
		{
			_ERR_MSG( "%s", strerror (errno));
			return -1;
		}

		return 0;
	}

	_chomp (buf);

	if (strncmp (buf, ctl_spec, sizeof (ctl_spec) - 1))
	{
		_ERR_MSG("not a control: %s", buf);
		return -1;
	}

	ctl_index = _is_control_index (buf + sizeof (ctl_spec) - 1);

	if (ctl_index == -1)
	{
		_ERR_MSG("no control found: %s", buf);
		return -1;
	}

	if (! _send_to_player (controls_[ctl_index].cmd, controls_[ctl_index].arg))
		return -1;

	return 1;
}

static char *
_pls_path_from_basename(const char *fname)
{
	char *path;

	if (! asprintf (&path, "%s/%s.pls", cfg_.pls_dir, basename(fname)) > 0)
	{
		_ERR_MSG("asprintf", 0);
		path = NULL;
	}

	return path;
}

static int
_save_last_played(const char *basename)
{
	FILE *fp = fopen("station_recent", "w");
	int ret = 0;

	if (fp)
	{
		if (fputs(basename, fp) != EOF)
			ret = 1;

		fclose(fp);
	}

	return ret;
}

static char *
_url_from_pls(const char *pls)
{
	if (! pls || ! *pls)
		return 0;

	char *contents = NULL;
	char *url = NULL;

	if (contents = _get_file_contents(pls, NULL))
	{
		/* urls are contained on lines =~ "File\d+=.*\n" */
		if (url = strcasestr(contents, "file"))
		{
			char *url_end;

			url_end = strchrnul(url, '\n');
			*url_end = 0;
			url = strchrnul(url, '=');
			++url;

			if (url_end > url)
				url = strdup(url);
			else
				url = NULL;
		}
	}

	free(contents);

	return url;
}

static int
_process_query_string (void)
{
	const char *query_string = getenv ("QUERY_STRING");
	char *pls_path = NULL;
	char *url = NULL;
	int ret = 0;

	if (! query_string || ! *query_string)
		return 0;

	if ((pls_path = _pls_path_from_basename(query_string)) &&
		(url = _url_from_pls(pls_path)))
	{
		if (_send_to_player("play", url))
		{
			_save_last_played(query_string);
			ret = 1;
		}
	}

	if (! ret)
		_ERR_MSG("URL from '%s'", pls_path);

cleanup:
	free(pls_path);
	free(url);

	return ret;
}

static int
_print_controls (void)
{
	int i;

	puts ("<ul id=\"controls\">\n"
	      "<form method=\"post\" action=\"\">");

	for (i = 0; i < controls_n_; i++)
		printf ("<li>"
		        "<button name=\"control\" type=\"submit\" value=\"%u\">%s</button>"
		        "</li>\n",
		        i, controls_[i].label);

	puts ("</form>\n"
	      "</ul>");

	return 1;
}

static int
_print_info (void)
{
	char *recent_stn = _get_file_contents ("station_recent", NULL);
	char *volume     = _get_file_contents ("volume", NULL);

	if (_chomp (recent_stn))
		printf ("<p>Last played: <a href=\"?%s\">%s</a></p>\n",
		        recent_stn, recent_stn);

	if (_chomp (volume))
		printf ("<p>Volume: %s</p>\n", volume);

	return 1;
}

int
main (int argc, char *argv[])
{
	puts ("Content-type: text/html\r\n\r\n"
	      "<!DOCTYPE html>\n"
		  "<html>\n"
	      "<head>\n"
		  "<link rel=\"stylesheet\" type=\"text/css\" href=\"radio.css\">\n"
		  "<title>RPi: Radio</title>\n"
		  "</head>\n\n"
		  "<body>\n"
		  "<article>\n"
		  "<h1>RPi: Radio</h1>");

    /* process any posted controls. if none were sent, process query string
     * for station
     */
	if (! _process_post_controls ())
		_process_query_string ();

	_print_info ();

	puts ("<h2>Controls</h2>");
	_print_controls ();

	puts ("<h2>Stations</h2>");
	_print_radio_playlists ();

	puts ("</article>\n</body>\n</html>");

	return EXIT_SUCCESS;
}
