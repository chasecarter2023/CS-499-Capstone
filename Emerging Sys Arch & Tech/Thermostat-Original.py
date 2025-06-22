from time import sleep
from datetime import datetime
from statemachine import StateMachine, State
import board, digitalio
import adafruit_ahtx0
import adafruit_character_lcd.character_lcd as characterlcd
import serial
from gpiozero import Button, PWMLED
from threading import Thread
from math import floor

DEBUG = True
if DEBUG:
    print("Thermostat.py loaded. DEBUG mode is", DEBUG)

# ── Sensor + Serial Setup ────────────────────────────────────────
i2c      = board.I2C()
thSensor = adafruit_ahtx0.AHTx0(i2c)

ser = serial.Serial(
    port='/dev/ttyS0',      # or '/dev/ttyAM0'
    baudrate=115200,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS,
    timeout=1
)

# ── LEDs ─────────────────────────────────────────────────────────
# Red LED on GPIO18 = heating, Blue LED on GPIO23 = cooling
heatLED = PWMLED(18)
coolLED = PWMLED(23)

# ── ManagedDisplay ───────────────────────────────────────────────
class ManagedDisplay:
    def __init__(self):
        # LCD wiring
        self.lcd_rs = digitalio.DigitalInOut(board.D17)
        self.lcd_en = digitalio.DigitalInOut(board.D27)
        self.lcd_d4 = digitalio.DigitalInOut(board.D5)
        self.lcd_d5 = digitalio.DigitalInOut(board.D6)
        self.lcd_d6 = digitalio.DigitalInOut(board.D13)
        self.lcd_d7 = digitalio.DigitalInOut(board.D26)

        self.lcd = characterlcd.Character_LCD_Mono(
            self.lcd_rs, self.lcd_en,
            self.lcd_d4, self.lcd_d5,
            self.lcd_d6, self.lcd_d7,
            16, 2
        )
        self.lcd.clear()

    def update_screen(self, line1, line2=""):
        self.lcd.clear()
        self.lcd.message = line1[:16] + "\n" + line2[:16]

    def cleanup(self):
        self.lcd.clear()
        for pin in (self.lcd_rs, self.lcd_en,
                    self.lcd_d4, self.lcd_d5,
                    self.lcd_d6, self.lcd_d7):
            pin.deinit()

# ── TemperatureMachine ───────────────────────────────────────────
class TemperatureMachine(StateMachine):
    off   = State(initial=True)
    heat  = State()
    cool  = State()
    cycle = off.to(heat) | heat.to(cool) | cool.to(off)

    def __init__(self, display: ManagedDisplay):
        super().__init__()
        self.display    = display
        self.setPoint   = 72           # default set point
        self.endDisplay = False

    # ─── State Callbacks ──────────────────────────────────────────
    def on_enter_heat(self):
        heatLED.off(); coolLED.off()
        if DEBUG: print("* STATE → HEAT")
        self._update_lights()

    def on_enter_cool(self):
        heatLED.off(); coolLED.off()
        if DEBUG: print("* STATE → COOL")
        self._update_lights()

    def on_enter_off(self):
        heatLED.off(); coolLED.off()
        if DEBUG: print("* STATE → OFF")

    # ─── Button Handlers ─────────────────────────────────────────
    def process_state_button(self):
        if DEBUG: print("Button1: cycle state")
        self.cycle()

    def process_temp_inc(self):
        self.setPoint += 1
        if DEBUG: print(f"Button2: setPoint → {self.setPoint}")
        self._update_lights()

    def process_temp_dec(self):
        self.setPoint -= 1
        if DEBUG: print(f"Button3: setPoint → {self.setPoint}")
        self._update_lights()

    # ─── LED Logic ────────────────────────────────────────────────
    def _update_lights(self):
        raw = self.get_fahrenheit()
        if raw is None:
            return
        temp = floor(raw)
        if self.current_state is self.heat:
            if temp < self.setPoint:
                heatLED.pulse(fade_in_time=1, fade_out_time=1)
            else:
                heatLED.on()
        elif self.current_state is self.cool:
            if temp > self.setPoint:
                coolLED.pulse(fade_in_time=1, fade_out_time=1)
            else:
                coolLED.on()

    def get_fahrenheit(self):
        try:
            c = thSensor.temperature
        except OSError as e:
            if DEBUG:
                print("Warning: sensor I²C error:", e)
            return None
        return (9/5) * c + 32

    def setup_serial_output(self):
        raw = self.get_fahrenheit()
        if raw is None:
            t = self.setPoint
        else:
            t = floor(raw)
        return f"{self.current_state.id},{t},{self.setPoint}\n"

    # ─── Display + UART Thread ────────────────────────────────────
    def run(self):
        Thread(target=self._display_loop, daemon=True).start()

    def _display_loop(self):
        counter = 1
        toggle  = False
        while not self.endDisplay:
            now   = datetime.now().strftime("%m/%d %H:%M:%S")
            line1 = now.ljust(16)

            raw = self.get_fahrenheit()
            if raw is None:
                sleep(1)
                continue
            temp = floor(raw)

            if toggle:
                line2 = f"{temp}°F".ljust(16)
            else:
                mode  = self.current_state.id.capitalize()
                line2 = f"{mode} @{self.setPoint}°F".ljust(16)
            toggle = not toggle

            self.display.update_screen(line1, line2)

            if counter % 30 == 0:
                ser.write(self.setup_serial_output().encode())
            counter += 1
            sleep(1)

        self.display.cleanup()

# ── Main ─────────────────────────────────────────────────────────
if __name__ == "__main__":
    screen = ManagedDisplay()
    tsm    = TemperatureMachine(screen)
    tsm.run()

    btn_cycle = Button(24, pull_up=False, bounce_time=0.05)
    btn_inc   = Button(25, pull_up=False, bounce_time=0.05)
    btn_dec   = Button(12, pull_up=False, bounce_time=0.05)

    btn_cycle.when_pressed = tsm.process_state_button
    btn_inc.when_pressed   = tsm.process_temp_inc
    btn_dec.when_pressed   = tsm.process_temp_dec

    try:
        while True:
            sleep(1)
    except KeyboardInterrupt:
        print("Cleaning up. Exiting…")
        tsm.endDisplay = True
        sleep(1)
