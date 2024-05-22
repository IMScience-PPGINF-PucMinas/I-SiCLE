###########################################################
# Created by Lucca Lacerda
# 2024
###########################################################

import tkinter as tk
from tkinter import filedialog
from PIL import Image, ImageTk, ImageDraw
import subprocess

# Constants for marking modes
MODE_NEW_SEED = "New Seed"
MODE_LOCATE_SUPERPIXEL = "Locate Superpixel"

# Coordinates for the single exclusive X (superpixel)
superpixel_coords = None

# List to store marked points (new seeds)
marked_seeds = []

# Variable to store the current marking mode
current_mode = None

# Define the output_text widget
output_text = None

# Define the image path
image_path = None


def run_sicle(image_path, n0=None, nf=None):
    # Adjust the command to include the selected values of n0 and nf
    cmd_sicle = ['./bin/RunSICLE', '--img', image_path, '--out', 'toyExOut.pgm', "--multiscale"]

    if n0 is not None:
        cmd_sicle.extend(['--n0', n0])
    if nf is not None:
        cmd_sicle.extend(['--nf', nf])

    process_sicle = subprocess.Popen(cmd_sicle,
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE)
    stdout_sicle, stderr_sicle = process_sicle.communicate()

    if stderr_sicle:
        # If there's an error from RunSICLE, display it
        output_text.delete(1.0, tk.END)
        output_text.insert(tk.END, "RunSICLE Errors:\n" + stderr_sicle.decode())
        output_text.pack()  # Ensure the output text is visible
    else:
        # No error from RunSICLE, proceed to run RunOvlayBorders
        run_ovlay_borders(image_path)


def run_ovlay_borders(image_path):
    # Command for RunOvlayBorders
    cmd_ovlay = [
        './bin/RunOvlayBorders', '--img', image_path, '--labels',
        'toyExOut1.pgm', '--out', 'interface.out', '--rgb', '0,1,0'
    ]
    process_ovlay = subprocess.Popen(cmd_ovlay,
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE)
    stdout_ovlay, stderr_ovlay = process_ovlay.communicate()

    if stderr_ovlay:
        # If there's an error from RunOvlayBorders, display it
        output_text.delete(1.0, tk.END)
        output_text.insert(tk.END,
                           "RunOvlayBorders Errors:\n" + stderr_ovlay.decode())
        output_text.pack()  # Ensure the output text is visible
    else:
        # If no error, hide the output_text widget and display the final image
        output_text.pack_forget()  # Hide the text widget if no errors
        display_image('interface.out')  # Assuming 'interface.out' is an image file


def select_image():
    global image_path
    # Let the user select an image file
    file_path = filedialog.askopenfilename()
    if file_path:
        # If a file was selected, run the processing chain with that image
        image_path = file_path
        n0_value = n0_entry.get() if n0_entry.get() else None
        nf_value = nf_entry.get() if nf_entry.get() else None
        run_sicle(file_path, n0_value, nf_value)


def display_image(image_path):

    def on_click(event):
        # Check the current marking mode and perform the corresponding action
        if current_mode == MODE_NEW_SEED:
            mark_seed(event)
        elif current_mode == MODE_LOCATE_SUPERPIXEL:
            locate_superpixel(event)

    global canvas_image
    # Open the image without resizing
    img = Image.open(image_path)
    imgTk = ImageTk.PhotoImage(img)

    # Adjust the canvas size to the image size
    canvas.config(width=img.width, height=img.height)

    if canvas_image is not None:
        canvas.delete(canvas_image)
    canvas_image = canvas.create_image(0, 0, anchor=tk.NW, image=imgTk)
    canvas.image = imgTk

    # Bind mouse click event to the canvas
    canvas.bind('<Button-1>', on_click)


def mark_seed(event):
    # Get the coordinates of the mouse click
    x, y = event.x, event.y

    # Store the coordinates of the click
    marked_seeds.append((x, y))

    # Draw a point at the clicked position
    draw_mark(x, y, "red")


