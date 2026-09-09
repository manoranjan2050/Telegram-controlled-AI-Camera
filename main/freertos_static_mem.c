/*
 * Root cause (confirmed 2026-09-09 on real hardware, via a temporary
 * diagnostic print inside FreeRTOS's xTaskCreateStaticPinnedToCore): with
 * CONFIG_SPIRAM=y, the ESP32-P4's internal-SRAM (L2MEM) heap pool is
 * fragmented/short enough by the time FreeRTOS creates its IDLE0/IDLE1
 * kernel tasks that ESP-IDF's default vApplicationGetIdleTaskMemory()
 * (components/freertos/port_common.c, which calls
 * pvPortMalloc(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)) sometimes has that
 * request satisfied out of the small 8KB TCM region instead of ordinary
 * internal SRAM - see components/heap/port/esp32p4/memory_layout.c: TCM's
 * medium-priority capability match list also includes MALLOC_CAP_INTERNAL,
 * so a plain INTERNAL request can fall through to TCM once L2MEM's
 * high-priority pool briefly can't satisfy it. FreeRTOS then rejects a
 * TCM-backed stack/TCB as invalid inside xTaskCreateStaticPinnedToCore()
 * (xPortcheckValidStackMem() / xPortCheckValidTCBMem()), asserting and
 * rebooting before app_main() ever runs - the device boot-loops.
 *
 * Fix: provide the idle/timer task TCB+stack memory from static .bss arrays
 * instead of the heap - the linker always places .bss in ordinary internal
 * DRAM, never TCM, so this is immune to heap fragmentation or PSRAM's
 * memory-map changes. This exactly mirrors FreeRTOS's own alternative
 * "kernel provided" implementation (FreeRTOS-Kernel-SMP/tasks.c, gated by
 * configKERNEL_PROVIDED_STATIC_MEMORY, which this IDF version's
 * FreeRTOSConfig.h does not enable).
 *
 * Redefining vApplicationGetIdleTaskMemory() etc. directly in this component
 * does NOT reliably override ESP-IDF's own copy in port_common.c: both live
 * in separate static-library archives (freertos.a and main.a) linked inside
 * one `--start-group ... --end-group`, and the linker resolves the
 * *first* archive member it scans that satisfies the undefined reference -
 * which is always port_common.c.o inside freertos.a itself, since that
 * archive is scanned to resolve its own tasks.c.o before main.a is even
 * reached. Our version would simply never get pulled into the link.
 *
 * Instead this uses the linker's --wrap mechanism (enabled in
 * main/CMakeLists.txt) so every caller of these three functions is
 * unconditionally redirected to our __wrap_* implementations below,
 * regardless of archive scan order. No ESP-IDF/SDK files are modified.
 */

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if (configSUPPORT_STATIC_ALLOCATION == 1)

/*
 * NOTE: on this IDF version's (non-SMP-kernel) ESP32-P4 port,
 * prvCreateIdleTasks() (FreeRTOS-Kernel/tasks.c) calls
 * vApplicationGetIdleTaskMemory() once per core in a loop, with no core
 * index argument - the stock heap-based implementation gets away with this
 * because pvPortMalloc() hands back a fresh block on every call. A static
 * override must reproduce that "fresh buffer per call" behavior itself, or
 * every core after the first would silently alias core 0's idle task
 * memory. A simple call counter over a fixed-size array does this safely:
 * this function only ever runs synchronously on core 0 during early boot,
 * called exactly configNUMBER_OF_CORES times total.
 */
static StaticTask_t s_idle_tcb[configNUMBER_OF_CORES];
static StackType_t s_idle_stack[configNUMBER_OF_CORES][configMINIMAL_STACK_SIZE];
static unsigned s_idle_call_count;

void __wrap_vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                           StackType_t **ppxIdleTaskStackBuffer,
                                           uint32_t *pulIdleTaskStackSize)
{
    unsigned idx = s_idle_call_count < configNUMBER_OF_CORES ? s_idle_call_count : 0;
    s_idle_call_count++;
    *ppxIdleTaskTCBBuffer = &s_idle_tcb[idx];
    *ppxIdleTaskStackBuffer = s_idle_stack[idx];
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

#if (CONFIG_FREERTOS_SMP) && (configNUMBER_OF_CORES > 1)

static StaticTask_t s_idle_passive_tcb[configNUMBER_OF_CORES - 1];
static StackType_t s_idle_passive_stack[configNUMBER_OF_CORES - 1][configMINIMAL_STACK_SIZE];

void __wrap_vApplicationGetPassiveIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                                  StackType_t **ppxIdleTaskStackBuffer,
                                                  uint32_t *pulIdleTaskStackSize,
                                                  BaseType_t xPassiveIdleTaskIndex)
{
    *ppxIdleTaskTCBBuffer = &s_idle_passive_tcb[xPassiveIdleTaskIndex];
    *ppxIdleTaskStackBuffer = s_idle_passive_stack[xPassiveIdleTaskIndex];
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

#endif /* (CONFIG_FREERTOS_SMP) && (configNUMBER_OF_CORES > 1) */

#if configUSE_TIMERS

static StaticTask_t s_timer_tcb;
static StackType_t s_timer_stack[configTIMER_TASK_STACK_DEPTH];

void __wrap_vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                            StackType_t **ppxTimerTaskStackBuffer,
                                            uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &s_timer_tcb;
    *ppxTimerTaskStackBuffer = s_timer_stack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

#endif /* configUSE_TIMERS */

#endif /* configSUPPORT_STATIC_ALLOCATION == 1 */
