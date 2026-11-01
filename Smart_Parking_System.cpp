/*
 * ============================================================
 *  SMART PARKING SYSTEM  -  Object Oriented Programming in C++
 * ------------------------------------------------------------
 *  Name    : Anshika
 *  UID     : 25BCD10154
 *  Section : 25 BCD-3A
 *  Subject : OOPs in C++
 *  Submitted to : Mr. Abhinay Sood
 * ------------------------------------------------------------
 *  OOP concepts used:
 *   - Classes & Objects, Encapsulation, Abstraction
 *   - Inheritance (single, multilevel-style hierarchy), Interface
 *   - Polymorphism (runtime: virtual functions; compile time:
 *     function & operator overloading)
 *   - Constructors, Destructors, Default arguments
 *   - Static members, Friend functions, const correctness
 *   - Templates (generic Repository<T>)
 *   - Exception handling (custom exception class)
 *   - STL containers, smart pointers, file handling
 *   - Composition / Aggregation
 *
 *  USP: Smart Allocation + Dynamic Surge Pricing + EV priority
 *       + Loyalty discount, all automatic.
 *
 *  Compile : g++ -std=c++17 smart_parking_system.cpp -o parking
 *  Run     : ./parking
 * ============================================================
 */

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

// ------------------------------------------------------------
//  Enums
// ------------------------------------------------------------
enum class VehicleType { BIKE = 0, CAR = 1, TRUCK = 2 };
enum class SlotSize { SMALL = 0, MEDIUM = 1, LARGE = 2 };

// Function overloading (compile-time polymorphism)
string toString(VehicleType t) {
    switch (t) {
        case VehicleType::BIKE:  return "Bike";
        case VehicleType::CAR:   return "Car";
        default:                 return "Truck";
    }
}

string toString(SlotSize s) {
    switch (s) {
        case SlotSize::SMALL:  return "Small";
        case SlotSize::MEDIUM: return "Medium";
        default:               return "Large";
    }
}

// ------------------------------------------------------------
//  Custom exception
// ------------------------------------------------------------
class ParkingException : public runtime_error {
public:
    explicit ParkingException(const string& msg) : runtime_error(msg) {}
};

// ------------------------------------------------------------
//  Interface (pure abstract class)
// ------------------------------------------------------------
class IPayable {
public:
    virtual double calculateFee(int hours, double surge) const = 0;
    virtual ~IPayable() = default;
};

// ------------------------------------------------------------
//  Abstract base class: Vehicle
// ------------------------------------------------------------
class Vehicle : public IPayable {
protected:                       // accessible to derived classes
    string plate;
    bool electric;
    static int totalCreated;     // static member shared by all objects

public:
    Vehicle(const string& plateNo, bool ev = false)   // default argument
        : plate(plateNo), electric(ev) {
        ++totalCreated;
    }

    virtual ~Vehicle() {}        // virtual destructor

    // Getters (encapsulation)
    string getPlate() const { return plate; }
    bool isElectric() const { return electric; }
    static int getTotalCreated() { return totalCreated; }

    // Pure virtual functions -> Vehicle is abstract
    virtual VehicleType getType() const = 0;
    virtual SlotSize requiredSize() const = 0;
    virtual double baseRate() const = 0;

    // Default fee logic, can be overridden
    double calculateFee(int hours, double surge) const override {
        return baseRate() * hours * surge;
    }

    // Friend function: operator overloading
    friend ostream& operator<<(ostream& os, const Vehicle& v) {
        os << toString(v.getType()) << " [" << v.plate << "]"
           << (v.electric ? " (EV)" : "");
        return os;
    }
};
int Vehicle::totalCreated = 0;

// ------------------------------------------------------------
//  Derived classes (inheritance + runtime polymorphism)
// ------------------------------------------------------------
class Bike : public Vehicle {
public:
    Bike(const string& p, bool ev = false) : Vehicle(p, ev) {}
    VehicleType getType() const override { return VehicleType::BIKE; }
    SlotSize requiredSize() const override { return SlotSize::SMALL; }
    double baseRate() const override { return 10.0; }
};

class Car : public Vehicle {
public:
    Car(const string& p, bool ev = false) : Vehicle(p, ev) {}
    VehicleType getType() const override { return VehicleType::CAR; }
    SlotSize requiredSize() const override { return SlotSize::MEDIUM; }
    double baseRate() const override { return 30.0; }

