from flask import Flask, Response, render_template, request

app = Flask(__name__)

last_reading = {
    "temp": 0.0,
    "joy_x": 0.0,
    "joy_y": 0.0,
    "btn_a": 0,
    "btn_b": 0,
}


@app.get("/")
def home():
    return render_template("index.html", **last_reading)


@app.post("/update_readings")
def update_readings():
    if not request.json:
        return Response({"detail": "Invalid Request"}, status=400)

    sensors_data = request.json

    last_reading["temp"] = sensors_data["temp"]
    last_reading["joy_x"] = sensors_data["joy_x"]
    last_reading["joy_y"] = sensors_data["joy_y"]
    last_reading["btn_a"] = sensors_data["btn_a"]
    last_reading["btn_b"] = sensors_data["btn_b"]

    return Response({"detail": "Sensor data recieved"}, status=200)


if __name__ == "__main__":
    app.run("0.0.0.0")
