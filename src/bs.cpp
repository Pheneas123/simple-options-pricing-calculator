#include "bs.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

struct TreeResult {
  double price, delta, gamma;
};

// American option via CRR binomial with early exercise
static TreeResult priceAmericanBinomialCore(bool is_call, double S, double K,
                                            double r, double q, double sigma,
                                            double T, int steps) {

  const double dt = T / steps;
  const double u = std::exp(sigma * std::sqrt(dt));
  const double d = 1.0 / u;
  const double disc = std::exp(-r * dt);
  const double a = std::exp((r - q) * dt);
  const double p = (a - d) / (u - d);
  if (!std::isfinite(p) || p < 0.0 || p > 1.0)
    return {NAN, NAN, NAN};

  // stock prices at maturity
  std::vector<double> ST(steps + 1);
  ST[0] = S * std::pow(d, steps);
  for (int i = 1; i <= steps; ++i)
    ST[i] = ST[i - 1] * (u / d);

  // option values at maturity
  std::vector<double> V(steps + 1);
  for (int i = 0; i <= steps; ++i) {
    V[i] = is_call ? std::max(ST[i] - K, 0.0) : std::max(K - ST[i], 0.0);
  }

  double delta = NAN, gamma = NAN;
  // Read spatial Greeks from the first two tree levels. Small spot bumps
  // differentiate the piecewise-linear lattice price and give unstable gamma.
  auto captureGreeks = [&](int level) {
    if (level == 2) {
      const double deltaUp = (V[2] - V[1]) / (ST[2] - ST[1]);
      const double deltaDown = (V[1] - V[0]) / (ST[1] - ST[0]);
      gamma = (deltaUp - deltaDown) / (0.5 * (ST[2] - ST[0]));
    } else if (level == 1) {
      delta = (V[1] - V[0]) / (ST[1] - ST[0]);
    }
  };
  captureGreeks(steps);

  // backward induction with early exercise
  for (int step = steps - 1; step >= 0; --step) {
    for (int i = 0; i <= step; ++i) {
      ST[i] = ST[i] / d; // roll down
      const double cont = disc * (p * V[i + 1] + (1.0 - p) * V[i]);
      const double exer =
          is_call ? std::max(ST[i] - K, 0.0) : std::max(K - ST[i], 0.0);
      V[i] = std::max(cont, exer);
      if (step == 0 && exer > 0.0 && exer >= cont) {
        delta = is_call ? 1.0 : -1.0;
        gamma = 0.0;
      }
    }
    captureGreeks(step);
  }
  return {V[0], delta, gamma};
}

static TreeResult priceAmericanBinomial(bool is_call, double S, double K,
                                        double r, double q, double sigma,
                                        double T, int steps) {
  const auto a =
      priceAmericanBinomialCore(is_call, S, K, r, q, sigma, T, steps);
  const auto b =
      priceAmericanBinomialCore(is_call, S, K, r, q, sigma, T, steps + 1);
  if (!std::isfinite(a.price) || !std::isfinite(b.price))
    return {NAN, NAN, NAN};
  const double intrinsic =
      is_call ? std::max(S - K, 0.0) : std::max(K - S, 0.0);
  return {std::max(intrinsic, 0.5 * (a.price + b.price)),
          0.5 * (a.delta + b.delta), 0.5 * (a.gamma + b.gamma)};
}

static bs::Result americanGreeks(bool is_call, double S, double K, double r,
                                 double q, double sigma, double T, int steps) {
  auto price = [=](double s, double rr, double vol, double time) {
    return priceAmericanBinomial(is_call, s, K, rr, q, vol, time, steps).price;
  };

  const double hVol = std::min(0.001, 0.5 * sigma);
  const double hR = 0.0001;
  const double hT = std::min(1.0 / 365.0, 0.5 * T);
  const auto base = priceAmericanBinomial(is_call, S, K, r, q, sigma, T, steps);
  const double pVu = price(S, r, sigma + hVol, T);
  const double pVd = price(S, r, sigma - hVol, T);
  const double pRu = price(S, r + hR, sigma, T);
  const double pRd = price(S, r - hR, sigma, T);
  const double pTu = price(S, r, sigma, T + hT);
  const double pTd = price(S, r, sigma, T - hT);

  return {base.price,
          base.delta,
          base.gamma,
          (pVu - pVd) / (2.0 * hVol),
          -(pTu - pTd) / (2.0 * hT),
          (pRu - pRd) / (2.0 * hR)};
}

} // namespace

