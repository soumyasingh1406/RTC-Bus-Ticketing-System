#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <iomanip>
#include <cctype>
#include <map>
#pragma comment(lib, "Ws2_32.lib")
using namespace std;

// Small helpers for this project’s lightweight JSON and URL handling.
string jsonEscape(const string& s) {
    string out;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') { out += "\\n"; continue; }
        if (c == '\r') continue;
        out += (char)c;
    }
    return out;
}
string getString(const string& s, const string& key) {
    string token = "\"" + key + "\"";
    size_t p = s.find(token);
    if (p == string::npos) return "";
    p = s.find(':', p);
    if (p == string::npos) return "";
    p = s.find('"', p);
    if (p == string::npos) return "";
    size_t e = s.find('"', p + 1);
    return e == string::npos ? "" : s.substr(p + 1, e - p - 1);
}
int getInt(const string& s, const string& key) {
    string token = "\"" + key + "\"";
    size_t p = s.find(token);
    if (p == string::npos) return -1;
    p = s.find(':', p);
    if (p == string::npos) return -1;
    try { return stoi(s.substr(p + 1)); } catch (...) { return -1; }
}
string urlDecode(const string& text) {
    string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '+') out += ' ';
        else if (text[i] == '%' && i + 2 < text.size()) {
            try { out += (char)stoi(text.substr(i + 1, 2), nullptr, 16); i += 2; }
            catch (...) { out += text[i]; }
        } else out += text[i];
    }
    return out;
}
string queryValue(const string& target, const string& key) {
    size_t p = target.find('?');
    if (p == string::npos) return "";
    string q = target.substr(p + 1);
    string wanted = key + "=";
    p = q.find(wanted);
    if (p == string::npos) return "";
    p += wanted.size();
    size_t e = q.find('&', p);
    return urlDecode(q.substr(p, e == string::npos ? string::npos : e - p));
}
string lowerText(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
    return s;
}

class Bus {
protected:
    string busNo, from, to, category;
    double km;
    vector<bool> seats;
public:
    Bus(string n, string f, string t, string c, double d)
        : busNo(n), from(f), to(t), category(c), km(d), seats(24, false) {}
    virtual double calculateFare() const = 0;
    virtual ~Bus() = default;
    string number() const { return busNo; }
    string type() const { return category; }
    string source() const { return from; }
    string destination() const { return to; }
    double distance() const { return km; }
    int availableSeats() const { return (int)count(seats.begin(), seats.end(), false); }
    bool isSeatAvailable(int s) const { return s >= 1 && s <= 24 && !seats[s - 1]; }
    bool bookSeat(int s) {
        if (!isSeatAvailable(s)) return false;
        seats[s - 1] = true;
        return true;
    }
    void releaseSeat(int s) { if (s >= 1 && s <= 24) seats[s - 1] = false; }
    void restoreSeat(int s) { if (s >= 1 && s <= 24) seats[s - 1] = true; }
    string seatJSON() const {
        string out = "[";
        for (int i = 0; i < 24; ++i) {
            if (i) out += ",";
            out += seats[i] ? "true" : "false";
        }
        return out + "]";
    }
};

class OrdinaryBus : public Bus {
public:
    OrdinaryBus(string n, string f, string t, double d) : Bus(n, f, t, "Ordinary", d) {}
    double calculateFare() const override { return km * 0.80; }
};
class ExpressBus : public Bus {
public:
    ExpressBus(string n, string f, string t, double d) : Bus(n, f, t, "Express", d) {}
    double calculateFare() const override { return km * 1.10; }
};
class SuperLuxuryBus : public Bus {
public:
    SuperLuxuryBus(string n, string f, string t, double d) : Bus(n, f, t, "Super Luxury", d) {}
    double calculateFare() const override { return km * 1.40; }
};
class ACBus : public Bus {
public:
    ACBus(string n, string f, string t, double d) : Bus(n, f, t, "AC", d) {}
    double calculateFare() const override { return km * 1.80; }
};

class Passenger {
    string name, phone;
public:
    Passenger(string n = "", string p = "") : name(n), phone(p) {}
    string getName() const { return name; }
    string getPhone() const { return phone; }
};

