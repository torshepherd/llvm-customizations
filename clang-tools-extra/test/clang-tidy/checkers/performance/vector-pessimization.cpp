// RUN: %check_clang_tidy %s performance-vector-pessimization %t

namespace std {
template <typename T> struct vector {};
} // namespace std

enum ExampleEnum { Val };

struct MoveConstructorThrows {
    MoveConstructorThrows(const MoveConstructorThrows&) { (void)0; }
    MoveConstructorThrows(MoveConstructorThrows&&) noexcept(false) { (void)0; }
};

void f1() { [[maybe_unused]] std::vector<MoveConstructorThrows> P{}; }
// CHECK-MESSAGES: :[[@LINE-1]]:35: warning: 'vector<MoveConstructorThrows>' will copy elements on resize instead of moving because the move constructor of 'struct MoveConstructorThrows' may throw [performance-vector-pessimization]
// CHECK-MESSAGES: :[[@LINE-7]]:8: note: 'struct MoveConstructorThrows' defined here
// CHECK-MESSAGES: :[[@LINE-6]]:5: note: throwing move constructor declared here

struct DerivedFromMoveConstructorThrows : public MoveConstructorThrows {};

void f2() { [[maybe_unused]] std::vector<DerivedFromMoveConstructorThrows> P{}; }
// CHECK-MESSAGES: :[[@LINE-1]]:35: warning: 'vector<DerivedFromMoveConstructorThrows>' will copy elements on resize instead of moving because the move constructor of 'struct DerivedFromMoveConstructorThrows' may throw [performance-vector-pessimization]
// CHECK-MESSAGES: :[[@LINE-4]]:8: note: 'struct DerivedFromMoveConstructorThrows' defined here
// CHECK-MESSAGES: :[[@LINE-5]]:43: note: because the move constructor of 'MoveConstructorThrows' may throw
// CHECK-MESSAGES: :[[@LINE-16]]:8: note: 'struct MoveConstructorThrows' defined here

class ContainsThrowingMoveConstructor {
  public:
    struct Inner {
        const MoveConstructorThrows Bar;
    };
    ExampleEnum ThisMemberIsFine;
    Inner Foo;
};

void f3() { [[maybe_unused]] std::vector<ContainsThrowingMoveConstructor> P{}; }
// CHECK-MESSAGES: :[[@LINE-1]]:35: warning: 'vector<ContainsThrowingMoveConstructor>' will copy elements on resize instead of moving because the move constructor of 'class ContainsThrowingMoveConstructor' may throw [performance-vector-pessimization]
// CHECK-MESSAGES: :[[@LINE-11]]:7: note: 'class ContainsThrowingMoveConstructor' defined here
// CHECK-MESSAGES: :[[@LINE-6]]:11: note: because the move constructor of 'Inner' may throw
// CHECK-MESSAGES: :[[@LINE-11]]:12: note: 'struct ContainsThrowingMoveConstructor::Inner' defined here
// CHECK-MESSAGES: :[[@LINE-11]]:37: note: because the move constructor of 'MoveConstructorThrows' may throw
// CHECK-MESSAGES: :[[@LINE-33]]:8: note: 'struct MoveConstructorThrows' defined here
// CHECK-MESSAGES: :[[@LINE-32]]:5: note: throwing move constructor declared here

struct ContainsDeeplyNestedThrowingMoveConstructor {
    ContainsThrowingMoveConstructor Baz;
};

void f4() { [[maybe_unused]] std::vector<ContainsDeeplyNestedThrowingMoveConstructor> P{}; }
// CHECK-MESSAGES: :[[@LINE-1]]:35: warning: 'vector<ContainsDeeplyNestedThrowingMoveConstructor>' will copy elements on resize instead of moving because the move constructor of 'struct ContainsDeeplyNestedThrowingMoveConstructor' may throw [performance-vector-pessimization]
// CHECK-MESSAGES: :[[@LINE-6]]:8: note: 'struct ContainsDeeplyNestedThrowingMoveConstructor' defined here
// CHECK-MESSAGES: :[[@LINE-6]]:37: note: because the move constructor of 'ContainsThrowingMoveConstructor' may throw
// CHECK-MESSAGES: :[[@LINE-26]]:7: note: 'class ContainsThrowingMoveConstructor' defined here
// CHECK-MESSAGES: :[[@LINE-21]]:11: note: because the move constructor of 'Inner' may throw
// CHECK-MESSAGES: :[[@LINE-26]]:12: note: 'struct ContainsThrowingMoveConstructor::Inner' defined here
// CHECK-MESSAGES: :[[@LINE-26]]:37: note: because the move constructor of 'MoveConstructorThrows' may throw
// Message output should end here due to max recursion depth

struct NothrowMoveConstructibleExample {
    NothrowMoveConstructibleExample(const NothrowMoveConstructibleExample&) { (void)0; }
    NothrowMoveConstructibleExample(NothrowMoveConstructibleExample&&) noexcept { (void)0; }
};

void f5() { [[maybe_unused]] std::vector<NothrowMoveConstructibleExample> P{}; }

struct TriviallyCopyableExample {
    int I;
};

void f6() { [[maybe_unused]] std::vector<TriviallyCopyableExample> P{}; }
