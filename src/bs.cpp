#include "bs.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

struct FDSteps {
  double hS, hsigma, hr, hT;
  FDSteps(double S, double sigma, double r, double T)
      : hS(std::max(1e-6, 1e-4 * S)), hsigma(std::max(1e-6, 1e-4 * sigma)),
        hr(std::max(1e-6, 1e-4 * std::fabs(r) + 1e-6)),
        hT(std::max(1e-6, 1e-4 * T + 1e-6)) {}
};

template <class PriceFunc>
bs::Result finiteDiffGreeks(PriceFunc price, double S, double K, double r,
                            double q, double sigma, double T) {
  FDSteps h(S, sigma, r, T);

  const double P0 = price(S, K, r, q, sigma, T);
  const double PSu = price(S + h.hS, K, r, q, sigma, T);
  const double PSd = price(S - h.hS, K, r, q, sigma, T);
  const double Pvup = price(S, K, r, q, sigma + h.hsigma, T);
  const double Pvdo = price(S, K, r, q, sigma - h.hsigma, T);
  const double Prup = price(S, K, r + h.hr, q, sigma, T);
  const double Prdo = price(S, K, r - h.hr, q, sigma, T);

  const double Ptup = price(S, K, r, q, sigma, T + h.hT);
  const double Tm = (T - h.hT > 1e-8) ? (T - h.hT) : std::max(1e-8, 0.5 * T);
  const double Ptdo = price(S, K, r, q, sigma, Tm);

  bs::Result R;
  R.price = P0;
  R.delta = (PSu - PSd) / (2.0 * h.hS);
  R.gamma = (PSu - 2.0 * P0 + PSd) / (h.hS * h.hS);
  R.vega = (Pvup - Pvdo) / (2.0 * h.hsigma);
  R.rho = (Prup - Prdo) / (2.0 * h.hr);
  R.theta = -(Ptup - Ptdo) / ((T + h.hT) - Tm);
  return R;
}

// American option via CRR binomial with early exercise
static inline double priceAmericanBinomialCore(bool is_call, double S, double K,
                                               double r, double q, double sigma,
                                               double T, int steps) {
  if (steps < 1)
    steps = 1;

  const double dt = T / steps;
  const double u = std::exp(sigma * std::sqrt(dt));
  const double d = 1.0 / u;
  const double disc = std::exp(-r * dt);
  const double a = std::exp((r - q) * dt);
  const double p = (a - d) / (u - d);
  if (!std::isfinite(p) || p < 0.0 || p > 1.0)
    return NAN;

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

  // backward induction with early exercise
  for (int step = steps - 1; step >= 0; --step) {
    for (int i = 0; i <= step; ++i) {
      ST[i] = ST[i] / d; // roll down
      const double cont = disc * (p * V[i + 1] + (1.0 - p) * V[i]);
      const double exer =
          is_call ? std::max(ST[i] - K, 0.0) : std::max(K - ST[i], 0.0);
      V[i] = std::max(cont, exer);
    }
  }
  return V[0];
}

static inline double priceAmericanBinomial(bool is_call, double S, double K,
                                           double r, double q, double sigma,
                                           double T, int steps) {
  const double a = priceAmericanBinomialCore(is_call, S, K, r, q, sigma, T,
                                              steps);
  const double b = priceAmericanBinomialCore(is_call, S, K, r, q, sigma, T,
                                              steps + 1);
  if (!std::isfinite(a) || !std::isfinite(b))
    return NAN;
  const double intrinsic =
      is_call ? std::max(S - K, 0.0) : std::max(K - S, 0.0);
  return std::max(intrinsic, 0.5 * (a + b));
}

static bs::Result americanFiniteDiffGreeks(bool is_call, double S, double K,
                                           double r, double q, double sigma,
                                           double T, int steps) {
  auto price = [=](double s, double rr, double vol, double time) {
    return priceAmericanBinomial(is_call, s, K, rr, q, vol, time, steps);
  };

  const double hS = std::max(1e-4, 0.005 * S);
  const double hVol = std::min(0.001, 0.5 * sigma);
  const double hR = 0.0001;
  const double hT = std::min(1.0 / 365.0, 0.5 * T);
  const double p0 = price(S, r, sigma, T);
  const double pSu = price(S + hS, r, sigma, T);
  const double pSd = price(S - hS, r, sigma, T);
  const double pVu = price(S, r, sigma + hVol, T);
  const double pVd = price(S, r, sigma - hVol, T);
  const double pRu = price(S, r + hR, sigma, T);
  const double pRd = price(S, r - hR, sigma, T);
  const double pTu = price(S, r, sigma, T + hT);
  const double pTd = price(S, r, sigma, T - hT);

  return {p0,
          (pSu - pSd) / (2.0 * hS),
          (pSu - 2.0 * p0 + pSd) / (hS * hS),
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
  if (!(S > 0.0) || !(K > 0.0) || !(sigma > 0.0) || !(T > 0.0) || steps < 1) {
    return {NAN, NAN, NAN, NAN, NAN, NAN};
  }
  return americanFiniteDiffGreeks(type == Type::Call, S, K, r, q, sigma, T,
                                  steps);
}

} // namespace bs
