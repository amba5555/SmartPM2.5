# Product Requirements Document: Hyperlocal Air Quality & Health Forecast Platform

**Version:** 1.0  
**Status:** Final  
**Date:** September 26, 2025  
**Author:** Student Innovator Team

## Table of Contents
- [1. Vision & Problem Statement](#1-vision--problem-statement)
- [2. Target Persona](#2-target-persona)
- [3. Product Scope & Features](#3-product-scope--features)
- [4. Success Metrics & KPIs](#4-success-metrics--kpis)
- [5. User Experience & Technical Requirements](#5-user-experience--technical-requirements)
- [6. Long-Term Vision & Future Growth](#6-long-term-vision--future-growth)
- [7. Technical Architecture & Specifications](#7-technical-architecture--specifications)

## 1. Vision & Problem Statement### 1.1. Vision

To empower individuals and communities to monitor their immediate environmental air quality, understand historical trends, and receive actionable forecasts to make informed, proactive decisions about their health and daily activities.

### 1.2. Problem StatementAir quality can vary significantly over short distances and is often a key environmental factor affecting health, especially for vulnerable populations like children and those with respiratory conditions. While government monitoring stations provide regional data, they lack the hyperlocal resolution needed for immediate, personal decision-making. Individuals, schools, and communities need a tool to see their local air quality in real-time and, more importantly, to anticipate poor air quality events before they happen.## 2. Target Persona

The primary user for this platform is the Student Innovator. This profile represents a student or a team of students (High School or University level) who are technically proficient and have developed the sensor hardware.

Their primary motivations are:

* **Community Impact:** To create a genuinely useful service for their school or local community, providing tangible health and safety benefits.
* **Competitive Innovation:** To develop a novel and technically impressive project for science fairs, hackathons, and technology competitions. They are driven by the need to demonstrate a solution that is not just a data display, but an intelligent, data-driven system.3. Product Scope & FeaturesThe project will be developed in two distinct, sequential phases.3.1. Phase 1 (MVP): The Real-Time School DashboardThis phase focuses on establishing the core data pipeline and providing immediate, visible value.Feature 3.1.1: Live AQI Display: A clear, prominent display of the current PM2.5 reading, translated into the official Air Quality Index (AQI) value and corresponding color code (Green, Yellow, Orange, Red, etc.).Feature 3.1.2: Historical Data Visualization: An interactive chart showing PM2.5 trends over the last 24 hours, 7 days, and 30 days.Feature 3.1.3: Simple Health Recommendations: Contextual advice displayed based on the current AQI level (e.g., "Good day for outdoor activities," or "Sensitive groups should reduce prolonged exertion outdoors.").Feature 3.1.4: Multi-Sensor Foundation: The system will be architected to support data intake from multiple sensors, even if only one is deployed initially.3.2. Phase 2: The Predictive Health Alert SystemThis phase builds upon the data collected in Phase 1 to deliver the core innovative feature.Feature 3.2.1: 24-Hour AQI Forecast Model: Develop and deploy a machine learning model that ingests historical sensor data (and potentially external data like weather APIs) to predict the average AQI for the next 24 hours.Feature 3.2.2: Forecast & Confidence Score Display: The main dashboard will display the 24-hour forecast clearly alongside a real-time Confidence Score (e.g., "Model Accuracy: 89%"), which is calculated based on the model's historical performance.Feature 3.2.3: Automated Email/SMS Alerts: An opt-in system for school administrators or designated staff to receive automated alerts when the forecast predicts a high-pollution event (e.g., AQI expected to exceed "Moderate").4. Success Metrics & Key Performance Indicators (KPIs)4.1. Primary Success MetricPredictive Accuracy: The primary measure of the project's success is the quantifiable accuracy of the forecasting model. The target is to achieve and maintain an accuracy of >85% in predicting the correct AQI category for the following day. This metric is critical for competition judging and building user trust.4.2. Secondary MetricsUser Engagement: Track daily/weekly active users of the dashboard.Alert Subscription Rate: The number of staff members who subscribe to the predictive alerts.Verified Impact (Qualitative): Collect testimonials or logs from school staff confirming that they altered schedules or took action based on the platform's data or alerts.5. User Experience & Technical Requirements5.1. Data FreshnessThe dashboard must reflect new data from the sensor in near-real-time, defined as a maximum latency of 10 seconds from sensor transmission to frontend display.5.2. ResponsivenessThe web application must be fully responsive and functional on all common devices, including desktop monitors (for public display), tablets, and mobile phones.5.3. ReliabilityThe entire data pipeline is expected to maintain an uptime of 99.9%. The sensor firmware should include logic for graceful reconnection to the network.5.4. Data IntegrityAll data transmitted from the sensor node must be validated to ensure its integrity. The firmware must implement and verify the sensor's native checksum for every reading. Any reading that fails checksum validation will be discarded and logged as an error to prevent data corruption from affecting the dataset and the accuracy of the predictive model.6. Long-Term Vision & Future GrowthThe long-term ambition for this project is to evolve into a "Total Environment" Monitor (Scale Up). After successfully implementing the core air quality features, the platform will be expanded by:Integrating additional sensors into the hardware nodes (e.g., for noise pollution, UV levels, humidity, and CO2).Developing the web platform to analyze and visualize correlations between these multiple environmental factors, providing a holistic view of the school's environment.## 7. Technical Architecture & Specifications

### 7.1. Data Flow Overview

1. **Capture:** An ESP32 microcontroller with a PMS5003 sensor reads air quality data.
2. **Transmit:** The ESP32 publishes data via MQTT to a HiveMQ Cloud broker.
3. **Process & Store:** A Python application on Render subscribes to the MQTT topic, processes the data, and stores it in a Supabase (PostgreSQL) database.
4. **Display:** A Svelte frontend application, hosted on Vercel, fetches data from Supabase and presents it to the user.

### 7.2. Technology Stack#### Code Management
* GitHub will be used for all source code, enabling a CI/CD workflow.

#### Hardware / Firmware
* **Microcontroller:** ESP32-WROOM-32
* **Sensor:** Plantower PMS5003
* **Display:** OLED Display (128x64, SSD1306 driver)
* **Language:** C++ (Arduino Framework)

#### Firmware Libraries
* `WiFi.h`: Core library for ESP32 WiFi connectivity
* `PubSubClient.h`: Standard client for MQTT communication
* `Wire.h`: For I2C communication with the OLED display
* `Adafruit_GFX.h`: Core graphics library for Adafruit displays
* `Adafruit_SSD1306.h`: Driver for the SSD1306 OLED display
* `SoftwareSerial.h`: To enable serial communication on GPIO pins for the PMS5003 sensor#### Backend
* **Language/Framework:** Python 3.10+ with FastAPI
* **Hosting:** Render (Free Tier)

#### Frontend
* **Framework:** Svelte with SvelteKit
* **Hosting:** Vercel (Hobby Tier)

#### Database
* **Service:** Supabase (Free Tier)
* **Type:** PostgreSQL

#### Real-time Messaging
* **Protocol:** MQTT
* **Broker:** HiveMQ Cloud (Free Tier)