def locate_superpixel(event):
    global superpixel_coords
    # Get the coordinates of the mouse click
    x, y = event.x, event.y

    # Store the coordinates of the click
    superpixel_coords = (x, y)

    # Draw an exclusive X at the clicked position
    draw_exclusive_x(x, y, "blue")


def draw_mark(x, y, color):
    # Draw a point at the specified position
    canvas.create_oval(x - 2, y - 2, x + 2, y + 2, fill=color, tags="marks")


def draw_exclusive_x(x, y, color):
    global superpixel_coords
    # Delete the previous exclusive X if it exists
    if superpixel_coords:
        canvas.delete("exclusive_x")

    # Draw an exclusive X at the specified position
    canvas.create_line(x - 5,
                       y - 5,
                       x + 5,
                       y + 5,
                       fill=color,
                       tags="exclusive_x")
    canvas.create_line(x + 5,
                       y - 5,
                       x - 5,
                       y + 5,
                       fill=color,
                       tags="exclusive_x")


def erase():
    # Clear all marked points and the exclusive X
    global marked_seeds, superpixel_coords
    marked_seeds = []
    superpixel_coords = None
    canvas.delete("marks")  # Delete all marks from the canvas
    canvas.delete("exclusive_x")  # Delete the exclusive X from the canvas


def set_mode_new_seed():
    # Set the current mode to mark new seeds
    global current_mode
    current_mode = MODE_NEW_SEED


def set_mode_locate_superpixel():
    # Set the current mode to locate superpixel
    global current_mode
    current_mode = MODE_LOCATE_SUPERPIXEL


def save_coordinates():
    global marked_seeds, superpixel_coords

    # Open a file dialog for saving coordinates with a default .csv extension
    file_path = filedialog.asksaveasfilename(defaultextension=".csv", filetypes=[("CSV files", "*.csv")])
    if file_path:
        with open(file_path, "w") as file:
            # Write headers
            file.write("Type;X;Y\n")
            # Write marked seed coordinates
            for seed in marked_seeds:
                file.write(f"Seed;{seed[0]};{seed[1]}\n")
            # Write located superpixel coordinates
            if superpixel_coords:
                file.write(f"Superpixel;{superpixel_coords[0]};{superpixel_coords[1]}\n")
        output_text.insert(tk.END, "\nCoordinates saved successfully.")


# Create the main application window
root = tk.Tk()
root.title("Image Board")

# Set window size
root.geometry("600x400")

# Frame to hold image and buttons
frame = tk.Frame(root)
frame.pack(fill=tk.BOTH, expand=True)

# Create a frame for the image
image_frame = tk.Frame(frame)
image_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

# Create entry fields for n0 and nf
n0_label = tk.Label(frame, text="n0:")
n0_label.pack()
n0_entry = tk.Entry(frame)
n0_entry.pack()

nf_label = tk.Label(frame, text="nf:")
nf_label.pack()
nf_entry = tk.Entry(frame)
nf_entry.pack()

# Create a button to select an image
select_button = tk.Button(frame, text="Select Image", command=select_image)
select_button.pack()

# Create buttons to set marking modes
seed_button = tk.Button(frame, text="New Seed", command=set_mode_new_seed)
seed_button.pack(side=tk.LEFT)

superpixel_button = tk.Button(frame,
                              text="Locate Superpixel",
                              command=set_mode_locate_superpixel)
superpixel_button.pack(side=tk.LEFT)

# Create an erase button
erase_button = tk.Button(frame, text="Erase", command=erase)
erase_button.pack(side=tk.LEFT)

# Create a button to save coordinates
save_button = tk.Button(frame, text="Save Coordinates", command=save_coordinates)
save_button.pack(side=tk.LEFT)

# Define the output_text widget
output_text = tk.Text(frame, height=10, width=50)
output_text.pack()

# Create a canvas to display the image
canvas = tk.Canvas(image_frame, width=400, height=300)
canvas.pack()

# Variable to store the image drawn on the canvas
canvas_image = None

# Start the Tkinter event loop
root.mainloop()
