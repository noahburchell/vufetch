#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#if __has_include(<sys/sysmacros.h>)
#  include <sys/sysmacros.h>
#endif

#include "stat.h"
#include "fetch.h"

static constexpr size_t INFO_MAX = 64;

#define UNKNOWN "unknown"

static void set_str(char *out, size_t size, const char *s) {
        if (size == 0)
                return;
        // truncating here is intended so bound the conversion rather than let format-truncation flag
        snprintf(out, size, "%.*s", (int)(size - 1), s);
}

[[nodiscard]] static int slurp(const char *path, char *buf, size_t size) {
        size_t used = 0;

        if (size == 0)
                return -1;

        int fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0)
                return -1;

        while (used < size - 1) {
                ssize_t n = read(fd, buf + used, size - 1 - used);

                if (n < 0) {
                        if (errno == EINTR)
                                continue;
                        close(fd);
                        return -1;
                }
                if (n == 0)
                        break;
                used += (size_t)n;
        }

        close(fd);
        buf[used] = '\0';
        return (int)used;
}

static void chomp(char *s) {
        size_t n = strlen(s);

        while (n > 0) {
                char c = s[n - 1];

                if (c != '\n' && c != '\r' && c != ' ' && c != '\t')
                        break;
                s[--n] = '\0';
        }
}

[[nodiscard]] static int read_line_file(const char *path, char *out, size_t size) {
        char buf[512];

        if (slurp(path, buf, sizeof buf) <= 0)
                return -1;

        char *nl = strchr(buf, '\n');
        if (nl)
                *nl = '\0';
        chomp(buf);
        if (buf[0] == '\0')
                return -1;

        set_str(out, size, buf);
        return 0;
}

[[nodiscard]] static int read_counter(const char *path, const char *key, unsigned long long *out) {
        char line[512];
        size_t klen = strlen(key);
        int found = -1;

        FILE *f = fopen(path, "re");
        if (!f)
                return -1;

        while (fgets(line, sizeof line, f)) {
                if (strncmp(line, key, klen) != 0)
                        continue;

                char sep = line[klen];

                if (sep != ' ' && sep != '\t' && sep != ':')
                        continue;
                *out = strtoull(line + klen + 1, nullptr, 10);
                found = 0;
                break;
        }

        fclose(f);
        return found;
}

static void format_bytes(char *out, size_t size, unsigned long long bytes) {
        static const char *const units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
        double v = (double)bytes;
        size_t i = 0;

        while (v >= 1024.0 && i + 1 < ARRAY_LEN(units)) {
                v /= 1024.0;
                i++;
        }

        if (i == 0)
                snprintf(out, size, "%llu %s", bytes, units[0]);
        else
                snprintf(out, size, "%.1f %s", v, units[i]);
}

static void format_duration(char *out, size_t size, unsigned long long secs) {
        unsigned long long d = secs / 86400;
        unsigned long long h = secs % 86400 / 3600;
        unsigned long long m = secs % 3600 / 60;
        unsigned long long s = secs % 60;

        if (d)
                snprintf(out, size, "%llud %lluh %llum", d, h, m);
        else if (h)
                snprintf(out, size, "%lluh %llum", h, m);
        else if (m)
                snprintf(out, size, "%llum %llus", m, s);
        else
                snprintf(out, size, "%llus", s);
}

fetch_line get_distro_id(void) {
        static char info[INFO_MAX];
        static const char *const paths[] = {"/etc/os-release", "/usr/lib/os-release"};
        char buf[4096];

        set_str(info, sizeof info, UNKNOWN);

        for (size_t p = 0; p < ARRAY_LEN(paths); p++) {
                if (slurp(paths[p], buf, sizeof buf) <= 0)
                        continue;

                for (char *line = buf; line; ) {
                        char *nl = strchr(line, '\n');

                        if (nl)
                                *nl = '\0';

                        if (strncmp(line, "ID=", 3) == 0) {
                                char *v = line + 3;

                                if (*v == '"' || *v == '\'') {
                                        char quote = *v++;
                                        char *end = strchr(v, quote);

                                        if (end)
                                                *end = '\0';
                                }
                                chomp(v);
                                if (*v) {
                                        set_str(info, sizeof info, v);
                                        return (fetch_line){.label = "OS", .info = info};
                                }
                        }

                        line = nl ? nl + 1 : nullptr;
                }
        }

        return (fetch_line){.label = "OS", .info = info};
}

static constexpr size_t BLOCKDIR_MAX = 64;

