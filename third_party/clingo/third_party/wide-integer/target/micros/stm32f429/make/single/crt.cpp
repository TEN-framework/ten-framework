#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>

namespace crt
{
  void init_ram();
}

namespace crt
{
  void init_ctors();
}

extern "C" void __my_startup(void) __attribute__((used, noinline));

void __my_startup(void)
{
  // Load the stack pointer.
  // The stack pointer is automatically loaded from
  // the base position of the interrupt vector table.
  // So we do nothing here.

  // TBD: Chip init: Watchdog, port, and oscillator, if any needed.

  // Initialize statics from ROM to RAM.
  // Zero-clear default-initialized static RAM.
  crt::init_ram();

  // Call all ctor initializations.
  crt::init_ctors();

  // Jump to main (and never return).
  asm volatile("ldr r3, =main");
  asm volatile("blx r3");

  exit(EXIT_SUCCESS);

  // TBD: Nothing on return from main.
}

extern "C" void _exit (int);

extern "C" void _exit (int) { }

extern "C"
{
  extern std::uintptr_t _rom_data_begin; // Start address for the initialization values of the rom-to-ram section.
  extern std::uintptr_t _data_begin;     // Start address for the .data section.
  extern std::uintptr_t _data_end;       // End address for the .data section.
  extern std::uintptr_t _bss_begin;      // Start address for the .bss section.
  extern std::uintptr_t _bss_end;        // End address for the .bss section.
}

void crt::init_ram()
{
  typedef std::uint32_t memory_aligned_type;

  // Copy the data segment initializers from ROM to RAM.
  // Note that all data segments are aligned by 4.
  const std::size_t size_data =
    std::size_t(  static_cast<const memory_aligned_type*>(static_cast<const void*>(&_data_end))
                - static_cast<const memory_aligned_type*>(static_cast<const void*>(&_data_begin)));

  std::copy(static_cast<const memory_aligned_type*>(static_cast<const void*>(&_rom_data_begin)),
            static_cast<const memory_aligned_type*>(static_cast<const void*>(&_rom_data_begin)) + size_data,
            static_cast<      memory_aligned_type*>(static_cast<      void*>(&_data_begin)));

  // Clear the bss segment.
  // Note that the bss segment is aligned by 4.
  std::fill(static_cast<memory_aligned_type*>(static_cast<void*>(&_bss_begin)),
            static_cast<memory_aligned_type*>(static_cast<void*>(&_bss_end)),
            static_cast<memory_aligned_type>(0U));
}

extern "C"
{
  struct ctor_type
  {
    typedef void(*function_type)();
    typedef std::reverse_iterator<const function_type*> const_reverse_iterator;
  };

  extern ctor_type::function_type _ctors_end[];
  extern ctor_type::function_type _ctors_begin[];
}

void crt::init_ctors()
{
  std::for_each(ctor_type::const_reverse_iterator(_ctors_end),
                ctor_type::const_reverse_iterator(_ctors_begin),
                [](const ctor_type::function_type pf)
                {
                  pf();
                });
}

extern "C" void __initial_stack_pointer();

extern "C" void __my_startup         () __attribute__((used, noinline));
extern "C" void __vector_unused_irq  () __attribute__((used, noinline));
extern "C" void __nmi_handler        () __attribute__((used, noinline));
extern "C" void __hard_fault_handler () __attribute__((used, noinline));
extern "C" void __mem_manage_handler () __attribute__((used, noinline));
extern "C" void __bus_fault_handler  () __attribute__((used, noinline));
extern "C" void __usage_fault_handler() __attribute__((used, noinline));
extern "C" void __svc_handler        () __attribute__((used, noinline));
extern "C" void __debug_mon_handler  () __attribute__((used, noinline));
extern "C" void __pend_sv_handler    () __attribute__((used, noinline));
extern "C" void __sys_tick_handler   () __attribute__((used, noinline));
extern "C" void __vector_timer4      ();

extern "C" void __vector_unused_irq  () { for(;;) { ; } }
extern "C" void __nmi_handler        () { for(;;) { ; } }
extern "C" void __hard_fault_handler () { for(;;) { ; } }
extern "C" void __mem_manage_handler () { for(;;) { ; } }
extern "C" void __bus_fault_handler  () { for(;;) { ; } }
extern "C" void __usage_fault_handler() { for(;;) { ; } }
extern "C" void __svc_handler        () { for(;;) { ; } }
extern "C" void __debug_mon_handler  () { for(;;) { ; } }
extern "C" void __pend_sv_handler    () { for(;;) { ; } }
extern "C" void __sys_tick_handler   () { for(;;) { ; } }

