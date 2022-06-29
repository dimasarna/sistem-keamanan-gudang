// Helper function to convert IP address to string
String IPToString(IPAddress ip) {
  String s = "";
  for (int i = 0; i < 4; i++) {
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  }
  return s;
};

void sendIPToBlynk() {
  String url = "http://" + IPToString(WiFi.localIP());
  Blynk.setProperty(V1, "url", url.c_str());
};

// Send IP address after connected to blynk
BLYNK_CONNECTED() {
  Blynk.syncAll();
  sendIPToBlynk();
}

BLYNK_APP_CONNECTED() {
  sendIPToBlynk();
}
