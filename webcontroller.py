import http.server
import socket
import urllib.parse
import signal
import subprocess
import os

hostname = socket.gethostname()

startPage = f"""
<!DOCTYPE html>
<html>
<body>
<form action="/start" method="POST">
    <label for="ip">LiveSplit Server IP:</label><br>
    <input type="text" id="ip" name="ip"><br>
    <label for="port">LiveSplit Server Port:</label><br>
    <input type="text" id="port" name="port"><br>
    <input type="submit" value="Start">
</form>
</body>
</html>
"""

stopPage = f"""
<!DOCTYPE html>
<html>
<body>
Display is running.
<form action="/stop" method="POST">
    <input type="submit" value="Stop">
</form>
</body>
</html>
"""

running = False
displayProcess = None

dir = os.path.dirname(os.path.realpath(__file__))

class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.end_headers()
        if running:
            self.wfile.write(stopPage.encode("utf-8"))
        else:
            self.wfile.write(startPage.encode("utf-8"))

    def do_POST(self):
        global running
        global displayProcess
        content_length = int(self.headers["Content-Length"])
        post_data = self.rfile.read(content_length).decode("utf-8")
        data = urllib.parse.parse_qs(post_data)

        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.end_headers()

        if running:
            displayProcess.send_signal(signal.SIGINT)
            self.wfile.write(startPage.encode("utf-8"))
        else:
            ip = data["ip"][0]
            port = data["port"][0]            

            displayProcess = subprocess.Popen([dir + "/timer-display", ip, port])

            running = True
            self.wfile.write(stopPage.encode("utf-8"))

server = http.server.HTTPServer(('', 80), Handler)
server.serve_forever()