namespace
{
  typedef void(*isr_type)();

  constexpr std::size_t number_of_interrupts = 128U;
}

extern "C"
const volatile std::array<isr_type, number_of_interrupts> __isr_vector __attribute__((section(".isr_vector")));

extern "C"
const volatile std::array<isr_type, number_of_interrupts> __isr_vector =
{{
  __initial_stack_pointer,   // 0x0000, initial stack pointer
  __my_startup,              // 0x0004, reset
  __nmi_handler,             // 0x0008, nmi exception
  __hard_fault_handler,      // 0x000C, hard fault exception
  __mem_manage_handler,      // 0x0010, memory management exception
  __bus_fault_handler,       // 0x0014, bus fault exception
  __usage_fault_handler,     // 0x0018, usage fault exception
  __vector_unused_irq,       // 0x001C, reserved
  __vector_unused_irq,       // 0x0020, reserved
  __vector_unused_irq,       // 0x0024, reserved
  __vector_unused_irq,       // 0x0028, reserved
  __svc_handler,             // 0x002C, svc handler
  __debug_mon_handler,       // 0x0030, debug monitor
  __vector_unused_irq,       // 0x0034, reserved
  __pend_sv_handler,         // 0x0038, pending svc,
  __sys_tick_handler,        // 0x003C, system tick handler,
  __vector_unused_irq,       // 0x0040, wwdg irq handler,
  __vector_unused_irq,       // 0x0044, pvd irq handler,
  __vector_unused_irq,       // 0x0048, tamper irq handler,
  __vector_unused_irq,       // 0x004C, rtc irq handler,
  __vector_unused_irq,       // 0x0050, flash irq handler,
  __vector_unused_irq,       // 0x0054, rcc irq handler,
  __vector_unused_irq,       // 0x0058, exti0 irq handler,
  __vector_unused_irq,       // 0x005C, exti1 irq handler,
  __vector_unused_irq,       // 0x0060, exti2 irq handler,
  __vector_unused_irq,       // 0x0064, exti3 irq handler,
  __vector_unused_irq,       // 0x0068, exti4 irq handler,
  __vector_unused_irq,       // 0x006C, dma_channel1 irq handler,
  __vector_unused_irq,       // 0x0070, dma_channel2 irq handler,
  __vector_unused_irq,       // 0x0074, dma_channel3 irq handler,
  __vector_unused_irq,       // 0x0078, dma_channel4 irq handler,
  __vector_unused_irq,       // 0x007C, dma_channel5 irq handler,
  __vector_unused_irq,       // 0x0080, dma_channel6 irq handler,
  __vector_unused_irq,       // 0x0084, dma_channel7 irq handler,
  __vector_unused_irq,       // 0x0088, adc irq handler,
  __vector_unused_irq,       // 0x008C, usb_hp_can_tx irq handler,
  __vector_unused_irq,       // 0x0090, usb_lp_can_rx0 irq handler,
  __vector_unused_irq,       // 0x0094, can_rx1 irq handler,
  __vector_unused_irq,       // 0x0098, can_sce irq handler,
  __vector_unused_irq,       // 0x009C, exti9_5 irq handler,
  __vector_unused_irq,       // 0x00A0, tim1_brk irq handler,
  __vector_unused_irq,       // 0x00A4, tim1_up irq handler,
  __vector_unused_irq,       // 0x00A8, tim1_trg_com irq handler,
  __vector_unused_irq,       // 0x00AC, tim1_cc irq handler,
  __vector_unused_irq,       // 0x00B0, tim2 irq handler,
  __vector_unused_irq,       // 0x00B4, tim3 irq handler,
  __vector_unused_irq,       // 0x00B8, tim4 irq handler,
  __vector_unused_irq,       // 0x00BC, i2c1_ev irq handler,
  __vector_unused_irq,       // 0x00C0, i2c1_er irq handler,
  __vector_unused_irq,       // 0x00C4, i2c2_ev irq handler,
  __vector_unused_irq,       // 0x00C8, i2c2_er irq handler,
  __vector_unused_irq,       // 0x00CC, spi1 irq handler,
  __vector_unused_irq,       // 0x00D0, spi2 irq handler,
  __vector_unused_irq,       // 0x00D4, usart1 irq handler,
  __vector_unused_irq,       // 0x00D8, usart2 irq handler,
  __vector_unused_irq,       // 0x00DC, usart3 irq handler,
  __vector_unused_irq,       // 0x00E0, exti15_10 irq handler,
  __vector_unused_irq,       // 0x00E4, rtcalarm irq handler,
  __vector_unused_irq,       // 0x00E8, usbwakeup irq handler,
  __vector_unused_irq,       // 0x00EC, tim8 break and tim12
  __vector_unused_irq,       // 0x00F0, tim8 update and tim13
  __vector_unused_irq,       // 0x00F4, tim8 trigger and commutation and tim14
  __vector_unused_irq,       // 0x00F8, tim8 capture compare
  __vector_unused_irq,       // 0x00FC, dma1 stream7
  __vector_unused_irq,       // 0x0100, fmc
  __vector_unused_irq,       // 0x0104, sdio
  __vector_unused_irq,       // 0x0108, tim5
  __vector_unused_irq,       // 0x010C, spi3
  __vector_unused_irq,       // 0x0110, uart4
  __vector_unused_irq,       // 0x0114, uart5
  __vector_unused_irq,       // 0x0118, tim6 and dac1&2 underrun errors
  __vector_unused_irq,       // 0x011C, tim7
  __vector_unused_irq,       // 0x0120, dma2 stream 0
  __vector_unused_irq,       // 0x0124, dma2 stream 1
  __vector_unused_irq,       // 0x0128, dma2 stream 2
  __vector_unused_irq,       // 0x012C, dma2 stream 3
  __vector_unused_irq,       // 0x0130, dma2 stream 4
  __vector_unused_irq,       // 0x0134, ethernet
  __vector_unused_irq,       // 0x0138, ethernet wakeup through exti line
  __vector_unused_irq,       // 0x013C, can2 tx
  __vector_unused_irq,       // 0x0140, can2 rx0
  __vector_unused_irq,       // 0x0144, can2 rx1
  __vector_unused_irq,       // 0x0148, can2 sce
  __vector_unused_irq,       // 0x014C, usb otg fs
  __vector_unused_irq,       // 0x0150, dma2 stream 5
  __vector_unused_irq,       // 0x0154, dma2 stream 6
  __vector_unused_irq,       // 0x0158, dma2 stream 7
  __vector_unused_irq,       // 0x015C, usart6
  __vector_unused_irq,       // 0x0160, i2c3 event
  __vector_unused_irq,       // 0x0164, i2c3 error
  __vector_unused_irq,       // 0x0168, usb otg hs end point 1 out
  __vector_unused_irq,       // 0x016C, usb otg hs end point 1 in
  __vector_unused_irq,       // 0x0170, usb otg hs wakeup through exti
  __vector_unused_irq,       // 0x0174, usb otg hs
  __vector_unused_irq,       // 0x0178, dcmi
  __vector_unused_irq,       // 0x017C, cryp crypto
  __vector_unused_irq,       // 0x0180, hash and rng
  __vector_unused_irq,       // 0x0184, fpu
  __vector_unused_irq,       // 0x0188, uart7
  __vector_unused_irq,       // 0x018C, uart8
  __vector_unused_irq,       // 0x0190, spi4
  __vector_unused_irq,       // 0x0194, spi5
  __vector_unused_irq,       // 0x0198, spi6
  __vector_unused_irq,       // 0x019C, sai1
  __vector_unused_irq,       // 0x01A0, reserved
  __vector_unused_irq,       // 0x01A4, reserved
  __vector_unused_irq,       // 0x01A8, dma2d
  nullptr,                   // 0x01AC, dummy
  nullptr,                   // 0x01B0, dummy
  nullptr,                   // 0x01B4, dummy
  nullptr,                   // 0x01B8, dummy
  nullptr,                   // 0x01BC, dummy
  nullptr,                   // 0x01C0, dummy
  nullptr,                   // 0x01C4, dummy
  nullptr,                   // 0x01C8, dummy
  nullptr,                   // 0x01CC, dummy
  nullptr,                   // 0x01D0, dummy
  nullptr,                   // 0x01D4, dummy
  nullptr,                   // 0x01D8, dummy
  nullptr,                   // 0x01DC, dummy
  nullptr,                   // 0x01E0, dummy
  nullptr,                   // 0x01E4, dummy
  nullptr,                   // 0x01E8, dummy
  nullptr,                   // 0x01EC, dummy
  nullptr,                   // 0x01F0, dummy
  nullptr,                   // 0x01F4, dummy
  nullptr,                   // 0x01F8, dummy
  nullptr                    // 0x01FC, dummy
}};

