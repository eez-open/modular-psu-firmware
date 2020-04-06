import ustruct
import utime

print(ustruct.pack('hhl', 1, 2, 3))
print(utime.ticks_ms())
print("sleep begin")
utime.sleep_ms(1000)
print("sleep end")
print(utime.ticks_ms())

# ",".join(map(lambda x: '"' + x + '"', MODULE_NAMES[1:]))
print(",".join(["1", "2", "3"]))


def fn(x):
    return '"' + str(x) + '"'

print(",".join(map(lambda x: '"' + str(x) + '"', [1, 2, 3])))

print(hex(11))