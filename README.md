# SetupKeys

Утилита SetupKeys предназначена для настройки клавиш STM8S-VUSB-KEYBOARD.

Среда разработки и компилятор - Microsoft Visual Studio 2015 + Windows Driver Kit 10

Использование:

SetupKeys.exe read/r/write/w key_num usage_page modifiers scancode

Чтение настроек: SetupKeys.exe read key_num

Пример:

: SetupKeys.exe read 2

Device found: VID: 0x043B  PID: 0x0325  UsagePage: 0x1  Usage: 0x80

Reading settings of key 2... OK!

Key_num: 2

Usage page: 0x7

Modifiers: 0x0

Scancode: 0x1E

Запись настроек: SetupKeys.exe write key_num usage_page modifiers scancode

Пример:

: SetupKeys.exe write 3 7 0 0x1f

Device found: VID: 0x043B  PID: 0x0325  UsagePage: 0x1  Usage: 0x80

Key_num: 3

Usage page: 0x7

Modifiers: 0x0

Scancode: 0x1F

Writing settings of key 3... OK!

