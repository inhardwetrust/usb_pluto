
import usb.core, usb.util
import time

VID, PID = 0x1234, 0x5679  # твои из дескриптора

dev = usb.core.find(idVendor=VID, idProduct=PID)
if dev is None:
    raise SystemExit("Device not found")



dev.set_configuration()  # конфигурация 1
cfg  = dev.get_active_configuration()
intf = cfg[(0, 0)]  # Interface #0, AltSetting 0

usb.util.claim_interface(dev, intf.bInterfaceNumber)

# Найдём bulk EP1 OUT и EP1 IN
ep_out = usb.util.find_descriptor(
    intf,
    custom_match=lambda e:
        usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT and
        usb.util.endpoint_type(e.bmAttributes) == usb.util.ENDPOINT_TYPE_BULK
)
ep_in = usb.util.find_descriptor(
    intf,
    custom_match=lambda e:
        usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN and
        usb.util.endpoint_type(e.bmAttributes) == usb.util.ENDPOINT_TYPE_BULK
)

print(hex(ep_out.bEndpointAddress), hex(ep_in.bEndpointAddress))  # ожидаем 0x01 и 0x81

# Отправим старт через EP0
dev.ctrl_transfer(
    0x40,         # bmRequestType: OUT | Vendor | Device (0b0100_0000)
    0x35,         # bRequest (REQ_STREAM_START)
    0,            # wValue
    0,            # wIndex
    None,         # нет data stage
    timeout=1000
)


# сколько байт читать
N = 1024
timeout_ms = 2000


# --- мои изменения ниже ---

NMEASURE = 2000  # количество пакетов в окне измерения
pkt_in_window = 0
bytes_in_window = 0
t0 = time.perf_counter()

while True:
    data = ep_in.read(N, timeout=timeout_ms)
    got = len(data)
    bytes_in_window += got
    pkt_in_window += 1

    if pkt_in_window >= NMEASURE:
        t1 = time.perf_counter()
        dt = t1 - t0
        if dt <= 0:
            dt = 1e-9

        bps = bytes_in_window / dt * 8.0
        mbps = bps / 1e6
        mBps = bps / 8e6
        pps = pkt_in_window / dt

        print(f"\r{mbps:7.3f} Mbit/s  ({mBps:6.3f} MB/s)  |  {pps:8.1f} pkt/s", end="")

        # сброс окна
        pkt_in_window = 0
        bytes_in_window = 0
        t0 = time.perf_counter()