/*
 * MIPS IRQ injection for halucinator's configurable machine.
 *
 * Exposes a `avatar-mips-inject-irq` QMP command that asserts the
 * MIPS CPU's int_pin (Cause.IP) for the requested IRQ index. This
 * mirrors `avatar-armv7m-inject-irq` for the ARMv7-M NVIC and
 * gives halucinator a way to deliver external IRQs through QEMU's
 * GDB/QMP path without modelling a custom interrupt controller.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qapi/qapi-commands-avatar-target.h"
#include "qapi/error.h"
#include "hw/irq.h"
#include "sysemu/sysemu.h"
#include "target/mips/cpu.h"

void qmp_avatar_mips_inject_irq(int64_t num_cpu, int64_t num_irq,
                                Error **errp)
{
    qemu_log_mask(LOG_AVATAR,
                  "Injecting MIPS IRQ %ld on cpu %ld\n",
                  num_irq, num_cpu);
    CPUState *cs = qemu_get_cpu(num_cpu);
    if (cs == NULL) {
        error_setg(errp, "avatar-mips-inject-irq: cpu %ld not found",
                   num_cpu);
        return;
    }
    if (num_irq < 0 || num_irq > 7) {
        error_setg(errp,
            "avatar-mips-inject-irq: num-irq must be in 0..7 (got %ld)",
            num_irq);
        return;
    }
    /* Fire qemu_irq for the given IP bit. The CPU's int_pin
     * lines are allocated in mips_cpu_irq_init (cpu_init.c) and
     * exposed as the first 8 GPIO inputs on the CPU device. The
     * configurable machine doesn't reconfigure them, so a simple
     * qdev_get_gpio_in() lookup gives us the right line. */
    DeviceState *cpu_dev = DEVICE(cs);
    qemu_irq line = qdev_get_gpio_in(cpu_dev, num_irq);
    if (line == NULL) {
        error_setg(errp,
            "avatar-mips-inject-irq: cpu has no GPIO input %ld",
            num_irq);
        return;
    }
    qemu_irq_pulse(line);
}
