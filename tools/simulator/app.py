#!/usr/bin/env python3
"""Desktop viewer for a native build of PagePet Reader's actual application."""
import argparse
from pathlib import Path
import queue
import sys

from client import Backend, as_pgm, save_png
from demo import ensure_demo


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--card", type=Path, help="Existing folder to use as the microSD card")
    parser.add_argument("--binary", type=Path, default=Path(".cache/simulator/pagepet-simulator"))
    parser.add_argument("--snapshot", type=Path, help="Save the Home screen as PNG without opening a window")
    parser.add_argument("--smoke-test", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    default_card = Path(".cache/simulator/sdcard").resolve()
    card = args.card.expanduser().resolve() if args.card else default_card
    if args.card and not card.is_dir():
        parser.error("--card must point to an existing directory")
    if not args.card:
        ensure_demo(card)
    if not args.binary.is_file():
        parser.error("Build the simulator with ./tools/simulate.sh first")
    if args.snapshot:
        backend = Backend(args.binary, card)
        try:
            _, pbm = backend.ready()
            save_png(pbm, args.snapshot)
            print(args.snapshot.resolve())
        finally:
            backend.close()
        return

    try:
        import tkinter as tk
        from tkinter import filedialog, messagebox, ttk
    except ImportError:
        parser.error("Python 3 with Tk is required for the desktop window")

    class Simulator:
        def __init__(self, root):
            self.root = root
            self.card = card
            self.backend = Backend(args.binary, card)
            self.state = {}
            self.pbm = None
            self.image = None
            self.closing = False
            self.smoke_phase = 0
            root.title("PagePet X3 — симулятор")
            root.configure(padx=16, pady=16)
            root.protocol("WM_DELETE_WINDOW", self.close)

            screen_frame = ttk.Frame(root, padding=5, relief="sunken")
            screen_frame.grid(row=0, column=0, sticky="n")
            self.screen = tk.Label(screen_frame, bg="white", borderwidth=0)
            self.screen.pack()
            front_keys = ttk.Frame(screen_frame)
            front_keys.pack(fill="x", pady=(6, 0))
            for column, label, command in ((0, "Назад", "back"), (1, "OK", "ok"),
                                            (2, "←", "left"), (3, "→", "right")):
                ttk.Button(front_keys, text=label, width=5, command=lambda c=command: self.send(c)).grid(
                    row=0, column=column, sticky="ew", padx=2)
                front_keys.columnconfigure(column, weight=1)
            panel = ttk.Frame(root, padding=(18, 0, 0, 0))
            panel.grid(row=0, column=1, sticky="n")
            ttk.Label(panel, text="PagePet / X3", font=("TkDefaultFont", 17, "bold")).pack(anchor="w")
            ttk.Label(panel, text="Симулятор чтения", padding=(0, 4, 0, 16)).pack(anchor="w")

            controls = ttk.Frame(panel)
            controls.pack(fill="x")
            for row, label, command in ((0, "↑  Вверх", "up"), (1, "OK  Подтверждение", "ok"),
                                         (2, "↓  Вниз", "down"), (3, "←  Назад", "back")):
                ttk.Button(controls, text=label, command=lambda c=command: self.send(c)).grid(
                    row=row, column=0, sticky="ew", pady=3)
            controls.columnconfigure(0, weight=1)
            ttk.Label(panel, text="Клавиши: ↑ / ↓ · Enter · Esc", padding=(0, 6, 0, 12)).pack(anchor="w")
            self.power = ttk.Button(panel, text="Питание / сон", command=lambda: self.send("power"))
            self.power.pack(fill="x", pady=3)
            ttk.Button(panel, text="Перезапустить", command=lambda: self.send("restart")).pack(fill="x", pady=3)
            ttk.Button(panel, text="Папка microSD…", command=self.choose_card).pack(fill="x", pady=3)
            self.card_button = ttk.Button(panel, text="Извлечь microSD", command=self.toggle_card)
            self.card_button.pack(fill="x", pady=3)
            ttk.Button(panel, text="+30 секунд чтения", command=lambda: self.send("advance")).pack(fill="x", pady=3)
            ttk.Button(panel, text="Сохранить экран…", command=self.snapshot).pack(fill="x", pady=3)

            battery = ttk.Frame(panel)
            battery.pack(fill="x", pady=(10, 0))
            ttk.Label(battery, text="Заряд симулятора, %").grid(row=0, column=0, columnspan=2, sticky="w")
            self.battery_level = tk.StringVar(value="82")
            ttk.Spinbox(battery, from_=0, to=100, textvariable=self.battery_level, width=5).grid(row=1, column=0, sticky="w")
            ttk.Button(battery, text="Показать", command=self.apply_battery).grid(row=1, column=1, padx=(8, 0))
            self.charging = tk.BooleanVar(value=False)
            ttk.Checkbutton(battery, text="Заряжается", variable=self.charging, command=self.apply_battery).grid(
                row=2, column=0, columnspan=2, sticky="w")

            self.scale = tk.StringVar(value="75%")
            scales = ttk.Combobox(panel, textvariable=self.scale, values=("50%", "75%", "100%"), state="readonly", width=8)
            scales.pack(anchor="w", pady=(12, 4))
            scales.bind("<<ComboboxSelected>>", lambda _: self.draw())

            self.status = tk.StringVar(value="Загрузка…")
            ttk.Label(panel, textvariable=self.status, wraplength=240, padding=(0, 10)).pack(anchor="w")
            ttk.Label(panel, text="Скорость ESP32, расход батареи\nи поведение e-ink проверяются\nна настоящем устройстве.",
                      foreground="#666666", padding=(0, 8)).pack(anchor="w")
            self.folder = tk.StringVar(value=str(card))
            ttk.Label(panel, textvariable=self.folder, wraplength=240, foreground="#666666").pack(anchor="w")
            self.log = tk.Text(panel, width=32, height=7, state="disabled", wrap="word", font=("TkFixedFont", 9))
            self.log.pack(fill="x", pady=(12, 0))

            for key, command in (("<Up>", "up"), ("<Left>", "left"), ("<Prior>", "up"),
                                 ("<Down>", "down"), ("<Right>", "right"), ("<Next>", "down"),
                                 ("<Return>", "ok"), ("<Escape>", "back"), ("<BackSpace>", "back"),
                                 ("<Key-p>", "power"), ("<Control-r>", "restart")):
                root.bind(key, lambda event, c=command: self.key(c))
            root.after(30, self.poll)
            if args.smoke_test:
                root.after(30000, self.smoke_timeout)

        def key(self, command):
            # Editing a fake sensor value must not trigger the reader's keys.
            if isinstance(self.root.focus_get(), (ttk.Spinbox, ttk.Combobox, ttk.Entry)):
                return None
            self.send(command)
            return "break"

        def send(self, command):
            try:
                self.backend.send(command)
            except (OSError, RuntimeError) as error:
                messagebox.showerror("Симулятор", str(error))

        def poll(self):
            if self.closing:
                return
            try:
                while True:
                    event = self.backend.events.get_nowait()
                    if isinstance(event, Exception):
                        self.status.set(str(event))
                        break
                    self.state, self.pbm = event
                    self.draw()
                    self.update_status()
                    if args.smoke_test and not self.state["busy"]:
                        self.smoke_step()
            except queue.Empty:
                pass
            lines = []
            try:
                while True:
                    lines.append(self.backend.logs.get_nowait())
            except queue.Empty:
                pass
            if lines:
                self.log.configure(state="normal")
                self.log.insert("end", "\n".join(lines) + "\n")
                if int(self.log.index("end-1c").split(".")[0]) > 200:
                    self.log.delete("1.0", "100.0")
                self.log.see("end")
                self.log.configure(state="disabled")
            self.root.after(30, self.poll)

        def draw(self):
            if not self.pbm:
                return
            image = tk.PhotoImage(data=as_pgm(self.pbm), format="PPM")
            if self.scale.get() == "75%":
                image = image.zoom(3, 3).subsample(4, 4)
            elif self.scale.get() == "50%":
                image = image.subsample(2, 2)
            self.image = image
            self.screen.configure(image=image)

        def update_status(self):
            s = self.state
            if s["frame"] <= 2 and s["batteryKnown"]:
                self.battery_level.set(str(s["battery"]))
                self.charging.set(s["charging"])
            names = {"home": "Главный экран", "library": "Библиотека", "reading": "Чтение", "settings": "Настройки",
                     "companion": "PagePet", "error": "Ошибка"}
            text = "Сон · нажмите питание" if s["sleeping"] else names.get(s["screen"], s["screen"])
            if s["busy"]:
                text += "\nПодготовка страниц…"
            elif s["screen"] == "reading" and not s["sleeping"]:
                text += f'\nГлава {s["chapter"] + 1}/{s["chapters"]} · страница {s["page"] + 1}/{s["pages"]}'
            elif s["screen"] == "library":
                text += f'\n{s["libraryPath"]}\nЭлементов: {s["libraryCount"]}'
            if s["error"]:
                text += "\n" + s["error"]
            if not s["card"]:
                text += "\nmicroSD извлечена"
            self.status.set(text)
            self.power.configure(text="Питание / проснуться" if s["sleeping"] else "Питание / сон")
            self.card_button.configure(text="Извлечь microSD" if s["card"] else "Вставить microSD")

        def toggle_card(self):
            self.send("card-off" if self.state.get("card", True) else "card-on")

        def apply_battery(self):
            try:
                percentage = int(self.battery_level.get())
                if not 0 <= percentage <= 100:
                    raise ValueError
            except ValueError:
                messagebox.showerror("Заряд симулятора", "Введите число от 0 до 100")
                return
            self.send(f"battery {percentage} {int(self.charging.get())}")

        def choose_card(self):
            selected = filedialog.askdirectory(title="Папка вместо microSD", initialdir=self.card)
            if selected:
                self.backend.close()
                self.card = Path(selected).resolve()
                self.folder.set(str(self.card))
                self.backend = Backend(args.binary, self.card)

        def snapshot(self):
            if not self.pbm:
                return
            path = filedialog.asksaveasfilename(title="Сохранить экран", defaultextension=".png",
                                              initialfile="pagepet-screen.png", filetypes=[("PNG", "*.png")])
            if path:
                try:
                    save_png(self.pbm, path)
                except OSError as error:
                    messagebox.showerror("Сохранить экран", str(error))

        def smoke_step(self):
            # Exercise actual Tk image decoding plus keyboard bindings.
            if self.smoke_phase == 0 and self.state["screen"] == "home":
                self.root.focus_force()
                if self.state["selection"] != 1:
                    self.root.after(50, lambda: self.root.event_generate("<Down>"))
                    return
                self.smoke_phase = 1
                self.root.after(50, lambda: self.root.event_generate("<Return>"))
            elif self.smoke_phase == 1 and self.state["screen"] == "library":
                row = self.state["librarySelection"] - self.state["libraryFirst"]
                if self.state["libraryRows"][row]["folder"]:
                    self.root.after(50, lambda: self.root.event_generate("<Down>"))
                    return
                self.smoke_phase = 2
                self.root.after(50, lambda: self.root.event_generate("<Return>"))
            elif self.smoke_phase == 2 and self.state["screen"] == "reading":
                self.smoke_phase = 3
                forward = self.state["page"] + 1 < self.state["pages"]
                self.smoke_expected_page = self.state["page"] + (1 if forward else -1)
                self.root.after(50, lambda: self.root.event_generate("<Down>" if forward else "<Up>"))
            elif self.smoke_phase == 3 and self.state["page"] == self.smoke_expected_page:
                self.smoke_phase = 4
                self.battery_level.set("5")
                self.charging.set(True)
                self.root.after(50, self.apply_battery)
            elif self.smoke_phase == 4 and self.state["battery"] == 5 and self.state["charging"]:
                self.smoke_phase = 5
                print("Tk simulator smoke test passed", flush=True)
                self.root.after(50, self.close)

        def smoke_timeout(self):
            if self.smoke_phase < 5:
                print("Tk simulator smoke test timed out", file=sys.stderr, flush=True)
                self.close()
                raise SystemExit(1)

        def close(self):
            self.closing = True
            self.backend.close()
            self.root.destroy()

    try:
        root = tk.Tk()
    except tk.TclError as error:
        parser.exit(1, f"Could not open the desktop window: {error}\n"
                    "Run from a graphical desktop, or use --snapshot screen.png.\n")
    Simulator(root)
    root.mainloop()


if __name__ == "__main__":
    main()