[[nodiscard]] static int root_block_dir(char *out, size_t size) {
        struct stat st = {};
        char line[1024];
        char source[256];

        if (stat("/", &st) == 0) {
                snprintf(out, size, "/sys/dev/block/%u:%u",
                         major(st.st_dev), minor(st.st_dev));
                if (access(out, F_OK) == 0)
                        return 0;
        }

        FILE *f = fopen("/proc/self/mountinfo", "re");
        if (!f)
                return -1;

        source[0] = '\0';
        while (fgets(line, sizeof line, f)) {
                char *save;
                char *mount_point = nullptr, *src = nullptr;
                int field = 0, dash = 0;

                for (char *tok = strtok_r(line, " \n", &save); tok;
                     tok = strtok_r(nullptr, " \n", &save)) {
                        field++;
                        if (field == 5)
                                mount_point = tok;
                        else if (!dash && field >= 7 && strcmp(tok, "-") == 0)
                                dash = field;
                        else if (dash && field == dash + 2) {
                                src = tok;
                                break;
                        }
                }

                if (mount_point && src && strcmp(mount_point, "/") == 0)
                        set_str(source, sizeof source, src);
        }
        fclose(f);

        if (source[0] != '/' || stat(source, &st) != 0 || !S_ISBLK(st.st_mode))
                return -1;

        snprintf(out, size, "/sys/dev/block/%u:%u",
                 major(st.st_rdev), minor(st.st_rdev));
        return access(out, F_OK) == 0 ? 0 : -1;
}

fetch_line get_root_sector(void) {
        static char info[INFO_MAX];
        char dir[BLOCKDIR_MAX];
        char path[BLOCKDIR_MAX + sizeof "/start"];
        char value[INFO_MAX];

        static_assert(sizeof path >= sizeof dir + sizeof "/start" - 1,
                      "path must hold dir plus the /start suffix");

        set_str(info, sizeof info, UNKNOWN);

        if (root_block_dir(dir, sizeof dir) == 0) {
                snprintf(path, sizeof path, "%s/start", dir);

                if (read_line_file(path, value, sizeof value) == 0)
                        set_str(info, sizeof info, value);
                else
                        // whole disk has no start file so it begins at 0 
                        set_str(info, sizeof info, "0");
        }

        return (fetch_line){.label = "Root Sector", .info = info};
}

fetch_line get_hostname(void) {
        static char info[INFO_MAX];

        if (gethostname(info, sizeof info) != 0 || info[0] == '\0') {
                // gethostname() may leave the name untruncated on overflow 
                if (read_line_file("/proc/sys/kernel/hostname", info, sizeof info) != 0)
                        set_str(info, sizeof info, UNKNOWN);
        }
        info[sizeof info - 1] = '\0';

        return (fetch_line){.label = "Host", .info = info};
}

fetch_line get_kernel_cc(void) {
        static char info[INFO_MAX];
        char buf[1024];
        char *start = nullptr, *end = nullptr;
        int depth = 0, group = 0;
        size_t n = 0;

        set_str(info, sizeof info, UNKNOWN);

        if (slurp("/proc/version", buf, sizeof buf) <= 0)
                return (fetch_line){.label = "Kernel CC", .info = info};

        // /proc/version puts the compiler in the second group
        for (char *p = buf; *p; p++) {
                if (*p == '(') {
                        if (depth++ == 0 && ++group == 2)
                                start = p + 1;
                } else if (*p == ')' && depth > 0) {
                        if (--depth == 0 && group == 2) {
                                end = p;
                                break;
                        }
                }
        }
        if (!start || !end)
                return (fetch_line){.label = "Kernel CC", .info = info};
        *end = '\0';

        depth = 0;
        for (const char *p = start; *p && n + 1 < sizeof info; p++) {
                if (*p == '(') {
                        depth++;
                        continue;
                }
                if (*p == ')') {
                        if (depth > 0)
                                depth--;
                        continue;
                }
                if (depth > 0)
                        continue;
                if (*p == ',')
                        break;
                if (*p == ' ' && (n == 0 || info[n - 1] == ' '))
                        continue;
                info[n++] = *p;
        }
        info[n] = '\0';
        chomp(info);
        if (info[0] == '\0')
                set_str(info, sizeof info, UNKNOWN);

        return (fetch_line){.label = "Kernel CC", .info = info};
}

fetch_line get_boot_uuid(void) {
        static char info[INFO_MAX];

        if (read_line_file("/proc/sys/kernel/random/boot_id", info, sizeof info) != 0)
                set_str(info, sizeof info, UNKNOWN);

        return (fetch_line){.label = "Boot ID", .info = info};
}

