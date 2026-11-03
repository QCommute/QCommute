#pragma once

#include <vector>
#include <string>
#include <cstdio>
#include <cstdint>
#include <functional>
#include "Sigma.h"
#include "Poly.h"

/**
 * @brief Return the Levi-Civita symbol ε_{ijk} for Pauli-algebra commutation.
 *
 * Given two Pauli-type indices i, j ∈ {1, 2, 3} (standing for x, y, z),
 * returns the signed index k such that [σᵢ, σⱼ] = 2i ε_{ijk} σₖ, or 0 if
 * i == j.  The magnitude of the return value is always k (the third index),
 * and its sign encodes whether the Levi-Civita symbol is +1 or -1.
 */
int LeviChivita(int i, int j);
/**
 * @brief Lazy evaluator for the commutator polynomial coefficient (+/- 2 * O1.poly * O2.poly).
 *
 * Delays expensive GMP polynomial multiplication until at least one generated
 * Pauli string passes the hash-window or polarization filter.
 */
class LazyCommutatorPoly {
private:
    const Poly* p1_;
    const Poly* p2_;
    mutable Poly posPoly_;
    mutable Poly negPoly_;
    mutable bool posComputed_ = false;
    mutable bool negComputed_ = false;

public:
    LazyCommutatorPoly(const Poly& p1, const Poly& p2) : p1_(&p1), p2_(&p2) {}

    /** @brief Return +2*p1*p2 (if negative == false) or -2*p1*p2 (if negative == true). */
    const Poly& get(bool negative) const {
        if (!posComputed_) {
            posPoly_ = (*p1_) * (*p2_);
            posPoly_ *= 2;
            posComputed_ = true;
        }
        if (!negative)
            return posPoly_;
        if (!negComputed_) {
            negPoly_ = posPoly_ * (-1);
            negComputed_ = true;
        }
        return negPoly_;
    }
};
/**
 * @brief A single term in a sum-of-Pauli-strings operator.
 *
 * An Expression represents one summand of the form
 *
 *     (polynomial coefficient) × i^(0 or 1) × σ_{a₁}(r₁) σ_{a₂}(r₂) … σ_{aₙ}(rₙ)
 *
 * where each σ_{aₖ}(rₖ) is a Pauli operator (x, y, or z) at lattice site rₖ,
 * and the coefficient is an element of ℤ[h₀, h₁, …] (a polynomial in the
 * coupling constants).
 *
 * **Canonical form**: Pauli operators are always stored sorted first by type
 * (x < y < z) and then by site coordinate.  This makes equality testing O(n)
 * and hashing straightforward.
 *
 * **Supported lattice dimensions**: 1-D, 2-D, and 3-D (selected automatically
 * when parsing the input string).
 *
 * The class is the central data structure of the program.  The main computation
 * loop calls `Commute()` for every pair (Hamiltonian term, observable term) and
 * accumulates the results in a disk-backed hash table.
 */
class Expression {
    friend struct ExpressionHasher;

    template<size_t D>
    friend void commuteImpl(const Expression& O1, const Expression& O2, std::function<void(Expression&& newExpression, const LazyCommutatorPoly& lazyPoly, bool isNegative)> callback);
private:
    Poly poly;                  ///< Polynomial coefficient (exact, arbitrary precision).
    std::vector<int> sigmaCoords; ///< Packed site coordinates in canonical order.
    uint32_t numSigmaX;         ///< Number of σˣ operators.
    uint32_t numSigmaY;         ///< Number of σʸ operators.
    int32_t  dim;               ///< Lattice dimension (0 = undefined, set when an
                                ///<   empty Pauli string is parsed from a string).
    bool im;                    ///< True if the overall prefactor includes a factor of i.

    void setSigmas1D(const Sigma<1>* sigmas, size_t sigmasSize);
    void setSigmas2D(const Sigma<2>* sigmas, size_t sigmasSize);
    void setSigmas3D(const Sigma<3>* sigmas, size_t sigmasSize);

    template<size_t D>
    void setSigmasFromPointSorted(const Sigma<D>* sigmas, size_t sigmasSize);
    template<size_t D>
    std::vector<Sigma<D>> getList() const;

public:
    /** @brief Default-construct an empty (zero) expression. */
    Expression() = default;

