# Control-comidas-mayita

Proyecto para Arduino Nano para llevar control de las comidas de Maya usando:

- una pantalla OLED SSD1306 de 128x64
- un RTC DS3231
- un encoder rotatorio con boton
- 2 LEDs de estado

El sketch muestra la hora actual, avisa cuando ya es hora de comer, guarda el estado de las comidas en EEPROM e incluye un menu manejado con encoder para ajustar reloj, desayuno y cena.

## Hardware

### Componentes principales

- Arduino Nano
- Pantalla OLED SSD1306 128x64 I2C
- Modulo RTC DS3231
- Encoder rotatorio con boton
- 1 LED rojo
- 1 LED azul
- 2 resistencias para los LEDs

### Tabla de pines

| Funcion | Pin en Arduino | Notas |
| --- | --- | --- |
| Encoder `CLK` | `D2` | Canal A del encoder |
| Encoder `DT` | `D3` | Canal B del encoder |
| Encoder `SW` | `D4` | Boton del encoder, usa `INPUT_PULLUP` |
| LED rojo | `D5` | Colocar resistencia en serie |
| LED azul | `D6` | Colocar resistencia en serie |
| I2C SDA | `A4` | Compartido entre OLED y DS3231 |
| I2C SCL | `A5` | Compartido entre OLED y DS3231 |
| 5V | `5V` | Alimentacion de los modulos |
| GND | `GND` | Tierra comun para todo |

### Dispositivos I2C

La pantalla OLED y el DS3231 comparten el bus I2C:

- OLED SDA -> `A4`
- OLED SCL -> `A5`
- DS3231 SDA -> `A4`
- DS3231 SCL -> `A5`

La direccion I2C usada por el sketch para la OLED es `0x3C`.

## Estructura del proyecto

