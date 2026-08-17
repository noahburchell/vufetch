#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "fetch.h"
#include "stat.h"

static constexpr size_t OUT_MAX = 4096;

int main(void) {
        char out[OUT_MAX];

        const fetch_line lines[] = {
                get_distro_id(),
                get_hostname(),
                get_kernel_cc(),
                get_boot_uuid(),
                get_bios_date(),
                get_root_sector(),
                get_modules(),
                get_context_switches(),
                get_forks(),
                get_total_page_faults(),
                get_nmi(),
                get_entropy(),
                get_thermal_zones(),
                get_bytes_written(),
                get_total_sleep(),
                get_knives(),
        };

        int n = construct(out, sizeof out, lines, ARRAY_LEN(lines));
        if (n < 0) {
                fputs("vufetch: output buffer too small\n", stderr);
                return 1;
        }

        // only one syscall except kinda not becasue i have to loop becasue kernel stupid >:(
        for (size_t done = 0; done < (size_t)n; ) {
                ssize_t w = write(STDOUT_FILENO, out + done, (size_t)n - done);

                if (w < 0) {
                        if (errno == EINTR)
                                continue;
                        perror("vufetch: write");
                        return 1;
                }
                done += (size_t)w;
        }

        return 0;
}
