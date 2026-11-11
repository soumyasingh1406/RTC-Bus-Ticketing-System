#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <iomanip>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

// JSON helpers
string jsonEscape(const string& s) {
    string out;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') { out += "\\n"; continue; }
        if (c == '\r') continue;
        out += static_cast<char>(c);
    }
    return out;
}

string getString(const string& s, const string& key) {
    string token = "\"" + key + "\"";
    size_t p = s.find(token);
    if (p == string::npos) return "";

    p = s.find(':', p + token.size());
    if (p == string::npos) return "";

    p = s.find('"', p + 1);
    if (p == string::npos) return "";

    size_t e = p + 1;
    while (e < s.size()) {
        if (s[e] == '"' && s[e - 1] != '\\') break;
        ++e;
    }
    if (e >= s.size()) return "";

    string value = s.substr(p + 1, e - p - 1);
    string decoded;

    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            ++i;
            if (value[i] == 'n') decoded += '\n';
            else if (value[i] == 'r') decoded += '\r';
            else decoded += value[i];
        } else {
            decoded += value[i];
        }
    }
    return decoded;
}

int getInt(const string& s, const string& key) {
    string token = "\"" + key + "\"";
    size_t p = s.find(token);
    if (p == string::npos) return -1;

    p = s.find(':', p + token.size());
    if (p == string::npos) return -1;

    try {
        return stoi(s.substr(p + 1));
    } catch (...) {
        return -1;
    }
}

string urlDecode(const string& text) {
    string out;

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '+') {
            out += ' ';
        } else if (text[i] == '%' && i + 2 < text.size()) {
            try {
                out += static_cast<char>(
                    stoi(text.substr(i + 1, 2), nullptr, 16));
                i += 2;
            } catch (...) {
                out += text[i];
            }
        } else {
            out += text[i];
        }
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
    if (p > 0 && q[p - 1] != '&') return "";

    p += wanted.size();
    size_t e = q.find('&', p);

    return urlDecode(
        q.substr(p, e == string::npos ? string::npos : e - p));
}

string lowerText(string s) {
    transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) {
            return static_cast<char>(tolower(c));
        });
    return s;
}

// BUS CLASS
class Bus {
protected:
    string busNo, from, to, category;
    double km;
    vector<bool> seats;

public:
    Bus(string n, string f, string t, string c, double d)
        : busNo(n), from(f), to(t), category(c),
          km(d), seats(24, false) {}

    virtual double calculateFare() const = 0;
    virtual ~Bus() = default;

    string number() const { return busNo; }
    string type() const { return category; }
    string source() const { return from; }
    string destination() const { return to; }
    double distance() const { return km; }

    int availableSeats() const {
        return static_cast<int>(
            count(seats.begin(), seats.end(), false));
    }

    bool isSeatAvailable(int s) const {
        return s >= 1 && s <= 24 && !seats[s - 1];
    }

    bool bookSeat(int s) {
        if (!isSeatAvailable(s)) return false;
        seats[s - 1] = true;
        return true;
    }

    void releaseSeat(int s) {
        if (s >= 1 && s <= 24)
            seats[s - 1] = false;
    }

    void restoreSeat(int s) {
        if (s >= 1 && s <= 24)
            seats[s - 1] = true;
    }

    string seatJSON() const {
        string out = "[";

        for (int i = 0; i < 24; i++) {
            if (i) out += ",";
            out += seats[i] ? "true" : "false";
        }

        return out + "]";
    }
};

// INHERITANCE
class OrdinaryBus : public Bus {
public:
    OrdinaryBus(string n, string f, string t, double d)
        : Bus(n, f, t, "Ordinary", d) {}

    double calculateFare() const override {
        return km * 0.80;
    }
};

class ExpressBus : public Bus {
public:
    ExpressBus(string n, string f, string t, double d)
        : Bus(n, f, t, "Express", d) {}

    double calculateFare() const override {
        return km * 1.10;
    }
};

class SuperLuxuryBus : public Bus {
public:
    SuperLuxuryBus(string n, string f, string t, double d)
        : Bus(n, f, t, "Super Luxury", d) {}

    double calculateFare() const override {
        return km * 1.40;
    }
};

class ACBus : public Bus {
public:
    ACBus(string n, string f, string t, double d)
        : Bus(n, f, t, "AC", d) {}

