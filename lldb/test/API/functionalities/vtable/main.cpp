#include <string>

class Shape {
public:
  virtual double Area() { return 1; }
  virtual double Perimeter() { return 1; }
  // Note that destructors generate two entries in the vtable: base object
  // destructor and deleting destructor.
  virtual ~Shape() = default;
};

class Rectangle : public Shape {
public:
  double Area() override { return 2; }

  double Perimeter() override { return 2; }

  // This *shouldn't* show up in the vtable.
  void RectangleSpecific() { return; }
};

class NotSubclass {
public:
  std::string greet() { return "Hello"; }
};

int main() {
  Shape shape;
  Rectangle rect;

  Shape *shape_ptr = &rect;
  // Shape is Rectangle
  shape_ptr = &shape;
  // Shape is Shape

  NotSubclass not_subclass;

  // At the end

  return 0;
}
