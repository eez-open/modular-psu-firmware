import ustruct
import utime

print(ustruct.pack('hhl', 1, 2, 3))
print(utime.ticks_ms())
print("sleep begin")
utime.sleep_ms(10000)
print("sleep end")
print(utime.ticks_ms())
