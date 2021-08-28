#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <numeric>
#include <vector>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "mouse.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "logger.hpp"
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"
#include "interrupt.hpp"
#include "asmfunc.h"
#include "queue.hpp"
#include "memory_map.hpp"
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "window.hpp"
#include "layer.hpp"
#include "timer.hpp"

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

char console_buf[sizeof(Console)];
Console* console;

int printk(const char *format, ...) {
  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  // Kernel 内で vsprintf, sprintf をするためのロジックは、day05d で実装
  result = vsprintf(s, format, ap);
  va_end(ap);

  console->PutString(s);

  return result;
}

char memory_manager_buf[sizeof(BitmapMemoryManager)];
BitmapMemoryManager* memory_manager;

unsigned int mouse_layer_id;

void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
  layer_manager->MoveRelative(mouse_layer_id, {displacement_x, displacement_y});
  StartLAPICTimer();
  layer_manager->Draw();
  auto elapsed = LAPICTimerElapsed();
  StopLAPICTimer();
  printk("MouseObserver: elapsed = %u\n", elapsed);
}

void SwitchEhci2Xhci(const pci::Device& xhc_dev) {
  bool intel_ehc_exist = false;
  for (int i = 0; i < pci::num_device; ++i) {
    if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x20u) /* EHCI */ &&
        0x8086 == pci::ReadVendorId(pci::devices[i])) {
      intel_ehc_exist = true;
      break;
    }
  }
  if (!intel_ehc_exist) {
    return;
  }

  uint32_t superspeed_ports = pci::ReadConfReg(xhc_dev, 0xdc); // USB3PRM
  pci::WriteConfReg(xhc_dev, 0xd8, superspeed_ports); // USB3_PSSEN
  uint32_t ehci2xhci_ports = pci::ReadConfReg(xhc_dev, 0xd4); // XUSB2PRM
  pci::WriteConfReg(xhc_dev, 0xd0, ehci2xhci_ports); // XUSB2PR
  Log(kDebug, "SwitchEhci2Xhci: SS = %02, xHCI = %02x\n",
      superspeed_ports, ehci2xhci_ports);
}

// 割り込みハンドラの実装
usb::xhci::Controller* xhc;

struct Message {
  enum Type {
    kInterruptXHCI,
  } type;
};

ArrayQueue<Message>* main_queue;

__attribute__((interrupt))
void IntHandlerXHCI(InterruptFrame* frame) {
  main_queue->Push(Message{Message::kInterruptXHCI});
  // ここでこの関数を呼び出す意味をあまり分かっていない。
  NotifyEndOfInterrupt();
}

// スタックの移行先
alignas(16) uint8_t kernel_main_stack[1024 * 1024];

