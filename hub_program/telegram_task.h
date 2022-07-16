#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "5593489325:AAHkoog4XIb2hLRbuLtPXWIjIORKMjrfuw4"

// Group/Chat ID
#define CHAT_ID "-658565417"

// Add certificate
X509List xCert(TELEGRAM_CERTIFICATE_ROOT);

WiFiClientSecure xSecuredClient;
UniversalTelegramBot xBot(BOT_TOKEN, xSecuredClient);

void vSetupTelegramBot() {
  xSecuredClient.setTrustAnchors(&xCert);  // Add root certificate for api.telegram.org
  _PP("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    _PP(".");
    delay(100);
    now = time(nullptr);
  }
  _PL(now);
}

void vSendTelegram() {
  if (xDB1.alarm == 1 || xDB2.alarm == 1)
    xBot.sendMessage(CHAT_ID, "Terdapat kebakaran!!", "");
}
