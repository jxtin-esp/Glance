import tkinter as tk
from tkinter import ttk, messagebox, colorchooser
import serial
import serial.tools.list_ports
import json
import time
import datetime
import calendar
import os

ser = None
EVENTS_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "calendar_events.json")
CONFIG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")

# Persistent Application Configuration (WiFi, Weather, Countdown settings)
def load_config():
    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return {}
    return {}

def save_config(config_dict):
    try:
        with open(CONFIG_FILE, "w", encoding="utf-8") as f:
            json.dump(config_dict, f, indent=2, ensure_ascii=False)
    except Exception as e:
        print(f"Error saving config: {e}")

saved_cfg = load_config()

def persist_all_settings():
    cfg = load_config()
    try:
        if 'wifi_ssid_entry' in globals():
            cfg["wifi_ssid"] = wifi_ssid_entry.get().strip()
        if 'wifi_pass_entry' in globals():
            cfg["wifi_pass"] = wifi_pass_entry.get().strip()
    except Exception:
        pass
    try:
        if 'w_city' in globals():
            cfg["weather_city"] = w_city.get().strip()
        if 'w_font' in globals():
            cfg["weather_font"] = w_font.current()
        if 'w_txt' in globals():
            cfg["weather_txt"] = w_txt.get().strip()
        if 'w_acc' in globals():
            cfg["weather_acc"] = w_acc.get().strip()
        if 'w_card' in globals():
            cfg["weather_card"] = w_card.get().strip()
    except Exception:
        pass
    try:
        if 'cal_font' in globals():
            cfg["cal_font"] = cal_font.current()
        if 'cal_txt' in globals():
            cfg["cal_txt"] = cal_txt.get().strip()
        if 'cal_acc' in globals():
            cfg["cal_acc"] = cal_acc.get().strip()
        if 'cal_card' in globals():
            cfg["cal_card"] = cal_card.get().strip()
    except Exception:
        pass
    try:
        if 'cd_title' in globals():
            cfg["cd_title"] = cd_title.get().strip()
        if 'spin_year' in globals():
            cfg["cd_year"] = int(spin_year.get())
            cfg["cd_month"] = int(spin_month.get())
            cfg["cd_day"] = int(spin_day.get())
            cfg["cd_hour"] = int(spin_hour.get())
            cfg["cd_minute"] = int(spin_min.get())
        if 'cd_font' in globals():
            cfg["cd_font"] = cd_font.current()
        if 'cd_txt' in globals():
            cfg["cd_txt"] = cd_txt.get().strip()
        if 'cd_acc' in globals():
            cfg["cd_acc"] = cd_acc.get().strip()
        if 'cd_card' in globals():
            cfg["cd_card"] = cd_card.get().strip()
    except Exception:
        pass
    save_config(cfg)