class Ticket {
    string id;
    Passenger passenger;
    Bus* bus;
    int seatNo;
    double fare;
    string journeyDate;
    string status;
public:
    Ticket(string i, Passenger p, Bus* b, int s, double f, string date = "")
        : id(i), passenger(p), bus(b), seatNo(s), fare(f), journeyDate(date), status("CONFIRMED") {}
    string getID() const { return id; }
    Bus* getBus() const { return bus; }
    int getSeat() const { return seatNo; }
    string getStatus() const { return status; }
    void cancel() { status = "CANCELLED"; }
    string toJSON() const {
        ostringstream o;
        o << fixed << setprecision(2)
          << "{\"bookingID\":\"" << jsonEscape(id)
          << "\",\"name\":\"" << jsonEscape(passenger.getName())
          << "\",\"phone\":\"" << jsonEscape(passenger.getPhone())
          << "\",\"busNumber\":\"" << bus->number()
          << "\",\"category\":\"" << bus->type()
          << "\",\"source\":\"" << bus->source()
          << "\",\"destination\":\"" << bus->destination()
          << "\",\"distance\":" << bus->distance()
          << ",\"seat\":" << seatNo << ",\"fare\":" << fare
          << ",\"journeyDate\":\"" << jsonEscape(journeyDate)
          << "\",\"status\":\"" << status << "\"}";
        return o.str();
    }
};

struct RouteInfo { string from, to; double km; };

class BookingSystem {
    vector<unique_ptr<Bus>> buses;
    vector<Ticket> tickets;
    int nextBooking = 1042;

    Bus* findBus(const string& number) {
        for (auto& b : buses) if (b->number() == number) return b.get();
        return nullptr;
    }
    Ticket* findTicket(const string& id) {
        for (auto& t : tickets) if (t.getID() == id) return &t;
        return nullptr;
    }
    void addRoute(const RouteInfo& r, int& serial) {
        buses.push_back(make_unique<OrdinaryBus>("RTC" + to_string(serial++), r.from, r.to, r.km));
        buses.push_back(make_unique<ExpressBus>("RTC" + to_string(serial++), r.from, r.to, r.km));
        buses.push_back(make_unique<SuperLuxuryBus>("RTC" + to_string(serial++), r.from, r.to, r.km));
        buses.push_back(make_unique<ACBus>("RTC" + to_string(serial++), r.from, r.to, r.km));
    }
    void save() {
        ofstream f("bookings_data.txt");
        for (const auto& t : tickets) f << t.toJSON() << '\n';
        ofstream c("booking_counter.txt");
        c << nextBooking;
    }
    void load() {
        ifstream c("booking_counter.txt");
        if (c) c >> nextBooking;
        ifstream f("bookings_data.txt");
        string line;
        while (getline(f, line)) {
            string id = getString(line, "bookingID");
            string name = getString(line, "name");
            string phone = getString(line, "phone");
            string busNo = getString(line, "busNumber");
            string status = getString(line, "status");
            string journeyDate = getString(line, "journeyDate");
            int seatNo = getInt(line, "seat");
            Bus* b = findBus(busNo);
            if (!b || id.empty() || seatNo < 1 || seatNo > 24) continue;
            double fare = 0;
            size_t p = line.find("\"fare\":");
            if (p != string::npos) { try { fare = stod(line.substr(p + 7)); } catch (...) {} }
            Ticket t(id, Passenger(name, phone), b, seatNo, fare, journeyDate);
            if (status == "CANCELLED") t.cancel();
            else b->restoreSeat(seatNo);
            tickets.push_back(t);
        }
    }
public:
    BookingSystem() {
        // Sample routes for the academic prototype. These are not live RTC schedules.
        vector<RouteInfo> routes = {
            {"Vijayawada", "Hyderabad", 300},
            {"Guntur", "Hyderabad", 320},
            {"Vijayawada", "Visakhapatnam", 350},
            {"Guntur", "Vijayawada", 35},
            {"Hyderabad", "Tirupati", 560},
            {"Vijayawada", "Tirupati", 410},
            {"Warangal", "Hyderabad", 150},
            {"Guntur", "Visakhapatnam", 385}
        };
        int serial = 101;
        for (const auto& r : routes) addRoute(r, serial);
        load();
    }
    string busList(const string& from, const string& to) {
        string out = "[";
        bool first = true;
        for (const auto& b : buses) {
            bool matchesFrom = from.empty() || lowerText(b->source()) == lowerText(from);
            bool matchesTo = to.empty() || lowerText(b->destination()) == lowerText(to);
            if (!matchesFrom || !matchesTo) continue;
            if (!first) out += ",";
            first = false;
            ostringstream o;
            o << fixed << setprecision(2)
              << "{\"busNumber\":\"" << b->number()
              << "\",\"category\":\"" << b->type()
              << "\",\"source\":\"" << b->source()
              << "\",\"destination\":\"" << b->destination()
              << "\",\"distance\":" << b->distance()
              << ",\"fare\":" << b->calculateFare()
              << ",\"availableSeats\":" << b->availableSeats() << "}";
            out += o.str();
        }
        return out + "]";
    }
    string seatList(const string& number) {
        Bus* b = findBus(number);
        if (!b) return "{\"error\":\"Bus not found\"}";
        return "{\"seats\":" + b->seatJSON() + "}";
    }
    string book(const string& body) {
        string busNo = getString(body, "busNumber");
        string name = getString(body, "name");
        string phone = getString(body, "phone");
        string journeyDate = getString(body, "journeyDate");
        int seatNo = getInt(body, "seat");
        Bus* b = findBus(busNo);
        if (!b) return "{\"error\":\"Bus not found\"}";
        if (name.empty() || journeyDate.empty() || phone.size() != 10 ||
            !all_of(phone.begin(), phone.end(), [](unsigned char c) { return isdigit(c); }))
            return "{\"error\":\"Enter a passenger name and a valid 10-digit phone number\"}";
        if (!b->bookSeat(seatNo)) return "{\"error\":\"That seat is not available. Please select another seat.\"}";
        Ticket t("RTC-" + to_string(nextBooking++), Passenger(name, phone), b, seatNo, b->calculateFare(), journeyDate);
        tickets.push_back(t);
        save();
        return t.toJSON();
    }
    string ticket(const string& id) {
        Ticket* t = findTicket(id);
        return t ? t->toJSON() : "{\"error\":\"Booking not found. Check the Booking ID.\"}";
    }
    string cancel(const string& body) {
        string id = getString(body, "bookingID");
        Ticket* t = findTicket(id);
        if (!t) return "{\"error\":\"Booking not found. Check the Booking ID.\"}";
        if (t->getStatus() == "CANCELLED") return "{\"error\":\"This ticket is already cancelled.\"}";
        t->getBus()->releaseSeat(t->getSeat());
        t->cancel();
        save();
        return t->toJSON();
    }
};

