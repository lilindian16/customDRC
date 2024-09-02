var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener("load", onload); // Init web socket when the page loads

const enable_debug = true;
var volume = 0;

/* Define all DOM elements here */
let password_window = document.getElementById("passwordEntrySection");
let main_window = document.getElementById("mainBody");
let quit_button = document.getElementById("quitButton");
let password_submit_button = document.getElementById("submitPasswordButton");

// Range values
let master_volume_range = document.getElementById("masterVolume");
let sub_volume_range = document.getElementById("subVolume");
let balance_range = document.getElementById("balance");
let fader_range = document.getElementById("fader");
/* DOM Elements end */

/* Define Websocket callback functions */
function onOpen(event) {
  // When websocket is established, call the get_remote_settings() function
  console.log("Connection opened");
  get_remote_settings(); // Get the latest settings of the remote
}

function onClose(event) {
  console.log("Connection closed");
  setTimeout(initWebSocket, 2000); // Try to reconnect to WS in 2 seconds
}

// Function that receives the message from the ESP32 with the remote control readings
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
/* End websocket callback functions */

// Define helper functions here
function update_range_display(range_to_update, value) {
  range_to_update.value = value;
  var inner_span_str = range_to_update.id + "Value";
  document.getElementById(inner_span_str).innerHTML = value;
}

// Function to be called when the window is opened
function onload(event) {
  initWebSocket();
  init_range_inputs();
  password_submit_button.addEventListener("click", send_password_auth);
}

function init_range_inputs() {
  master_volume_range.addEventListener("input", on_range_update);
  sub_volume_range.addEventListener("input", on_range_update);
  balance_range.addEventListener("input", on_range_update);
  fader_range.addEventListener("input", on_range_update);

  // Eventually we would update with the latest from the MCU
  update_range_display(master_volume_range, 0);
  update_range_display(sub_volume_range, 0);
  update_range_display(balance_range, 18);
  update_range_display(fader_range, 18);
}

function get_remote_settings() {
  websocket.send("getRemoteSettings");
}

function send_password_auth() {
  var password = document.getElementById("passwordEntry").value;
  var message = `password: ${password}`;
  console.log(message);
  // websocket.send(message)

  // For now just render the next window
  password_window.style.display = "none";
  main_window.style.display = "block";
  quit_button.style.display = "block";
  quit_button.addEventListener("click", on_quit_button_pressed);
}

function on_quit_button_pressed() {
  quit_button.style.display = "none";
  main_window.style.display = "none";
  password_window.style.display = "block";
}

function on_range_update() {
  var slider_value = this.value;
  var message = `${this.id}: ${slider_value}`;
  var dom_value_string = this.id + "Value";
  document.getElementById(dom_value_string).innerHTML = this.value;
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
    update_range_display(master_volume_range, volume);
    console.log(volume);
  }
}