    /**
     * @brief Parse an expression from a human-readable string.
     *
     * The expected format is the same as produced by `print()`.
     * Example: `"[1,1]*Sx(0)*Sy(1)"` represents i⁰ × h₀ × σˣ(0) σʸ(1).
     */
    Expression(const std::string& s);

    /**
     * @brief Construct from a Pauli-operator list and a polynomial coefficient.
     *
     * @param v        Array of Sigma objects (1-D sites).
     * @param vSize    Number of elements in @p v.
     * @param r        Polynomial coefficient.
     * @param imm      Whether the prefactor contains a factor of i.
     */
    Expression(const Sigma<1>* v, size_t vSize, const Poly& r, bool imm);
    Expression(const Sigma<2>* v, size_t vSize, const Poly& r, bool imm); ///< 2-D variant.
    Expression(const Sigma<3>* v, size_t vSize, const Poly& r, bool imm); ///< 3-D variant.

    /**
     * @brief Deserialise from a byte buffer.
     * @param data  Buffer produced by a previous serialize() call.
     * @param ptr   Read cursor; advanced past the consumed bytes on return.
     */
    Expression(const std::vector<uint8_t>& data, size_t& ptr);

    /**
     * @brief Compute the commutator [*this, O] and deliver each resulting term
     *        to @p callback.
     *
     * The callback may be invoked zero or more times, each time with one term
     * of the commutator expressed as an rvalue Expression.  The caller is
     * responsible for accumulating and deduplicating these terms.
     *
     * @param O        The right-hand operand.
     * @param callback Called once per resulting term; must not store a reference
     *                 to the argument after it returns.
     */
    void Commute(const Expression& O,
                 std::function<void(Expression&& newExpression)> callback) const;
    /**
     * @brief Compute the commutator [*this, O] with lazy polynomial evaluation.
     *
     * The callback receives the structural Expression (with an empty Poly),
     * a LazyCommutatorPoly reference, and the sign flag `isNegative`. This avoids
     * multiplying or copying GMP polynomials for terms rejected by window filters.
     */
    void Commute(const Expression& O,
                 std::function<void(Expression&& newExpression,
                                    const LazyCommutatorPoly& lazyPoly,
                                    bool isNegative)> callback) const;
    /**
     * @brief Test structural equality (same Pauli string, ignoring coefficient).
     *
     * Two expressions are equal iff they have the same `im` flag, the same
     * sigma-type counts, the same lattice dimension, and the same sorted
     * coordinate list.  The polynomial coefficient is **not** compared.
     */
    bool operator==(const Expression& ex1) const;

    /**
     * @brief Strict weak ordering compatible with operator==.
     *
     * Expressions are ordered lexicographically by (im, numSigmaX, numSigmaY,
     * empty?, dim, sigmaCoords).  This order is used to maintain sorted linked
     * lists inside each hash-table bucket.
     */
    bool operator<(const Expression& ex1) const;

    /** @brief Return a mutable reference to the polynomial coefficient. */
    Poly& getPoly();

    /** @brief Return a const reference to the polynomial coefficient. */
    const Poly& getPoly() const;

    /**
     * @brief Return the counts of σˣ, σʸ, and σᶻ operators.
     * @return Tuple (numX, numY, numZ).
     */
    std::tuple<size_t, size_t, size_t> getNumSigmas() const;

    /** @brief Print the expression in human-readable form to stdout. */
    void print() const;

    /**
     * @brief Append the serialised representation to @p res.
     *
     * The format is self-delimiting and compatible with the deserialising
     * constructor.
     */
    void serialize(std::vector<uint8_t>& res) const;
};

/**
 * @brief Hash functor for Expression, suitable for use with std::unordered_*.
 *
 * The hash is computed from the structural fields (im, dim, sigmaCoords) that
 * participate in operator==.  Two equal expressions always produce the same
 * hash; collisions are possible for expressions that share coordinates but
 * differ in sigma-type counts.
 */
struct ExpressionHasher {
    size_t operator()(const Expression& expr) const;
};
