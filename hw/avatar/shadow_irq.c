/*
 * Shadow-write IRQ delivery for halucinator's configurable machine.
 *
 * Writes the post-ack IRQ state (irq_number, irq_fired=1) directly
 * into the firmware's RAM globals via cpu_physical_memory_write,
 * on the iothread, under BQL. Bypasses CPU exception machinery
 * entirely.
 *
 * Why a QMP command instead of a GDB M-packet write: halucinator's
 * QEMUBackend dispatch loop blocks in wait_for_stop on the same
 * GDB socket the peripheral_server thread would use to inject. The
 * Ctrl-C / stop-reply / write-memory / continue handshake races
 * between the two threads — the dispatch thread tends to consume
 * the stop reply, the inject thread times out, and the write
 * either lands while the CPU is still running (rejected) or never
 * happens. Routing through QMP avoids the conflict entirely.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qapi/qapi-commands-avatar-target.h"
#include "qapi/error.h"
#include "exec/cpu-common.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"

void qmp_avatar_shadow_irq(int64_t number_addr, int64_t fired_addr,
                           int64_t irq_num, Error **errp)
{
    /*
     * Use the address-space helpers so the byte order matches
     * what the guest CPU expects. address_space_stl_be / _le
     * pack the 32-bit word in target endianness; choosing the
     * right one means the firmware reading the word back gets
     * the value it expects regardless of host endianness.
     *
     * Pick by the configurable_machine's compile-time target
     * endianness: BE machines are MIPS BE and PowerPC; LE are
     * ARM / x86 / RISC-V / etc.
     */
#if defined(TARGET_WORDS_BIGENDIAN)
    address_space_stl_be(&address_space_memory, (hwaddr)number_addr,
                         (uint32_t)irq_num,
                         MEMTXATTRS_UNSPECIFIED, NULL);
    address_space_stl_be(&address_space_memory, (hwaddr)fired_addr,
                         1, MEMTXATTRS_UNSPECIFIED, NULL);
#else
    address_space_stl_le(&address_space_memory, (hwaddr)number_addr,
                         (uint32_t)irq_num,
                         MEMTXATTRS_UNSPECIFIED, NULL);
    address_space_stl_le(&address_space_memory, (hwaddr)fired_addr,
                         1, MEMTXATTRS_UNSPECIFIED, NULL);
#endif
    qemu_log_mask(LOG_AVATAR,
                  "Shadow-IRQ: num=%ld @0x%lx fired=1 @0x%lx\n",
                  irq_num, number_addr, fired_addr);
}
