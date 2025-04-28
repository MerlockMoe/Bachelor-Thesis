from flask import Flask, request

app = Flask(__name__)

@app.route('/upload', methods=['POST'])
def upload():
    img = request.data
    with open('/tmp/esp_image.jpg', 'wb') as f:
        f.write(img)
    print("Bild empfangen und gespeichert")
    return 'OK', 200

if __name__ == '__main__':
    # Lauscht auf allen Interfaces, Port 5000
    app.run(host='0.0.0.0', port=5000)
