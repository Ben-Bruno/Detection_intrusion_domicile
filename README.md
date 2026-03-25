# 🛡️ Sentinel-S3 : Smart Edge Security System

**Sentinel-S3** est un système de vidéosurveillance IoT autonome basé sur un **ESP32-S3**, conçu pour fonctionner en **Edge Computing** avec traitement local, détection d’intrusion et alertes sécurisées en temps réel.

---

## ✨ Fonctionnalités

* 👁️ Capture d’images (UXGA) + traitement local (Edge AI prêt pour reconnaissance faciale avec PSRAM OPI)
* 📱 Alertes Telegram en temps réel avec image (TLS/SSL via WiFiClientSecure)
* 🌐 Double serveur :

  * Port 80 → streaming vidéo (MJPEG)
  * Port 81 → dashboard web de contrôle
* 💾 Stockage SD avec horodatage (NTP)
* 🏃 Détection hybride PIR + logiciel (anti faux positifs)

---

## 🧠 Défis Techniques

* Optimisation mémoire (partition Huge APP 3MB)
* Gestion conflits matériels (PSRAM OPI vs SD en mode 1-bit)
* Multitâche stable (stream + serveur + API sans crash / watchdog)

---

## 🏗️ Architecture

[ PIR ] → [ ESP32-S3 ] ← [ Caméra ]
                  ↑
                 [ NTP ]
                  ↓
[ SD ] ← stockage          → Port 80 (stream)
[ Telegram ] ← alertes    → Port 81 (dashboard)

---

## 🛠️ Matériel

* ESP32-S3 (Freenove recommandé, avec PSRAM)
* Caméra OV2640 / OV3660
* Carte microSD (classe 10)
* Capteur PIR (HC-SR501)

---

## 🚀 Installation

1. Cloner :

   ```bash
   git clone https://github.com/Ben-Bruno/Detection_intrusion_domicile.git
   ```
2. Ouvrir dans Arduino IDE / PlatformIO
3. Configurer :

   * WiFi (SSID / password)
   * Telegram (Bot Token + Chat ID)
4. Paramètres obligatoires :

   * Board : ESP32S3 Dev Module
   * PSRAM : OPI PSRAM
   * Partition : Huge APP (3MB No OTA)
5. Compiler et téléverser

---

## ⚠️ Notes Importantes

* Carte SD requise si stockage activé
* PSRAM obligatoire pour stabilité (sinon crash possible)
* Réduire la résolution si mémoire insuffisante
* Alimentation stable fortement recommandée

---

## 🔮 Roadmap

* Reconnaissance faciale complète
* Batterie LiPo (autonomie)
* Dashboard avancé (React + Laravel + WebSocket)

---

## 📄 Licence

Usage libre pour projets éducatifs et personnels.
