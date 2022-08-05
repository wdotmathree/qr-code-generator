from flask import Flask, request
import qr
import importlib

app = Flask(__name__)


@app.route('/ws', methods=['POST'])
def send_qr():
    try:
        importlib.reload(qr)
        return qr.generate_qr(*request.get_json(True).values())
    except ValueError:
        return "Invalid version"


app.run(port=3001)
