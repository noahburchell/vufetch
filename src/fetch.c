#include <stdio.h>
#include <string.h>

#include "fetch.h"

static constexpr int LOGO_GAP = 5;

static const char logo_ascii[] =
        "     .--.        _\n"
        "    |o_o |      | |\n"
        "    |:_/ |      | |\n"
        "   //   \\ \\     |_|\n"
        "  (|     | )     _\n"
        " /'\\_   _/`\\    (_)\n"
        " \\___)=(___/\n"
;

[[nodiscard]] static size_t logo_metrics(size_t *width) {
        size_t rows = 0, max = 0;

        for (const char *p = logo_ascii; *p; ) {
                const char *nl = strchr(p, '\n');
                size_t len = nl ? (size_t)(nl - p) : strlen(p);

                if (len > max)
                        max = len;
                rows++;
                p = nl ? nl + 1 : p + len;
        }

        *width = max;
        return rows;
}

int construct(char *out, size_t size, const fetch_line *lines, size_t count) {
        size_t used = 0;
        size_t logo_width = 0;
        size_t logo_rows = logo_metrics(&logo_width);
        const char *lp = logo_ascii;
        int label_width = 0;

        if (!out || size == 0)
                return -1;
        out[0] = '\0';

        if (!lines)
                count = 0;

        for (size_t i = 0; i < count; i++) {
                int n = (int)strlen(lines[i].label);

                if (n > label_width)
                        label_width = n;
        }

        size_t rows = logo_rows > count ? logo_rows : count;

        for (size_t r = 0; r < rows; r++) {
                const char *seg = "";
                size_t seglen = 0;
                int n;

                if (r < logo_rows) {
                        const char *nl = strchr(lp, '\n');

                        seg = lp;
                        seglen = nl ? (size_t)(nl - lp) : strlen(lp);
                        lp = nl ? nl + 1 : lp + seglen;
                }

                if (r < count) {
                        const char *info = lines[r].info ? lines[r].info : "";
                        int label_pad = label_width -
                                        (int)strlen(lines[r].label) + 1;

                        n = snprintf(out + used, size - used,
                                     "%-*.*s%*s%s:%*s%s\n",
                                     (int)logo_width, (int)seglen, seg,
                                     LOGO_GAP, "",
                                     lines[r].label,
                                     label_pad, "",
                                     info);
                } else {
                        n = snprintf(out + used, size - used, "%.*s\n",
                                     (int)seglen, seg);
                }

                if (n < 0 || (size_t)n >= size - used)
                        return -1;
                used += (size_t)n;
        }

        return (int)used;
}
