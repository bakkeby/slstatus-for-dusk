#include <libconfig.h>

#define PATH_MAX 4080
const char *progname = "slstatus";

static char *unknown_string = NULL;
static int num_modules = 0;
static struct arg *modules = NULL;
int maximum_status_length = MAXLEN;

#if HAVE_MPD
#ifndef MPD_TITLE_LENGTH
#define MPD_TITLE_LENGTH 20
#endif

#ifndef MPD_ON_TEXT_FITS
#define MPD_ON_TEXT_FITS NO_SCROLL
#endif

#ifndef MPD_LOOP_TEXT
#define MPD_LOOP_TEXT " ~ "
#endif

int mpd_title_length = MPD_TITLE_LENGTH;
char *mpd_loop_text = NULL;
int mpd_on_text_fits = MPD_ON_TEXT_FITS;
#endif // HAVE_MPD

void set_config_path(const char* filename, char *config_path, char *config_file);
int config_lookup_strdup(const config_t *cfg, const char *name, char **strptr);
int config_setting_lookup_strdup(const config_setting_t *cfg, const char *name, char **strptr);
int config_lookup_unsigned_int(const config_t *cfg, const char *name, unsigned int *ptr);
int config_setting_lookup_unsigned_int(const config_setting_t *cfg, const char *name, unsigned int *ptr);
int config_setting_get_unsigned_int(const config_setting_t *cfg_item, unsigned int *ptr);

void cleanup_config(void);
void load_config(void);
void load_fallback_config(void);
void load_modules(config_t *cfg);
#if HAVE_MPD
void load_mpdonair(config_t *cfg);
int parse_mpd_on_text_fits(const char *string);
#endif

ArgFunc parse_function(const char *string);

int
config_lookup_unsigned_int(const config_t *cfg, const char *name, unsigned int *ptr)
{
	return config_setting_get_unsigned_int(config_lookup(cfg, name), ptr);
}

int
config_setting_lookup_unsigned_int(const config_setting_t *cfg, const char *name, unsigned int *ptr)
{
	return config_setting_get_unsigned_int(config_setting_lookup(cfg, name), ptr);
}

int
config_setting_get_unsigned_int(const config_setting_t *cfg_item, unsigned int *ptr)
{
	if (!cfg_item)
		return 0;

	int integer = config_setting_get_int(cfg_item);

	if (integer >= 0) {
		*ptr = (unsigned int)integer;
		return 1;
	}

	return 1;
}

int
config_lookup_strdup(const config_t *cfg, const char *name, char **strptr)
{
	const char *string;
	if (config_lookup_string(cfg, name, &string)) {
		free(*strptr);
		*strptr = strdup(string);
		return 1;
	}

	return 0;
}

int
config_setting_lookup_strdup(const config_setting_t *cfg, const char *name, char **strptr)
{
	const char *string;
	if (config_setting_lookup_string(cfg, name, &string)) {
		free(*strptr);
		*strptr = strdup(string);
		return 1;
	}

	return 0;
}

void
set_config_path(const char* filename, char *config_path, char *config_file)
{
	const char *xdg_config_home = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (xdg_config_home && xdg_config_home[0] != '\0') {
		snprintf(config_path, PATH_MAX, "%s/%s/", xdg_config_home, progname);
	} else if (home) {
		snprintf(config_path, PATH_MAX, "%s/.config/%s/", home, progname);
	} else {
		return;
	}

	snprintf(config_file, PATH_MAX, "%s/%s.cfg", config_path, filename);
}

void
load_config(void)
{
	config_t cfg;
	char config_path[PATH_MAX] = {0};
	char config_file[PATH_MAX] = {0};

	set_config_path(progname, config_path, config_file);
	config_init(&cfg);
	config_set_include_dir(&cfg, config_path);

	if (config_read_file(&cfg, config_file)) {
		config_lookup_unsigned_int(&cfg, "interval", &interval);
		config_lookup_int(&cfg, "maximum_length", &maximum_status_length);
		config_lookup_strdup(&cfg, "unknown_string", &unknown_string);
		load_modules(&cfg);
		#if HAVE_MPD
		load_mpdonair(&cfg);
		#endif
	} else if (strcmp(config_error_text(&cfg), "file I/O error")) {
		fprintf(stderr, "Error reading config at %s\n", config_file);
		fprintf(stderr, "%s:%d - %s\n",
			config_error_file(&cfg),
			config_error_line(&cfg),
			config_error_text(&cfg)
		);
	}

	load_fallback_config();
	config_destroy(&cfg);
}

void
load_fallback_config(void)
{
	int i;

	#if HAVE_MPD
	if (mpd_loop_text == NULL) {
		mpd_loop_text = strdup(MPD_LOOP_TEXT);
	}
	#endif

	/* Fall back to default configuration if there is no config file */
	if (!modules) {
		num_modules = LEN(args);
		modules = calloc(num_modules, sizeof(arg));
		for (i = 0; i < num_modules; i++) {
			modules[i].func = args[i].func;
			modules[i].fmt = (args[i].fmt ? strdup(args[i].fmt) : NULL);
			modules[i].args = (args[i].args ? strdup(args[i].args) : NULL);
			modules[i].status_no = (args[i].status_no ? strdup(args[i].status_no) : NULL);
			modules[i].update_interval = args[i].update_interval;
		}
	}
}

