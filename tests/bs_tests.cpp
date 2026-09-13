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
} // namespace

int main() {
  const auto call = bs::blackScholes(bs::Type::Call, 100, 100, 0.05, 0, 0.2, 1);
  const auto put = bs::blackScholes(bs::Type::Put, 100, 100, 0.05, 0, 0.2, 1);
  near(call.price, 10.45058357, 1e-7, "Black-Scholes call benchmark");
  near(put.price, 5.57352602, 1e-7, "Black-Scholes put benchmark");
  near(call.price - put.price, 100 - 100 * std::exp(-0.05), 1e-10,
       "put-call parity");

  const auto binary = bs::binaryCashOrNothing(bs::Type::Call, 105, 100, 0.04,
                                              0.01, 0.25, 0.8, 1);
  const double h = 1e-5;
  const double priceLonger =
      bs::binaryCashOrNothing(bs::Type::Call, 105, 100, 0.04, 0.01, 0.25,
                              0.8 + h, 1)
          .price;
  const double priceShorter =
      bs::binaryCashOrNothing(bs::Type::Call, 105, 100, 0.04, 0.01, 0.25,
                              0.8 - h, 1)
          .price;
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

  // Non-dividend calls with nonnegative rates provide an analytic benchmark
  // for spatial Greeks, including away from the strike and near expiry.
  for (double spot : {80.0, 100.0, 120.0}) {
    for (double vol : {0.15, 0.4}) {
      for (double time : {0.1, 1.0}) {
        const auto european =
            bs::blackScholes(bs::Type::Call, spot, 100, 0.05, 0, vol, time);
        const auto american = bs::americanOption(bs::Type::Call, spot, 100,
                                                 0.05, 0, vol, time, 300);
        near(american.delta, european.delta, 0.003,
             "American call delta versus analytic benchmark");
        near(american.gamma, european.gamma, 0.0003 + 0.02 * european.gamma,
             "American call gamma versus analytic benchmark");
      }
    }
  }
  const auto coarse =
      bs::americanOption(bs::Type::Call, 100, 100, 0.05, 0, 0.2, 1, 100);
  const auto fine =
      bs::americanOption(bs::Type::Call, 100, 100, 0.05, 0, 0.2, 1, 1000);
  check(std::fabs(fine.gamma - call.gamma) <
            std::fabs(coarse.gamma - call.gamma),
        "American gamma converges with tree resolution");
  const auto exercised =
      bs::americanOption(bs::Type::Put, 20, 100, 0.05, 0, 0.2, 1, 300);
  near(exercised.price, 80, 1e-8, "immediate exercise price");
  near(exercised.delta, -1, 1e-8, "immediate exercise delta");
  near(exercised.gamma, 0, 1e-8, "immediate exercise gamma");
  check(std::isfinite(
            bs::americanOption(bs::Type::Call, 100, 100, 0.05, 0, 0.2, 1, 2)
                .gamma),
        "two-step tree gamma");
  check(std::isnan(
            bs::americanOption(bs::Type::Call, 100, 100, 0.05, 0, 0.2, 1, 1)
                .price),
        "spatial Greeks require at least two steps");

  for (auto type : {bs::Type::Call, bs::Type::Put}) {
    for (double spot : {80.0, 100.0, 120.0}) {
      for (double rate : {-0.02, 0.05}) {
        const auto b =
            bs::binaryCashOrNothing(type, spot, 100, rate, 0.03, 0.25, 0.8, 1);
        const auto up = bs::binaryCashOrNothing(type, spot, 100, rate, 0.03,
                                                0.25, 0.8 + h, 1);
        const auto down = bs::binaryCashOrNothing(type, spot, 100, rate, 0.03,
                                                  0.25, 0.8 - h, 1);
        near(b.theta, -(up.price - down.price) / (2 * h), 1e-7,
             "binary calendar theta");
        const auto e = bs::blackScholes(type, spot, 100, rate, 0.03, 0.25, 0.8);
        const auto eu =
            bs::blackScholes(type, spot, 100, rate, 0.03, 0.25, 0.8 + h);
        const auto ed =
            bs::blackScholes(type, spot, 100, rate, 0.03, 0.25, 0.8 - h);
        near(e.theta, -(eu.price - ed.price) / (2 * h), 1e-7,
             "European calendar theta");
      }
    }
  }
  const auto binaryCall =
      bs::binaryCashOrNothing(bs::Type::Call, 100, 100, 0.05, 0, 0.2, 1, 1);
  const auto binaryPut =
      bs::binaryCashOrNothing(bs::Type::Put, 100, 100, 0.05, 0, 0.2, 1, 1);
  near(binaryCall.theta + binaryPut.theta, 0.05 * std::exp(-0.05), 1e-12,
       "binary theta parity");

  const auto invalid =
      bs::americanOption(bs::Type::Call, 100, 100, 0.25, 0, 0.01, 30, 300);
  check(std::isnan(invalid.price), "invalid CRR probability");
  return failures == 0 ? 0 : 1;
}