void sendResponse(SOCKET client, int code, const string& contentType, const string& body) {
    string status = code == 200 ? "200 OK" : code == 404 ? "404 Not Found" : "400 Bad Request";
    string header = "HTTP/1.1 " + status + "\r\nContent-Type: " + contentType +
        "; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\nContent-Length: " + to_string(body.size()) +
        "\r\nConnection: close\r\n\r\n";
    string response = header + body;
    send(client, response.c_str(), (int)response.size(), 0);
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "Could not initialise Windows networking.\n";
        return 1;
    }
    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    address.sin_addr.s_addr = INADDR_ANY;
    if (server == INVALID_SOCKET || bind(server, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR ||
        listen(server, 10) == SOCKET_ERROR) {
        cout << "Port 8080 is busy. Stop the older server window and try again.\n";
        if (server != INVALID_SOCKET) closesocket(server);
        WSACleanup();
        return 1;
    }

    BookingSystem app;
    cout << "RTC Bus Ticketing System is running.\n";
    cout << "Open http://localhost:8080 in your browser. Keep this window open.\n";

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        char buffer[16384]{};
        int received = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) { closesocket(client); continue; }

        string request(buffer, received);
        size_t lineEnd = request.find("\r\n");
        size_t headerEnd = request.find("\r\n\r\n");
        istringstream firstLine(request.substr(0, lineEnd));
        string method, path, version;
        firstLine >> method >> path >> version;
        string body = headerEnd == string::npos ? "" : request.substr(headerEnd + 4);

        if (method == "OPTIONS") {
            sendResponse(client, 200, "text/plain", "");
        } else {
            string output, contentType = "application/json";
            int status = 200;
            if (path.find("/api/buses") == 0) {
                output = app.busList(queryValue(path, "from"), queryValue(path, "to"));
            } else if (path.find("/api/seats") == 0) {
                output = app.seatList(queryValue(path, "bus"));
            } else if (path == "/api/book") {
                output = app.book(body);
            } else if (path.find("/api/ticket") == 0) {
                output = app.ticket(queryValue(path, "id"));
            } else if (path == "/api/cancel") {
                output = app.cancel(body);
            } else {
                string filePath = path == "/" ? "frontend/index.html" : "frontend" + path;
                output = [&]() {
                    ifstream file(filePath, ios::binary);
                    if (!file) return string();
                    stringstream data; data << file.rdbuf(); return data.str();
                }();
                if (filePath.size() >= 4 && filePath.substr(filePath.size() - 4) == ".css") contentType = "text/css";
                else if (filePath.size() >= 3 && filePath.substr(filePath.size() - 3) == ".js") contentType = "application/javascript";
                else contentType = "text/html";
                if (output.empty()) { output = "File not found. Start the server from the project root folder."; status = 404; contentType = "text/plain"; }
            }
            if (path.rfind("/api/", 0) == 0 && output.find("\"error\"") != string::npos) status = 400;
            sendResponse(client, status, contentType, output);
        }
        closesocket(client);
    }
    closesocket(server);
    WSACleanup();
    return 0;
}
