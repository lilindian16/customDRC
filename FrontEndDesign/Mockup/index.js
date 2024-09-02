var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener("load", onload); // Init web socket when the page loads

const enable_debug = true;
var volume = 0;

// Function to be called when the window is opened
function onload(event) {
  initWebSocket();
  init_range_inputs();
  var password_submit_button = document.getElementById("submitPasswordButton");
  password_submit_button.addEventListener("click", send_password_auth);
}

function init_range_inputs() {
  var master_volume_range = document.getElementById("masterVolume");
  master_volume_range.addEventListener("input", on_range_update);

  var sub_volume_range = document.getElementById("subVolume");
  sub_volume_range.addEventListener("input", on_range_update);

  var balance_range = document.getElementById("balance");
  balance_range.addEventListener("input", on_range_update);

  var fader_range = document.getElementById("fader");
  fader_range.addEventListener("input", on_range_update);
}

function get_remote_settings() {
  websocket.send("getRemoteSettings");
}

function send_password_auth() {
  var password = document.getElementById("passwordEntry").value;
  var message = `password: ${password}`;
  console.log(message);
  // websocket.send(message)
}

function on_range_update() {
  var slider_value = this.value;
  var message = `${this.id}: ${slider_value}`;
  console.log(message);
  // websocket.send(message)// Now we send this through the WS to the MCU
}

function initWebSocket() {
  console.log("Trying to open a WebSocket connection…");
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;

  if (enable_debug) {
    volume += 20;
    if (volume > 120) {
      volume = 0;
    }
    let volumeValue = document.getElementById("masterVolume");
    volumeValue.value = volume;
    console.log(volume);
  }
}

// When websocket is established, call the getReadings() function
function onOpen(event) {
  console.log("Connection opened");
  get_remote_settings(); // Get the latest settings of the remote
}

function onClose(event) {
  console.log("Connection closed");
  setTimeout(initWebSocket, 2000);
}

// Function that receives the message from the ESP32 with the readings
// JSON keys are the same as the element ID
function onMessage(event) {
  console.log(event.data);
  var myObj = JSON.parse(event.data);
  var keys = Object.keys(myObj);

  for (var i = 0; i < keys.length; i++) {
    var key = keys[i];
    document.getElementById(key).innerHTML = myObj[key];
  }
}
