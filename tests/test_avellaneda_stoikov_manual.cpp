/**

 * Avellaneda–Stoikov strategy: reservation price, spreads, sensitivities, and robustness.

 * Spread formula: gamma*sigma^2*tau + (2/gamma)*ln(1+gamma/kappa) — the second term falls as gamma

 * grows, so total spread need not increase monotonically with gamma; we verify the risk term does.

 */



#include "mmsim/avellaneda_stoikov.h"

#include "mmsim/types.h"



#include <cmath>

#include <cstdio>

#include <cstdlib>

#include <iomanip>

#include <iostream>

#include <optional>

namespace {



using mmsim::AsQuotes;

using mmsim::AvellanedaStoikovStrategy;

using mmsim::Price;

using mmsim::Quantity;



int g_failures = 0;



void fail(const char* msg) {

    std::fprintf(stderr, "FAIL: %s\n", msg);

    ++g_failures;

}



void pass(const char* msg) {

    std::printf("PASS: %s\n", msg);

}



[[nodiscard]] Price mid_price() {

    return static_cast<Price>(std::llround(100.0 * 10000.0));

}



[[nodiscard]] double human(Price p) {

    return static_cast<double>(p) / 10000.0;

}

} // namespace



int main() {

    std::printf("=== test_avellaneda_stoikov_manual ===\n\n");



    const Price mid = mid_price();

    const double sigma = 0.02;

    const double tau = 1.0;

    const double gamma = 0.1;

    const double kappa = 1.5;



    AvellanedaStoikovStrategy as(AvellanedaStoikovStrategy::Config{.gamma = gamma, .kappa = kappa});



    // --- 1. Zero inventory ---

    std::printf("--- 1. Zero inventory (gamma=%.2f, sigma=%.2f, kappa=%.2f, T-t=%.2f) ---\n", gamma,

                sigma, kappa, tau);

    {

        const Quantity q = 0;

        const double r = as.reservation_price(mid, q, sigma, tau);

        const std::optional<AsQuotes> oq = as.optimal_quotes(mid, q, sigma, tau);



        const double mid_d = static_cast<double>(mid);

        const double spread_d = as.optimal_spread(sigma, tau);



        std::cout << std::fixed << std::setprecision(8);

        std::cout << "| Field              | Value (raw)   | Human price  |\n";

        std::cout << "|--------------------+---------------+-------------|\n";

        std::cout << "| mid                | " << std::setw(13) << mid_d << " | " << std::setw(11) << human(mid)

                  << " |\n";

        std::cout << "| reservation_price  | " << std::setw(13) << r << " | " << std::setw(11)

                  << r / 10000.0 << " |\n";

        if (oq.has_value()) {

            std::cout << "| bid                | " << std::setw(13) << static_cast<double>(oq->bid) << " | "

                      << std::setw(11) << human(oq->bid) << " |\n";

            std::cout << "| ask                | " << std::setw(13) << static_cast<double>(oq->ask) << " | "

                      << std::setw(11) << human(oq->ask) << " |\n";

        }

        std::cout << "| spread (double)    | " << std::setw(13) << spread_d << " | (sigma units) |\n";

        std::cout << '\n';



        const double tol = 1e-6 * std::max(1.0, std::abs(mid_d));

        bool ok = std::isfinite(r) && std::abs(r - mid_d) < tol;

        if (oq.has_value()) {

            const double center =

                0.5 * (static_cast<double>(oq->bid) + static_cast<double>(oq->ask));

            const double half_up = static_cast<double>(oq->ask) - mid_d;

            const double half_dn = mid_d - static_cast<double>(oq->bid);

            ok = ok && std::abs(center - r) < 1.0 && std::abs(half_up - half_dn) < 1.0;

        } else {

            ok = false;

        }

        if (!ok) {

            fail("1. r=mid, symmetric quotes around mid");

        } else {

            pass("1. Zero inventory: r = mid; bid/ask symmetric (within tick rounding)");

        }

        std::printf("    Printed spread (optimal_spread) = %.10f\n", spread_d);

    }



    std::printf("\n");



    // --- 2. Long inventory +20 ---

    std::printf("--- 2. Long inventory q=+20 ---\n");

    {

        const Quantity q_long = 20;

        const double spread = as.optimal_spread(sigma, tau);

        const double r_long = as.reservation_price(mid, q_long, sigma, tau);

        const auto flat = as.optimal_quotes(mid, 0, sigma, tau);

        const auto lng = as.optimal_quotes(mid, q_long, sigma, tau);

        const double r0 = as.reservation_price(mid, 0, sigma, tau);

        const double skew = r0 - r_long;

        const double mid_d = static_cast<double>(mid);

        const double ask_flat_d = r0 + spread * 0.5;

        const double ask_long_d = r_long + spread * 0.5;



        std::cout << "| Case        | reservation (human) | ask (rounded) | ask (pre-round double) |\n";

        std::cout << "|-------------+---------------------+---------------+------------------------|\n";

        std::cout << "| q=0         | " << std::setw(19) << human(mid) << " | " << std::setw(13)

                  << (flat.has_value() ? human(flat->ask) : -1.0) << " | " << std::setw(22) << ask_flat_d

                  << " |\n";

        std::cout << "| q=+20       | " << std::setw(19) << r_long / 10000.0 << " | " << std::setw(13)

                  << (lng.has_value() ? human(lng->ask) : -1.0) << " | " << std::setw(22) << ask_long_d

                  << " |\n";

        std::cout << "| skew r0-r   | " << std::setw(19) << skew / 10000.0 << " |               |\n";

        std::cout << '\n';



        // Sub-tick reservation shift: compare doubles before integer rounding to Price ticks

        bool ok = r_long < mid_d && r_long < r0 && ask_long_d < ask_flat_d && std::isfinite(ask_long_d);

        if (!ok) {

            fail("2. Long: r < mid, theoretical ask lower than flat");

        } else {

            pass("2. Long inventory: reservation below mid; pre-round ask below flat case");

        }

        std::printf("    Skew (mid - r), raw units: %.6f  human price: %.8f\n", skew, skew / 10000.0);

    }



    std::printf("\n");



    // --- 3. Short inventory -20 ---

    std::printf("--- 3. Short inventory q=-20 ---\n");

    {

        const Quantity q_short = -20;

        const double spread = as.optimal_spread(sigma, tau);

        const double r_short = as.reservation_price(mid, q_short, sigma, tau);

        const auto flat = as.optimal_quotes(mid, 0, sigma, tau);

        const auto sh = as.optimal_quotes(mid, q_short, sigma, tau);

        const double r0 = as.reservation_price(mid, 0, sigma, tau);

        const double mid_d = static_cast<double>(mid);

        const double bid_flat_d = r0 - spread * 0.5;

        const double bid_short_d = r_short - spread * 0.5;



        std::cout << "| Case        | reservation (human) | bid (rounded) | bid (pre-round double) |\n";

        std::cout << "|-------------+---------------------+---------------+------------------------|\n";

        std::cout << "| q=0         | " << std::setw(19) << human(mid) << " | " << std::setw(13)

                  << (flat.has_value() ? human(flat->bid) : -1.0) << " | " << std::setw(22) << bid_flat_d

                  << " |\n";

        std::cout << "| q=-20       | " << std::setw(19) << r_short / 10000.0 << " | " << std::setw(13)

                  << (sh.has_value() ? human(sh->bid) : -1.0) << " | " << std::setw(22) << bid_short_d

                  << " |\n";

        std::cout << '\n';



        bool ok = r_short > mid_d && r_short > r0 && bid_short_d > bid_flat_d && std::isfinite(bid_short_d);

        if (!ok) {

            fail("3. Short: r > mid, theoretical bid higher than flat");

        } else {

            pass("3. Short inventory: reservation above mid; pre-round bid above flat case");

        }

    }



    std::printf("\n");



    // --- 4. Volatility sensitivity ---

    std::printf("--- 4. Volatility sensitivity (q=0, gamma=%.2f, T-t=%.2f) ---\n", gamma, tau);

    {

        std::cout << "| sigma | spread (double) |\n";

        std::cout << "|-------+------------------|\n";

        double prev = -1.0;

        bool mono = true;

        for (int i = 1; i <= 10; ++i) {

            const double sig = 0.01 * static_cast<double>(i);

            const double sp = as.optimal_spread(sig, tau);

            std::cout << "| " << std::setw(4) << sig << " | " << std::fixed << std::setprecision(8)

                      << std::setw(16) << sp << " |\n";

            if (prev >= 0.0 && !(sp > prev + 1e-15)) {

                mono = false;

            }

            prev = sp;

        }

        std::cout << '\n';

        if (!mono) {

            fail("4. Spread increases monotonically with sigma");

        } else {

            pass("4. optimal_spread(sigma) strictly increasing in sigma on [0.01, 0.10]");

        }

    }



    std::printf("\n");



    // --- 5. Risk aversion: risk term vs full spread ---

    std::printf("--- 5. Gamma sensitivity (q=0, sigma=%.2f, T-t=%.2f) ---\n", sigma, tau);

    std::printf(

        "    Note: full spread = gamma*sigma^2*tau + (2/gamma)*ln(1+gamma/kappa); the second term\n"

        "    falls when gamma rises, so total spread need not increase monotonically with gamma.\n"

        "    We verify the risk component gamma*sigma^2*tau increases with gamma.\n");

    {

        std::cout << "| gamma | risk term g*s^2*t | full spread |\n";

        std::cout << "|-------+--------------------+-------------|\n";

        const double s2t = sigma * sigma * tau;

        double prev_risk = -1.0;

        bool risk_mono = true;

        for (int k = 1; k <= 100; ++k) {

            const double g = 0.01 * static_cast<double>(k);

            AvellanedaStoikovStrategy sg(AvellanedaStoikovStrategy::Config{.gamma = g, .kappa = kappa});

            const double risk = g * s2t;

            const double full = sg.optimal_spread(sigma, tau);

            if (prev_risk >= 0.0 && !(risk > prev_risk + 1e-18)) {

                risk_mono = false;

            }

            prev_risk = risk;

            if (k <= 10 || k % 10 == 0 || k == 100) {

                std::cout << "| " << std::setw(5) << g << " | " << std::fixed << std::setprecision(8)

                          << std::setw(18) << risk << " | " << std::setw(11) << full << " |\n";

            }

        }

        std::cout << '\n';

        if (!risk_mono) {

            fail("5. Risk component gamma*sigma^2*tau increases with gamma");

        } else {

            pass("5. Risk component gamma*sigma^2*tau strictly increasing in gamma on grid");

        }

    }



    std::printf("\n");



    // --- 6. Extreme inventory ---

    std::printf("--- 6. Extreme inventory ---\n");

    {

        // Large but bounded (no int64 Price wraparound in display)
        const Quantity extremes[] = {1000000LL, 25000000LL, 80000000LL};

        std::cout << "| inventory | finite r? | quotes? | bid (human) | ask (human) |\n";

        std::cout << "|-----------+-----------+---------+-------------+-------------|\n";

        bool ok = true;

        for (Quantity q : extremes) {

            const double r = as.reservation_price(mid, q, sigma, tau);

            const auto oq = as.optimal_quotes(mid, q, sigma, tau);

            const bool fin = std::isfinite(r);

            const bool has = oq.has_value();

            std::cout << "| " << std::setw(9) << q << " | " << std::setw(9) << (fin ? "yes" : "no")

                      << " | " << std::setw(7) << (has ? "yes" : "no") << " | ";

            if (has) {

                std::cout << std::setw(11) << human(oq->bid) << " | " << std::setw(11)

                          << human(oq->ask) << " |\n";

            } else {

                std::cout << std::setw(11) << "n/a"

                          << " | " << std::setw(11) << "n/a"

                          << " |\n";

            }

            if (!fin) {

                ok = false;

            }

            if (has) {

                const double bd = static_cast<double>(oq->bid);

                const double ad = static_cast<double>(oq->ask);

                if (!std::isfinite(bd) || !std::isfinite(ad) || bd >= ad) {

                    ok = false;

                }

            }

        }

        std::cout << '\n';

        if (!ok) {

            fail("6. Extreme inventory: finite prices, no NaN, or valid nullopt");

        } else {

            pass("6. Extreme inventory: no NaN/Inf; strategy may omit quotes if tick rounding invalid");

        }

    }



    std::printf("\n");



    // --- 7. Time decay ---

    std::printf("--- 7. Time decay (q=0, gamma=%.2f, sigma=%.2f) ---\n", gamma, sigma);

    {

        std::cout << "| T-t   | spread (double) | risk term |\n";

        std::cout << "|-------+-----------------+-----------|\n";

        const double taus[] = {1.0, 0.5, 0.25, 0.1, 0.05, 0.02, 0.01, 0.005, 0.001};

        double sp_prev_row = -1.0;

        bool narrow = true;

        for (double t : taus) {

            const double sp = as.optimal_spread(sigma, t);

            const double risk = gamma * sigma * sigma * t;

            std::cout << "| " << std::setw(5) << t << " | " << std::fixed << std::setprecision(8)

                      << std::setw(15) << sp << " | " << std::setw(9) << risk << " |\n";

            if (sp_prev_row >= 0.0 && !(sp < sp_prev_row + 1e-12)) {

                narrow = false;

            }

            sp_prev_row = sp;

        }

        std::cout << '\n';

        // tau = 0 still valid

        const double sp0 = as.optimal_spread(sigma, 0.0);

        const bool ok0 = std::isfinite(sp0) && sp0 >= 0.0;

        std::printf("    spread at T-t=0: %.10f (finite, intensity term only)\n", sp0);

        const auto q0 = as.optimal_quotes(mid, 0, sigma, 0.0);

        std::printf("    quotes at T-t=0: %s\n", q0.has_value() ? "ok" : "nullopt");



        if (!narrow || !ok0) {

            fail("7. Spread narrows as T-t decreases; tau=0 safe");

        } else {

            pass("7. Spread decreases as T-t decreases; tau=0 yields finite spread and quotes");

        }

    }



    std::printf("\n--- Summary: %d failure(s) ---\n", g_failures);

    return g_failures > 0 ? EXIT_FAILURE : EXIT_SUCCESS;

}


