var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener("load", onload); // Init web socket when the page loads

var volume = 0;

/* Define all DOM elements here */
let password_window = document.getElementById("passwordEntrySection");
let main_window = document.getElementById("mainBody");
let quit_button = document.getElementById("quitButton");
let mute_switch = document.getElementById("mute_switch");
let input_select_radio = document.getElementsByName("InputSelect");
let current_source_label = document.getElementById("currentSourceLabel");
let change_source_button = document.getElementById("changeSourceButton");
let dsp_memory_a_radio = document.getElementById("DSPMemA");
let dsp_memory_b_radio = document.getElementById("DSPMemB");
let updateWifiCredentialsForm = document.getElementById(
  "credentialsUpdateForm"
);
let credentialsUpdateButton = document.getElementById(
  "credentialsUpdateButton"
);
let credentialsModal = document.getElementById("confirmCredentialsModal");
let ssidInput = document.getElementById("wifiCredentailsSSID");
let passwordInput = document.getElementById("wifiCredentailsPassword");
let confirmCredentialsUpdateButton = document.getElementById(
  "confirmCredentialsUpdateButton"
);

let ota_file_upload_button = document.getElementById("upload_ota_file_button");
let ota_file_upload_progress_bar = document.getElementById(
  "ota_file_upload_progress"
);

// Range values
let master_volume_range = document.getElementById("masterVolume");
let sub_volume_range = document.getElementById("subVolume");
let balance_range = document.getElementById("balance");
let fader_range = document.getElementById("fader");

// Link Bus connection
let link_bus_connection_display = document.getElementById("busConnection");
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
    console.log(key[i]);
    if (key == "dspMemory") {
      let mem = myObj[key];
      if (mem == 0) {
        dsp_memory_a_radio.checked = true;
      } else if (mem == 1) {
        dsp_memory_b_radio.checked = true;
      }
      console.log("dspMemory: ", mem);
    } else if (key == "currentSource") {
      let currentSource = myObj[key];
      console.log("currentSource: ", currentSource);
      current_source_label.innerHTML = currentSource;
    }
    // else if (key == "mute") {
    //   var muted = myObj[key];
    //   mute_switch.checked = Boolean(muted);
    //   console.log("Mute: ", mute_switch.checked);
    // }
    else if (key == "masterVolume") {
      update_range_display(master_volume_range, myObj[key]);
    } else if (key == "fader") {
      update_range_display(fader_range, myObj[key]);
    } else if (key == "balance") {
      update_range_display(balance_range, myObj[key]);
    } else if (key == "subVolume") {
      update_range_display(sub_volume_range, myObj[key]);
    } else if (key == "usbConnected") {
      var bus_connected = Boolean(myObj[key]);
      if (bus_connected == true) {
        link_bus_connection_display.innerHTML = "<del>Link Bus Active</del>";
      } else {
        link_bus_connection_display.innerHTML = "<ins>Link Bus Active</ins>";
      }
    } else {
      console.log("Unknown websocket JSON key");
    }
  }
}
/* End websocket callback functions */

