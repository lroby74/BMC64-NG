#include "vice.h"

#ifdef HAVE_RS232BMC

#include "machine.h"
#include "rs232bmc.h"
#include "rs232.h"

void rs232bmc_init(void)
{
    bmcmodem_init();
}

void rs232bmc_reset(void)
{
    bmcmodem_reset();
}

int rs232bmc_open(int device)
{
    return bmcmodem_open(device);
}

void rs232bmc_close(int fd)
{
    bmcmodem_close(fd);
}

int rs232bmc_putc(int fd, uint8_t byte)
{
    return bmcmodem_putc(fd, byte);
}

int rs232bmc_getc(int fd, uint8_t *byte)
{
    return bmcmodem_getc(fd, byte);
}

int rs232bmc_set_status(int fd, enum rs232handshake_out status)
{
    (void) fd;
    bmcmodem_set_status(status);
    return 0;
}

enum rs232handshake_in rs232bmc_get_status(int fd)
{
    int linea = bmcmodem_has_carrier();

    (void) fd;









    if (machine_class == VICE_MACHINE_PLUS4 ||
        machine_class == VICE_MACHINE_VIC20) {
        return RS232_HSI_CTS | RS232_HSI_DSR | (linea ? RS232_HSI_DCD : 0);
    }










    if (machine_class == VICE_MACHINE_C64 ||
        machine_class == VICE_MACHINE_C64SC ||
        machine_class == VICE_MACHINE_SCPU64 ||
        machine_class == VICE_MACHINE_C128) {
        return RS232_HSI_CTS | RS232_HSI_DSR | (linea ? RS232_HSI_DCD : 0);
    }
    /* aciacore maps CTS to active-low DCD for this virtual TCP modem. Report
       both lines only while the socket (or queued final response) has carrier. */
    return linea ? (RS232_HSI_CTS | RS232_HSI_DSR) : 0;
}

void rs232bmc_set_bps(int fd, unsigned int bps)
{
    (void) fd;
    bmcmodem_set_bps(bps);
}

#endif