    // Overridden: EV cars get a 10% green discount
    double calculateFee(int hours, double surge) const override {
        double fee = Vehicle::calculateFee(hours, surge);
        return electric ? fee * 0.90 : fee;
    }
};

class Truck : public Vehicle {
public:
    Truck(const string& p, bool ev = false) : Vehicle(p, ev) {}
    VehicleType getType() const override { return VehicleType::TRUCK; }
    SlotSize requiredSize() const override { return SlotSize::LARGE; }
    double baseRate() const override { return 60.0; }

    // Overridden: flat heavy-vehicle handling fee added
    double calculateFee(int hours, double surge) const override {
        return Vehicle::calculateFee(hours, surge) + 50.0;
    }
};

// ------------------------------------------------------------
//  Slot class
// ------------------------------------------------------------
class Slot {
private:
    int id;
    SlotSize size;
    int distance;          // metres from entrance
    bool charger;          // EV charging point available
    bool occupied;
    string plate;

public:
    Slot(int i, SlotSize s, int d, bool c = false)
        : id(i), size(s), distance(d), charger(c), occupied(false) {}

    int getId() const { return id; }
    SlotSize getSize() const { return size; }
    int getDistance() const { return distance; }
    bool hasCharger() const { return charger; }
    bool isOccupied() const { return occupied; }

    bool canFit(const Vehicle& v) const {
        return !occupied &&
               static_cast<int>(size) >= static_cast<int>(v.requiredSize());
    }

    void occupy(const string& p) {
        if (occupied) throw ParkingException("Slot already occupied!");
        occupied = true;
        plate = p;
    }

    void release() {
        occupied = false;
        plate.clear();
    }

    // Operator overloading: compare slots by distance
    bool operator<(const Slot& other) const { return distance < other.distance; }

    friend ostream& operator<<(ostream& os, const Slot& s) {
        os << "Slot " << setw(2) << s.id << " | "
           << left << setw(6) << toString(s.size) << right
           << " | " << setw(3) << s.distance << " m | "
           << (s.charger ? "EV-Charger" : "          ") << " | "
           << (s.occupied ? "OCCUPIED (" + s.plate + ")" : "Free");
        return os;
    }
};

// ------------------------------------------------------------
//  Ticket (composition: holds a shared_ptr to the vehicle)
// ------------------------------------------------------------
class Ticket {
private:
    static int counter;
    int id;
    shared_ptr<Vehicle> vehicle;
    int slotId;
    double surgeAtEntry;
    time_t entryTime;

public:
    Ticket(shared_ptr<Vehicle> v, int slot, double surge)
        : id(++counter), vehicle(v), slotId(slot),
          surgeAtEntry(surge), entryTime(time(nullptr)) {}

    int getId() const { return id; }
    int getSlotId() const { return slotId; }
    double getSurge() const { return surgeAtEntry; }
    shared_ptr<Vehicle> getVehicle() const { return vehicle; }
    string entryTimeStr() const {
        string t = ctime(&entryTime);
        t.pop_back();      // remove newline
        return t;
    }
};
int Ticket::counter = 1000;

// ------------------------------------------------------------
//  Receipt
// ------------------------------------------------------------
class Receipt {
public:
    int ticketId;
    string plate;
    string type;
    int slotId;
    int hours;
    double surge;
    double discount;
    double amount;

    friend ostream& operator<<(ostream& os, const Receipt& r) {
        os << "----------- PARKING RECEIPT -----------\n"
           << "Ticket ID   : " << r.ticketId << "\n"
           << "Vehicle     : " << r.type << " [" << r.plate << "]\n"
           << "Slot        : " << r.slotId << "\n"
           << "Hours       : " << r.hours << "\n"
           << "Surge       : x" << fixed << setprecision(2) << r.surge << "\n"
           << "Loyalty disc: Rs. " << r.discount << "\n"
           << "TOTAL       : Rs. " << r.amount << "\n"
           << "---------------------------------------\n";
        return os;
    }
};

// ------------------------------------------------------------
//  Template class: generic Repository<T>
// ------------------------------------------------------------
template <typename T>
class Repository {
private:
    vector<T> items;

public:
    void add(const T& item) { items.push_back(item); }
    const vector<T>& all() const { return items; }
    size_t size() const { return items.size(); }
};

