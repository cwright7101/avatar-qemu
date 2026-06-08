/*
 * PowerPC IRQ injection for halucinator's configurable machine.
 *
 * Exposes a `avatar-ppc-inject-irq` QMP command that asserts the
 * PowerPC CPU's IRQ input *num-irq* (an index into
 * env->irq_inputs[]) and schedules the deassert on the next
 * iothread main-loop iteration so the vCPU thread has a chance
 * to take the External Interrupt exception while the input is
 * high. A naive qemu_irq_pulse(line) under BQL races against
 * MTTCG: both edges land while the vCPU is blocked on the lock
 * and the CPU never observes the high level. Deferring the
 * deassert via a bottom-half releases BQL between the assert
 * and the deassert.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qemu/main-loop.h"
#include "qapi/qapi-commands-avatar-target.h"
#include "qapi/error.h"
#include "hw/irq.h"
#include "sysemu/sysemu.h"
#include "target/ppc/cpu.h"
#include "hw/ppc/ppc.h"

typedef struct {
    qemu_irq line;
    QEMUBH *bh;
} PpcDeassertCtx;

static void ppc_deassert_bh(void *opaque)
{
    PpcDeassertCtx *ctx = opaque;
    qemu_set_irq(ctx->line, 0);
    qemu_bh_delete(ctx->bh);
    g_free(ctx);
}

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
    qemu_set_irq(inputs[num_irq], 1);
    PpcDeassertCtx *ctx = g_new0(PpcDeassertCtx, 1);
    ctx->line = inputs[num_irq];
    ctx->bh = qemu_bh_new(ppc_deassert_bh, ctx);
    qemu_bh_schedule(ctx->bh);
}