    double calculateFare() const override {
        return km * 1.80;
    }
};

// PASSENGER CLASS
class Passenger {
    string name, phone;

public:
    Passenger(string n = "", string p = "")
        : name(n), phone(p) {}

    string getName() const { return name; }
    string getPhone() const { return phone; }
};

// TICKET CLASS
class Ticket {
    string id;
    Passenger passenger;
    Bus* bus;
    int seatNo;
    double fare;
    string journeyDate;
    string status;

public:
    Ticket(string i, Passenger p, Bus* b, int s,
           double f, string date = "")
        : id(i), passenger(p), bus(b), seatNo(s),
          fare(f), journeyDate(date), status("CONFIRMED") {}

    string getID() const { return id; }
    Bus* getBus() const { return bus; }
    int getSeat() const { return seatNo; }
    string getStatus() const { return status; }

    void cancel() {
        status = "CANCELLED";
    }

    string toJSON() const {
        ostringstream o;

        o << fixed << setprecision(2)
          << "{\"bookingID\":\"" << jsonEscape(id)
          << "\",\"name\":\"" << jsonEscape(passenger.getName())
          << "\",\"phone\":\"" << jsonEscape(passenger.getPhone())
          << "\",\"busNumber\":\"" << jsonEscape(bus->number())
          << "\",\"category\":\"" << jsonEscape(bus->type())
          << "\",\"source\":\"" << jsonEscape(bus->source())
          << "\",\"destination\":\"" << jsonEscape(bus->destination())
          << "\",\"distance\":" << bus->distance()
          << ",\"seat\":" << seatNo
          << ",\"fare\":" << fare
          << ",\"journeyDate\":\"" << jsonEscape(journeyDate)
          << "\",\"status\":\"" << status << "\"}";

        return o.str();
    }
};

// BOOKING SYSTEM
struct RouteInfo {
    string from, to;
    double km;
};

class BookingSystem {
    vector<unique_ptr<Bus>> buses;
    vector<Ticket> tickets;
    int nextBooking = 1042;

    Bus* findBus(const string& number) {
        for (auto& b : buses) {
            if (b->number() == number)
                return b.get();
        }
        return nullptr;
    }

    Ticket* findTicket(const string& id) {
        for (auto& t : tickets) {
            if (t.getID() == id)
                return &t;
        }
        return nullptr;
    }

    void addRoute(const RouteInfo& r, int& serial) {
        buses.push_back(make_unique<OrdinaryBus>(
            "RTC" + to_string(serial++), r.from, r.to, r.km));

        buses.push_back(make_unique<ExpressBus>(
            "RTC" + to_string(serial++), r.from, r.to, r.km));

        buses.push_back(make_unique<SuperLuxuryBus>(
            "RTC" + to_string(serial++), r.from, r.to, r.km));

        buses.push_back(make_unique<ACBus>(
            "RTC" + to_string(serial++), r.from, r.to, r.km));
    }

    void save() {
        ofstream f("bookings_data.txt");

        for (const auto& t : tickets)
            f << t.toJSON() << '\n';

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

            if (!b || id.empty() || seatNo < 1 || seatNo > 24)
                continue;

            double fare = 0;
            size_t p = line.find("\"fare\":");

            if (p != string::npos) {
                try {
                    fare = stod(line.substr(p + 7));
                } catch (...) {}
            }

            Ticket t(id, Passenger(name, phone), b,
                     seatNo, fare, journeyDate);

            if (status == "CANCELLED")
                t.cancel();
            else
                b->restoreSeat(seatNo);

            tickets.push_back(t);
        }
    }

