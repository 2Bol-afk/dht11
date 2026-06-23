#pragma once
// Copy this file to secrets.h and fill in your own values.

// ================= FIREBASE =================
#define WEB_API_KEY   "YOUR_FIREBASE_WEB_API_KEY"
#define DATABASE_URL  "https://YOUR_PROJECT-default-rtdb.firebaseio.com/"
#define USER_EMAIL    "your-firebase-auth-user@example.com"
#define USER_PASS     "your-firebase-auth-password"

// ================= GMAIL SMTP =================
// Gmail account that SENDS the alerts. Enable 2-Step Verification, then create
// a 16-char App Password at https://myaccount.google.com/apppasswords
#define SMTP_SENDER_EMAIL    "your-sender@gmail.com"
#define SMTP_SENDER_NAME     "DHT11 Alert"
#define SMTP_APP_PASSWORD    "xxxx xxxx xxxx xxxx"
