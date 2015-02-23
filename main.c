#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PLAYER_PIPE_ "in.pipe"
#define PLAYER_OUTPUT_ "stdout"
#define RADIO_PLS_DIR_ "/home/pi/hdd/radio/"

static struct control_ {
	const char *name;
	const char  key;
} controls_[] = { "stop", 'q',
                  "vol_up", '=',
                  "vol_down", '-',
                  0, 0 };


void
_err_msg (const char * msg, ...)
{
	va_list args;

	printf ("<p><strong>ERROR: ");
	va_start (args, msg);
	vfprintf (stdout, msg, args);
	va_end (args);
	puts ("</strong></p>");
}

static int
_list_radio_playlists (void)
{
	char buf[512];
	const char cmd[] = "for file in $(ls -1 " RADIO_PLS_DIR_ "*pls); "
	                   "do basename \"$file\" .pls; done";

	FILE *pipe = popen (cmd, "r");
	if (pipe)
	{
		puts ("<ul>");
		char *newline;
		while (fgets (buf, sizeof (buf), pipe))
		{
			newline = strrchr (buf, (int)'\n');
			if (newline)
				*newline = '\0';
			printf ("<li><a href=\"?%s\">%s</a></li>\n", buf, buf);
		}
		puts ("</ul>");

		pclose (pipe);
	}
	else
		_err_msg ("failed to open pipe: %s", cmd);

	return 0;
}

static int
_player_running ()
{
	if (! system ("pgrep omxplayer 2>&1 > /dev/null"))
		return 1;
	else
		return 0;
}

static int
_process_player_output (void)
{
	FILE *fp = fopen (PLAYER_OUTPUT_, "r");
	const char *grep_vol = "Current Volume: ";
	const size_t grep_vol_len = strlen (grep_vol);
	char buffer[512];
	char volume[10];
	char *vol = NULL, *vol_end = NULL, *point = NULL;

	if (! fp)
		return 0;

	while (fgets (buffer, sizeof (buffer), fp))
	{
		if (! strncmp (grep_vol, buffer, grep_vol_len))
		{
			vol = buffer + grep_vol_len;
			vol_end = strchr (vol, 'd');
			if (! vol_end)
				continue;

			*vol_end = '\0';

			point = strchr (vol, '.');
			if (point)
				strcpy (point, point+1);
			
			strncpy (volume, vol, sizeof (volume));
			volume[sizeof (volume) -1] = '\0';
		}
	}

	fclose (fp);

	if (vol)
	{
		fp = fopen ("volume", "w");
		if (fp)
		{
			fputs (volume, fp); 
			fclose (fp);
		}
	}

	return 1;
}

static int
_send_to_player (const char cmd)
{
	int pipe = -1;
	int retry;	
	
	for (retry = 10; pipe == -1 && retry; --retry)
		pipe = open ("in.pipe", O_WRONLY | O_NONBLOCK);

	if (pipe == -1)
	{
		_err_msg ("failed to open player pipe"); 
		return 0;
	}

	if (cmd)
		write (pipe, &cmd, sizeof (cmd));

	if (cmd == 'q')
	{
		while (_player_running ())
		{
			write (pipe, "qqqqqqqqqq", 10);
			fsync (pipe);
			sleep (1);
		}
		_process_player_output ();
	}

	close (pipe);

	return 1;
}

static int
_process_query_string (void)
{
	int player_running = _player_running ();
	const char *query_string = getenv ("QUERY_STRING");

	if (! query_string) 
		return 0;
	
	/* if query_string contains a commmand, carry it out */

	struct control_ *ctl = controls_;
	for (; ctl->name; ctl++)
	{
		if (! strcmp (ctl->name, query_string))
		{
			if (player_running) 
				_send_to_player (ctl->key); 

			ctl = NULL;
			break; 
		}
	}	
	if (ctl)
	{
		/* no command was found. assume query_string to be
		 * criteria for which station to listen to
		 */
		if (player_running)
			_send_to_player ('q');

		char cmd[1024];
		snprintf (cmd, sizeof (cmd),
				  "/usr/local/bin/fl-radio.sh \"%s\""
				  " --vol \"$(cat volume)\" < " PLAYER_PIPE_
				  " 2> stderr 1> " PLAYER_OUTPUT_ 
				  " & echo > " PLAYER_PIPE_,
				  query_string);
		system (cmd);
	}

	return 1;
}

static int
_print_controls (void)
{
	const struct control_ *ctl = controls_;
	
	puts ("<ul>");

	for (; ctl->name; ctl++)
		printf ("<li><a href=\"?%s\">%s</a></li>\n", ctl->name, ctl->name);

	puts ("</ul>");

	return 1;
}
	
int
main (int argc, char *argv[])
{
	puts ("Content-type: text/html\n\n" 
		  "<html>\n"
	      "<head>\n"
		  "<link rel=\"stylesheet\" type=\"text/css\" href=\"radio.css\">\n"
		  "<title>RPi: Radio</title>\n"
		  "</head>\n\n"
		  "<body>\n"
		  "<h1>RPi: Radio</h1>");

	_process_query_string (); 
	_print_controls ();
	_list_radio_playlists ();

	puts ("</body>\n</html>");

	return EXIT_SUCCESS;
}
