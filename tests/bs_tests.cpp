#include "bs.hpp"
#include <cmath>
#include <iostream>

namespace {
int failures = 0;

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

void near(double actual, double expected, double tolerance,
          const char *message) {
  check(std::isfinite(actual) && std::fabs(actual - expected) <= tolerance,
        message);
}
}

int main() {
  const auto call = bs::blackScholes(bs::Type::Call, 100, 100, 0.05, 0, 0.2, 1);
  const auto put = bs::blackScholes(bs::Type::Put, 100, 100, 0.05, 0, 0.2, 1);
  near(call.price, 10.45058357, 1e-7, "Black-Scholes call benchmark");
  near(put.price, 5.57352602, 1e-7, "Black-Scholes put benchmark");
  near(call.price - put.price, 100 - 100 * std::exp(-0.05), 1e-10,
       "put-call parity");

  const auto binary =
      bs::binaryCashOrNothing(bs::Type::Call, 105, 100, 0.04, 0.01, 0.25, 0.8, 1);
  const double h = 1e-5;
  const double priceLonger =
      bs::binaryCashOrNothing(bs::Type::Call, 105, 100, 0.04, 0.01, 0.25,
                              0.8 + h, 1).price;
  const double priceShorter =
      bs::binaryCashOrNothing(bs::Type::Call, 105, 100, 0.04, 0.01, 0.25,
                              0.8 - h, 1).price;
  near(binary.theta, -(priceLonger - priceShorter) / (2 * h), 1e-7,
       "binary call theta derivative");

  const auto americanCall =
      bs::americanOption(bs::Type::Call, 100, 100, 0.05, 0, 0.2, 1, 500);
  near(americanCall.price, call.price, 0.01,
       "non-dividend American call benchmark");

  const auto americanPut =
      bs::americanOption(bs::Type::Put, 40, 40, 0.06, 0, 0.2, 1, 500);
  near(americanPut.price, 2.319, 0.01, "American put benchmark");
  check(americanPut.delta >= -1 && americanPut.delta <= 0,
        "American put delta bounds");
  check(americanPut.gamma >= 0, "American put gamma sign");

  const auto invalid =
      bs::americanOption(bs::Type::Call, 100, 100, 0.25, 0, 0.01, 30, 300);
  check(std::isnan(invalid.price), "invalid CRR probability");
  return failures == 0 ? 0 : 1;
}
