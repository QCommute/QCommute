#pragma once

#include <vector>
#include <map>
#include <functional>
#include "BigInt.h"

class Poly;

/**
 * @brief Sparse multivariate polynomial with arbitrary-precision coefficients.
 *
 * This is the "output" polynomial type used for statistics (moments and quench
 * coefficients).  Unlike Poly, which stores its monomials in a compact packed
 * array optimised for fast arithmetic, Poly2 uses a `std::map` from degree
 * vector to coefficient.  This makes accumulation via `operator+=` simple and
 * efficient when the number of distinct monomials is large and not known in
 * advance.
 *
 * **Invariant**: no entry in `data_` has a zero coefficient.
 *
 * **Degree limit**: same as Poly — max degree per variable per monomial is 253.
 */
class Poly2 {
private:
    std::map<std::vector<uint8_t>, BigInt> data_; ///< monomial → coefficient map
public:
    /** @brief Construct the zero polynomial. */
    Poly2() = default;

    /**
     * @brief Construct from a Poly, converting the compact representation.
     *
     * Every monomial present in @p poly becomes an entry in the map.
     */
    explicit Poly2(const Poly& poly);

    /**
     * @brief Print the polynomial in Wolfram Language (Mathematica) syntax.
     *
     * @param groupByFirst  If > 0, group monomials by the first
     *                      @p groupByFirst variables and format as a nested
     *                      list, suitable for copying into Mathematica.
     * @param varNameFunc   Maps a variable index to its display name string
     *                      (e.g. index 0 → "px", index 3 → "h0").
     */
    void printAsMathematica(size_t groupByFirst,
                            std::function<std::string(size_t)> varNameFunc) const;

    /** @brief Add @p other in-place; combines like terms and drops zeros. */
    Poly2& operator+=(const Poly2& other);
};