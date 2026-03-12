import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import multiprocessing
import threading
import time
import urllib.request
import urllib.parse
import json
import os

bot_token = "5142290467:AAH8aHrW7GIyoZ5IGUCUZ3So5actekfmh0Q"

def send_telegram_message(message, chat_id, log_callback=None):
    if not chat_id or chat_id == "YOUR_CHAT_ID_HERE":
        if log_callback:
            log_callback("[-] Vui lòng điền Chat ID Telegram để gửi thông báo.\n")
        return

    try:
        url = f"https://api.telegram.org/bot{bot_token}/sendMessage"
        data = urllib.parse.urlencode({'chat_id': chat_id, 'text': message}).encode('utf-8')
        req = urllib.request.Request(url, data=data)
        with urllib.request.urlopen(req) as response:
            res = json.loads(response.read().decode())
            if res.get("ok"):
                if log_callback:
                    log_callback("[+] Đã gửi thông báo Telegram.\n")
            else:
                if log_callback:
                    log_callback(f"[-] Lỗi gửi Telegram: {res}\n")
    except Exception as e:
        if log_callback:
            log_callback(f"[-] Lỗi gửi Telegram: {e}\n")

def cpu_stress_task(stop_event):
    memory_hog = []
    while not stop_event.is_set():
        # 1. Stress CPU: Thực hiện list comprehension và tính toán số thực liên tục
        _ = [x ** 2.5 for x in range(30000)]
        
        # 2. Stress RAM: Sử dụng bytearray sinh từ chunk ngẫu nhiên ngốn RAM thực sự
        if len(memory_hog) < 1000:
            memory_hog.append(os.urandom(1024 * 1024))

class CPUStressApp:
    def __init__(self, root):
        self.root = root
        self.root.title("CPU Stress Test UI")
        self.root.geometry("600x450")
        self.root.configure(padx=20, pady=20)
        
        self.stop_event = multiprocessing.Event()
        self.processes = []
        self.is_running = False
        
        # Styles
        style = ttk.Style()
        style.theme_use('clam')
        style.configure('TLabel', font=('Segoe UI', 10))
        style.configure('TButton', font=('Segoe UI', 10, 'bold'), padding=5)
        style.configure('Header.TLabel', font=('Segoe UI', 16, 'bold'))

        # Header
        ttk.Label(root, text="Chương Trình Mô Phỏng Tải CPU", style='Header.TLabel').pack(pady=(0, 20))

        # Inputs Frame
        input_frame = ttk.Frame(root)
        input_frame.pack(fill=tk.X, pady=5)

        # Duration
        ttk.Label(input_frame, text="Thời gian (giây):").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.duration_var = tk.IntVar(value=30)
        self.duration_spinbox = ttk.Spinbox(input_frame, from_=1, to=3600, textvariable=self.duration_var, width=10)
        self.duration_spinbox.grid(row=0, column=1, sticky=tk.W, padx=10, pady=5)

        # Telegram Chat ID
        ttk.Label(input_frame, text="Telegram Chat ID:").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.chat_id_var = tk.StringVar(value="2047641647")
        ttk.Entry(input_frame, textvariable=self.chat_id_var, width=25).grid(row=1, column=1, sticky=tk.W, padx=10, pady=5)

        # Buttons Frame
        btn_frame = ttk.Frame(root)
        btn_frame.pack(fill=tk.X, pady=15)

        self.start_btn = ttk.Button(btn_frame, text=" BẮT ĐẦU ", command=self.start_stress)
        self.start_btn.pack(side=tk.LEFT, padx=(0, 10))

        self.stop_btn = ttk.Button(btn_frame, text=" DỪNG KHẨN CẤP ", command=self.stop_stress, state=tk.DISABLED)
        self.stop_btn.pack(side=tk.LEFT)

        # Log Console
        ttk.Label(root, text="Nhật ký hoạt động:").pack(anchor=tk.W)
        self.log_area = scrolledtext.ScrolledText(root, height=10, font=('Consolas', 9), bg='#1e1e1e', fg='#00ff00')
        self.log_area.pack(fill=tk.BOTH, expand=True, pady=(5, 0))

    def log(self, message):
        self.log_area.configure(state='normal')
        self.log_area.insert(tk.END, message)
        self.log_area.see(tk.END)
        self.log_area.configure(state='disabled')
        self.root.update_idletasks()

    def start_stress(self):
        if self.is_running:
            return
            
        try:
            duration = int(self.duration_var.get())
        except ValueError:
            messagebox.showerror("Lỗi", "Thời gian không hợp lệ!")
            return

        chat_id = self.chat_id_var.get().strip()
        num_cores = multiprocessing.cpu_count()
        
        self.is_running = True
        self.start_btn.config(state=tk.DISABLED)
        self.stop_btn.config(state=tk.NORMAL)
        self.stop_event.clear()
        
        self.log("=========================================\n")
        start_msg = f"Hệ thống cấp phát {num_cores} luồng CPU. Đang đẩy mức sử dụng lên cao trong tối đa {duration} giây...\n"
        self.log(start_msg)
        
        # Chạy logic trong thread khác để không block GUI
        threading.Thread(target=self._run_stress_thread, args=(duration, num_cores, chat_id), daemon=True).start()

    def _run_stress_thread(self, duration, num_cores, chat_id):
        # Gửi telegram
        send_telegram_message(f"Bắt đầu CPU Stress: Hệ thống cấp phát {num_cores} luồng CPU trong {duration} giây.", chat_id, self.log)
        
        self.processes = []
        for _ in range(num_cores):
            p = multiprocessing.Process(target=cpu_stress_task, args=(self.stop_event,))
            self.processes.append(p)
            p.start()
            
        start_time = time.time()
        
        # Theo dõi
        while True:
            if self.stop_event.is_set():
                break
                
            elapsed_time = time.time() - start_time
            if elapsed_time >= duration:
                end_msg = f"\n[+] Đã chạy đủ thời gian giới hạn ({duration} giây). Đang tự động dừng...\n"
                self.root.after(0, self.log, end_msg)
                send_telegram_message(end_msg.strip(), chat_id, lambda m: self.root.after(0, self.log, m))
                self.stop_event.set()
                break
                
            time.sleep(0.5)
            
        # Chờ các tiến trình kết thúc
        for p in self.processes:
            p.join()
            
        final_msg = "Hoàn tất dọn dẹp. Hệ thống đã an toàn và tài nguyên được giải phóng hoàn toàn.\n"
        self.root.after(0, self.log, final_msg)
        send_telegram_message(final_msg.strip(), chat_id, lambda m: self.root.after(0, self.log, m))
        
        # Reset UI
        self.root.after(0, self._reset_ui)

    def stop_stress(self):
        if not self.is_running:
            return
            
        stop_msg = "\n[!] Đã nhận lệnh dừng khẩn cấp từ người dùng...\n"
        self.log(stop_msg)
        send_telegram_message(stop_msg.strip(), self.chat_id_var.get().strip(), self.log)
        self.stop_event.set()

    def _reset_ui(self):
        self.is_running = False
        self.start_btn.config(state=tk.NORMAL)
        self.stop_btn.config(state=tk.DISABLED)

if __name__ == '__main__':
    # Bắt buộc cho multiprocessing trên Windows
    multiprocessing.freeze_support()
    root = tk.Tk()
    app = CPUStressApp(root)
    root.mainloop()