// ------------------------------------------------------------
//  ParkingLot (aggregates Slots, Tickets, Receipts)
// ------------------------------------------------------------
class ParkingLot {
private:
    string name;
    vector<Slot> slots;
    map<string, Ticket> active;          // plate -> ticket
    Repository<Receipt> history;         // template used
    map<string, int> visits;             // loyalty tracking

    // USP: smart slot scoring (lower score = better slot)
    Slot* findBestSlot(const Vehicle& v) {
        Slot* best = nullptr;
        int bestScore = numeric_limits<int>::max();

        for (auto& s : slots) {
            if (!s.canFit(v)) continue;

            int score = s.getDistance();
            // penalty for wasting a bigger slot
            score += (static_cast<int>(s.getSize()) -
                      static_cast<int>(v.requiredSize())) * 15;
            // EVs strongly prefer chargers, others avoid blocking them
            if (v.isElectric() && s.hasCharger()) score -= 1000;
            if (!v.isElectric() && s.hasCharger()) score += 25;

            if (score < bestScore) {
                bestScore = score;
                best = &s;
            }
        }
        return best;
    }

public:
    ParkingLot(const string& n) : name(n) {}

    void addSlot(const Slot& s) { slots.push_back(s); }

    int totalSlots() const { return static_cast<int>(slots.size()); }

    int occupiedSlots() const {
        return static_cast<int>(count_if(slots.begin(), slots.end(),
                                [](const Slot& s) { return s.isOccupied(); }));
    }

    double occupancy() const {
        return totalSlots() == 0 ? 0.0 : (double)occupiedSlots() / totalSlots();
    }

    // USP: dynamic surge pricing based on occupancy
    double surgeMultiplier() const {
        double o = occupancy();
        if (o >= 0.90) return 1.50;
        if (o >= 0.70) return 1.25;
        return 1.00;
    }

    // Park a vehicle: returns the allocated ticket
    Ticket park(shared_ptr<Vehicle> v) {
        if (active.count(v->getPlate()))
            throw ParkingException("Vehicle " + v->getPlate() + " is already parked!");

        Slot* slot = findBestSlot(*v);
        if (!slot)
            throw ParkingException("No suitable slot available for " +
                                   toString(v->getType()) + ".");

        double surge = surgeMultiplier();
        slot->occupy(v->getPlate());
        Ticket t(v, slot->getId(), surge);
        active.insert({v->getPlate(), t});
        return t;
    }

    // Checkout: hours is supplied for demo/simulation purposes
    Receipt checkout(const string& plate, int hours) {
        auto it = active.find(plate);
        if (it == active.end())
            throw ParkingException("No active ticket for vehicle " + plate);

        if (hours < 1) hours = 1;
        const Ticket& t = it->second;

        // runtime polymorphism: correct calculateFee() is chosen automatically
        double fee = t.getVehicle()->calculateFee(hours, t.getSurge());

        // USP: loyalty discount after 3 completed visits
        double discount = (visits[plate] >= 3) ? fee * 0.10 : 0.0;

        Receipt r;
        r.ticketId = t.getId();
        r.plate = plate;
        r.type = toString(t.getVehicle()->getType());
        r.slotId = t.getSlotId();
        r.hours = hours;
        r.surge = t.getSurge();
        r.discount = discount;
        r.amount = fee - discount;

        for (auto& s : slots)
            if (s.getId() == t.getSlotId()) s.release();

        visits[plate]++;
        active.erase(it);
        history.add(r);
        saveReceipt(r);
        return r;
    }

    // File handling
    void saveReceipt(const Receipt& r) const {
        ofstream out("receipts.txt", ios::app);
        if (out) out << r << "\n";
    }

    void showStatus() const {
        cout << "\n===== " << name << " =====\n";
        for (const auto& s : slots) cout << s << "\n";
        cout << "Occupancy: " << occupiedSlots() << "/" << totalSlots()
             << " (" << fixed << setprecision(0) << occupancy() * 100 << "%)"
             << "  |  Current surge: x" << setprecision(2) << surgeMultiplier() << "\n";
    }

    void showHistory() const {
        if (history.size() == 0) {
            cout << "\nNo completed parking sessions yet.\n";
            return;
        }
        for (const auto& r : history.all()) cout << r;
    }

