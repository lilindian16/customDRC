var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener("load", onload); // Init web socket when the page loads

const enable_debug = false;
var volume = 0;

/* Define all DOM elements here */
let password_window = document.getElementById("passwordEntrySection");
let main_window = document.getElementById("mainBody");
let quit_button = document.getElementById("quitButton");
let password_submit_button = document.getElementById("submitPasswordButton");
let mute_switch = document.getElementById("mute_switch");
let input_select_radio = document.getElementsByName("InputSelect");

// Range values
let master_volume_range = document.getElementById("masterVolume");
let sub_volume_range = document.getElementById("subVolume");
let balance_range = document.getElementById("balance");
let fader_range = document.getElementById("fader");
/* DOM Elements end */

/* Define global file constants here */
const WEBSOCKET_RECONNECT_WAIT_SECONDS = 2;
const MASTER_VOLUME_STARTING_VALUE = 0;
const MASTER_VOLUME_MAX_VALUE = 120;
const MASTER_VOLUME_MIN_VALUE = 0;
const SUB_VOLUME_STARTING_VALUE = 0;
const SUB_VOLUME_MAX_VALUE = 24;
const SUB_VOLUME_MIN_VALUE = 0;
const BALANCE_STARTING_VALUE = 18;
const BALANCE_MAX_VALUE = 36;
const BALANCE_MIN_VALUE = 0;
const FADER_STARTING_VALUE = 18;
const FADER_MAX_VALUE = 36;
const FADER_MIN_VALUE = 0;
/* End global constant defines

/* Define Websocket callback functions */
function onOpen(event) {
  // When websocket is established, call the get_remote_settings() function
  console.log("Connection opened");
  get_remote_settings(); // Get the latest settings of the remote
}

function onClose(event) {
  console.log("Connection closed");
  setTimeout(initWebSocket, WEBSOCKET_RECONNECT_WAIT_SECONDS * 1000); // Try to reconnect to WS in x seconds
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

function on_mute_switch_change() {
  ws_message = "{mute: ";
  if (mute_switch.checked) {
    ws_message += "1}";
  } else {
    ws_message += "0}";
  }
  websocket.send(ws_message);
  console.log(ws_message);
}

// Function to be called when the window is opened
function onload(event) {
  initWebSocket();
  init_range_inputs();
  password_submit_button.addEventListener("click", send_password_auth);
  mute_switch.addEventListener("change", on_mute_switch_change);

  document.getElementsByName("dspMemory").forEach(function (e) {
    e.addEventListener("click", function () {
      let message = "{dspMemory: " + e.value + "}";
      console.log(message);
      websocket.send(message);
    });
  });

  document.getElementsByName("inputSelect").forEach(function (e) {
    e.addEventListener("click", function () {
      let message = "{inputSelect: " + e.value + "}";
      console.log(message);
      websocket.send(message);
    });
  });
}

function init_range_inputs() {
  master_volume_range.addEventListener("input", on_range_update);
  sub_volume_range.addEventListener("input", on_range_update);
  balance_range.addEventListener("input", on_range_update);
  fader_range.addEventListener("input", on_range_update);

  // Eventually we would update with the latest from the MCU
  update_range_display(master_volume_range, MASTER_VOLUME_STARTING_VALUE);
  update_range_display(sub_volume_range, SUB_VOLUME_STARTING_VALUE);
  update_range_display(balance_range, BALANCE_STARTING_VALUE);
  update_range_display(fader_range, FADER_STARTING_VALUE);
}

function get_remote_settings() {
  websocket.send("{getRemoteSettings: 1}");
}

function send_password_auth() {
  var password = document.getElementById("passwordEntry").value;
  var message = `{password: "${password}"}`;
  console.log(message);
  websocket.send(message);

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
  var message = `{${this.id}: ${slider_value}}`;
  var dom_value_string = this.id + "Value";
  document.getElementById(dom_value_string).innerHTML = this.value;
  console.log(message);
  websocket.send(message); // Now we send this through the WS to the MCU
}

function initWebSocket() {
  console.log("Trying to open a WebSocket connection…");
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;
}
