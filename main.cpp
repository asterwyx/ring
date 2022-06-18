#include "ring.hpp"
#include <queue>
#include <memory>
#include <thread>
using namespace std;

int main() {
    shared_ptr<ring<string>> r(new ring<string>);
    shared_ptr<thread> t(new thread);
    return 0;
}