```text
Control-comidas-mayita/
|- .vscode/
|  |- arduino.json
|  `- settings.json
|- Control_comida_Maya/
|  `- Control_comida_Maya.ino
`- README.md
```

## Funciones principales

- Pantalla principal con reloj y horarios de desayuno y cena
- Pantalla de aviso cuando ya toca comer
- LED rojo de alerta que parpadea mas rapido conforme pasa el tiempo
- LED azul de confirmacion cuando ya se marco la comida
- Almacenamiento en EEPROM de:
  - ultima comida registrada
  - slot de comida registrado
  - hora de desayuno
  - hora de cena
- Menu de configuracion al llenar la barra con el encoder
- Cierre automatico del menu tras 30 segundos de inactividad

## Librerias de Arduino

Instala estas librerias antes de compilar:

- `RTClib`
- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `Adafruit BusIO`

El sketch tambien usa librerias incluidas con Arduino:

- `Wire`
- `EEPROM`

## Configuracion de la placa

Este proyecto esta configurado actualmente para:

- Placa: `Arduino Nano`
- Core: `arduino:avr`
- Procesador: `ATmega328P (Old Bootloader)`
- Puerto: `COM6` en este workspace local

La configuracion del workspace esta en:

- [.vscode/arduino.json](</c:/Users/alber/Documents/2026/proyecto Maya/proyectogit-comidamaya/Control-comidas-mayita/.vscode/arduino.json>)
- [.vscode/settings.json](</c:/Users/alber/Documents/2026/proyecto Maya/proyectogit-comidamaya/Control-comidas-mayita/.vscode/settings.json>)

## Compilar y subir desde VS Code

### Requisitos

- VS Code
- Extension Arduino Community Edition: `vscode-arduino.vscode-arduino-community`
- Arduino CLI instalado en Windows
- Core `Arduino AVR Boards` instalado
- Librerias del proyecto instaladas

### Nota importante

Este workspace esta configurado para usar `arduino-cli`, no el ejecutable viejo del Arduino IDE. Si `Verify` o `Upload` falla en VS Code, revisa que existan estas opciones:

```json
{
  "arduino.useArduinoCli": true,
  "arduino.path": "C:\\Program Files\\Arduino CLI",
  "arduino.commandPath": "arduino-cli.exe"
}
```

### Verificar desde VS Code

1. Abre esta carpeta en VS Code.
2. Confirma que la placa sea `arduino:avr:nano`.
3. Confirma que el procesador sea `cpu=atmega328old`.
4. Confirma que el puerto serial sea el que esta usando tu Nano.
5. Ejecuta `Arduino: Verify`.

### Subir desde VS Code

1. Conecta el Nano por USB.
2. Selecciona el puerto serial correcto.
3. Ejecuta `Arduino: Upload`.

Si tu placa no esta en `COM6`, primero actualiza `.vscode/arduino.json`.

## Compilar y subir con arduino-cli

### Verificar la herramienta

```powershell
arduino-cli version
```

### Instalar el core AVR

```powershell
arduino-cli core update-index
arduino-cli core install arduino:avr
```

### Instalar librerias

```powershell
arduino-cli lib install "RTClib"
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "Adafruit BusIO"
```

### Compilar

```powershell
arduino-cli compile --build-path build --fqbn arduino:avr:nano:cpu=atmega328old "Control_comida_Maya"
```

### Subir

```powershell
arduino-cli upload -p COM6 --fqbn arduino:avr:nano:cpu=atmega328old "Control_comida_Maya"
```

Si tu placa usa otro puerto serial, sustituye `COM6`.

### Listar puertos disponibles

```powershell
arduino-cli board list
```

## Comportamiento del sketch

### Modo normal

- La OLED muestra la hora actual.
- En la parte inferior se muestran los horarios de desayuno (`D`) y cena (`C`).

### Aviso de comida

- Cuando ya toca comer, la pantalla muestra `Hora de comer!`
- El LED rojo se enciende.
- A medida que pasa el tiempo, el parpadeo se acelera.

### Comida confirmada

- Presiona el boton del encoder para marcar la comida como hecha.
- El LED azul se enciende.
- La pantalla muestra cuanto tiempo ha pasado desde que Maya comio.

### Acceso al menu

- Gira el encoder en sentido horario para llenar la barra de acceso.
- Gira en sentido antihorario para vaciarla.
- Cuando la barra llega al maximo, se abre el menu.
- Si no hay actividad durante 30 segundos, la barra se vacia sola.

### Opciones del menu

- `Reloj`
- `Desayuno`
- `Cena`
- `Exit`

Cada pantalla de ajuste de hora funciona asi:

- Girar para cambiar el valor
- Clic para confirmar horas
- Girar para cambiar minutos
- Clic otra vez para guardar

## Solucion de problemas

### En VS Code falla `Verify` o `Upload` con `arduino_debug.exe`

Eso significa que la extension esta intentando usar el ejecutable viejo del Arduino IDE. La solucion es activar Arduino CLI en `.vscode/settings.json`.

### La subida falla

Revisa:

- puerto COM correcto
- que el Nano use la opcion de old bootloader
- que el cable USB soporte datos y no solo carga
- que no haya otro monitor serial ocupando el puerto

### El encoder todavia salta a veces

El sketch ya usa un decodificador mas robusto para el encoder, pero los encoders mecanicos baratos pueden seguir metiendo ruido. Si sigue inestable:

- revisa el cableado de `CLK`, `DT` y `GND`
- usa cables cortos
- asegurate de compartir tierra entre todos los modulos
- verifica que el encoder no este flojo en la protoboard

## Archivos importantes

- Sketch principal: [Control_comida_Maya/Control_comida_Maya.ino](</c:/Users/alber/Documents/2026/proyecto Maya/proyectogit-comidamaya/Control-comidas-mayita/Control_comida_Maya/Control_comida_Maya.ino>)
- Configuracion Arduino de VS Code: [.vscode/arduino.json](</c:/Users/alber/Documents/2026/proyecto Maya/proyectogit-comidamaya/Control-comidas-mayita/.vscode/arduino.json>)
- Ajustes Arduino de VS Code: [.vscode/settings.json](</c:/Users/alber/Documents/2026/proyecto Maya/proyectogit-comidamaya/Control-comidas-mayita/.vscode/settings.json>)
