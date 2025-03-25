from selenium import webdriver
from selenium.webdriver.chrome.options import Options as chrome_options
from selenium.webdriver.firefox.options import Options as firefox_options
from selenium.webdriver.edge.options import Options as edge_options
import os
import sys
import time
import json
import base64

url = "https://testpage"
if len(sys.argv) > 1:
    url += sys.argv[1]
print(url)

target_browser = os.environ["TARGET_BROWSER"]
if target_browser == 'chrome':
    options = chrome_options()
elif target_browser == 'firefox':
    options = firefox_options()
elif target_browser == 'edge':
    options = edge_options()
else:
    print(f"unknown target browser: {target_browser}")
    sys.exit(1)

print(f"target browser: {target_browser}")
MAX_RETRIES = 30  # Retry for up to 30 seconds
WAIT_TIME = 1  # Wait 1 second between retries
start_time = time.time()
while True:
    try:
        driver = webdriver.Remote(command_executor=os.environ["SELENIUM_URL"], options=options)
        print("selenium up and running")
        break
    except Exception as e:
        elapsed_time = time.time() - start_time
        if elapsed_time > MAX_RETRIES:
            print(f"Error: selenium is not available after {MAX_RETRIES} seconds.")
            raise e
        print(f"Waiting for selenium...")
        time.sleep(WAIT_TIME)  # Wait before retrying

driver.get(url)

start_time = time.time()
while True:
    try:
        # Inject JavaScript to capture output in real-time
        driver.execute_script("""
    // Create a hidden <div> to track output
    let outputDiv = document.createElement('div');
    outputDiv.id = 'outputCapture';
    outputDiv.style.display = 'none';
    document.body.appendChild(outputDiv);

    hookOutput((c) => {
        const s = new TextDecoder().decode(new Uint8Array(c))
        outputDiv.textContent += s;  // Append new character
    });
""")
        break
    except Exception as e:
        elapsed_time = time.time() - start_time
        if elapsed_time > MAX_RETRIES:
            print(f"Error: page is not available after {MAX_RETRIES} seconds.")
            raise e
        print(f"Waiting for page...")
        time.sleep(WAIT_TIME)  # Wait before retrying

def send_input(char):
    """Send a character to the web page via `input()`."""
    char_escaped = base64.b64encode(char.encode()).decode()
    driver.execute_script(f"input('{char_escaped}')")

def monitor_output():
    """Continuously monitor the page for new output and print it."""
    last_output = ""
    try:
        while True:
            # Fetch current output from the hidden <div>
            output_text = driver.execute_script("return document.getElementById('outputCapture').textContent;")
            if output_text != last_output:
                new_chars = output_text[len(last_output):]  # Get newly added characters
                sys.stdout.write(new_chars)
                sys.stdout.flush()
                last_output = output_text
            time.sleep(0.05)  # Small delay to avoid excessive CPU usage
    except KeyboardInterrupt:
        print("\nStopping output monitoring.")

user_agent = driver.execute_script("return navigator.userAgent;")
print(f"user agent: {user_agent}")

try:
    print("Type characters to send. Press Ctrl+C to exit.")

    # Start monitoring output in a separate thread
    import threading
    output_thread = threading.Thread(target=monitor_output, daemon=True)
    output_thread.start()

    # Read and send input characters
    while True:
        char = sys.stdin.read(1)  # Read a single character
        if not char:
            break
        send_input(char)

except KeyboardInterrupt:
    print("\nExiting...")

finally:
    print("\nClosing...")
    driver.quit()
