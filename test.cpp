#include <iostream>
int main() {
    volatile int testValue = 95; // ใช้ volatile เพื่อป้องกัน optimization
    std::cout << "Test value: " << testValue << std::endl;
    std::cout << "Address of testValue: " << &testValue << std::endl;
    std::cout << "Press Enter to change value to 100..." << std::endl;
    std::cin.get();
    testValue = 100;
    std::cout << "Test value changed to: " << testValue << std::endl;
    std::cout << "Address of testValue: " << &testValue << std::endl;
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    return 0;
}