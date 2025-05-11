#include <iostream>
int main() {
    static volatile int testValue = 1; // ใช้ static เพื่อให้ที่อยู่นิ่ง
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