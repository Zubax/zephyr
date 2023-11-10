#include <zephyr/arch/common/semihost.h>
#include <stdarg.h>
#include <zephyr/kernel.h>

void semihost_log(const char* fmt, ...)
{
    char    buf[256];
    va_list args;
    va_start(args, fmt);
    const int len = vsnprintk(buf, sizeof(buf), fmt, args);
    va_end(args);
    const char* const path = "./test_results/test_log.txt";
    const int         fd   = semihost_open(path, SEMIHOST_OPEN_A);
    if ((fd < 0) || (semihost_write(fd, buf, len) < 0))
    {
        // Semihost I/O is a vital component of the test suite. There is no sensible
        // failure management policy in existence, so we simply abort execution.
        k_panic();
    }
    (void) semihost_close(fd);
}