fetch_line get_bios_date(void) {
        static char info[INFO_MAX];

        if (read_line_file("/sys/class/dmi/id/bios_date", info, sizeof info) != 0)
                set_str(info, sizeof info, UNKNOWN);

        return (fetch_line){.label = "BIOS Date", .info = info};
}

fetch_line get_context_switches(void) {
        static char info[INFO_MAX];
        unsigned long long ctxt;

        if (read_counter("/proc/stat", "ctxt", &ctxt) == 0)
                snprintf(info, sizeof info, "%llu", ctxt);
        else
                set_str(info, sizeof info, UNKNOWN);

        return (fetch_line){.label = "Ctx Switches", .info = info};
}

fetch_line get_modules(void) {
        static char info[INFO_MAX];
        char line[512];
        unsigned long count = 0;

        FILE *f = fopen("/proc/modules", "re");
        if (!f) {
                set_str(info, sizeof info, UNKNOWN);
                return (fetch_line){.label = "Modules", .info = info};
        }

        while (fgets(line, sizeof line, f)) {
                if (strchr(line, '\n'))
                        count++;
        }
        fclose(f);

        snprintf(info, sizeof info, "%lu", count);
        return (fetch_line){.label = "Modules", .info = info};
}

fetch_line get_entropy(void) {
        static char info[INFO_MAX];
        char value[32];

        if (read_line_file("/proc/sys/kernel/random/entropy_avail", value, sizeof value) == 0)
                snprintf(info, sizeof info, "%s bits", value);
        else
                set_str(info, sizeof info, UNKNOWN);

        return (fetch_line){.label = "Entropy", .info = info};
}

fetch_line get_nmi(void) {
        static char info[INFO_MAX];
        char *line = nullptr;
        size_t cap = 0;

        set_str(info, sizeof info, UNKNOWN);

        FILE *f = fopen("/proc/interrupts", "re");
        if (!f)
                return (fetch_line){.label = "NMIs", .info = info};

        // one column per cpu so these lines have no useful upper bound
        while (getline(&line, &cap, f) > 0) {
                const char *p = line;
                unsigned long long total = 0;

                while (*p == ' ' || *p == '\t')
                        p++;
                if (strncmp(p, "NMI:", 4) != 0)
                        continue;

                for (const char *cur = p + 4;;) {
                        char *next;
                        unsigned long long v = strtoull(cur, &next, 10);

                        if (next == cur)
                                break;
                        total += v;
                        cur = next;
                }

                snprintf(info, sizeof info, "%llu", total);
                break;
        }

        free(line);
        fclose(f);

        return (fetch_line){.label = "NMIs", .info = info};
}

fetch_line get_total_sleep(void) {
        static char info[INFO_MAX];
        char value[INFO_MAX];
        struct timespec boot, mono;

        // only newer kernels expose hardware sleep residency here
        if (read_line_file("/sys/power/suspend_stats/total_hw_sleep", value, sizeof value) == 0) {
                format_duration(info, sizeof info,
                                strtoull(value, nullptr, 10) / 1000000ULL);
                return (fetch_line){.label = "Slept", .info = info};
        }

        if (clock_gettime(CLOCK_BOOTTIME, &boot) != 0 ||
            clock_gettime(CLOCK_MONOTONIC, &mono) != 0) {
                set_str(info, sizeof info, UNKNOWN);
                return (fetch_line){.label = "Slept", .info = info};
        }

        time_t delta = boot.tv_sec - mono.tv_sec;

        if (boot.tv_nsec < mono.tv_nsec)
                delta--;
        if (delta > 0) {
                format_duration(info, sizeof info, (unsigned long long)delta);
                return (fetch_line){.label = "Slept", .info = info};
        }

        if (read_line_file("/sys/power/suspend_stats/success", value, sizeof value) == 0 &&
            strtoull(value, nullptr, 10) == 0)
                set_str(info, sizeof info, "never");
        else
                set_str(info, sizeof info, "0s");

        return (fetch_line){.label = "Slept", .info = info};
}

fetch_line get_total_page_faults(void) {
        static char info[INFO_MAX];
        unsigned long long faults;

        if (read_counter("/proc/vmstat", "pgfault", &faults) == 0)
                snprintf(info, sizeof info, "%llu", faults);
        else
                set_str(info, sizeof info, UNKNOWN);

        return (fetch_line){.label = "Page Faults", .info = info};
}

