#pragma once

#include "KDTree.hpp"
#include <string>
#include <fstream>
#include <random>
#include <iostream>

namespace CinemaKnot {

/**
 * @brief Corpus database for associative retrieval, supporting both direct
 * nearest-neighbor retrieval and non-linear aleatoric graph walking ("knotting").
 */
template <typename PayloadType = uint32_t>
class Corpus {
private:
    std::vector<typename KDTree<PayloadType>::Entry> m_entries;
    KDTree<PayloadType> m_tree;
    std::vector<float> m_featureWeights;
    std::mt19937 m_rng{1337};

public:
    Corpus() = default;

    size_t size() const { return m_entries.size(); }
    bool empty() const { return m_entries.empty(); }

    void setWeights(const std::vector<float>& weights) {
        m_featureWeights = weights;
        m_tree.setDimensionWeights(weights);
    }

    /**
     * @brief Add an entry to the corpus before building the search tree.
     */
    void addEntry(const FeatureVector& vector, const PayloadType& payload) {
        m_entries.push_back({vector, payload});
    }

    /**
     * @brief Compile the spatial index. Call once after adding all entries.
     */
    void compileIndex() {
        m_tree.build(m_entries);
    }

    /**
     * @brief Direct nearest-neighbor query (1-NN).
     */
    bool queryNearest(const FeatureVector& query, PayloadType& outPayload, float& outDist) const {
        SearchResult res;
        if (m_tree.findNearest(query, res, &outPayload)) {
            outDist = res.distance();
            return true;
        }
        return false;
    }

    /**
     * @brief K-Nearest Neighbors query.
     */
    std::vector<SearchResult> queryKNearest(const FeatureVector& query, size_t k) const {
        return m_tree.findKNearest(query, k);
    }

    /**
     * @brief Aleatoric / Stochastic Knot Query:
     * Instead of strictly picking the #1 neighbor ("follow the leader"),
     * it evaluates the top-K cluster and samples probabilistically based on distance,
     * or introduces controlled deviation ("knotting" the data).
     * @param query Input vector.
     * @param k Candidate cluster size (e.g. 4 to 8).
     * @param randomness 0.0 = deterministic (always closest), 1.0 = uniform random among top-K.
     */
    bool queryKnot(const FeatureVector& query, size_t k, float randomness,
                   PayloadType& outPayload, float& outDist) {
        auto neighbors = m_tree.findKNearest(query, k);
        if (neighbors.empty()) return false;

        if (randomness <= 0.001f || neighbors.size() == 1) {
            outPayload = static_cast<PayloadType>(neighbors[0].id);
            outDist = neighbors[0].distance();
            return true;
        }

        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        if (dist(m_rng) < randomness) {
            std::uniform_int_distribution<size_t> pick(0, neighbors.size() - 1);
            size_t idx = pick(m_rng);
            outPayload = static_cast<PayloadType>(neighbors[idx].id);
            outDist = neighbors[idx].distance();
        } else {
            outPayload = static_cast<PayloadType>(neighbors[0].id);
            outDist = neighbors[0].distance();
        }
        return true;
    }

    /**
     * @brief Export corpus to offline binary file format (air-gapped / SD card).
     */
    bool saveBinary(const std::string& path) const {
        std::ofstream out(path, std::ios::binary);
        if (!out) return false;

        uint32_t count = static_cast<uint32_t>(m_entries.size());
        uint32_t dim = count > 0 ? static_cast<uint32_t>(m_entries[0].point.dim()) : 0;

        out.write(reinterpret_cast<const char*>(&count), sizeof(count));
        out.write(reinterpret_cast<const char*>(&dim), sizeof(dim));

        for (const auto& e : m_entries) {
            out.write(reinterpret_cast<const char*>(&e.payload), sizeof(PayloadType));
            for (size_t d = 0; d < dim; ++d) {
                float v = e.point[d];
                out.write(reinterpret_cast<const char*>(&v), sizeof(float));
            }
        }
        return true;
    }

    /**
     * @brief Load corpus from offline binary file format.
     */
    bool loadBinary(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;

        uint32_t count = 0, dim = 0;
        in.read(reinterpret_cast<char*>(&count), sizeof(count));
        in.read(reinterpret_cast<char*>(&dim), sizeof(dim));

        m_entries.clear();
        m_entries.reserve(count);

        for (uint32_t i = 0; i < count; ++i) {
            PayloadType payload;
            in.read(reinterpret_cast<char*>(&payload), sizeof(PayloadType));

            FeatureVector fv(dim);
            for (uint32_t d = 0; d < dim; ++d) {
                float v = 0.0f;
                in.read(reinterpret_cast<char*>(&v), sizeof(float));
                fv[d] = v;
            }
            m_entries.push_back({fv, payload});
        }

        compileIndex();
        return true;
    }
};

} // namespace CinemaKnot
