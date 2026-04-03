/**

 * Volatility estimator integration tests: constant price, GBM calibration,

 * window efficiency, trending series, and spike recovery.

 */



#include "mmsim/types.h"

#include "mmsim/volatility.h"



#include <cmath>

#include <cstdio>

#include <cstdlib>

#include <iomanip>

#include <iostream>

#include <random>

#include <span>

#include <tuple>

#include <vector>



namespace {



using mmsim::OhlcBar;

using mmsim::Price;

using mmsim::VolatilityEstimator;



int g_failures = 0;



void fail(const char* msg) {

    std::fprintf(stderr, "FAIL: %s\n", msg);

    ++g_failures;

}



void pass(const char* msg) {

    std::printf("PASS: %s\n", msg);

}



[[nodiscard]] Price human(double x) {

    return static_cast<Price>(std::llround(x * 10000.0));

}



[[nodiscard]] std::vector<OhlcBar> build_ohlc_from_closes(const std::vector<Price>& closes,

                                                           double sigma_intra, std::mt19937_64& rng) {

    std::normal_distribution<double> nz(0.0, 1.0);

    std::vector<OhlcBar> hist;

    hist.reserve(closes.size());

    for (std::size_t i = 0; i < closes.size(); ++i) {

        if (i == 0) {

            hist.push_back(OhlcBar{closes[0], closes[0], closes[0], closes[0]});

            continue;

        }

        const double o = static_cast<double>(closes[i - 1]);

        const double c = static_cast<double>(closes[i]);

        const double mx = std::max(o, c);

        const double mn = std::min(o, c);

        const double zh = std::abs(nz(rng));

        const double zl = std::abs(nz(rng));

        const double spread = 0.35 * sigma_intra;

        OhlcBar b{};

        b.open = closes[i - 1];

        b.close = closes[i];

        b.high = static_cast<Price>(std::llround(mx * (1.0 + spread * zh)));

        b.low_price = static_cast<Price>(std::llround(std::max(1.0, mn * (1.0 - spread * zl))));

        if (b.high < b.low_price) {

            std::swap(b.high, b.low_price);

        }

        hist.push_back(b);

    }

    return hist;

}



[[nodiscard]] std::vector<Price> gbm_closes(std::size_t n, double mu, double sigma, double s0,

                                            std::mt19937_64& rng) {

    std::normal_distribution<double> zdist(0.0, 1.0);

    std::vector<Price> closes;

    closes.reserve(n);

    double s = s0;

    closes.push_back(static_cast<Price>(std::llround(s)));

    for (std::size_t i = 1; i < n; ++i) {

        const double z = zdist(rng);

        const double log_ret = (mu - 0.5 * sigma * sigma) + sigma * z;

        s = s * std::exp(log_ret);

        closes.push_back(static_cast<Price>(std::llround(s)));

    }

    return closes;

}



void print_table_header() {

    std::cout << std::left << std::setw(14) << "| Window Size" << std::setw(14) << "| CC Vol"

              << std::setw(16) << "| Parkinson Vol" << std::setw(18) << "| Yang-Zhang Vol"

              << std::setw(12) << "| True Vol |\n";

    std::cout << std::string(86, '-') << '\n';

}



} // namespace