fetch_line get_bytes_written(void) {
        static char info[INFO_MAX];
        char line[512];
        unsigned long long sectors = 0;
        bool any = false;

        FILE *f = fopen("/proc/diskstats", "re");
        if (!f) {
                set_str(info, sizeof info, UNKNOWN);
                return (fetch_line){.label = "Written", .info = info};
        }

        while (fgets(line, sizeof line, f)) {
                char name[64];
                char path[sizeof "/sys/block/" + sizeof name];
                unsigned long long written;

                // major minor name then six fields before sectors written
                if (sscanf(line, " %*u %*u %63s %*u %*u %*u %*u %*u %*u %llu",
                           name, &written) != 2)
                        continue;

                snprintf(path, sizeof path, "/sys/block/%s", name);
                // only whole disks live in /sys/block so partitions drop out
                if (access(path, F_OK) != 0)
                        continue;

                sectors += written;
                any = true;
        }
        fclose(f);

        if (any)
                format_bytes(info, sizeof info, sectors * 512ULL);
        else
                set_str(info, sizeof info, UNKNOWN);

        return (fetch_line){.label = "Written", .info = info};
}

[[nodiscard]] static bool ends_with(const char *s, const char *suffix) {
        size_t ls = strlen(s), lsuf = strlen(suffix);

        return ls >= lsuf && strcmp(s + ls - lsuf, suffix) == 0;
}

static void note_temp(long milli, long *hottest, bool *have_temp) {
        if (!*have_temp || milli > *hottest) {
                *hottest = milli;
                *have_temp = true;
        }
}

[[nodiscard]] static unsigned long hwmon_temps(long *hottest, bool *have_temp) {
        static constexpr char root[] = "/sys/class/hwmon";
        struct dirent *chip;
        unsigned long count = 0;

        DIR *d = opendir(root);
        if (!d)
                return 0;

        while ((chip = readdir(d)) != nullptr) {
                char dir[sizeof root + sizeof chip->d_name];
                struct dirent *ent;

                if (strncmp(chip->d_name, "hwmon", 5) != 0)
                        continue;

                snprintf(dir, sizeof dir, "%s/%s", root, chip->d_name);

                DIR *c = opendir(dir);
                if (!c)
                        continue;

                while ((ent = readdir(c)) != nullptr) {
                        char path[sizeof dir + sizeof ent->d_name];
                        char value[32];

                        // tempN_input only skip _label _crit _max siblings
                        if (strncmp(ent->d_name, "temp", 4) != 0 ||
                            !ends_with(ent->d_name, "_input"))
                                continue;

                        snprintf(path, sizeof path, "%s/%s", dir, ent->d_name);
                        if (read_line_file(path, value, sizeof value) != 0)
                                continue;

                        count++;
                        note_temp(strtol(value, nullptr, 10), hottest, have_temp);
                }
                closedir(c);
        }
        closedir(d);

        return count;
}

fetch_line get_thermal_zones(void) {
        static char info[INFO_MAX];
        unsigned long count = 0;
        long hottest = 0;
        bool have_temp = false;

        DIR *d = opendir("/sys/class/thermal");
        if (d) {
                struct dirent *ent;

                while ((ent = readdir(d)) != nullptr) {
                        char path[sizeof "/sys/class/thermal//temp" + sizeof ent->d_name];
                        char value[32];

                        if (strncmp(ent->d_name, "thermal_zone", 12) != 0)
                                continue;
                        count++;

                        snprintf(path, sizeof path, "/sys/class/thermal/%s/temp",
                                 ent->d_name);
                        if (read_line_file(path, value, sizeof value) != 0)
                                continue;

                        // millidegrees C
                        note_temp(strtol(value, nullptr, 10), &hottest, &have_temp);
                }
                closedir(d);
        }

        if (count == 0)
                count = hwmon_temps(&hottest, &have_temp);

        if (count == 0)
                set_str(info, sizeof info, "none");
        else if (have_temp)
                snprintf(info, sizeof info, "%lu sensors (%.1f C max)",
                         count, hottest / 1000.0);
        else
                snprintf(info, sizeof info, "%lu sensors", count);

        return (fetch_line){.label = "Thermal", .info = info};
}

fetch_line get_forks(void) {
        static char info[INFO_MAX];
        unsigned long long forks;

        if (read_counter("/proc/stat", "processes", &forks) == 0)
                snprintf(info, sizeof info, "%llu", forks);
        else
                set_str(info, sizeof info, UNKNOWN);

        return (fetch_line){.label = "Forks", .info = info};
}

// actually oom kills 
fetch_line get_knives(void) {
        static char info[INFO_MAX];
        unsigned long long kills;

        if (read_counter("/proc/vmstat", "oom_kill", &kills) == 0)
                snprintf(info, sizeof info, "%llu", kills);
        else
                set_str(info, sizeof info, "0");

        return (fetch_line){.label = "OOM Kills", .info = info};
}
