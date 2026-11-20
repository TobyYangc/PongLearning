"""階段 0：只建立一個 Tk 視窗。"""
import tkinter as tk

root = tk.Tk()
root.title("Pong - Step 0")
root.geometry("1280x720")
canvas = tk.Canvas(root, bg="black", highlightthickness=0)
canvas.pack(fill="both", expand=True)
canvas.create_text(640, 360, text="PONG", fill="white",
                   font=("Consolas", 64, "bold"))
root.mainloop()
