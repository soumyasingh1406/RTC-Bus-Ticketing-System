
FROM gcc:13

WORKDIR /app

COPY . .

RUN g++ -std=c++17 -pthread backend/server.cpp -o rtc_server

EXPOSE 8080

CMD ["./rtc_server"]
