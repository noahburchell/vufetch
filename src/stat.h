#ifndef STAT_H
#define STAT_H

#include "fetch.h"

[[nodiscard]] fetch_line get_distro_id(void);
[[nodiscard]] fetch_line get_root_sector(void);
[[nodiscard]] fetch_line get_hostname(void);
[[nodiscard]] fetch_line get_kernel_cc(void);
[[nodiscard]] fetch_line get_boot_uuid(void);
[[nodiscard]] fetch_line get_bios_date(void);
[[nodiscard]] fetch_line get_context_switches(void);
[[nodiscard]] fetch_line get_modules(void);
[[nodiscard]] fetch_line get_entropy(void);
[[nodiscard]] fetch_line get_nmi(void);
[[nodiscard]] fetch_line get_total_sleep(void);
[[nodiscard]] fetch_line get_total_page_faults(void);
[[nodiscard]] fetch_line get_bytes_written(void);
[[nodiscard]] fetch_line get_thermal_zones(void);
[[nodiscard]] fetch_line get_forks(void);
[[nodiscard]] fetch_line get_knives(void);
// ^ actually oom kills

#endif
