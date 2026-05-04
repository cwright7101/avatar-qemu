/*
 * PowerPC IRQ injection for halucinator's configurable machine.
 *
 * Exposes a `avatar-ppc-inject-irq` QMP command that asserts the
 * PowerPC CPU's IRQ input *num-irq* (an index into
 * env->irq_inputs[]) and immediately deasserts it, so the CPU
 * sees a single edge. The IRQ inputs are allocated by the CPU's
 * model-specific init (ppce500_irq_init etc.) and not directly
 * reachable through qdev_get_gpio_in.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qapi/qapi-commands-avatar-target.h"
#include "qapi/error.h"
#include "hw/irq.h"
#include "sysemu/sysemu.h"
#include "target/ppc/cpu.h"
#include "hw/ppc/ppc.h"

void qmp_avatar_ppc_inject_irq(int64_t num_cpu, int64_t num_irq,
                               Error **errp)
{
    qemu_log_mask(LOG_AVATAR,
                  "Injecting PPC IRQ %ld on cpu %ld\n",
                  num_irq, num_cpu);
    CPUState *cs = qemu_get_cpu(num_cpu);
    if (cs == NULL) {
        error_setg(errp, "avatar-ppc-inject-irq: cpu %ld not found",
                   num_cpu);
        return;
    }
    PowerPCCPU *cpu = POWERPC_CPU(cs);
    qemu_irq *inputs = (qemu_irq *)cpu->env.irq_inputs;
    if (inputs == NULL) {
        error_setg(errp,
            "avatar-ppc-inject-irq: CPU model doesn't initialise "
            "env->irq_inputs[]");
        return;
    }
    if (num_irq < 0 || num_irq > 31) {
        error_setg(errp,
            "avatar-ppc-inject-irq: num-irq out of range (got %ld)",
            num_irq);
        return;
    }
    qemu_irq_pulse(inputs[num_irq]);
}