namespace bs {

double normPdf(double x) {
  static constexpr double INV_SQRT_2PI{0.39894228040143267794};
  return INV_SQRT_2PI * std::exp(-0.5 * x * x);
}

double normCdf(double x) {
  static constexpr double SQRT_2{1.41421356237309504880};
  return 0.5 * std::erfc(-x / SQRT_2);
}

// Black–Scholes European
Result blackScholes(Type type, double S, double K, double r, double q,
                    double sigma, double T) {
  if (!(S > 0.0) || !(K > 0.0) || !(sigma > 0.0) || !(T > 0.0)) {
    return {NAN, NAN, NAN, NAN, NAN, NAN};
  }

  const double sqrtT = std::sqrt(T);
  const double sigmaSqrtT = sigma * sqrtT;
  const double d1 =
      (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / sigmaSqrtT;
  const double d2 = d1 - sigmaSqrtT;
  const double Nd1 = normCdf(d1);
  const double Nd2 = normCdf(d2);
  const double Nmd1 = normCdf(-d1);
  const double Nmd2 = normCdf(-d2);
  const double phi1 = normPdf(d1);
  const double discountedR = std::exp(-r * T);
  const double discountedQ = std::exp(-q * T);
  const bool isCall = (type == Type::Call);

  // Price
  const double price = isCall ? S * discountedQ * Nd1 - K * discountedR * Nd2
                              : K * discountedR * Nmd2 - S * discountedQ * Nmd1;

  // Greeks
  const double delta = isCall ? discountedQ * Nd1 : discountedQ * (Nd1 - 1.0);
  const double gamma = discountedQ * phi1 / (S * sigmaSqrtT);
  const double vega = S * discountedQ * phi1 * sqrtT;

  const double thetaCommon = -(S * discountedQ * phi1 * sigma) / (2.0 * sqrtT);
  const double theta = isCall ? thetaCommon - r * K * discountedR * Nd2 +
                                    q * S * discountedQ * Nd1
                              : thetaCommon + r * K * discountedR * Nmd2 -
                                    q * S * discountedQ * Nmd1;

  const double rho =
      isCall ? K * T * discountedR * Nd2 : -K * T * discountedR * Nmd2;

  return {price, delta, gamma, vega, theta, rho};
}

// Binary cash-or-nothing
Result binaryCashOrNothing(Type type, double S, double K, double r, double q,
                           double sigma, double T, double payout) {
  if (!(S > 0.0) || !(K > 0.0) || !(sigma > 0.0) || !(T > 0.0)) {
    return {NAN, NAN, NAN, NAN, NAN, NAN};
  }

  const bool isCall = (type == Type::Call);

  const double sqrtT = std::sqrt(T);
  const double sigmaSqrtT = sigma * sqrtT;

  const double d1 =
      (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / sigmaSqrtT;
  const double d2 = d1 - sigmaSqrtT;

  const double disc = std::exp(-r * T);
  const double Nd2 = bs::normCdf(d2);
  const double Nmd2 = bs::normCdf(-d2);
  const double phi2 = bs::normPdf(d2);

  // Price
  const double price = payout * disc * (isCall ? Nd2 : Nmd2);

  // Greeks
  // Delta
  const double delta_sign = isCall ? +1.0 : -1.0;
  const double delta = delta_sign * payout * disc * (phi2 / (S * sigmaSqrtT));

  // Gamma
  // Call:  -Q e^{-rT} d1 phi(d2) / (S^2 sigma^2 T)
  // Put :  +Q e^{-rT} d1 phi(d2) / (S^2 sigma^2 T)
  const double gamma_sign = isCall ? -1.0 : +1.0;
  const double gamma =
      gamma_sign * payout * disc * (d1 * phi2) / (S * S * sigma * sigma * T);

  // Vega
  // Call: -Q e^{-rT} (d1/sigma) phi(d2)
  // Put : +Q e^{-rT} (d1/sigma) phi(d2)
  const double vega_sign = isCall ? -1.0 : +1.0;
  const double vega = vega_sign * payout * disc * (d1 * phi2 / sigma);

  const double d2T =
      (r - q - 0.5 * sigma * sigma) / (sigma * sqrtT) - d2 / (2.0 * T);
  const double theta =
      payout * disc *
      (isCall ? (r * Nd2 - phi2 * d2T) : (r * Nmd2 + phi2 * d2T));

  // Rho
  // Call:  Q e^{-rT} [ -T Phi(d2) + (sqrt(T)/sigma) phi(d2) ]
  // Put :  Q e^{-rT} [ -T Phi(-d2) - (sqrt(T)/sigma) phi(d2) ]
  const double rho = payout * disc *
                     (isCall ? (-T * Nd2 + (sqrtT / sigma) * phi2)
                             : (-T * Nmd2 - (sqrtT / sigma) * phi2));

  return {price, delta, gamma, vega, theta, rho};
}

// American option
Result americanOption(Type type, double S, double K, double r, double q,
                      double sigma, double T, int steps) {
  if (!(S > 0.0) || !(K > 0.0) || !(sigma > 0.0) || !(T > 0.0) || steps < 2) {
    return {NAN, NAN, NAN, NAN, NAN, NAN};
  }
  return americanGreeks(type == Type::Call, S, K, r, q, sigma, T, steps);
}

} // namespace bs
