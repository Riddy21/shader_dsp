#include <iostream>
#include <memory>

int main() {
    // Allocate an integer
    std::unique_ptr<void, void(*)(void*)> ptr(new int(42), [](void* p) {
        delete static_cast<int*>(p);  // Cast to the correct type before deleting
    });

    // Cast back to the correct type when needed
    int* int_ptr = static_cast<int*>(ptr.get());
    std::cout << "Value: " << *int_ptr << std::endl;  // Output: 42

    return 0; // The custom deleter is called automatically when ptr goes out of scope
}