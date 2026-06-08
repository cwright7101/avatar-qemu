/*
 * ARM IRQ injection helper for halucinator's configurable machine.
 *
 * Pulses an SPI input line on a sysbus arm_gic device by name
 * ("gic" — the YAML peripheral name halucinator emits for the GIC)
 * so a SPI-class IRQ reaches the CPU through the standard QEMU
 * GIC -> CPU IRQ wiring.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qapi/qapi-commands-avatar-target.h"
#include "qapi/error.h"
#include "hw/irq.h"
#include "hw/sysbus.h"
#include "hw/qdev-core.h"
#include "qom/object.h"
#include "sysemu/sysemu.h"

void qmp_avatar_arm_inject_irq(int64_t num_cpu, int64_t num_irq,
                               Error **errp)
{
    qemu_log_mask(LOG_AVATAR,
                  "Injecting ARM IRQ %ld on cpu %ld\n",
                  num_irq, num_cpu);
    /* The configurable machine names the arm_gic peripheral
     * "gic" (per the halucinator YAML convention). Look it up
     * by name on the machine and assert GPIO `num_irq - 32` —
     * the GIC's input GPIOs cover SPI/PPI starting at 0. */
    Object *machine = qdev_get_machine();
    Object *gic = object_resolve_path_component(machine, "gic");
    if (gic == NULL) {
        error_setg(errp,
            "avatar-arm-inject-irq: no peripheral named 'gic' on the "
            "configurable machine; declare one with qemu_name: arm_gic");
        return;
    }
    DeviceState *gicdev = (DeviceState *)object_dynamic_cast(gic,
                                                              TYPE_DEVICE);
    if (gicdev == NULL) {
        error_setg(errp,
            "avatar-arm-inject-irq: peripheral 'gic' is not a "
            "DeviceState");
        return;
    }
    /* GIC's qdev GPIOs cover SGI 0..15, PPI 16..31, SPI 32..N.
     * The user passes the IRQ ID directly (matching ISER bit
     * positions for SPI), so subtract the 32-PPI offset to get
     * the GPIO index. */
    int64_t gpio = num_irq;
    if (gpio < 0 || gpio > 1019) {
        error_setg(errp,
            "avatar-arm-inject-irq: num-irq must be in 0..1019");
        return;
    }
    qemu_irq line = qdev_get_gpio_in(gicdev, gpio);
    if (line == NULL) {
        error_setg(errp,
            "avatar-arm-inject-irq: GIC has no GPIO input %ld",
            gpio);
        return;
    }
    qemu_irq_pulse(line);
}