# Load calendar events from local JSON file
def load_events():
    if os.path.exists(EVENTS_FILE):
        try:
            with open(EVENTS_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return {}
    return {}

def save_events_to_file(events_dict):
    try:
        with open(EVENTS_FILE, "w", encoding="utf-8") as f:
            json.dump(events_dict, f, indent=2, ensure_ascii=False)
    except Exception as e:
        print(f"Error saving events: {e}")

all_calendar_events = load_events()

def get_com_ports():
    ports = serial.tools.list_ports.comports()
    return [p.device for p in ports]

def connect_serial():
    global ser
    port = port_combobox.get()
    if not port:
        messagebox.showerror("Error", "Please select a serial port from the dropdown.")
        return
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        time.sleep(1.2)
        status_label.config(text=f"Connected: {port}", fg="#10B981")
        messagebox.showinfo("Connected", f"Connected to {port} at 115200 baud!")
    except Exception as e:
        messagebox.showerror("Connection Error", str(e))

def disconnect_serial():
    global ser
    if ser and ser.is_open:
        ser.close()
    ser = None
    status_label.config(text="Disconnected", fg="#EF4444")

def send_payload(payload):
    global ser
    if not ser or not ser.is_open:
        messagebox.showwarning("Not Connected", "Connect to the ESP32 Serial Port first!")
        return False
    try:
        json_str = json.dumps(payload) + "\n"
        ser.write(json_str.encode('utf-8'))
        ser.flush()
        status_label.config(text="Data Synced!", fg="#3B82F6")
        return True
    except Exception as e:
        messagebox.showerror("Send Error", str(e))
        return False

def trigger_touch_calibration():
    if send_payload({"type": "calibrate_touch"}):
        messagebox.showinfo(
            "Touch Calibration Started",
            "Calibration has started on your ESP32 screen!\n\n"
            "Please touch the 4 corner crosshairs shown on the display one by one:\n"
            "1. Top-Left\n"
            "2. Bottom-Left\n"
            "3. Top-Right\n"
            "4. Bottom-Right\n\n"
            "The new orientation will be permanently saved to flash memory."
        )

def choose_color(entry_widget, preview_widget):
    current = entry_widget.get()
    chosen = colorchooser.askcolor(current)[1]
    if chosen:
        entry_widget.delete(0, tk.END)
        entry_widget.insert(0, chosen.upper())
        preview_widget.config(bg=chosen)

# --- GUI ROOT ---
root = tk.Tk()
root.title("ESP32-S3 3D Platformer Dashboard Companion")
root.geometry("680x850")
root.configure(bg="#0F172A")

# Center Tkinter window on desktop screen
root.update_idletasks()
w = 680
h = 850
sw = root.winfo_screenwidth()
sh = root.winfo_screenheight()
x = (sw - w) // 2
y = max(20, (sh - h) // 2)
root.geometry(f"{w}x{h}+{x}+{y}")

style = ttk.Style()
style.theme_use('clam')
style.configure("TNotebook", background="#0F172A", borderwidth=0)
style.configure("TNotebook.Tab", background="#1E293B", foreground="#F8FAFC", padding=[14, 8], font=("Segoe UI", 9, "bold"))
style.map("TNotebook.Tab", background=[("selected", "#3B82F6")], foreground=[("selected", "#FFFFFF")])

# --- TOP SERIAL CONNECTION BAR ---
top_frame = tk.Frame(root, bg="#1E293B", pady=8, padx=14)
top_frame.pack(fill="x", padx=10, pady=(8, 4))

tk.Label(top_frame, text="COM Port:", bg="#1E293B", fg="#F8FAFC", font=("Segoe UI", 9, "bold")).pack(side="left", padx=4)
port_combobox = ttk.Combobox(top_frame, values=get_com_ports(), width=9, state="readonly")
port_combobox.pack(side="left", padx=4)
if port_combobox['values']:
    port_combobox.current(0)

def refresh_ports():
    ports = get_com_ports()
    port_combobox.config(values=ports)
    if ports:
        port_combobox.current(0)

tk.Button(top_frame, text="⟳", bg="#334155", fg="#FFF", relief="flat", padx=6, command=refresh_ports).pack(side="left", padx=2)
tk.Button(top_frame, text="Connect", bg="#10B981", fg="#FFF", relief="flat", padx=8, command=connect_serial).pack(side="left", padx=3)
tk.Button(top_frame, text="Disconnect", bg="#EF4444", fg="#FFF", relief="flat", padx=6, command=disconnect_serial).pack(side="left", padx=3)
tk.Button(top_frame, text="🎯 Calibrate Touch", bg="#8B5CF6", fg="#FFF", relief="flat", padx=8, font=("Segoe UI", 8, "bold"),
          command=trigger_touch_calibration).pack(side="left", padx=5)

status_label = tk.Label(top_frame, text="Disconnected", bg="#1E293B", fg="#EF4444", font=("Segoe UI", 9, "bold"))
status_label.pack(side="right", padx=4)

# --- WIFI SETUP BAR ---
wifi_frame = tk.Frame(root, bg="#1E293B", pady=8, padx=14)
wifi_frame.pack(fill="x", padx=10, pady=4)

tk.Label(wifi_frame, text="📶 WiFi SSID:", bg="#1E293B", fg="#94A3B8", font=("Segoe UI", 9)).pack(side="left")
wifi_ssid_entry = tk.Entry(wifi_frame, width=14, bg="#0F172A", fg="#FFF", insertbackground="white", relief="flat")
wifi_ssid_entry.insert(0, saved_cfg.get("wifi_ssid", ""))
wifi_ssid_entry.pack(side="left", padx=(4, 8), ipady=2)

tk.Label(wifi_frame, text="Password:", bg="#1E293B", fg="#94A3B8", font=("Segoe UI", 9)).pack(side="left")
wifi_pass_entry = tk.Entry(wifi_frame, width=14, bg="#0F172A", fg="#FFF", insertbackground="white", relief="flat", show="*")
wifi_pass_entry.insert(0, saved_cfg.get("wifi_pass", ""))
wifi_pass_entry.pack(side="left", padx=(4, 8), ipady=2)

def send_wifi_creds():
    ssid = wifi_ssid_entry.get().strip()
    password = wifi_pass_entry.get().strip()
    if not ssid:
        messagebox.showerror("Error", "Please enter your WiFi SSID.")
        return
    persist_all_settings()
    payload = {"type": "wifi", "ssid": ssid, "pass": password}
    if send_payload(payload):
        messagebox.showinfo("WiFi Sent", f"Credentials sent and permanently saved to ESP32! Connecting to '{ssid}'...")

tk.Button(wifi_frame, text="Connect ESP32 to WiFi", bg="#3B82F6", fg="#FFF", relief="flat", padx=8, command=send_wifi_creds).pack(side="right")

# --- QUICK SCREEN SWITCHER BAR ---
nav_frame = tk.Frame(root, bg="#0F172A", pady=2)
nav_frame.pack(fill="x", padx=10)
tk.Button(nav_frame, text="👁 Show Weather", bg="#1E293B", fg="#38BDF8", relief="flat", pady=4,
          command=lambda: send_payload({"type":"switch_screen","screen":0})).pack(side="left", expand=True, fill="x", padx=2)
tk.Button(nav_frame, text="👁 Show Calendar", bg="#1E293B", fg="#34D399", relief="flat", pady=4,
          command=lambda: send_payload({"type":"switch_screen","screen":1})).pack(side="left", expand=True, fill="x", padx=2)
tk.Button(nav_frame, text="👁 Show Countdown", bg="#1E293B", fg="#F472B6", relief="flat", pady=4,
          command=lambda: send_payload({"type":"switch_screen","screen":2})).pack(side="left", expand=True, fill="x", padx=2)

notebook = ttk.Notebook(root)
notebook.pack(fill="both", expand=True, padx=10, pady=6)

# UI Helpers
def create_field(parent, label, default_val):
    frame = tk.Frame(parent, bg="#0F172A")
    frame.pack(fill="x", pady=4)
    tk.Label(frame, text=label, width=16, anchor="w", bg="#0F172A", fg="#94A3B8", font=("Segoe UI", 9)).pack(side="left")
    entry = tk.Entry(frame, bg="#1E293B", fg="#F8FAFC", insertbackground="white", relief="flat", font=("Segoe UI", 9))
    entry.insert(0, default_val)
    entry.pack(side="left", fill="x", expand=True, ipady=4, padx=(0, 4))
    return entry

def create_color_picker(parent, label, default_hex):
    frame = tk.Frame(parent, bg="#0F172A")
    frame.pack(fill="x", pady=3)
    tk.Label(frame, text=label, width=16, anchor="w", bg="#0F172A", fg="#94A3B8", font=("Segoe UI", 9)).pack(side="left")
    entry = tk.Entry(frame, width=10, bg="#1E293B", fg="#F8FAFC", insertbackground="white", relief="flat", font=("Segoe UI", 9))
    entry.insert(0, default_hex)
    entry.pack(side="left", padx=4, ipady=3)
    preview = tk.Label(frame, width=4, height=1, bg=default_hex, relief="groove")
    preview.pack(side="left", padx=4)
    btn = tk.Button(frame, text="Pick Color", bg="#334155", fg="#FFF", relief="flat", padx=6, pady=1,
                    command=lambda: choose_color(entry, preview))
    btn.pack(side="left")
    return entry, preview

# ================= TAB 1: WEATHER =================
tab_weather = tk.Frame(notebook, bg="#0F172A", padx=16, pady=12)
notebook.add(tab_weather, text=" 🌤️ Live Weather ")

info_box = tk.Label(tab_weather, text="ℹ️ The ESP32 connects to WiFi and automatically fetches live temperature, conditions, and humidity for your chosen city over open weather services.\n\nThe main weather card features big procedural 8-bit weather art (Sun, Storm, Clouds, Rain, Snow) right beside the current temperature!",
                    bg="#1E293B", fg="#38BDF8", font=("Segoe UI", 8), wraplength=600, justify="left", padx=10, pady=8)
info_box.pack(fill="x", pady=(0, 10))

w_city = create_field(tab_weather, "City / Location:", saved_cfg.get("weather_city", "Tokyo"))

font_frame_w = tk.Frame(tab_weather, bg="#0F172A")
font_frame_w.pack(fill="x", pady=4)
tk.Label(font_frame_w, text="Typography:", width=16, anchor="w", bg="#0F172A", fg="#94A3B8", font=("Segoe UI", 9)).pack(side="left")
w_font = ttk.Combobox(font_frame_w, values=["0: Retro 8-Bit Pixel", "1: Clean Modern"], width=22, state="readonly")
w_font.current(saved_cfg.get("weather_font", 0))
w_font.pack(side="left", ipady=3)

tk.Label(tab_weather, text="🎨 Card & Text Palette Customization", bg="#0F172A", fg="#38BDF8", font=("Segoe UI", 10, "bold")).pack(anchor="w", pady=(12, 4))
w_txt, _ = create_color_picker(tab_weather, "Text Color:", saved_cfg.get("weather_txt", "#FFFFFF"))
w_acc, _ = create_color_picker(tab_weather, "Accent Color:", saved_cfg.get("weather_acc", "#F59E0B"))
w_card, _ = create_color_picker(tab_weather, "Card Container:", saved_cfg.get("weather_card", "#172338"))

def send_weather_city():
    city = w_city.get().strip()
    if not city:
        messagebox.showerror("Error", "City cannot be empty.")
        return
    persist_all_settings()
    payload = {
        "type": "weather_city",
        "city": city,
        "font": int(w_font.get().split(":")[0]),
        "txt": w_txt.get().strip(),
        "acc": w_acc.get().strip(),
        "card": w_card.get().strip()
    }
    if send_payload(payload):
        messagebox.showinfo("City Updated", f"Sent city '{city}' to ESP32! Saved to flash & live weather fetch initiated.")

tk.Button(tab_weather, text="Update City & Fetch Live Weather", bg="#3B82F6", fg="#FFF", font=("Segoe UI", 10, "bold"),
          relief="flat", pady=8, command=send_weather_city).pack(fill="x", pady=(16, 0))

# ================= TAB 2: INTERACTIVE GRID CALENDAR =================
tab_calendar = tk.Frame(notebook, bg="#0F172A", padx=12, pady=10)
notebook.add(tab_calendar, text=" 📅 Grid Calendar ")

# Current calendar state
today_dt = datetime.date.today()
cal_year = today_dt.year
cal_month = today_dt.month
selected_date = today_dt

cal_main_frame = tk.Frame(tab_calendar, bg="#0F172A")
cal_main_frame.pack(fill="both", expand=True)

left_cal_pane = tk.Frame(cal_main_frame, bg="#1E293B", padx=10, pady=8, relief="groove")
left_cal_pane.pack(side="left", fill="both", expand=True, padx=(0, 6))

right_event_pane = tk.Frame(cal_main_frame, bg="#1E293B", padx=10, pady=8, relief="groove")
right_event_pane.pack(side="right", fill="both", expand=True, padx=(6, 0))

# Month navigation
month_nav_frame = tk.Frame(left_cal_pane, bg="#1E293B")
month_nav_frame.pack(fill="x", pady=(0, 6))

def prev_month():
    global cal_month, cal_year
    cal_month -= 1
    if cal_month < 1:
        cal_month = 12
        cal_year -= 1
    render_calendar_grid()

def next_month():
    global cal_month, cal_year
    cal_month += 1
    if cal_month > 12:
        cal_month = 1
        cal_year += 1
    render_calendar_grid()

tk.Button(month_nav_frame, text="◀", bg="#334155", fg="#FFF", relief="flat", padx=6, command=prev_month).pack(side="left")
lbl_month_year = tk.Label(month_nav_frame, text="", bg="#1E293B", fg="#F8FAFC", font=("Segoe UI", 10, "bold"))
lbl_month_year.pack(side="left", expand=True)
tk.Button(month_nav_frame, text="▶", bg="#334155", fg="#FFF", relief="flat", padx=6, command=next_month).pack(side="right")

# Days of Week Header
days_header = tk.Frame(left_cal_pane, bg="#1E293B")
days_header.pack(fill="x")
for i, dname in enumerate(["Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"]):
    tk.Label(days_header, text=dname, width=4, bg="#1E293B", fg="#94A3B8", font=("Segoe UI", 8, "bold")).grid(row=0, column=i, padx=1, pady=2)

grid_days_frame = tk.Frame(left_cal_pane, bg="#1E293B")
grid_days_frame.pack(fill="both", expand=True)

# Right Event Pane Controls
lbl_selected_header = tk.Label(right_event_pane, text="", bg="#1E293B", fg="#34D399", font=("Segoe UI", 9, "bold"))
lbl_selected_header.pack(anchor="w", pady=(0, 6))

tk.Label(right_event_pane, text="Event 1:", bg="#1E293B", fg="#94A3B8", font=("Segoe UI", 8)).pack(anchor="w")
e1_entry = tk.Entry(right_event_pane, bg="#0F172A", fg="#FFF", insertbackground="white", relief="flat", font=("Segoe UI", 9))
e1_entry.pack(fill="x", pady=(2, 6), ipady=3)

tk.Label(right_event_pane, text="Event 2:", bg="#1E293B", fg="#94A3B8", font=("Segoe UI", 8)).pack(anchor="w")
e2_entry = tk.Entry(right_event_pane, bg="#0F172A", fg="#FFF", insertbackground="white", relief="flat", font=("Segoe UI", 9))
e2_entry.pack(fill="x", pady=(2, 6), ipady=3)

tk.Label(right_event_pane, text="Event 3:", bg="#1E293B", fg="#94A3B8", font=("Segoe UI", 8)).pack(anchor="w")
e3_entry = tk.Entry(right_event_pane, bg="#0F172A", fg="#FFF", insertbackground="white", relief="flat", font=("Segoe UI", 9))
e3_entry.pack(fill="x", pady=(2, 8), ipady=3)

def load_selected_date_events():
    dstr = selected_date.strftime("%Y-%m-%d")
    wname = selected_date.strftime("%A")
    lbl_selected_header.config(text=f"Editing: {dstr} ({wname})")
    evs = all_calendar_events.get(dstr, ["", "", ""])
    e1_entry.delete(0, tk.END)
    e1_entry.insert(0, evs[0] if len(evs) > 0 else "")
    e2_entry.delete(0, tk.END)
    e2_entry.insert(0, evs[1] if len(evs) > 1 else "")
    e3_entry.delete(0, tk.END)
    e3_entry.insert(0, evs[2] if len(evs) > 2 else "")

def select_date(y, m, d):
    global selected_date
    selected_date = datetime.date(y, m, d)
    render_calendar_grid()
    load_selected_date_events()

def render_calendar_grid():
    lbl_month_year.config(text=f"{calendar.month_name[cal_month]} {cal_year}")
    for widget in grid_days_frame.winfo_children():
        widget.destroy()

    month_cal = calendar.monthcalendar(cal_year, cal_month)
    for r, week in enumerate(month_cal):
        for c, day_num in enumerate(week):
            if day_num == 0:
                tk.Label(grid_days_frame, text="", width=4, bg="#1E293B").grid(row=r, column=c, padx=1, pady=1)
            else:
                cur_dt = datetime.date(cal_year, cal_month, day_num)
                dstr = cur_dt.strftime("%Y-%m-%d")
                has_event = (dstr in all_calendar_events) and any(bool(e.strip()) for e in all_calendar_events[dstr])

                bg_col = "#334155"
                fg_col = "#F8FAFC"

                if cur_dt == selected_date:
                    bg_col = "#10B981" # Selected date highlighted in emerald green
                    fg_col = "#000"
                elif cur_dt == today_dt:
                    bg_col = "#3B82F6" # Today highlighted in blue
                    fg_col = "#FFF"
                elif has_event:
                    bg_col = "#1E3A5F" # Day with scheduled events

                btn_text = f"{day_num} •" if has_event else f"{day_num}"
                btn = tk.Button(grid_days_frame, text=btn_text, width=4, bg=bg_col, fg=fg_col,
                                font=("Segoe UI", 8, "bold" if (cur_dt==today_dt or cur_dt==selected_date) else "normal"),
                                relief="flat", command=lambda y=cal_year, m=cal_month, d=day_num: select_date(y, m, d))
                btn.grid(row=r, column=c, padx=1, pady=1)

render_calendar_grid()
load_selected_date_events()

def save_current_date_events():
    dstr = selected_date.strftime("%Y-%m-%d")
    e1 = e1_entry.get().strip()
    e2 = e2_entry.get().strip()
    e3 = e3_entry.get().strip()

    if not e1 and not e2 and not e3:
        if dstr in all_calendar_events:
            del all_calendar_events[dstr]
    else:
        all_calendar_events[dstr] = [e1, e2, e3]

    save_events_to_file(all_calendar_events)
    render_calendar_grid()
    sync_full_calendar_to_esp32(silent=False)

def clear_current_date_events():
    dstr = selected_date.strftime("%Y-%m-%d")
    if dstr in all_calendar_events:
        del all_calendar_events[dstr]
        save_events_to_file(all_calendar_events)
    e1_entry.delete(0, tk.END)
    e2_entry.delete(0, tk.END)
    e3_entry.delete(0, tk.END)
    render_calendar_grid()
    sync_full_calendar_to_esp32(silent=True)
    messagebox.showinfo("Cleared", f"Events cleared for {dstr}.")

tk.Button(right_event_pane, text="💾 Save Events for this Date", bg="#10B981", fg="#FFF", font=("Segoe UI", 9, "bold"),
          relief="flat", pady=5, command=save_current_date_events).pack(fill="x", pady=2)
tk.Button(right_event_pane, text="🗑 Clear Date Events", bg="#334155", fg="#EF4444", font=("Segoe UI", 8),
          relief="flat", pady=3, command=clear_current_date_events).pack(fill="x", pady=2)

# Styling & Appearance for Calendar Screen
cal_style_frame = tk.Frame(tab_calendar, bg="#0F172A", pady=6)
cal_style_frame.pack(fill="x")

font_frame_c = tk.Frame(cal_style_frame, bg="#0F172A")
font_frame_c.pack(fill="x", pady=2)
tk.Label(font_frame_c, text="Typography:", width=16, anchor="w", bg="#0F172A", fg="#94A3B8", font=("Segoe UI", 9)).pack(side="left")
cal_font = ttk.Combobox(font_frame_c, values=["0: Retro 8-Bit Pixel", "1: Clean Modern"], width=22, state="readonly")
cal_font.current(saved_cfg.get("cal_font", 0))
cal_font.pack(side="left", ipady=2)

cal_txt, _ = create_color_picker(cal_style_frame, "Text Color:", saved_cfg.get("cal_txt", "#FFFFFF"))
cal_acc, _ = create_color_picker(cal_style_frame, "Accent Dot:", saved_cfg.get("cal_acc", "#10B981"))
cal_card, _ = create_color_picker(cal_style_frame, "Card Container:", saved_cfg.get("cal_card", "#111827"))

def sync_full_calendar_to_esp32(silent=False):
    persist_all_settings()
    # Prepare bulk calendar payload
    events_list = []
    for dstr, evs in all_calendar_events.items():
        events_list.append({
            "date": dstr,
            "e1": evs[0] if len(evs) > 0 else "",
            "e2": evs[1] if len(evs) > 1 else "",
            "e3": evs[2] if len(evs) > 2 else ""
        })

    payload = {
        "type": "calendar_sync",
        "events": events_list[:24], # Up to 24 scheduled dates
        "font": int(cal_font.get().split(":")[0]),
        "txt": cal_txt.get().strip(),
        "acc": cal_acc.get().strip(),
        "card": cal_card.get().strip()
    }
    ok = send_payload(payload)
    if ok and not silent:
        messagebox.showinfo("Calendar Synced", f"Synchronized {len(events_list)} scheduled day(s) to ESP32!\n\nThe schedule is saved to flash and will automatically display each day's events as that day arrives.")

tk.Button(tab_calendar, text="⚡ Sync Full Calendar Schedule to ESP32", bg="#10B981", fg="#FFF", font=("Segoe UI", 10, "bold"),
          relief="flat", pady=8, command=lambda: sync_full_calendar_to_esp32(silent=False)).pack(fill="x", pady=(6, 0))

# ================= TAB 3: COUNTDOWN =================
tab_countdown = tk.Frame(notebook, bg="#0F172A", padx=16, pady=12)
notebook.add(tab_countdown, text=" ⏳ Countdown ")

cd_title = create_field(tab_countdown, "Target Event:", saved_cfg.get("cd_title", "CYBERPUNK 2077"))

# Date & Time Selection Box
date_box = tk.LabelFrame(tab_countdown, text=" Select Target Date & Time ", bg="#0F172A", fg="#F472B6", font=("Segoe UI", 9, "bold"), padx=10, pady=8)
date_box.pack(fill="x", pady=8)

now = datetime.datetime.now()
target_default = now + datetime.timedelta(days=14, hours=5, minutes=30)

row_dt = tk.Frame(date_box, bg="#0F172A")
row_dt.pack(fill="x", pady=2)

tk.Label(row_dt, text="Year:", bg="#0F172A", fg="#94A3B8").pack(side="left")
spin_year = ttk.Spinbox(row_dt, from_=2024, to=2035, width=6)
spin_year.set(saved_cfg.get("cd_year", target_default.year))
spin_year.pack(side="left", padx=(2, 8))

tk.Label(row_dt, text="Month:", bg="#0F172A", fg="#94A3B8").pack(side="left")
spin_month = ttk.Spinbox(row_dt, from_=1, to=12, width=4)
spin_month.set(saved_cfg.get("cd_month", target_default.month))
spin_month.pack(side="left", padx=(2, 8))

tk.Label(row_dt, text="Day:", bg="#0F172A", fg="#94A3B8").pack(side="left")
spin_day = ttk.Spinbox(row_dt, from_=1, to=31, width=4)
spin_day.set(saved_cfg.get("cd_day", target_default.day))
spin_day.pack(side="left", padx=(2, 8))

tk.Label(row_dt, text="Time:", bg="#0F172A", fg="#94A3B8").pack(side="left")
spin_hour = ttk.Spinbox(row_dt, from_=0, to=23, width=4)
spin_hour.set(saved_cfg.get("cd_hour", target_default.hour))
spin_hour.pack(side="left", padx=2)
tk.Label(row_dt, text=":", bg="#0F172A", fg="#FFF").pack(side="left")
spin_min = ttk.Spinbox(row_dt, from_=0, to=59, width=4)
spin_min.set(saved_cfg.get("cd_minute", target_default.minute))
spin_min.pack(side="left", padx=2)

# Quick Presets
presets_frame = tk.Frame(date_box, bg="#0F172A")
presets_frame.pack(fill="x", pady=(6, 2))

def set_preset(days_ahead):
    t = datetime.datetime.now() + datetime.timedelta(days=days_ahead)
    spin_year.set(t.year)
    spin_month.set(t.month)
    spin_day.set(t.day)
    spin_hour.set(t.hour)
    spin_min.set(t.minute)
    update_countdown_label()

tk.Button(presets_frame, text="+1 Day", bg="#1E293B", fg="#FFF", relief="flat", padx=6, command=lambda: set_preset(1)).pack(side="left", padx=2)
tk.Button(presets_frame, text="+7 Days", bg="#1E293B", fg="#FFF", relief="flat", padx=6, command=lambda: set_preset(7)).pack(side="left", padx=2)
tk.Button(presets_frame, text="+30 Days", bg="#1E293B", fg="#FFF", relief="flat", padx=6, command=lambda: set_preset(30)).pack(side="left", padx=2)

def set_new_year():
    ny = datetime.datetime(datetime.datetime.now().year + 1, 1, 1, 0, 0)
    spin_year.set(ny.year)
    spin_month.set(ny.month)
    spin_day.set(ny.day)
    spin_hour.set(0)
    spin_min.set(0)
    update_countdown_label()

tk.Button(presets_frame, text="Next New Year", bg="#1E293B", fg="#F472B6", relief="flat", padx=6, command=set_new_year).pack(side="left", padx=2)

# Live dynamic preview label
lbl_preview_cd = tk.Label(date_box, text="", bg="#0F172A", fg="#F472B6", font=("Segoe UI", 9, "bold"))
lbl_preview_cd.pack(anchor="w", pady=(6, 0))

def update_countdown_label(*args):
    try:
        t = datetime.datetime(int(spin_year.get()), int(spin_month.get()), int(spin_day.get()),
                              int(spin_hour.get()), int(spin_min.get()))
        diff = t - datetime.datetime.now()
        tot = max(0, int(diff.total_seconds()))
        d = tot // 86400
        h = (tot % 86400) // 3600
        m = (tot % 3600) // 60
        s = tot % 60
        lbl_preview_cd.config(text=f"⏳ Time Remaining: {d} Days, {h} Hours, {m} Mins, {s} Secs")
    except Exception:
        lbl_preview_cd.config(text="⚠️ Invalid date entered!")

for sp in (spin_year, spin_month, spin_day, spin_hour, spin_min):
    sp.bind("<KeyRelease>", update_countdown_label)
    sp.bind("<<Increment>>", update_countdown_label)
    sp.bind("<<Decrement>>", update_countdown_label)
update_countdown_label()

font_frame_cd = tk.Frame(tab_countdown, bg="#0F172A")
font_frame_cd.pack(fill="x", pady=4)
tk.Label(font_frame_cd, text="Typography:", width=16, anchor="w", bg="#0F172A", fg="#94A3B8", font=("Segoe UI", 9)).pack(side="left")
cd_font = ttk.Combobox(font_frame_cd, values=["0: Retro 8-Bit Pixel", "1: Clean Modern"], width=22, state="readonly")
cd_font.current(saved_cfg.get("cd_font", 0))
cd_font.pack(side="left", ipady=3)

tk.Label(tab_countdown, text="🎨 Card & Text Palette Customization", bg="#0F172A", fg="#F472B6", font=("Segoe UI", 10, "bold")).pack(anchor="w", pady=(10, 4))
cd_txt, _ = create_color_picker(tab_countdown, "Digits Text:", saved_cfg.get("cd_txt", "#FFFFFF"))
cd_acc, _ = create_color_picker(tab_countdown, "Accent Border:", saved_cfg.get("cd_acc", "#EC4899"))
cd_card, _ = create_color_picker(tab_countdown, "Card Container:", saved_cfg.get("cd_card", "#20083B"))

def send_countdown():
    try:
        target_dt = datetime.datetime(int(spin_year.get()), int(spin_month.get()), int(spin_day.get()),
                                      int(spin_hour.get()), int(spin_min.get()))
        target_epoch = int(target_dt.timestamp())
        diff = target_dt - datetime.datetime.now()
        total_seconds = max(0, int(diff.total_seconds()))
    except Exception as e:
        messagebox.showerror("Error", f"Invalid target date: {e}")
        return

    persist_all_settings()
    payload = {
        "type": "countdown",
        "title": cd_title.get().strip(),
        "target_epoch": target_epoch,
        "seconds": total_seconds,
        "font": int(cd_font.get().split(":")[0]),
        "txt": cd_txt.get().strip(),
        "acc": cd_acc.get().strip(),
        "card": cd_card.get().strip()
    }
    if send_payload(payload):
        messagebox.showinfo("Countdown Updated", f"Countdown target set and permanently saved to ESP32 flash!\nTarget: {target_dt.strftime('%Y-%m-%d %H:%M')} ({total_seconds // 86400} days left)")

tk.Button(tab_countdown, text="Sync Countdown Target to ESP32", bg="#EC4899", fg="#FFF", font=("Segoe UI", 10, "bold"),
          relief="flat", pady=8, command=send_countdown).pack(fill="x", pady=(14, 0))

# --- BOTTOM FULL SYNC BUTTON ---
bottom_frame = tk.Frame(root, bg="#0F172A", pady=6)
bottom_frame.pack(fill="x", padx=10, pady=4)

def sync_all():
    persist_all_settings()
    send_weather_city()
    time.sleep(0.08)
    sync_full_calendar_to_esp32(silent=True)
    time.sleep(0.08)
    send_countdown()

tk.Button(bottom_frame, text="⚡ Sync All Screens (Weather + Calendar + Countdown)", bg="#6366F1", fg="#FFF", font=("Segoe UI", 10, "bold"),
          relief="flat", pady=6, command=sync_all).pack(fill="x")

root.mainloop()