    void showSummary() const {
        double revenue = 0;
        for (const auto& r : history.all()) revenue += r.amount;
        cout << "\n----- DAILY SUMMARY -----\n"
             << "Vehicles created this session : " << Vehicle::getTotalCreated() << "\n"
             << "Sessions completed            : " << history.size() << "\n"
             << "Currently parked              : " << active.size() << "\n"
             << "Total revenue                 : Rs. " << fixed << setprecision(2)
             << revenue << "\n";
    }
};

// ------------------------------------------------------------
//  Helpers
// ------------------------------------------------------------
shared_ptr<Vehicle> makeVehicle(int choice, const string& plate, bool ev) {
    switch (choice) {
        case 1:  return make_shared<Bike>(plate, ev);
        case 2:  return make_shared<Car>(plate, ev);
        case 3:  return make_shared<Truck>(plate, ev);
        default: throw ParkingException("Invalid vehicle type!");
    }
}

int readInt(const string& prompt) {
    int x;
    while (true) {
        cout << prompt;
        if (cin >> x) return x;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Please enter a valid number.\n";
    }
}

string readPlate() {
    string p;
    cout << "Enter number plate: ";
    cin >> p;
    transform(p.begin(), p.end(), p.begin(), ::toupper);
    return p;
}

void buildLot(ParkingLot& lot) {
    // id, size, distance from entrance, charger
    lot.addSlot(Slot(1,  SlotSize::SMALL,  5));
    lot.addSlot(Slot(2,  SlotSize::SMALL,  10));
    lot.addSlot(Slot(3,  SlotSize::SMALL,  15));
    lot.addSlot(Slot(4,  SlotSize::SMALL,  20));
    lot.addSlot(Slot(5,  SlotSize::MEDIUM, 8,  true));
    lot.addSlot(Slot(6,  SlotSize::MEDIUM, 12, true));
    lot.addSlot(Slot(7,  SlotSize::MEDIUM, 16));
    lot.addSlot(Slot(8,  SlotSize::MEDIUM, 20));
    lot.addSlot(Slot(9,  SlotSize::MEDIUM, 24));
    lot.addSlot(Slot(10, SlotSize::MEDIUM, 28));
    lot.addSlot(Slot(11, SlotSize::LARGE,  30));
    lot.addSlot(Slot(12, SlotSize::LARGE,  35));
}

// ------------------------------------------------------------
//  main
// ------------------------------------------------------------
int main() {
    ParkingLot lot("SMART PARKING SYSTEM");
    buildLot(lot);

    int choice;
    do {
        cout << "\n======= SMART PARKING SYSTEM =======\n"
             << "1. Park a vehicle\n"
             << "2. Exit a vehicle (checkout)\n"
             << "3. Show parking status\n"
             << "4. Show receipts history\n"
             << "5. Daily summary\n"
             << "0. Quit\n";
        choice = readInt("Enter choice: ");

        try {
            switch (choice) {
                case 1: {
                    cout << "Vehicle type (1=Bike, 2=Car, 3=Truck): ";
                    int type = readInt("");
                    string plate = readPlate();
                    int ev = readInt("Electric vehicle? (1=Yes, 0=No): ");
                    auto v = makeVehicle(type, plate, ev == 1);
                    Ticket t = lot.park(v);
                    cout << "\nParked: " << *v << "\n"
                         << "Ticket ID : " << t.getId() << "\n"
                         << "Slot      : " << t.getSlotId() << "\n"
                         << "Entry time: " << t.entryTimeStr() << "\n"
                         << "Surge     : x" << fixed << setprecision(2)
                         << t.getSurge() << "\n";
                    break;
                }
                case 2: {
                    string plate = readPlate();
                    int hrs = readInt("Hours parked (simulated): ");
                    Receipt r = lot.checkout(plate, hrs);
                    cout << "\n" << r;
                    break;
                }
                case 3: lot.showStatus();  break;
                case 4: lot.showHistory(); break;
                case 5: lot.showSummary(); break;
                case 0: cout << "Thank you for using Smart Parking System!\n"; break;
                default: cout << "Invalid choice, try again.\n";
            }
        } catch (const ParkingException& e) {
            cout << "\n[Error] " << e.what() << "\n";
        }
    } while (choice != 0);

    return 0;
}