int main() {

    std::printf("=== test_volatility_estimators_manual ===\n\n");



    constexpr double kTrueVol = 0.02;

    constexpr double kTolFlat = 1e-5;

    constexpr double kGbmRelTol = 0.20;

    std::mt19937_64 rng(42);



    // --- 1. Constant price (1000 ticks @ 100.00) ---

    {

        const Price p = human(100.0);

        std::vector<Price> closes(1000, p);

        std::vector<OhlcBar> hist(1000, OhlcBar{p, p, p, p});



        const std::size_t w = 200;

        VolatilityEstimator est(w);

        const auto cc = est.close_to_close(closes);

        const auto pk = est.parkinson(std::span<const OhlcBar>(hist.data() + (hist.size() - w), w));

        const auto yz = est.yang_zhang(hist);



        bool ok = cc.has_value() && pk.has_value() && yz.has_value();

        if (ok) {

            ok = (std::abs(*cc) < kTolFlat) && (std::abs(*pk) < kTolFlat) && (std::abs(*yz) < kTolFlat);

        }

        if (!ok) {

            fail("1. constant price: all estimators ~0");

            if (cc.has_value()) {

                std::fprintf(stderr, "  CC=%.8g PK=%s YZ=%s\n", *cc,

                             pk.has_value() ? "ok" : "null", yz.has_value() ? "ok" : "null");

            }

        } else {

            pass("1. Constant price 1000x100.00: CC, Parkinson, Yang-Zhang ~0");

        }

        std::printf("    CC=%.6e  Parkinson=%.6e  Yang-Zhang=%.6e\n", *cc, *pk, *yz);

    }



    std::printf("\n");



    // --- 2. GBM known volatility (10000 steps), check within 20% of 0.02 ---

    {

        rng.seed(42);

        std::vector<Price> closes =

            gbm_closes(10000, 0.0, kTrueVol, static_cast<double>(human(100.0)), rng);

        std::vector<OhlcBar> hist = build_ohlc_from_closes(closes, kTrueVol, rng);



        const std::size_t w_check = 2000;

        VolatilityEstimator est(w_check);

        const auto cc = est.close_to_close(closes);

        est.set_window(w_check);

        const auto pk =

            est.parkinson(std::span<const OhlcBar>(hist.data() + (hist.size() - w_check), w_check));

        const auto yz = est.yang_zhang(hist);



        const double lo = kTrueVol * (1.0 - kGbmRelTol);

        const double hi = kTrueVol * (1.0 + kGbmRelTol);



        auto in_band = [lo, hi](double x) { return x >= lo && x <= hi; };



        std::printf("--- 2. GBM (mu=0, sigma=0.02), n=10000, window=%zu (20%% band [%.4f, %.4f]) ---\n",

                    w_check, lo, hi);

        std::printf("    CC=%.6f  Parkinson=%.6f  Yang-Zhang=%.6f\n", cc.value_or(-1.0),

                    pk.value_or(-1.0), yz.value_or(-1.0));



        bool ok = cc.has_value() && pk.has_value() && yz.has_value() && in_band(*cc) && in_band(*pk) &&

                  in_band(*yz);

        if (!ok) {

            fail("2. GBM: all three within 20% of 0.02");

        } else {

            pass("2. GBM: all estimators within 20% of true 0.02");

        }

    }



    std::printf("\n");



    // --- 3. Efficiency table (same GBM path; OHLC intra-bar uses fixed seed 99) ---

    {

        std::mt19937_64 r_path(42);

        std::vector<Price> closes =

            gbm_closes(10000, 0.0, kTrueVol, static_cast<double>(human(100.0)), r_path);

        std::mt19937_64 r_ohlc(99);

        std::vector<OhlcBar> hist = build_ohlc_from_closes(closes, kTrueVol, r_ohlc);



        std::printf("--- 3. Window efficiency (same synthetic series, GBM seed=42, OHLC seed=99) ---\n");

        print_table_header();



        const std::size_t windows[] = {20, 50, 100, 200};

        for (std::size_t w : windows) {

            VolatilityEstimator est(w);

            const auto cc = est.close_to_close(closes);

            const auto pk = est.parkinson(std::span<const OhlcBar>(hist.data() + (hist.size() - w), w));

            const auto yz = est.yang_zhang(hist);



            std::cout << "| " << std::setw(11) << w << " | " << std::fixed << std::setprecision(6)

                      << std::setw(11) << cc.value_or(-1.0) << " | " << std::setw(13)

                      << pk.value_or(-1.0) << " | " << std::setw(15) << yz.value_or(-1.0) << " | "

                      << std::setw(9) << kTrueVol << " |\n";

        }

        std::cout << '\n';



        VolatilityEstimator e20(20);

        VolatilityEstimator e20p(20);

        VolatilityEstimator e20y(20);

        const double cc20 = e20.close_to_close(closes).value_or(999.0);

        const double pk20 =

            e20p.parkinson(std::span<const OhlcBar>(hist.data() + (hist.size() - 20), 20)).value_or(999.0);

        const double yz20 = e20y.yang_zhang(hist).value_or(999.0);

        const double err_cc = std::abs(cc20 - kTrueVol);

        const double err_pk = std::abs(pk20 - kTrueVol);

        const double err_yz = std::abs(yz20 - kTrueVol);



        std::printf("    At window=20: |CC-0.02|=%.6f  |Parkinson-0.02|=%.6f  |YZ-0.02|=%.6f\n", err_cc,

                    err_pk, err_yz);

        std::printf("    Point estimates at w=20: CC=%.6f  PK=%.6f  YZ=%.6f (YZ often lowest σ-hat with range info)\n",

                    cc20, pk20, yz20);

        const char* closest = "CC";

        double best = err_cc;

        if (err_pk < best) {

            best = err_pk;

            closest = "Parkinson";

        }

        if (err_yz < best) {

            closest = "Yang-Zhang";

        }

        std::printf("    Closest to true vol at w=20 (this sample): %s\n", closest);

        VolatilityEstimator e200(200);

        const double cc200 = e200.close_to_close(closes).value_or(-1.0);

        const double pk200 =

            e200.parkinson(std::span<const OhlcBar>(hist.data() + (hist.size() - 200), 200)).value_or(-1.0);

        const double yz200 = e200.yang_zhang(hist).value_or(-1.0);

        const double lo3 = kTrueVol * 0.75;

        const double hi3 = kTrueVol * 1.35;

        const bool sane = cc200 >= lo3 && cc200 <= hi3 && pk200 >= lo3 && pk200 <= hi3 && yz200 >= lo3 &&

                          yz200 <= hi3;

        if (!sane) {

            fail("3. Estimators at w=200 should cluster near true vol (sanity check)");

        } else {

            pass("3. Efficiency table printed; at w=200 all estimators near 0.02 (YZ often best at small w in "

                 "theory—see closest line above)");

        }

    }



    std::printf("\n");



    // --- 4. Trending linear price 100 -> 200 over 1000 ticks ---

    {

        std::vector<Price> closes(1000);

        std::vector<OhlcBar> hist;

        hist.reserve(1000);

        for (std::size_t i = 0; i < 1000; ++i) {

            const double px = 100.0 + 100.0 * static_cast<double>(i) / 999.0;

            closes[i] = human(px);

        }

        for (std::size_t i = 0; i < 1000; ++i) {

            if (i == 0) {

                hist.push_back(OhlcBar{closes[0], closes[0], closes[0], closes[0]});

                continue;

            }

            const Price o = closes[i - 1];

            const Price c = closes[i];

            const Price mx = std::max(o, c);

            const Price mn = std::min(o, c);

            hist.push_back(OhlcBar{o, mx, mn, c});

        }



        const std::size_t w = 200;

        VolatilityEstimator est(w);

        const auto cc = est.close_to_close(closes);

        const auto pk = est.parkinson(std::span<const OhlcBar>(hist.data() + (hist.size() - w), w));

        const auto yz = est.yang_zhang(hist);



        std::printf("--- 4. Linear trend 100->200 (1000 ticks), window=%zu ---\n", w);

        std::printf("    CC=%.6f  Parkinson=%.6f  Yang-Zhang=%.6f\n", cc.value_or(-1.0), pk.value_or(-1.0),

                    yz.value_or(-1.0));



        const double cap = 0.15;

        bool ok = cc.has_value() && pk.has_value() && yz.has_value();

        if (ok) {

            ok = (*cc < cap && *pk < cap && *yz < cap && *yz <= *cc * 1.01 + 1e-6);

        }

        if (!ok) {

            fail("4. Trend: bounded vol and YZ not worse than CC");

        } else {

            pass("4. Trending series: vol bounded; Yang-Zhang <= CC (handles trend better)");

        }

    }



    std::printf("\n");



    // --- 5. Spike: 1000 normal, +10% spike, 1000 normal ---

    {

        std::mt19937_64 r_spike(7);

        std::vector<Price> closes =

            gbm_closes(2001, 0.0, 0.015, static_cast<double>(human(100.0)), r_spike);

        {

            const double spiked = static_cast<double>(closes[1000]) * 1.10;

            closes[1000] = static_cast<Price>(std::llround(spiked));

        }

        std::mt19937_64 r_ohlc2(11);

        std::vector<OhlcBar> hist = build_ohlc_from_closes(closes, 0.015, r_ohlc2);



        const std::size_t w = 150;



        auto eval_at = [&](std::size_t end_exclusive) -> std::tuple<double, double, double> {

            std::vector<Price> sub(closes.begin(), closes.begin() + static_cast<std::ptrdiff_t>(end_exclusive));

            std::vector<OhlcBar> hsub(hist.begin(), hist.begin() + static_cast<std::ptrdiff_t>(end_exclusive));

            VolatilityEstimator e(w);

            const double c = e.close_to_close(sub).value_or(-1.0);

            const double p =

                e.parkinson(std::span<const OhlcBar>(hsub.data() + (hsub.size() - w), w)).value_or(-1.0);

            const double y = e.yang_zhang(hsub).value_or(-1.0);

            return {c, p, y};

        };



        const auto [cc_spike, pk_spike, yz_spike] = eval_at(1100);

        const auto [cc_end, pk_end, yz_end] = eval_at(2001);



        std::printf("--- 5. Spike (+10%% at t=1000), window=%zu ---\n", w);

        std::printf("    With spike in window (n=1100): CC=%.6f  PK=%.6f  YZ=%.6f\n", cc_spike, pk_spike,

                    yz_spike);

        std::printf("    After spike left window (n=2001): CC=%.6f  PK=%.6f  YZ=%.6f\n", cc_end, pk_end,

                    yz_end);



        const double baseline = 0.015;

        auto settled = [&](double x) { return x < baseline * 3.0 && x < 0.12; };



        bool ok = settled(cc_end) && settled(pk_end) && settled(yz_end);

        if (!ok) {

            fail("5. Post-spike estimators return to normal range");

        } else {

            pass("5. Spike exits window: vol estimates recover (not permanently exploded)");

        }

    }



    std::printf("\n--- Summary: %d failure(s) ---\n", g_failures);

    return g_failures > 0 ? EXIT_FAILURE : EXIT_SUCCESS;

}