void
cleanup_config(void)
{
	int i;

	free(unknown_string);
	for (i = 0; i < num_modules; i++) {
		free(modules[i].fmt);
		free(modules[i].args);
		free(modules[i].status_no);
	}
	free(modules);
	#if HAVE_MPD
	free(mpd_loop_text);
	#endif
}

void
load_modules(config_t *cfg)
{
	int i;
	const char *func;
	const config_setting_t *modules_t, *module_t;

	modules_t = config_lookup(cfg, "modules");
	if (!modules_t || !config_setting_is_list(modules_t))
		return;

	num_modules = config_setting_length(modules_t);
	if (!num_modules)
		return;

	modules = calloc(num_modules, sizeof(arg));

	/* Parse and set the functions and arguments based on config */
	for (i = 0; i < num_modules; i++) {
		module_t = config_setting_get_elem(modules_t, i);

		if (config_setting_lookup_string(module_t, "function", &func)) {
			modules[i].func = parse_function(func);
		}

		if (!config_setting_lookup_strdup(module_t, "format", &modules[i].fmt))
			modules[i].fmt = strdup("%s");
		if (!config_setting_lookup_strdup(module_t, "argument", &modules[i].args))
			modules[i].args = NULL;
		if (!config_setting_lookup_strdup(module_t, "status_no", &modules[i].status_no)) {
			fprintf(stderr, "Warning! no status_no specified for function = \"%s\", format = \"%s\", argument = \"%s\"\n", func, modules[i].fmt, modules[i].args);
			modules[i].status_no = NULL;
		}
		if (!config_setting_lookup_unsigned_int(module_t, "update_interval", &modules[i].update_interval))
			modules[i].update_interval = 1;
	}
}

#if HAVE_MPD
void
load_mpdonair(config_t *cfg)
{
	const char *string;

	config_lookup_int(cfg, "mpd.title_length", &mpd_title_length);
	config_lookup_strdup(cfg, "mpd.loop_text", &mpd_loop_text);
	if (config_lookup_string(cfg, "mpd.on_text_fits", &string)) {
		mpd_on_text_fits = parse_mpd_on_text_fits(string);
	}
}
#endif // HAVE_MPD

#define map(S, I) if (!strcasecmp(string, S)) return I;

ArgFunc
parse_function(const char *string)
{
	map("backlight_perc", backlight_perc);
	map("battery_perc", battery_perc);
	map("battery_remaining", battery_remaining);
	map("battery_state", battery_state);
	map("cat", cat);
	map("cpu_freq", cpu_freq);
	map("cpu_perc", cpu_perc);
	map("datetime", datetime);
	map("disk_free", disk_free);
	map("disk_perc", disk_perc);
	map("disk_total", disk_total);
	map("disk_used", disk_used);
	map("entropy", entropy);
	map("gid", gid);
	map("hostname", hostname);
	map("io_in", io_in);
	map("io_out", io_out);
	map("io_perc", io_perc);
	map("ipv4", ipv4);
	map("ipv6", ipv6);
	map("kernel_release", kernel_release);
	map("keyboard_indicators", keyboard_indicators);
	map("keymap", keymap);
	map("load_avg", load_avg);
	#if HAVE_MPD
	map("mpdonair", mpdonair);
	#endif
	map("netspeed_rx", netspeed_rx);
	map("netspeed_tx", netspeed_tx);
	map("num_files", num_files);
	map("ram_free", ram_free);
	map("ram_perc", ram_perc);
	map("ram_total", ram_total);
	map("ram_used", ram_used);
	map("run_command", run_command);
	map("run_exec", run_exec);
	map("run_exec", run_exec);
	map("swap_free", swap_free);
	map("swap_perc", swap_perc);
	map("swap_total", swap_total);
	map("swap_used", swap_used);
	map("temp", temp);
	map("uid", uid);
	map("uptime", uptime);
	map("username", username);
	map("vol_perc", vol_perc);
	map("wifi_essid", wifi_essid);
	map("wifi_perc", wifi_perc);

	fprintf(stderr, "Warning: config could not find function with name %s\n", string);
	return datetime;
}

#if HAVE_MPD
int
parse_mpd_on_text_fits(const char *string)
{
	map("NO_SCROLL", NO_SCROLL);
	map("FULL_SPACE_SEPARATOR", FULL_SPACE_SEPARATOR);
	map("FORCE_SCROLL", FORCE_SCROLL);

	fprintf(stderr, "Warning: config could not find on text fits option with name %s\n", string);
	return NO_SCROLL;
}
#endif // HAVE_MPD

#undef map
