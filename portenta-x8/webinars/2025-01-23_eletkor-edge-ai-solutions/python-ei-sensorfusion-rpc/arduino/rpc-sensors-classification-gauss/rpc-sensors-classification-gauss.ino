/*
    Implement a fake, BME680-style sensor using Normal (Gaussian) distribution
    to generate sensor data moving from room to room.

    See https://docs.edgeimpulse.com/docs/tutorials/end-to-end-tutorials/sensor-fusion
    for reference.
*/

#include <RPC.h>
#include <SerialRPC.h>

#include <Gaussian.h>
#include <Arduino_JSON.h>

#define SerialDebug Serial1

// IDEA: get configuration via RPC
constexpr auto readInterval_us { 9.9 * 1000ul };
auto readNow_us { readInterval_us };

constexpr auto changeInterval { 10 * 1000ul };
auto changeNow { changeInterval };

// Sensor data
float temperature {};
float humidity {};
float pressure {};
float gas {};

// Status variables
std::string jsonString {};
bool newValue { false };
uint8_t inferredIndex {};

// Gaussian generators
Gaussian gTem;
Gaussian gHum;
Gaussian gPre;
Gaussian gGas;

// Rooms – they must be the same as the classes of the EI model
enum class Rooms : uint8_t {
    Unknown = 0,
    RoomA,
    RoomB,
    RoomC,
    NumRooms // This is just to keep the lenght of the enum
};

// Enum helpers
constexpr inline uint8_t getIndexFromRoom(Rooms room)
{
    auto index { 0u };

    switch (room) {
    case Rooms::RoomA:
        index = 1;
        break;
    case Rooms::RoomB:
        index = 2;
        break;
    case Rooms::RoomC:
        index = 3;
        break;
    case Rooms::NumRooms:
        index = 4;
        break;
    default:
        break;
    }

    return index;
}

constexpr inline Rooms getRoomFromIndex(uint8_t index)
{
    Rooms room { Rooms::Unknown };

    switch(index) {
    case 1:
        room = Rooms::RoomA;
        break;
    case 2:
        room = Rooms::RoomB;
        break;
    case 3:
        room = Rooms::RoomC;
        break;
    default:
        break;
    }

    return room;
}

// For distribution configuration
struct GaussConf {
    float mean;
    float vari;
};

// For Room configuration
struct RoomConf {
    Rooms room;
    GaussConf confTem;
    GaussConf confHum;
    GaussConf confPre;
    GaussConf confGas;
};

// Our rooms.
constexpr RoomConf RoomsConfigs[] {
    { Rooms::Unknown, { 24, 0.1 }, { 55, 1.5 }, { 1023, 2 }, { 500, 10 } },
    { Rooms::RoomA, { 18, 0.25 }, { 50, 1 }, { 1023, 2 }, { 1000, 10 } },
    { Rooms::RoomB, { 22, 0.4 }, { 45, 1.1 }, { 1023, 1.9 }, { 1500, 19 } },
    { Rooms::RoomC, { 14, 0.5 }, { 65, 0.2 }, { 1023, .7 }, { 2000, 45 } }
};

Rooms currentRoom { Rooms::Unknown };

void setup()
{
    RPC.begin();

    // SerialDebug.begin(115200);
    // SerialDebug.println("Fake BME680 test on M4");

    // RPC endpoints for sensor data
    RPC.bind("temperature", [] {
        return temperature;
    });
    RPC.bind("humidity", [] {
        return humidity;
    });
    RPC.bind("pressure", [] {
        return pressure;
    });
    RPC.bind("gas", [] {
        return gas;
    });

    // RPC endopoint for classification results
    RPC.bind("classification", [inferredIndex](std::string const& input) {
        jsonString = input;
        newValue = true;
        return inferredIndex;
    });

    // SerialDebug.println("Starting");
}

void loop()
{
    // Generate new sensor data
    if (micros() > readNow_us) {
        temperature = gTem.random();
        humidity = gHum.random();
        pressure = gPre.random();
        gas = gGas.random();
        readNow_us = micros() + readInterval_us;
#if 0
    String line;
    line.reserve(64);

    line = "";
    line += temperature;
    line += ',';
    line += humidity;
    line += ',';
    line += pressure;
    line += ',';
    line += gas;

    SerialDebug.println(line);
#endif
    }

    // Simulate moving from a room to another
    if (millis() > changeNow) {
        // SerialDebug.print("Current Room: ");
        // SerialDebug.println(getLabelFromRoom(currentRoom));

        auto roomIndex = getIndexFromRoom(currentRoom);

        gTem.setMean(RoomsConfigs[roomIndex].confTem.mean);
        gTem.setVariance(RoomsConfigs[roomIndex].confTem.vari);

        gHum.setMean(RoomsConfigs[roomIndex].confHum.mean);
        gHum.setVariance(RoomsConfigs[roomIndex].confHum.vari);

        gPre.setMean(RoomsConfigs[roomIndex].confPre.mean);
        gPre.setVariance(RoomsConfigs[roomIndex].confPre.vari);

        gGas.setMean(RoomsConfigs[roomIndex].confGas.mean);
        gGas.setVariance(RoomsConfigs[roomIndex].confGas.vari);

        constexpr auto numRooms { getIndexFromRoom(Rooms::NumRooms) };
        currentRoom = getRoomFromIndex(++roomIndex % numRooms);

        // SerialDebug.print("Next Room: ");
        // SerialDebug.println(getLabelFromRoom(currentRoom));

        changeNow = millis() + changeInterval;
    }

    // Update the status and blink LED
    if (newValue == true) {
        inferredIndex = classification(jsonString);
        newValue = false;
    }
}

// Get Enum from String
inline Rooms getRoomFromLabel(String label)
{
    Rooms room { Rooms::Unknown };

    if (label == "Room A")
        room = Rooms::RoomA;
    else if (label == "Room B")
        room = Rooms::RoomB;
    else if (label == "Room C")
        room = Rooms::RoomC;

    return room;
}

// Get String from Enum
inline String getLabelFromRoom(Rooms room)
{
    String label = "Unknown";

    if (room == Rooms::RoomA)
        label = "Room A";
    else if (room == Rooms::RoomB)
        label = "Room B";
    else if (room == Rooms::RoomC)
        label = "Room C";

    return label;
}

// Blink LED according to classification
uint8_t classification(std::string const& input)
{
    if (input.length() == 0)
        return 0;

    auto json = JSON.parse(input.c_str());

    String label = json["label"];
    double value = json["value"];

#if 0
  SerialDebug.print(label);
  SerialDebug.print(": ");
  SerialDebug.print(value, 4);
  SerialDebug.println();
#endif

    auto room = getRoomFromLabel(label);
    auto index = getIndexFromRoom(room);

    for (auto i = 0u; i <= index; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(25);
        digitalWrite(LED_BUILTIN, LOW);
        delay(25);
    }

    return index;
}
