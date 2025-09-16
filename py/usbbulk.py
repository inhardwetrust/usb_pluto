
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
N = 8
timeout_ms = 2000
while True:
    data=None
    data = ep_in.read(N, timeout=timeout_ms)
    data = data.tobytes() if hasattr(data, "tobytes") else bytes(data)

    print(f"Read {len(data)} bytes:")
    for b in data:
        print(b)   # десятичное значение байта
    #time.sleep(0.5)