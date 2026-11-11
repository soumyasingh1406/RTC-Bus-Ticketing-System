# RTC Connect – Bus Ticketing System

This is an academic C++ OOP prototype with a browser-based frontend.

## Requirements
- Windows
- VS Code (or another editor)
- MinGW-w64 g++ compiler

## Folder structure
- `backend/server.cpp` – C++ server, OOP classes, route data and booking operations
- `frontend/index.html` – website pages
- `frontend/style.css` – styling and responsive layout
- `frontend/app.js` – frontend interactions and API calls

## Run in VS Code
1. Open the `RTC_Bus_Ticketing_System_Upgraded` folder in VS Code.
2. Open Terminal → New Terminal. Make sure the terminal is at the project root (the folder containing `backend` and `frontend`).
3. Compile:

   ```powershell
   g++ -std=c++17 -Wall -Wextra backend/server.cpp -o rtc_server.exe -lws2_32
   ```

4. Run:

   ```powershell
   .\rtc_server.exe
   ```

5. Open `http://localhost:8080` in Chrome.
6. Keep the terminal window running while using the website. Press `Ctrl+C` in the terminal to stop it.

## Features
- Eight illustrative sample routes, with four bus categories per route
- Route-based search and category filter
- 24 seats per bus, seat availability and selection
- Passenger details and ticket generation
- Ticket lookup and cancellation
- Booking records saved in `bookings_data.txt`; counter saved in `booking_counter.txt`

## OOP concepts
- Abstraction: abstract `Bus` class and pure virtual `calculateFare()`
- Inheritance: `OrdinaryBus`, `ExpressBus`, `SuperLuxuryBus`, `ACBus`
- Polymorphism: overridden fare calculation called through `Bus` objects
- Encapsulation: private data in `Passenger` and `Ticket`
- Composition: `Ticket` contains a `Passenger`
- Controller/management: `BookingSystem` manages buses and tickets

## Important notes
- All routes and bus services are illustrative sample data, not official RTC schedules.
- The project does not connect to live RTC inventory, payment gateways or real-time tracking.
- Booking files are stored in the folder from which the server executable is launched. Run the executable from this project root.
- This lightweight server is intended for a local classroom prototype, not production or public deployment.
