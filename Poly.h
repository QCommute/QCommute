#pragma once

#include <vector>
#include <string>
#include <functional>
#include "BigInt.h"

/**
 * @brief Multivariate polynomial with arbitrary-precision integer coefficients.
 *
 * Variables are indexed 0, 1, 2, … (no symbolic names at this level).
 * Each monomial is stored as a degree vector whose entries are packed into a
 * `uint8_t` array separated by the sentinel `MONOM_END = 254`; the whole
 * polynomial is terminated by `POLY_END = 255`.  This compact encoding avoids
 * dynamic per-monomial heap allocations and keeps the cache footprint small.
 *
 * **Invariants** maintained at all times:
 *  - Monomials are sorted in lexicographic order of their degree vectors.
 *  - No two monomials have the same degree vector (like terms are combined).
 *  - No monomial has a zero coefficient.
 *
 * **Degree limit**: the degree of any single variable in a single monomial
 * must not exceed 253 (values 254 and 255 are reserved as sentinels).
 * The number of variables is effectively unlimited.
 */
class Poly {
    friend class Poly2;
private:
    std::vector<uint8_t> powers_; ///< Packed monomial degree vectors.
    std::vector<BigInt>  coefs_;  ///< Coefficients, one per monomial.

    static constexpr uint8_t MONOM_END = 254; ///< Separates degree vectors.
    static constexpr uint8_t POLY_END  = 255; ///< Terminates the polynomial.

    /// Private constructor used internally after arithmetic operations.
    Poly(std::vector<uint8_t>&& powers_, const std::vector<BigInt>& coefs);

public:
    /** @brief Construct the zero polynomial. */
    Poly();

    /**
     * @brief Parse a polynomial from a string.
     *
     * Expected format: `[coef,d0,d1,...,coef,d0,d1,...,0]`
     * where each run `coef,d0,d1,...` describes one monomial.
     * A trailing `0` signals the end of the polynomial.
     */
    Poly(const std::string& str);

    /**
     * @brief Deserialise from a byte buffer.
     * @param data  Buffer produced by a previous serialize() call.
     * @param ptr   Read cursor; advanced past the consumed bytes on return.
     */
    Poly(const std::vector<uint8_t>& data, size_t& ptr);

    /** @brief Return true iff the polynomial is identically zero. */
    bool isZero() const;

    /** @brief Print the polynomial in human-readable form to stdout. */
    void print() const;

    /**
     * @brief Append the serialised representation to @p res.
     * The format is self-delimiting and compatible with the deserialising
     * constructor.
     */
    void serialize(std::vector<uint8_t>& res) const;

    /**
     * @brief Return a copy with @p numVariables new variables prepended.
     *
     * Used when embedding a polynomial into a larger variable set; the new
     * variables all appear with degree 0 in every existing monomial.
     */
    Poly prependVariables(size_t numVariables) const;

    /** @brief Add @p other in-place; combines like terms. */
    Poly& operator+=(const Poly& other);

    /** @brief Multiply and return the result. */
    Poly operator*(const Poly& other) const;

    /** @brief Exact equality test (compares both structure and coefficients). */
    bool operator==(const Poly& other) const;

    /** @brief Multiply every coefficient by integer @p n in-place. */
    Poly& operator*=(int n);

    /** @brief Return a copy scaled by integer @p n. */
    Poly operator*(int n) const;
};