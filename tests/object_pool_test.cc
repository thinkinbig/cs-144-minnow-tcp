#include "object_pool_test_harness.hh"
#include "test_should_be.hh"

#include <cstdint>
#include <stdexcept>
#include <string>

using namespace std;

// 测试用例1：基本功能测试
void test_basic() {
    ObjectPoolTestHarness<int> test(
        "basic",
        []() { return 0; },
        [](int&) {}
    );

    test.expect_size(0);
    test.expect_available(0);
    test.expect_borrowed(0);

    auto ref = test.borrow();
    test.expect_size(1);
    test.expect_available(0);
    test.expect_borrowed(1);

    test.return_object(ref.get());
    test.expect_size(1);
    test.expect_available(1);
    test.expect_borrowed(0);
}

// 测试用例2：预分配测试
void test_pre_allocated() {
    ObjectPoolTestHarness<string> test(
        "pre_allocated",
        []() { return string(); },
        [](string& s) { s.clear(); },
        3
    );

    test.expect_size(3);
    test.expect_available(3);
    test.expect_borrowed(0);

    auto ref1 = test.borrow();
    auto ref2 = test.borrow();
    auto ref3 = test.borrow();

    test.expect_size(3);
    test.expect_available(0);
    test.expect_borrowed(3);

    test.return_object(ref1.get());
    test.return_object(ref2.get());
    test.return_object(ref3.get());

    test.expect_size(3);
    test.expect_available(3);
    test.expect_borrowed(0);
}

// 测试用例3：最大容量测试
void test_max_size() {
    ObjectPoolTestHarness<double> test(
        "max_size",
        []() { return 0.0; },
        [](double&) {},
        0,
        2
    );

    test.expect_size(0);
    test.expect_available(0);
    test.expect_borrowed(0);

    auto ref1 = test.borrow();
    auto ref2 = test.borrow();

    test.expect_size(2);
    test.expect_available(0);
    test.expect_borrowed(2);

    test.return_object(ref1.get());
    test.return_object(ref2.get());

    test.expect_size(2);
    test.expect_available(2);
    test.expect_borrowed(0);
}

// 测试用例4：重置器测试
void test_resetter() {
    int reset_count = 0;
    ObjectPoolTestHarness<int> test(
        "resetter",
        []() { return 42; },
        [&reset_count](int&) { reset_count++; }
    );

    auto ref = test.borrow();
    test.expect_size(1);
    test.expect_available(0);
    test.expect_borrowed(1);

    test.return_object(ref.get());
    test.expect_size(1);
    test.expect_available(1);
    test.expect_borrowed(0);

    if (reset_count != 1) {
        throw runtime_error("Resetter was not called");
    }
}

int main() {
    try {
        test_basic();
        test_pre_allocated();
        test_max_size();
        test_resetter();
        return EXIT_SUCCESS;
    } catch (const exception& e) {
        cerr << "Test failed: " << e.what() << endl;
        return EXIT_FAILURE;
    }
} 