public:
    BookingSystem() {
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

        for (const auto& r : routes)
            addRoute(r, serial);

        load();
    }

    string busList(const string& from, const string& to) {
        string out = "[";
        bool first = true;

        for (const auto& b : buses) {
            bool matchesFrom = from.empty() ||
                lowerText(b->source()) == lowerText(from);

            bool matchesTo = to.empty() ||
                lowerText(b->destination()) == lowerText(to);

            if (!matchesFrom || !matchesTo)
                continue;

            if (!first) out += ",";
            first = false;

            ostringstream o;

            o << fixed << setprecision(2)
              << "{\"busNumber\":\"" << jsonEscape(b->number())
              << "\",\"category\":\"" << jsonEscape(b->type())
              << "\",\"source\":\"" << jsonEscape(b->source())
              << "\",\"destination\":\"" << jsonEscape(b->destination())
              << "\",\"distance\":" << b->distance()
              << ",\"fare\":" << b->calculateFare()
              << ",\"availableSeats\":" << b->availableSeats() << "}";

            out += o.str();
        }

        return out + "]";
    }

    string seatList(const string& number) {
        Bus* b = findBus(number);

        if (!b)
            return "{\"error\":\"Bus not found\"}";

        return "{\"seats\":" + b->seatJSON() + "}";
    }

    string book(const string& body) {
        string busNo = getString(body, "busNumber");
        string name = getString(body, "name");
        string phone = getString(body, "phone");
        string journeyDate = getString(body, "journeyDate");

        int seatNo = getInt(body, "seat");
        Bus* b = findBus(busNo);

        if (!b)
            return "{\"error\":\"Bus not found\"}";

        if (name.empty() || journeyDate.empty() ||
            phone.size() != 10 ||
            !all_of(phone.begin(), phone.end(),
                [](unsigned char c) { return isdigit(c); })) {
            return "{\"error\":\"Enter a passenger name and a valid 10-digit phone number\"}";
        }

        if (!b->bookSeat(seatNo)) {
            return "{\"error\":\"That seat is not available. Please select another seat.\"}";
        }

        Ticket t("RTC-" + to_string(nextBooking++),
                 Passenger(name, phone), b, seatNo,
                 b->calculateFare(), journeyDate);

        tickets.push_back(t);
        save();

        return t.toJSON();
    }

    string ticket(const string& id) {
        Ticket* t = findTicket(id);

        return t ? t->toJSON()
            : "{\"error\":\"Booking not found. Check the Booking ID.\"}";
    }

    string cancel(const string& body) {
        string id = getString(body, "bookingID");
        Ticket* t = findTicket(id);

        if (!t)
            return "{\"error\":\"Booking not found. Check the Booking ID.\"}";

        if (t->getStatus() == "CANCELLED")
            return "{\"error\":\"This ticket is already cancelled.\"}";

        t->getBus()->releaseSeat(t->getSeat());
        t->cancel();
        save();

        return t->toJSON();
    }
};

// HTTP SERVER
bool sendAll(int client, const string& data) {
    size_t sent = 0;

    while (sent < data.size()) {
        ssize_t n = send(client, data.data() + sent,
                         data.size() - sent, 0);

        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }

    return true;
}

void sendResponse(int client, int code,
                  const string& contentType,
                  const string& body) {

    string status =
        code == 200 ? "200 OK" :
        code == 404 ? "404 Not Found" :
        code == 405 ? "405 Method Not Allowed" :
        "400 Bad Request";

    string header =
        "HTTP/1.1 " + status + "\r\n"
        "Content-Type: " + contentType + "; charset=utf-8\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Content-Length: " + to_string(body.size()) + "\r\n"
        "Connection: close\r\n\r\n";

    sendAll(client, header + body);
}

string getMimeType(const string& path) {
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".html")
        return "text/html";

    if (path.size() >= 4 && path.substr(path.size() - 4) == ".css")
        return "text/css";

    if (path.size() >= 3 && path.substr(path.size() - 3) == ".js")
        return "application/javascript";

    if (path.size() >= 4 && path.substr(path.size() - 4) == ".png")
        return "image/png";

    if (path.size() >= 4 && path.substr(path.size() - 4) == ".jpg")
        return "image/jpeg";

    if (path.size() >= 5 && path.substr(path.size() - 5) == ".jpeg")
        return "image/jpeg";

    if (path.size() >= 4 && path.substr(path.size() - 4) == ".svg")
        return "image/svg+xml";

    if (path.size() >= 4 && path.substr(path.size() - 4) == ".ico")
        return "image/x-icon";

    return "application/octet-stream";
}