// Define helper functions here
function update_range_display(range_to_update, value) {
  range_to_update.value = value;
  var inner_span_str = range_to_update.id + "Value";
  document.getElementById(inner_span_str).innerHTML = value;
  return false;
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

function on_ota_file_upload_button_clicked(event) {
  // We have to intercept the form submit action to avoid page refresh
  let selected_file = document.getElementById("ota_file");
  var file_name = selected_file.files[0].name;
  console.log(file_name);
  var file1Size = selected_file.files[0].size;
  console.log(`OTA file size: ${file1Size}`);

  if (file_name.includes(".bin")) {
    console.log("We have a binary file I believe");
    var url = "/upload";
    var request = new XMLHttpRequest();
    request.open("POST", url, true);
    request.onload = function () {};
    request.onerror = function () {
      console.log("Failed to create the /upload request");
    };
    request.upload.addEventListener("progress", function (e) {
      if (e.loaded <= file1Size) {
        var percent = Math.round((e.loaded / file1Size) * 100);
        ota_file_upload_progress_bar.value = percent;
      }
      if (e.loaded == e.total) {
        ota_file_upload_progress_bar.value = 100;
      }
    });
    request.send(new FormData(event.target)); // create FormData from form that triggered event
    ota_file_upload_progress_bar.style.display = "block";
  } else {
    console.log("We do not have a binary file");
  }
  event.preventDefault();
}

function on_credeitals_form_input(event) {
  let password = passwordInput.value;
  if (password.length >= 8) {
    passwordInput.setAttribute("aria-invalid", "false");
    credentialsUpdateButton.disabled = false;
  } else {
    passwordInput.setAttribute("aria-invalid", "true");
    credentialsUpdateButton.disabled = true;
  }
}

function on_change_source_button_clicked(event) {
  ws_message = "{changeSource: 1}";
  websocket.send(ws_message);
  console.log(ws_message);
}

// Function to be called when the window is opened
function onload(event) {
  initWebSocket();
  init_range_inputs();
  // mute_switch.addEventListener("change", on_mute_switch_change);
  change_source_button.addEventListener(
    "click",
    on_change_source_button_clicked
  );
  document
    .getElementById("ota_form")
    .addEventListener("submit", on_ota_file_upload_button_clicked);

  updateWifiCredentialsForm.addEventListener("input", on_credeitals_form_input);

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
  let message = "{getRemoteSettings: 1}";
  console.log(message);
  websocket.send(message);
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

/*
 * Modal
 *
 * Pico.css - https://picocss.com
 * Copyright 2019-2024 - Licensed under MIT
 */

// Config
const isOpenClass = "modal-is-open";
const openingClass = "modal-is-opening";
const closingClass = "modal-is-closing";
const scrollbarWidthCssVar = "--pico-scrollbar-width";
const animationDuration = 400; // ms
let visibleModal = null;

// Toggle modal
const toggleModal = (event) => {
  event.preventDefault();
  const modal = document.getElementById(event.currentTarget.dataset.target);
  if (!modal) return;
  modal && (modal.open ? closeModal(modal) : openModal(modal));
};

// Open modal
const openModal = (modal) => {
  const { documentElement: html } = document;
  const scrollbarWidth = getScrollbarWidth();
  if (scrollbarWidth) {
    html.style.setProperty(scrollbarWidthCssVar, `${scrollbarWidth}px`);
  }
  html.classList.add(isOpenClass, openingClass);
  setTimeout(() => {
    visibleModal = modal;
    html.classList.remove(openingClass);
  }, animationDuration);
  modal.showModal();
};

// Close modal
const closeModal = (modal) => {
  visibleModal = null;
  const { documentElement: html } = document;
  html.classList.add(closingClass);
  setTimeout(() => {
    html.classList.remove(closingClass, isOpenClass);
    html.style.removeProperty(scrollbarWidthCssVar);
    modal.close();
  }, animationDuration);
};

confirmCredentialsUpdateButton.addEventListener("click", (event) => {
  event.preventDefault();
  let ssid = ssidInput.value;
  let password = passwordInput.value;
  if (password.length >= 8) {
    console.log(`SSID: ${ssid}\tPassword: ${password}`);
    let ws_message = `{\n"ssid": "${ssid}",\n"password": "${password}"\n}`;
    console.log(ws_message);
  } else {
    console.error("Password must be 8 or more characters");
  }
});

// Close with a click outside
document.addEventListener("click", (event) => {
  if (visibleModal === null) return;
  const modalContent = visibleModal.querySelector("article");
  const isClickInside = modalContent.contains(event.target);
  !isClickInside && closeModal(visibleModal);
});

// Close with Esc key
document.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && visibleModal) {
    closeModal(visibleModal);
  }
});

// Get scrollbar width
const getScrollbarWidth = () => {
  const scrollbarWidth =
    window.innerWidth - document.documentElement.clientWidth;
  return scrollbarWidth;
};

// Is scrollbar visible
const isScrollbarVisible = () => {
  return document.body.scrollHeight > screen.height;
};