extern "C" void KernelMainNewStack(
    const FrameBufferConfig& frame_buffer_config_ref,
    const MemoryMap& memory_map_ref) {
  FrameBufferConfig frame_buffer_config{frame_buffer_config_ref};
  MemoryMap memory_map{memory_map_ref};

  switch (frame_buffer_config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf)RGBResv8BitPerColorPixelWriter{frame_buffer_config};
      break;
    case kPixelBGRResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf)BGRResv8BitPerColorPixelWriter{frame_buffer_config};
      break;
  }

  DrawDesktop(*pixel_writer);

  console = new(console_buf)Console{kDesktopFGColor, kDesktopBGColor};
  console->SetWriter(pixel_writer);
  printk("Welcome to HonOS!!!\n");
  SetLogLevel(kWarn);

  InitializeLAPICTimer();

  // UEFI から OS にメモリの情報を持っていき、メモリのセグメンテーションの設定
  SetupSegments();

  const uint16_t kernel_cs = 1 << 3;
  const uint16_t kernel_ss = 2 << 3;
  SetDSAll(0);
  SetCSSS(kernel_cs, kernel_ss);

  // ページングのセットアップ
  SetupIdentityPageTable();

  // メモリマネージャの作成
  ::memory_manager = new(memory_manager_buf) BitmapMemoryManager;

  const auto memory_map_base = reinterpret_cast<uintptr_t>(memory_map.buffer);
  uintptr_t available_end = 0;
  // UEFI から受け取ったメモリマップが使用中か未使用かを判定する。
  // 初期時点では、メモリマネージャはメモリ領域全体が未使用だと思っているので、使用領域を伝えるロジックを作成する。
  for (uintptr_t iter = memory_map_base;
       iter < memory_map_base + memory_map.map_size;
       iter += memory_map.descriptor_size) {
    auto desc = reinterpret_cast<const MemoryDescriptor*>(iter);
    if (available_end < desc->physical_start) {
      memory_manager->MarkAllocated(
        FrameID{available_end / kBytesPerFrame},
        (desc->physical_start - available_end) / kBytesPerFrame);
    }

    const auto physical_end =
      desc->physical_start + desc->number_of_pages * kUEFIPageSize;
    if (IsAvailable(static_cast<MemoryType>(desc->type))) {
      // 未使用の領域
      available_end = physical_end;
    } else {
      // メモリマネージャに伝える
      memory_manager->MarkAllocated(
        FrameID{desc->physical_start / kBytesPerFrame},
        desc->number_of_pages * kUEFIPageSize / kBytesPerFrame);
    }
  }
  memory_manager->SetMemoryRange(FrameID{1}, FrameID{available_end / kBytesPerFrame});

  // ヒープ領域の確保
  if (auto err = InitializeHeap(*memory_manager)) {
    Log(kError, "failed to allocate pages: %s at %s:%d\n",
        err.Name(), err.File(), err.Line());
    exit(1);
  }

  printk("memory_map: %p\n", &memory_map);
  for (uintptr_t iter = reinterpret_cast<uintptr_t>(memory_map.buffer);
       iter < reinterpret_cast<uintptr_t>(memory_map.buffer) + memory_map.map_size;
       iter += memory_map.descriptor_size) {
    auto desc = reinterpret_cast<MemoryDescriptor*>(iter);
    if (IsAvailable(static_cast<MemoryType>(desc->type))) {
      printk("type = %u, phys = %08lx - %08lx, pages = %lu, attr = %08lx\n",
          desc->type,
          desc->physical_start,
          desc->physical_start + desc->number_of_pages * 4096 - 1,
          desc->number_of_pages,
          desc->attribute);
    }
  }

  std::array<Message, 32> main_queue_data;
  ArrayQueue<Message> main_queue{main_queue_data};
  ::main_queue = &main_queue;

  // PCI デバイスを列挙する
  auto err = pci::ScanAllBus();
  printk("ScanAllBus: %s\n", err.Name());
  Log(kWarn, "unsigned long %ld, int %d, char %d, size_t %d\n",
    sizeof(unsigned long), sizeof(int), sizeof(char), sizeof(size_t));

  for (int i = 0; i < pci::num_device; ++i) {
    const auto& dev = pci::devices[i];
    auto vendor_id = pci::ReadVendorId(dev);
    auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
    printk("%d.%d.%d: vend %04x, class %08x, head %02x\n",
        dev.bus, dev.device, dev.function,
        vendor_id, class_code, dev.header_type);
  }

  pci::Device* xhc_dev = nullptr;
  for (int i = 0; i < pci::num_device; ++i) {
    // クラスコードが 0x0c, 0x03, 0x30 のものを探索する。
    if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x30u)) {
      xhc_dev = &pci::devices[i];

      if (0x8086 == pci::ReadVendorId(*xhc_dev)) {
        break;
      }
    }
  }

  if (xhc_dev) {
    Log(kInfo, "xHC has been found: %d.%d.%d\n",
        xhc_dev->bus, xhc_dev->device, xhc_dev->function);
  }

  SetIDTEntry(idt[InterruptVector::kXHCI], MakeIDTAttr(DescriptorType::kInterruptGate, 0),
              reinterpret_cast<uint64_t>(IntHandlerXHCI), kernel_cs);
  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));

  // 0xfee00020 番地のビット 31:24 を読み取る。
  // その結果、このプログラムが動作しているコアの Local APIC ID を取得できる。
  const uint8_t bsp_local_apic_id =
    *reinterpret_cast<const uint32_t*>(0xfee00020) >> 24;
  pci::ConfigureMSIFixedDestination(
    *xhc_dev, bsp_local_apic_id,
    pci::MSITriggerMode::kLevel, pci::MSIDeliveryMode::kFixed,
    InterruptVector::kXHCI, 0
  );

  const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
  Log(kDebug, "ReadBar: %s\n", xhc_bar.error.Name());
  const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);
  Log(kDebug, "xHC mmio_base = %08lx\n", xhc_mmio_base);

  usb::xhci::Controller xhc{xhc_mmio_base};

  if (0x8086 == pci::ReadVendorId(*xhc_dev)) {
    SwitchEhci2Xhci(*xhc_dev);
  }
  {
    auto err = xhc.Initialize();
    Log(kDebug, "xhc.Initialize: %s\n", err.Name());
  }

  Log(kInfo, "xHC starting\n");
  xhc.Run();

  ::xhc = &xhc;

  usb::HIDMouseDriver::default_observer = MouseObserver;

  for (int i = 1; i <= xhc.MaxPorts(); ++i) {
    auto port = xhc.PortAt(i);
    Log(kDebug, "Port %d: IsConnected=%d\n", i, port.IsConnected());

    if (port.IsConnected()) {
      if (auto err = ConfigurePort(xhc, port)) {
        Log(kError, "failed to configure port: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
        continue;
      }
    }
  }

  // レイヤの初期化処理
  const int kFrameWidth = frame_buffer_config.horizontal_resolution;
  const int kFrameHeight = frame_buffer_config.vertical_resolution;

  // 背景の window を初期化
  auto bgwindow = std::make_shared<Window>(kFrameWidth, kFrameHeight);
  auto bgwriter = bgwindow->Writer();

  DrawDesktop(*bgwriter);
  console->SetWriter(bgwriter);

  auto mouse_window = std::make_shared<Window>(
    kMouseCursorWidth, kMouseCursorHeight);
  mouse_window->SetTransparentColor(kMouseTransparentColor);
  DrawMouseCursor(mouse_window->Writer(), {0, 0});

  // layer manager の初期化
  layer_manager = new LayerManager;
  layer_manager->SetWriter(pixel_writer);

  auto bglayer_id = layer_manager->NewLayer()
    .SetWindow(bgwindow)
    .Move({0, 0})
    .ID();
  // {300, 200} でマウスの初期位置を決めてる
  mouse_layer_id = layer_manager->NewLayer()
    .SetWindow(mouse_window)
    .Move({200, 200})
    .ID();

  layer_manager->UpDown(bglayer_id, 0);
  layer_manager->UpDown(mouse_layer_id, 1);
  layer_manager->Draw();

  while (true) {
    __asm__("cli");
    if (main_queue.Count() == 0) {
      __asm__("sti\n\thlt");
      continue;
    }

    Message msg = main_queue.Front();
    main_queue.Pop();
    __asm__("sti");

    switch (msg.type) {
      case Message::kInterruptXHCI:
        while (xhc.PrimaryEventRing()->HasFront()) {
          if (auto err = ProcessEvent(xhc)) {
              Log(kError, "Error while ProcessEvent: %s at %s:%d\n",
                  err.Name(), err.File(), err.Line());
          }
        }
        break;
      default:
        Log(kError, "Unknown message type: %d\n", msg.type);
    }
  }

}

extern "C" void __cxa_pure_virtual(){
  while (1) __asm__("hlt");
}