bool readRequest(int client, string& request) {
    char buffer[8192];
    size_t headerEnd = string::npos;
    size_t contentLength = 0;

    while (request.size() < 1024 * 1024) {
        ssize_t n = recv(client, buffer, sizeof(buffer), 0);

        if (n <= 0) return false;

        request.append(buffer, static_cast<size_t>(n));

        if (headerEnd == string::npos) {
            headerEnd = request.find("\r\n\r\n");

            if (headerEnd != string::npos) {
                string headers = request.substr(0, headerEnd);
                istringstream stream(headers);
                string line;

                getline(stream, line);

                while (getline(stream, line)) {
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();

                    size_t colon = line.find(':');
                    if (colon == string::npos) continue;

                    string name = lowerText(line.substr(0, colon));

                    if (name == "content-length") {
                        try {
                            contentLength = static_cast<size_t>(
                                stoul(line.substr(colon + 1)));
                        } catch (...) {
                            contentLength = 0;
                        }
                    }
                }
            }
        }

        if (headerEnd != string::npos &&
            request.size() >= headerEnd + 4 + contentLength)
            return true;
    }

    return false;
}

string readFile(const string& path) {
    ifstream file(path, ios::binary);
    if (!file) return "";

    stringstream data;
    data << file.rdbuf();

    return data.str();
}

// MAIN
int main() {
    int port = 8080;

    if (const char* envPort = getenv("PORT")) {
        try {
            int requestedPort = stoi(envPort);

            if (requestedPort > 0 && requestedPort <= 65535)
                port = requestedPort;
        } catch (...) {}
    }

    int server = socket(AF_INET, SOCK_STREAM, 0);

    if (server < 0) {
        cerr << "Could not create server socket: "
             << strerror(errno) << '\n';
        return 1;
    }

    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
               &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port));
    address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) < 0) {
        cerr << "Could not bind to port " << port << ": "
             << strerror(errno) << '\n';
        close(server);
        return 1;
    }

    if (listen(server, 20) < 0) {
        cerr << "Could not start listening: "
             << strerror(errno) << '\n';
        close(server);
        return 1;
    }

    BookingSystem app;

    cout << "RTC Bus Ticketing System running on port "
         << port << ".\n";

    while (true) {
        int client = accept(server, nullptr, nullptr);

        if (client < 0) {
            if (errno == EINTR) continue;

            cerr << "Connection error: "
                 << strerror(errno) << '\n';
            continue;
        }

        string request;

        if (!readRequest(client, request)) {
            sendResponse(client, 400, "text/plain",
                         "Invalid or incomplete request.");
            close(client);
            continue;
        }

        size_t lineEnd = request.find("\r\n");
        size_t headerEnd = request.find("\r\n\r\n");

        istringstream firstLine(request.substr(0, lineEnd));

        string method, target, version;
        firstLine >> method >> target >> version;

        string body = headerEnd == string::npos
            ? ""
            : request.substr(headerEnd + 4);

        if (method == "OPTIONS") {
            sendResponse(client, 200, "text/plain", "");
        }
        else if (method != "GET" && method != "POST") {
            sendResponse(client, 405, "text/plain",
                         "Method not allowed.");
        }
        else {
            string output;
            string contentType = "application/json";
            int status = 200;

            size_t queryPos = target.find('?');
            string path = target.substr(0, queryPos);

            if (path == "/api/buses") {
                output = app.busList(
                    queryValue(target, "from"),
                    queryValue(target, "to"));
            }
            else if (path == "/api/seats") {
                output = app.seatList(
                    queryValue(target, "bus"));
            }
            else if (path == "/api/book" && method == "POST") {
                output = app.book(body);
            }
            else if (path == "/api/ticket") {
                output = app.ticket(
                    queryValue(target, "id"));
            }
            else if (path == "/api/cancel" && method == "POST") {
                output = app.cancel(body);
            }
            else if (path.rfind("/api/", 0) == 0) {
                output = "{\"error\":\"API endpoint not found\"}";
                status = 404;
            }
            else {
                string relativePath =
                    path == "/" ? "/index.html" : path;

                if (relativePath.find("..") != string::npos) {
                    output = "Invalid file path.";
                    status = 400;
                    contentType = "text/plain";
                }
                else {
                    string filePath = "frontend" + relativePath;
                    output = readFile(filePath);

                    if (output.empty()) {
                        output = "File not found. Check that frontend files exist and start the server from the project root.";
                        status = 404;
                        contentType = "text/plain";
                    }
                    else {
                        contentType = getMimeType(filePath);
                    }
                }
            }

            if (path.rfind("/api/", 0) == 0 &&
                output.find("\"error\"") != string::npos) {
                status = (status == 404) ? 404 : 400;
            }

            sendResponse(client, status, contentType, output);
        }

        close(client);
    }

    close(server);
    return 0;
}


